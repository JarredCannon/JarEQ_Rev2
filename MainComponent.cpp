#include "MainComponent.h"

MainComponent::MainComponent()
	: mProcessor(*this)
{
	setSize(600, 400);
	auto& lf = getLookAndFeel();
	lf.setColour(juce::Slider::thumbColourId, juce::Colours::purple);
	lf.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::white);
	lf.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::grey);
	lf.setColour(juce::TextButton::buttonColourId, juce::Colours::purple);
	lf.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

	addAndMakeVisible(mGainSlider);
	mGainSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
	mGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 75, 25);
	mGainSlider.setRange(-60.0f, 12.0f, 0.1f);
	mGainSlider.setValue(0.0f);
	mGainSlider.addListener(this);

	addAndMakeVisible(mPeakFreqSlider);
	mPeakFreqSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
	mPeakFreqSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 75, 25);
	mPeakFreqSlider.setRange(20.0f, 20000.0f, 0.1f);
	mPeakFreqSlider.setValue(1000.0f);
	mPeakFreqSlider.addListener(this);

	addAndMakeVisible(mPeakGainSlider);
	mPeakGainSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
	mPeakGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 75, 25);
	mPeakGainSlider.setRange(-24.0f, 24.0f, 0.1f);
	mPeakGainSlider.setValue(0.0f);
	mPeakGainSlider.addListener(this);

	addAndMakeVisible(mPeakQualitySlider);
	mPeakQualitySlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
	mPeakQualitySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 75, 25);
	mPeakQualitySlider.setRange(0.1f, 10.0f, 0.1f);
	mPeakQualitySlider.setValue(1.0f);
	mPeakQualitySlider.addListener(this);

	addAndMakeVisible(mLowCutSlider);
	mLowCutSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
	mLowCutSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 75, 25);
	mLowCutSlider.setRange(20.0f, 20000.0f, 0.1f);
	mLowCutSlider.setValue(20.0f, 20000.f, 0.1f);

	addAndMakeVisible(mHighCutSlider);
	mHighCutSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
	mHighCutSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 75, 25);
	mHighCutSlider.setRange(20.0f, 20000.0f, 0.1f);

	setSize(600, 200);
}

MainComponent::~MainComponent()
{
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
	mProcessor.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
	mProcessor.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
	mProcessor.releaseResources();
}

void MainComponent::paint(juce::Graphics& g)
{
	g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
	auto border = 4;
	auto rect = getLocalBounds();
	auto h = rect.getHeight();
	//Top Section
	auto topRect = rect.removeFromTop(h * 0.25);
	auto lowCutRect = topRect.removeFromLeft(topRect.getWidth() * 0.33);
	auto highCutRect = topRect.removeFromRight(topRect.getWidth() * 0.5);
	mLowCutSlider.setBounds(lowCutRect.reduced(border));
	mHighCutSlider.setBounds(highCutRect.reduced(border));

	//Middle Section
	auto middleRect = rect.removeFromTop(h * 0.5);
	auto peakFreqRect = middleRect.removeFromLeft(middleRect.getWidth() * 0.33);
	auto peakGainRect = middleRect.removeFromLeft(middleRect.getWidth() * 0.5);
	mPeakFreqSlider.setBounds(peakFreqRect.reduced(border));
	mPeakGainSlider.setBounds(peakGainRect.reduced(border));

	//Bottom Section
	auto bottomRect = rect;
	mPeakQualitySlider.setBounds(bottomRect.reduced(border));
}

