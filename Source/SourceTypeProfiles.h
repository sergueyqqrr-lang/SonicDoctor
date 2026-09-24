#pragma once
#include "SoundProfile.h"

// ============================================================================
// SourceTypeProfiles
//
// Centraliza TODO lo que depende del tipo de fuente en un solo lugar, para
// no tener reglas sueltas regadas por el código (eso fue lo que causaba los
// falsos positivos: el "rumble" y la "sibilancia" se evaluaban igual para
// un Kick que para una Voz, lo cual no tiene sentido).
//
// Los números de las curvas de referencia son aproximaciones prácticas de
// balance espectral típico, no mediciones certificadas — ver README.
// ============================================================================

struct SourceTypeConfig
{
    const char* label;
    float referenceDb[SoundProfile::numBands]; // Sub, Graves, Low-Mid, Medios, Upper-Mid, Presencia, Brillo

    // Umbral de "rumble" (energía < 30Hz considerada problema, no contenido musical).
    // Un Kick o un Bajo tienen sub real por diseño, así que su umbral es mucho más
    // permisivo que el de una Voz, donde casi no debería haber nada ahí.
    float rumbleThresholdDb;

    // Si la sibilancia (4-10kHz) tiene sentido evaluarla para este tipo de fuente.
    // Un Kick o un Bajo no tienen "eses" — recomendarles un de-esser no tiene sentido.
    bool sibilanceRelevant;

    // Umbral de crest factor por debajo del cual se sugiere "posible sobre-compresión".
    // Un Kick de EDM o un Bajo suelen comprimirse fuerte a propósito; una Voz o una
    // Mezcla completa normalmente necesitan más rango dinámico.
    float crestFactorWarnDb;

    // Si tiene sentido el diagnóstico de loudness (LUFS). Para un one-shot percusivo
    // aislado (Kick, Snare) la métrica de loudness continuo no es muy informativa.
    bool loudnessCheckRelevant;
};

inline const SourceTypeConfig& getSourceTypeConfig (SourceType type)
{
    //                                    Sub    Graves LowMid Medios UppMid Presen Brillo   rumble  sib    crest  loud
    static const SourceTypeConfig table[] = {
        /* Generico       */ { "Generico",       { -24, -14, -16, -14, -18, -22, -28 }, -45.0f, true,  6.0f, true  },
        /* Voz            */ { "Voz",            { -40, -28, -18, -12, -14, -18, -26 }, -55.0f, true,  6.0f, true  },
        /* Kick           */ { "Kick",           { -10,  -8, -18, -22, -26, -28, -32 }, -30.0f, false, 3.5f, false },
        /* Snare          */ { "Snare",          { -35, -18, -10, -14, -16, -14, -20 }, -45.0f, false, 4.0f, false },
        /* Bajo           */ { "Bajo",           {  -8, -10, -16, -22, -30, -34, -38 }, -25.0f, false, 3.5f, false },
        /* Guitarra       */ { "Guitarra",       { -35, -20, -12, -10, -14, -18, -26 }, -48.0f, true,  6.0f, true  },
        /* Piano          */ { "Piano/Teclado",  { -30, -16, -14, -12, -14, -18, -24 }, -48.0f, true,  6.0f, true  },
        /* CuerdasMetales */ { "Cuerdas/Metales",{ -32, -18, -14, -11, -13, -17, -24 }, -48.0f, true,  6.0f, true  },
        /* SynthPad       */ { "Synth/Pad",      { -20, -14, -15, -13, -15, -19, -25 }, -40.0f, true,  8.0f, true  },
        /* BateriaCompleta*/ { "Bateria completa",{ -22, -14, -16, -14, -13, -15, -18 }, -35.0f, false, 4.0f, true  },
        /* MezclaCompleta */ { "Mezcla completa",{ -18, -12, -14, -12, -14, -18, -24 }, -40.0f, true,  6.0f, true  },
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
