#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SourceTypeProfiles.h"
#include <algorithm>
#include <cmath>

SonicDoctorAudioProcessor::SonicDoctorAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

SonicDoctorAudioProcessor::~SonicDoctorAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout SonicDoctorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "sourceType", "Tipo de fuente", getSourceTypeChoices(), 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "resonanceSensitivity", "Sensibilidad resonancias",
        juce::NormalisableRange<float> (1.0f, 12.0f, 0.1f), 5.0f));

    return { params.begin(), params.end() };
}

void SonicDoctorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate;

    auto numChannels = juce::jmax (1, getTotalNumOutputChannels());
    kWeighting.clear();
    kWeighting.resize ((size_t) numChannels);
    for (auto& kw : kWeighting)
    {
        // Aproximación del pre-filtro K-weighting de BS.1770: shelf en agudos + HPF en subgraves.
        *kw.highShelf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 1500.0, 0.7f,
                                        juce::Decibels::decibelsToGain (4.0f));
        *kw.highPass.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 38.0, 0.5f);
        kw.reset();
    }

    momentaryWindowSamples = (int) (0.4 * sampleRate);
    momentaryRingBuffer.assign ((size_t) juce::jmax (1, momentaryWindowSamples), 0.0f);
    momentaryWritePos = 0;
    runningMeanSquare400ms = 0.0;

    shortTermWindowSamples = (int) (3.0 * sampleRate);
    shortTermRingBuffer.assign ((size_t) juce::jmax (1, shortTermWindowSamples), 0.0f);
    shortTermWritePos = 0;
    runningMeanSquare3s = 0.0;

    peakHold = 0.0f;
    rmsEnvelope = 0.0f;
    sumL = sumR = sumLR = 0.0;
    correlationSampleCount = 0;
    dcAccumulator = 0.0;
    dcSampleCount = 0;
    noiseFloorFollower = 1.0f; // arranca alto y va bajando hasta encontrar el silencio real

    juce::ScopedLock lock (profileLock);
    currentProfile = SoundProfile();
}

void SonicDoctorAudioProcessor::releaseResources() {}

