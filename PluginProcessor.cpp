#include "PluginProcessor.h"
#include "Constants.h"

//==============================================================================
JarEQAudioProcessor::JarEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
#if JucePlugin_IsMidiEffect
    .withInput("Input", juce::AudioChannelSet::mono(), true)
    .withOutput("Output", juce::AudioChannelSet::mono(), true)
#else
    .withInput("Input", juce::AudioChannelSet::stereo(), true)
#if JucePlugin_IsMidiEffect
    .withMidiInput("Midi Input", true)
#endif
    .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
#endif
#endif
)
#endif
{
    // Set initial values for plugin parameters
    for (int i = 0; i < maxNumFilterBands; ++i)
    {
        frequencyParams[i].reset(new AudioParameterFloat(frequencyParameterID + juce::String(i), "Band " + juce::String(i + 1) + " Frequency", NormalisableRange<float>(20.f, 20000.f, 1.f), 1000.f));
        gainParams[i].reset(new AudioParameterFloat(gainParameterID + juce::String(i), "Band " + juce::String(i + 1) + " Gain", NormalisableRange<float>(-24.f, 24.f, 0.5f), 0.f));
        QParams[i].reset(new AudioParameterFloat(QParameterID + juce::String(i), "Band " + juce::String(i + 1) + " Q", NormalisableRange<float>(0.1f, 10.f, 0.1f), 1.f));
    }

    globalGainParam.reset(new AudioParameterFloat(globalGainParameterID, "Global Gain", NormalisableRange<float>(-24.f, 24.f, 0.5f), 0.f));
    mixParam.reset(new AudioParameterFloat(mixParameterID, "Mix", NormalisableRange<float>(0.f, 1.f, 0.01f), 0.5f));
    bypassParam.reset(new AudioParameterBool(bypassParameterID, "Bypass", false));
    analyzerParam.reset(new AudioParameterBool(analyzerParameterID, "Analyzer", false));

    // Add parameters to the processor
    for (int i = 0; i < maxNumFilterBands; ++i)
    {
        addParameter(frequencyParams[i].get());
        addParameter(gainParams[i].get());
        addParameter(QParams[i].get());
    }

    addParameter(globalGainParam.get());
    addParameter(mixParam.get());
    addParameter(bypassParam.get());
    addParameter(analyzerParam.get());

    // Initialize analyzer
    fftDataGenerator.reset(new AudioVisualiserComponent::FFTDataGenerator(fftOrder));
    fftDataGenerator->addChangeListener(this);
}

JarEQAudioProcessor::~JarEQAudioProcessor()
{
}

//==============================================================================
const juce::String JarEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool JarEQAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool JarEQAudioProcessor::producesMidi() const
{
return false;
}
bool JarEQAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
return true;
#else
return false;
#endif
}

double JarEQAudioProcessor::getTailLengthSeconds() const
{
return 0.0;
}

int JarEQAudioProcessor::getNumPrograms()
{
return 1; // we only have one program
}

int JarEQAudioProcessor::getCurrentProgram()
{
return 0;
}

void JarEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String JarEQAudioProcessor::getProgramName (int index)
{
return {};
}

void JarEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void JarEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
for (int i = 0; i < maxNumFilterBands; ++i)
{
filters[i].reset(new IIRFilter());
    float freq = *frequencyParams[i];
    float gain = *gainParams[i];
    float Q = *QParams[i];

    filters[i]->setCoefficients(IIRCoefficients::makePeakFilter(sampleRate, freq, Q, Decibels::decibelsToGain(gain)));

    filterStates[i].reset(new IIRFilter::FilterStates<float>(1));
    filterStates[i]->reset();
}

// Set sample rate and block size for analyzer
fftDataGenerator->prepare({ static_cast<size_t> (samplesPerBlock), static_cast<size_t> (getTotalNumInputChannels()) });

lastSampleRate = sampleRate;
}

void JarEQAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool JarEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
ignoreUnused (layouts);
return true;
#else
// This is the place where you check if the layout is supported.
// In this template code we only support mono or stereo.
// Some plugin hosts, such as certain GarageBand versions, will only
// load plugins that support stereo bus layouts.
// Therefore it is important to make sure the channel count is
// limited to mono or stereo.
if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
&& layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
return false;
// This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
return false;
#endif
return true;
#endif
}
#endif

void JarEQAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
ScopedNoDenormals noDenormals;

// Apply filters
for (int i = 0; i < maxNumFilterBands; ++i)
{
    float freq = *frequencyParams[i];
    float gain = *gainParams[i];
    float Q = *QParams[i];

    if (freq != lastFrequencyValues[i] || gain != lastGainValues[i] || Q != lastQValues[i] || lastSampleRate != getSampleRate())
    {
        filters[i]->setCoefficients(IIRCoefficients::makePeakFilter(getSampleRate(), freq, Q, Decibels::decibelsToGain(gain)));
    }

    for (int channel = 0; channel < getTotalNumInputChannels(); ++channel)    {
        filters[i]->processSamples(buffer.getWritePointer(channel), buffer.getNumSamples(), filterStates[i]->getRawDataPointer());
    }

    lastFrequencyValues[i] = freq;
    lastGainValues[i] = gain;
    lastQValues[i] = Q;
}

// Apply global gain
auto globalGain = Decibels::decibelsToGain(*globalGainParam);

for (int channel = 0; channel < getTotalNumInputChannels(); ++channel)
{
    buffer.applyGain(channel, 0, buffer.getNumSamples(), globalGain);
}

// Mix dry and wet signals
auto mix = *mixParam;
buffer.applyGain(0, 0, buffer.getNumSamples(), 1.f - mix);
buffer.applyGain(1, 0, buffer.getNumSamples(), mix);

// Bypass if necessary
if (*bypassParam)
{
    buffer.copyFrom(1, 0, buffer.getReadPointer(0), buffer.getNumSamples());
}

// Generate FFT data for analyzer
if (*analyzerParam)
{
    auto* writePtr = fftDataGenerator->getFFTData();
    auto* readPtr = buffer.getReadPointer(0);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        *writePtr++ = *readPtr++;
    }

    fftDataGenerator->setNumSamples(buffer.getNumSamples());
}
}

//==============================================================================
AudioProcessorValueTreeState& JarEQAudioProcessor::getValueTreeState()
{
return valueTreeState;
}

void JarEQAudioProcessor::parameterChanged(const String& parameterID, float newValue)
{
if (parameterID == globalGainParameterID)
{
updateHostDisplay();
}
}

void JarEQAudioProcessor::updateHostDisplay()
{
// Send parameter update message to host
AudioProcessorParameter* globalGainParamPtr = getParameter(globalGainParameterID);
if (globalGainParamPtr != nullptr)
{
    AudioProcessorParameter::ListenerList listeners(globalGainParamPtr->getListeners());
    listeners.call(&AudioProcessorParameter::Listener::parameterValueChanged, globalGainParamPtr, globalGainParamPtr->getValue());
}
}

void JarEQAudioProcessor::changeListenerCallback(ChangeBroadcaster* source)
{
if (source == fftDataGenerator.get())
{
sendChangeMessage();
}
}

//==============================================================================
#ifndef JucePlugin_PreferredChannelConfigurations
bool JarEQAudioProcessor::supportsDoublePrecisionProcessing() const
{
return false;
}
#endif

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
return new JarEQAudioProcessor();
}

void JarEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (bypassed)
    {
        return;
    }

    updateFilters();

    dsp::AudioBlock<float> block (buffer);
    dsp::ProcessContextReplacing<float> context (block);

    // Process the audio through the filter chains
    for (auto& filterChain : filterChains)
    {
        filterChain.process (context);
    }

    // Apply global gain and mix
    dsp::multiply (block, Decibels::decibelsToGain (globalGainParam->get()), block);
    dsp::mix (block, dryBuffer, wetBuffer, mixParam->get());
}

void JarEQAudioProcessor::updateFilters()
{
    // Update the filter chains based on the current parameters
    for (int i = 0; i < filterChains.size(); ++i)
    {
        auto& filterChain = filterChains.getReference(i);
        auto& filterParams = filterParamsList[i];

        filterChain.reset();

        for (int j = 0; j < filterParams.size(); ++j)
        {
            auto& filter = filterChain.getReference(j);

            filter.setType (filterParams[j].type);
            filter.setFrequency (filterParams[j].frequency);
            filter.setQ (filterParams[j].Q);
            filter.setGain (filterParams[j].gain);
        }
    }
}

void JarEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Allocate the buffers for the dry and wet signals
    dryBuffer.setSize (2, samplesPerBlock);
    wetBuffer.setSize (2, samplesPerBlock);

    // Initialize the filter chains
    for (int i = 0; i < maxNumFilterBands; ++i)
    {
        filterChains.add (dsp::ProcessorChain<dsp::IIR::Filter<float>, dsp::IIR::Filter<float>>());
        filterParamsList.add ({FilterParams (dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (44100.0)), 
                               FilterParams (dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (44100.0))});
    }

    updateFilters();
}

void JarEQAudioProcessor::releaseResources()
{
    // Reset the filter chains
    filterChains.clear();

    // Free the buffers
    dryBuffer.setSize (0, 0);
    wetBuffer.setSize (0, 0);
}

void JarEQAudioProcessor::reset()
{
    // Reset all filter parameters to their default values
    for (auto& filterParams : filterParamsList)
    {
        for (auto& params : filterParams)
        {
            params.type = 1;
            params.frequency = 1000.0f;
            params.Q = 1.0f;
            params.gain = 0.0f;
        }
    }

    // Reset global gain and mix
    *globalGainParam = 0.0f;
    *mixParam = 0.5f;

    // Update the filters
    updateFilters();
}

AudioProcessorValueTreeState::ParameterLayout JarEQAudioProcessor::createParameterLayout()
{
    juce::OwnedArray<AudioParameterFloat> params;

    // Global gain parameter
    auto globalGainParam = std::make_unique<AudioParameterFloat> ("global_gain", "Global Gain", -48.0f, 48.0f, 0.0f);
    this->globalGainParam = globalGainParam.get();
    params.add (std::move (globalGainParam));
    
    // Mix parameter
auto mixParam = std::make_unique<AudioParameterFloat> ("mix", "Mix", 0.0f, 1.0f, 0.5f);
this->mixParam = mixParam.get();
params.add (std::move (mixParam));

// Filter band parameters
for (int i = 0; i < maxNumFilterBands; ++i)
{
    auto frequencyParam = std::make_unique<AudioParameterFloat> ("frequency_" + juce::String (i), "Frequency " + juce::String (i + 1), 
                                                                  20.0f, 20000.0f, 1000.0f);
    frequencyParams.add (frequencyParam.get());
    params.add (std::move (frequencyParam));

    auto gainParam = std::make_unique<AudioParameterFloat> ("gain_" + juce::String (i), "Gain " + juce::String (i + 1), 
                                                             -24.0f, 24.0f, 0.0f);
    gainParams.add (gainParam.get());
    params.add (std::move (gainParam));

    auto QParam = std::make_unique<AudioParameterFloat> ("Q_" + juce::String (i), "Q " + juce::String (i + 1), 
                                                          0.1f, 10.0f, 1.0f);
    QParams.add (QParam.get());
    params.add (std::move (QParam));

    auto typeParam = std::make_unique<AudioParameterChoice> ("type_" + juce::String (i), "Type " + juce::String (i + 1), 
                                                              juce::StringArray::fromTokens ("LPF,HPF,BPF,Notch,All Pass,Peak,LSF,HSF", ",", ""));
    typeParams.add (typeParam.get());
    params.add (std::move (typeParam));
}

return { params.begin(), params.end() };

}

