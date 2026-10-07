#pragma once
#include "UI.h"

class BackReverseEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit BackReverseEditor (BackReverseProcessor&);
    ~BackReverseEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;
    void mouseDown (const juce::MouseEvent&) override;
    static constexpr int W = 1280, H = 820;
    int numTabs() const;
    void selectTab (int);
    struct Content;
private:
    void timerCallback() override;
    BackReverseProcessor& proc;
    ui::Look look;
    std::unique_ptr<Content> content;
    juce::TooltipWindow tips { this, 500 };
};
