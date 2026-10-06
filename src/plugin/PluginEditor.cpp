#include "PluginEditor.h"
#include <chrono>

BackReverseAudioProcessorEditor::BackReverseAudioProcessorEditor(BackReverseAudioProcessor& proc)
:AudioProcessorEditor(&proc),p(proc){
    setResizable(true,true); setResizeLimits(760,520,1500,1000); setSize(980,680);
    title.setText("BACKREVERSE",juce::dontSendNotification); title.setFont(juce::Font(28.0f,juce::Font::bold)); addAndMakeVisible(title);
    status.setText("Near-real-time reverse buffer",juce::dontSendNotification); addAndMakeVisible(status);
    addAndMakeVisible(latencyLabel);

    auto addCombo=[this](juce::ComboBox& b,std::initializer_list<const char*> xs){int i=1;for(auto* x:xs)b.addItem(x,i++);addAndMakeVisible(b);};
    addCombo(reverseMode,{"Sequential","Whole Source","Reordered","Free Scrub","Hybrid"});
    addCombo(orderMode,{"Sequential","Reverse Order","Random","Shuffle","Ping Pong","Odds/Evens","Evens/Odds","Rotate Left","Rotate Right","User"});
    addCombo(timeMode,{"Rate","Time Stretch"}); addCombo(polarity,{"Normal","Invert L","Invert R","Invert Both"});

    configureSlider(chunk,"Chunk length in seconds. Live reverse latency follows this value.");
    configureSlider(ratio,"Playback-rate or time-stretch ratio. 0.25=quarter, 0.5=half, 2=double, 3=triple.");
    configureSlider(pan,"Stereo pan"); configureSlider(phase,"Frequency-dependent all-pass phase rotation");
    configureSlider(dry,"Dry level"); configureSlider(wet,"Processed level");
    playhead.setSliderStyle(juce::Slider::LinearHorizontal); playhead.setRange(0,1,0.0001); playhead.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0); playhead.setTooltip("Loaded-file playhead / scratch seek"); addAndMakeVisible(playhead);
    playhead.onDragEnd=[this]{p.seekFile(playhead.getValue());};

    for(auto* b:{&swap,&stutter,&delay,&echo}) addAndMakeVisible(*b);
    for(int i=0;i<16;++i){
        gates[(std::size_t)i].setButtonText(juce::String(i+1));
        gates[(std::size_t)i].setClickingTogglesState(true); gates[(std::size_t)i].setToggleState(true,juce::dontSendNotification);
        gates[(std::size_t)i].setTooltip("Gate step: click on/off");
        gates[(std::size_t)i].onClick=[this,i]{p.setGateEnabled(i,gates[(std::size_t)i].getToggleState());};
        addAndMakeVisible(gates[(std::size_t)i]);
        gateDir[(std::size_t)i].setButtonText("F"); gateDir[(std::size_t)i].setTooltip("Force this gate forward inside a reversed chunk");
        gateDir[(std::size_t)i].onClick=[this,i]{p.setGateForward(i,gateDir[(std::size_t)i].getToggleState());};
        addAndMakeVisible(gateDir[(std::size_t)i]);
    }

    addAndMakeVisible(load);addAndMakeVisible(play);addAndMakeVisible(random);
    load.setTooltip("Load WAV, AIFF, FLAC, MP3 or OGG supported by the platform");
    load.onClick=[this]{
        auto chooser=std::make_shared<juce::FileChooser>("Load audio",juce::File{}, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this,chooser](const juce::FileChooser& fc){
            auto f=fc.getResult(); if(f.existsAsFile()) p.loadAudioFile(f);
        });
    };
    play.onClick=[this]{p.setFilePlaying(!p.isFilePlaying());play.setButtonText(p.isFilePlaying()?"Pause":"Play");};
    random.onClick=[this]{auto s=(std::uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();p.randomize(s);};

    auto& s=p.state;
    sas.emplace_back(std::make_unique<SA>(s,"chunkSeconds",chunk)); sas.emplace_back(std::make_unique<SA>(s,"ratio",ratio));
    sas.emplace_back(std::make_unique<SA>(s,"pan",pan)); sas.emplace_back(std::make_unique<SA>(s,"phase",phase)); sas.emplace_back(std::make_unique<SA>(s,"dry",dry)); sas.emplace_back(std::make_unique<SA>(s,"wet",wet));
    bas.emplace_back(std::make_unique<BA>(s,"swapStereo",swap)); bas.emplace_back(std::make_unique<BA>(s,"stutterOn",stutter));bas.emplace_back(std::make_unique<BA>(s,"delayOn",delay));bas.emplace_back(std::make_unique<BA>(s,"echoOn",echo));
    cas.emplace_back(std::make_unique<CA>(s,"reverseMode",reverseMode));cas.emplace_back(std::make_unique<CA>(s,"orderMode",orderMode));cas.emplace_back(std::make_unique<CA>(s,"temporalMode",timeMode));cas.emplace_back(std::make_unique<CA>(s,"polarity",polarity));
    startTimerHz(15);
}

