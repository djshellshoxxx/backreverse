#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

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

    juce::Label title,status,latencyLabel;
    juce::TextButton load{"Load Audio"},play{"Pause"},random{"Randomize"};
    juce::ComboBox reverseMode,orderMode,timeMode,polarity;
    juce::Slider chunk,ratio,pan,phase,dry,wet,playhead;
    juce::ToggleButton swap{"Swap L/R"},stutter{"Stutter"},delay{"Delay"},echo{"Echo"};
    std::array<juce::TextButton,16> gates;
    std::array<juce::ToggleButton,16> gateDir;

    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<SA>> sas;
    std::vector<std::unique_ptr<BA>> bas;
    std::vector<std::unique_ptr<CA>> cas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackReverseAudioProcessorEditor)
};
