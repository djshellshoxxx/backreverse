#pragma once
#include <JuceHeader.h>
#include "backreverse/BackReverseEngine.h"

class BackReverseAudioProcessor final : public juce::AudioProcessor {
public:
    BackReverseAudioProcessor();
    ~BackReverseAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "BackReverse"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    juce::AudioProcessorValueTreeState state;

    bool loadAudioFile(const juce::File&);
    void unloadAudioFile();
    bool hasLoadedFile() const noexcept { return fileActive.load(); }
    void setFilePlaying(bool b) noexcept { filePlaying.store(b); }
    bool isFilePlaying() const noexcept { return filePlaying.load(); }
    void seekFile(double normalized);
    double filePlayheadNormalized() const noexcept;
    double fileLengthSeconds() const noexcept;
    std::vector<float> getWaveformPeaks() const { return waveformPeaks; }

    void setGateEnabled(int index,bool enabled);
    bool gateEnabled(int index) const;
    void setGateForward(int index,bool forward);
    bool gateForwardState(int index) const;
    int activeGateCount() const;
    void randomize(std::uint64_t seed);
    void syncEngineFromParameters();

private:
    static double beatsForDivision(int index);
    br::BackReverseEngine engine;
    br::ChunkParams params;
    br::EffectSettings fx;
    std::vector<bool> gateMask;
    std::vector<bool> gateForward;
    std::uint64_t seed {0xBACC0FFEEULL};
    std::atomic<double> lastBpm {120.0};

    juce::AudioFormatManager formatManager;
    std::vector<float> loadedInterleaved;
    std::vector<float> waveformPeaks;
    std::atomic<bool> fileActive {false};
    std::atomic<bool> filePlaying {false};
    std::atomic<std::size_t> fileFrame {0};
    std::size_t loadedFrames {0};
    int loadedChannels {0};
    double loadedSampleRate {0.0};

    std::vector<float> inScratch, outScratch;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackReverseAudioProcessor)
};