bool SonicDoctorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void SonicDoctorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    constexpr float peakDecayPerBlock = 0.999f;   // decaimiento suave del pico retenido
    constexpr float rmsAttack = 0.3f, rmsRelease = 0.02f;

    const bool isCapturingThisBlock = capturing.load();
    if (isCapturingThisBlock && captureResetRequested.exchange (false))
    {
        // Se acaba de pedir "Iniciar captura": limpiamos acumuladores en el hilo de audio.
        juce::ScopedLock lock (captureLock);
        capSumSquares = 0.0; capSampleCount = 0; capPeakLinear = 0.0f;
        capDcSum = 0.0; capDcCount = 0; capClipped = false;
        capCorrSumL = capCorrSumR = capCorrSumLR = 0.0; capCorrCount = 0;
    }

    // Acumuladores de ESTE bloque para la captura (se suman al total una sola vez al final,
    // para no tomar el lock por cada muestra).
    double blockSumSquares = 0.0;
    int blockSampleCount = 0;
    float blockPeakLinear = 0.0f;
    double blockDcSum = 0.0;
    int blockDcCount = 0;
    bool blockClipped = false;
    double blockCorrSumL = 0.0, blockCorrSumR = 0.0, blockCorrSumLR = 0.0;
    int blockCorrCount = 0;

    for (int n = 0; n < numSamples; ++n)
    {
        float monoSum = 0.0f;
        float kWeightedSquareSum = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float x = buffer.getSample (ch, n);
            monoSum += x;

            // --- K-weighting para loudness aproximado ---
            auto& kw = kWeighting[(size_t) ch];
            float weighted = kw.highPass.processSample (kw.highShelf.processSample (x));
            kWeightedSquareSum += weighted * weighted;

            // --- Pico ---
            peakHold = juce::jmax (peakHold * peakDecayPerBlock, std::abs (x));

            // --- DC offset ---
            dcAccumulator += x;
            ++dcSampleCount;

            if (isCapturingThisBlock)
            {
                blockPeakLinear = juce::jmax (blockPeakLinear, std::abs (x));
                blockDcSum += x;
                ++blockDcCount;
                if (std::abs (x) >= 0.98f)
                    blockClipped = true;
            }
        }

        float monoAvg = numChannels > 0 ? monoSum / (float) numChannels : 0.0f;

        // --- RMS (para crest factor) ---
        float instant = monoAvg * monoAvg;
        float rmsCoeff = (instant > rmsEnvelope) ? rmsAttack : rmsRelease;
        rmsEnvelope = rmsCoeff * rmsEnvelope + (1.0f - rmsCoeff) * instant;

        if (isCapturingThisBlock)
        {
            blockSumSquares += instant;
            ++blockSampleCount;
        }

        // --- Loudness momentáneo (ventana ~400ms) ---
        if (momentaryWindowSamples > 0)
        {
            float kwMeanAcrossCh = kWeightedSquareSum / (float) juce::jmax (1, numChannels);
            runningMeanSquare400ms -= momentaryRingBuffer[(size_t) momentaryWritePos];
            momentaryRingBuffer[(size_t) momentaryWritePos] = kwMeanAcrossCh;
            runningMeanSquare400ms += kwMeanAcrossCh;
            momentaryWritePos = (momentaryWritePos + 1) % momentaryWindowSamples;
        }

        // --- Loudness short-term (ventana ~3s) ---
        if (shortTermWindowSamples > 0)
        {
            float kwMeanAcrossCh = kWeightedSquareSum / (float) juce::jmax (1, numChannels);
            runningMeanSquare3s -= shortTermRingBuffer[(size_t) shortTermWritePos];
            shortTermRingBuffer[(size_t) shortTermWritePos] = kwMeanAcrossCh;
            runningMeanSquare3s += kwMeanAcrossCh;
            shortTermWritePos = (shortTermWritePos + 1) % shortTermWindowSamples;
        }

        // --- Correlación de fase (solo si hay 2 canales) ---
        if (numChannels >= 2)
        {
            float l = buffer.getSample (0, n);
            float r = buffer.getSample (1, n);
            sumL += (double) l * l;
            sumR += (double) r * r;
            sumLR += (double) l * r;
            ++correlationSampleCount;

            if (isCapturingThisBlock)
            {
                blockCorrSumL += (double) l * l;
                blockCorrSumR += (double) r * r;
                blockCorrSumLR += (double) l * r;
                ++blockCorrCount;
            }
        }

        // --- Piso de ruido: sigue el mínimo del RMS con caída muy lenta ---
        float rmsLinear = std::sqrt (rmsEnvelope);
        if (rmsLinear < noiseFloorFollower)
            noiseFloorFollower = rmsLinear;
        else
            noiseFloorFollower += (rmsLinear - noiseFloorFollower) * 0.00001f; // sube muy lento (olvida silencios viejos)

        pushNextSampleIntoFifo (monoAvg);
    }

    // Vuelca los totales de este bloque a los acumuladores de captura (una sola vez, con lock).
    if (isCapturingThisBlock)
    {
        juce::ScopedLock lock (captureLock);
        capSumSquares += blockSumSquares;
        capSampleCount += blockSampleCount;
        capPeakLinear = juce::jmax (capPeakLinear, blockPeakLinear);
        capDcSum += blockDcSum;
        capDcCount += blockDcCount;
        capClipped = capClipped || blockClipped;
        capCorrSumL += blockCorrSumL;
        capCorrSumR += blockCorrSumR;
        capCorrSumLR += blockCorrSumLR;
        capCorrCount += blockCorrCount;
    }
    wasCapturingLastBlock = isCapturingThisBlock;


    // Publica los valores calculados a nivel de bloque en el perfil compartido.
    // (El análisis espectral fino ocurre por separado, ver computeNextSpectrumFrame / analyseSpectrumIntoProfile,
    //  llamado desde el editor cuando hay un frame de FFT nuevo.)
    {
        juce::ScopedLock lock (profileLock);

        currentProfile.peakDb = juce::Decibels::gainToDecibels (peakHold, -100.0f);
        float rmsDb = juce::Decibels::gainToDecibels (std::sqrt (rmsEnvelope), -100.0f);
        currentProfile.crestFactorDb = currentProfile.peakDb - rmsDb;

        constexpr float lufsOffset = -0.691f; // offset estándar de BS.1770
        if (momentaryWindowSamples > 0)
        {
            auto meanSquare = juce::jmax (1.0e-10, runningMeanSquare400ms / momentaryWindowSamples);
            currentProfile.momentaryLufs = (float) (10.0 * std::log10 (meanSquare) + (double) lufsOffset);
        }
        if (shortTermWindowSamples > 0)
        {
            auto meanSquare = juce::jmax (1.0e-10, runningMeanSquare3s / shortTermWindowSamples);
            currentProfile.shortTermLufs = (float) (10.0 * std::log10 (meanSquare) + (double) lufsOffset);
        }

        if (correlationSampleCount > 0 && sumL > 0.0 && sumR > 0.0)
            currentProfile.phaseCorrelation = (float) (sumLR / std::sqrt (sumL * sumR));

        if (dcSampleCount > 0)
            currentProfile.dcOffset = (float) (dcAccumulator / dcSampleCount);

        currentProfile.noiseFloorDb = juce::Decibels::gainToDecibels (noiseFloorFollower, -100.0f);
        currentProfile.clippingDetected = peakHold >= 0.98f;

        currentProfile.sourceType = (SourceType) (int) apvts.getRawParameterValue ("sourceType")->load();
    }
}

