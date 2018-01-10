#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>
#include <essentia/essentiamath.h>

struct CustomLookAndFeel    : public LookAndFeel_V4
{
    CustomLookAndFeel()
    {
        setColour(Slider::trackColourId, Colour::Colour(0xFFFF5F07));
        setColour(Slider::backgroundColourId, Colour::Colour(0xFF121258));
        setColour(Slider::textBoxTextColourId, Colours::white);
        setColour(Slider::textBoxOutlineColourId, Colours::white);
        setColour(Label::textColourId, Colours::white);
    }
};

class MainContentComponent :
public AudioAppComponent,
public MenuBarModel,
public Slider::Listener,
public Button::Listener,
private Timer
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
    void buttonClicked (Button* button) override;
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
    void timerCallback() override;
    void sendOSC(String oscAddress, int value);
    void sendMIDI(int noteNumber);
    void estimateMelody();
    void computeHighpassCoefficient(const double cutoffFreq, const double sampleRate);
    
    static const int lengthToEstimateMelody_sample = 8192;//8192サンプルごとにメロディー推定を行う
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
    const String midiPortName = "MelodyEstimator";
    ScopedPointer<MidiOutput> midiOut;
    MidiMessage midiMessage;
    int lastNote = -1;
    
    std::atomic<float> RMSlevel_dB{-100.0};
    
    //オーディオインターフェース,ノイズゲート設定の記録、呼び出し用
    ScopedPointer<ApplicationProperties> appProperties;
    
    //小音量時にメロディー判定を行わないようにするための閾値(dB)
    Slider sl_noiseGateThreshold;
    Label lbl_noiseGate;
    //ハイパスフィルター
    Slider sl_hpf;
    Label lbl_hpf;
    ToggleButton tgl_hpf;
    //アプリ名,バージョン表示
    Label lbl_appName;
    Label lbl_version;
    
    ScopedPointer<dsp::Oversampling<float>> oversampling;
    static const int overSampleFactor = 2;//2^overSampleFactor
    using iir = dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, dsp::IIR::Coefficients<float>>;
    dsp::ProcessorChain<iir, iir> highpass;
    CustomLookAndFeel lookAndFeel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

// (This function is called by the app startup code to create our main component)
Component* createMainContentComponent()     { return new MainContentComponent(); }
