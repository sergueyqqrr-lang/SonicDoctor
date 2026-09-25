#pragma once
#include <array>
#include <juce_core/juce_core.h>

// ============================================================================
// SoundProfile
//
// Es el "informe médico" del sonido: todo lo que el motor de análisis mide,
// empaquetado en un solo lugar. CUALQUIER recomendador (KickForge EQ,
// Surgical De-Esser, o el próximo plugin que hagas) lee de aquí — así,
// agregar soporte para un plugin nuevo nunca requiere tocar el analizador,
// solo escribir un recomendador nuevo que interprete estos mismos datos.
// ============================================================================

struct ResonancePeak
{
    float frequencyHz = 0.0f;
    float prominenceDb = 0.0f; // cuánto sobresale sobre el promedio local
};

struct SpectralBand
{
    const char* name;
    float lowHz;
    float highHz;
    float energyDb = -100.0f;      // energía medida en esta banda
    float referenceDb = -100.0f;   // cuánto se espera que esta banda esté por encima/debajo
                                    // del promedio general DEL PROPIO sonido (no un nivel
                                    // absoluto) — así funciona igual sin importar el gain staging.
    float deviationDb = 0.0f;      // desviación real vs. lo esperado (positivo = exceso, negativo = falta)
};

enum class SourceType
{
    Generico, Voz, Kick, Snare, Bajo, Guitarra, Piano,
    CuerdasMetales, SynthPad, BateriaCompleta, MezclaCompleta
};

struct SoundProfile
{
    // --- Loudness / dinámica ---
    float momentaryLufs = -100.0f;   // aproximado (K-weighting simplificado, ventana 400ms)
    float shortTermLufs = -100.0f;   // aproximado, ventana 3s
    float peakDb = -100.0f;
    float crestFactorDb = 0.0f;      // peak - RMS: dinámica. Bajo (<6dB) sugiere sobre-compresión.

    // --- Estéreo / fase ---
    float phaseCorrelation = 1.0f;   // -1 (fuera de fase) a +1 (mono perfecto). ~0.3-0.8 es normal.

    // --- Problemas específicos ---
    float sibilanceLevelDb = -100.0f;  // energía en banda de eses (4-10kHz)
    float sibilanceExcessDb = 0.0f;    // cuánto excede lo típico (ya relativo al promedio propio del sonido)
    float rumbleLevelDb = -100.0f;     // energía por debajo de 30Hz (ruido de manejo, HVAC, etc.)
    float rumbleRelativeDb = -100.0f;  // rumbleLevelDb relativo al promedio propio del sonido
    float broadbandAverageDb = -100.0f; // promedio de las 7 bandas — el "cero" contra el que se comparan las demás medidas
    bool  clippingDetected = false;
    float dcOffset = 0.0f;
    float noiseFloorDb = -100.0f;

    // --- Balance espectral (7 bandas) ---
    static constexpr int numBands = 7;
    std::array<SpectralBand, numBands> bands { {
        { "Sub",        20.0f,   60.0f },
        { "Graves",     60.0f,  250.0f },
        { "Low-Mid",   250.0f,  500.0f },
        { "Medios",    500.0f, 2000.0f },
        { "Upper-Mid",2000.0f, 4000.0f },
        { "Presencia",4000.0f, 6000.0f },
        { "Brillo",   6000.0f,20000.0f }
    } };

    // --- Resonancias detectadas (hasta 3, ordenadas por prominencia) ---
    static constexpr int maxResonances = 3;
    std::array<ResonancePeak, maxResonances> resonances {};
    int numResonancesFound = 0;

    SourceType sourceType = SourceType::Generico;
};
