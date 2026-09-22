#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SoundProfile.h"

class SonicDoctorAudioProcessor : public juce::AudioProcessor
{
public:
    SonicDoctorAudioProcessor();
    ~SonicDoctorAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Sonic Doctor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // El editor llama a esto (en el hilo de mensajes) para obtener una
    // copia consistente del perfil de sonido más reciente.
    SoundProfile getLatestProfile() const;

    // --- Analizador de espectro (FFT), igual patrón que en Surgical De-Esser ---
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int scopeSize = fftSize / 2;

    bool isNextFFTBlockReady() const noexcept { return nextFFTBlockReady.load(); }
    void computeNextSpectrumFrame();
    const std::array<float, scopeSize>& getScopeData() const noexcept { return scopeData; }

    // --- Sistema de captura: fija una lectura confiable en vez de un valor
    // que se congela por accidente al detener la reproducción. ---
    void startCapture();
    void stopCapture();
    void resetCapture();
    bool isCapturingNow() const noexcept { return capturing.load(); }
    bool hasCapturedData() const;
    SoundProfile getCapturedProfile() const;

    // Para paneles que deben ser "confiables" al comparar: usa la captura
    // fija si existe, si no cae a la lectura en vivo.
    SoundProfile getProfileForDisplay() const
    {
        return hasCapturedData() ? getCapturedProfile() : getLatestProfile();
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void pushNextSampleIntoFifo (float sample) noexcept;
    void analyseSpectrumIntoProfile(); // usa scopeData ya calculado para llenar bandas/resonancias/sibilancia
    void finalizeCapture();

    // --- Filtro K-weighting simplificado (aprox. BS.1770) para LUFS ---
    struct KWeightingChannel
    {
        KWeightingChannel() = default;
        KWeightingChannel (const KWeightingChannel&) = delete;
        KWeightingChannel& operator= (const KWeightingChannel&) = delete;
        KWeightingChannel (KWeightingChannel&&) = default;
        KWeightingChannel& operator= (KWeightingChannel&&) = default;

        juce::dsp::IIR::Filter<float> highShelf, highPass;
        void reset() { highShelf.reset(); highPass.reset(); }
    };
    std::vector<KWeightingChannel> kWeighting;
    std::vector<float> momentaryRingBuffer; // ventana deslizante ~400ms, suma de cuadrados
    int momentaryWritePos = 0;
    int momentaryWindowSamples = 0;
    double runningMeanSquare400ms = 0.0;

    std::vector<float> shortTermRingBuffer; // ~3s
    int shortTermWritePos = 0;
    int shortTermWindowSamples = 0;
    double runningMeanSquare3s = 0.0;

    // --- Picos / RMS / crest factor ---
    float peakHold = 0.0f;
    float rmsEnvelope = 0.0f;

    // --- Correlación de fase ---
    double sumL = 0.0, sumR = 0.0, sumLR = 0.0;
    int correlationSampleCount = 0;

    // --- DC offset / ruido de fondo ---
    double dcAccumulator = 0.0;
    int dcSampleCount = 0;
    float noiseFloorFollower = 0.0f;

    // --- FFT ---
    juce::dsp::FFT forwardFFT { fftOrder };
    juce::dsp::WindowingFunction<float> windowFn { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> fifo {};
    std::array<float, fftSize * 2> fftData {};
    int fifoIndex = 0;
    std::atomic<bool> nextFFTBlockReady { false };
    std::array<float, scopeSize> scopeData {};

    // Perfil compartido con la UI (protegido con un lock simple, se actualiza pocas veces por segundo)
    mutable juce::CriticalSection profileLock;
    SoundProfile currentProfile;

    // --- Acumuladores de la captura (promedios reales de todo el pasaje, no un valor que decae) ---
    mutable juce::CriticalSection captureLock;
    std::atomic<bool> capturing { false };
    bool captureHasData = false;
    SoundProfile capturedProfileResult;

    double capSumSquares = 0.0;
    int capSampleCount = 0;
    float capPeakLinear = 0.0f;
    double capDcSum = 0.0;
    int capDcCount = 0;
    bool capClipped = false;
    double capCorrSumL = 0.0, capCorrSumR = 0.0, capCorrSumLR = 0.0;
    int capCorrCount = 0;

    double capBandSumLinear[SoundProfile::numBands] = {};
    int capBandCount[SoundProfile::numBands] = {};
    double capSibilanceSumLinear = 0.0;
    int capSibilanceCount = 0;
    double capRumbleSumLinear = 0.0;
    int capRumbleCount = 0;
    std::vector<ResonancePeak> capResonancesAll;
    bool wasCapturingLastBlock = false;
    std::atomic<bool> captureResetRequested { false };

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SonicDoctorAudioProcessor)
};