void BackReverseAudioProcessorEditor::configureSlider(juce::Slider& s,const juce::String& tip){s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,80,20);s.setTooltip(tip);addAndMakeVisible(s);}
void BackReverseAudioProcessorEditor::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(0xff0b0e13));g.setColour(juce::Colour(0xff141b24));g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12),12);
    g.setColour(juce::Colour(0xff3ed6c6));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(12),12,1.5f);
    g.setColour(juce::Colour(0xff8b95a5));g.setFont(12);g.drawText("Clickable gates  |  F = force gate forward  |  drag playhead to seek/scratch source",20,getHeight()-35,getWidth()-40,20,juce::Justification::centred);
}
void BackReverseAudioProcessorEditor::resized(){
    auto r=getLocalBounds().reduced(22);auto top=r.removeFromTop(54);title.setBounds(top.removeFromLeft(230));status.setBounds(top.removeFromLeft(270));latencyLabel.setBounds(top.removeFromLeft(220));
    auto actions=r.removeFromTop(36);load.setBounds(actions.removeFromLeft(110));play.setBounds(actions.removeFromLeft(80));random.setBounds(actions.removeFromLeft(110));playhead.setBounds(actions.reduced(8,0));
    auto combos=r.removeFromTop(42);reverseMode.setBounds(combos.removeFromLeft(170).reduced(4));orderMode.setBounds(combos.removeFromLeft(170).reduced(4));timeMode.setBounds(combos.removeFromLeft(150).reduced(4));polarity.setBounds(combos.removeFromLeft(150).reduced(4));
    auto knobs=r.removeFromTop(155); const int knobW=knobs.getWidth()/6;
    for(auto* s:{&chunk,&ratio,&pan,&phase,&dry,&wet}) s->setBounds(knobs.removeFromLeft(knobW).reduced(5));
    auto toggles=r.removeFromTop(34);swap.setBounds(toggles.removeFromLeft(110));stutter.setBounds(toggles.removeFromLeft(100));delay.setBounds(toggles.removeFromLeft(90));echo.setBounds(toggles.removeFromLeft(90));
    r.removeFromTop(12);auto gatesArea=r.removeFromTop(130);int w=gatesArea.getWidth()/16;
    for(int i=0;i<16;++i){auto cell=gatesArea.removeFromLeft(w).reduced(2);gates[(std::size_t)i].setBounds(cell.removeFromTop(70));gateDir[(std::size_t)i].setBounds(cell.removeFromTop(28));}
}
void BackReverseAudioProcessorEditor::timerCallback(){
    playhead.setValue(p.filePlayheadNormalized(),juce::dontSendNotification);
    latencyLabel.setText("Buffer: "+juce::String(*p.state.getRawParameterValue("chunkSeconds"),3)+" s",juce::dontSendNotification);
    for(int i=0;i<16;++i) gates[(std::size_t)i].setToggleState(p.gateEnabled(i),juce::dontSendNotification);
}
