#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>

class NeuralDSP {
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numEqBands = 8;

    NeuralDSP() : forwardFFT(fftOrder), window(fftSize, juce::dsp::WindowingFunction<float>::hann) {}

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = spec.sampleRate;
        numPreparedChannels = spec.numChannels;

        lowCutFilter.prepare(spec);
        lowCutFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);

        // Filtro Mono-Maker a 120 Hz sul canale Side (mantiene Sub/Kick 100% mono)
        juce::dsp::ProcessSpec monoSpec { spec.sampleRate, spec.maximumBlockSize, 1 };
        sideHighPassFilter.prepare(monoSpec);
        sideHighPassFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        sideHighPassFilter.setCutoffFrequency(120.0f);
        sideHighPassFilter.reset();
        constexpr float aFreqs[numEqBands] = { 45.0f, 110.0f, 260.0f, 620.0f, 1500.0f, 3600.0f, 8000.0f, 14500.0f };
        for (int b = 0; b < numEqBands; ++b) {
            analyzerFilters[static_cast<size_t>(b)].prepare(monoSpec);
            *analyzerFilters[static_cast<size_t>(b)].coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass(spec.sampleRate, aFreqs[b], 1.4f);
            analyzerFilters[static_cast<size_t>(b)].reset();
            bandEnvState[static_cast<size_t>(b)] = 0.0f;
            liveBandDb[static_cast<size_t>(b)].store(-60.0f);
        }

        for (int b = 0; b < numEqBands; ++b) {
            matchEqBands[b].prepare(spec);
            matchEqBands[b].reset();
        }
        updateMatchEqCoefficients(true);

        compressor.prepare(spec);

        // 4x Oversampling (2^2 = 4x) con filtri IIR polifase per saturazione armonica e limiting True-Peak senza aliasing
        oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
            spec.numChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampler->initProcessing(spec.maximumBlockSize);

        juce::dsp::ProcessSpec osSpec {
            spec.sampleRate * static_cast<double>(oversampler->getOversamplingFactor()),
            spec.maximumBlockSize * static_cast<juce::uint32>(oversampler->getOversamplingFactor()),
            spec.numChannels
        };
        limiter.prepare(osSpec);
        limiter.setRelease(45.0f);
    }

    void setReferenceCurve(const std::array<float, 64>& curve64) {
        constexpr int binRanges[numEqBands][2] = {
            {0, 6},    // 45 Hz (Sub)
            {6, 13},   // 110 Hz (Bass)
            {13, 21},  // 260 Hz (Low-Mid)
            {21, 30},  // 620 Hz (Mid)
            {30, 39},  // 1.5 kHz (Upper-Mid)
            {39, 48},  // 3.6 kHz (Presence)
            {48, 56},  // 8.0 kHz (Air)
            {56, 64}   // 14.5 kHz (Brilliance)
        };

        for (int b = 0; b < numEqBands; ++b) {
            float sum = 0.0f;
            int count = 0;
            for (int i = binRanges[b][0]; i < binRanges[b][1]; ++i) {
                sum += curve64[static_cast<size_t>(i)];
                ++count;
            }
            float avgNorm = (count > 0) ? (sum / static_cast<float>(count)) : 0.5f;
            float deltaDb = juce::jlimit(-7.5f, 7.5f, (avgNorm - 0.50f) * 15.0f);
            targetBandGainsDb[static_cast<size_t>(b)].store(deltaDb);
        }
        hasReferenceData.store(true);
        curveDirty.store(true);
    }

    void updateParameters(float lowCutHz, float punchAmount, float widthAmount, float ceilingDb, float driveDb, float eqMatchPercent) {
        lowCutFilter.setCutoffFrequency(juce::jlimit(20.0f, 300.0f, lowCutHz));

        float clampedMatch = juce::jlimit(0.0f, 100.0f, eqMatchPercent);
        if (std::abs(clampedMatch - lastMatchPercent) > 0.05f || curveDirty.exchange(false)) {
            lastMatchPercent = clampedMatch;
            updateMatchEqCoefficients(false);
        }

        currentPunchAmount = juce::jlimit(0.0f, 1.0f, punchAmount);
        compThresholdDb = juce::jmap(currentPunchAmount, 0.0f, 1.0f, 0.0f, -24.0f);
        compRatio = juce::jmap(currentPunchAmount, 0.0f, 1.0f, 1.0f, 6.0f);
        compressor.setThreshold(compThresholdDb);
        compressor.setRatio(compRatio);
        compressor.setAttack(12.0f);
        compressor.setRelease(90.0f);

        stereoWidth = juce::jlimit(0.0f, 2.0f, widthAmount);
        inputGainLinear = juce::Decibels::decibelsToGain(driveDb);
        limiterCeilingDb = ceilingDb;
        limiter.setThreshold(ceilingDb);
    }

    void process(juce::AudioBuffer<float>& buffer) {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0 || numChannels <= 0) return;

        // 1. Input Drive
        buffer.applyGain(inputGainLinear);

        float prePeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            prePeak = juce::jmax(prePeak, buffer.getMagnitude(ch, 0, numSamples));
        }

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);

        // 2. Sub Clean High-Pass
        lowCutFilter.process(context);

        // 3. 8-Band Auto-EQ Matchering
        if (hasReferenceData.load() && lastMatchPercent > 0.5f) {
            for (int b = 0; b < numEqBands; ++b) {
                matchEqBands[b].process(context);
            }
        }

        // 4. Punch Compressor
        compressor.process(context);

        // 5. Mid/Side Stereo Width con Club Mono-Maker (< 120 Hz in mono puro)
        if (numChannels >= 2) {
            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getWritePointer(1);
            for (int i = 0; i < numSamples; ++i) {
                float mid = 0.5f * (left[i] + right[i]);
                float rawSide = 0.5f * (left[i] - right[i]);
                // Filtra via il sub dal canale Side così tutto sotto 120 Hz resta centrato in mono
                float cleanSide = sideHighPassFilter.processSample(0, rawSide) * stereoWidth;
                left[i] = mid + cleanSide;
                right[i] = mid - cleanSide;
            }
        }

        // 6. 4x Oversampled Analog Soft-Clipper + True-Peak Limiter
        if (oversampler != nullptr) {
            auto osBlock = oversampler->processSamplesUp(block);
            const size_t osChannels = osBlock.getNumChannels();
            const size_t osSamples = osBlock.getNumSamples();

            // Saturazione morbida proporzionale al Neural Punch (calore armonico senza aliasing)
            const float satDrive = 1.0f + currentPunchAmount * 0.65f;
            const float satNorm = 1.0f / std::tanh(satDrive);
            for (size_t ch = 0; ch < osChannels; ++ch) {
                auto* data = osBlock.getChannelPointer(ch);
                for (size_t i = 0; i < osSamples; ++i) {
                    data[i] = std::tanh(data[i] * satDrive) * satNorm;
                }
            }

            juce::dsp::ProcessContextReplacing<float> osContext(osBlock);
            limiter.process(osContext);
            oversampler->processSamplesDown(block);
        } else {
            limiter.process(context);
        }

        // 7. Output RMS, LUFS e Gain Reduction
        float sumSq = 0.0f;
        float postPeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            sumSq += buffer.getRMSLevel(ch, 0, numSamples);
            postPeak = juce::jmax(postPeak, buffer.getMagnitude(ch, 0, numSamples));
        }
        float avgRms = sumSq / juce::jmax(1, numChannels);
        currentRms.store(avgRms);

        float lufsVal = (avgRms > 1e-6f) ? (juce::Decibels::gainToDecibels(avgRms) - 0.691f) : -60.0f;
        currentLufs.store(juce::jlimit(-60.0f, 0.0f, lufsVal));

        float grDb = 0.0f;
        if (prePeak > postPeak && prePeak > 1e-5f) {
            grDb = juce::Decibels::gainToDecibels(prePeak) - juce::Decibels::gainToDecibels(postPeak);
        }
        currentGainReductionDb.store(juce::jlimit(0.0f, 24.0f, grDb));

                        float blockMaxAbs = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            blockMaxAbs = juce::jmax(blockMaxAbs, buffer.getMagnitude(ch, 0, numSamples));
        float blockTpDb = juce::jlimit(-60.0f, 6.0f, juce::Decibels::gainToDecibels(blockMaxAbs));
        float prevTp = currentTruePeakDb.load();
        currentTruePeakDb.store(blockTpDb > prevTp ? blockTpDb : (prevTp * 0.92f + blockTpDb * 0.08f));
        // 8. Feed FFT & 8-Band Real-Time Telemetry Analyzer
        const auto* monoRead = buffer.getReadPointer(0);
        for (int i = 0; i < numSamples; ++i) {
            float s = monoRead[i];
            pushNextSampleIntoFifo(s);
            if ((i & 3) == 0) { // Decimazione 4x per zero impatto CPU
                for (int b = 0; b < numEqBands; ++b) {
                    float bp = analyzerFilters[static_cast<size_t>(b)].processSample(s);
                    float sq = bp * bp;
                    bandEnvState[static_cast<size_t>(b)] = bandEnvState[static_cast<size_t>(b)] * 0.985f + sq * 0.015f;
                }
            }
        }
        for (int b = 0; b < numEqBands; ++b) {
            float rmsB = std::sqrt(juce::jmax(1e-12f, bandEnvState[static_cast<size_t>(b)]));
            liveBandDb[static_cast<size_t>(b)].store(juce::jlimit(-60.0f, 0.0f, juce::Decibels::gainToDecibels(rmsB)));
        }
    }

    float getRMSLevel() const { return currentRms.load(); }
    float getLUFS() const { return currentLufs.load(); }
    float getGainReductionDb() const { return currentGainReductionDb.load(); }
    float getTruePeakDb() const { return currentTruePeakDb.load(); }
    float getPlrDb() const {
        float lufs = currentLufs.load();
        float tp = currentTruePeakDb.load();
        if (lufs <= -58.0f) return 0.0f;
        return juce::jmax(0.0f, tp - lufs);
    }
    juce::String getTelemetrySummary(float driveDb, float lowCutHz, float punchPct, float widthPct, float matchPct, float ceilingDb, bool hasRef, const juce::String& refName) const {
        float lufs = currentLufs.load();
        float gr   = currentGainReductionDb.load();
        juce::String lufsStr = (lufs <= -58.0f) ? "-INF (Silence/Idle)" : (juce::String(lufs, 1) + " LUFS");

        juce::String out;
        out << "LIVE_AUDIO: " << lufsStr << " | TruePeak=" << juce::String(getTruePeakDb(), 1) << "dBTP | PLR=" << juce::String(getPlrDb(), 1) << "dB"
            << " | Limiter_GR=" << juce::String(gr, 1) << "dB"
            << " | DSP[Drive=" << juce::String(driveDb, 1) << "dB"
            << ", SubCut=" << juce::String(static_cast<int>(lowCutHz)) << "Hz"
            << ", Punch=" << juce::String(static_cast<int>(punchPct * 100.0f)) << "%"
            << ", Width=" << juce::String(static_cast<int>(widthPct * 100.0f)) << "%"
            << ", EQMatch=" << juce::String(static_cast<int>(matchPct)) << "%"
            << ", Ceiling=" << juce::String(ceilingDb, 1) << "dB]";

        if (lufs > -55.0f) {
            constexpr const char* bandNames[numEqBands] = { "Sub45Hz", "Bass110Hz", "LowMid260Hz", "Mid620Hz", "UpMid1.5k", "Pres3.6k", "Air8k", "Bril14k" };
            out << " | BandLevels[";
            for (int b = 0; b < numEqBands; ++b) {
                float liveDb = liveBandDb[static_cast<size_t>(b)].load();
                out << bandNames[b] << "=" << juce::String(liveDb, 1) << "dB";
                if (hasRef) {
                    float refTargetDelta = targetBandGainsDb[static_cast<size_t>(b)].load();
                    // Se targetBandGainsDb e' negativo significa che il live e' piu' alto della reference
                    float diffVsRef = -refTargetDelta;
                    out << "(vsRef:" << (diffVsRef >= 0.0f ? "+" : "") << juce::String(diffVsRef, 1) << "dB)";
                }
                if (b < numEqBands - 1) out << ", ";
            }
            out << "]";
        }
        if (hasRef && refName.isNotEmpty()) {
            out << " | RefTrack=" << refName;
        }
        return out;
    }

    bool getNextFFTBlock(std::array<float, fftSize / 2>& dest) {
        if (!nextFFTBlockReady.load()) return false;
        window.multiplyWithWindowingTable(fftData.data(), fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());
        for (int i = 0; i < fftSize / 2; ++i) {
            dest[i] = fftData[i];
        }
        nextFFTBlockReady.store(false);
        return true;
    }

