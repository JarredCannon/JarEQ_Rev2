// PluginEditor.cpp

#include "PluginEditor.h"

JarEQAudioProcessorEditor::JarEQAudioProcessorEditor (JarEQAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p)
{
    // Set up GUI components
setSize (600, 400);

titleLabel.setText ("JarEQ", juce::NotificationType::dontSendNotification);
titleLabel.setFont (juce::Font (24.0f, juce::Font::bold));
addAndMakeVisible (titleLabel);

globalGainLabel.setText ("Global Gain", juce::NotificationType::dontSendNotification);
addAndMakeVisible (globalGainLabel);

globalGainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
globalGainSlider.setRange (-48.0f, 48.0f, 0.1f);
globalGainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 80, 20);
globalGainSlider.setValue (*audioProcessor.globalGainParam);
globalGainSlider.addListener (this);
addAndMakeVisible (globalGainSlider);

mixLabel.setText ("Mix", juce::NotificationType::dontSendNotification);
addAndMakeVisible (mixLabel);

mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
mixSlider.setRange (0.0f, 1.0f, 0.01f);
mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 80, 20);
mixSlider.setValue (*audioProcessor.mixParam);
mixSlider.addListener (this);
addAndMakeVisible (mixSlider);

bypassButton.setButtonText ("Bypass");
bypassButton.addListener (this);
addAndMakeVisible (bypassButton);

analyzerButton.setButtonText ("Analyzer");
analyzerButton.addListener (this);
addAndMakeVisible (analyzerButton);

for (int i = 0; i < maxNumFilterBands; ++i)
{
    frequencyLabels[i].setText ("Freq", juce::NotificationType::dontSendNotification);
    addAndMakeVisible (frequencyLabels[i]);

    gainLabels[i].setText ("Gain", juce::NotificationType::dontSendNotification);
    addAndMakeVisible (gainLabels[i]);

    QLabels[i].setText ("Q", juce::NotificationType::dontSendNotification);
    addAndMakeVisible (QLabels[i]);

    typeLabels[i].setText ("Type", juce::NotificationType::dontSendNotification);
    addAndMakeVisible (typeLabels[i]);

    frequencySliders[i].setSliderStyle (juce::Slider::LinearHorizontal);
    frequencySliders[i].setRange (20.0f, 20000.0f, 0.1f);
    frequencySliders[i].setValue (*audioProcessor.frequencyParams[i]);
    frequencySliders[i].setTextBoxStyle (juce::Slider::TextBoxBelow, true, 80, 20);
    frequencySliders[i].addListener (this);
    addAndMakeVisible (frequencySliders[i]);

    gainSliders[i].setSliderStyle (juce::Slider::LinearVertical);
    gainSliders[i].setRange (-24.0f, 24.0f, 0.1f);
    gainSliders[i].setValue (*audioProcessor.gainParams[i]);
    gainSliders[i].setTextBoxStyle (juce::Slider::TextBoxBelow, true, 80, 20);
    gainSliders[i].addListener (this);
    addAndMakeVisible (gainSliders[i]);

    
    QSliders[i].setSliderStyle (juce::Slider::LinearVertical);
    QSliders[i].setRange (0.1f, 10.0f, 0.1f);
    QSliders[i].setValue (*audioProcessor.QParams[i]);
    QSliders[i].setTextBoxStyle (juce::Slider::TextBoxBelow, true, 80, 20);
    QSliders[i].addListener (this);
    addAndMakeVisible (QSliders[i]);

    typeComboBoxes[i].addItem ("Low Pass", 1);
    typeComboBoxes[i].addItem ("High Pass", 2);
    typeComboBoxes[i].addItem ("Band Pass", 3);
    typeComboBoxes[i].addItem ("Notch", 4);
    typeComboBoxes[i].addItem ("All Pass", 5);
    typeComboBoxes[i].addItem ("Peak", 6);
    typeComboBoxes[i].addItem ("Low Shelf", 7);
    typeComboBoxes[i].addItem ("High Shelf", 8);
    typeComboBoxes[i].setSelectedId (*audioProcessor.typeParams[i]);
    typeComboBoxes[i].addListener (this);
    addAndMakeVisible (typeComboBoxes[i]);
}

// Set up the layout
resized();

}

JarEQAudioProcessorEditor::~JarEQAudioProcessorEditor()
{
}

void JarEQAudioProcessorEditor::paint (juce::Graphics& g)
{
g.fillAll (juce::Colours::black);
}

void JarEQAudioProcessorEditor::resized()
{
int y = 10;
// Title
titleLabel.setBounds (10, y, getWidth() - 20, 30);
y += 40;

// Global gain
globalGainLabel.setBounds (10, y, 80, 20);
globalGainSlider.setBounds (100, y, getWidth() - 120, 20);
y += 30;

// Mix
mixLabel.setBounds (10, y, 80, 20);
mixSlider.setBounds (100, y, getWidth() - 120, 20);
y += 30;

// Bypass and analyzer buttons
bypassButton.setBounds (10, y, 80, 20);
analyzerButton.setBounds (100, y, 80, 20);
y += 30;

// Filter bands
for (int i = 0; i < audioProcessor.getNumFilterBands(); ++i)
{
    frequencyLabels[i].setBounds (10, y, 50, 20);
    gainLabels[i].setBounds (70, y, 50, 20);
    QLabels[i].setBounds (130, y, 50, 20);
    typeLabels[i].setBounds (190, y, 50, 20);
    frequencySliders[i].setBounds (10, y + 20, 50, 100);
    gainSliders[i].setBounds (70, y + 20, 50, 100);
    QSliders[i].setBounds (130, y + 20, 50, 100);
    typeComboBoxes[i].setBounds (190, y + 20, 80, 20);
    y += 130;
}
}

void JarEQAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    }

void JarEQAudioProcessorEditor::buttonClicked (juce::Button* button)
{
if (button == &bypassButton)
{
audioProcessor.bypassed = !audioProcessor.bypassed;
}
else if (button == &analyzerButton)
{
audioProcessor.analyzerEnabled = !audioProcessor.analyzerEnabled;
}
}

void JarEQAudioProcessorEditor::comboBoxChanged (juce::ComboBox* comboBox)
{
for (int i = 0; i < audioProcessor.getNumFilterBands(); ++i)
{
if (comboBox == &typeComboBoxes[i])
{
*audioProcessor.typeParams[i] = comboBox->getSelectedId();
}
}
}

// Analyzer class

JarEQAnalyzer::JarEQAnalyzer (JarEQAudioProcessor& p) : audioProcessor (p)
{
}

void JarEQAnalyzer::paint (juce::Graphics& g)
{
g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);

juce::Path path;

const auto waveform = audioProcessor.getWaveform();

if (! waveform.isEmpty())
{
    auto xRatio = getLocalBounds().getWidth() / (float) waveform.getNumSamples();
    auto yRatio = getLocalBounds().getHeight() / 2.0f;

    path.startNewSubPath (0, yRatio);

    for (int i = 0; i < waveform.getNumSamples(); ++i)
    {
        path.lineTo (i * xRatio, yRatio - waveform[i] * yRatio);
    }

    g.strokePath (path, juce::PathStrokeType (1.0f));
}

}

void JarEQAnalyzer::resized()
{
}

void JarEQAnalyzer::timerCallback()
{
repaint();
}

void JarEQAnalyzer::start()
{
startTimerHz (60);
}

void JarEQAnalyzer::stop()
{
stopTimer();
}

