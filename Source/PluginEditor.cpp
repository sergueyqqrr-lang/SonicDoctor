#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace Palette
{
    const juce::Colour bg      (0xff14181d);
    const juce::Colour panel   (0xff1e252c);
    const juce::Colour cyan    (0xff4fd6c8);
    const juce::Colour orange  (0xffe8a33d);
    const juce::Colour red     (0xffe0524a);
    const juce::Colour text    (0xffe4ecef);
}

// ============================================================================
// DoctorSpectrum
// ============================================================================
DoctorSpectrum::DoctorSpectrum (SonicDoctorAudioProcessor& p) : proc (p) { startTimerHz (30); }

float DoctorSpectrum::freqToX (float freqHz, float width) const
{
    constexpr float minFreq = 20.0f, maxFreq = 20000.0f;
    auto logMin = std::log10 (minFreq);
    auto logMax = std::log10 (maxFreq);
    auto logF = std::log10 (juce::jlimit (minFreq, maxFreq, freqHz));
    return width * (logF - logMin) / (logMax - logMin);
}

void DoctorSpectrum::timerCallback()
{
    if (proc.isNextFFTBlockReady())
        proc.computeNextSpectrumFrame();

    const auto& target = proc.getScopeData();
    for (size_t i = 0; i < smoothedData.size(); ++i)
    {
        float coeff = (target[i] > smoothedData[i]) ? attackCoeff : releaseCoeff;
        smoothedData[i] = coeff * smoothedData[i] + (1.0f - coeff) * target[i];
    }
    repaint();
}

void DoctorSpectrum::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    ProLookAndFeel::drawInsetScreen (g, bounds, 8.0f);
    bounds = bounds.reduced (3.0f);

    // Curva del espectro en vivo
    juce::Path curve;
    bool started = false;
    auto sr = 44100.0; // solo para el eje; el mapeo real usa fftSize/sampleRate internamente en el procesador
    for (int i = 1; i < (int) smoothedData.size(); ++i)
    {
        auto freq = (float) i * (float) sr / (float) SonicDoctorAudioProcessor::fftSize;
        if (freq < 20.0f || freq > 20000.0f) continue;
        auto x = bounds.getX() + freqToX (freq, bounds.getWidth());
        auto y = bounds.getBottom() - smoothedData[(size_t) i] * bounds.getHeight();
        if (! started) { curve.startNewSubPath (x, y); started = true; }
        else curve.lineTo (x, y);
    }
    g.setColour (Palette::cyan.withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Palette::cyan);
    g.strokePath (curve, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Marcadores de resonancias detectadas (líneas naranjas verticales + etiqueta)
    auto profile = proc.getLatestProfile();
    for (int i = 0; i < profile.numResonancesFound; ++i)
    {
        const auto& res = profile.resonances[(size_t) i];
        auto x = bounds.getX() + freqToX (res.frequencyHz, bounds.getWidth());
        g.setColour (Palette::orange.withAlpha (0.6f));
        g.drawVerticalLine ((int) x, bounds.getY(), bounds.getBottom());
        g.setFont (10.0f);
        g.drawText (juce::String (res.frequencyHz, 0) + "Hz", (int) x - 20, (int) bounds.getY() + 2, 60, 12, juce::Justification::left);
    }

    // Etiquetas de frecuencia
    g.setColour (Palette::text.withAlpha (0.35f));
    g.setFont (10.0f);
    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        auto x = bounds.getX() + freqToX (f, bounds.getWidth());
        auto label = f < 1000.0f ? juce::String ((int) f) : juce::String ((int) (f / 1000.0f)) + "k";
        g.drawText (label, (int) x - 12, (int) bounds.getBottom() - 12, 30, 12, juce::Justification::left);
    }
}

// ============================================================================
// MeterRow
// ============================================================================
void MeterRow::drawMeterCell (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& label,
                               const juce::String& value, juce::Colour accent) const
{
    ProLookAndFeel::drawRaisedPanel (g, area.reduced (3.0f), 6.0f);

    g.setColour (Palette::text.withAlpha (0.6f));
    g.setFont (10.0f);
    g.drawText (label, area.reduced (8.0f, 4.0f), juce::Justification::topLeft);

    g.setColour (accent);
    g.setFont (juce::Font (18.0f, juce::Font::bold));
    g.drawText (value, area.reduced (8.0f, 4.0f), juce::Justification::centred);
}

void MeterRow::paint (juce::Graphics& g)
{
    auto profile = proc.getLatestProfile();
    auto r = getLocalBounds().toFloat();
    auto cellWidth = r.getWidth() / 5.0f;

    auto fmt = [] (float v, const char* suffix)
    {
        if (v <= -99.0f) return juce::String ("--");
        return juce::String (v, 1) + suffix;
    };

    drawMeterCell (g, r.removeFromLeft (cellWidth), "SHORT-TERM LUFS (aprox)", fmt (profile.shortTermLufs, ""), Palette::cyan);
    drawMeterCell (g, r.removeFromLeft (cellWidth), "PEAK", fmt (profile.peakDb, " dB"), Palette::text);
    drawMeterCell (g, r.removeFromLeft (cellWidth), "CREST FACTOR", fmt (profile.crestFactorDb, " dB"),
                   profile.crestFactorDb < 6.0f ? Palette::red : Palette::cyan);
    drawMeterCell (g, r.removeFromLeft (cellWidth), "CORRELACION FASE", juce::String (profile.phaseCorrelation, 2),
                   profile.phaseCorrelation < 0.1f ? Palette::red : Palette::cyan);
    drawMeterCell (g, r, "SIBILANCIA (exceso)", fmt (profile.sibilanceExcessDb, " dB"),
                   profile.sibilanceExcessDb > 3.0f ? Palette::orange : Palette::text);
}

