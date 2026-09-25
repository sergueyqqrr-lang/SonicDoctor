#pragma once
#include "SoundProfile.h"

// ============================================================================
// SourceTypeProfiles
//
// Centraliza TODO lo que depende del tipo de fuente en un solo lugar.
//
// IMPORTANTE: los numeros de "referenceDb" NO son niveles absolutos en dB.
// Son la forma espectral esperada, expresada como "cuanto por encima o por
// debajo del promedio general del PROPIO sonido deberia estar esta banda".
// Esto es a proposito: comparar contra un nivel absoluto depende del gain
// staging/normalizacion del archivo (un kick grabado 6dB mas bajo no tiene
// "menos sub" -- sigue teniendo la misma forma). Comparando contra el propio
// promedio del sonido, el analisis funciona igual sin importar el nivel.
//
// Mismo criterio para rumbleThresholdDb y sibilanceRelativeDb: son umbrales
// relativos al promedio propio del sonido, no niveles absolutos.
// ============================================================================

struct SourceTypeConfig
{
    const char* label;
    float referenceDb[SoundProfile::numBands]; // Sub, Graves, Low-Mid, Medios, Upper-Mid, Presencia, Brillo -- relativos al promedio propio

    // Umbral relativo de "rumble": si el contenido <30Hz esta a menos de esta
    // cantidad de dB por debajo del promedio propio del sonido, se considera
    // sospechoso. Un Kick/Bajo es mucho mas permisivo (su sub real puede
    // legitimamente acercarse a su propio promedio); una Voz es mas estricto.
    float rumbleThresholdDb;

    // Si la sibilancia (4-10kHz) tiene sentido evaluarla para este tipo de fuente.
    bool sibilanceRelevant;

    // Nivel relativo esperado de la banda de sibilancia vs. el promedio propio
    // del sonido (solo se usa si sibilanceRelevant es true).
    float sibilanceRelativeDb;

    // Umbral de crest factor por debajo del cual se sugiere "posible sobre-compresion".
    float crestFactorWarnDb;

    // Si tiene sentido el diagnostico de loudness continuo (LUFS).
    bool loudnessCheckRelevant;
};

inline const SourceTypeConfig& getSourceTypeConfig (SourceType type)
{
    //                                     Sub     Graves  LowMid  Medios  UppMid  Presen  Brillo    rumble  sib    sibRel crest  loud
    static const SourceTypeConfig table[] = {
        /* Generico       */ { "Generico",        { -4.6f,  5.4f,  3.4f,  5.4f,  1.4f, -2.6f, -8.6f }, -10.0f, true, -10.0f, 6.0f, true  },
        /* Voz            */ { "Voz",             {-17.7f, -5.7f,  4.3f, 10.3f,  8.3f,  4.3f, -3.7f }, -14.0f, true,  -8.0f, 6.0f, true  },
        /* Kick           */ { "Kick",            { 10.6f, 12.6f,  2.6f, -1.4f, -5.4f, -7.4f,-11.4f },  -4.0f, false,  0.0f, 3.5f, false },
        /* Snare          */ { "Snare",           {-16.9f,  0.1f,  8.1f,  4.1f,  2.1f,  4.1f, -1.9f },  -8.0f, false,  0.0f, 4.0f, false },
        /* Bajo           */ { "Bajo",            { 14.6f, 12.6f,  6.6f,  0.6f, -7.4f,-11.4f,-15.4f },  -2.0f, false,  0.0f, 3.5f, false },
        /* Guitarra       */ { "Guitarra",        {-15.7f, -0.7f,  7.3f,  9.3f,  5.3f,  1.3f, -6.7f }, -12.0f, true, -12.0f, 6.0f, true  },
        /* Piano          */ { "Piano/Teclado",   {-11.7f,  2.3f,  4.3f,  6.3f,  4.3f,  0.3f, -5.7f }, -12.0f, true, -10.0f, 6.0f, true  },
        /* CuerdasMetales */ { "Cuerdas/Metales", {-13.6f,  0.4f,  4.4f,  7.4f,  5.4f,  1.4f, -5.6f }, -12.0f, true, -10.0f, 6.0f, true  },
        /* SynthPad       */ { "Synth/Pad",       { -2.7f,  3.3f,  2.3f,  4.3f,  2.3f, -1.7f, -7.7f },  -8.0f, true, -10.0f, 8.0f, true  },
        /* BateriaCompleta*/ { "Bateria completa",{ -6.0f,  2.0f,  0.0f,  2.0f,  3.0f,  1.0f, -2.0f },  -6.0f, false,  0.0f, 4.0f, true  },
        /* MezclaCompleta */ { "Mezcla completa", { -2.0f,  4.0f,  2.0f,  4.0f,  2.0f, -2.0f, -8.0f }, -10.0f, true,  -8.0f, 6.0f, true  },
    };

    auto index = juce::jlimit (0, (int) (sizeof (table) / sizeof (table[0])) - 1, (int) type);
    return table[index];
}

inline juce::StringArray getSourceTypeChoices()
{
    juce::StringArray choices;
    for (int i = 0; i <= (int) SourceType::MezclaCompleta; ++i)
        choices.add (getSourceTypeConfig ((SourceType) i).label);
    return choices;
}
