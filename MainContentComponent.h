/*
  ==============================================================================

    MainContentComponent.h
    Created: 8 Mar 2023 1:09:06am
    Author:  jarre

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/
class MainContentComponent  : public juce::Component
{
public:
    MainContentComponent();
    ~MainContentComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};
