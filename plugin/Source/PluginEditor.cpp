#include "PluginEditor.h"

NeuralLabAudioProcessorEditor::NeuralLabAudioProcessorEditor(NeuralLabAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    setLookAndFeel(&darkLookAndFeel);
    setSize(980, 610);

    promptInput.setMultiLine(false);
    promptInput.setTextToShowWhenEmpty("Describe track or arrangement (e.g. 'Dark Melodic Techno 126 BPM, Fm, -8.5 LUFS')...", juce::Colour(0xff6b7280));
    promptInput.setFont(juce::FontOptions(13.5f));
    addAndMakeVisible(promptInput);

    chatDisplayBox.setMultiLine(true, true);
    chatDisplayBox.setReadOnly(true);
    chatDisplayBox.setScrollbarsShown(true);
    chatDisplayBox.setCaretVisible(false);
    chatDisplayBox.setPopupMenuEnabled(true);
    chatDisplayBox.setFont(juce::FontOptions(11.5f));
    chatDisplayBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff070b12));
    chatDisplayBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff1e3a5f));
    chatDisplayBox.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe2e8f0));
    addAndMakeVisible(chatDisplayBox);
    promptInput.onReturnKey = [this]() { auto q = promptInput.getText(); promptInput.clear(); audioProcessor.sendChatQuestion(q); repaint(); };
    addAndMakeVisible(askChatBtn);
    askChatBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0c2d48)); askChatBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
    askChatBtn.onClick = [this]() { auto q = promptInput.getText(); promptInput.clear(); audioProcessor.sendChatQuestion(q); repaint(); };

    generateTrackBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff083344));
    generateTrackBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00e5ff));
    generateTrackBtn.onClick = [this] { audioProcessor.sendPromptToAIEngine(promptInput.getText(), "generate_track"); };
    addAndMakeVisible(generateTrackBtn);

    arrangeFullBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e163b));
    arrangeFullBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffa78bfa));
    arrangeFullBtn.onClick = [this] { audioProcessor.sendPromptToAIEngine(promptInput.getText(), "arrange_full"); };
    addAndMakeVisible(arrangeFullBtn);

    autoMasterBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b1d08));
    autoMasterBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffbbf24));
    autoMasterBtn.onClick = [this] { audioProcessor.sendPromptToAIEngine(promptInput.getText(), "auto_master"); };
    addAndMakeVisible(autoMasterBtn);

    addFolderBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    addFolderBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
    addFolderBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser>("Select Sample Folder", juce::File::getSpecialLocation(juce::File::userDesktopDirectory));
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this](const juce::FileChooser& fc) {
            if (fc.getResult().isDirectory()) audioProcessor.addSampleFolder(fc.getResult());
        });
    };
    addAndMakeVisible(addFolderBtn);

    loadRefBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1c1917));
    loadRefBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffbbf24));
    loadRefBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser>("Select Reference Track", juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.wav;*.mp3;*.flac;*.aiff");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
            if (fc.getResult().existsAsFile()) audioProcessor.setReferenceTrack(fc.getResult());
        });
    };
    addAndMakeVisible(loadRefBtn);

    playRefBtn.setButtonText("> REF");
    playRefBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    playRefBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffbbf24));
    playRefBtn.onClick = [this] {
        audioProcessor.toggleReferencePlayback();
        repaint();
    };
    addAndMakeVisible(playRefBtn);

    mcpToggleBtn.setButtonText("MCP: ON");
    mcpToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff064e3b));
    mcpToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff34d399));
    mcpToggleBtn.onClick = [this] {
        audioProcessor.toggleMcpAgent();
        repaint();
    };
    addAndMakeVisible(mcpToggleBtn);

    for (int i = 0; i < 4; ++i) {
        auditionButtons[i].setButtonText(">");
        auditionButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f172a));
        auditionButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff10b981));
        auditionButtons[i].onClick = [this, i] { audioProcessor.auditionStemAtIndex(i); repaint(); };
        addChildComponent(auditionButtons[i]);

        lockButtons[i].setButtonText("LOCK");
        lockButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f172a));
        lockButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff94a3b8));
        lockButtons[i].onClick = [this, i] { audioProcessor.toggleLockStemAtIndex(i); repaint(); };
        addChildComponent(lockButtons[i]);

        swapButtons[i].setButtonText("SWAP");
        swapButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        swapButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00e5ff));
        swapButtons[i].onClick = [this, i] { audioProcessor.swapStemAtIndex(i); };
        addChildComponent(swapButtons[i]);
    }

    auto cyanLed  = juce::Colour(0xff00e5ff);
    auto amberLed = juce::Colour(0xfff59e0b);
    setupKnob(driveSlider,   driveLabel,   "INPUT DRIVE",  cyanLed);
    setupKnob(lowCutSlider,  lowCutLabel,  "SUB CLEAN",    cyanLed);
    setupKnob(punchSlider,   punchLabel,   "NEURAL PUNCH", amberLed);
    setupKnob(widthSlider,   widthLabel,   "STEREO WIDTH", cyanLed);
    setupKnob(eqMatchSlider, eqMatchLabel, "EQ MATCH %",   cyanLed);
    setupKnob(ceilingSlider, ceilingLabel, "CEILING",      amberLed);

    driveAtt   = std::make_unique<Attachment>(audioProcessor.apvts, "drive",   driveSlider);
    lowCutAtt  = std::make_unique<Attachment>(audioProcessor.apvts, "lowcut",  lowCutSlider);
    punchAtt   = std::make_unique<Attachment>(audioProcessor.apvts, "punch",   punchSlider);
    widthAtt   = std::make_unique<Attachment>(audioProcessor.apvts, "width",   widthSlider);
    eqMatchAtt = std::make_unique<Attachment>(audioProcessor.apvts, "eqmatch", eqMatchSlider);
    ceilingAtt = std::make_unique<Attachment>(audioProcessor.apvts, "ceiling", ceilingSlider);

    splashProgress = 0.0f;
    startTimerHz(60);
}

