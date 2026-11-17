#include "PluginProcessor.h"
#include "PluginEditor.h"

NeuralLabAudioProcessor::NeuralLabAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {
    oscSender.connect("127.0.0.1", 9001);
    if (oscReceiver.connect(9002)) oscReceiver.addListener(this);
    refFormatManager.registerBasicFormats();
    loadUserConfig();
    launchAIBackendIfNeeded();
    oscSender.send(juce::OSCMessage("/neural_lab/ping"));
}

NeuralLabAudioProcessor::~NeuralLabAudioProcessor() { oscReceiver.removeListener(this); }

void NeuralLabAudioProcessor::launchAIBackendIfNeeded() {
    juce::File projectRoot("C:/Users/Utente/Desktop/Neural Lab/NEURAL-LAB");
    if (!projectRoot.exists()) {
        auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        while (exeDir.exists() && !exeDir.getChildFile("ai_engine").exists() && exeDir.getParentDirectory() != exeDir)
            exeDir = exeDir.getParentDirectory();
        if (exeDir.getChildFile("ai_engine").exists()) projectRoot = exeDir;
    }
    auto pythonExe = projectRoot.getChildFile(".venv/Scripts/pythonw.exe");
    if (!pythonExe.existsAsFile()) pythonExe = projectRoot.getChildFile(".venv/Scripts/python.exe");
    auto serverScript = projectRoot.getChildFile("ai_engine/server.py");
    if (pythonExe.existsAsFile() && serverScript.existsAsFile()) {
        juce::String args = "\"" + serverScript.getFullPathName() + "\"";
        if (pythonExe.startAsProcess(args)) {
            aiStatusMessage = "NEURAL-LAB: STARTING...";
            chatHistory.push_back({ "Avvio motore NEURAL-LAB...", false }); chatDirty = true;
        }
    } else {
        aiStatusMessage = "AI ENGINE: READY (PORT 9001)";
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout NeuralLabAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("lowcut", 1), "Sub Clean (Hz)", juce::NormalisableRange<float>(20.0f, 250.0f, 1.0f, 0.5f), 30.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("punch", 1), "Neural Punch", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("width", 1), "Stereo Width", juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.05f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("drive", 1), "Input Drive (dB)", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("ceiling", 1), "Master Ceiling (dB)", juce::NormalisableRange<float>(-6.0f, 0.0f, 0.1f), -0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("eqmatch", 1), "EQ Match (%)", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));
    return { params.begin(), params.end() };
}

void NeuralLabAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());
    neuralDSP.prepare(spec);
    const juce::ScopedLock sl(refAudioLock);
    refTransportSource.prepareToPlay(samplesPerBlock, sampleRate);
}

void NeuralLabAudioProcessor::releaseResources() { const juce::ScopedLock sl(refAudioLock); refTransportSource.releaseResources(); }

void NeuralLabAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);
    neuralDSP.updateParameters(
        apvts.getRawParameterValue("lowcut")->load(),
        apvts.getRawParameterValue("punch")->load(),
        apvts.getRawParameterValue("width")->load(),
        apvts.getRawParameterValue("ceiling")->load(),
        apvts.getRawParameterValue("drive")->load(), apvts.getRawParameterValue("eqmatch")->load());
    neuralDSP.process(buffer);

    {
        const juce::ScopedTryLock stl(refAudioLock);
        if (stl.isLocked() && refTransportSource.isPlaying()) {
            buffer.clear();
            juce::AudioSourceChannelInfo info(&buffer, 0, buffer.getNumSamples());
            refTransportSource.getNextAudioBlock(info);
        }
    }
}

void NeuralLabAudioProcessor::addSampleFolder(const juce::File& folder) {
    if (!folder.isDirectory()) return;
    auto path = folder.getFullPathName();
    if (!sampleFolders.contains(path)) { sampleFolders.add(path); saveUserConfig(); }
    aiStatusMessage = "CLAP: INDEXING (" + folder.getFileName() + ")...";
    juce::OSCMessage msg("/neural_lab/add_folder");
    msg.addString(path);
    oscSender.send(msg);
}

