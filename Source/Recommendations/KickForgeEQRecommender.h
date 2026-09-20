#pragma once
#include "PluginRecommender.h"

// Traduce el SoundProfile a sugerencias concretas para las 12 bandas de
// KickForge EQ (ver el proyecto KickForgeEQ, Source/PluginProcessor.h,
// array kDefaultBands, para los nombres/frecuencias exactas de cada banda).
class KickForgeEQRecommender : public PluginRecommender
{
public:
    juce::String getTargetPluginName() const override { return "KickForge EQ"; }
    std::vector<Recommendation> generate (const SoundProfile& profile) const override;
};