AudioBuffer<float> JarEQAudioProcessor::getWaveform()
{
if (analyzerEnabled && ! bypassed)
{
AudioBuffer<float> buffer (1, 1024);
    ScopedNoDenormals noDenormals;
    ScopedLock lock (audioBufferLock);

    for (int channel = 0; channel < getTotalNumInputChannels(); ++channel)
    {
        buffer.clear();
        buffer.addFrom (0, 0, bufferToFill.buffer, channel, 0, buffer.getNumSamples());
    }

    return buffer;
}

return {};

}

// FilterParams class

JarEQAudioProcessor::FilterParams::FilterParams (const IIR::Coefficients<float>& coefficients)
{
setCoefficients (coefficients);
}

void JarEQAudioProcessor::FilterParams::setCoefficients (const IIR::Coefficients<float>& coefficients)
{
type = 1;
frequency = coefficients.getCutOffFrequency();
Q = coefficients.getResonance();
gain = 0.0f;
}

// FilterChain class

void JarEQAudioProcessor::FilterChain::reset()
{
for (auto& filter : filters)
{
filter.reset();
}
}

void JarEQAudioProcessor::FilterChain::prepare (const dsp::ProcessSpec& spec)
{
for (auto& filter : filters)
{
filter.prepare (spec);
}
}

void JarEQAudioProcessor::FilterChain::process (const dsp::ProcessContextReplacing<float>& context)
{
for (auto& filter : filters)
{
filter.process (context);
}
}

void JarEQAudioProcessor::FilterChain::addFilter (const IIR::Coefficients<float>& coefficients)
{
filters.add (IIR::Filter<float> (coefficients));
}

void JarEQAudioProcessor::FilterChain::setFrequency (float frequency)
{
for (auto& filter : filters)
{
filter.setFrequency (frequency);
}
}

void JarEQAudioProcessor::FilterChain::setQ (float Q)
{
for (auto& filter : filters)
{
filter.setQ (Q);
}
}

void JarEQAudioProcessor::FilterChain::setGain (float gain)
{
for (auto& filter : filters)
{
filter.setGain (Decibels::decibelsToGain (gain));
}
}

void JarEQAudioProcessor::FilterChain::setType (int type)
{
if (type != lastType)
{
filters.clear();

    switch (type)
    {
        case 0:
            filters.add (IIR::Filter<float> (IIR::Coefficients<float>::makeLowPass (44100.0, 1000.0)));
            break;

        case 1:
            filters.add (IIR::Filter<float> (IIR::Coefficients<float>::makeHighPass (44100.0, 1000.0)));
            break;

        case 2:
            filters.add (IIR::Filter<float> (IIR::Coefficients<float>::makeBandPass (44100.0, 1000.0, 1.0)));
            break;

        case 3:
            filters.add (IIR::Filter<float> (IIR::Coefficients<float>::makeNotch (44100.0, 1000.0, 1.0)));
            break;

        case 4:
            filters.add (IIR::Filter<float> (IIR::Coefficients<float>::makeAllPass (44100.0, 1000.0)));
            break;

        case 5:
            filters.add (IIR::Filter<float> (IIR::Coefficients<float>::makePeakFilter (44100.0, 1000.0, 1.0, 0.0)));
            break;

        case 6:
            filters.add (IIR::Filter<float> (IIR::Coefficients<float>::makeLowShelf (44100.0, 1000.0, 1.0, 0.0)));
            break;

        case 7:
            filters.add (IIR::Filter<float> (IIR::Coefficients<float>::makeHighShelf (44100.0, 1000.0, 1.0, 0.0)));
            break;

        default:
            break;
    }

    lastType = type;
}
}
// PresetManager class

void JarEQAudioProcessor::PresetManager::setCurrentPresetName (const juce::String& name)
{
currentPresetName = name;
}

const juce::String& JarEQAudioProcessor::PresetManager::getCurrentPresetName() const
{
return currentPresetName;
}

void JarEQAudioProcessor::PresetManager::savePreset (const juce::String& name)
{
ValueTree presetTree = processorTree.getOrCreateChildWithName ("PRESET", nullptr);
bool alreadyExists = false;

for (int i = 0; i < presetTree.getNumChildren(); ++i)
{
    if (presetTree.getChild (i).hasProperty ("name") && presetTree.getChild (i).getProperty ("name") == name)
    {
        presetTree.removeChild (i, nullptr);
        alreadyExists = true;
    }
}

ValueTree preset ("PRESET");
preset.setProperty ("name", name, nullptr);

for (int i = 0; i < processor.getNumFilterBands(); ++i)
{
    ValueTree filter ("FILTER");
    filter.setProperty ("index", i, nullptr);
    filter.setProperty ("type", processor.getTypeParam (i)->getIndex(), nullptr);
    filter.setProperty ("frequency", processor.getFrequencyParam (i)->get(), nullptr);
    filter.setProperty ("Q", processor.getQParam (i)->get(), nullptr);
    filter.setProperty ("gain", processor.getGainParam (i)->get(), nullptr);

    preset.addChild (filter, -1, nullptr);
}

if (! alreadyExists)
    presetTree.addChild (preset, -1, nullptr);

processorTree.setProperty ("presetIndex", presetTree.indexOf (preset), nullptr);

juce::File presetFile (getPresetDirectory().getFullPathName() + "/" + name + ".xml");
ScopedPointer<XmlElement> xml = processorTree.createXml();

copyXmlToBinary (*xml, *(presetFile.createOutputStream()));

currentPresetName = name;

}

void JarEQAudioProcessor::PresetManager::loadPreset (const juce::String& name)
{
ValueTree presetTree = processorTree.getOrCreateChildWithName ("PRESET", nullptr);
ValueTree preset;

for (int i = 0; i < presetTree.getNumChildren(); ++i)
{
    if (presetTree.getChild (i).hasProperty ("name") && presetTree.getChild (i).getProperty ("name") == name)
    {
        preset = presetTree.getChild (i);
        break;
    }
}

if (! preset.isValid())
    return;

processorTree.setProperty ("presetIndex", presetTree.indexOf (preset), nullptr);

for (int i = 0; i < processor.getNumFilterBands(); ++i)
{
    if (preset.getChildWithName ("FILTER" + juce::String (i)).isValid())
    {
        auto type = preset.getChildWithName ("FILTER" + juce::String (i)).getProperty ("type");
        auto frequency = preset.getChildWithName ("FILTER" + juce::String (i)).getProperty ("frequency");
        auto Q = preset.getChildWithName ("FILTER" + juce::String (i)).getProperty ("Q");
        auto gain = preset.getChildWithName ("FILTER" + juce::String (i)).getProperty ("gain");

        processor.getTypeParam (i)->setValueNotifyingHost (type);
        processor.getFrequencyParam (i)->setValueNotifyingHost (frequency);
        processor.getQParam (i)->setValueNotifyingHost (Q);
        processor.getGainParam (i)->setValueNotifyingHost (gain);
    }
}

currentPresetName = name;
}

void JarEQAudioProcessor::PresetManager::deletePreset (const juce::String& name)
{
ValueTree presetTree = processorTree.getOrCreateChildWithName ("PRESET", nullptr);

for (int i = 0; i < presetTree.getNumChildren(); ++i)
{
    if (presetTree.getChild (i).hasProperty ("name") && presetTree.getChild (i).getProperty ("name") == name)
    {
        presetTree.removeChild (i, nullptr);
        break;
    }
}
}

int JarEQAudioProcessor::PresetManager::getNumPresets() const
{
ValueTree presetTree = processorTree.getOrCreateChildWithName ("PRESET", nullptr);
return presetTree.getNumChildren();
}

juce::String JarEQAudioProcessor::PresetManager::getPresetName (int index) const
{
ValueTree presetTree = processorTree.getOrCreateChildWithName ("PRESET", nullptr);
return presetTree.getChild (index).getProperty ("name");
}

void JarEQAudioProcessor::PresetManager::getNextPreset()
{
int index = getCurrentPresetIndex();
index = index == getNumPresets() - 1 ? 0 : index + 1;
loadPreset (getPresetName (index));
}

void JarEQAudioProcessor::PresetManager::getPreviousPreset()
{
int index = getCurrentPresetIndex();
index = index == 0 ? getNumPresets() - 1 : index - 1;
loadPreset (getPresetName (index));
}

int JarEQAudioProcessor::PresetManager::getCurrentPresetIndex() const
{
ValueTree presetTree = processorTree.getOrCreateChildWithName ("PRESET", nullptr);
return processorTree.getProperty ("presetIndex", 0);
}

void JarEQAudioProcessor::PresetManager::renamePreset (const juce::String& oldName, const juce::String& newName)
{
ValueTree presetTree = processorTree.getOrCreateChildWithName ("PRESET", nullptr);


for (int i = 0; i < presetTree.getNumChildren(); ++i)
{
    if (presetTree.getChild (i).hasProperty ("name") && presetTree.getChild (i).getProperty ("name") == oldName)
    {
        presetTree.getChild (i).setProperty ("name", newName, nullptr);
        break;
    }
}
}
// PluginEditor class

void JarEQAudioProcessorEditor::resized()
{
auto bounds = getLocalBounds();


auto filterSection = bounds.removeFromTop (bounds.getHeight() * 0.25);
filterSection.removeFromTop (10);

auto globalGainSection = bounds.removeFromTop (bounds.getHeight() * 0.15);
globalGainSection.removeFromTop (10);

auto mixSection = bounds.removeFromTop (bounds.getHeight() * 0.15);
mixSection.removeFromTop (10);

auto bypassSection = bounds.removeFromTop (bounds.getHeight() * 0.15);
bypassSection.removeFromTop (10);

auto analyzerSection = bounds.removeFromTop (bounds.getHeight() * 0.15);
analyzerSection.removeFromTop (10);

auto presetSection = bounds.removeFromTop (bounds.getHeight() * 0.15);
presetSection.removeFromTop (10);

auto filterControls = filterComponent->getLocalBounds();
filterControls.setWidth (filterControls.getWidth() / filterComponent->getNumFilterBands());

for (int i = 0; i < filterComponent->getNumFilterBands(); ++i)
{
    filterComponent->getFilter(i)->setBounds (filterControls.removeFromLeft (filterControls.getWidth()).reduced (10));
}

globalGainSlider.setBounds (globalGainSection.removeFromLeft (200).reduced (10));
mixSlider.setBounds (mixSection.removeFromLeft (200).reduced (10));
bypassButton.setBounds (bypassSection.removeFromLeft (200).reduced (10));
analyzerButton.setBounds (analyzerSection.removeFromLeft (200).reduced (10));
presetComboBox.setBounds (presetSection.removeFromLeft (200).reduced (10));


}

void JarEQAudioProcessorEditor::comboBoxChanged (ComboBox* comboBox)
{
if (comboBox == presetComboBox)
{
processor.loadPreset (presetComboBox->getText());
}
}

void JarEQAudioProcessorEditor::buttonClicked (Button* button)
{
if (button == &bypassButton)
{
processor.setBypassed (! processor.isBypassed());
}
else if (button == &analyzerButton)
{
analyzerButton.setToggleState (true, NotificationType::dontSendNotification);
analyzerComponent.setVisible (true);
}
}

void JarEQAudioProcessorEditor::sliderValueChanged (Slider* slider)
{
if (slider == &globalGainSlider)
{
processor.setGlobalGain (globalGainSlider.getValue());
}
else if (slider == &mixSlider)
{
processor.setMix (mixSlider.getValue());
}
}


// PluginEditor class (continued)

void JarEQAudioProcessorEditor::setFilterComponent (FilterComponent* component)
{
filterComponent.reset (component);
addAndMakeVisible (filterComponent.get());
filterComponent->setBands (processor.getParameters());
filterComponent->setLabels ("FREQ", "Q", "GAIN");
}

