#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>
#include <essentia/essentiamath.h>

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
    //==============================================================================
    std::vector<essentia::Real> essentiaInput;
    std::vector<essentia::Real> essentiaPitch;
    std::vector<essentia::Real> essentiaPitchConfidence;
    std::vector<essentia::Real> essentiaFreq;
    essentia::standard::Algorithm* melodyDetection;
    essentia::standard::Algorithm* pitchfilter;
    essentia::standard::Algorithm* equalloudness;
    
private:
    //==============================================================================
    void sendOSC(String oscAddress, int value);
    void sendMIDI(int noteNumber);
    float computeRMS(std::vector<float> &buffer);
    
    static const int lengthToDetectMelody_sample = 8192;//8192サンプルごとにメロディー推定を行う
    
    //OSC
    OSCSender oscSender;
    const String ip = "127.0.0.1";
    static const int port = 8080;
    const String oscAddress_note = "/melodyDetection/note";
    
    //MIDI
    static const int midiChannel = 1;
    const String midiPortName = "MelodyDetection";
    ScopedPointer<MidiOutput> midiOut;
    MidiMessage midiMessage;
    int lastNote = -1;
    
    //オーディオインターフェース,ノイズゲート設定の記録、呼び出し用
    ScopedPointer<ApplicationProperties> appProperties;
    
    //小音量時にメロディー判定を行わないようにするための閾値(dB)
    Slider noiseGateThreshold;
    Label noiseGateLabel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

// (This function is called by the app startup code to create our main component)
Component* createMainContentComponent()     { return new MainContentComponent(); }
