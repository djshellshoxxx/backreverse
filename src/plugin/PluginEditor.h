#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class GateGrid final : public juce::Component {
public:
    explicit GateGrid(BackReverseAudioProcessor& p):processor(p){}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
private:
    void applyAt(juce::Point<int>,const juce::ModifierKeys&);
    BackReverseAudioProcessor& processor;
};

class GateCurveEditor final : public juce::Component {
public:
    explicit GateCurveEditor(BackReverseAudioProcessor& p):processor(p){}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
private:
    void edit(const juce::MouseEvent&);
    BackReverseAudioProcessor& processor;
};

class WaveformView final : public juce::Component {
public:
    explicit WaveformView(BackReverseAudioProcessor& p):processor(p){}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    void seek(const juce::MouseEvent&,bool start);
    BackReverseAudioProcessor& processor;
    double lastNorm {0.0},lastMs {0.0};
};

class BackReverseAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit BackReverseAudioProcessorEditor(BackReverseAudioProcessor&);
    ~BackReverseAudioProcessorEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void configureSlider(juce::Slider&,const juce::String&);
    BackReverseAudioProcessor& p;
    GateGrid gateGrid;
    GateCurveEditor gateCurve;
    WaveformView waveform;

    juce::Label title,status,latencyLabel;
    juce::TextButton load{"Load Audio"},play{"Pause"},random{"Randomize"},help{"Help"},undo{"Undo"},redo{"Redo"};
    juce::TextButton quarter{"1/4x"},half{"1/2x"},normal{"1x"},dbl{"2x"},triple{"3x"};
    juce::TextEditor patternText;
    juce::ComboBox reverseMode,orderMode,timeMode,polarity,syncDivision,gateSteps,gateShape,gapMode,fxOrder,preset,chunkUnit,scratchMode,scratchRelease;
    juce::Slider chunk,ratio,pan,phase,dry,wet,gateWidth,gateGap;
    juce::Slider stutterWet,stutterDry,stutterDecay,stutterRepeats;
    juce::Slider delayWet,delayDry,delayFeedback,delayLowpass,delayHighpass;
    juce::Slider echoWet,echoDry,echoFeedback,echoDamping,echoSpread,echoDrift,echoWow;
    juce::Slider scratchInertia,scratchFriction,scratchMaxRate;
    juce::ToggleButton swap{"Swap L/R"},hostSync{"Host Sync"},stutter{"Stutter"},delay{"Delay"},echo{"Echo"},stutterAlternate{"Alt Stutter"},delayPingPong{"Ping Pong"},scratchReverseOnly{"Rev Only"};

    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<SA>> sas;
    std::vector<std::unique_ptr<BA>> bas;
    std::vector<std::unique_ptr<CA>> cas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackReverseAudioProcessorEditor)
};
