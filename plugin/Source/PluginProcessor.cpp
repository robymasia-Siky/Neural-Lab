#include "PluginProcessor.h"
#include "PluginEditor.h"

NeuralLabAudioProcessor::NeuralLabAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {
    // Connette OSC Sender verso Python (127.0.0.1:9001) e Receiver dal Python (9002)
    oscSender.connect("127.0.0.1", 9001);
    if (oscReceiver.connect(9002)) {
        oscReceiver.addListener(this);
    }
}

NeuralLabAudioProcessor::~NeuralLabAudioProcessor() {
    oscReceiver.removeListener(this);
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

    float lowCut = apvts.getRawParameterValue("lowcut")->load();
    float punch = apvts.getRawParameterValue("punch")->load();
    float width = apvts.getRawParameterValue("width")->load();
    float drive = apvts.getRawParameterValue("drive")->load();
    float ceiling = apvts.getRawParameterValue("ceiling")->load();

    neuralDSP.updateParameters(lowCut, punch, width, ceiling, drive);
    neuralDSP.process(buffer);
}

void NeuralLabAudioProcessor::sendPromptToAIEngine(const juce::String& promptText, const juce::String& mode) {
    aiStatusMessage = "AI PROCESSING: " + mode.toUpperCase() + "...";
    juce::OSCMessage msg("/neural_lab/command");
    msg.addString(mode);
    msg.addString(promptText);
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
        aiStatusMessage = "AI APPLIED MIX & MASTER PROFILE";
    } else if (addr == "/neural_lab/status" && message.size() >= 1) {
        aiStatusMessage = message[0].getString();
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
