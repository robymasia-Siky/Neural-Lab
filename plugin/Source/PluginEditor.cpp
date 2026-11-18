#include "PluginEditor.h"

namespace {
    const juce::Colour kBgDeep        (0xff06080d);
    const juce::Colour kBgObsidian    (0xff0b0f17);
    const juce::Colour kGlassFillTop  (0xeb101622);
    const juce::Colour kGlassFillBot  (0xf2080b12);
    const juce::Colour kGlassBorder   (0xff1e293b);
    const juce::Colour kGlassHighlight(0x28ffffff);
    const juce::Colour kCyanNeon      (0xff00e5ff);
    const juce::Colour kCyanSoft      (0xff38bdf8);
    const juce::Colour kAmberNeon     (0xfffbbf24);
    const juce::Colour kAmberDeep     (0xfff59e0b);
    const juce::Colour kEmeraldNeon   (0xff10b981);
    const juce::Colour kVioletNeon    (0xffa78bfa);
    const juce::Colour kTextPrimary   (0xfff1f5f9);
    const juce::Colour kTextSecondary (0xff94a3b8);
    const juce::Colour kTextMuted     (0xff475569);

    void drawObsidianCard(juce::Graphics& g, juce::Rectangle<float> bounds, float radius = 8.0f, juce::Colour accentBorder = juce::Colour()) {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 3.0f), radius);

        juce::ColourGradient glassGrad(kGlassFillTop, bounds.getX(), bounds.getY(),
                                       kGlassFillBot, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(bounds, radius);

        g.setColour(kGlassHighlight);
        g.drawHorizontalLine(static_cast<int>(bounds.getY() + 1.0f), bounds.getX() + radius, bounds.getRight() - radius);

        g.setColour(accentBorder.isTransparent() ? kGlassBorder : accentBorder.withAlpha(0.45f));
        g.drawRoundedRectangle(bounds, radius, 1.15f);
    }
}

