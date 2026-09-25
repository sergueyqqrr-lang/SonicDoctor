#pragma once
#include "../SoundProfile.h"
#include <juce_core/juce_core.h>
#include <vector>

// ============================================================================
// PARA AGREGAR SOPORTE A TU PRÓXIMO PLUGIN:
//
// 1. Crea un archivo nuevo, por ejemplo "MiPluginRecommender.h/.cpp" en esta
//    misma carpeta (Source/Recommendations/).
// 2. Copia la forma de KickForgeEQRecommender o SurgicalDeEsserRecommender:
//    una clase que hereda de PluginRecommender e implementa generate().
// 3. Dentro de generate(), lee lo que necesites de SoundProfile (ver
//    Source/SoundProfile.h para ver todo lo disponible: loudness, bandas
//    espectrales, resonancias, sibilancia, correlación de fase, etc.) y
//    devuelve una lista de Recommendation con el parámetro sugerido.
// 4. Regístralo en RecommendationEngine.cpp (una sola línea, ver ese archivo).
// 5. Agrégalo también a CMakeLists.txt (target_sources).
//
// NO hace falta tocar el motor de análisis (PluginProcessor) para nada de esto.
// ============================================================================

struct Recommendation
{
    juce::String parameterName;   // ej: "Threshold"
    juce::String suggestedValue;  // ej: "-34 dB"  (texto, no un valor crudo — más legible)
    juce::String reason;          // ej: "Sibilancia detectada 6dB por encima de lo típico en 5.8kHz"
};

class PluginRecommender
{
public:
    virtual ~PluginRecommender() = default;

    // Nombre mostrado en la pestaña de la interfaz (ej: "KickForge EQ")
    virtual juce::String getTargetPluginName() const = 0;

    // Analiza el perfil y devuelve una lista de sugerencias concretas.
    // Puede devolver una lista vacía si no encuentra nada que sugerir para ese plugin.
    virtual std::vector<Recommendation> generate (const SoundProfile& profile) const = 0;
};
