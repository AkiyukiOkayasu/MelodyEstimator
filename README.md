# Melody Estimator  

[![GitHub version](https://badge.fury.io/gh/AkiyukiOkayasu%2FMelodyEstimator.svg)](https://badge.fury.io/gh/AkiyukiOkayasu%2FMelodyEstimator)
[![LICENSE](https://img.shields.io/github/license/AkiyukiOkayasu/MelodyEstimator)](LICENSE)

Stand-alone application for estimate the main melody from streaming audio in real-time.  
This is made with [JUCE](https://github.com/WeAreROLI/JUCE) and [Essentia](https://github.com/MTG/essentia).  
Commentary(in Japasene) is [here](https://qiita.com/AkiyukiOkayasu/items/7b5a0671cbfc8e704590).  

![screen shot](https://github.com/AkiyukiOkayasu/MelodyEstimator/blob/master/screenshot.png)  

## Feature  
- Real-time estimating.
- Wide estimate pitch range(C2~C6).
- Sending MIDI that estimated result.

## System Requirements    
MacOS 10.11 later  
The sample rate must be 44100Hz, not supported other sample rates.

## Download  
You can download the pre-built application from [release](https://github.com/AkiyukiOkayasu/MelodyEstimator/releases).