private:
    void updateMatchEqCoefficients(bool forceFlat) {
        if (sampleRate <= 0.0) return;
        constexpr float bandFreqs[numEqBands] = { 45.0f, 110.0f, 260.0f, 620.0f, 1500.0f, 3600.0f, 8000.0f, 14500.0f };
        constexpr float bandQs[numEqBands]    = { 0.71f, 1.20f,  1.30f,  1.30f,  1.30f,   1.30f,   1.20f,   0.71f };
        float scale = forceFlat ? 0.0f : (lastMatchPercent / 100.0f);

        for (int b = 0; b < numEqBands; ++b) {
            float gainDb = targetBandGainsDb[static_cast<size_t>(b)].load() * scale;
            float gainLinear = juce::Decibels::decibelsToGain(gainDb);
            if (b == 0) {
                *matchEqBands[b].state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, bandFreqs[b], bandQs[b], gainLinear);
            } else if (b == numEqBands - 1) {
                *matchEqBands[b].state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, bandFreqs[b], bandQs[b], gainLinear);
            } else {
                *matchEqBands[b].state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, bandFreqs[b], bandQs[b], gainLinear);
            }
        }
    }

    void pushNextSampleIntoFifo(float sample) noexcept {
        if (fifoIndex == fftSize) {
            if (!nextFFTBlockReady.load()) {
                std::fill(fftData.begin(), fftData.end(), 0.0f);
                std::copy(fifo.begin(), fifo.end(), fftData.begin());
                nextFFTBlockReady.store(true);
            }
            fifoIndex = 0;
        }
        fifo[fifoIndex++] = sample;
    }

    double sampleRate = 44100.0;
    juce::uint32 numPreparedChannels = 2;
    float stereoWidth = 1.0f;
    float inputGainLinear = 1.0f;
    float currentPunchAmount = 0.35f;
    float compThresholdDb = -12.0f;
    float compRatio = 3.0f;
    float limiterCeilingDb = -0.3f;
    float lastMatchPercent = 50.0f;

    std::atomic<bool> hasReferenceData { false };
    std::atomic<bool> curveDirty { false };
    std::array<std::atomic<float>, numEqBands> targetBandGainsDb {};
    std::array<std::atomic<float>, numEqBands> liveBandDb {};
    std::array<float, numEqBands> bandEnvState {};
    std::array<juce::dsp::IIR::Filter<float>, numEqBands> analyzerFilters;

    std::atomic<float> currentRms { 0.0f };
    std::atomic<float> currentLufs { -60.0f };
    std::atomic<float> currentGainReductionDb { 0.0f };
    std::atomic<float> currentTruePeakDb { -60.0f };

    using StereoIIR = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    std::array<StereoIIR, numEqBands> matchEqBands;

    juce::dsp::StateVariableTPTFilter<float> lowCutFilter;
    juce::dsp::StateVariableTPTFilter<float> sideHighPassFilter;
    juce::dsp::Compressor<float> compressor;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::Limiter<float> limiter;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize> fifo {};
    std::array<float, fftSize * 2> fftData {};
    int fifoIndex = 0;
    std::atomic<bool> nextFFTBlockReady { false };
};


