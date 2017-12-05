#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>
#include <essentia/essentiamath.h>

class MainContentComponent :
public AudioAppComponent,
public MenuBarModel,
public Slider::Listener,
public ComboBox::Listener
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
    void comboBoxChanged (ComboBox* comboBox) override;
    StringArray getMenuBarNames() override;
    PopupMenu getMenuForIndex (int topLevelMenuIndex, const String& /*menuName*/) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;
    void showAudioSettings();
    //==============================================================================
    std::vector<essentia::Real> essentiaInput;
    std::vector<essentia::Real> essentiaPitch;
    std::vector<essentia::Real> essentiaPitchConfidence;
    std::vector<essentia::Real> essentiaFreq;
    essentia::standard::Algorithm* melodyEstimate;
    essentia::standard::Algorithm* pitchfilter;
    
private:
    //==============================================================================
    void sendOSC(String oscAddress, int value);
    void sendMIDI(int noteNumber);
    void estimateMelody();
    void computeHighpassCoefficient(const double cutoffFreq, const double sampleRate);
    
    static const int lengthToDetectMelody_sample = 8192;//8192サンプルごとにメロディー推定を行う
    struct bufferAndIndex{
        AudioSampleBuffer buffer;//Essentiaでメロディー推定するための直近8192サンプルを保持するバッファー
        int index = 0;
    };
    bufferAndIndex preApplyEssentia;
    
    //OSC
    OSCSender oscSender;
    const String ip = "127.0.0.1";
    static const int port = 8080;
    const String oscAddress_note = "/melodyEstimator/note";
    
    //MIDI
    static const int midiChannel = 1;
    const String midiPortName = "MelodyDetection";
    ScopedPointer<MidiOutput> midiOut;
    MidiMessage midiMessage;
    int lastNote = -1;
    
    //オーディオインターフェース,ノイズゲート設定の記録、呼び出し用
    ScopedPointer<ApplicationProperties> appProperties;
    
    //小音量時にメロディー判定を行わないようにするための閾値(dB)
    Slider sl_noiseGateThreshold;
    Label lbl_noiseGate;
    ComboBox cmb_hpf;
    Label lbl_hpf;
    
    using iir = dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, dsp::IIR::Coefficients<float>>;
    struct highpassAndEnable{
        dsp::ProcessorChain<iir, iir> processor;
        bool enabled = false;
    };
    highpassAndEnable highpass;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

// (This function is called by the app startup code to create our main component)
Component* createMainContentComponent()     { return new MainContentComponent(); }
