#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

class NeuralDSP {
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numEqBands = 8;

    NeuralDSP() : forwardFFT(fftOrder), window(fftSize, juce::dsp::WindowingFunction<float>::hann) {}

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = spec.sampleRate;
        lowCutFilter.prepare(spec);
        lowCutFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);

        for (int b = 0; b < numEqBands; ++b) {
            matchEqBands[b].prepare(spec);
            matchEqBands[b].reset();
        }
        updateMatchEqCoefficients(true);

        compressor.prepare(spec);
        limiter.prepare(spec);
        limiter.setRelease(50.0f);
    }

    void setReferenceCurve(const std::array<float, 64>& curve64) {
        // Mappa i 64 bin logaritmici in 8 bande EQ target (dB delta rispetto a curva bilanciata 0.50)
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
            // Scostamento in dB (range -7.5 dB .. +7.5 dB)
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

        compThresholdDb = juce::jmap(punchAmount, 0.0f, 1.0f, 0.0f, -24.0f);
        compRatio = juce::jmap(punchAmount, 0.0f, 1.0f, 1.0f, 6.0f);
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

        // 3. 8-Band Auto-EQ Matchering (attivo quando e' caricata una reference e Match > 0%)
        if (hasReferenceData.load() && lastMatchPercent > 0.5f) {
            for (int b = 0; b < numEqBands; ++b) {
                matchEqBands[b].process(context);
            }
        }

        // 4. Punch Compressor
        compressor.process(context);

        // 5. Mid/Side Stereo Width
        if (numChannels >= 2) {
            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getWritePointer(1);
            for (int i = 0; i < numSamples; ++i) {
                float mid = 0.5f * (left[i] + right[i]);
                float side = 0.5f * (left[i] - right[i]) * stereoWidth;
                left[i] = mid + side;
                right[i] = mid - side;
            }
        }

        // 6. True-Peak Limiter
        limiter.process(context);

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

        // 8. Feed FFT
        const auto* monoRead = buffer.getReadPointer(0);
        for (int i = 0; i < numSamples; ++i) {
            pushNextSampleIntoFifo(monoRead[i]);
        }
    }

    float getRMSLevel() const { return currentRms.load(); }
    float getLUFS() const { return currentLufs.load(); }
    float getGainReductionDb() const { return currentGainReductionDb.load(); }

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
    float stereoWidth = 1.0f;
    float inputGainLinear = 1.0f;
    float compThresholdDb = -12.0f;
    float compRatio = 3.0f;
    float limiterCeilingDb = -0.3f;
    float lastMatchPercent = 50.0f;

    std::atomic<bool> hasReferenceData { false };
    std::atomic<bool> curveDirty { false };
    std::array<std::atomic<float>, numEqBands> targetBandGainsDb {};

    std::atomic<float> currentRms { 0.0f };
    std::atomic<float> currentLufs { -60.0f };
    std::atomic<float> currentGainReductionDb { 0.0f };

    using StereoIIR = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    std::array<StereoIIR, numEqBands> matchEqBands;

    juce::dsp::StateVariableTPTFilter<float> lowCutFilter;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Limiter<float> limiter;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize> fifo {};
    std::array<float, fftSize * 2> fftData {};
    int fifoIndex = 0;
    std::atomic<bool> nextFFTBlockReady { false };
};