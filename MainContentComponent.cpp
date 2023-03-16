#include "MainContentComponent.h"
#include "Constants.h"
#include "PluginEditor.h"

//==============================================================================
MainContentComponent::MainContentComponent(JarEQAudioProcessor& p) :
    processor(p),
    globalGainSlider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxBelow),
    mixSlider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxBelow),
    bypassButton("Bypass"),
    analyzerButton("Analyzer")
{
    addAndMakeVisible(globalGainSlider);
    addAndMakeVisible(mixSlider);
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(analyzerButton);

    // Add filter bands
    for (int i = 0; i < maxNumFilterBands; ++i)
    {
        frequencySliders.add(new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxBelow));
        addAndMakeVisible(frequencySliders.getLast());

        gainSliders.add(new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxBelow));
        addAndMakeVisible(gainSliders.getLast());

        QSliders.add(new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxBelow));
        addAndMakeVisible(QSliders.getLast());
    }
}

MainContentComponent::~MainContentComponent()
{
    for (int i = 0; i < frequencySliders.size(); ++i)
    {
        delete frequencySliders[i];
        delete gainSliders[i];
        delete QSliders[i];
    }
}

void MainContentComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff323e44));
}

void MainContentComponent::resized()
{
    globalGainSlider.setBounds(20, 20, 100, 20);
    mixSlider.setBounds(20, 50, 100, 20);
    bypassButton.setBounds(20, 80, 80, 20);
    analyzerButton.setBounds(20, 110, 80, 20);

    // Position filter band sliders
    int y = 150;
    int height = 40;
    for (int i = 0; i < maxNumFilterBands; ++i)
    {
        frequencySliders[i]->setBounds(20, y, 100, height);
        gainSliders[i]->setBounds(130, y, 100, height);
        QSliders[i]->setBounds(240, y, 100, height);
        y += height + 10;
    }
}


//==============================================================================
MainContentComponent::MainContentComponent()
{
    setSize (800, 600);
    
    addAndMakeVisible(freqSlider);
    freqSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    freqSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 50, 25);
    freqSlider.setRange(20.0, 20000.0, 0.1);
    freqSlider.setValue(1000.0);
    freqSlider.addListener(this);
}

MainContentComponent::~MainContentComponent()
{
}

void MainContentComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainContentComponent::resized()
{
    freqSlider.setBounds(200,200,400,400);
}

void MainContentComponent::sliderValueChanged(juce::Slider* slider)
{
    if(slider == &freqSlider)
    {
        pluginProcessor.setFrequency(freqSlider.getValue());
    }
}
