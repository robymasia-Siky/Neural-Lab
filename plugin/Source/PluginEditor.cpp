#include "PluginEditor.h"

NeuralLabAudioProcessorEditor::NeuralLabAudioProcessorEditor(NeuralLabAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    setLookAndFeel(&darkLookAndFeel);
    setSize(860, 520);

    // Prompt Input
    promptInput.setMultiLine(false);
    promptInput.setTextToShowWhenEmpty("Describe track or mix target (e.g. 'Dark Melodic Techno 126 BPM, punchy low-end, -8.5 LUFS')...", juce::Colour(0xff6b7280));
    promptInput.setFont(juce::FontOptions(14.0f));
    addAndMakeVisible(promptInput);

    // Pulsanti AI Hardware
    generateTrackBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff005f73));
    generateTrackBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00e5ff));
    generateTrackBtn.onClick = [this] {
        audioProcessor.sendPromptToAIEngine(promptInput.getText(), "generate_track");
    };
    addAndMakeVisible(generateTrackBtn);

    autoMasterBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff78350f));
    autoMasterBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffbbf24));
    autoMasterBtn.onClick = [this] {
        audioProcessor.sendPromptToAIEngine(promptInput.getText(), "auto_master");
    };
    addAndMakeVisible(autoMasterBtn);

    // Setup 5 Knobs Dark Hardware
    auto cyanLed = juce::Colour(0xff00e5ff);
    auto amberLed = juce::Colour(0xfff59e0b);

    setupKnob(driveSlider,   driveLabel,   "INPUT DRIVE", cyanLed);
    setupKnob(lowCutSlider,  lowCutLabel,  "SUB CLEAN",   cyanLed);
    setupKnob(punchSlider,   punchLabel,   "NEURAL PUNCH", amberLed);
    setupKnob(widthSlider,   widthLabel,   "STEREO WIDTH", cyanLed);
    setupKnob(ceilingSlider, ceilingLabel, "CEILING",      amberLed);

    driveAtt   = std::make_unique<Attachment>(audioProcessor.apvts, "drive",   driveSlider);
    lowCutAtt  = std::make_unique<Attachment>(audioProcessor.apvts, "lowcut",  lowCutSlider);
    punchAtt   = std::make_unique<Attachment>(audioProcessor.apvts, "punch",   punchSlider);
    widthAtt   = std::make_unique<Attachment>(audioProcessor.apvts, "width",   widthSlider);
    ceilingAtt = std::make_unique<Attachment>(audioProcessor.apvts, "ceiling", ceilingSlider);

    startTimerHz(60); // 60 FPS fluidi
}

NeuralLabAudioProcessorEditor::~NeuralLabAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void NeuralLabAudioProcessorEditor::setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, juce::Colour ledColour) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    slider.setColour(juce::Slider::rotarySliderFillColourId, ledColour);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffcbd5e1));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    addAndMakeVisible(label);
}

void NeuralLabAudioProcessorEditor::timerCallback() {
    if (audioProcessor.neuralDSP.getNextFFTBlock(fftMagnitudes)) {
        for (size_t i = 0; i < smoothedBands.size(); ++i) {
            size_t bin = static_cast<size_t>(std::pow(1.09, static_cast<double>(i)));
            bin = juce::jmin(bin, fftMagnitudes.size() - 1);
            float db = juce::Decibels::gainToDecibels(fftMagnitudes[bin] / static_cast<float>(NeuralDSP::fftSize)) + 60.0f;
            float norm = juce::jlimit(0.0f, 1.0f, db / 60.0f);
            smoothedBands[i] = smoothedBands[i] * 0.75f + norm * 0.25f;
        }
    }
    currentMeterLevel = currentMeterLevel * 0.8f + audioProcessor.neuralDSP.getRMSLevel() * 0.2f;
    repaint();
}