void SonicDoctorAudioProcessor::pushNextSampleIntoFifo (float sample) noexcept
{
    if (fifoIndex == fftSize)
    {
        if (! nextFFTBlockReady.load())
        {
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            std::copy (fifo.begin(), fifo.end(), fftData.begin());
            nextFFTBlockReady.store (true);
        }
        fifoIndex = 0;
    }
    fifo[(size_t) fifoIndex++] = sample;
}

void SonicDoctorAudioProcessor::computeNextSpectrumFrame()
{
    windowFn.multiplyWithWindowingTable (fftData.data(), fftSize);
    forwardFFT.performFrequencyOnlyForwardTransform (fftData.data());

    constexpr float minDb = -100.0f;
    constexpr float maxDb = 0.0f;

    // IMPORTANTE: JUCE no normaliza la magnitud de la FFT por el tamaño de la
    // ventana. Sin esto, los niveles medidos salen inflados ~20-60dB por encima
    // de lo real, y el efecto es peor justo en la banda Sub (pocos bins de
    // resolución ahí, así que un solo bin con energía concentrada — típico de
    // un kick — dispara el promedio de esa banda mucho más que en las demás).
    constexpr float fftNormalisation = 2.0f / (float) fftSize;

    for (int i = 0; i < scopeSize; ++i)
    {
        auto magnitude = fftData[(size_t) i] * fftNormalisation;
        auto levelDb = juce::Decibels::gainToDecibels (magnitude, minDb);
        scopeData[(size_t) i] = juce::jlimit (0.0f, 1.0f, juce::jmap (levelDb, minDb, maxDb, 0.0f, 1.0f));
    }

    nextFFTBlockReady.store (false);
    analyseSpectrumIntoProfile();
}