NeuralLabAudioProcessorEditor::~NeuralLabAudioProcessorEditor() { stopTimer(); setLookAndFeel(nullptr); }

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
    if (splashProgress < 1.0f) splashProgress = juce::jmin(1.0f, splashProgress + 0.008f);
    for (int i = 0; i < 4; ++i) {
        bool show = (i < static_cast<int>(audioProcessor.selectedStems.size()) && splashProgress >= 0.9f);
        auditionButtons[i].setVisible(show);
        lockButtons[i].setVisible(show);
        swapButtons[i].setVisible(show);
        if (show) {
            bool isAud = (audioProcessor.auditioningStemIndex == i);
            auditionButtons[i].setButtonText(isAud ? "||" : ">");
            auditionButtons[i].setColour(juce::TextButton::buttonColourId, isAud ? juce::Colour(0xff065f46) : juce::Colour(0xff0f172a));

            bool isLck = audioProcessor.selectedStems[i].isLocked;
            lockButtons[i].setButtonText(isLck ? "LCKD" : "LOCK");
            lockButtons[i].setColour(juce::TextButton::buttonColourId, isLck ? juce::Colour(0xff78350f) : juce::Colour(0xff0f172a));
            lockButtons[i].setColour(juce::TextButton::textColourOffId, isLck ? juce::Colour(0xfffbbf24) : juce::Colour(0xff94a3b8));

            swapButtons[i].setEnabled(!isLck);
            swapButtons[i].setAlpha(isLck ? 0.45f : 1.0f);
        }
    }
    mcpToggleBtn.setVisible(splashProgress >= 0.9f);
    if (audioProcessor.mcpAgentEnabled) {
        mcpToggleBtn.setButtonText("MCP: ON");
        mcpToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff064e3b));
        mcpToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff34d399));
    } else {
        mcpToggleBtn.setButtonText("MCP: OFF");
        mcpToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        mcpToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff64748b));
    }

    chatDisplayBox.setVisible(splashProgress >= 0.9f);
    if (audioProcessor.chatDirty || audioProcessor.chatHistory.size() != lastChatCount) {
        audioProcessor.chatDirty = false;
        lastChatCount = audioProcessor.chatHistory.size();
        chatDisplayBox.clear();
        for (size_t i = 0; i < audioProcessor.chatHistory.size(); ++i) {
            const auto& entry = audioProcessor.chatHistory[i];
            juce::String line = entry.text;
            if (!line.startsWith("YOU:"))
                line = "NEURAL-LAB: " + line;
            juce::Colour lineCol = entry.isError
                ? juce::Colour(0xfff87171)
                : (line.startsWith("YOU:") ? juce::Colour(0xff38bdf8) : juce::Colour(0xffe2e8f0));
            chatDisplayBox.setColour(juce::TextEditor::textColourId, lineCol);
            chatDisplayBox.insertTextAtCaret(line + (i + 1 < audioProcessor.chatHistory.size() ? "\n\n" : ""));
        }
        chatDisplayBox.moveCaretToEnd();
    }

    bool hasRef = audioProcessor.referenceTrackPath.isNotEmpty();
    bool isRefPlaying = audioProcessor.isReferencePlaying();
    playRefBtn.setVisible(splashProgress >= 0.9f);
    playRefBtn.setEnabled(hasRef);
    playRefBtn.setAlpha(hasRef ? 1.0f : 0.45f);
    playRefBtn.setButtonText(isRefPlaying ? "|| REF" : "> REF");
    playRefBtn.setColour(juce::TextButton::buttonColourId, isRefPlaying ? juce::Colour(0xff78350f) : juce::Colour(0xff1e293b));
    if (audioProcessor.aiStatusMessage.containsIgnoreCase("STARTING")) {
        if (++pingTickCounter >= 90) {
            pingTickCounter = 0;
            audioProcessor.sendOscPing();
        }
    }
    repaint();
}

