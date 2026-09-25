#pragma once
#include "PluginRecommender.h"
#include <memory>

struct PluginRecommendationSet
{
    juce::String pluginName;
    std::vector<Recommendation> recommendations;
};

class RecommendationEngine
{
public:
    RecommendationEngine();

    // Diagnóstico general en texto plano (no depende de ningún plugin específico)
    static std::vector<juce::String> generateGeneralDiagnostics (const SoundProfile& profile);

    // Recomendaciones por cada plugin registrado
    std::vector<PluginRecommendationSet> generateAll (const SoundProfile& profile) const;

private:
    std::vector<std::unique_ptr<PluginRecommender>> recommenders;
};
