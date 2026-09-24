#include "PluginEditor.h"

namespace
{
constexpr float kMinScale = 0.5f, kMaxScale = 2.0f;
constexpr int kW = sgui::MainPanel::kWidth, kH = sgui::MainPanel::kHeight;
}

SuperGeminiEditor::SuperGeminiEditor (SuperGeminiProcessor& p)
    : AudioProcessorEditor (p), proc (p), panel (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (panel);
    panel.onScaleChange = [this] (float s) { setUiScale (s); };
    panel.getScale = [this] { return scale; };

    // Stored scale, else the largest of 100 % / what fits the screen.
    float s = static_cast<float> (proc.apvts.state.getProperty ("uiScale", 0.0));
    if (s <= 0.0f)
    {
        s = 1.0f;
        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const auto area = display->userBounds;
            s = juce::jmin (1.0f, 0.92f * static_cast<float> (area.getWidth()) / static_cast<float> (kW),
                                  0.88f * static_cast<float> (area.getHeight()) / static_cast<float> (kH));
        }
    }
    scale = juce::jlimit (kMinScale, kMaxScale, s);

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (kW * kMinScale), juce::roundToInt (kH * kMinScale),
                     juce::roundToInt (kW * kMaxScale), juce::roundToInt (kH * kMaxScale));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (static_cast<double> (kW) / static_cast<double> (kH));
    setSize (juce::roundToInt (kW * scale), juce::roundToInt (kH * scale));
}

SuperGeminiEditor::~SuperGeminiEditor()
{
    setLookAndFeel (nullptr);
}

void SuperGeminiEditor::setUiScale (float s)
{
    s = juce::jlimit (kMinScale, kMaxScale, s);
    setSize (juce::roundToInt (kW * s), juce::roundToInt (kH * s));
}

void SuperGeminiEditor::resized()
{
    scale = juce::jlimit (kMinScale, kMaxScale, static_cast<float> (getWidth()) / static_cast<float> (kW));
    panel.setTransform (juce::AffineTransform::scale (scale));
    panel.setBounds (0, 0, kW, kH);
    proc.apvts.state.setProperty ("uiScale", scale, nullptr);
}

void SuperGeminiEditor::paint (juce::Graphics& g)
{
    g.fillAll (sgui::col::panel);
}
