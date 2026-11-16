#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_osc/juce_osc.h>
#include "DSP/NeuralDSP.h"

class NeuralLabAudioProcessor : public juce::AudioProcessor,
                                private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback> {
public:
    NeuralLabAudioProcessor();
    ~NeuralLabAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NEURAL LAB"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Comunicazione verso Backend AI Python (porta 9001)
    void sendPromptToAIEngine(const juce::String& promptText, const juce::String& mode);

    juce::AudioProcessorValueTreeState apvts;
    NeuralDSP neuralDSP;
    juce::String aiStatusMessage { "AI ENGINE: READY (LOCAL GPU)" };

private:
    void oscMessageReceived(const juce::OSCMessage& message) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::OSCSender oscSender;
    juce::OSCReceiver oscReceiver;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeuralLabAudioProcessor)
};
