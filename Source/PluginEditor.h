#pragma once
#include "PluginProcessor.h"
#include "ProLookAndFeel.h"
#include "Recommendations/RecommendationEngine.h"

// Analizador de espectro con la curva de referencia (típica para el tipo de
// fuente elegido) superpuesta, y marcadores de resonancias detectadas.
class DoctorSpectrum : public juce::Component, private juce::Timer
{
public:
    explicit DoctorSpectrum (SonicDoctorAudioProcessor& p);
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    float freqToX (float freqHz, float width) const;

    SonicDoctorAudioProcessor& proc;
    std::array<float, SonicDoctorAudioProcessor::scopeSize> smoothedData {};
    static constexpr float attackCoeff = 0.35f, releaseCoeff = 0.90f;
};

// Fila de medidores (LUFS, Peak, Crest Factor, Correlación)
class MeterRow : public juce::Component, private juce::Timer
{
public:
    explicit MeterRow (SonicDoctorAudioProcessor& p) : proc (p) { startTimerHz (10); }
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override { repaint(); }
    void drawMeterCell (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& label,
                         const juce::String& value, juce::Colour accent) const;
    SonicDoctorAudioProcessor& proc;
};

// Panel de diagnóstico general (texto). Usa la captura fija si existe,
// si no, cae a la lectura en vivo.
class DiagnosticsPanel : public juce::Component, private juce::Timer
{
public:
    explicit DiagnosticsPanel (SonicDoctorAudioProcessor& p) : proc (p) { startTimerHz (2); }
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override { repaint(); }
    SonicDoctorAudioProcessor& proc;
};

// Panel de recomendaciones para un plugin específico (una pestaña).
// Usa la captura fija si existe, si no, cae a la lectura en vivo.
class RecommendationPanel : public juce::Component, private juce::Timer
{
public:
    RecommendationPanel (SonicDoctorAudioProcessor& p, RecommendationEngine& engine, int pluginIndex);
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override { repaint(); }
    SonicDoctorAudioProcessor& proc;
    RecommendationEngine& engine;
    int pluginIndex;
};

// Barra de captura: Iniciar / Detener / Reiniciar, con indicador de estado.
// Esto es lo que resuelve "quiero comparar con vs sin, con un número fijo
// confiable" en vez de un valor que se congela por accidente al parar.
class CaptureBar : public juce::Component, private juce::Timer
{
public:
    explicit CaptureBar (SonicDoctorAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }
    SonicDoctorAudioProcessor& proc;
    juce::TextButton startButton { "Iniciar captura" };
    juce::TextButton stopButton { "Detener captura" };
    juce::TextButton resetButton { "Reiniciar" };
};

class SonicDoctorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SonicDoctorAudioProcessorEditor (SonicDoctorAudioProcessor&);
    ~SonicDoctorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SonicDoctorAudioProcessor& audioProcessor;
    RecommendationEngine recommendationEngine;

    juce::Label title;
    juce::ComboBox sourceTypeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceTypeAttach;

    DoctorSpectrum spectrum;
    MeterRow meters;
    CaptureBar captureBar;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    DiagnosticsPanel diagnosticsPanel;
    juce::OwnedArray<RecommendationPanel> recommendationPanels;

    ProLookAndFeel proLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SonicDoctorAudioProcessorEditor)
};