void JarEQAudioProcessorEditor::setAnalyzerComponent (AudioVisualiserComponent* component)
{
analyzerComponent.reset (component);
addAndMakeVisible (analyzerComponent.get());
analyzerComponent->setSamplesPerBlock (512);
analyzerComponent->setBufferSize (1024);
analyzerComponent->setColours (Colours::white, Colours::grey, Colours::transparentWhite);
analyzerComponent->setVerticalZoomFactor (1.0f);
analyzerComponent->setHorizontalZoomFactor (1.0f);
analyzerComponent->setUpdateInterval (30);
analyzerComponent->setRange (Range<float> (20.0f, 20000.0f), Range<float> (-40.0f, 20.0f));
analyzerComponent->setTransformFunction ([] (float value) { return Decibels::gainToDecibels (value); });
analyzerComponent->setVerticalAxisTextFunction ([] (float value, int) { return Decibels::toString (value, 2) + " dB"; });
analyzerComponent->setVisible (false);
}

// FilterComponent class

void FilterComponent::paint (Graphics& g)
{
g.fillAll (Colours::darkgrey);
}

void FilterComponent::resized()
{
auto bounds = getLocalBounds();
bounds.reduce (20, 20);

if (bands.size() > 0)
{
    auto filterControls = bounds.removeFromTop (bounds.getHeight() * 0.8f);
    filterControls.setWidth (filterControls.getWidth() / bands.size());

    for (int i = 0; i < bands.size(); ++i)
    {
        auto& band = bands.getReference (i);
        band.frequencySlider.setBounds (filterControls.removeFromLeft (filterControls.getWidth()).reduced (5));
        band.QSlider.setBounds (filterControls.removeFromLeft (filterControls.getWidth()).reduced (5));
        band.gainSlider.setBounds (filterControls.removeFromLeft (filterControls.getWidth()).reduced (5));
    }
}

addBandButton.setBounds (bounds.removeFromTop (bounds.getHeight() * 0.5f));
removeBandButton.setBounds (bounds.removeFromTop (bounds.getHeight() * 0.5f));

}

void FilterComponent::addBand()
{
auto& processor = dynamic_cast<JarEQAudioProcessor&> (getProcessor());

if (bands.size() < processor.getMaxNumFilterBands())
{
    auto index = bands.size();
    auto frequencyParam = processor.getParameters().getUnchecked (index * 3);
    auto QParam = processor.getParameters().getUnchecked (index * 3 + 1);
    auto gainParam = processor.getParameters().getUnchecked (index * 3 + 2);

    Band band;
    band.frequencySlider.setSliderStyle (Slider::Rotary);
    band.frequencySlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    band.frequencySlider.setRange (frequencyParam->getRange().getStart(), frequencyParam->getRange().getEnd(), 1.0);
    band.frequencySlider.setValue (*frequencyParam);
    band.frequencySlider.addListener (this);
    addAndMakeVisible (band.frequencySlider);

    band.QSlider.setSliderStyle (Slider::Rotary);
    band.QSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    band.QSlider.setRange (QParam->getRange().getStart(), QParam->getRange().getEnd(), 1.0);
    band.QSlider.setValue (*QParam);
    band.QSlider.addListener (this);
    addAndMakeVisible (band.QSlider);

    band.gainSlider.setSliderStyle (Slider::Rotary);
    band.gainSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    band.gainSlider.setRange (gainParam->getRange().getStart(), gainParam->getRange().getEnd(), 0.1);
    band.gainSlider.setValue (*gainParam);
    band.gainSlider.addListener (this);
    addAndMakeVisible (band.gainSlider);

    bands.add (band);
    processor.setNumFilterBands (bands.size());
    resized();
}

}

void FilterComponent::removeBand()
{
auto& processor = dynamic_cast<JarEQAudioProcessor&> (getProcessor());

scss


if (bands.size() > processor.getMinNumFilterBands())
{
    bands.removeLast();
    processor.setNumFilterBands (bands.size());
    resized();
}
}

void FilterComponent::sliderValueChanged (Slider* slider)
{
auto& processor = dynamic_cast<JarEQAudioProcessor&> (getProcessor());


for (int i = 0; i < bands.size(); ++i)
{
    auto& band = bands.getReference (if (slider == &band.frequencySlider)
    {
        processor.getFilter(i)->setFrequency (*processor.getParameters().getUnchecked (i * 3));
    }
    else if (slider == &band.QSlider)
    {
        processor.getFilter(i)->setQ (*processor.getParameters().getUnchecked (i * 3 + 1));
    }
    else if (slider == &band.gainSlider)
    {
        processor.getFilter(i)->setGain (*processor.getParameters().getUnchecked (i * 3 + 2));
    }
}
}

void FilterComponent::setBands (AudioProcessorValueTreeState& parameters)
{
int numBands = processor.getNumFilterBands();

while (bands.size() < numBands)
{
    addBand();
}

while (bands.size() > numBands)
{
    removeBand();
}

for (int i = 0; i < numBands; ++i)
{
    auto frequencyParam = parameters.getUnchecked (i * 3);
    auto QParam = parameters.getUnchecked (i * 3 + 1);
    auto gainParam = parameters.getUnchecked (i * 3 + 2);

    bands[i].frequencySlider.setValue (*frequencyParam);
    bands[i].QSlider.setValue (*QParam);
    bands[i].gainSlider.setValue (*gainParam);
}

}

void FilterComponent::setLabels (String freqLabel, String QLabel, String gainLabel)
{
for (int i = 0; i < bands.size(); ++i)
{
auto& band = bands.getReference (i);
band.frequencySlider.setTooltip (freqLabel);
band.QSlider.setTooltip (QLabel);
band.gainSlider.setTooltip (gainLabel);
}
}

struct Band
{
Slider frequencySlider;
Slider QSlider;
Slider gainSlider;
};


// PluginProcessor class (continued)

void JarEQAudioProcessor::updateFilters()
{
for (int i = 0; i < filterBands.size(); ++i)
{
auto& filter = filterBands.getReference(i);
    if (filter.coefficients != *apvts.getParameters().getUnchecked(i * 3 + 1))
    {
        auto freq = *apvts.getRawParameterValue ("FREQ" + String (i + 1));
        auto Q = *apvts.getRawParameterValue ("Q" + String (i + 1));
        auto gain = *apvts.getRawParameterValue ("GAIN" + String (i + 1));

        filter.coefficients = makePeakFilterCoefficients (getSampleRate(), freq, Q, Decibels::decibelsToGain (gain));
        filter.state = filter.coefficients.state;
    }
}

}

void JarEQAudioProcessor::setNumFilterBands (int numBands)
{
while (filterBands.size() < numBands)
{
filterBands.add ({});
}

while (filterBands.size() > numBands)
{
    filterBands.removeLast();
}

apvts.state = ValueTree (Identifier ("JarEQ"), {});

for (int i = 0; i < filterBands.size(); ++i)
{
    auto& filter = filterBands.getReference(i);

    auto freqParam = std::make_unique<AudioParameterFloat> ("FREQ" + String (i + 1),
                                                             "Frequency " + String (i + 1),
                                                             NormalisableRange<float> (20.0f, 20000.0f, 0.01f, 0.3f, true),
                                                             filter.coefficients.frequency,
                                                             String(), AudioParameterFloat::genericParameter,
                                                             [](float value, int) { return String (roundToInt (value)) + " Hz"; },
                                                             [](const String& text) { return text.getFloatValue(); });

    auto QParam = std::make_unique<AudioParameterFloat> ("Q" + String (i + 1),
                                                          "Q " + String (i + 1),
                                                          NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.3f, true),
                                                          filter.coefficients.Q,
                                                          String(), AudioParameterFloat::genericParameter,
                                                          [](float value, int) { return String (value, 1); },
                                                          [](const String& text) { return text.getFloatValue(); });

    auto gainParam = std::make_unique<AudioParameterFloat> ("GAIN" + String (i + 1),
                                                             "Gain " + String (i + 1),
                                                             NormalisableRange<float> (-40.0f, 40.0f, 0.1f, 0.3f, true),
                                                             Decibels::gainToDecibels (filter.coefficients.gain),
                                                             String(), AudioParameterFloat::genericParameter,
                                                             [](float value, int) { return String (roundToInt (value)) + " dB"; },
                                                             [](const String& text) { return Decibels::decibelsToGain (text.getFloatValue(), -40.0f); });

    apvts.createAndAddParameter (freqParam.release());
    apvts.createAndAddParameter (QParam.release());
    apvts.createAndAddParameter (gainParam.release());
}
}

AudioProcessorEditor* JarEQAudioProcessor::createEditor()
{
return new JarEQAudioProcessorEditor (*this);
}

void JarEQAudioProcessor::getStateInformation (MemoryBlock& destData)
{
auto state = apvts.copyState();
std::unique_ptr<XmlElement> xml (state.createXml());
copyXmlToBinary (*xml, destData);
}

void JarEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

if (xmlState.get() != nullptr)
{
    if (xmlState->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (ValueTree::fromXml (*xmlState));
        updateFilters();
    }
}
}

void JarEQAudioProcessorEditor::paint (Graphics& g)
{
g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void JarEQAudioProcessorEditor::resized()
{
auto bounds = getLocalBounds();
// Top section
auto topSection = bounds.removeFromTop (30);
bypassButton.setBounds (topSection.removeFromLeft (100).reduced (5));
analyzerButton.setBounds (topSection.removeFromRight (100).reduced (5));
mixSlider.setBounds (topSection.removeFromRight (80).reduced (5));
globalGainSlider.setBounds (topSection.removeFromRight (80).reduced (5));

// Filter bands
auto filterSection = bounds.removeFromTop (bounds.getHeight() * 0.8f);
filterComponent.setBounds (filterSection.reduced (10));

// Presets
auto presetSection = bounds;
presetBox.setBounds (presetSection.reduced (10));
savePresetButton.setBounds (presetSection.removeFromTop (25).removeFromLeft (100).reduced (5));
loadPresetButton.setBounds (presetSection.removeFromTop (25).removeFromLeft (100).reduced (5));
}

void JarEQAudioProcessorEditor::buttonClicked (Button* button)
{
if (button == &bypassButton)
{
auto bypassed = bypassButton.getToggleState();
processor.setBypassed (bypassed);
}
else if (button == &analyzerButton)
{
analyzerButton.setToggleState (! analyzerButton.getToggleState(), NotificationType::dontSendNotification);
filterComponent.setAnalyzerEnabled (analyzerButton.getToggleState());
}
else if (button == &savePresetButton)
{
auto state = processor.apvts.copyState();
presetBox.addItem (state, "Preset " + String (presetBox.getNumItems() + 1));
}
else if (button == &loadPresetButton)
{
auto state = presetBox.getSelectedItem();

    if (state != nullptr)
    {
        processor.apvts.replaceState (*state);
    }
}

}

void JarEQAudioProcessorEditor::comboBoxChanged (ComboBox* comboBox)
{
auto state = comboBox->getSelectedItem();
if (state != nullptr)
{
    processor.apvts.replaceState (*state);
}
}

void JarEQAudioProcessorEditor::sliderValueChanged (Slider* slider)
{
if (slider == &globalGainSlider)
{
processor.setGlobalGain (globalGainSlider.getValue());
}
else if (slider == &mixSlider)
{
processor.setMix (mixSlider.getValue());
}
}

void FilterComponent::resized()
{
auto bounds = getLocalBounds();
auto bandHeight = bounds.getHeight() / filterBands.size();


for (int i = 0; i < filterBands.size(); ++i)
{
    auto& band = filterBands.getReference(i);

    band.frequencySlider.setBounds (bounds.removeFromTop (bandHeight).removeFromLeft (100).reduced (5));
    band.QSlider.setBounds (bounds.removeFromTop (bandHeight).removeFromLeft (80).reduced (5));
    band.gainSlider.setBounds (bounds.removeFromTop (bandHeight).removeFromLeft (80).reduced (5));
}
}

void FilterComponent::setAnalyzerEnabled (bool enabled)
{
analyzerEnabled = enabled;
}

void FilterComponent::paint (Graphics& g)
{
if (analyzerEnabled)
{
auto bounds = getLocalBounds();
auto fftSize = 1 << 11;
AudioBuffer<float> buffer (2, fftSize);

 if (auto* processor = dynamic_cast<JarEQAudioProcessor*> (getProcessor()))
    {
        ScopedNoDenormals noDenormals;
        ScopedLock sl (processor->getCallbackLock());
        processor->getBusBuffer(buffer, true);
        auto fftBounds = bounds.removeFromTop (100);
        Path waveformPath;
        auto window = dsp::WindowingFunction<float>::hann (fftSize);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto channelData = buffer.getReadPointer (channel);
            dsp::FFT forwardFFT (log2 (fftSize));
            forwardFFT.performFrequencyOnlyForwardTransform (channelData, window);
            Path channelPath;

            for (int i = 0; i < fftSize / 2; ++i)
            {
                auto binMagnitude = jmax (0.0f, channelData[i]);
                auto binNormalized = binMagnitude / (float) fftSize;
                auto x = mapToLog10 (i, 0, fftSize / 2, fftBounds.getX(), fftBounds.getRight());
                auto y = jmap (binNormalized, 0.0f, 1.0f, fftBounds.getBottom(), fftBounds.getY());
                auto point = Point<float> (x, y);

                if (i == 0)
                {
                    channelPath.startNewSubPath (point);
                }
                else
                {
                    channelPath.lineTo (point);
                }
            }

            waveformPath.addPath (channelPath);
        }

        g.setColour (Colours::white);
        g.strokePath (waveformPath, PathStrokeType (2.0f));
    }
}

}

