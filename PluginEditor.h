// PluginEditor.h

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Constants.h"

class JarEQAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                   public juce::Slider::Listener,
                                   public juce::Button::Listener,
                                   public juce::ComboBox::Listener
{
public:
    explicit JarEQAudioProcessorEditor (JarEQAudioProcessor&);
    ~JarEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void sliderValueChanged (juce::Slider* slider) override;
    void buttonClicked (juce::Button* button) override;
    void comboBoxChanged (juce::ComboBox* comboBox) override;

private:
    // Processor reference
    JarEQAudioProcessor& audioProcessor;

    // GUI components
    juce::Label titleLabel;
    juce::Label globalGainLabel;
    juce::Slider globalGainSlider;
    juce::Label mixLabel;
    juce::Slider mixSlider;
    juce::TextButton bypassButton;
    juce::TextButton analyzerButton;

    // Filter band parameters
    std::array<juce::Slider, maxNumFilterBands> frequencySliders;
    std::array<juce::Slider, maxNumFilterBands> gainSliders;
    std::array<juce::Slider, maxNumFilterBands> QSliders;
    std::array<juce::Label, maxNumFilterBands> frequencyLabels;
    std::array<juce::Label, maxNumFilterBands> gainLabels;
    std::array<juce::Label, maxNumFilterBands> QLabels;
    std::array<juce::ComboBox, maxNumFilterBands> typeComboBoxes;
    std::array<juce::Label, maxNumFilterBands> typeLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JarEQAudioProcessorEditor)
};
