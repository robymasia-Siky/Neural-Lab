#include "PluginEditor.h"

NeuralLabAudioProcessorEditor::NeuralLabAudioProcessorEditor(NeuralLabAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    setLookAndFeel(&darkLookAndFeel);
    setSize(980, 610);

    // 1. Prompt Input
    promptInput.setMultiLine(false);
    promptInput.setTextToShowWhenEmpty("Describe track or master target (e.g. 'Dark Melodic Techno 126 BPM, punchy kick, -8.5 LUFS')...", juce::Colour(0xff6b7280));
    promptInput.setFont(juce::FontOptions(14.0f));
    addAndMakeVisible(promptInput);

    // 2. Pulsanti AI
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

    // 3. Pulsante Multi-Cartella Sample (+ SAMPLE FOLDER)
    addFolderBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    addFolderBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
    addFolderBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser>("Select Sample / Loop Library Folder",
                                                          juce::File::getSpecialLocation(juce::File::userDesktopDirectory));
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
            auto folder = fc.getResult();
            if (folder.isDirectory()) {
                audioProcessor.addSampleFolder(folder);
            }
        });
    };
    addAndMakeVisible(addFolderBtn);

    // 4. Pulsante Reference Track (Matchering 2.0)
    loadRefBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    loadRefBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffbbf24));
    loadRefBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser>("Select Reference Track (.wav / .mp3 / .flac)",
                                                          juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                                          "*.wav;*.mp3;*.flac;*.aiff");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile()) {
                audioProcessor.setReferenceTrack(file);
            }
        });
    };
    addAndMakeVisible(loadRefBtn);

    // 5. Setup 5 Knobs Dark Hardware
    auto cyanLed  = juce::Colour(0xff00e5ff);
    auto amberLed = juce::Colour(0xfff59e0b);

    setupKnob(driveSlider,   driveLabel,   "INPUT DRIVE",  cyanLed);
    setupKnob(lowCutSlider,  lowCutLabel,  "SUB CLEAN",    cyanLed);
    setupKnob(punchSlider,   punchLabel,   "NEURAL PUNCH", amberLed);
    setupKnob(widthSlider,   widthLabel,   "STEREO WIDTH", cyanLed);
    setupKnob(ceilingSlider, ceilingLabel, "CEILING",      amberLed);

    driveAtt   = std::make_unique<Attachment>(audioProcessor.apvts, "drive",   driveSlider);
    lowCutAtt  = std::make_unique<Attachment>(audioProcessor.apvts, "lowcut",  lowCutSlider);
    punchAtt   = std::make_unique<Attachment>(audioProcessor.apvts, "punch",   punchSlider);
    widthAtt   = std::make_unique<Attachment>(audioProcessor.apvts, "width",   widthSlider);
    ceilingAtt = std::make_unique<Attachment>(audioProcessor.apvts, "ceiling", ceilingSlider);

    startTimerHz(60);
}

NeuralLabAudioProcessorEditor::~NeuralLabAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void NeuralLabAudioProcessorEditor::setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, juce::Colour ledColour) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
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
    currentMeterLevel  = currentMeterLevel * 0.8f + audioProcessor.neuralDSP.getRMSLevel() * 0.2f;
    currentLufsDisplay = currentLufsDisplay * 0.85f + audioProcessor.neuralDSP.getLUFS() * 0.15f;
    currentGrDisplay   = currentGrDisplay * 0.75f + audioProcessor.neuralDSP.getGainReductionDb() * 0.25f;
    repaint();
}