void FilterComponent::sliderValueChanged (Slider* slider)
{
if (auto* processor = dynamic_cast<JarEQAudioProcessor*> (getProcessor()))
{
auto numBands = filterBands.size();

    for (int i = 0; i < numBands; ++i)
    {
        if (slider == &filterBands.getReference(i).frequencySlider)
        {
            processor->apvts.getParameter ("FREQ" + String (i + 1))->setValueNotifyingHost (filterBands.getReference(i).frequencySlider.getValue());
        }
        else if (slider == &filterBands.getReference(i).QSlider)
        {
            processor->apvts.getParameter ("Q" + String (i + 1))->setValueNotifyingHost (filterBands.getReference(i).QSlider.getValue());
        }
        else if (slider == &filterBands.getReference(i).gainSlider)
        {
            processor->apvts.getParameter ("GAIN" + String (i + 1))->setValueNotifyingHost (filterBands.getReference(i).gainSlider.getValue());
        }
    }
}

}

void FilterBand::updateFilter()
{
dsp::IIR::Coefficients<float>::Ptr coefficients;

if (filterType == FilterType::LowCut)
{
    coefficients = dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod (static_cast<int>(order), sampleRate, frequency);
}
else if (filterType == FilterType::HighCut)
{
    coefficients = dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod (static_cast<int>(order), sampleRate, frequency);
}
else if (filterType == FilterType::LowShelf)
{
    coefficients = dsp::FilterDesign<float>::designIIRLowShelf (sampleRate, frequency, static_cast<int>(order), gainInDecibels, static_cast<float>(Q));
}
else if (filterType == FilterType::HighShelf)
{
    coefficients = dsp::FilterDesign<float>::designIIRHighShelf (sampleRate, frequency, static_cast<int>(order), gainInDecibels, static_cast<float>(Q));
}
else if (filterType == FilterType::BandPass)
{
    coefficients = dsp::FilterDesign<float>::designIIRBandpass (sampleRate, frequency, static_cast<int>(order), static_cast<float>(Q));
}

if (coefficients != nullptr)
{
    leftFilter.setCoefficients (*coefficients);
    rightFilter.setCoefficients (*coefficients);
}
}

void FilterBand::reset()
{
leftFilter.reset();
rightFilter.reset();
}

void FilterBand::process (dsp::AudioBlock<float>& block)
{
auto leftBlock = block.getSingleChannelBlock (0);
auto rightBlock = block.getSingleChannelBlock (1);
leftFilter.process (leftBlock);
rightFilter.process (rightBlock);
}

void JarEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
for (auto& band : filterBands)
{
band.sampleRate = sampleRate;
band.updateFilter();
}
}

void JarEQAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
ScopedNoDenormals noDenormals;
processAudioBlock (dsp::AudioBlock<float> (buffer));
}

void JarEQAudioProcessor::processBlockBypassed (AudioBuffer<float>& buffer, MidiBuffer& midiBuffer)
{
buffer.copyFrom (1, 0, buffer.getReadPointer (0), buffer.getNumSamples());
}

void JarEQAudioProcessor::setBypassed (bool bypassed)
{
if (bypassed != isBypassed())
{
if (bypassed)
{
processBlock = &JarEQAudioProcessor::processBlockBypassed;
}
else
{
processBlock = &JarEQAudioProcessor::processBlock;
}
}
AudioProcessorParameterGroup::setParameterNotifyingHost (bypassedParameter, bypassed ? 1.0f : 0.0f);
}

void JarEQAudioProcessor::setGlobalGain (float gain)
{
globalGain = gain;
globalGainLinear = Decibels::decibelsToGain (globalGain);
}

void JarEQAudioProcessor::setMix (float mix)
{
wetMix = mix;
dryMix = 1.0f - mix;
}

void JarEQAudioProcessor::getBusBuffer (AudioBuffer<float>& buffer, bool isForInput)
{
if (isForInput)
{
auto mainInput = getBusBuffer (buffer, true, 0);
auto sidechainInput = getBusBuffer (buffer, true, 1);

    if (mainInput != nullptr && sidechainInput != nullptr)
    {
        buffer.copyFrom (0, 0, mainInput->getReadPointer (0), jmin (buffer.getNumSamples(), mainInput->getNumSamples()));
        buffer.copyFrom (1, 0, sidechainInput->getReadPointer (0), jmin (buffer.getNumSamples(), sidechainInput->getNumSamples()));
    }
}
else
{
    auto mainOutput = getBusBuffer (buffer, false, 0);
    auto sidechainOutput = getBusBuffer (buffer, false, 1);

    if (mainOutput != nullptr && sidechainOutput != nullptr)
    {
        buffer.copyFrom (0, 0, mainOutput->getWritePointer (0), jmin (buffer.getNumSamples(), mainOutput->getNumSamples()));
        buffer.copyFrom (1, 0, sidechainOutput->getWritePointer (0), jmin (buffer.getNumSamples(), sidechainOutput->getNumSamples()));
    }
}
}

void JarEQAudioProcessor::processAudioBlock (dsp::AudioBlock<float>& block)
{
if (isBypassed())
return;

auto numBands = filterBands.size();
auto numChannels = jmin (block.getNumChannels(), 2);

for (int channel = 0; channel < numChannels; ++channel)
{
    auto channelBlock = block.getChannelPointer (channel);

    for (int i = 0; i < numBands; ++i)
    {
        filterBands[i].process (dsp::AudioBlock<float> (channelBlock, 1, block.getNumSamples()));
    }
}

block.multiply (globalGainLinear);

if (wetMix > 0.0f)
{
    dsp::AudioBlock<float> inputBlock (block);
    dsp::AudioBlock<float> outputBlock (block);
    dsp::AudioBlock<float> tempBlock (outputBlock.getSubBlock (0, outputBlock.getNumChannels() - 1));
    wetProcessor->process (inputBlock, tempBlock);
    tempBlock.multiply (wetMix);
    block.multiply (dryMix);
    block.add (tempBlock);
}
}

AudioProcessorEditor* JarEQAudioProcessor::createEditor()
{
return new JarEQAudioProcessorEditor (*this);
}

void JarEQAudioProcessor::getStateInformation (MemoryBlock& destData)
{
auto state = parameters.copyState();
std::unique_ptr<XmlElement> xml (state.createXml());
copyXmlToBinary (*xml, destData);
}

void JarEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

if (xmlState != nullptr)
{
    if (xmlState->hasTagName (parameters.state.getType()))
    {
        parameters.replaceState (ValueTree::fromXml (*xmlState));
        updateBypass();
        updateGlobalGain();
        updateMix();
        updateFilterBands();
    }
}

}

void JarEQAudioProcessor::updateBypass()
{
auto bypassedValue = parameters.getParameter (bypassedParameter)->getValue();
setBypassed (bypassedValue > 0.0f);
}

void JarEQAudioProcessor::updateGlobalGain()
{
auto globalGainValue = parameters.getParameter (globalGainParameter)->getValue();
setGlobalGain (globalGainValue);
}

void JarEQAudioProcessor::updateMix()
{
auto mixValue = parameters.getParameter (mixParameter)->getValue();
setMix (mixValue);
}

void JarEQAudioProcessor::updateFilterBands()
{
for (auto& band : filterBands)
{
band.updateParameters();
band.updateFilter();
}
}

JarEQAudioProcessorEditor::JarEQAudioProcessorEditor (JarEQAudioProcessor& p)
: AudioProcessorEditor (&p), processor (p), analyzer (p.getSampleRate())
{
// Create filter band components
for (int i = 0; i < processor.getNumFilterBands(); ++i)
{
addAndMakeVisible (filterBandComponents.add (new FilterBandComponent (processor.getFilterBand (i), processor.getParameters())));
}

// Create global gain component
globalGainComponent.reset (new GlobalGainComponent (processor.getParameters()));
addAndMakeVisible (globalGainComponent.get());

// Create mix component
mixComponent.reset (new MixComponent (processor.getParameters()));
addAndMakeVisible (mixComponent.get());

// Create bypass button
bypassButton.reset (new TextButton ("Bypass"));
bypassButton->addListener (this);
addAndMakeVisible (bypassButton.get());

// Create analyzer button
analyzerButton.reset (new TextButton ("Analyzer"));
analyzerButton->addListener (this);
addAndMakeVisible (analyzerButton.get());

// Set up analyzer component
analyzer.addChangeListener (this);
addAndMakeVisible (analyzer);
setSize (800, 600);
}

void JarEQAudioProcessorEditor::paint (Graphics& g)
{
g.fillAll (Colours::black);
}