void MainComponent::sliderValueChanged(juce::Slider* slider)
{
	if (slider == &mGainSlider)
	{
		mProcessor.setGain(static_cast<float>(mGainSlider.getValue()));
	}
	else if (slider == &mLowCutSlider)
	{
		auto cutFreq = static_cast<float>(mLowCutSlider.getValue());
		auto& params = mProcessor.getParameters();
		auto index = static_cast<int>(AudioProcessorParameters::ParameterID::LowCutFreq);
		params[index]->setValueNotifyingHost(cutFreq);
	}
	else if (slider == &mHighCutSlider)
	{
		auto cutFreq = static_cast<float>(mHighCutSlider.getValue());
		auto& params = mProcessor.getParameters();
		auto index = static_cast<int>(AudioProcessorParameters::ParameterID::HighCutFreq);
		params[index]->setValueNotifyingHost(cutFreq);
	}
	else if (slider == &mPeakSlider)
	{
		auto peakFreq = static_cast<float>(mPeakFreqSlider.getValue());
		auto& params = mProcessor.getParameters();
		auto index = static_cast<int>(AudioProcessorParameters::ParameterID::PeakFreq);
		params[index]->setValueNotifyingHost(peakFreq);
		auto peakGain = static_cast<float>(mPeakGainSlider.getValue());
		index = static_cast<int>(AudioProcessorParameters::ParameterID::PeakGain);
		params[index]->setValueNotifyingHost(peakGain);

		auto peakQuality = static_cast<float>(mPeakQualitySlider.getValue());
		index = static_cast<int>(AudioProcessorParameters::ParameterID::PeakQuality);
		params[index]->setValueNotifyingHost(peakQuality);
	}
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
	// Use this method as the place to do any pre-playback
	// initialisation that you need..
	mProcessor.prepareToPlay(samplesPerBlockExpected, sampleRate);

	updateGui();
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
	// Your audio-processing code goes here!
	mProcessor.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
	mProcessor.releaseResources();

}

void MainComponent::paint(juce::Graphics& g)
{
	g.fillAll(juce::Colours::black);
}

void MainComponent::resized()
{
	auto bounds = getLocalBounds();
	auto padding = 10;
	auto sliderWidth = (bounds.getWidth() - 3 * padding) / 2;
	auto sliderHeight = bounds.getHeight() - 2 * padding;
	mLowCutSlider.setBounds(padding, padding, sliderWidth, sliderHeight);
	mHighCutSlider.setBounds(padding * 2 + sliderWidth, padding, sliderWidth, sliderHeight);
	mPeakSlider.setBounds(padding * 3 + sliderWidth * 2, padding, sliderWidth, sliderHeight);
}

void MainComponent::updateGui()
{
	auto& params = mProcessor.getParameters();
	mLowCutAttachment.reset(new Attachment(params[0], mLowCutSlider));
	mHighCutAttachment.reset(new Attachment(params[1], mHighCutSlider));
	mPeakAttachment.reset(new Attachment(params[2], mPeakSlider));
	mPeakFreqAttachment.reset(new Attachment(params[3], mPeakFreqSlider));
	mPeakGainAttachment.reset(new Attachment(params[4], mPeakGainSlider));
	mPeakQualityAttachment.reset(new Attachment(params[5], mPeakQualitySlider));
}

void MainComponent::sliderValueChanged(juce::Slider* slider)
{
	auto& params = mProcessor.getParameters();
	auto value = (float)slider->getValue();
	if (slider == &mGainSlider)
	{
		value = juce::Decibels::decibelsToGain(value, -60.0f);
	}

	else if (slider == &mPeakFreqSlider)
	{
		value = juce::dsp::Frequency::hertzToNormalizedFrequency(value, getSampleRate());
	}

	else if (slider == &mPeakGainSlider)
	{
		value = juce::Decibels::decibelsToGain(value, -24.0f);
	}

	else if (slider == &mPeakQSlider)
	{
		value = juce::dsp::BiquadFilter<float>::getQForGain(value, 1.0f);
	}

	auto index = slider->getProperty(Attachment::sliderAttachedPropertyId).toString().getIntValue();
	params[index]->setValueNotifyingHost(value);
}

void MainComponent::releaseResources()
{
	mProcessor.reset();
}

