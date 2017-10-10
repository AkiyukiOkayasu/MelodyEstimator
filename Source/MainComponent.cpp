/*
 ==============================================================================
 
 This file was auto-generated!
 
 ==============================================================================
 */

//==============================================================================
/*
 This component lives inside our window, and this is where you should put all
 your controls and content.
 */

//==============================================================================
#include "MainComponent.h"

MainContentComponent::MainContentComponent()
{
    essentia::init();
    essentia::standard::AlgorithmFactory& factory = essentia::standard::AlgorithmFactory::instance();
    
    //    mel = factory.create("PitchMelodia", "minFrequency", (essentia::Real)220.0f, "maxFrequency", (essentia::Real)1760.0f, "voicingTolerance", -1.0f);//モノフォニック用
    mel = factory.create("PredominantPitchMelodia", "minFrequency", (essentia::Real)220.0f, "maxFrequency", (essentia::Real)7040.0f, "voicingTolerance", -1.0f);//ポリフォニック用
    //    voicingToleranceパラメータは要調整!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //    voicingTolerance (real ∈ [-1.0, 1.4], default = 0.2) :
    //    allowed deviation below the average contour mean salience of all contours (fraction of the standard deviation)
    //    http://essentia.upf.edu/documentation/reference/std_PredominantPitchMelodia.html
    
    pitchfilter = factory.create("PitchFilter");
    equalloudness = factory.create("EqualLoudness");
    std::cout<<"Essentia: algorithm created"<<std::endl;
    
    essentiaAudioBuffer.reserve(melFrameSize);
    essentiaAudioBuffer.resize(melFrameSize);
    std::iota(std::begin(essentiaAudioBuffer), std::end(essentiaAudioBuffer), 0.0f);
    pitch.reserve(200);
    pitchConfidence.reserve(200);
    freq.reserve(200);
    
    equalloudness->input("signal").set(essentiaAudioBuffer);
    equalloudness->output("signal").set(essentiaAudioBuffer);
    mel->input("signal").set(essentiaAudioBuffer);
    mel->output("pitch").set(pitch);
    mel->output("pitchConfidence").set(pitchConfidence);
    pitchfilter->input("pitch").set(pitch);
    pitchfilter->input("pitchConfidence").set(pitchConfidence);
    pitchfilter->output("pitchFiltered").set(freq);
    std::cout<<"Essentia: algorithm connected"<<std::endl;
    
    if(! sender.connect(ip, portnumber)){
        std::cout<<"OSC connection Error..."<<std::endl;
    }
    
    setSize (400, 400);
    setMacMainMenu(this);
    setAudioChannels (1, 0);
}

MainContentComponent::~MainContentComponent()
{
    shutdownAudio();
    essentia::shutdown();
}

//==============================================================================
void MainContentComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    std::cout<<"prepareToPlay()"<<std::endl;
    std::cout<<"Sample Rate: "<<sampleRate<<std::endl;
    std::cout<<"Buffer Size: "<<samplesPerBlockExpected<<std::endl;
}

void MainContentComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    const float* lch = bufferToFill.buffer->getReadPointer(0);
    
    for (int samples = 0; samples < bufferToFill.buffer->getNumSamples(); ++samples)
    {
        if (bufferIndex >= melFrameSize)
        {
            equalloudness->compute();
            mel->compute();
            mel->reset();//compute()あとにreset()は必ず呼ぶこと
            pitchfilter->compute();
            
            for (auto& el: freq)
            {
                int note = freqToMidi(el);
                if (midinote != note && note > 0)
                {
                    midinote = freqToMidi(el);
                    if (! sender.send("/juce/notenumber", (int)midinote)) {
                        std::cout<<"OSC send Error..."<<std::endl;
                    }
                }
            }
            bufferIndex = 0;
        }
        essentiaAudioBuffer.at(bufferIndex) = (essentia::Real) lch[samples];
        bufferIndex++;
    }
}

void MainContentComponent::releaseResources()
{
}

//==============================================================================
void MainContentComponent::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void MainContentComponent::resized()
{
}

StringArray MainContentComponent::getMenuBarNames()
{
    const char* const names[] = { "Settings", nullptr };
    return StringArray (names);
}

PopupMenu MainContentComponent::getMenuForIndex (int topLevelMenuIndex, const String& /*menuName*/)
{
    PopupMenu menu;
    if (topLevelMenuIndex == 0)
    {
        menu.addItem(1, "Audio Settings");
    }
    return menu;
}

void MainContentComponent::menuItemSelected(int menuItemID, int topLevelMenuIndex)
{
    if (menuItemID == 1)
    {
        showAudioSettings();
    }
}

void MainContentComponent::showAudioSettings()
{
    AudioDeviceSelectorComponent audioSettingsComp (deviceManager,
                                                    0, 256,
                                                    0, 256,
                                                    true, true, true, false);
    
    audioSettingsComp.setSize (500, 450);
    
    DialogWindow::LaunchOptions o;
    o.content.setNonOwned (&audioSettingsComp);
    o.dialogTitle                   = "Audio Settings";
    o.componentToCentreAround       = this;
    o.dialogBackgroundColour        = Colours::black;
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = true;
    o.resizable                     = false;
    o.runModal();
}

int MainContentComponent::freqToMidi(float freq)
{
    if (freq <= 20.0)//20Hz以下の時は-1を返す
    {
        return -1;
    }
    
    int notenumber = std::nearbyint(69.0 + 12.0 * log2(freq / 440.0));
    return notenumber;
}
