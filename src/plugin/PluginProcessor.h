#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include "../core/Engine.h"
#include <thread>

namespace lk { // lane / UI-state keys in the LANES tree
#define BR_LANE_KEYS(X) X(userPat) X(sizes) X(rates) X(laneLen) X(chunkFlags) X(panLen) X(pan) X(polLen) X(pol) X(gates) X(curve)
#define BR_UI_KEYS(X) X(loopA) X(loopB) X(filePath) X(randAmt) X(randEx) X(randLock) X(uiTab) X(zoom) X(laneSel)
#define BR_DECL(k) inline const juce::Identifier k { #k };
BR_LANE_KEYS (BR_DECL) BR_UI_KEYS (BR_DECL)
#undef BR_DECL
}

struct LoadedSource { juce::AudioBuffer<float> buf; br::SourceBuffer sb; juce::String name; };

class BackReverseProcessor : public juce::AudioProcessor, private juce::ValueTree::Listener, private juce::Timer
{
public:
    BackReverseProcessor();
    ~BackReverseProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "BackReverse"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }
    // Presets are offered in the editor, not as host programs, so a host program change can never wipe restored state.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- parameters
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>*, brp::kNum> raw {};
    juce::RangedAudioParameter* param (int i) const { return apvts.getParameter (brp::kDefs[i].id); }
    float get (int i) const { return raw[(size_t) i]->load(); }
    void set (int i, float plain);
    br::EngineParams snapshot() const { br::EngineParams P; brp::read (raw, P); return P; }

    // ---- lanes (pattern state; undoable)
    juce::ValueTree lanes { "LANES" };
    br::Patterns pats;                 // message-thread copy of the published patterns
    void applyPatterns (const br::Patterns&, bool undoable = true);
    void setUserPattern (const juce::String& text, bool undoable = true);
    juce::String userPattern() const { return lanes[lk::userPat].toString(); }
    static void parseUserPattern (const juce::String& text, br::Patterns& p);
    void setUi (const juce::Identifier& k, const juce::var& v) { lanes.setProperty (k, v, nullptr); }
    juce::var ui (const juce::Identifier& k, const juce::var& def = {}) const { return lanes.getProperty (k, def); }

    // ---- engine
    br::Engine engine;
    std::atomic<long long> blockCounter { 0 };
    double sr = 48000;

    // ---- file / capture
    bool loadFile (const juce::File&, juce::String* error = nullptr);
    std::shared_ptr<LoadedSource> source() const { return activeSrc; }
    juce::File currentFile;
    int sourceGeneration = 0;
    void startCapture();
    void stopCapture();
    bool capturing() const { return engine.ctl.capture.load() || captureStopAt >= 0; }

    // ---- record processed output / offline render (value feature)
    bool startRecording (const juce::File&);
    void stopRecording();
    bool isRecording() const { return writer.load() != nullptr; }
    juce::File recordingFile;
    bool startRender (const juce::File&);
    std::atomic<float> renderProgress { -1.0f };
    juce::File lastRender;

    // ---- presets / A-B / randomize / clipboard
    juce::StringArray presetNames() const;
    static int numFactoryPresets();
    void loadPreset (int index);
    bool saveUserPreset (const juce::String& name);
    static juce::File presetDir();
    int currentPreset = 0;
    void selectAB (int slot);
    void copyABToOther();
    int abSlot = 0;
    void randomize (bool reroll);
    juce::String copySection (const juce::String& section);
    bool pasteSection (const juce::String& text);
    void resetToDefaults();

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void timerCallback() override;
    void publishPatterns();
    void setSource (std::shared_ptr<LoadedSource>);
    std::shared_ptr<LoadedSource> makeSource (juce::AudioBuffer<float>&& buf, double rate, const juce::String& name);
    juce::ValueTree stateTree();
    void applyStateTree (const juce::ValueTree& root, bool includeFile);

    br::SeqBox<br::Patterns> patBox;
    br::Patterns audioPats; uint32_t patSeq = 0;
    std::shared_ptr<LoadedSource> activeSrc;
    std::vector<std::pair<std::shared_ptr<LoadedSource>, long long>> retired;
    std::atomic<juce::uint32> lastBlockMs { 0 };
    long long captureStopAt = -1;
    juce::AudioFormatManager formats;
    juce::TimeSliceThread writerThread { "BackReverse writer" };
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> writer { nullptr };
    std::thread renderThread; std::atomic<bool> renderCancel { false };
    juce::MemoryBlock ab[2]; bool abValid[2] { false, false };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BackReverseProcessor)
};
