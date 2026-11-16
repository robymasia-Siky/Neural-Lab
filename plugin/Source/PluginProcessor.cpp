#include "PluginProcessor.h"
#include "PluginEditor.h"

NeuralLabAudioProcessor::NeuralLabAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {
    oscSender.connect("127.0.0.1", 9001);
    if (oscReceiver.connect(9002)) {
        oscReceiver.addListener(this);
    }
    loadUserConfig();
    launchAIBackendIfNeeded();
}

NeuralLabAudioProcessor::~NeuralLabAudioProcessor() {
    oscReceiver.removeListener(this);
}

void NeuralLabAudioProcessor::launchAIBackendIfNeeded() {
    // Cerca la cartella NEURAL-LAB partendo dai percorsi noti o dalla posizione del binario
    juce::File projectRoot("C:/Users/Utente/Desktop/Neural Lab/NEURAL-LAB");
    if (!projectRoot.exists()) {
        auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        while (exeDir.exists() && !exeDir.getChildFile("ai_engine").exists() && exeDir.getParentDirectory() != exeDir) {
            exeDir = exeDir.getParentDirectory();
        }
        if (exeDir.getChildFile("ai_engine").exists()) {
            projectRoot = exeDir;
        }
    }

    auto pythonExe = projectRoot.getChildFile(".venv/Scripts/pythonw.exe");
    if (!pythonExe.existsAsFile()) {
        pythonExe = projectRoot.getChildFile(".venv/Scripts/python.exe");
    }
    auto serverScript = projectRoot.getChildFile("ai_engine/server.py");

    if (pythonExe.existsAsFile() && serverScript.existsAsFile()) {
        juce::ChildProcess proc;
        juce::StringArray cmd;
        cmd.add(pythonExe.getFullPathName());
        cmd.add(serverScript.getFullPathName());
        if (proc.start(cmd)) {
            aiStatusMessage = "AI ENGINE: ONLINE (GPU 8GB)";
        }
    } else {
        aiStatusMessage = "AI ENGINE: READY (PORT 9001)";
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout NeuralLabAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lowcut", 1), "Sub Clean (Hz)",
        juce::NormalisableRange<float>(20.0f, 250.0f, 1.0f, 0.5f), 30.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("punch", 1), "Neural Punch",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("width", 1), "Stereo Width",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.05f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("drive", 1), "Input Drive (dB)",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ceiling", 1), "Master Ceiling (dB)",
        juce::NormalisableRange<float>(-6.0f, 0.0f, 0.1f), -0.3f));

    return { params.begin(), params.end() };
}

void NeuralLabAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());
    neuralDSP.prepare(spec);
}

void NeuralLabAudioProcessor::releaseResources() {}

void NeuralLabAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    float lowCut  = apvts.getRawParameterValue("lowcut")->load();
    float punch   = apvts.getRawParameterValue("punch")->load();
    float width   = apvts.getRawParameterValue("width")->load();
    float drive   = apvts.getRawParameterValue("drive")->load();
    float ceiling = apvts.getRawParameterValue("ceiling")->load();

    neuralDSP.updateParameters(lowCut, punch, width, ceiling, drive);
    neuralDSP.process(buffer);
}

void NeuralLabAudioProcessor::addSampleFolder(const juce::File& folder) {
    if (!folder.isDirectory()) return;
    auto path = folder.getFullPathName();
    if (!sampleFolders.contains(path)) {
        sampleFolders.add(path);
        saveUserConfig();
    }
    aiStatusMessage = "CLAP: INDEXING FOLDER (" + folder.getFileName() + ")...";
    juce::OSCMessage msg("/neural_lab/add_folder");
    msg.addString(path);
    oscSender.send(msg);
}

void NeuralLabAudioProcessor::setReferenceTrack(const juce::File& refFile) {
    if (!refFile.existsAsFile()) return;
    referenceTrackPath = refFile.getFullPathName();
    referenceTrackName = refFile.getFileName();
    saveUserConfig();

    aiStatusMessage = "MATCHERING 2.0: ANALYZING REFERENCE...";
    juce::OSCMessage msg("/neural_lab/set_reference");
    msg.addString(referenceTrackPath);
    oscSender.send(msg);
}

void NeuralLabAudioProcessor::sendPromptToAIEngine(const juce::String& promptText, const juce::String& mode) {
    if (mode == "generate_track") {
        selectedStems.clear();
    }
    aiStatusMessage = "AI PROCESSING: " + mode.toUpperCase() + "...";
    juce::OSCMessage msg("/neural_lab/command");
    msg.addString(mode);
    msg.addString(promptText);
    msg.addString(referenceTrackPath);
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
    } else if (addr == "/neural_lab/stem_selected" && message.size() >= 3) {
        SelectedStemInfo info;
        info.role = message[0].getString();
        info.filename = message[1].getString();
        info.similarity = message[2].getFloat32();
        selectedStems.push_back(info);
    } else if (addr == "/neural_lab/status" && message.size() >= 1) {
        aiStatusMessage = message[0].getString();
    }
}

void NeuralLabAudioProcessor::saveUserConfig() {
    auto configDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NeuralLabAudio");
    configDir.createDirectory();
    auto cfgFile = configDir.getChildFile("config.xml");

    juce::XmlElement root("NeuralLabConfig");
    root.setAttribute("referenceTrack", referenceTrackPath);
    for (const auto& f : sampleFolders) {
        auto* child = root.createNewChildElement("Folder");
        child->setAttribute("path", f);
    }
    root.writeTo(cfgFile);
}

void NeuralLabAudioProcessor::loadUserConfig() {
    auto cfgFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("NeuralLabAudio/config.xml");
    if (auto xml = juce::XmlDocument::parse(cfgFile)) {
        referenceTrackPath = xml->getStringAttribute("referenceTrack");
        if (referenceTrackPath.isNotEmpty()) {
            referenceTrackName = juce::File(referenceTrackPath).getFileName();
        }
        sampleFolders.clear();
        for (auto* child : xml->getChildIterator()) {
            if (child->hasTagName("Folder")) {
                sampleFolders.add(child->getStringAttribute("path"));
            }
        }
    }
}

void NeuralLabAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void NeuralLabAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

juce::AudioProcessorEditor* NeuralLabAudioProcessor::createEditor() {
    return new NeuralLabAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new NeuralLabAudioProcessor();
}