void NeuralLabAudioProcessor::setReferenceTrack(const juce::File& refFile) {
    if (!refFile.existsAsFile()) return;
    referenceTrackPath = refFile.getFullPathName();
    referenceTrackName = refFile.getFileName();
    {
        const juce::ScopedLock sl(refAudioLock);
        refTransportSource.stop();
        refTransportSource.setSource(nullptr);
        refReaderSource.reset();
        if (auto* reader = refFormatManager.createReaderFor(refFile)) {
            refReaderSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
            refReaderSource->setLooping(true);
            refTransportSource.setSource(refReaderSource.get(), 0, nullptr, reader->sampleRate);
        }
    }
    saveUserConfig();
    aiStatusMessage = "MATCHERING: EXTRACTING EQ CURVE...";
    juce::OSCMessage msg("/neural_lab/set_reference");
    msg.addString(referenceTrackPath);
    oscSender.send(msg);
}

void NeuralLabAudioProcessor::toggleReferencePlayback() {
    const juce::ScopedLock sl(refAudioLock);
    if (refReaderSource == nullptr && referenceTrackPath.isNotEmpty()) {
        juce::File f(referenceTrackPath);
        if (f.existsAsFile()) {
            if (auto* reader = refFormatManager.createReaderFor(f)) {
                refReaderSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
                refReaderSource->setLooping(true);
                refTransportSource.setSource(refReaderSource.get(), 0, nullptr, reader->sampleRate);
            }
        }
    }
    if (refReaderSource == nullptr) return;
    if (refTransportSource.isPlaying()) {
        refTransportSource.stop();
    } else {
        refTransportSource.start();
    }
}

void NeuralLabAudioProcessor::sendPromptToAIEngine(const juce::String& promptText, const juce::String& mode) {
    if (mode == "generate_track" || mode == "arrange_full") { selectedStems.erase(std::remove_if(selectedStems.begin(), selectedStems.end(), [](const SelectedStemInfo& s) { return !s.isLocked; }), selectedStems.end()); auditioningStemIndex = -1; }
    aiStatusMessage = "AI: " + mode.toUpperCase() + "...";
    juce::OSCMessage msg("/neural_lab/command");
    msg.addString(mode);
    msg.addString(promptText);
    msg.addString(referenceTrackPath);
    oscSender.send(msg);
}

void NeuralLabAudioProcessor::swapStemAtIndex(int stemIndex) {
    if (stemIndex < 0 || stemIndex >= static_cast<int>(selectedStems.size()) || selectedStems[stemIndex].isLocked) return;
    aiStatusMessage = "SWAPPING " + selectedStems[stemIndex].role.toUpperCase() + "...";
    juce::OSCMessage msg("/neural_lab/swap_stem");
    msg.addInt32(stemIndex);
    msg.addString(selectedStems[stemIndex].role);
    oscSender.send(msg);
}



void NeuralLabAudioProcessor::auditionStemAtIndex(int stemIndex) {
    if (stemIndex < 0 || stemIndex >= static_cast<int>(selectedStems.size())) return;
    bool turnOn = (auditioningStemIndex != stemIndex);
    auditioningStemIndex = turnOn ? stemIndex : -1;
    juce::OSCMessage msg("/neural_lab/audition_stem");
    msg.addInt32(selectedStems[stemIndex].trackIndex);
    msg.addInt32(turnOn ? 1 : 0);
    oscSender.send(msg);
}

void NeuralLabAudioProcessor::toggleLockStemAtIndex(int stemIndex) {
    if (stemIndex < 0 || stemIndex >= static_cast<int>(selectedStems.size())) return;
    selectedStems[stemIndex].isLocked = !selectedStems[stemIndex].isLocked;
    juce::OSCMessage msg("/neural_lab/lock_stem");
    msg.addInt32(stemIndex);
    msg.addInt32(selectedStems[stemIndex].isLocked ? 1 : 0);
    oscSender.send(msg);
}