void JarEQAudioProcessorEditor::resized()
{
auto bounds = getLocalBounds();

// Layout filter band components
auto filterBandWidth = bounds.getWidth() / processor.getNumFilterBands();
auto filterBandHeight = bounds.getHeight() * 0.7f;
auto filterBandBounds = bounds.removeFromTop (filterBandHeight).reduced (20, 10);

for (int i = 0; i < processor.getNumFilterBands(); ++i)
{
    auto filterBandComponent = filterBandComponents[i];
    auto filterBandX = filterBandWidth * i;
    auto filterBandY = filterBandBounds.getY();
    auto filterBandWidthMinusMargin = filterBandWidth - 10;
    filterBandComponent->setBounds (filterBandX + 5, filterBandY, filterBandWidthMinusMargin, filterBandHeight);
}

// Layout global gain component
auto globalGainWidth = bounds.getWidth() * 0.5f;
auto globalGainHeight = bounds.getHeight() * 0.1f;
auto globalGainX = bounds.getX() + (bounds.getWidth() - globalGainWidth) * 0.5f;
auto globalGainY = bounds.getBottom() - globalGainHeight - 20;
globalGainComponent->setBounds (globalGainX, globalGainY, globalGainWidth, globalGainHeight);

// Layout mix component
auto mixWidth = globalGainWidth;
auto mixHeight = globalGainHeight;
auto mixX = globalGainX;
auto mixY = globalGainY - mixHeight - 10;
mixComponent->setBounds (mixX, mixY, mixWidth, mixHeight);

// Layout bypass button
auto buttonWidth = bounds.getWidth() * 0.1f;
auto buttonHeight = bounds.getHeight() * 0.05f;
auto buttonX = bounds.getRight() - buttonWidth - 20;
auto buttonY = bounds.getTop() + 20;
bypassButton->setBounds (buttonX, buttonY, buttonWidth, buttonHeight);

// Layout analyzer button
analyzerButton->setBounds (buttonX, buttonY + buttonHeight + 10, buttonWidth, buttonHeight);


void JarEQAudioProcessorEditor::buttonClicked (Button* button)
{
if (button == bypassButton.get())
{
auto newValue = bypassButton->getToggleState() ? 1.0f : 0.0f;
processor.getParameters().getParameter (processor.getBypassedParameter())->setValueNotifyingHost (newValue);
}
else if (button == analyzerButton.get())
{
if (analyzerButton->getToggleState())
{
// Start analyzing
analyzer.start();
}
else
{
// Stop analyzing
analyzer.stop();
}
}
}

void JarEQAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster* source)
{
if (source == &analyzer)
{
repaint();
}
}

void JarEQAudioProcessorEditor::paintOverChildren (Graphics& g)
{
if (analyzerButton->getToggleState())
{
// Draw frequency spectrum
auto bounds = getLocalBounds();
auto analyzerBounds = bounds.reduced (20, 10).removeFromBottom (bounds.getHeight() * 0.3f);
analyzer.draw (g, analyzerBounds.toFloat());
}
}

FilterBand::FilterBand (dsp::ProcessorChain<dsp::IIR::Filter<float>, dsp::Gain<float>>& chainToUse)
: processorChain (chainToUse)
{
updateParameters();
updateFilter();
}

void FilterBand::setFrequency (float newFrequency)
{
frequency = newFrequency;
updateParameters();
updateFilter();
}

void FilterBand::setGain (float newGain)
{
gain = newGain;
updateParameters();
updateFilter();
}

void FilterBand::setQ (float newQ)
{
q = newQ;
updateParameters();
updateFilter();
}

void FilterBand::updateParameters()
{
auto sampleRate = processorChain.getSampleRate();
auto coeffs = dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, frequency, q, gain);
processorChain.get<0>().setCoefficients (coeffs);
processorChain.get<1>().setGainLinear (Decibels::decibelsToGain (gain));
}

void FilterBand::updateFilter()
{
processorChain.reset();
}

// FilterBandComponent class

FilterBandComponent::FilterBandComponent (FilterBand& bandToUse, AudioProcessorValueTreeState& parametersToUse)
: filterBand (bandToUse), parameters (parametersToUse)
{
// Create frequency knob
addAndMakeVisible (frequencyKnob);
frequencyAttachment = std::make_uniqueAudioProcessorValueTreeState::SliderAttachment (parameters, filterBand.getFrequencyParameterID(), frequencyKnob);

// Create gain knob
addAndMakeVisible (gainKnob);
gainAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (parameters, filterBand.getGainParameterID(), gainKnob);

// Create Q knob
addAndMakeVisible (qKnob);
qAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (parameters, filterBand.getQParameterID(), qKnob);

// Set up label for frequency knob
addAndMakeVisible (frequencyLabel);
frequencyLabel.setText ("Freq", dontSendNotification);
frequencyLabel.attachToComponent (&frequencyKnob, false);

// Set up label for gain knob
addAndMakeVisible (gainLabel);
gainLabel.setText ("Gain", dontSendNotification);
gainLabel.attachToComponent (&gainKnob, false);

// Set up label for Q knob
addAndMakeVisible (qLabel);
qLabel.setText ("Q", dontSendNotification);
qLabel.attachToComponent (&qKnob, false);

}

void FilterBandComponent::resized()
{
auto bounds = getLocalBounds();
auto knobSize = bounds.getHeight() * 0.75f;
// Layout frequency knob
auto frequencyX = bounds.getX() + bounds.getWidth() * 0.1f;
auto frequencyY = bounds.getBottom() - knobSize;
frequencyKnob.setBounds (frequencyX, frequencyY, knobSize, knobSize);

// Layout gain knob
auto gainX = bounds.getX() + bounds.getWidth() * 0.4f;
auto gainY = bounds.getBottom() - knobSize;
gainKnob.setBounds (gainX, gainY, knobSize, knobSize);

// Layout Q knob
auto qX = bounds.getX() + bounds.getWidth() * 0.7f;
auto qY = bounds.getBottom() - knobSize;
qKnob.setBounds (qX, qY, knobSize, knobSize);

// Layout labels
auto labelHeight = bounds.getHeight() * 0.2f;
frequencyLabel.setBounds (frequencyX, frequencyY - labelHeight, knobSize, labelHeight);
gainLabel.setBounds (gainX, gainY - labelHeight, knobSize, labelHeight);
qLabel.setBounds (qX, qY - labelHeight, knobSize, labelHeight);

}

// MainComponent class

MainComponent::MainComponent (JarEQAudioProcessor& p)
: AudioProcessorEditor (&p), processor (p)
{
// Set up filter bands
for (int i = 0; i < numFilterBands; ++i)
{
auto& chain = processor.getFilterChain().get<0>().get<0>();
auto& band = chain[i];
auto* component = new FilterBandComponent (band, processor.getParameters());
filterBandComponents.add (component);
addAndMakeVisible (component);
}
// Set up global gain knob
addAndMakeVisible (globalGainKnob);
globalGainAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (processor.getParameters(), processor.getGlobalGainParameterID(), globalGainKnob);

// Set up mix knob
addAndMakeVisible (mixKnob);
mixAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (processor.getParameters(), processor.getMixParameterID(), mixKnob);

// Set up bypass button
addAndMakeVisible (bypassButton);
bypassAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.getParameters(), processor.getBypassedParameterID(), bypassButton);

// Set up analyzer button
addAndMakeVisible (analyzerButton);
analyzerButton.setClickingTogglesState (true);

// Set up analyzer
analyzer.setSamplesPerBlock (processor.getSamplesPerBlock());
analyzer.setSource (std::make_unique<AudioBufferSource> (processor.getAudioTransportSource()));

// Set up timer for analyzer updates
startTimerHz (30);
}

void MainComponent::resized()
{
auto bounds = getLocalBounds().reduced (10);

// Layout filter bands
auto filterBandBounds = bounds.removeFromTop (bounds.getHeight() * 0.6f).withTrimmedTop (10);
auto numComponents = filterBandComponents.size();
auto componentWidth = filterBandBounds.getWidth() / numComponents;
auto componentHeight = filterBandBounds.getHeight();
for (int i = 0; i < numComponents; ++i)
{
    auto* component = filterBandComponents.getUnchecked (i);
    component->setBounds (filterBandBounds.removeFromLeft (componentWidth));
    component->setHeight (componentHeight);
}

// Layout global gain knob
auto globalGainX = bounds.getX() + bounds.getWidth() * 0.1f;
auto globalGainY = bounds.getBottom() - bounds.getHeight() * 0.4f;
auto globalGainSize = bounds.getHeight() * 0.3f;
globalGainKnob.setBounds (globalGainX, globalGainY, globalGainSize, globalGainSize);

// Layout mix knob
auto mixX = bounds.getX() + bounds.getWidth() * 0.5f - globalGainSize / 2.0f;
auto mixY = bounds.getBottom() - bounds.getHeight() * 0.4f;
auto mixSize = bounds.getHeight() * 0.3f;
mixKnob.setBounds (mixX, mixY, mixSize, mixSize);

// Layout bypass button
auto bypassX = bounds.getX() + bounds.getWidth() * 0.9f - globalGainSize;

auto bypassY = bounds.getBottom() - bounds.getHeight() * 0.4f;
auto bypassSize = bounds.getHeight() * 0.3f;
bypassButton.setBounds (bypassX, bypassY, bypassSize, bypassSize);

// Layout analyzer button
auto analyzerX = bounds.getX() + bounds.getWidth() * 0.1f;
auto analyzerY = bounds.getBottom() - bounds.getHeight() * 0.8f;
auto analyzerSize = bounds.getHeight() * 0.3f;
analyzerButton.setBounds (analyzerX, analyzerY, analyzerSize, analyzerSize);

}

void MainComponent::timerCallback()
{
if (analyzerButton.getToggleState())
analyzer.update();
}

void MainComponent::paint (Graphics& g)
{
g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
// Draw analyzer
if (analyzerButton.getToggleState())
{
    auto bounds = analyzerButton.getLocalBounds().withSizeKeepingCentre (getHeight() * 0.5f, getHeight() * 0.5f);
    auto analysisBounds = bounds.reduced (getHeight() * 0.1f);
    analyzer.setBounds (analysisBounds);
    analyzer.paint (g);
}

}
FilterBandComponent::FilterBandComponent (dsp::IIR::Filter<float>& f, AudioProcessorValueTreeState& p)
: filter (f)
{
// Set up frequency knob
frequencyKnob.setSliderStyle (Slider::RotaryVerticalDrag);
frequencyKnob.setTextBoxStyle (Slider::TextBoxBelow, true, 60, 20);
frequencyKnob.setRange (20.0, 20000.0, 1.0);
frequencyKnob.setValue (filter.get<0>().getFrequency());
frequencyAttachment = std::make_uniqueAudioProcessorValueTreeState::SliderAttachment (p, "frequency" + String (filter.getIndex() + 1), frequencyKnob);
addAndMakeVisible (frequencyKnob);

// Set up gain knob
gainKnob.setSliderStyle (Slider::RotaryVerticalDrag);
gainKnob.setTextBoxStyle (Slider::TextBoxBelow, true, 60, 20);
gainKnob.setRange (-24.0, 24.0, 0.1);
gainKnob.setValue (filter.get<0>().getGain());
gainAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p, "gain" + String (filter.getIndex() + 1), gainKnob);
addAndMakeVisible (gainKnob);

// Set up Q knob
qKnob.setSliderStyle (Slider::RotaryVerticalDrag);
qKnob.setTextBoxStyle (Slider::TextBoxBelow, true, 60, 20);
qKnob.setRange (0.1, 10.0, 0.01);
qKnob.setValue (filter.get<0>().getQ());
qAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p, "q" + String (filter.getIndex() + 1), qKnob);
addAndMakeVisible (qKnob);

}

void FilterBandComponent::resized()
{
auto bounds = getLocalBounds().reduced (10);


// Layout frequency knob
auto frequencyX = bounds.getX() + bounds.getWidth() * 0.1f;
auto frequencyY = bounds.getY() + bounds.getHeight() * 0.3f;
auto knobSize = bounds.getHeight() * 0.2f;
frequencyKnob.setBounds (frequencyX, frequencyY, knobSize, knobSize);

// Layout gain knob
auto gainX = bounds.getX() + bounds.getWidth() * 0.4f;
auto gainY = bounds.getY() + bounds.getHeight() * 0.3f;
gainKnob.setBounds (gainX, gainY, knobSize, knobSize);

// Layout Q knob
auto qX = bounds.getX() + bounds.getWidth() * 0.7f;
auto qY = bounds.getY() + bounds.getHeight() * 0.3f;
qKnob.setBounds (qX, qY, knobSize, knobSize);

// Layout labels
auto labelHeight = 20;
frequencyLabel.setBounds (frequencyX, frequencyY - labelHeight, knobSize, labelHeight);
gainLabel.setBounds (gainX, gainY - labelHeight, knobSize, labelHeight);
qLabel.setBounds (qX, qY - labelHeight, knobSize, labelHeight);
}