void NeuralLabAudioProcessorEditor::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient bgGrad(juce::Colour(0xff161920), 0.0f, 0.0f, juce::Colour(0xff0c0e12), 0.0f, bounds.getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();
    g.setColour(juce::Colour(0xff2d3340));
    g.drawRect(bounds.reduced(2.0f), 2.0f);

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

    g.setColour(juce::Colour(0xff10b981));
    g.fillEllipse(bounds.getWidth() - 310.0f, 22.0f, 8.0f, 8.0f);
    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(audioProcessor.aiStatusMessage, static_cast<int>(bounds.getWidth()) - 296, 13, 280, 26, juce::Justification::centredLeft);

    juce::Rectangle<float> libBar(24.0f, 108.0f, bounds.getWidth() - 48.0f, 34.0f);
    g.setColour(juce::Colour(0xff0e1117));
    g.fillRoundedRectangle(libBar, 4.0f);
    g.setColour(juce::Colour(0xff1f2532));
    g.drawRoundedRectangle(libBar, 4.0f, 1.0f);

    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(11.0f));
    juce::String folderText = "LIBRARIES: " + juce::String(audioProcessor.sampleFolders.size()) + " folder(s)";
    if (audioProcessor.sampleFolders.size() > 0) folderText += " [" + juce::File(audioProcessor.sampleFolders[audioProcessor.sampleFolders.size() - 1]).getFileName() + "]";
    g.drawText(folderText, 165, 113, 285, 24, juce::Justification::centredLeft, true);

    g.setColour(juce::Colour(0xfffbbf24));
    g.drawText("REF: " + audioProcessor.referenceTrackName, 456, 113, 318, 24, juce::Justification::centredRight, true);

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

    if (audioProcessor.hasReferenceCurve) {
        juce::Path refPath;
        for (size_t i = 0; i < 64; ++i) {
            float xPos = screenArea.getX() + 8.0f + (i + 0.5f) * barWidth;
            float yPos = screenArea.getBottom() - 12.0f - audioProcessor.referenceEqCurve[i] * (screenArea.getHeight() - 24.0f);
            if (i == 0) refPath.startNewSubPath(xPos, yPos);
            else refPath.lineTo(xPos, yPos);
        }
        g.setColour(juce::Colour(0xfffbbf24).withAlpha(0.95f));
        g.strokePath(refPath, juce::PathStrokeType(2.2f));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText("REF TARGET EQ CURVE", static_cast<int>(screenArea.getRight()) - 150, static_cast<int>(screenArea.getY()) + 6, 140, 14, juce::Justification::centredRight);
    }

    // AI Copilot Chat rendered by scrollable chatDisplayBox

    juce::Rectangle<float> stemsBox(576.0f, 152.0f, 280.0f, 245.0f);
    g.setColour(juce::Colour(0xff090c10));
    g.fillRoundedRectangle(stemsBox, 6.0f);
    g.setColour(juce::Colour(0xff1e2430));
    g.drawRoundedRectangle(stemsBox, 6.0f, 1.5f);

    g.setColour(juce::Colour(0xff00e5ff));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("AI STEMS & MIDI (CLICK SWAP)", 588, 160, 256, 20, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff1e2430));
    g.drawLine(584.0f, 184.0f, 848.0f, 184.0f, 1.0f);

    if (audioProcessor.selectedStems.empty()) {
        g.setColour(juce::Colour(0xff475569));
        g.setFont(juce::FontOptions(11.0f));
        g.drawFittedText("No stems active.\nClick GENERATE TRACK or ARRANGE FULL.", 590, 220, 250, 60, juce::Justification::centred, 3);
    } else {
        int yRow = 192;
        for (size_t idx = 0; idx < audioProcessor.selectedStems.size() && idx < 4; ++idx) {
            const auto& stem = audioProcessor.selectedStems[idx];
            juce::Rectangle<float> rowRect(584.0f, static_cast<float>(yRow), 264.0f, 44.0f);
            g.setColour(juce::Colour(0xff11161f));
            g.fillRoundedRectangle(rowRect, 4.0f);
            g.setColour(juce::Colour(0xfff59e0b));
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText(stem.role.toUpperCase() + " [" + juce::String(static_cast<int>(stem.similarity * 100)) + "%]", 592, yRow + 4, 138, 14, juce::Justification::centredLeft);
            g.setColour(juce::Colour(0xffe2e8f0));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(stem.filename, 592, yRow + 20, 138, 18, juce::Justification::centredLeft, true);
            yRow += 50;
        }
    }

    float meterTop = 152.0f, meterH = 215.0f;
    juce::Rectangle<float> grRect(870.0f, meterTop, 22.0f, meterH);
    g.setColour(juce::Colour(0xff07090c)); g.fillRoundedRectangle(grRect, 3.0f);
    float grBarH = juce::jlimit(0.0f, 1.0f, currentGrDisplay / 12.0f) * (meterH - 4.0f);
    g.setColour(juce::Colour(0xfff59e0b)); g.fillRoundedRectangle(grRect.getX() + 2.0f, grRect.getY() + 2.0f, 18.0f, grBarH, 2.0f);

    juce::Rectangle<float> lufsRect(900.0f, meterTop, 22.0f, meterH);
    g.setColour(juce::Colour(0xff07090c)); g.fillRoundedRectangle(lufsRect, 3.0f);
    float lufsBarH = juce::jlimit(0.0f, 1.0f, (currentLufsDisplay + 48.0f) / 48.0f) * (meterH - 4.0f);
    g.setColour(juce::Colour(0xff00e5ff)); g.fillRoundedRectangle(lufsRect.getX() + 2.0f, lufsRect.getBottom() - 2.0f - lufsBarH, 18.0f, lufsBarH, 2.0f);

    juce::Rectangle<float> outRect(930.0f, meterTop, 22.0f, meterH);
    g.setColour(juce::Colour(0xff07090c)); g.fillRoundedRectangle(outRect, 3.0f);
    float outNorm = juce::jlimit(0.0f, 1.0f, currentMeterLevel * 2.5f);
    float outBarH = outNorm * (meterH - 4.0f);
    g.setColour(outNorm > 0.9f ? juce::Colour(0xffef4444) : juce::Colour(0xff10b981));
    g.fillRoundedRectangle(outRect.getX() + 2.0f, outRect.getBottom() - 2.0f - outBarH, 18.0f, outBarH, 2.0f);

    g.setColour(juce::Colour(0xff94a3b8)); g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("GR", 868, 370, 26, 12, juce::Justification::centred);
    g.drawText("LUFS", 896, 370, 30, 12, juce::Justification::centred);
    g.drawText("OUT", 928, 370, 26, 12, juce::Justification::centred);
    g.setColour(juce::Colour(0xff00e5ff)); g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    juce::String lufsStr = (currentLufsDisplay <= -58.0f) ? "-INF" : juce::String(currentLufsDisplay, 1);
    g.drawText(lufsStr + " LUFS", 866, 384, 90, 14, juce::Justification::centred);

    juce::Rectangle<float> bottomRack(24.0f, 412.0f, bounds.getWidth() - 48.0f, 174.0f);
    g.setColour(juce::Colour(0xff12151c)); g.fillRoundedRectangle(bottomRack, 6.0f);
    g.setColour(juce::Colour(0xff252a36)); g.drawRoundedRectangle(bottomRack, 6.0f, 1.0f);

    if (splashProgress < 1.0f) {
        float alpha = (splashProgress > 0.85f) ? (1.0f - (splashProgress - 0.85f) / 0.15f) : 1.0f;
        g.setColour(juce::Colour(0xff090b0f).withAlpha(alpha)); g.fillAll();
        g.setColour(juce::Colour(0xff00e5ff).withAlpha(alpha)); g.drawRoundedRectangle(bounds.reduced(16.0f), 8.0f, 1.5f);
        g.setFont(juce::FontOptions(34.0f, juce::Font::bold)); g.setColour(juce::Colours::white.withAlpha(alpha));
        g.drawText("NEURAL LAB", 0, 210, getWidth(), 44, juce::Justification::centred);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold)); g.setColour(juce::Colour(0xfff59e0b).withAlpha(alpha));
        g.drawText("PRO STUDIO & AUTO-MASTERING WORKSTATION  v1.1", 0, 256, getWidth(), 22, juce::Justification::centred);
        float barW = 420.0f, barX = (getWidth() - barW) * 0.5f;
        g.setColour(juce::Colour(0xff1e2430).withAlpha(alpha)); g.fillRoundedRectangle(barX, 310.0f, barW, 8.0f, 4.0f);
        g.setColour(juce::Colour(0xff00e5ff).withAlpha(alpha)); g.fillRoundedRectangle(barX, 310.0f, barW * splashProgress, 8.0f, 4.0f);
        juce::String stepTxt = splashProgress < 0.35f ? "INITIALIZING 64-BIT NEURAL DSP ENGINE..." : (splashProgress < 0.70f ? "MOUNTING LAION-CLAP & REFERENCE EQ CORTEX..." : "CONNECTING 0.5B ORCHESTRATOR & ABLETON ARRANGER...");
        g.setFont(juce::FontOptions(11.0f)); g.setColour(juce::Colour(0xff94a3b8).withAlpha(alpha));
        g.drawText(stepTxt, 0, 328, getWidth(), 20, juce::Justification::centred);
    }
}

