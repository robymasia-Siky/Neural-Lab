#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/DarkHardwareLookAndFeel.h"

class NeuralLabAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::DragAndDropContainer,
                                      private juce::Timer {
public:
    explicit NeuralLabAudioProcessorEditor(NeuralLabAudioProcessor&);
    ~NeuralLabAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, juce::Colour ledColour);
    int getStemRowAtPoint(juce::Point<int> pos) const;

    NeuralLabAudioProcessor& audioProcessor;
    DarkHardwareLookAndFeel darkLookAndFeel;

    juce::TextEditor promptInput;
    juce::TextEditor chatDisplayBox;
    size_t lastChatCount = 0;
    juce::TextButton generateTrackBtn { "GENERATE TRACK" };
    juce::TextButton arrangeFullBtn   { "ARRANGE FULL" };
    juce::TextButton askChatBtn       { "ASK / MIDI" };
    juce::TextButton autoMasterBtn    { "AUTO MASTER" };
    juce::TextButton applyDspBtn      { "APPLY DSP PRESET" };

    juce::TextButton addFolderBtn     { "+ SAMPLE FOLDER" };
    juce::TextButton loadRefBtn       { "LOAD REFERENCE" };
    juce::TextButton playRefBtn       { "> REF" };
    juce::TextButton mcpToggleBtn     { "MCP: ON" };
    std::array<juce::TextButton, 4> auditionButtons;
    std::array<juce::TextButton, 4> lockButtons;
    std::array<juce::TextButton, 4> swapButtons;
    std::array<juce::Rectangle<float>, 4> stemRowBounds;
    int hoveredStemIndex = -1;
    int draggingStemIndex = -1;
    bool externalDragTriggered = false;
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
    int pingTickCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeuralLabAudioProcessorEditor)
};
