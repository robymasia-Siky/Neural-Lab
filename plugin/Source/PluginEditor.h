#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/DarkHardwareLookAndFeel.h"

class NeuralLabAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit NeuralLabAudioProcessorEditor(NeuralLabAudioProcessor&);
    ~NeuralLabAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, juce::Colour ledColour);

    NeuralLabAudioProcessor& audioProcessor;
    DarkHardwareLookAndFeel darkLookAndFeel;

    // Sezione Prompt & Orchestratore AI
    juce::TextEditor promptInput;
    juce::TextButton generateTrackBtn { "GENERATE TRACK" };
    juce::TextButton autoMasterBtn { "AUTO MIX & MASTER" };

    // 5 Potenziometri Hardware Mastering & Mix
    juce::Slider lowCutSlider, punchSlider, widthSlider, driveSlider, ceilingSlider;
    juce::Label lowCutLabel, punchLabel, widthLabel, driveLabel, ceilingLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> lowCutAtt, punchAtt, widthAtt, driveAtt, ceilingAtt;

    // Dati per Analizzatore Spettro FFT
    std::array<float, NeuralDSP::fftSize / 2> fftMagnitudes {};
    std::array<float, 64> smoothedBands {};
    float currentMeterLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeuralLabAudioProcessorEditor)
};