void NeuralLabAudioProcessorEditor::resized() {
    auto area = getLocalBounds();
    area.removeFromTop(62);
    auto promptRow = area.removeFromTop(36).reduced(24, 0);
    autoMasterBtn.setBounds(promptRow.removeFromRight(120));
    promptRow.removeFromRight(6);
    arrangeFullBtn.setBounds(promptRow.removeFromRight(125));
    promptRow.removeFromRight(6);
    generateTrackBtn.setBounds(promptRow.removeFromRight(130));
    promptRow.removeFromRight(8);
    askChatBtn.setBounds(promptRow.removeFromRight(112));
    promptRow.removeFromRight(6);
    chatDisplayBox.setBounds(32, 317, 524, 76);
    promptInput.setBounds(promptRow);

    addFolderBtn.setBounds(30, 112, 128, 26);
    playRefBtn.setBounds(784, 112, 54, 26);
    loadRefBtn.setBounds(842, 112, 108, 26);
    mcpToggleBtn.setBounds(435, 13, 88, 26);

    for (int i = 0; i < 4; ++i) {
        int yBtn = 200 + i * 50;
        auditionButtons[i].setBounds(730, yBtn, 26, 26);
        lockButtons[i].setBounds(758, yBtn, 38, 26);
        swapButtons[i].setBounds(798, yBtn, 44, 26);
    }

    auto knobRack = getLocalBounds().removeFromBottom(182).reduced(48, 12);
    int colWidth = knobRack.getWidth() / 6;
    auto placeKnob = [&](juce::Slider& s, juce::Label& l, int idx) {
        auto col = knobRack.withX(knobRack.getX() + idx * colWidth).withWidth(colWidth);
        l.setBounds(col.removeFromTop(20));
        s.setBounds(col.reduced(10, 2));
    };
    placeKnob(driveSlider,   driveLabel,   0);
    placeKnob(lowCutSlider,  lowCutLabel,  1);
    placeKnob(punchSlider,   punchLabel,   2);
    placeKnob(widthSlider,   widthLabel,   3);
    placeKnob(eqMatchSlider, eqMatchLabel, 4);
    placeKnob(ceilingSlider, ceilingLabel, 5);
}