void SonicDoctorAudioProcessor::analyseSpectrumIntoProfile()
{
    auto sr = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    auto sensitivity = apvts.getRawParameterValue ("resonanceSensitivity")->load();

    SoundProfile snapshot;
    {
        juce::ScopedLock lock (profileLock);
        snapshot = currentProfile; // partimos de lo ya calculado a nivel de bloque
    }

    // --- Balance espectral por banda ---
    bool isCapturingNow = capturing.load();
    int bandIdxForCapture = 0;
    for (auto& band : snapshot.bands)
    {
        double sumLinear = 0.0;
        int count = 0;
        for (int i = 1; i < scopeSize; ++i)
        {
            auto freq = (float) i * (float) sr / (float) fftSize;
            if (freq < band.lowHz || freq > band.highHz)
                continue;
            auto db = juce::jmap (scopeData[(size_t) i], 0.0f, 1.0f, -100.0f, 0.0f);
            sumLinear += juce::Decibels::decibelsToGain (db);
            ++count;
        }
        band.energyDb = count > 0 ? juce::Decibels::gainToDecibels ((float) (sumLinear / count), -100.0f) : -100.0f;

        if (isCapturingNow && count > 0)
        {
            juce::ScopedLock lock (captureLock);
            capBandSumLinear[bandIdxForCapture] += sumLinear / count;
            capBandCount[bandIdxForCapture] += 1;
        }
        ++bandIdxForCapture;
    }

    int refIndex = 0;
    const auto& cfg = getSourceTypeConfig (snapshot.sourceType);

    // El "cero" contra el que se comparan las bandas: el promedio general
    // DE ESTE MISMO sonido, no un nivel absoluto — así el análisis funciona
    // igual sin importar el gain staging/normalización del archivo.
    double broadbandSum = 0.0;
    for (auto& band : snapshot.bands)
        broadbandSum += band.energyDb;
    snapshot.broadbandAverageDb = (float) (broadbandSum / SoundProfile::numBands);

    for (auto& band : snapshot.bands)
    {
        band.referenceDb = cfg.referenceDb[refIndex++];
        band.deviationDb = (band.energyDb - snapshot.broadbandAverageDb) - band.referenceDb;
    }

    // --- Sibilancia (4-10kHz) ---
    {
        double sumLinear = 0.0;
        int count = 0;
        for (int i = 1; i < scopeSize; ++i)
        {
            auto freq = (float) i * (float) sr / (float) fftSize;
            if (freq < 4000.0f || freq > 10000.0f)
                continue;
            auto db = juce::jmap (scopeData[(size_t) i], 0.0f, 1.0f, -100.0f, 0.0f);
            sumLinear += juce::Decibels::decibelsToGain (db);
            ++count;
        }
        snapshot.sibilanceLevelDb = count > 0 ? juce::Decibels::gainToDecibels ((float) (sumLinear / count), -100.0f) : -100.0f;
        snapshot.sibilanceExcessDb = (snapshot.sibilanceLevelDb - snapshot.broadbandAverageDb) - cfg.sibilanceRelativeDb;

        if (isCapturingNow && count > 0)
        {
            juce::ScopedLock lock (captureLock);
            capSibilanceSumLinear += sumLinear / count;
            capSibilanceCount += 1;
        }
    }

    // --- Rumble (<30Hz, por debajo de lo que produce cualquier instrumento musical,
    //     incluido un kick — así que si hay energía aquí, es genuinamente ruido) ---
    {
        double sumLinear = 0.0;
        int count = 0;
        for (int i = 1; i < scopeSize; ++i)
        {
            auto freq = (float) i * (float) sr / (float) fftSize;
            if (freq > 30.0f)
                break;
            auto db = juce::jmap (scopeData[(size_t) i], 0.0f, 1.0f, -100.0f, 0.0f);
            sumLinear += juce::Decibels::decibelsToGain (db);
            ++count;
        }
        snapshot.rumbleLevelDb = count > 0 ? juce::Decibels::gainToDecibels ((float) (sumLinear / count), -100.0f) : -100.0f;
        snapshot.rumbleRelativeDb = snapshot.rumbleLevelDb - snapshot.broadbandAverageDb;

        if (isCapturingNow && count > 0)
        {
            juce::ScopedLock lock (captureLock);
            capRumbleSumLinear += sumLinear / count;
            capRumbleCount += 1;
        }
    }

    // --- Detección de resonancias: picos angostos que sobresalen del promedio local ---
    {
        std::vector<ResonancePeak> found;
        constexpr int localWindow = 8;
        for (int i = localWindow; i < scopeSize - localWindow; ++i)
        {
            auto freq = (float) i * (float) sr / (float) fftSize;
            if (freq < 40.0f || freq > 16000.0f)
                continue;

            auto dbHere = juce::jmap (scopeData[(size_t) i], 0.0f, 1.0f, -100.0f, 0.0f);

            double localSum = 0.0;
            for (int k = -localWindow; k <= localWindow; ++k)
            {
                if (k == 0) continue;
                localSum += juce::jmap (scopeData[(size_t) (i + k)], 0.0f, 1.0f, -100.0f, 0.0f);
            }
            auto localAvgDb = (float) (localSum / (2 * localWindow));
            auto prominence = dbHere - localAvgDb;

            if (prominence > sensitivity)
                found.push_back ({ freq, prominence });
        }

        std::sort (found.begin(), found.end(), [] (const ResonancePeak& a, const ResonancePeak& b)
        {
            return a.prominenceDb > b.prominenceDb;
        });

        snapshot.numResonancesFound = juce::jmin ((int) found.size(), SoundProfile::maxResonances);
        for (int i = 0; i < snapshot.numResonancesFound; ++i)
            snapshot.resonances[(size_t) i] = found[(size_t) i];

        if (isCapturingNow && ! found.empty())
        {
            juce::ScopedLock lock (captureLock);
            for (int i = 0; i < snapshot.numResonancesFound; ++i)
                capResonancesAll.push_back (found[(size_t) i]);
            // Mantenemos la lista acotada: nos quedamos solo con las mejores 10 vistas hasta ahora.
            std::sort (capResonancesAll.begin(), capResonancesAll.end(), [] (const ResonancePeak& a, const ResonancePeak& b)
            {
                return a.prominenceDb > b.prominenceDb;
            });
            if (capResonancesAll.size() > 10)
                capResonancesAll.resize (10);
        }
    }

    juce::ScopedLock lock (profileLock);
    currentProfile = snapshot;
}

SoundProfile SonicDoctorAudioProcessor::getLatestProfile() const
{
    juce::ScopedLock lock (profileLock);
    return currentProfile;
}

void SonicDoctorAudioProcessor::startCapture()
{
    {
        juce::ScopedLock lock (captureLock);
        capSumSquares = 0.0; capSampleCount = 0; capPeakLinear = 0.0f;
        capDcSum = 0.0; capDcCount = 0; capClipped = false;
        capCorrSumL = capCorrSumR = capCorrSumLR = 0.0; capCorrCount = 0;
        for (int i = 0; i < SoundProfile::numBands; ++i) { capBandSumLinear[i] = 0.0; capBandCount[i] = 0; }
        capSibilanceSumLinear = 0.0; capSibilanceCount = 0;
        capRumbleSumLinear = 0.0; capRumbleCount = 0;
        capResonancesAll.clear();
        captureHasData = false;
    }
    captureResetRequested.store (true);
    capturing.store (true);
}

