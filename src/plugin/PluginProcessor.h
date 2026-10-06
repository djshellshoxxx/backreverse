#pragma once
#include <JuceHeader.h>
#include <array>
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
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState state;

    bool loadAudioFile(const juce::File&);
    void unloadAudioFile();
    bool hasLoadedFile() const noexcept { return fileActive.load(); }
    void setFilePlaying(bool b) noexcept { filePlaying.store(b); }
    bool isFilePlaying() const noexcept { return filePlaying.load(); }
    void seekFile(double normalized);
    void beginScratch(double normalized);
    void updateScratch(double normalized,double normalizedVelocityPerSecond);
    void endScratch();
    double filePlayheadNormalized() const noexcept;
    double fileLengthSeconds() const noexcept;
    std::vector<float> getWaveformPeaks() const { return waveformPeaks; }

    void setGateEnabled(int index,bool enabled);
    bool gateEnabled(int index) const;
    void setGateForward(int index,bool forward);
    bool gateForwardState(int index) const;
    void setGateEffect(int index,br::EffectType effect,bool enabled);
    bool gateEffectState(int index,br::EffectType effect) const;
    void setGateCurvePoint(int index,float value);
    float gateCurvePoint(int index) const;
    void refreshGateCurveCache();
    int activeGateCount() const;
    std::size_t preparedBufferFrames() const noexcept { return preparedBufferFramesLimit.load(); }
    void randomize(std::uint64_t seed);
    void loadFactoryPreset(int index);
    void setUserPatternText(const juce::String& text);
    juce::String userPatternText() const;
    void syncEngineFromParameters();

private:
    struct LoadedAudioBuffer {
        std::vector<float> samples;
        std::size_t frames {0};
        int channels {0};
        double sampleRate {0.0};
    };
    int acquireFileBuffer() noexcept;
    void releaseFileBuffer(int slot) noexcept;
    static double beatsForDivision(int index);
    br::BackReverseEngine engine;
    br::ChunkParams params;
    br::EffectSettings fx;
    std::array<std::atomic<bool>,64> gateMask {},gateForward {};
    std::array<std::atomic<bool>,64> gateStutter {},gateDelay {},gateEcho {};
    std::array<std::atomic<float>,8> gateCurve {};
    std::atomic<std::uint64_t> seed {0xBACC0FFEEULL};
    std::atomic<double> lastBpm {120.0};
    std::atomic<bool> scratchActive {false};
    std::atomic<double> scratchTargetNorm {0.0},scratchVelocityNormPerSec {0.0};
    std::atomic<bool> scratchReleaseRequested {false};
    double scratchAudioFrame {0.0},scratchAudioVelocity {0.0};
    std::atomic<double> scratchReturnFrame {0.0};

    juce::AudioFormatManager formatManager;
    std::array<LoadedAudioBuffer,2> loadedAudio;
    std::array<std::atomic<unsigned>,2> fileReaders {};
    std::atomic<int> activeFileSlot {-1};
    std::vector<float> waveformPeaks;
    std::atomic<bool> fileActive {false};
    std::atomic<bool> filePlaying {false};
    std::atomic<double> fileFrame {0.0};
    std::atomic<std::size_t> loadedFrames {0};
    std::atomic<int> loadedChannels {0};
    std::atomic<double> loadedSampleRate {0.0};

    std::vector<float> inScratch, outScratch;
    std::atomic<std::size_t> preparedBufferFramesLimit {48000u*5u};
    std::atomic<bool> resetEngineRequested {false};
    int lastReportedLatency {-1};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackReverseAudioProcessor)
};