NeuralLabAudioProcessorEditor::NeuralLabAudioProcessorEditor(NeuralLabAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    setLookAndFeel(&darkLookAndFeel);
    setSize(1160, 680);

    chatDisplayBox.setMultiLine(true, true);
    chatDisplayBox.setReadOnly(true);
    chatDisplayBox.setScrollbarsShown(true);
    chatDisplayBox.setCaretVisible(false);
    chatDisplayBox.setPopupMenuEnabled(true);
    chatDisplayBox.setFont(juce::FontOptions(12.0f));
    chatDisplayBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff070a11));
    chatDisplayBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff172234));
    chatDisplayBox.setColour(juce::TextEditor::textColourId, kTextPrimary);
    addAndMakeVisible(chatDisplayBox);

    promptInput.setMultiLine(false);
    promptInput.setTextToShowWhenEmpty("Ask Copilot or describe track (e.g. 'Hypnotic Techno 128 BPM, Fm')...", juce::Colour(0xff64748b));
    promptInput.setFont(juce::FontOptions(12.5f));
    promptInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff090d16));
    promptInput.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff1e3a5f));
    promptInput.setColour(juce::TextEditor::focusedOutlineColourId, kCyanNeon);
    promptInput.setColour(juce::TextEditor::textColourId, kTextPrimary);
    promptInput.onReturnKey = [this]() {
        auto q = promptInput.getText().trim();
        if (q.isNotEmpty()) {
            promptInput.clear();
            audioProcessor.sendChatQuestion(q);
            repaint();
        }
    };
    addAndMakeVisible(promptInput);

    askChatBtn.setButtonText("SEND");
    askChatBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0c2d48));
    askChatBtn.setColour(juce::TextButton::textColourOffId, kCyanSoft);
    askChatBtn.onClick = [this]() {
        auto q = promptInput.getText().trim();
        if (q.isNotEmpty()) {
            promptInput.clear();
            audioProcessor.sendChatQuestion(q);
            repaint();
        }
    };
    addAndMakeVisible(askChatBtn);

    generateTrackBtn.setButtonText("GENERATE");
    generateTrackBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff083344));
    generateTrackBtn.setColour(juce::TextButton::textColourOffId, kCyanNeon);
    generateTrackBtn.onClick = [this] { audioProcessor.sendPromptToAIEngine(promptInput.getText(), "generate_track"); };
    addAndMakeVisible(generateTrackBtn);

    arrangeFullBtn.setButtonText("ARRANGE");
    arrangeFullBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e163b));
    arrangeFullBtn.setColour(juce::TextButton::textColourOffId, kVioletNeon);
    arrangeFullBtn.onClick = [this] { audioProcessor.sendPromptToAIEngine(promptInput.getText(), "arrange_full"); };
    addAndMakeVisible(arrangeFullBtn);

    autoMasterBtn.setButtonText("MASTER");
    autoMasterBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b1d08));
    autoMasterBtn.setColour(juce::TextButton::textColourOffId, kAmberNeon);
    autoMasterBtn.onClick = [this] { audioProcessor.sendPromptToAIEngine(promptInput.getText(), "auto_master"); };
    addAndMakeVisible(autoMasterBtn);

    mcpToggleBtn.setButtonText("MCP: ON");
    mcpToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff064e3b));
    mcpToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff34d399));
    mcpToggleBtn.onClick = [this] {
        audioProcessor.toggleMcpAgent();
        repaint();
    };
    addAndMakeVisible(mcpToggleBtn);

    addFolderBtn.setButtonText("+ SAMPLES");
    addFolderBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff111827));
    addFolderBtn.setColour(juce::TextButton::textColourOffId, kCyanSoft);
    addFolderBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser>("Select Sample Folder", juce::File::getSpecialLocation(juce::File::userDesktopDirectory));
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this](const juce::FileChooser& fc) {
            if (fc.getResult().isDirectory()) audioProcessor.addSampleFolder(fc.getResult());
        });
    };
    addAndMakeVisible(addFolderBtn);

    loadRefBtn.setButtonText("LOAD REF");
    loadRefBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1c1917));
    loadRefBtn.setColour(juce::TextButton::textColourOffId, kAmberNeon);
    loadRefBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser>("Select Reference Track", juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.wav;*.mp3;*.flac;*.aiff");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
            if (fc.getResult().existsAsFile()) audioProcessor.setReferenceTrack(fc.getResult());
        });
    };
    addAndMakeVisible(loadRefBtn);

    playRefBtn.setButtonText("> REF");
    playRefBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    playRefBtn.setColour(juce::TextButton::textColourOffId, kAmberNeon);
    playRefBtn.onClick = [this] {
        audioProcessor.toggleReferencePlayback();
        repaint();
    };
    addAndMakeVisible(playRefBtn);

    for (int i = 0; i < 4; ++i) {
        auditionButtons[i].setButtonText(">");
        auditionButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f172a));
        auditionButtons[i].setColour(juce::TextButton::textColourOffId, kEmeraldNeon);
        auditionButtons[i].onClick = [this, i] { audioProcessor.auditionStemAtIndex(i); repaint(); };
        addChildComponent(auditionButtons[i]);

        lockButtons[i].setButtonText("LOCK");
        lockButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f172a));
        lockButtons[i].setColour(juce::TextButton::textColourOffId, kTextSecondary);
        lockButtons[i].onClick = [this, i] { audioProcessor.toggleLockStemAtIndex(i); repaint(); };
        addChildComponent(lockButtons[i]);

        swapButtons[i].setButtonText("SWAP");
        swapButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        swapButtons[i].setColour(juce::TextButton::textColourOffId, kCyanNeon);
        swapButtons[i].onClick = [this, i] { audioProcessor.swapStemAtIndex(i); };
        addChildComponent(swapButtons[i]);
    }

    setupKnob(driveSlider,   driveLabel,   "INPUT DRIVE",  kCyanNeon);
    setupKnob(lowCutSlider,  lowCutLabel,  "SUB CLEAN",    kCyanNeon);
    setupKnob(punchSlider,   punchLabel,   "NEURAL PUNCH", kAmberDeep);
    setupKnob(widthSlider,   widthLabel,   "STEREO WIDTH", kCyanNeon);
    setupKnob(eqMatchSlider, eqMatchLabel, "EQ MATCH %",   kCyanNeon);
    setupKnob(ceilingSlider, ceilingLabel, "CEILING",      kAmberDeep);

    driveAtt   = std::make_unique<Attachment>(audioProcessor.apvts, "drive",   driveSlider);
    lowCutAtt  = std::make_unique<Attachment>(audioProcessor.apvts, "lowcut",  lowCutSlider);
    punchAtt   = std::make_unique<Attachment>(audioProcessor.apvts, "punch",   punchSlider);
    widthAtt   = std::make_unique<Attachment>(audioProcessor.apvts, "width",   widthSlider);
    eqMatchAtt = std::make_unique<Attachment>(audioProcessor.apvts, "eqmatch", eqMatchSlider);
    ceilingAtt = std::make_unique<Attachment>(audioProcessor.apvts, "ceiling", ceilingSlider);

    splashProgress = 0.0f;
    startTimerHz(60);
}