void NeuralLabAudioProcessorEditor::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // 1. Chassis Metallo Spazzolato Dark Hardware
    juce::ColourGradient bgGrad(juce::Colour(0xff161920), 0.0f, 0.0f,
                                juce::Colour(0xff0d0f13), 0.0f, bounds.getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    // Bordo rack metallico
    g.setColour(juce::Colour(0xff2d3340));
    g.drawRect(bounds.reduced(2.0f), 2.0f);

    // 2. Header Bar
    auto headerArea = bounds.removeFromTop(54.0f);
    g.setColour(juce::Colour(0xff0a0c0f));
    g.fillRect(headerArea);
    g.setColour(juce::Colour(0xff262b36));
    g.drawLine(0.0f, 54.0f, bounds.getWidth(), 54.0f, 1.5f);

    // Logo NEURAL LAB
    g.setColour(juce::Colour(0xfff8fafc));
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText("NEURAL LAB", 24, 10, 220, 34, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff00e5ff));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("AI STUDIO & MASTERING CONSOLE", 158, 18, 250, 20, juce::Justification::centredLeft);

    // Status LED in alto a destra
    g.setColour(juce::Colour(0xff10b981));
    g.fillEllipse(bounds.getWidth() - 255.0f, 22.0f, 8.0f, 8.0f);
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(audioProcessor.aiStatusMessage, static_cast<int>(bounds.getWidth()) - 240, 14, 225, 26, juce::Justification::centredLeft);

    // 3. Display Centrale Analizzatore Spettro FFT (OLED Hardware Screen)
    juce::Rectangle<float> screenArea(24.0f, 118.0f, bounds.getWidth() - 74.0f, 200.0f);
    g.setColour(juce::Colour(0xff07090c));
    g.fillRoundedRectangle(screenArea, 6.0f);
    g.setColour(juce::Colour(0xff1e2430));
    g.drawRoundedRectangle(screenArea, 6.0f, 1.5f);

    // Griglia decibel sullo schermo
    g.setColour(juce::Colour(0xff141821));
    for (int i = 1; i < 4; ++i) {
        float yLine = screenArea.getY() + (screenArea.getHeight() / 4.0f) * i;
        g.drawLine(screenArea.getX() + 4.0f, yLine, screenArea.getRight() - 4.0f, yLine, 1.0f);
    }

    // Barre Spettro Luminose Ciano/Ambra
    float barWidth = (screenArea.getWidth() - 16.0f) / static_cast<float>(smoothedBands.size());
    for (size_t i = 0; i < smoothedBands.size(); ++i) {
        float barHeight = smoothedBands[i] * (screenArea.getHeight() - 16.0f);
        float xPos = screenArea.getX() + 8.0f + i * barWidth;
        float yPos = screenArea.getBottom() - 8.0f - barHeight;

        juce::Colour barCol = (i < 16) ? juce::Colour(0xfff59e0b) : juce::Colour(0xff00e5ff);
        g.setColour(barCol.withAlpha(0.85f));
        g.fillRoundedRectangle(xPos, yPos, barWidth - 2.0f, barHeight, 1.5f);
    }

    // 4. VU Meter Output Destro
    juce::Rectangle<float> meterArea(bounds.getWidth() - 38.0f, 118.0f, 16.0f, 200.0f);
    g.setColour(juce::Colour(0xff07090c));
    g.fillRoundedRectangle(meterArea, 3.0f);
    float levelH = juce::jlimit(0.0f, 1.0f, currentMeterLevel * 2.5f) * (meterArea.getHeight() - 4.0f);
    g.setColour(levelH > 175.0f ? juce::Colour(0xffef4444) : juce::Colour(0xff10b981));
    g.fillRoundedRectangle(meterArea.getX() + 2.0f, meterArea.getBottom() - 2.0f - levelH, meterArea.getWidth() - 4.0f, levelH, 2.0f);

    // 5. Pannello Inferiore Controlli Hardware
    juce::Rectangle<float> bottomRack(24.0f, 332.0f, bounds.getWidth() - 48.0f, 166.0f);
    g.setColour(juce::Colour(0xff12151c));
    g.fillRoundedRectangle(bottomRack, 6.0f);
    g.setColour(juce::Colour(0xff252a36));
    g.drawRoundedRectangle(bottomRack, 6.0f, 1.0f);
}

void NeuralLabAudioProcessorEditor::resized() {
    auto area = getLocalBounds();
    area.removeFromTop(66); // Spazio sotto header

    // Barra Prompt + Pulsanti AI
    auto promptRow = area.removeFromTop(38).reduced(24, 0);
    autoMasterBtn.setBounds(promptRow.removeFromRight(155));
    promptRow.removeFromRight(8);
    generateTrackBtn.setBounds(promptRow.removeFromRight(140));
    promptRow.removeFromRight(10);
    promptInput.setBounds(promptRow);

    // Area Knobs in basso
    auto knobRack = getLocalBounds().removeFromBottom(175).reduced(40, 12);
    int colWidth = knobRack.getWidth() / 5;

    auto placeKnob = [&](juce::Slider& s, juce::Label& l, int idx) {
        auto col = knobRack.withX(knobRack.getX() + idx * colWidth).withWidth(colWidth);
        l.setBounds(col.removeFromTop(20));
        s.setBounds(col.reduced(8, 2));
    };

    placeKnob(driveSlider,   driveLabel,   0);
    placeKnob(lowCutSlider,  lowCutLabel,  1);
    placeKnob(punchSlider,   punchLabel,   2);
    placeKnob(widthSlider,   widthLabel,   3);
    placeKnob(ceilingSlider, ceilingLabel, 4);
}
