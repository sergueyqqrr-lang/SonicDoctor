#include "KickForgeEQRecommender.h"
#include "../SourceTypeProfiles.h"

std::vector<Recommendation> KickForgeEQRecommender::generate (const SoundProfile& profile) const
{
    std::vector<Recommendation> out;
    const auto& cfg = getSourceTypeConfig (profile.sourceType);

    // Banda "Sub Rumble" (30Hz) — ruido de manejo/HVAC. El umbral depende del tipo de
    // fuente: un Kick tiene sub real por diseño, así que es mucho más permisivo que
    // una Voz (donde casi no debería haber nada por debajo de 30Hz).
    if (profile.rumbleRelativeDb > cfg.rumbleThresholdDb)
    {
        out.push_back ({ "Sub Rumble (30Hz)", "Corte -4 a -6 dB (o activar como HPF)",
            "Se detectó energía por debajo de 30Hz (" + juce::String (profile.rumbleLevelDb, 1) + " dB), fuera de lo esperado incluso para este tipo de fuente." });
    }

    // Bandas relacionadas al balance de graves/cuerpo (usa las bandas espectrales generales del perfil)
    const auto& subBand = profile.bands[0];    // Sub 20-60Hz
    const auto& gravesBand = profile.bands[1]; // Graves 60-250Hz
    const auto& lowMidBand = profile.bands[2]; // Low-Mid 250-500Hz

    if (subBand.deviationDb > 4.0f)
        out.push_back ({ "Sub Fundamental (50Hz)", "Corte " + juce::String (-subBand.deviationDb * 0.5f, 1) + " dB",
            "Exceso de energía sub (" + juce::String (subBand.deviationDb, 1) + " dB sobre lo típico para este tipo de fuente)." });

    if (gravesBand.deviationDb > 3.0f)
        out.push_back ({ "Low Body (80Hz) / Upper Body (120Hz)", "Corte suave, prueba -2 a -3 dB",
            "El rango de graves está " + juce::String (gravesBand.deviationDb, 1) + " dB por encima del promedio esperado." });

    if (lowMidBand.deviationDb > 3.0f)
        out.push_back ({ "Boxiness (200Hz) / Low Mid Mud (300Hz)", "Corte -3 a -5 dB",
            "Zona 250-500Hz elevada (" + juce::String (lowMidBand.deviationDb, 1) + " dB): posible sonido \"boxy\" o embarrado." });

    // Resonancias detectadas: sugiere la banda de KickForge más cercana en frecuencia
    for (int i = 0; i < profile.numResonancesFound; ++i)
    {
        const auto& res = profile.resonances[(size_t) i];
        out.push_back ({
            "Banda mas cercana a " + juce::String (res.frequencyHz, 0) + " Hz",
            "Corte -" + juce::String (juce::jmin (res.prominenceDb, 8.0f), 1) + " dB, Q alto (2.5-3.5)",
            "Resonancia detectada: sobresale " + juce::String (res.prominenceDb, 1) + " dB sobre el promedio local."
        });
    }

    return out;
}