void MainComponent::paint(juce::Graphics& g)
{
	g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
	auto area = getLocalBounds();

	g.setColour(juce::Colours::white);
	g.setFont(15.0f);
	g.drawFittedText("Gain", area.removeFromTop(25), juce::Justification::centred, 1);

	area.removeFromTop(10);

	g.setFont(12.0f);
	g.drawFittedText("-60 dB", area.removeFromLeft(50), juce::Justification::centredLeft, 1);
	g.drawFittedText("12 dB", area.removeFromRight(50), juce::Justification::centredRight, 1);

	g.setColour(juce::Colours::purple);
	g.setFont(20.0f);
	g.drawFittedText(juce::String(mGainSlider.getValue(), 2), area, juce::Justification::centred, 1);

	g.setColour(juce::Colours::white);
	g.setFont(15.0f);
	g.drawFittedText("Peak Freq", area.removeFromTop(25), juce::Justification::centred, 1);

	area.removeFromTop(10);

	g.setFont(12.0f);
	g.drawFittedText("20 Hz", area.removeFromLeft(50), juce::Justification::centredLeft, 1);
	g.drawFittedText("20 kHz", area.removeFromRight(50), juce::Justification::centredRight, 1);

	g.setColour(juce::Colours::purple);
	g.setFont(20.0f);
	g.drawFittedText(juce::String(mPeakFreqSlider.getValue(), 2), area, juce::Justification::centred, 1);

	g.setColour(juce::Colours::white);
	g.setFont(15.0f);
	g.drawFittedText("Peak Gain", area.removeFromTop(25), juce::Justification::centred, 1);

	area.removeFromTop(10);

	g.setFont(12.0f);
	g.drawFittedText("-24 dB", area.removeFromLeft(50), juce::Justification::centredLeft, 1);
	g.drawFittedText("24 dB", area.removeFromRight(50), juce::Justification::centredRight, 1);

	g.setColour(juce::Colours::purple);
	g.setFont(20.0f);
	g.drawFittedText(juce::String(mPeakGainSlider.getValue(), 2), area, juce::Justification::centred, 1);

	g.setColour(juce::Colours::white);
	g.setFont(15.0f);
	g.drawFittedText("Peak Q", area.removeFromTop(25), juce::Justification::centred, 1);

	area.removeFromTop(10);

	g.setFont(12.0f);
	g.drawFittedText("0.1", area.removeFromLeft(50), juce::Justification::centredLeft, 1);
	g.drawFittedText("10", area.removeFromRight(50), juce::Justification::centredRight, 1);

	g.setColour(juce::Colours::purple);
	g.setFont(20.0f);
	g.drawFittedText(juce::String(mPeakQSlider.getValue(), 2), area, juce::Justification::centred, 1);
	// Peak Quality Label
	addAndMakeVisible(mPeakQualityLabel);
	mPeakQualityLabel.setText("Q", juce::NotificationType::dontSendNotification);
	mPeakQualityLabel.attachToComponent(&mPeakQualitySlider, false);
	mPeakQualityLabel.setJustificationType(juce::Justification::centred);

	// Gain Label
	addAndMakeVisible(mGainLabel);
	mGainLabel.setText("Gain", juce::NotificationType::dontSendNotification);
	mGainLabel.attachToComponent(&mGainSlider, false);
	mGainLabel.setJustificationType(juce::Justification::centred);

	// Peak Frequency Label
	addAndMakeVisible(mPeakFreqLabel);
	mPeakFreqLabel.setText("Frequency", juce::NotificationType::dontSendNotification);
	mPeakFreqLabel.attachToComponent(&mPeakFreqSlider, false);
	mPeakFreqLabel.setJustificationType(juce::Justification::centred);

	// Low Cut Label
	addAndMakeVisible(mLowCutLabel);
	mLowCutLabel.setText("Low Cut", juce::NotificationType::dontSendNotification);
	mLowCutLabel.attachToComponent(&mLowCutSlider, false);
	mLowCutLabel.setJustificationType(juce::Justification::centred);

	// High Cut Label
	addAndMakeVisible(mHighCutLabel);
	mHighCutLabel.setText("High Cut", juce::NotificationType::dontSendNotification);
	mHighCutLabel.attachToComponent(&mHighCutSlider, false);
	mHighCutLabel.setJustificationType(juce::Justification::centred);

	setSize(600, 200);
}

