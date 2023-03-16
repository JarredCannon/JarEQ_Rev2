#include "MainComponent.h"
#include "MainContentComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    setSize (800, 600);

    addAndMakeVisible(new MainContentComponent(processor));
}

MainComponent::~MainComponent()
{
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    getComponent(0)->setBounds(0, 0, getWidth(), getHeight());
}