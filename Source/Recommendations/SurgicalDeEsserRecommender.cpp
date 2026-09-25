#include "SurgicalDeEsserRecommender.h"
#include "../SourceTypeProfiles.h"

std::vector<Recommendation> SurgicalDeEsserRecommender::generate (const SoundProfile& profile) const
{
    std::vector<Recommendation> out;
    const auto& cfg = getSourceTypeConfig (profile.sourceType);

    if (! cfg.sibilanceRelevant)
    {
        out.push_back ({ "General", "No aplica",
            juce::String (cfg.label) + " no suele tener sibilancia (\"eses\") — un de-esser probablemente no es relevante aquí." });
        return out;
    }

    if (profile.sibilanceExcessDb <= 0.5f)
    {
        out.push_back ({ "General", "No hace falta de-esser (o muy suave)",
            "Sibilancia dentro de lo normal (" + juce::String (profile.sibilanceLevelDb, 1) + " dB)." });
        return out;
    }

    // Busca si alguna resonancia detectada cae dentro del rango típico de eses (3-10kHz)
    // para centrar el rango del de-esser justo ahí.
    float centerFreq = 6000.0f; // valor por defecto razonable
    bool foundResonanceInRange = false;
    for (int i = 0; i < profile.numResonancesFound; ++i)
    {
        const auto& res = profile.resonances[(size_t) i];
        if (res.frequencyHz >= 3000.0f && res.frequencyHz <= 10000.0f)
        {
            centerFreq = res.frequencyHz;
            foundResonanceInRange = true;
            break;
        }
    }

    float low = juce::jmax (2000.0f, centerFreq - 1800.0f);
    float high = juce::jmin (14000.0f, centerFreq + 2500.0f);

    out.push_back ({ "Inicio eses", juce::String (low, 0) + " Hz",
        foundResonanceInRange ? "Centrado en una resonancia detectada dentro del rango de sibilancia."
                               : "Rango típico ajustado según el exceso de sibilancia medido." });
    out.push_back ({ "Fin eses", juce::String (high, 0) + " Hz", "" });

    // Threshold sugerido: más agresivo cuanto mayor es el exceso detectado
    float suggestedThreshold = -24.0f - juce::jmin (profile.sibilanceExcessDb, 20.0f);
    out.push_back ({ "Threshold", juce::String (suggestedThreshold, 0) + " dB",
        "Exceso de sibilancia: " + juce::String (profile.sibilanceExcessDb, 1) + " dB sobre lo típico." });

    float suggestedRatio = profile.sibilanceExcessDb > 10.0f ? 6.0f : 4.0f;
    out.push_back ({ "Ratio", juce::String (suggestedRatio, 0) + ":1", "" });

    float suggestedMaxCut = juce::jlimit (6.0f, 16.0f, profile.sibilanceExcessDb);
    out.push_back ({ "Max Cut", juce::String (suggestedMaxCut, 0) + " dB", "" });

    return out;
}