MainComponent::~MainComponent()
{
	mPeakFreqSlider.removeListener(this);
	mPeakGainSlider.removeListener(this);
	mPeakQualitySlider.removeListener(this);
	mLowCutSlider.removeListener(this);
	mHighCutSlider.removeListener(this);
	mPeakFreqAttachment.reset();
	mPeakGainAttachment.reset();
	mPeakQualityAttachment.reset();
	mLowCutSlopeAttachment.reset();
	mHighCutSlopeAttachment.reset();
	mLowCutFrequencyAttachment.reset();
	mHighCutFrequencyAttachment.reset();
	mLowCutBypassButtonAttachment.reset();
	mHighCutBypassButtonAttachment.reset();
	mPeakBypassButtonAttachment.reset();
	mGainSliderAttachment.reset();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
	mProcessor.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
	mProcessor.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
	mProcessor.releaseResources();
}

void MainComponent::paint(juce::Graphics& g)
{
	g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
	auto border = 4;
	auto rect = getLocalBounds();
	auto h = rect.getHeight();

	// Top Section
	auto topRect = rect.removeFromTop(h * 0.25);
	auto lowCutRect = topRect.removeFromLeft(topRect.getWidth() * 0.33);
	auto highCutRect = topRect.removeFromRight(topRect.getWidth() * 0.5);
	mLowCutSlider.setBounds(lowCutRect.reduced(border));
	mHighCutSlider.setBounds(highCutRect.reduced(border));

		// Middle Section
auto middleRect = rect.removeFromTop(h * 0.5);
auto peakFreqRect = middleRect.removeFromLeft(middleRect.getWidth() * 0.33);
auto peakGainRect = middleRect.removeFromLeft(middleRect.getWidth() * 0.5);
auto peakQRect = middleRect;
mPeakFreqSlider.setBounds(peakFreqRect.reduced(border));
mPeakGainSlider.setBounds(peakGainRect.reduced(border));
mPeakQualitySlider.setBounds(peakQRect.reduced(border));

// Bottom Section
auto bottomRect = rect;
auto gainRect = bottomRect.removeFromLeft(bottomRect.getWidth() * 0.5);
mGainSlider.setBounds(gainRect.reduced(border));
mGainLabel.setBounds(gainRect.removeFromTop(20));
}

void MainComponent::sliderValueChanged(juce::Slider* slider)
{
if (slider == &mGainSlider)
{
auto gain = static_cast<float>(slider->getValue());
mProcessor.setGain(gain);
}
else if (slider == &mPeakFreqSlider)
{
auto freq = static_cast<float>(slider->getValue());
mProcessor.setPeakFrequency(freq);
}
else if (slider == &mPeakGainSlider)
{
auto gain = static_cast<float>(slider->getValue());
mProcessor.setPeakGain(gain);
}
else if (slider == &mPeakQualitySlider)
{
auto q = static_cast<float>(slider->getValue());
mProcessor.setPeakQuality(q);
}
else if (slider == &mLowCutSlider)
{
auto freq = static_cast<float>(slider->getValue());
mProcessor.setLowCutFreq(freq);
}
else if (slider == &mHighCutSlider)
{
auto freq = static_cast<float>(slider->getValue());
mProcessor.setHighCutFreq(freq);
}
auto index = slider->getProperty(Attachment::sliderAttachedPropertyId).toString().getIntValue();
params[index]->setValueNotifyingHost(slider->getValue());

}

void MainComponent::updateToggleState(juce::Button* button)
{
	if (button == &mLowCutBypassButton)
	{
		auto state = button->getToggleState();
		mProcessor.setLowCutBypassed(state);
	}
	else if (button == &mHighCutBypassButton)
	{
		auto state = button->getToggleState();
		mProcessor.setHighCutBypassed(state);
	}
	else if (button == &mPeakBypassButton)
	{
		auto state = button->getToggleState();
		mProcessor.setPeakBypassed(state);
	}
}

void MainComponent::comboBoxChanged(juce::ComboBox* comboBox)
{
	auto selection = comboBox->getSelectedItemIndex();
	mProcessor.setProcessorType(selection);
}

void MainComponent::timerCallback()
{
	updateParametersFromFilter();
}
