#pragma once
#include "PluginRecommender.h"

class SurgicalDeEsserRecommender : public PluginRecommender
{
public:
    juce::String getTargetPluginName() const override { return "Surgical De-Esser"; }
    std::vector<Recommendation> generate (const SoundProfile& profile) const override;
};