void SonicDoctorAudioProcessor::stopCapture()
{
    capturing.store (false);
    finalizeCapture();
}

void SonicDoctorAudioProcessor::resetCapture()
{
    juce::ScopedLock lock (captureLock);
    captureHasData = false;
    capturedProfileResult = SoundProfile();
}

bool SonicDoctorAudioProcessor::hasCapturedData() const
{
    juce::ScopedLock lock (captureLock);
    return captureHasData;
}

SoundProfile SonicDoctorAudioProcessor::getCapturedProfile() const
{
    juce::ScopedLock lock (captureLock);
    return capturedProfileResult;
}

void SonicDoctorAudioProcessor::finalizeCapture()
{
    juce::ScopedLock lock (captureLock);
    if (capSampleCount <= 0)
        return;

    SoundProfile result;
    result.sourceType = (SourceType) (int) apvts.getRawParameterValue ("sourceType")->load();

    auto meanSquare = capSumSquares / capSampleCount;
    auto rmsDb = juce::Decibels::gainToDecibels ((float) std::sqrt (meanSquare), -100.0f);
    result.peakDb = juce::Decibels::gainToDecibels (capPeakLinear, -100.0f);
    result.crestFactorDb = result.peakDb - rmsDb;

    auto integratedLufs = (float) (10.0 * std::log10 (juce::jmax (1.0e-10, meanSquare)) - 0.691);
    result.shortTermLufs = integratedLufs;
    result.momentaryLufs = integratedLufs; // en la captura mostramos el mismo valor integrado en ambos

    if (capDcCount > 0)
        result.dcOffset = (float) (capDcSum / capDcCount);
    result.clippingDetected = capClipped;

    if (capCorrCount > 0 && capCorrSumL > 0.0 && capCorrSumR > 0.0)
        result.phaseCorrelation = (float) (capCorrSumLR / std::sqrt (capCorrSumL * capCorrSumR));

    for (int i = 0; i < SoundProfile::numBands; ++i)
    {
        auto& band = result.bands[(size_t) i];
        band.energyDb = capBandCount[i] > 0
            ? juce::Decibels::gainToDecibels ((float) (capBandSumLinear[i] / capBandCount[i]), -100.0f)
            : -100.0f;
    }

    const auto& cfg = getSourceTypeConfig (result.sourceType);
    double broadbandSum = 0.0;
    for (auto& band : result.bands)
        broadbandSum += band.energyDb;
    result.broadbandAverageDb = (float) (broadbandSum / SoundProfile::numBands);

    for (int i = 0; i < SoundProfile::numBands; ++i)
    {
        auto& band = result.bands[(size_t) i];
        band.referenceDb = cfg.referenceDb[i];
        band.deviationDb = (band.energyDb - result.broadbandAverageDb) - band.referenceDb;
    }

    result.sibilanceLevelDb = capSibilanceCount > 0
        ? juce::Decibels::gainToDecibels ((float) (capSibilanceSumLinear / capSibilanceCount), -100.0f) : -100.0f;
    result.sibilanceExcessDb = (result.sibilanceLevelDb - result.broadbandAverageDb) - cfg.sibilanceRelativeDb;

    result.rumbleLevelDb = capRumbleCount > 0
        ? juce::Decibels::gainToDecibels ((float) (capRumbleSumLinear / capRumbleCount), -100.0f) : -100.0f;
    result.rumbleRelativeDb = result.rumbleLevelDb - result.broadbandAverageDb;

    std::sort (capResonancesAll.begin(), capResonancesAll.end(), [] (const ResonancePeak& a, const ResonancePeak& b)
    {
        return a.prominenceDb > b.prominenceDb;
    });
    result.numResonancesFound = juce::jmin ((int) capResonancesAll.size(), SoundProfile::maxResonances);
    for (int i = 0; i < result.numResonancesFound; ++i)
        result.resonances[(size_t) i] = capResonancesAll[(size_t) i];

    capturedProfileResult = result;
    captureHasData = true;
}

juce::AudioProcessorEditor* SonicDoctorAudioProcessor::createEditor()
{
    return new SonicDoctorAudioProcessorEditor (*this);
}

void SonicDoctorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SonicDoctorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SonicDoctorAudioProcessor();
}
