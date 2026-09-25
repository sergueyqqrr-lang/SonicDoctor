#include "RecommendationEngine.h"
#include "KickForgeEQRecommender.h"
#include "SurgicalDeEsserRecommender.h"
#include "../SourceTypeProfiles.h"

RecommendationEngine::RecommendationEngine()
{
    // --- REGISTRO DE RECOMENDADORES ---
    // Cuando hagas un plugin nuevo y crees su recomendador (ver PluginRecommender.h
    // para los pasos), la ÚNICA línea que necesitas agregar aquí es un push_back más:
    recommenders.push_back (std::make_unique<KickForgeEQRecommender>());
    recommenders.push_back (std::make_unique<SurgicalDeEsserRecommender>());
    // recommenders.push_back (std::make_unique<TuNuevoPluginRecommender>());
}

std::vector<PluginRecommendationSet> RecommendationEngine::generateAll (const SoundProfile& profile) const
{
    std::vector<PluginRecommendationSet> out;
    for (auto& r : recommenders)
        out.push_back ({ r->getTargetPluginName(), r->generate (profile) });
    return out;
}

std::vector<juce::String> RecommendationEngine::generateGeneralDiagnostics (const SoundProfile& profile)
{
    std::vector<juce::String> lines;
    const auto& cfg = getSourceTypeConfig (profile.sourceType);

    // --- Loudness (no siempre es informativo para one-shots percusivos aislados) ---
    if (cfg.loudnessCheckRelevant)
    {
        if (profile.shortTermLufs > -8.0f)
            lines.push_back ("Loudness alto (" + juce::String (profile.shortTermLufs, 1) + " LUFS aprox.) — riesgo de fatiga auditiva o distorsión en la cadena posterior.");
        else if (profile.shortTermLufs < -40.0f && profile.shortTermLufs > -99.0f)
            lines.push_back ("Loudness muy bajo (" + juce::String (profile.shortTermLufs, 1) + " LUFS aprox.) — considera normalizar antes de procesar.");
    }

    // --- Dinámica (el umbral de "sobre-comprimido" depende del tipo de fuente:
    //     un Kick o un Bajo suelen comprimirse fuerte a propósito) ---
    if (profile.crestFactorDb < cfg.crestFactorWarnDb && profile.peakDb > -60.0f)
        lines.push_back ("Rango dinámico bajo (crest factor " + juce::String (profile.crestFactorDb, 1) + " dB) — posible sobre-compresión o limitación excesiva.");
    else if (profile.crestFactorDb > 20.0f)
        lines.push_back ("Rango dinámico muy amplio (crest factor " + juce::String (profile.crestFactorDb, 1) + " dB) — puede necesitar compresión para sentarse en una mezcla.");

    // --- Clipping ---
    if (profile.clippingDetected)
        lines.push_back ("Clipping detectado (picos en o cerca de 0dBFS) — baja la ganancia de entrada antes de procesar más.");

    // --- DC offset ---
    if (std::abs (profile.dcOffset) > 0.01f)
        lines.push_back ("Offset de DC detectado (" + juce::String (profile.dcOffset, 4) + ") — considera un filtro DC-blocker antes de otros procesos.");

    // --- Correlación de fase ---
    if (profile.phaseCorrelation < 0.1f)
        lines.push_back ("Correlación de fase baja (" + juce::String (profile.phaseCorrelation, 2) + ") — revisa problemas de fase entre canales, puede sonar hueco en mono.");

    // --- Sibilancia (no aplica a fuentes sin "eses", como Kick o Bajo) ---
    if (cfg.sibilanceRelevant && profile.sibilanceExcessDb > 3.0f)
        lines.push_back ("Sibilancia elevada (" + juce::String (profile.sibilanceExcessDb, 1) + " dB sobre lo típico) — considera un de-esser.");

    // --- Rumble (umbral distinto por tipo: un Kick/Bajo tiene sub real por diseño) ---
    if (profile.rumbleRelativeDb > cfg.rumbleThresholdDb)
        lines.push_back ("Ruido de muy baja frecuencia (<30Hz) presente - esto esta por debajo de lo que produce cualquier instrumento musical, probablemente sea ruido de manejo/HVAC. Considera un high-pass filter.");

    // --- Piso de ruido ---
    if (profile.noiseFloorDb > -50.0f)
        lines.push_back ("Piso de ruido relativamente alto (" + juce::String (profile.noiseFloorDb, 1) + " dB) — revisa la fuente/grabación o considera reducción de ruido.");

    // --- Resonancias ---
    for (int i = 0; i < profile.numResonancesFound; ++i)
    {
        const auto& r = profile.resonances[(size_t) i];
        lines.push_back ("Resonancia en " + juce::String (r.frequencyHz, 0) + " Hz (sobresale " + juce::String (r.prominenceDb, 1) + " dB) — candidata a un corte con EQ.");
    }

    // --- Balance espectral general ---
    for (const auto& band : profile.bands)
    {
        if (std::abs (band.deviationDb) > 5.0f)
        {
            lines.push_back (juce::String (band.name) + ": " + (band.deviationDb > 0 ? "exceso" : "falta")
                              + " de " + juce::String (std::abs (band.deviationDb), 1) + " dB respecto a lo típico para este tipo de fuente.");
        }
    }

    if (lines.empty())
        lines.push_back ("No se detectaron problemas evidentes. El sonido está dentro de parámetros típicos.");

    return lines;
}
