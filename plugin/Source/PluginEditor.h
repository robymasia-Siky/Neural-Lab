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
    void setupKnob(juce::Slider lowCutSlider, punchSlider, widthSlider, driveSlider, eqMatchSlider, ceilingSlider;

    NeuralLabAudioProcessor& audioProcessor;
    DarkHardwareLookAndFeel darkLookAndFeel;

    juce::TextEditor promptInput;
    juce::TextButton generateTrackBtn { "GENERATE TRACK" };
    juce::TextButton arrangeFullBtn   { "ARRANGE FULL" };
    juce::TextButton askChatBtn { "ASK / MIDI" };
    juce::TextButton autoMasterBtn    { "AUTO MASTER" };

    juce::TextButton addFolderBtn     { "+ SAMPLE FOLDER" };
    juce::TextButton loadRefBtn       { "LOAD REFERENCE" };
    std::array<juce::TextButton, 4> swapButtons;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Slider lowCutSlider, punchSlider, widthSlider, driveSlider, eqMatchSlider, ceilingSlider;
    juce::Label  lowCutLabel,  punchLabel,  widthLabel,  driveLabel,  eqMatchLabel,  ceilingLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> lowCutAtt, punchAtt, widthAtt, driveAtt, eqMatchAtt, ceilingAtt;

    std::array<float, NeuralDSP::fftSize / 2> fftMagnitudes {};
    std::array<float, 64> smoothedBands {};
    float currentMeterLevel = 0.0f;
    float currentLufsDisplay = -60.0f;
    float currentGrDisplay = 0.0f;
    float splashProgress = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeuralLabAudioProcessorEditor)
};