void NeuralLabAudioProcessor::toggleMcpAgent() {
    mcpAgentEnabled = !mcpAgentEnabled;
    juce::OSCMessage msg("/neural_lab/set_mcp_enabled");
    msg.addInt32(mcpAgentEnabled ? 1 : 0);
    oscSender.send(msg);
}
void NeuralLabAudioProcessor::sendChatQuestion(const juce::String& question) { if (question.trim().isNotEmpty()) { chatHistory.push_back({ "YOU: " + question.trim(), false }); if (chatHistory.size() > 40) chatHistory.erase(chatHistory.begin()); chatDirty = true; }
    if (question.trim().isEmpty()) return;
    chatHistory.push_back({ "TU: " + question.trim(), false });
    if (chatHistory.size() > 40) chatHistory.erase(chatHistory.begin()); chatDirty = true;
    juce::OSCMessage msg("/neural_lab/ask");
    msg.addString(question.trim());
    oscSender.send(msg);
}
void NeuralLabAudioProcessor::oscMessageReceived(const juce::OSCMessage& message) {
    auto addr = message.getAddressPattern().toString();
    if (addr == "/neural_lab/set_dsp" && message.size() >= 5) {
        if (auto* p = apvts.getParameter("lowcut"))  p->setValueNotifyingHost(p->convertTo0to1(message[0].getFloat32()));
        if (auto* p = apvts.getParameter("punch"))   p->setValueNotifyingHost(p->convertTo0to1(message[1].getFloat32()));
        if (auto* p = apvts.getParameter("width"))   p->setValueNotifyingHost(p->convertTo0to1(message[2].getFloat32()));
        if (auto* p = apvts.getParameter("drive"))   p->setValueNotifyingHost(p->convertTo0to1(message[3].getFloat32()));
        if (auto* p = apvts.getParameter("ceiling")) p->setValueNotifyingHost(p->convertTo0to1(message[4].getFloat32()));
    } else if (addr == "/neural_lab/stem_selected" && message.size() >= 4) {
        int idx = message[0].getInt32();
        SelectedStemInfo info;
        info.trackIndex = idx;
        info.role = message[1].getString();
        info.filename = message[2].getString();
        info.similarity = message[3].getFloat32();
        if (idx >= 0 && idx < static_cast<int>(selectedStems.size())) { info.isLocked = selectedStems[idx].isLocked; selectedStems[idx] = info; }
        else selectedStems.push_back(info);
    } else if (addr == "/neural_lab/mcp_state" && message.size() >= 1) { mcpAgentEnabled = (message[0].getInt32() != 0); } else if (addr == "/neural_lab/ref_curve" && message.size() >= 64) {
        for (int i = 0; i < 64; ++i) referenceEqCurve[i] = message[i].getFloat32(); neuralDSP.setReferenceCurve(referenceEqCurve);
        hasReferenceCurve = true;
    } else if (addr == "/neural_lab/chat" && message.size() >= 1) {
        juce::String msgText = message[0].getString();
        bool isErr = (message.size() >= 2 && message[1].isInt32()) ? (message[1].getInt32() != 0) : false;
        chatHistory.push_back({ msgText, isErr });
        if (chatHistory.size() > 40) chatHistory.erase(chatHistory.begin()); chatDirty = true;
    } else if (addr == "/neural_lab/status" && message.size() >= 1) {
        aiStatusMessage = message[0].getString();
    }
}

void NeuralLabAudioProcessor::saveUserConfig() {
    auto configDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NeuralLabAudio");
    configDir.createDirectory();
    juce::XmlElement root("NeuralLabConfig");
    root.setAttribute("referenceTrack", referenceTrackPath);
    for (const auto& f : sampleFolders) root.createNewChildElement("Folder")->setAttribute("path", f);
    root.writeTo(configDir.getChildFile("config.xml"));
}

void NeuralLabAudioProcessor::loadUserConfig() {
    auto cfgFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NeuralLabAudio/config.xml");
    if (auto xml = juce::XmlDocument::parse(cfgFile)) {
        referenceTrackPath = xml->getStringAttribute("referenceTrack");
        if (referenceTrackPath.isNotEmpty()) { juce::File rf(referenceTrackPath); referenceTrackName = rf.getFileName(); if (rf.existsAsFile()) { if (auto* r = refFormatManager.createReaderFor(rf)) { refReaderSource = std::make_unique<juce::AudioFormatReaderSource>(r, true); refReaderSource->setLooping(true); refTransportSource.setSource(refReaderSource.get(), 0, nullptr, r->sampleRate); } } }
        sampleFolders.clear();
        for (auto* child : xml->getChildIterator())
            if (child->hasTagName("Folder")) sampleFolders.add(child->getStringAttribute("path"));
    }
}

void NeuralLabAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    std::unique_ptr<juce::XmlElement> xml(apvts.copyState().createXml());
    copyXmlToBinary(*xml, destData);
}

void NeuralLabAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* NeuralLabAudioProcessor::createEditor() { return new NeuralLabAudioProcessorEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NeuralLabAudioProcessor(); }