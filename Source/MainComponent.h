/*
  ==============================================================================

    MainComponent.h
    Created: 10 Oct 2017 1:25:14pm
    Author:  Akiyuki Okayasu

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>
#include <essentia/essentiamath.h>
#include <essentia/pool.h>

class MainContentComponent :
public AudioAppComponent,
public MenuBarModel,
public Slider::Listener
{
public:
    MainContentComponent();
    ~MainContentComponent();
    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    //==============================================================================
    void paint (Graphics& g) override;
    void resized() override;
    void sliderValueChanged (Slider* slider) override;
    StringArray getMenuBarNames() override;
    PopupMenu getMenuForIndex (int topLevelMenuIndex, const String& /*menuName*/) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;
    void showAudioSettings();
    int freqToMidi(float freq);
    //==============================================================================
    std::vector<essentia::Real> essentiaAudioBuffer;
    std::vector<essentia::Real> pitch;
    std::vector<essentia::Real> pitchConfidence;
    std::vector<essentia::Real> freq;
    essentia::standard::Algorithm* mel;
    essentia::standard::Algorithm* pitchfilter;
    essentia::standard::Algorithm* equalloudness;
    
private:
    //==============================================================================
    void sendOSC(String oscAddress, int value);
    void sendMIDI(int noteNumber);
    bool rmsThreshold(std::vector<float> &buf, float threshold);
    
    //8192サンプルごとにメロディー推定を行う
    static const int melFrameSize = 8192;
    
    //OSC
    OSCSender sender;
    String ip = "127.0.0.1";
    static const int portnumber = 8080;
    
    //MIDI
    int midiNote = -1;
    static const int midiChannel = 1;
    ScopedPointer<MidiOutput> midiOut;
    MidiMessage midiMessage;
    
    //オーディオインターフェースの設定の記録、呼び出し用
    ScopedPointer<ApplicationProperties> appProperties;
    
    //小音量時にメロディー判定を行わないようにするための閾値(dB)
    Slider noiseGateThreshold;
    Label noiseGateLabel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

// (This function is called by the app startup code to create our main component)
Component* createMainContentComponent()     { return new MainContentComponent(); }