NeuralLabAudioProcessorEditor::~NeuralLabAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void NeuralLabAudioProcessorEditor::setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text, juce::Colour ledColour) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 17);
    slider.setColour(juce::Slider::rotarySliderFillColourId, ledColour);
    slider.setColour(juce::Slider::textBoxTextColourId, kTextPrimary);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, kTextSecondary);
    addAndMakeVisible(label);
}

void NeuralLabAudioProcessorEditor::timerCallback() {
    if (audioProcessor.neuralDSP.getNextFFTBlock(fftMagnitudes)) {
        for (size_t i = 0; i < smoothedBands.size(); ++i) {
            size_t bin = static_cast<size_t>(std::pow(1.09, static_cast<double>(i)));
            bin = juce::jmin(bin, fftMagnitudes.size() - 1);
            float db = juce::Decibels::gainToDecibels(fftMagnitudes[bin] / static_cast<float>(NeuralDSP::fftSize)) + 60.0f;
            float norm = juce::jlimit(0.0f, 1.0f, db / 60.0f);
            smoothedBands[i] = smoothedBands[i] * 0.74f + norm * 0.26f;
        }
    }

    currentMeterLevel  = currentMeterLevel * 0.80f + audioProcessor.neuralDSP.getRMSLevel() * 0.20f;
    currentLufsDisplay = currentLufsDisplay * 0.85f + audioProcessor.neuralDSP.getLUFS() * 0.15f;
    currentGrDisplay   = currentGrDisplay * 0.75f + audioProcessor.neuralDSP.getGainReductionDb() * 0.25f;

    if (splashProgress < 1.0f)
        splashProgress = juce::jmin(1.0f, splashProgress + 0.012f);

    bool uiReady = (splashProgress >= 0.92f);

    for (int i = 0; i < 4; ++i) {
        bool show = (i < static_cast<int>(audioProcessor.selectedStems.size()) && uiReady);
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
            lockButtons[i].setColour(juce::TextButton::textColourOffId, isLck ? kAmberNeon : kTextSecondary);

            swapButtons[i].setEnabled(!isLck);
            swapButtons[i].setAlpha(isLck ? 0.45f : 1.0f);
        }
    }

    mcpToggleBtn.setVisible(uiReady);
    addFolderBtn.setVisible(uiReady);
    loadRefBtn.setVisible(uiReady);
    playRefBtn.setVisible(uiReady);
    chatDisplayBox.setVisible(uiReady);
    promptInput.setVisible(uiReady);
    askChatBtn.setVisible(uiReady);
    generateTrackBtn.setVisible(uiReady);
    arrangeFullBtn.setVisible(uiReady);
    autoMasterBtn.setVisible(uiReady);

    if (audioProcessor.mcpAgentEnabled) {
        mcpToggleBtn.setButtonText("MCP: ON");
        mcpToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff064e3b));
        mcpToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff34d399));
    } else {
        mcpToggleBtn.setButtonText("MCP: OFF");
        mcpToggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        mcpToggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff64748b));
    }

    if (audioProcessor.chatDirty || audioProcessor.chatHistory.size() != lastChatCount) {
        audioProcessor.chatDirty = false;
        lastChatCount = audioProcessor.chatHistory.size();
        chatDisplayBox.clear();
        for (size_t i = 0; i < audioProcessor.chatHistory.size(); ++i) {
            const auto& entry = audioProcessor.chatHistory[i];
            juce::String line = entry.text;
            if (!line.startsWith("YOU:"))
                line = "COPILOT: " + line;
            juce::Colour lineCol = entry.isError
                ? juce::Colour(0xfff87171)
                : (line.startsWith("YOU:") ? kCyanNeon : kTextPrimary);
            chatDisplayBox.setColour(juce::TextEditor::textColourId, lineCol);
            chatDisplayBox.insertTextAtCaret(line + (i + 1 < audioProcessor.chatHistory.size() ? "\n\n" : ""));
        }
        chatDisplayBox.moveCaretToEnd();
    }

    bool hasRef = audioProcessor.referenceTrackPath.isNotEmpty();
    bool isRefPlaying = audioProcessor.isReferencePlaying();
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

    juce::ColourGradient bgGrad(kBgObsidian, 0.0f, 0.0f, kBgDeep, bounds.getWidth(), bounds.getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    juce::ColourGradient cyanGlow(kCyanNeon.withAlpha(0.07f), 140.0f, 90.0f,
                                  juce::Colours::transparentBlack, 480.0f, 340.0f, true);
    g.setGradientFill(cyanGlow);
    g.fillEllipse(-120.0f, -80.0f, 700.0f, 520.0f);

    juce::ColourGradient amberGlow(kAmberDeep.withAlpha(0.06f), bounds.getWidth() - 180.0f, bounds.getHeight() - 120.0f,
                                   juce::Colours::transparentBlack, bounds.getWidth() - 520.0f, bounds.getHeight() - 400.0f, true);
    g.setGradientFill(amberGlow);
    g.fillEllipse(bounds.getWidth() - 540.0f, bounds.getHeight() - 420.0f, 640.0f, 500.0f);

    g.setColour(kGlassBorder);
    g.drawRect(bounds.reduced(1.0f), 1.5f);

    g.setColour(juce::Colour(0xd9070a10));
    g.fillRect(0.0f, 0.0f, bounds.getWidth(), 52.0f);
    g.setColour(kGlassBorder);
    g.drawLine(0.0f, 52.0f, bounds.getWidth(), 52.0f, 1.0f);

    float iconX = 20.0f, iconY = 16.0f;
    const float barHeights[5] = { 8.0f, 16.0f, 20.0f, 13.0f, 7.0f };
    for (int b = 0; b < 5; ++b) {
        g.setColour(b == 2 ? kAmberNeon : kCyanNeon);
        g.fillRoundedRectangle(iconX + b * 4.5f, iconY + (20.0f - barHeights[b]) * 0.5f, 2.5f, barHeights[b], 1.2f);
    }

    g.setColour(kTextPrimary);
    g.setFont(juce::FontOptions(19.0f, juce::Font::bold));
    g.drawText("NEURAL-LAB", 48, 9, 145, 34, juce::Justification::centredLeft);

    g.setColour(kCyanNeon.withAlpha(0.85f));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("OBSIDIAN GLASS STUDIO  |  LOCAL AI & DSP", 195, 17, 260, 20, juce::Justification::centredLeft);

    juce::Rectangle<float> statusPill(bounds.getWidth() - 320.0f, 12.0f, 300.0f, 28.0f);
    g.setColour(juce::Colour(0xff0b111d));
    g.fillRoundedRectangle(statusPill, 14.0f);
    g.setColour(kGlassBorder);
    g.drawRoundedRectangle(statusPill, 14.0f, 1.0f);

    bool isOffline = audioProcessor.aiStatusMessage.containsIgnoreCase("OFFLINE") ||
                     audioProcessor.aiStatusMessage.containsIgnoreCase("ERROR");
    g.setColour(isOffline ? juce::Colour(0xffef4444) : kEmeraldNeon);
    g.fillEllipse(statusPill.getX() + 12.0f, statusPill.getY() + 10.0f, 8.0f, 8.0f);

    g.setColour(kTextPrimary);
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText(audioProcessor.aiStatusMessage,
               static_cast<int>(statusPill.getX() + 26.0f), 12,
               static_cast<int>(statusPill.getWidth() - 34.0f), 28,
               juce::Justification::centredLeft, true);

    const float margin = 16.0f;
    const float topY = 64.0f;
    const float totalH = bounds.getHeight() - topY - margin;
    const float rightColW = 356.0f;
    const float leftColW = bounds.getWidth() - rightColW - (margin * 3.0f);
    const float rightColX = margin * 2.0f + leftColW;

    const float specH = 316.0f;
    juce::Rectangle<float> specCard(margin, topY, leftColW, specH);
    drawObsidianCard(g, specCard, 10.0f, kCyanNeon);

    g.setColour(kTextPrimary);
    g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    g.drawText("SPECTRUM ANALYZER", static_cast<int>(specCard.getX() + 16), static_cast<int>(specCard.getY() + 10), 180, 22, juce::Justification::centredLeft);

    g.setColour(kCyanNeon);
    g.fillRoundedRectangle(specCard.getX() + 185.0f, specCard.getY() + 17.0f, 10.0f, 3.0f, 1.5f);
    g.setColour(kTextSecondary);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("LIVE MIX", static_cast<int>(specCard.getX() + 200), static_cast<int>(specCard.getY() + 10), 65, 20, juce::Justification::centredLeft);

    g.setColour(kAmberNeon);
    g.fillRoundedRectangle(specCard.getX() + 268.0f, specCard.getY() + 17.0f, 10.0f, 3.0f, 1.5f);
    g.setColour(kTextSecondary);
    g.drawText(audioProcessor.hasReferenceCurve ? "REF TARGET" : "NO REF",
               static_cast<int>(specCard.getX() + 283), static_cast<int>(specCard.getY() + 10), 85, 20, juce::Justification::centredLeft);

    g.setColour(kAmberNeon.withAlpha(0.9f));
    g.setFont(juce::FontOptions(10.5f));
    g.drawText(audioProcessor.referenceTrackName,
               static_cast<int>(specCard.getRight() - 270), static_cast<int>(specCard.getY() + 10), 170, 20,
               juce::Justification::centredRight, true);

    const float metersAreaW = 86.0f;
    juce::Rectangle<float> plotRect(specCard.getX() + 36.0f, specCard.getY() + 42.0f,
                                    specCard.getWidth() - 36.0f - metersAreaW - 14.0f,
                                    specCard.getHeight() - 68.0f);

    g.setColour(juce::Colour(0xff05070c));
    g.fillRoundedRectangle(plotRect, 6.0f);
    g.setColour(juce::Colour(0xff151e2e));
    g.drawRoundedRectangle(plotRect, 6.0f, 1.0f);

    const char* dbLabels[4] = { "0 dB", "-18", "-36", "-54" };
    g.setFont(juce::FontOptions(9.0f));
    for (int row = 0; row < 4; ++row) {
        float yLine = plotRect.getY() + (plotRect.getHeight() / 4.0f) * static_cast<float>(row);
        if (row > 0) {
            g.setColour(juce::Colour(0xff111827));
            g.drawLine(plotRect.getX() + 1.0f, yLine, plotRect.getRight() - 1.0f, yLine, 1.0f);
        }
        g.setColour(kTextMuted);
        g.drawText(dbLabels[row], static_cast<int>(specCard.getX() + 4), static_cast<int>(yLine - 6.0f), 28, 12, juce::Justification::centredRight);
    }

    const char* freqLabels[7] = { "30Hz", "80", "200", "500", "1.5k", "5k", "16k" };
    for (int col = 0; col < 7; ++col) {
        float frac = static_cast<float>(col) / 6.0f;
        float xLine = plotRect.getX() + frac * plotRect.getWidth();
        if (col > 0 && col < 6) {
            g.setColour(juce::Colour(0xff111827));
            g.drawLine(xLine, plotRect.getY() + 1.0f, xLine, plotRect.getBottom() - 1.0f, 1.0f);
        }
        g.setColour(kTextMuted);
        g.drawText(freqLabels[col], static_cast<int>(xLine - 18.0f), static_cast<int>(plotRect.getBottom() + 4.0f), 36, 14, juce::Justification::centred);
    }

    auto buildSmoothCurve = [&](const std::array<float, 64>& data, juce::Path& strokePath, juce::Path& fillPath) {
        std::array<juce::Point<float>, 64> pts;
        for (size_t i = 0; i < 64; ++i) {
            float t = static_cast<float>(i) / 63.0f;
            float x = plotRect.getX() + t * plotRect.getWidth();
            float y = plotRect.getBottom() - juce::jlimit(0.0f, 1.0f, data[i]) * (plotRect.getHeight() - 8.0f);
            pts[i] = { x, y };
        }
        strokePath.startNewSubPath(pts[0]);
        fillPath.startNewSubPath(plotRect.getX(), plotRect.getBottom());
        fillPath.lineTo(pts[0]);

        for (size_t i = 0; i < 63; ++i) {
            juce::Point<float> mid((pts[i].x + pts[i + 1].x) * 0.5f, (pts[i].y + pts[i + 1].y) * 0.5f);
            strokePath.quadraticTo(pts[i], mid);
            fillPath.quadraticTo(pts[i], mid);
        }
        strokePath.lineTo(pts[63]);
        fillPath.lineTo(pts[63]);
        fillPath.lineTo(plotRect.getRight(), plotRect.getBottom());
        fillPath.closeSubPath();
    };

    if (audioProcessor.hasReferenceCurve) {
        juce::Path refStroke, refFill;
        buildSmoothCurve(audioProcessor.referenceEqCurve, refStroke, refFill);

        juce::ColourGradient amberFillGrad(kAmberNeon.withAlpha(0.24f), 0.0f, plotRect.getY(),
                                           kAmberDeep.withAlpha(0.01f), 0.0f, plotRect.getBottom(), false);
        g.setGradientFill(amberFillGrad);
        g.fillPath(refFill);

        g.setColour(kAmberNeon.withAlpha(0.95f));
        g.strokePath(refStroke, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    {
        juce::Path liveStroke, liveFill;
        buildSmoothCurve(smoothedBands, liveStroke, liveFill);

        juce::ColourGradient cyanFillGrad(kCyanNeon.withAlpha(0.32f), 0.0f, plotRect.getY(),
                                          kCyanSoft.withAlpha(0.02f), 0.0f, plotRect.getBottom(), false);
        g.setGradientFill(cyanFillGrad);
        g.fillPath(liveFill);

        g.setColour(kCyanNeon.withAlpha(0.25f));
        g.strokePath(liveStroke, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(kCyanNeon);
        g.strokePath(liveStroke, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    float meterX0 = plotRect.getRight() + 12.0f;
    float meterY0 = plotRect.getY();
    float meterH  = plotRect.getHeight() - 18.0f;

    auto drawSlimMeter = [&](float x, float normVal, juce::Colour col, const char* tag) {
        juce::Rectangle<float> mBg(x, meterY0, 18.0f, meterH);
        g.setColour(juce::Colour(0xff05070c));
        g.fillRoundedRectangle(mBg, 4.0f);
        g.setColour(juce::Colour(0xff1e293b));
        g.drawRoundedRectangle(mBg, 4.0f, 1.0f);

        float fillH = juce::jlimit(0.0f, 1.0f, normVal) * (meterH - 4.0f);
        if (tag[0] == 'G' && tag[1] == 'R') {
            g.setColour(col);
            g.fillRoundedRectangle(x + 2.0f, meterY0 + 2.0f, 14.0f, fillH, 2.5f);
        } else {
            g.setColour(col);
            g.fillRoundedRectangle(x + 2.0f, mBg.getBottom() - 2.0f - fillH, 14.0f, fillH, 2.5f);
        }
        g.setColour(kTextSecondary);
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText(tag, static_cast<int>(x - 4), static_cast<int>(mBg.getBottom() + 3.0f), 26, 12, juce::Justification::centred);
    };

    drawSlimMeter(meterX0,         currentGrDisplay / 12.0f,             kAmberDeep, "GR");
    drawSlimMeter(meterX0 + 24.0f, (currentLufsDisplay + 48.0f) / 48.0f, kCyanNeon,  "LUFS");
    float outNorm = juce::jlimit(0.0f, 1.0f, currentMeterLevel * 2.5f);
    drawSlimMeter(meterX0 + 48.0f, outNorm, outNorm > 0.9f ? juce::Colour(0xffef4444) : kEmeraldNeon, "OUT");

    const float barY = specCard.getBottom() + 10.0f;
    const float barH = 38.0f;
    juce::Rectangle<float> libBar(margin, barY, leftColW, barH);
    drawObsidianCard(g, libBar, 8.0f);

    g.setColour(kTextSecondary);
    g.setFont(juce::FontOptions(11.0f));
    juce::String folderText = "SAMPLES: " + juce::String(audioProcessor.sampleFolders.size()) + " folder(s)";
    if (audioProcessor.sampleFolders.size() > 0)
        folderText += "  [" + juce::File(audioProcessor.sampleFolders[audioProcessor.sampleFolders.size() - 1]).getFileName() + "]";
    g.drawText(folderText, static_cast<int>(libBar.getX() + 118), static_cast<int>(barY + 7), 310, 24, juce::Justification::centredLeft, true);

    g.setColour(kCyanNeon);
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    juce::String lufsStr = (currentLufsDisplay <= -58.0f) ? "-INF LUFS" : (juce::String(currentLufsDisplay, 1) + " LUFS");
    g.drawText(lufsStr, static_cast<int>(libBar.getRight() - 265), static_cast<int>(barY + 7), 85, 24, juce::Justification::centredRight);

    const float botY = libBar.getBottom() + 10.0f;
    const float botH = bounds.getBottom() - botY - margin;
    const float stemsW = 292.0f;
    const float dspW = leftColW - stemsW - 10.0f;

    juce::Rectangle<float> stemsCard(margin, botY, stemsW, botH);
    drawObsidianCard(g, stemsCard, 10.0f);

    g.setColour(kCyanNeon);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("AI STEMS & MIDI", static_cast<int>(stemsCard.getX() + 14), static_cast<int>(stemsCard.getY() + 8), 180, 18, juce::Justification::centredLeft);
    g.setColour(kGlassBorder);
    g.drawLine(stemsCard.getX() + 12.0f, stemsCard.getY() + 30.0f, stemsCard.getRight() - 12.0f, stemsCard.getY() + 30.0f, 1.0f);

    if (audioProcessor.selectedStems.empty()) {
        g.setColour(kTextMuted);
        g.setFont(juce::FontOptions(11.0f));
        g.drawFittedText("No active stems.\nClick GENERATE or ARRANGE in Copilot.",
                         static_cast<int>(stemsCard.getX() + 16), static_cast<int>(stemsCard.getY() + 65),
                         static_cast<int>(stemsCard.getWidth() - 32), 60, juce::Justification::centred, 3);
    } else {
        int yRow = static_cast<int>(stemsCard.getY() + 36);
        for (size_t idx = 0; idx < audioProcessor.selectedStems.size() && idx < 4; ++idx) {
            const auto& stem = audioProcessor.selectedStems[idx];
            juce::Rectangle<float> rowRect(stemsCard.getX() + 10.0f, static_cast<float>(yRow), stemsCard.getWidth() - 20.0f, 40.0f);
            g.setColour(juce::Colour(0xff0c121e));
            g.fillRoundedRectangle(rowRect, 6.0f);
            g.setColour(juce::Colour(0xff1b263b));
            g.drawRoundedRectangle(rowRect, 6.0f, 1.0f);

            g.setColour(kAmberNeon);
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.drawText(stem.role.toUpperCase() + " [" + juce::String(static_cast<int>(stem.similarity * 100)) + "%]",
                       static_cast<int>(rowRect.getX() + 8), yRow + 4, 140, 13, juce::Justification::centredLeft);

            g.setColour(kTextPrimary);
            g.setFont(juce::FontOptions(10.5f));
            g.drawText(stem.filename, static_cast<int>(rowRect.getX() + 8), yRow + 18, 140, 16, juce::Justification::centredLeft, true);
            yRow += 45;
        }
    }

    juce::Rectangle<float> dspCard(stemsCard.getRight() + 10.0f, botY, dspW, botH);
    drawObsidianCard(g, dspCard, 10.0f);

    g.setColour(kAmberNeon);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("NEURAL DSP & AUTO-MASTERING CONSOLE", static_cast<int>(dspCard.getX() + 14), static_cast<int>(dspCard.getY() + 8), 280, 18, juce::Justification::centredLeft);
    g.setColour(kGlassBorder);
    g.drawLine(dspCard.getX() + 12.0f, dspCard.getY() + 30.0f, dspCard.getRight() - 12.0f, dspCard.getY() + 30.0f, 1.0f);

    juce::Rectangle<float> copilotCard(rightColX, topY, rightColW, totalH);
    drawObsidianCard(g, copilotCard, 10.0f, kCyanSoft);

    g.setColour(kCyanNeon);
    g.fillEllipse(copilotCard.getX() + 16.0f, copilotCard.getY() + 16.0f, 8.0f, 8.0f);
    g.setColour(kTextPrimary);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("AI COPILOT", static_cast<int>(copilotCard.getX() + 30), static_cast<int>(copilotCard.getY() + 9), 150, 22, juce::Justification::centredLeft);

    g.setColour(kTextMuted);
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText("OLLAMA LOCAL + ABLETON MCP", static_cast<int>(copilotCard.getRight() - 185), static_cast<int>(copilotCard.getY() + 10), 170, 20, juce::Justification::centredRight);

    g.setColour(kGlassBorder);
    g.drawLine(copilotCard.getX() + 12.0f, copilotCard.getY() + 38.0f, copilotCard.getRight() - 12.0f, copilotCard.getY() + 38.0f, 1.0f);

    if (splashProgress < 1.0f) {
        float alpha = (splashProgress > 0.85f) ? (1.0f - (splashProgress - 0.85f) / 0.15f) : 1.0f;
        g.setColour(kBgDeep.withAlpha(alpha));
        g.fillAll();
        g.setColour(kCyanNeon.withAlpha(alpha * 0.7f));
        g.drawRoundedRectangle(bounds.reduced(16.0f), 10.0f, 1.5f);

        g.setFont(juce::FontOptions(38.0f, juce::Font::bold));
        g.setColour(juce::Colours::white.withAlpha(alpha));
        g.drawText("NEURAL-LAB", 0, 240, getWidth(), 46, juce::Justification::centred);

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(kAmberNeon.withAlpha(alpha));
        g.drawText("OBSIDIAN GLASS STUDIO  v2.0", 0, 290, getWidth(), 22, juce::Justification::centred);

        float barW = 440.0f, barX = (getWidth() - barW) * 0.5f;
        g.setColour(juce::Colour(0xff1e293b).withAlpha(alpha));
        g.fillRoundedRectangle(barX, 342.0f, barW, 6.0f, 3.0f);
        g.setColour(kCyanNeon.withAlpha(alpha));
        g.fillRoundedRectangle(barX, 342.0f, barW * splashProgress, 6.0f, 3.0f);

        juce::String stepTxt = splashProgress < 0.35f
            ? "INITIALIZING 64-BIT NEURAL DSP ENGINE..."
            : (splashProgress < 0.70f ? "MOUNTING LAION-CLAP & REFERENCE EQ CORTEX..."
                                      : "CONNECTING LOCAL OLLAMA COPILOT & ABLETON MCP...");
        g.setFont(juce::FontOptions(11.0f));
        g.setColour(kTextSecondary.withAlpha(alpha));
        g.drawText(stepTxt, 0, 358, getWidth(), 20, juce::Justification::centred);
    }
}

void NeuralLabAudioProcessorEditor::resized() {
    const int w = getWidth();
    const int h = getHeight();
    const int margin = 16;
    const int topY = 64;
    const int totalH = h - topY - margin;
    const int rightColW = 356;
    const int leftColW = w - rightColW - (margin * 3);
    const int rightColX = margin * 2 + leftColW;

    mcpToggleBtn.setBounds(w - 418, 13, 86, 26);

    const int specH = 316;
    const int barY = topY + specH + 10;
    addFolderBtn.setBounds(margin + 10, barY + 6, 100, 26);
    playRefBtn.setBounds(margin + leftColW - 172, barY + 6, 68, 26);
    loadRefBtn.setBounds(margin + leftColW - 98, barY + 6, 88, 26);

    const int botY = barY + 38 + 10;
    const int botH = h - botY - margin;
    const int stemsW = 292;
    const int dspX = margin + stemsW + 10;
    const int dspW = leftColW - stemsW - 10;

    for (int i = 0; i < 4; ++i) {
        int yBtn = botY + 43 + i * 45;
        auditionButtons[i].setBounds(margin + stemsW - 122, yBtn, 26, 24);
        lockButtons[i].setBounds    (margin + stemsW - 93,  yBtn, 38, 24);
        swapButtons[i].setBounds    (margin + stemsW - 52,  yBtn, 40, 24);
    }

    juce::Rectangle<int> knobArea(dspX + 12, botY + 34, dspW - 24, botH - 40);
    const int cols = 3;
    const int rows = 2;
    const int cellW = knobArea.getWidth() / cols;
    const int cellH = knobArea.getHeight() / rows;

    auto placeKnob = [&](juce::Slider& s, juce::Label& l, int col, int row) {
        juce::Rectangle<int> cell(knobArea.getX() + col * cellW, knobArea.getY() + row * cellH, cellW, cellH);
        l.setBounds(cell.removeFromTop(16));
        s.setBounds(cell.reduced(6, 0));
    };

    placeKnob(driveSlider,   driveLabel,   0, 0);
    placeKnob(lowCutSlider,  lowCutLabel,  1, 0);
    placeKnob(punchSlider,   punchLabel,   2, 0);
    placeKnob(widthSlider,   widthLabel,   0, 1);
    placeKnob(eqMatchSlider, eqMatchLabel, 1, 1);
    placeKnob(ceilingSlider, ceilingLabel, 2, 1);

    juce::Rectangle<int> copilotInner(rightColX + 12, topY + 46, rightColW - 24, totalH - 58);

    auto promptRow = copilotInner.removeFromBottom(34);
    askChatBtn.setBounds(promptRow.removeFromRight(64));
    promptRow.removeFromRight(6);
    promptInput.setBounds(promptRow);

    copilotInner.removeFromBottom(8);

    auto quickRow = copilotInner.removeFromBottom(28);
    int pillW = (quickRow.getWidth() - 12) / 3;
    generateTrackBtn.setBounds(quickRow.removeFromLeft(pillW));
    quickRow.removeFromLeft(6);
    arrangeFullBtn.setBounds(quickRow.removeFromLeft(pillW));
    quickRow.removeFromLeft(6);
    autoMasterBtn.setBounds(quickRow);

    copilotInner.removeFromBottom(8);

    chatDisplayBox.setBounds(copilotInner);
}
