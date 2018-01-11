#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>
#include <essentia/essentiamath.h>

#define XMLKEYAUDIOSETTINGS "audioDeviceState"
#define XMLKEYNOISEGATE "noiseGateSettings"
#define XMLKEYHIGHPASS "highpassFilterSettings"
#define XMLKEYLOWPASS "lowpassFilterSettings"

struct CustomLookAndFeel    : public LookAndFeel_V4
{
    CustomLookAndFeel()
    {
        setColour(Slider::thumbColourId, Colour::Colour(0xFFAED1E6));//ロータリーエンコーダーのつまみ
        setColour(Slider::rotarySliderFillColourId, Colour::Colour(0xFFC6DBF0));//ロータリーエンコーダーの外周(有効範囲)
        setColour(Slider::rotarySliderOutlineColourId, Colour::Colour(0xFF2B2B2A));//ロータリーエンコーダーの外周(非有効範囲)
        setColour(Slider::trackColourId, Colour::Colour(0xFFAED1E6));//スライダーの有効範囲
        setColour(Slider::backgroundColourId, Colour::Colour(0xFF2B2B2A));//スライダーの背景
        setColour(Slider::textBoxTextColourId, Colour::Colour(0xFF2B2B2A));
        setColour(Slider::textBoxOutlineColourId, Colour::Colour(0xFF2B2B2A));
        setColour(Label::textColourId, Colour::Colour(0xFF2B2B2A));
        setColour(ToggleButton::tickColourId, Colour::Colour(0xFF2B2B2A));
        setColour(ToggleButton::tickDisabledColourId, Colour::Colour(0xFF2B2B2A));
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
    void updateHighpassCoefficient(const double cutoffFreq, const double sampleRate);
    void updateLowpassCoefficient(const double cutoffFreq, const double sampleRate);
    
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
    std::unique_ptr<XmlElement> savedAudioState;
    std::unique_ptr<XmlElement> noiseGateSettings;
    std::unique_ptr<XmlElement> highpassSettings;
    std::unique_ptr<XmlElement> lowpassSettings;
    
    //小音量時にメロディー判定を行わないようにするための閾値(dB)
    Slider sl_noiseGateThreshold;
    Label lbl_noiseGate;
    //ハイパスフィルター
    Slider sl_hpf;
    Label lbl_hpf;
    ToggleButton tgl_hpf;
    //ローパスフィルター
    Slider sl_lpf;
    Label lbl_lpf;
    ToggleButton tgl_lpf;
    //アプリ名,バージョン表示
    Label lbl_appName;
    Label lbl_version;
    
    ScopedPointer<dsp::Oversampling<float>> oversampling;
    static const int overSampleFactor = 1;//2^overSampleFactor
    using iir = dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, dsp::IIR::Coefficients<float>>;
    dsp::ProcessorChain<iir, iir> highpass;
    dsp::ProcessorDuplicator<dsp::FIR::Filter<float>, dsp::FIR::Coefficients<float>> lowpass;
    CustomLookAndFeel lookAndFeel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

// (This function is called by the app startup code to create our main component)
Component* createMainContentComponent()     { return new MainContentComponent(); }
