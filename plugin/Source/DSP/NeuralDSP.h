#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

class NeuralDSP {
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder; // 2048 punti

    NeuralDSP() : forwardFFT(fftOrder), window(fftSize, juce::dsp::WindowingFunction<float>::hann) {}

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = spec.sampleRate;
        lowCutFilter.prepare(spec);
        lowCutFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);

        compressor.prepare(spec);
        limiter.prepare(spec);
        limiter.setRelease(50.0f);
    }

    void updateParameters(float lowCutHz, float punchAmount, float widthAmount, float ceilingDb, float driveDb) {
        lowCutFilter.setCutoffFrequency(juce::jlimit(20.0f, 300.0f, lowCutHz));

        // Punch controlla threshold e ratio del compressore
        compressor.setThreshold(juce::jmap(punchAmount, 0.0f, 1.0f, 0.0f, -24.0f));
        compressor.setRatio(juce::jmap(punchAmount, 0.0f, 1.0f, 1.0f, 6.0f));
        compressor.setAttack(12.0f);
        compressor.setRelease(90.0f);

        stereoWidth = juce::jlimit(0.0f, 2.0f, widthAmount);
        inputGainLinear = juce::Decibels::decibelsToGain(driveDb);
        limiter.setThreshold(ceilingDb);
    }

    void process(juce::AudioBuffer<float>& buffer) {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        // 1. Input Drive / Gain Staging
        buffer.applyGain(inputGainLinear);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);

        // 2. Low-Cut Sub Cleaning
        lowCutFilter.process(context);

        // 3. Punch Compressor
        compressor.process(context);

        // 4. Mid/Side Stereo Width
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

        // 5. True-Peak Mastering Limiter
        limiter.process(context);

        // 6. Calcolo Metering RMS/Peak & Feed FFT per UI
        float sumSq = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            sumSq += buffer.getRMSLevel(ch, 0, numSamples);
        }
        currentRms.store(sumSq / juce::jmax(1, numChannels));

        const auto* monoRead = buffer.getReadPointer(0);
        for (int i = 0; i < numSamples; ++i) {
            pushNextSampleIntoFifo(monoRead[i]);
        }
    }

    float getRMSLevel() const { return currentRms.load(); }

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
    std::atomic<float> currentRms { 0.0f };

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