// Analyzer class

Analyzer::Analyzer (AudioBufferQueue<float>& bufferQueueToUse)
: forwardFFT (fftOrder),
fifo (bufferQueueToUse),
spectrumData (fftSize)
{
setOpaque (false);
setBuffer (bufferQueueToUse);
}

void Analyzer::setBuffer (AudioBufferQueue<float>& bufferQueueToUse)
{
fifo.reset();
fifo.setBuffer (bufferQueueToUse);
}

void Analyzer::paint (Graphics& g)
{
auto area = getLocalBounds();
auto h = area.getHeight();
auto w = area.getWidth();

g.setColour (Colour (0xffd3d3d3));
g.fillRect (area);

g.setColour (Colour (0xff424242));
Path path;

// Calculate the bin width and scale factor
auto binWidth = (float) w / (float) fftSize / 2.0f;
auto scaleFactor = h / 100.0f;

// Calculate the FFT magnitudes
forwardFFT.performFrequencyOnlyForwardTransform (spectrumData.data());
FloatVectorOperations::abs (spectrumData);
auto numBins = spectrumData.size();

// Normalize the FFT data
spectrumData /= (float) numBins;
spectrumData[0] *= 0.5f;
spectrumData[numBins - 1] *= 0.5f;

// Plot the FFT data as a set of vertical bars
for (int i = 0; i < numBins; ++i)
{
    auto binHeight = jlimit (1.0f, 100.0f, 100.0f * log10f (1.0f + scaleFactor * spectrumData[i]));
    auto x = (float) i * binWidth * 2.0f;
    auto y = h - binHeight;
    auto rect = Rectangle<float> (x, y, binWidth, binHeight);
    path.addRectangle (rect);
}

g.fillPath (path);
}

JarEQAudioProcessorEditor::JarEQAudioProcessorEditor (JarEQAudioProcessor& p)
: AudioProcessorEditor (&p),
processor (p),
analyzer (p.getAudioBufferQueue()),
bypassButton ("Bypass"),
mixSlider (Slider::LinearHorizontal, Slider::NoTextBox),
analyzerButton ("Analyzer")
{
// Set up bypass button
bypassButton.setClickingTogglesState (true);
bypassButton.setColour (TextButton::buttonColourId, Colours::red);
bypassButton.setButtonText ("Bypass");
addAndMakeVisible (bypassButton);

// Set up mix slider
mixSlider.setRange (0.0, 1.0);
mixSlider.setValue (1.0);
mixSlider.onValueChange = [this] { processor.setMix (mixSlider.getValue()); };
addAndMakeVisible (mixSlider);

// Set up analyzer button
analyzerButton.setClickingTogglesState (true);
analyzerButton.setColour (TextButton::buttonColourId, Colours::green);
analyzerButton.setButtonText ("Analyzer");
addAndMakeVisible (analyzerButton);

// Add filter band components
auto& params = processor.getParameters();
for (int i = 0; i < maxNumFilters; ++i)
{
    auto* f = processor.getFilter(i);
    if (f != nullptr)
    {
        auto* component = new FilterBandComponent (*f, params);
        addAndMakeVisible (component);
        filterComponents.add (component);
    }
}

// Set up analyzer component
addAndMakeVisible (analyzer);

// Set up the timer to update the analyzer component
startTimerHz (30);

}

void JarEQAudioProcessorEditor::resized()
{
auto bounds = getLocalBounds();

// Layout bypass button
auto bypassX = bounds.getX() + bounds.getWidth() * 0.7f;
auto bypassY = bounds.getBottom() - bounds.getHeight() * 0.4f;
auto bypassSize = bounds.getHeight() * 0.3f;
bypassButton.setBounds (bypassX, bypassY, bypassSize, bypassSize);

// Layout mix slider
auto mixX = bounds.getX() + bounds.getWidth() * 0.1f;
auto mixY = bounds.getBottom() - bounds.getHeight() * 0.3f;
auto mixWidth = bounds.getWidth() * 0.8f;
mixSlider.setBounds (mixX, mixY, mixWidth, 20);

// Layout filter band components
auto numFilters = filterComponents.size();
auto componentHeight = bounds.getHeight() / (numFilters + 1);
for (int i = 0; i < numFilters; ++i)
{
    auto* component = filterComponents[i];
    auto componentBounds = bounds.removeFromTop (componentHeight);
    component->setBounds (componentBounds);
}

// Layout analyzer component
auto analyzerBounds = bounds;
analyzer.setBounds (analyzerBounds);

// Layout analyzer button
auto analyzerX = bounds.getX() + bounds.getWidth() * 0.1f;
auto analyzerY = bounds.getBottom() - bounds.getHeight() * 0.8f;
auto analyzerSize = bounds.getHeight() * 0.3f;
analyzerButton.setBounds (analyzerX, analyzerY, analyzerSize, analyzerSize);

}

void JarEQAudioProcessorEditor::timerCallback()
{
if (analyzerButton.getToggleState())
analyzer.update();
}

// FilterBandComponent class

FilterBandComponent::FilterBandComponent (Filter& f, AudioProcessorValueTreeState& params)
: filter (f)
{
// Set up frequency slider
frequencySlider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
frequencySlider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
frequencySlider.setRange (filter.getMinFrequency(), filter.getMaxFrequency());
frequencySlider.setValue (filter.getFrequency());
frequencySlider.onValueChange = [this] { filter.setFrequency (frequencySlider.getValue()); };
addAndMakeVisible (frequencySlider);
frequencyAttachment = std::make_uniqueAudioProcessorValueTreeState::SliderAttachment (params, filter.getFrequencyParamName(), frequencySlider);

// Set up gain slider
gainSlider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
gainSlider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
gainSlider.setRange (filter.getMinGain(), filter.getMaxGain());
gainSlider.setValue (filter.getGain());
gainSlider.onValueChange = [this] { filter.setGain (gainSlider.getValue()); };
addAndMakeVisible (gainSlider);
gainAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (params, filter.getGainParamName(), gainSlider);

// Set up Q slider
qSlider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
qSlider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
qSlider.setRange (filter.getMinQ(), filter.getMaxQ());
qSlider.setValue (filter.getQ());
qSlider.onValueChange = [this] { filter.setQ (qSlider.getValue()); };
addAndMakeVisible (qSlider);
qAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (params, filter.getQParamName(), qSlider);

// Set up filter type menu
filterTypeMenu.addItem ("None", 1);
filterTypeMenu.addItem ("Low-pass", 2);
filterTypeMenu.addItem ("High-pass", 3);
filterTypeMenu.addItem ("Band-pass", 4);
filterTypeMenu.addItem ("Notch", 5);
filterTypeMenu.setSelectedId (filter.getType() + 1, NotificationType::dontSendNotification);
filterTypeMenu.onChange = [this] { filter.setType (filterTypeMenu.getSelectedId() - 1); };
addAndMakeVisible (filterTypeMenu);
filterTypeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (params, filter.getTypeParamName(), filterTypeMenu);

// Set up filter bypass button
filterBypassButton.setClickingTogglesState (true);
filterBypassButton.setColour (TextButton::buttonColourId, Colours::red);
filterBypassButton.setButtonText ("Bypass");
addAndMakeVisible (filterBypassButton);
filterBypassAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (params, filter.getBypassParamName(), filterBypassButton);


}

void FilterBandComponent::paint (Graphics& g)
{
auto area = getLocalBounds().toFloat();
auto margin = area.getWidth() * 0.05f;

g.setColour (Colours::white);
g.fillRoundedRectangle (area, margin);
g.setColour (Colours::grey);
g.drawRoundedRectangle (area, margin, 1.0f);

}

void FilterBandComponent::resized()
{
auto bounds = getLocalBounds().reduced (5);
auto rowHeight = bounds.getHeight()

// Layout frequency slider
auto frequencySliderBounds = bounds.removeFromTop (rowHeight);
frequencySlider.setBounds (frequencySliderBounds.removeFromLeft (100).reduced (10));

// Layout gain slider
auto gainSliderBounds = bounds.removeFromTop (rowHeight);
gainSlider.setBounds (gainSliderBounds.removeFromLeft (100).reduced (10));

// Layout Q slider
auto qSliderBounds = bounds.removeFromTop (rowHeight);
qSlider.setBounds (qSliderBounds.removeFromLeft (100).reduced (10));

// Layout filter type menu
auto filterTypeMenuBounds = bounds.removeFromTop (rowHeight);
filterTypeMenu.setBounds (filterTypeMenuBounds.removeFromLeft (100).reduced (10));

// Layout filter bypass button
auto filterBypassButtonBounds = bounds.removeFromTop (rowHeight);
filterBypassButton.setBounds (filterBypassButtonBounds.removeFromLeft (100).reduced (10));
}

// EQComponent class

EQComponent::EQComponent (AudioProcessorValueTreeState& params)
: eqProcessor (getSampleRate()),
frequencyRangeLow (20.0f, 20000.0f, 20.0f, 2.0f),
frequencyRangeMid (200.0f, 8000.0f, 200.0f, 1.0f),
frequencyRangeHigh (2000.0f, 20000.0f, 2000.0f, 2.0f)
{
// Set up global gain slider
globalGainSlider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
globalGainSlider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
globalGainSlider.setRange (eqProcessor.getMinGain(), eqProcessor.getMaxGain());
globalGainSlider.setValue (eqProcessor.getGain());
globalGainSlider.onValueChange = [this] { eqProcessor.setGain (globalGainSlider.getValue()); };
addAndMakeVisible (globalGainSlider);
globalGainAttachment = std::make_uniqueAudioProcessorValueTreeState::SliderAttachment (params, "global_gain", globalGainSlider);

// Set up mix slider
mixSlider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
mixSlider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
mixSlider.setRange (0.0f, 1.0f);
mixSlider.setValue (1.0f);
mixSlider.onValueChange = [this] { };
addAndMakeVisible (mixSlider);

// Set up bypass button
bypassButton.setClickingTogglesState (true);
bypassButton.setColour (TextButton::buttonColourId, Colours::red);
bypassButton.setButtonText ("Bypass");
addAndMakeVisible (bypassButton);
bypassButtonAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (params, "bypass", bypassButton);

// Set up analyzer button
analyzerButton.setClickingTogglesState (true);
analyzerButton.setButtonText ("Analyzer");
addAndMakeVisible (analyzerButton);
analyzerButtonAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (params, "analyzer", analyzerButton);

// Set up filter band components
addFilterBand (eqProcessor.getLowBand(), frequencyRangeLow, params);
addFilterBand (eqProcessor.getMidBand(), frequencyRangeMid, params);
addFilterBand (eqProcessor.getHighBand(), frequencyRangeHigh, params);

// Set up filter chain
eqProcessor.updateFilterChain();
}

void EQComponent::addFilterBand (Filter& f, FrequencyRange& range, AudioProcessorValueTreeState& params)
{
auto bandComponent = std::make_unique<FilterBandComponent> (f, params);
addAndMakeVisible (bandComponent.get());
filterBandComponents.push_back (std::move (bandComponent));
auto rangeComponent = std::make_unique<FrequencyRangeComponent> (f, range);
addAndMakeVisible (rangeComponent.get());
frequencyRangeComponents.push_back (std::move (rangeComponent));
}

void EQComponent::paint (Graphics& g)
{
auto area = getLocalBounds().toFloat();
auto margin = area.getWidth() * 0.05f;
g.setColour (Colours::black);
g.fillRect (area);
g.setColour (Colours::grey);
g.drawRoundedRectangle (area, margin, 1.0f);
}

