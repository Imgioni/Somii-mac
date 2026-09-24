#pragma once

// The plugin window: the 1774 × 887 panel, freely resizable 50 %–200 % with a locked aspect
// ratio (build prompt §1, DD-68). The scale is remembered with the project.

#include "PluginProcessor.h"
#include "ui/Panel.h"

class SuperGeminiEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SuperGeminiEditor (SuperGeminiProcessor& p);
    ~SuperGeminiEditor() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

    sgui::MainPanel& getPanel() noexcept { return panel; }
    void setUiScale (float scale);
    float getUiScale() const noexcept { return scale; }

private:
    SuperGeminiProcessor& proc;
    sgui::SgLookAndFeel lookAndFeel;   // outlives the panel
    sgui::MainPanel panel;
    juce::TooltipWindow tooltips { this, 600 };
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuperGeminiEditor)
};