// ============================================================================
// DiagnosticsPanel
// ============================================================================
void DiagnosticsPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);
    auto profile = proc.getLatestProfile();
    auto lines = RecommendationEngine::generateGeneralDiagnostics (profile);

    auto r = getLocalBounds().reduced (12);
    g.setColour (Palette::text);
    g.setFont (13.0f);

    int y = r.getY();
    for (auto& line : lines)
    {
        g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa2 ")) + line,
                           r.getX(), y, r.getWidth(), 20, juce::Justification::topLeft, 2);
        y += 26;
    }
}

// ============================================================================
// RecommendationPanel
// ============================================================================
RecommendationPanel::RecommendationPanel (SonicDoctorAudioProcessor& p, RecommendationEngine& e, int idx)
    : proc (p), engine (e), pluginIndex (idx)
{
    startTimerHz (2);
}

void RecommendationPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);
    auto profile = proc.getLatestProfile();
    auto allSets = engine.generateAll (profile);

    if (pluginIndex < 0 || pluginIndex >= (int) allSets.size())
        return;

    const auto& set = allSets[(size_t) pluginIndex];
    auto r = getLocalBounds().reduced (12);

    if (set.recommendations.empty())
    {
        g.setColour (Palette::text.withAlpha (0.6f));
        g.setFont (13.0f);
        g.drawText ("Sin recomendaciones por ahora — el sonido no muestra problemas relevantes para este plugin.",
                    r, juce::Justification::topLeft);
        return;
    }

    int y = r.getY();
    for (auto& rec : set.recommendations)
    {
        ProLookAndFeel::drawRaisedPanel (g, juce::Rectangle<float> ((float) r.getX(), (float) y, (float) r.getWidth(), 58.0f), 6.0f);

        g.setColour (Palette::orange);
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText (rec.parameterName + "  ->  " + rec.suggestedValue, r.getX() + 10, y + 6, r.getWidth() - 20, 18, juce::Justification::left);

        if (rec.reason.isNotEmpty())
        {
            g.setColour (Palette::text.withAlpha (0.7f));
            g.setFont (11.5f);
            g.drawFittedText (rec.reason, r.getX() + 10, y + 26, r.getWidth() - 20, 26, juce::Justification::topLeft, 2);
        }

        y += 64;
    }
}

// ============================================================================
// Editor principal
// ============================================================================
SonicDoctorAudioProcessorEditor::SonicDoctorAudioProcessorEditor (SonicDoctorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      spectrum (p), meters (p), diagnosticsPanel (p)
{
    setLookAndFeel (&proLookAndFeel);

    title.setText ("SONIC DOCTOR", juce::dontSendNotification);
    title.setFont (juce::Font (20.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, Palette::cyan);
    addAndMakeVisible (title);

    sourceTypeBox.addItemList ({ "Generico", "Voz", "Kick", "Bajo", "Mezcla completa" }, 1);
    addAndMakeVisible (sourceTypeBox);
    sourceTypeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.apvts, "sourceType", sourceTypeBox);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (meters);

    tabs.setColour (juce::TabbedComponent::backgroundColourId, Palette::bg);
    tabs.addTab ("Diagnostico", Palette::panel, &diagnosticsPanel, false);

    auto pluginSets = recommendationEngine.generateAll (p.getLatestProfile());
    for (int i = 0; i < (int) pluginSets.size(); ++i)
    {
        auto* panel = new RecommendationPanel (p, recommendationEngine, i);
        recommendationPanels.add (panel);
        tabs.addTab (pluginSets[(size_t) i].pluginName, Palette::panel, panel, false);
    }

    addAndMakeVisible (tabs);

    setSize (820, 620);
}

SonicDoctorAudioProcessorEditor::~SonicDoctorAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void SonicDoctorAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient bgGrad (Palette::bg.brighter (0.06f), bounds.getX(), bounds.getY(),
                                  Palette::bg.darker (0.35f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (bgGrad);
    g.fillAll();
}

void SonicDoctorAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (14);

    auto topRow = r.removeFromTop (30);
    title.setBounds (topRow.removeFromLeft (300));
    sourceTypeBox.setBounds (topRow.removeFromRight (160));

    r.removeFromTop (10);
    spectrum.setBounds (r.removeFromTop (220));

    r.removeFromTop (10);
    meters.setBounds (r.removeFromTop (70));

    r.removeFromTop (10);
    tabs.setBounds (r);
}