void NeuralLabAudioProcessorEditor::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // 1. Chassis Metallo Spazzolato Dark Hardware
    juce::ColourGradient bgGrad(juce::Colour(0xff161920), 0.0f, 0.0f,
                                juce::Colour(0xff0c0e12), 0.0f, bounds.getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();
    g.setColour(juce::Colour(0xff2d3340));
    g.drawRect(bounds.reduced(2.0f), 2.0f);

    // 2. Header Bar
    g.setColour(juce::Colour(0xff090b0e));
    g.fillRect(0.0f, 0.0f, bounds.getWidth(), 52.0f);
    g.setColour(juce::Colour(0xff262b36));
    g.drawLine(0.0f, 52.0f, bounds.getWidth(), 52.0f, 1.5f);

    g.setColour(juce::Colour(0xfff8fafc));
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText("NEURAL LAB", 24, 9, 180, 34, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff00e5ff));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("PRO STUDIO & AUTO-MASTERING SUITE", 155, 17, 250, 20, juce::Justification::centredLeft);

    // Status LED
    g.setColour(juce::Colour(0xff10b981));
    g.fillEllipse(bounds.getWidth() - 310.0f, 22.0f, 8.0f, 8.0f);
    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(audioProcessor.aiStatusMessage, static_cast<int>(bounds.getWidth()) - 296, 13, 280, 26, juce::Justification::centredLeft);

    // 3. Barra Info Libreria Sample & Reference Track (Sub-bar)
    juce::Rectangle<float> libBar(24.0f, 108.0f, bounds.getWidth() - 48.0f, 34.0f);
    g.setColour(juce::Colour(0xff0e1117));
    g.fillRoundedRectangle(libBar, 4.0f);
    g.setColour(juce::Colour(0xff1f2532));
    g.drawRoundedRectangle(libBar, 4.0f, 1.0f);

    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(11.0f));
    juce::String folderText = "LIBRARIES: " + juce::String(audioProcessor.sampleFolders.size()) + " folder(s) indexed";
    if (audioProcessor.sampleFolders.size() > 0) {
        folderText += " [" + juce::File(audioProcessor.sampleFolders[audioProcessor.sampleFolders.size() - 1]).getFileName() + "]";
    }
    g.drawText(folderText, 165, 113, 350, 24, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xfffbbf24));
    g.drawText("REF: " + audioProcessor.referenceTrackName, 530, 113, 280, 24, juce::Justification::centredRight);

    // 4. Display Sinistro: Analizzatore Spettro FFT
    juce::Rectangle<float> screenArea(24.0f, 152.0f, 540.0f, 245.0f);
    g.setColour(juce::Colour(0xff07090c));
    g.fillRoundedRectangle(screenArea, 6.0f);
    g.setColour(juce::Colour(0xff1e2430));
    g.drawRoundedRectangle(screenArea, 6.0f, 1.5f);

    g.setColour(juce::Colour(0xff141821));
    for (int i = 1; i < 4; ++i) {
        float yLine = screenArea.getY() + (screenArea.getHeight() / 4.0f) * i;
        g.drawLine(screenArea.getX() + 4.0f, yLine, screenArea.getRight() - 4.0f, yLine, 1.0f);
    }

    float barWidth = (screenArea.getWidth() - 16.0f) / static_cast<float>(smoothedBands.size());
    for (size_t i = 0; i < smoothedBands.size(); ++i) {
        float barHeight = smoothedBands[i] * (screenArea.getHeight() - 16.0f);
        float xPos = screenArea.getX() + 8.0f + i * barWidth;
        float yPos = screenArea.getBottom() - 8.0f - barHeight;
        juce::Colour barCol = (i < 16) ? juce::Colour(0xfff59e0b) : juce::Colour(0xff00e5ff);
        g.setColour(barCol.withAlpha(0.85f));
        g.fillRoundedRectangle(xPos, yPos, barWidth - 2.0f, barHeight, 1.5f);
    }

    // 5. Display Centrale: Lista Visiva Stems Selezionati dall'AI
    juce::Rectangle<float> stemsBox(576.0f, 152.0f, 280.0f, 245.0f);
    g.setColour(juce::Colour(0xff090c10));
    g.fillRoundedRectangle(stemsBox, 6.0f);
    g.setColour(juce::Colour(0xff1e2430));
    g.drawRoundedRectangle(stemsBox, 6.0f, 1.5f);

    g.setColour(juce::Colour(0xff00e5ff));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("SELECTED AI STEMS (CLAP ENGINE)", 588, 160, 256, 20, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff1e2430));
    g.drawLine(584.0f, 184.0f, 848.0f, 184.0f, 1.0f);

    if (audioProcessor.selectedStems.empty()) {
        g.setColour(juce::Colour(0xff475569));
        g.setFont(juce::FontOptions(11.0f));
        g.drawFittedText("No stems generated yet.\nAdd a sample folder and click GENERATE TRACK.",
                           590, 220, 250, 60, juce::Justification::centred, 3);
    } else {
        int yRow = 192;
        for (const auto& stem : audioProcessor.selectedStems) {
            if (yRow > 365) break;
            juce::Rectangle<float> rowRect(584.0f, static_cast<float>(yRow), 264.0f, 44.0f);
            g.setColour(juce::Colour(0xff11161f));
            g.fillRoundedRectangle(rowRect, 4.0f);

            g.setColour(juce::Colour(0xfff59e0b));
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText(stem.role.toUpperCase() + "  [" + juce::String(static_cast<int>(stem.similarity * 100)) + "% MATCH]",
                       592, yRow + 4, 245, 14, juce::Justification::centredLeft);

            g.setColour(juce::Colour(0xffe2e8f0));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(stem.filename, 592, yRow + 20, 245, 18, juce::Justification::centredLeft, true);
            yRow += 50;
        }
    }

    // 6. Sezione Misuratori Dedicati Destri: GR (Gain Reduction) + LUFS + OUT
    float meterTop = 152.0f;
    float meterH = 215.0f;

    // Meter 1: Gain Reduction (Ambra dall'alto verso il basso)
    juce::Rectangle<float> grRect(870.0f, meterTop, 22.0f, meterH);
    g.setColour(juce::Colour(0xff07090c));
    g.fillRoundedRectangle(grRect, 3.0f);
    float grNorm = juce::jlimit(0.0f, 1.0f, currentGrDisplay / 12.0f);
    float grBarH = grNorm * (meterH - 4.0f);
    g.setColour(juce::Colour(0xfff59e0b));
    g.fillRoundedRectangle(grRect.getX() + 2.0f, grRect.getY() + 2.0f, 18.0f, grBarH, 2.0f);

    // Meter 2: LUFS Integrato/Momentaneo (Ciano)
    juce::Rectangle<float> lufsRect(900.0f, meterTop, 22.0f, meterH);
    g.setColour(juce::Colour(0xff07090c));
    g.fillRoundedRectangle(lufsRect, 3.0f);
    float lufsNorm = juce::jlimit(0.0f, 1.0f, (currentLufsDisplay + 48.0f) / 48.0f);
    float lufsBarH = lufsNorm * (meterH - 4.0f);
    g.setColour(juce::Colour(0xff00e5ff));
    g.fillRoundedRectangle(lufsRect.getX() + 2.0f, lufsRect.getBottom() - 2.0f - lufsBarH, 18.0f, lufsBarH, 2.0f);

    // Meter 3: Output Peak/RMS (Verde/Rosso)
    juce::Rectangle<float> outRect(930.0f, meterTop, 22.0f, meterH);
    g.setColour(juce::Colour(0xff07090c));
    g.fillRoundedRectangle(outRect, 3.0f);
    float outNorm = juce::jlimit(0.0f, 1.0f, currentMeterLevel * 2.5f);
    float outBarH = outNorm * (meterH - 4.0f);
    g.setColour(outNorm > 0.9f ? juce::Colour(0xffef4444) : juce::Colour(0xff10b981));
    g.fillRoundedRectangle(outRect.getX() + 2.0f, outRect.getBottom() - 2.0f - outBarH, 18.0f, outBarH, 2.0f);

    // Etichette sotto i 3 meter + Valore numerico LUFS
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("GR",   868, 370, 26, 12, juce::Justification::centred);
    g.drawText("LUFS", 896, 370, 30, 12, juce::Justification::centred);
    g.drawText("OUT",  928, 370, 26, 12, juce::Justification::centred);

    g.setColour(juce::Colour(0xff00e5ff));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    juce::String lufsStr = (currentLufsDisplay <= -58.0f) ? "-INF" : juce::String(currentLufsDisplay, 1);
    g.drawText(lufsStr + " LUFS", 866, 384, 90, 14, juce::Justification::centred);

    // 7. Pannello Inferiore Controlli Hardware
    juce::Rectangle<float> bottomRack(24.0f, 412.0f, bounds.getWidth() - 48.0f, 174.0f);
    g.setColour(juce::Colour(0xff12151c));
    g.fillRoundedRectangle(bottomRack, 6.0f);
    g.setColour(juce::Colour(0xff252a36));
    g.drawRoundedRectangle(bottomRack, 6.0f, 1.0f);
}

