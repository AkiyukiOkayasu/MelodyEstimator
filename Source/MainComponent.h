#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>
#include <essentia/essentiamath.h>

#define XMLKEYAUDIOSETTINGS "audioDeviceState"
#define XMLKEYNOISEGATE "noiseGateSettings"
#define XMLKEYHIGHPASS "highpassFilterSettings"


//==============================================================================
struct CustomLookAndFeel    : public LookAndFeel_V4
{
    Colour me_green = Colour::Colour(0xFF73DB72);
    Colour me_baseColour = Colour::Colour(0xFF43444D);
    Colour me_outlineColour = Colour::Colour(0xFF888BA1);
    Colour me_sliderColour = Colour::Colour(0xFF4392F1);
    Colour me_textColour = Colour::Colour(0xFFFCFFFD);
    Colour me_clear = Colour::Colour(0x00000000);
    
    CustomLookAndFeel()
    {
        //Fader
        setColour(ResizableWindow::backgroundColourId, me_baseColour);
        setColour(Slider::thumbColourId, me_sliderColour);
        setColour(Slider::backgroundColourId, me_clear);
        setColour(Slider::textBoxTextColourId, me_textColour);
        setColour(Slider::textBoxOutlineColourId, me_clear);
        //Rotary
        setColour(Slider::rotarySliderFillColourId, me_sliderColour);
        //Label
        setColour(Label::textColourId,me_textColour);
    }
    
    void drawLinearSlider (Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const Slider::SliderStyle style, Slider& slider) override
    {
        auto lineThickness = jmin (15.0f, jmin (width, height) * 0.45f) * 0.1f;
        Path outlineArc;
        outlineArc.addRectangle(x, y, width, height);
        g.setColour(me_outlineColour);
        g.strokePath (outlineArc, PathStrokeType (lineThickness));
        drawLinearSliderThumb (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
    }
    
    void drawLinearSliderThumb (Graphics& g, int x, int y, int width, int height,
                                float sliderPos, float minSliderPos, float maxSliderPos,
                                const Slider::SliderStyle style, Slider& slider) override
    {
        auto knobColour = slider.findColour (Slider::rotarySliderFillColourId);
        g.setColour (knobColour);
        g.fillRect(Rectangle<float>(1.0f, sliderPos, width, 5.0f));
    }
    
    
    void drawRotarySlider (Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, Slider& slider) override
    {
        auto radius = jmin (width / 2, height / 2) - 2.0f;
        auto centreX = x + width * 0.5f;
        auto centreY = y + height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        
        g.setColour (slider.findColour (Slider::rotarySliderFillColourId));
        
        Path filledArc;
        filledArc.addPieSegment (rx, ry, rw, rw, rotaryStartAngle, angle, 0.0);
        g.fillPath (filledArc);

        auto lineThickness = jmin (15.0f, jmin (width, height) * 0.45f) * 0.1f;
        Path outlineArc;
        outlineArc.addPieSegment (rx, ry, rw, rw, rotaryStartAngle, rotaryEndAngle, 0.0);
        g.setColour(me_outlineColour);
        g.strokePath (outlineArc, PathStrokeType (lineThickness));
    }
};

//==============================================================================
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
    void sendMIDI(int noteNumber);
    void estimateMelody();
    void updateHighpassCoefficient(const double cutoffFreq, const double sampleRate);
    
    static constexpr int lengthToEstimateMelody_sample = 8192;//8192サンプルごとにメロディー推定を行う
    static constexpr int minNoteToEstimate = 48;//推定音域下限C2
    static constexpr int maxNoteToEstimate = 96;//推定音域上限C6
    static constexpr float standardPitch = 440.0f;//A=440Hz
    struct bufferAndIndex{
        AudioSampleBuffer buffer;//Essentiaでメロディー推定するための直近8192サンプルを保持するバッファー
        int index = 0;
    };
    bufferAndIndex preApplyEssentia;
    
    //MIDI
    static constexpr int midiChannel = 1;
    const String midiPortName = "MelodyEstimator";
    ScopedPointer<MidiOutput> midiOut;
    MidiMessage midiMessage;
    std::atomic<int> lastNote{-1};
    
    std::atomic<float> RMSlevel_dB{-100.0};
    
    //オーディオインターフェース,ノイズゲート設定の記録、呼び出し用
    ScopedPointer<ApplicationProperties> appProperties;
    std::unique_ptr<XmlElement> savedAudioState;
    std::unique_ptr<XmlElement> noiseGateSettings;
    std::unique_ptr<XmlElement> highpassSettings;
    
    //小音量時にメロディー判定を行わないようにするための閾値(dB)
    Slider sl_noiseGateThreshold;
    Label lbl_noiseGate;
    //ハイパスフィルター
    Slider sl_hpf;
    Label lbl_hpf;
    //バージョン表示
    Label lbl_version;
    //音名表示
    Label lbl_pitch;
    
    ScopedPointer<dsp::Oversampling<float>> oversampling;
    static const int overSampleFactor = 1;//2^overSampleFactor
    using iir = dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, dsp::IIR::Coefficients<float>>;
    dsp::ProcessorChain<iir, iir> highpass;
    CustomLookAndFeel lookAndFeel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

// (This function is called by the app startup code to create our main component)
Component* createMainContentComponent()     { return new MainContentComponent(); }
