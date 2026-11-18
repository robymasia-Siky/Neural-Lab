#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_osc/juce_osc.h>
#include <array>
#include "DSP/NeuralDSP.h"

struct PendingDspPreset {
    juce::String name;
    float lowcut = 30.0f;
    float punch = 0.35f;
    float width = 1.05f;
    float drive = 0.0f;
    float eqmatch = 50.0f;
    float ceiling = -0.3f;
};

struct SelectedStemInfo {
    int trackIndex = 0;
    juce::String role;
    juce::String filename;
    float similarity = 0.0f;
    bool isLocked = false;
    juce::String filePath;
};

struct SuggestedDspPreset {
    bool hasSuggestion = false;
    juce::String name;
    float lowcut  = 30.0f;
    float punch   = 0.35f;
    float width   = 1.05f;
    float drive   = 0.0f;
    float ceiling = -0.3f;
    float eqmatch = 50.0f;
};

class NeuralLabAudioProcessor : public juce::AudioProcessor,
                                private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback> {
public:
    NeuralLabAudioProcessor();
    ~NeuralLabAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NEURAL LAB"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void launchAIBackendIfNeeded();
    void sendPromptToAIEngine(const juce::String& promptText, const juce::String& mode);
    void swapStemAtIndex(int stemIndex);
    void auditionStemAtIndex(int stemIndex);
    void toggleLockStemAtIndex(int stemIndex);
    int auditioningStemIndex = -1;
    bool mcpAgentEnabled = true;
    void toggleMcpAgent();
    void sendOscPing() { oscSender.send(juce::OSCMessage("/neural_lab/ping")); }
    void addSampleFolder(const juce::File& folder);
    void setReferenceTrack(const juce::File& refFile);
    void toggleReferencePlayback();
    bool isReferencePlaying() const { return refTransportSource.isPlaying(); }

    juce::AudioProcessorValueTreeState apvts;
    NeuralDSP neuralDSP;

    struct ChatLine { juce::String text; bool isError; };
    std::vector<ChatLine> chatHistory;
    bool chatDirty = true;
    juce::String aiStatusMessage { "NEURAL-LAB: STARTING..." };
    void sendChatQuestion(const juce::String& question);
    SuggestedDspPreset suggestedDsp;
    void applySuggestedDspPreset();
    juce::StringArray sampleFolders;
    juce::String referenceTrackPath;
    juce::AudioFormatManager refFormatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> refReaderSource;
    juce::AudioTransportSource refTransportSource;
    juce::CriticalSection refAudioLock;
    juce::String referenceTrackName { "None (Click to load)" };
    std::vector<SelectedStemInfo> selectedStems;
    std::array<float, 64> referenceEqCurve {};
    bool hasReferenceCurve = false;
    PendingDspPreset pendingDspPreset;
    bool hasPendingDspPreset = false;
    void applyPendingDspPreset();
    void dismissPendingDspPreset() { hasPendingDspPreset = false; }

private:
    void oscMessageReceived(const juce::OSCMessage& message) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void saveUserConfig();
    void loadUserConfig();

    juce::OSCSender oscSender;
    juce::OSCReceiver oscReceiver;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeuralLabAudioProcessor)
};