void NeuralLabAudioProcessorEditor::resized() {
    auto area = getLocalBounds();
    area.removeFromTop(62);

    // Riga 1: Prompt + Pulsanti AI
    auto promptRow = area.removeFromTop(36).reduced(24, 0);
    autoMasterBtn.setBounds(promptRow.removeFromRight(160));
    promptRow.removeFromRight(8);
    generateTrackBtn.setBounds(promptRow.removeFromRight(145));
    promptRow.removeFromRight(10);
    promptInput.setBounds(promptRow);

    // Riga 2: Pulsanti Multi-Folder e Reference Track
    addFolderBtn.setBounds(30, 112, 128, 26);
    loadRefBtn.setBounds(822, 112, 128, 26);

    // Riga 3: 5 Knobs in basso
    auto knobRack = getLocalBounds().removeFromBottom(182).reduced(48, 12);
    int colWidth = knobRack.getWidth() / 5;

    auto placeKnob = [&](juce::Slider& s, juce::Label& l, int idx) {
        auto col = knobRack.withX(knobRack.getX() + idx * colWidth).withWidth(colWidth);
        l.setBounds(col.removeFromTop(20));
        s.setBounds(col.reduced(10, 2));
    };

    placeKnob(driveSlider,   driveLabel,   0);
    placeKnob(lowCutSlider,  lowCutLabel,  1);
    placeKnob(punchSlider,   punchLabel,   2);
    placeKnob(widthSlider,   widthLabel,   3);
    placeKnob(ceilingSlider, ceilingLabel, 4);
}
