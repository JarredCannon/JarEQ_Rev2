/*
  ==============================================================================

    Constants.h
    Created: 8 Mar 2023 12:53:48am
    Author:  jarre

  ==============================================================================
*/

#pragma once

constexpr int maxNumFilterBands = 10;
constexpr float sampleRate = 44100.0f;
float freqs[maxNumFilterBands] = { 20.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f, 0.0f };
float Qs[maxNumFilterBands] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
float gains[maxNumFilterBands] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