void EQComponent::resized()
{
    // Layout global gain slider
auto globalGainSliderBounds = getLocalBounds().removeFromTop (rowHeight).reduced (10, 0);
globalGainSlider.setBounds (globalGainSliderBounds.removeFromLeft (100));

// Layout mix slider
auto mixSliderBounds = getLocalBounds().removeFromBottom (rowHeight).reduced (0, 10);
mixSlider.setBounds (mixSliderBounds.removeFromLeft (100));

// Layout bypass button
auto bypassButtonBounds = getLocalBounds().removeFromBottom (rowHeight).reduced (10, 0);
bypassButton.setBounds (bypassButtonBounds.removeFromLeft (100));

// Layout analyzer button
auto analyzerButtonBounds = getLocalBounds().removeFromBottom (rowHeight).reduced (10, 0);
analyzerButton.setBounds (analyzerButtonBounds.removeFromLeft (100));

// Layout filter band components
auto filterBandComponentBounds = getLocalBounds().removeFromTop (rowHeight * 4).removeFromLeft (getWidth() * 0.5f).reduced (10, 0);
auto frequencyRangeComponentBounds = filterBandComponentBounds.removeFromBottom (rowHeight);
for (int i = 0; i < filterBandComponents.size(); ++i)
{
filterBandComponents[i]->setBounds (filterBandComponentBounds.removeFromLeft (getWidth() * 0.25f));
frequencyRangeComponents[i]->setBounds (frequencyRangeComponentBounds.removeFromLeft (getWidth() * 0.25f));
filterBandComponentBounds.removeFromLeft (10);
frequencyRangeComponentBounds.removeFromLeft (10);
}
// FilterBandComponent class

FilterBandComponent::FilterBandComponent (Filter& f, AudioProcessorValueTreeState& params)
: filter (f),
frequencySlider (Slider::SliderStyle::RotaryHorizontalVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox),
gainSlider (Slider::SliderStyle::RotaryHorizontalVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox),
qSlider (Slider::SliderStyle::RotaryHorizontalVerticalDrag, Slider::TextEntryBoxPosition::NoTextBox),
filterTypeMenu ("", {"Lowpass", "Highpass", "Bandpass", "Bandstop"})
{
// Set up frequency slider
frequencySlider.setRange (filter.getMinFrequency(), filter.getMaxFrequency());
frequencySlider.setValue (filter.getFrequency());
frequencySlider.onValueChange = [this] { filter.setFrequency (frequencySlider.getValue()); };
addAndMakeVisible (frequencySlider);
frequencyAttachment = std::make_uniqueAudioProcessorValueTreeState::SliderAttachment (params, filter.getFrequencyParamID(), frequencySlider);

// Set up gain slider
gainSlider.setRange (filter.getMinGain(), filter.getMaxGain());
gainSlider.setValue (filter.getGain());
gainSlider.onValueChange = [this] { filter.setGain (gainSlider.getValue()); };
addAndMakeVisible (gainSlider);
gainAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (params, filter.getGainParamID(), gainSlider);

// Set up Q slider
qSlider.setRange (filter.getMinQ(), filter.getMaxQ());
qSlider.setValue (filter.getQ());
qSlider.onValueChange = [this] { filter.setQ (qSlider.getValue()); };
addAndMakeVisible (qSlider);
qAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (params, filter.getQParamID(), qSlider);

// Set up filter type menu
filterTypeMenu.setSelectedId (filter.getFilterType(), dontSendNotification);
filterTypeMenu.onChange = [this] { filter.setFilterType (filterTypeMenu.getSelectedId()); };
addAndMakeVisible (filterTypeMenu);
filterTypeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (params, filter.getFilterTypeParamID(), filterTypeMenu);

}

void FilterBandComponent::paint (Graphics& g)
{
auto area = getLocalBounds().toFloat().reduced (10);
g.setColour (Colours::grey);
g.drawRoundedRectangle (area, 5.0f, 1.0f);

auto titleArea = area.removeFromTop (20.0f);
g.setFont (Font (18.0f, Font::bold));
g.drawText (filter.getTitle(), titleArea, Justification::centred, true);

g.setFont (Font (14.0f));
g.setColour (Colours::white);

auto frequencyArea = area.removeFromLeft (area.getWidth() * 0.33f).reduced (5.0f, 0.0f);
g.drawText ("Frequency", frequencyArea, Justification::centredLeft, true);
frequencySlider.setBounds (frequencyArea.removeFromTop (20.0f).reduced (0, 5));

auto gainArea = area.removeFromLeft (area.getWidth() * 0.33f).reduced (5.0f, 0.0f);
g.drawText ("Gain", gainArea, Justification::centredLeft, true);
gainSlider.setBounds (gainArea.removeFromTop (20.0f).reduced (0, 5));

auto qArea = area.removeFromLeft (area.getWidth() * 0.33f).reduced (5.0f, 0.0f);
g.drawText ("Q", qArea, Justification::centredLeft, true);
qSlider.setBounds (qArea.removeFromTop (20.0f).reduced (0, 5));
auto filterTypeArea = area.removeFromLeft (area.getWidth() * 0.5f).reduced (5.0f, 0.0f);
g.drawText ("Type", filterTypeArea, Justification::centredLeft, true);
filterTypeMenu.setBounds (filterTypeArea.removeFromTop (20.0f).reduced (0, 5));
}

void FilterBandComponent::resized()
{
auto bounds = getLocalBounds().toFloat();
auto titleArea = bounds.removeFromTop (20.0f);
auto slidersArea = bounds.removeFromTop (bounds.getHeight() * 0.75f);
auto frequencyArea = slidersArea.removeFromLeft (slidersArea.getWidth() * 0.33f);
auto gainArea = slidersArea.removeFromLeft (slidersArea.getWidth() * 0.33f);
auto qArea = slidersArea.removeFromLeft (slidersArea.getWidth() * 0.33f);
}
// FilterBand class

void FilterBand::prepare (const dsp::ProcessSpec& spec)
{
filter.prepare (spec);
filter.reset();
}

void FilterBand::process (const dsp::ProcessContextReplacing<float>& context)
{
filter.process (context);
}

void FilterBand::setFilterType (int newType)
{
filterType = newType;
switch (newType)
{
    case Lowpass:
        filter.setType (dsp::StateVariableTPTFilterType::lowpass);
        break;

    case Highpass:
        filter.setType (dsp::StateVariableTPTFilterType::highpass);
        break;

    case Bandpass:
        filter.setType (dsp::StateVariableTPTFilterType::bandpass);
        break;

    case Bandstop:
        filter.setType (dsp::StateVariableTPTFilterType::bandstop);
        break;
}
}

int FilterBand::getFilterType() const
{
return filterType;
}

void FilterBand::setFrequency (float newFrequency)
{
frequency = newFrequency;
filter.setCutoffFrequency (newFrequency);
}

float FilterBand::getFrequency() const
{
return frequency;
}

void FilterBand::setGain (float newGain)
{
gain = newGain;
filter.setGain (newGain);
}

float FilterBand::getGain() const
{
return gain;
}

void FilterBand::setQ (float newQ)
{
q = newQ;
filter.setQ (newQ);
}

float FilterBand::getQ() const
{
return q;
}

void FilterBand::setParamsFromTree (const AudioProcessorValueTreeState& tree)
{
setFilterType (tree.getParameter (getFilterTypeParamID())->getValue());
setFrequency (tree.getParameter (getFrequencyParamID())->getValue());
setGain (tree.getParameter (getGainParamID())->getValue());
setQ (tree.getParameter (getQParamID())->getValue());
}

String FilterBand::getTitle() const
{
return "Band " + String (bandIndex + 1);
}

String FilterBand::getFilterTypeParamID() const
{
return "band" + String (bandIndex + 1) + "_type";
}

String FilterBand::getFrequencyParamID() const
{
return "band" + String (bandIndex + 1) + "_frequency";
}

String FilterBand::getGainParamID() const
{
return "band" + String (bandIndex + 1) + "_gain";
}

String FilterBand::getQParamID() const
{
return "band" + String (bandIndex + 1) + "_q";
}

float FilterBand::getMinFrequency() const
{
return 20.0f;
}

float FilterBand::getMaxFrequency() const
{
return 20000.0f;
}

float FilterBand::getMinGain() const
{
return -24.0f;
}

float FilterBand::getMaxGain() const
{
return 24.0f;
}

float FilterBand::getMinQ() const
{
return 0.1f;
}

float FilterBand::getMaxQ() const
{
return 10.0f;
}

// Analyzer class

void Analyzer::paint (Graphics& g)
{
g.fillAll (Colours::black);
if (shouldShowAnalyzer)
{
    g.setColour (Colours::white);

    Path p;
    const float halfHeight = getHeight() * 0.5f;
    const float pixelWidth = static_cast<float> (getWidth()) / scopeSize;

    p.startNewSubPath (0, halfHeight);

    for (int i = 0; i < scopeSize; ++i)
    {
        float level = jmap (scopeData[i], 0.0f, 1.0f, 0.0f, getHeight() * 0.5f);
        p.lineTo (i * pixelWidth, halfHeight - level);
    }

    for (int i = scopeSize - 1; i >= 0; --i)
    {
        float level = jmap (scopeData[i], 0.0f, 1.0f, 0.0f, getHeight() * 0.5f);
        p.lineTo (i * pixelWidth, halfHeight + level);
    }

    g.strokePath (p, PathStrokeType (2.0f));
}
}

void Analyzer::process (const AudioBuffer<float>& buffer)
{
const int numSamples = buffer.getNumSamples();
const int numChannels = buffer.getNumChannels();
const int scopeSize = getScopeSize();

for (int channel = 0; channel < numChannels; ++channel)
{
    auto* channelData = buffer.getReadPointer (channel);

    for (int i = 0; i < numSamples; ++i)
    {
        scopeData[scopeIndex] = channelData[i];
        scopeIndex = (scopeIndex + 1) % scopeSize;
    }
}

repaint();



}

void Analyzer::setScopeSize (int newSize)
{
scopeSize = newSize;
scopeData.resize (scopeSize, 0.0f);
scopeIndex = 0;
}

int Analyzer::getScopeSize() const
{
return scopeSize;
}

void Analyzer::setShouldShowAnalyzer (bool shouldShow)
{
shouldShowAnalyzer = shouldShow;
repaint();
}

// JarEQAudioProcessorEditor class

JarEQAudioProcessorEditor::JarEQAudioProcessorEditor (JarEQAudioProcessor& p)
: AudioProcessorEditor (&p), processor (p)
{
// Set editor size
setSize (800, 600);

// Add filter band controls
for (int i = 0; i < processor.getNumBands(); ++i)
{
    FilterBandComponent* bandComponent = new FilterBandComponent (processor.getBand (i));
    bandComponents.add (bandComponent);
    addAndMakeVisible (bandComponent);
}

// Add global gain control
globalGainSlider.setSliderStyle (Slider::RotaryVerticalDrag);
globalGainSlider.setTextBoxStyle (Slider::TextBoxBelow, true, 80, 20);
globalGainSlider.setRange (-24.0f, 24.0f, 0.1f);
globalGainSlider.setValue (0.0f);
globalGainSlider.addListener (this);
addAndMakeVisible (globalGainSlider);

globalGainLabel.setText ("Global Gain", NotificationType::dontSendNotification);
globalGainLabel.attachToComponent (&globalGainSlider, false);
globalGainLabel.setJustificationType (Justification::centred);
globalGainLabel.setFont (Font (15.0f));
addAndMakeVisible (globalGainLabel);

// Add mix control
mixSlider.setSliderStyle (Slider::RotaryVerticalDrag);
mixSlider.setTextBoxStyle (Slider::TextBoxBelow, true, 80, 20);
mixSlider.setRange (0.0f, 1.0f, 0.01f);
mixSlider.setValue (1.0f);
mixSlider.addListener (this);
addAndMakeVisible (mixSlider);

mixLabel.setText ("Mix", NotificationType::dontSendNotification);
mixLabel.attachToComponent (&mixSlider, false);
mixLabel.setJustificationType (Justification::centred);
mixLabel.setFont (Font (15.0f));
addAndMakeVisible (mixLabel);

// Add bypass button
bypassButton.setButtonText ("Bypass");
bypassButton.addListener (this);
addAndMakeVisible (bypassButton);

// Add analyzer button
analyzerButton.setButtonText ("Analyzer");
analyzerButton.addListener (this);
addAndMakeVisible (analyzerButton);
}

void JarEQAudioProcessorEditor::paint (Graphics& g)
{
g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void JarEQAudioProcessorEditor::resized()
{
const int margin = 20;
const int bandHeight = 150;
const int bandSpacing = 20;
const int bandWidth = getWidth() - margin * 2;
const int bandY = margin;

Rectangle<int> globalGainBounds (margin, bandY + bandHeight + bandSpacing, 100, 100);
Rectangle<int> mixBounds (getWidth() - margin - 100, bandY + bandHeight + bandSpacing, 100, 100);
Rectangle<int> bypassBounds (getWidth() - margin - 100, getHeight() - margin - 30, 100, 30);
Rectangle<int> analyzerBounds (margin, getHeight() - margin - 30, 100, 30);

globalGainSlider.setBounds (globalGainBounds.reduced (10));
mixSlider.setBounds (mixBounds.reduced (10));
bypassButton.setBounds (bypassBounds.reduced (10));
analyzerButton.setBounds (analyzerBounds.reduced (10));

for (int i = 0; i < bandComponents.size(); ++i){
    int x = margin;
    int y = bandY + i * (bandHeight + bandSpacing);
    bandComponents[i]->setBounds (x, y, bandWidth, bandHeight);
}
}

void JarEQAudioProcessorEditor::sliderValueChanged (Slider* slider)
{
if (slider == &globalGainSlider)
{
processor.setGlobalGain (globalGainSlider.getValue());
}
else if (slider == &mixSlider)
{
processor.setMix (mixSlider.getValue());
}
}

void JarEQAudioProcessorEditor::buttonClicked (Button* button)
{
if (button == &bypassButton)
{
processor.setBypass (!processor.isBypassed());
bypassButton.setButtonText (processor.isBypassed() ? "Unbypass" : "Bypass");
}
else if (button == &analyzerButton)
{
analyzer.setShouldShowAnalyzer (!analyzer.shouldShowAnalyzer());
analyzerButton.setButtonText (analyzer.shouldShowAnalyzer() ? "Hide Analyzer" : "Analyzer");
}
}
// FilterBandComponent class

FilterBandComponent::FilterBandComponent (AudioProcessorValueTreeState::ParameterLayout& layout)
: layout (layout)
{
// Add frequency, gain, and Q controls
addAndMakeVisible (frequencySlider);
frequencyAttachment = std::make_uniqueAudioProcessorValueTreeState::SliderAttachment (layout.getParameter ("frequency"), frequencySlider);
frequencySlider.setSliderStyle (Slider::RotaryVerticalDrag);
frequencySlider.setTextBoxStyle (Slider::TextBoxBelow, true, 80, 20);
frequencySlider.setRange (20.0f, 20000.0f, 0.1f);
frequencySlider.setValue (1000.0f);
frequencySlider.addListener (this);
addAndMakeVisible (gainSlider);
gainAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (layout.getParameter ("gain"), gainSlider);
gainSlider.setSliderStyle (Slider::RotaryVerticalDrag);
gainSlider.setTextBoxStyle (Slider::TextBoxBelow, true, 80, 20);
gainSlider.setRange (-24.0f, 24.0f, 0.1f);
gainSlider.setValue (0.0f);
gainSlider.addListener (this);

addAndMakeVisible (qSlider);
qAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (layout.getParameter ("q"), qSlider);
qSlider.setSliderStyle (Slider::RotaryVerticalDrag);
qSlider.setTextBoxStyle (Slider::TextBoxBelow, true, 80, 20);
qSlider.setRange (0.1f, 10.0f, 0.01f);
qSlider.setValue (1.0f);
qSlider.addListener (this);

// Add label
label.setText ("Filter Band", NotificationType::dontSendNotification);
label.attachToComponent (&frequencySlider, false);
label.setJustificationType (Justification::centred);
label.setFont (Font (15.0f));
addAndMakeVisible (label);
}

void FilterBandComponent::paint (Graphics& g)
{
g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void FilterBandComponent::resized()
{
const int margin = 20;
const int labelHeight = 30;
const int sliderHeight = 100;
const int sliderWidth = getWidth() - margin * 2;
const int labelY = margin;
const int frequencySliderY = labelY + labelHeight + margin;
const int gainSliderY = frequencySliderY + sliderHeight + margin;
const int qSliderY = gainSliderY + sliderHeight + margin;

label.setBounds (0, labelY, getWidth(), labelHeight);
frequencySlider.setBounds (margin, frequencySliderY, sliderWidth, sliderHeight);
gainSlider.setBounds (margin, gainSliderY, sliderWidth, sliderHeight);
qSlider.setBounds (margin, qSliderY, sliderWidth, sliderHeight);
}

void FilterBandComponent::sliderValueChanged (Slider* slider)
{
repaint();
}

// JarEQAudioProcessor class

JarEQAudioProcessor::JarEQAudioProcessor()
: AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true).withOutput ("Output", AudioChannelSet::stereo(), true))
, parameters (*this, nullptr, "Parameters", createParameterLayout())
, globalGain (1.0f)
, mix (1.0f)
, bypassed (false)
{
for (int i = 0; i < maxNumBands; ++i)
{
filterBands.add (std::make_unique<IIRFilter> (designIIRFilter()));
}
updateFilterCoefficients();

}

void JarEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
for (int i = 0; i < filterBands.size(); ++i)
{
filterBands[i]->reset();
}

dsp::ProcessSpec spec { sampleRate, static_cast<uint32> (samplesPerBlock), getTotalNumInputChannels() };
stateVariableFilter.reset();
stateVariableFilter.prepare (spec);

}

void JarEQAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
if (bypassed)
return;

ScopedNoDenormals noDenormals;

int numSamples = buffer.getNumSamples();

// Process input buffer with each filter band
for (int i = 0; i < filterBands.size(); ++i)
{
    const float gain = parameters.getParameter ("band_" + String (i) + "_gain")->getValue();
    const float frequency = parameters.getParameter ("band_" + String (i) + "_frequency")->getValue();
    const float q = parameters.getParameter ("band_" + String (i) + "_q")->getValue();
    filterBands[i]->setCoefficients (designIIRFilter (frequency, q, gain));

    dsp::AudioBlock<float> block (buffer);
    dsp::ProcessContextReplacing<float> context (block);
    filterBands[i]->process (context);
}

// Apply state variable filter
const float highpassFreq = parameters.getParameter ("highpass_frequency")->getValue();
const float lowpassFreq = parameters.getParameter ("lowpass_frequency")->getValue();
stateVariableFilter.state->setType (dsp::StateVariableFilter::Parameters<float>::Type::bandPass);
stateVariableFilter.state->setCutParams (highpassFreq, lowpassFreq);
stateVariableFilter.process (dsp::ProcessContextReplacing<float> (buffer));

// Apply global gain
buffer.applyGain (globalGain);

// Mix with original signal
if (mix < 1.0f)
{
    AudioBuffer<float> dryBuffer (buffer);
    dryBuffer.applyGain (1.0f - mix);
    buffer.applyGain (mix);
    buffer.addFrom (0, 0, dryBuffer, 0, 0, numSamples);
    buffer.addFrom (1, 0, dryBuffer, 1, 0, numSamples);
}
}

void JarEQAudioProcessor::updateFilterCoefficients()
{
// Update state variable filter coefficients
const float sampleRate = getSampleRate();
const float highpassFreq = parameters.getParameter ("highpass_frequency")->getValue();
const float lowpassFreq = parameters.getParameter ("lowpass_frequency")->getValue();
const float q = 0.7071f;
const float gain = 0.0f;
stateVariableFilter.state->setType (dsp::StateVariableFilter::Parameters<float>::Type::bandPass);
stateVariableFilter.state->setCutParams (highpassFreq, lowpassFreq, q);
stateVariableFilter.state->setGainParams (gain);

}

AudioProcessorValueTreeState::ParameterLayout JarEQAudioProcessor::createParameterLayout()
{
AudioProcessorValueTreeState::ParameterLayout layout;

// Add global gain parameter
layout.add (std::make_unique<AudioParameterFloat> ("global_gain", "Global Gain", 0.0f, 2.0f, 1.0f));

// Add mix parameter
layout.add (std::make_unique<AudioParameterFloat> ("mix", "Mix", 0.0f, 1.0f, 1.0f));

// Add bypass parameter
layout.add (std::make_unique<AudioParameterBool> ("bypass", "Bypass", false));

// Add analyzer parameter
layout.add (std::make_unique<AudioParameterBool> ("analyzer", "Analyzer", false));

// Add highpass frequency parameter
layout.add (std::make_unique<AudioParameterFloat> ("highpass_frequency", "Highpass Frequency", AudioProcessorParameter::nonLinearWithSkew, 20.0f, 20000.0f, 20.0f, [](float value, float skew) { return std::pow (value / 20000.0f, skew) * 20000.0f; }, [](float value, float skew) { return std::pow (value / 20000.0f, 1.0f / skew) * 20000.0f; }, "Hz"));

// Add lowpass frequency parameter
layout.add (std::make_unique<AudioParameterFloat> ("lowpass_frequency", "Lowpass Frequency", AudioProcessorParameter::nonLinearWithSkew, 20.0f, 20000.0f, 20000.0f, [](float value, float skew) { return std::pow (value / 20000.0f, skew) * 20000.0f; }, [](float value, float skew) { return std::pow (value / 20000.0f, 1.0f / skew) * 20000.0f; }, "Hz"));

// Add filter band parameters
for (int i = 0; i < maxNumBands; ++i)
{
    // Add band frequency parameter
    const float defaultFrequency = 200.0f * std::pow (2.0f, i);
    layout.add (std::make_unique<AudioParameterFloat> ("band_" + String (i) + "_frequency", "Band " + String (i + 1) + " Frequency", AudioProcessorParameter::nonLinearWithSkew, 20.0f, 20000.0f, defaultFrequency, [](float value, float skew) { return std::pow (value / 20000.0f, skew) * 20000.0f; }, [](float value, float skew) { return std::pow (value / 20000.0f, 1.0f / skew) * 20000.0f; }, "Hz"));

    // Add band gain parameter
    layout.add (std::make_unique<AudioParameterFloat> ("band_" + String (i) + "_gain", "Band " + String (i + 1) + " Gain", -24.0f, 24.0f, 0.0f, String(), AudioParameter::genericParameter, [](float value, int) { return String (value, 1) + " dB"; }, [](const String& text) { return text.getFloatValue(); }));

    // Add band
    // Add band Q parameter
    layout.add (std::make_unique<AudioParameterFloat> ("band_" + String (i) + "_q", "Band " + String (i + 1) + " Q", 0.1f, 10.0f, 1.0f, String(), AudioParameter::genericParameter, [](float value, int) { return String (value, 2); }, [](const String& text) { return text.getFloatValue(); }));
}

return layout;

}

// This creates new instances of the plugin
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
return new JarEQAudioProcessor();
}