#include "Theme.h"

#include "Artwork.h"

namespace sgui
{

namespace
{
struct FaceChoice
{
    juce::String family;
    float horizontalScale = 1.0f;
};

const FaceChoice& face()
{
    static const FaceChoice choice = []
    {
        const auto names = juce::Font::findAllTypefaceNames();
        for (const char* n : { "Barlow Condensed", "Bahnschrift SemiCondensed", "Bahnschrift Condensed", "DIN Condensed",
                               "Roboto Condensed", "Arial Narrow" })
            if (names.contains (n)) return FaceChoice { n, 1.0f };
        for (const char* n : { "Bahnschrift", "Helvetica Neue", "Segoe UI", "Arial" })
            if (names.contains (n)) return FaceChoice { n, 0.86f };
        return FaceChoice { {}, 0.86f };
    }();
    return choice;
}

// Brushed metal: horizontal streaks of slightly different brightness plus fine grain.
const juce::Image& brushed()
{
    static const juce::Image image = []
    {
        juce::Image im (juce::Image::ARGB, 256, 256, true);
        juce::Random rnd (20260911);
        for (int y = 0; y < 256; ++y)
        {
            const float row = (rnd.nextFloat() - 0.5f) * 0.05f;              // streak
            for (int x = 0; x < 256; ++x)
            {
                const float v = row + (rnd.nextFloat() - 0.5f) * 0.035f;     // grain
                im.setPixelAt (x, y, (v > 0.0f ? juce::Colours::white : juce::Colours::black).withAlpha (std::abs (v)));
            }
        }
        return im;
    }();
    return image;
}

void softShadow (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Point<float> offset, float spread, float alpha)
{
    constexpr int layers = 4;
    for (int i = layers; i >= 1; --i)
    {
        const float s = spread * static_cast<float> (i) / static_cast<float> (layers);
        g.setColour (juce::Colour (0xff2b3138).withAlpha (alpha / static_cast<float> (layers)));
        g.fillRoundedRectangle (r.translated (offset.x, offset.y).expanded (s), corner + s);
    }
}

void softShadowEllipse (juce::Graphics& g, juce::Rectangle<float> r, juce::Point<float> offset, float spread, float alpha)
{
    constexpr int layers = 4;
    for (int i = layers; i >= 1; --i)
    {
        const float s = spread * static_cast<float> (i) / static_cast<float> (layers);
        g.setColour (juce::Colour (0xff2b3138).withAlpha (alpha / static_cast<float> (layers)));
        g.fillEllipse (r.translated (offset.x, offset.y).expanded (s));
    }
}
} // namespace

juce::Font uiFont (float height, bool bold)
{
    const auto& f = face();
    const int style = bold ? juce::Font::bold : juce::Font::plain;
    auto options = f.family.isNotEmpty() ? juce::FontOptions (f.family, height, style) : juce::FontOptions (height, style);
    return juce::Font (options.withHorizontalScale (f.horizontalScale));
}

void drawText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
               juce::Justification just, juce::Colour colour, bool bold, int maxLines)
{
    g.setColour (colour);
    g.setFont (uiFont (height, bold));
    g.drawFittedText (text, area.toNearestInt(), just, maxLines, 0.7f);
}

void drawSubLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, bool active, bool onDark)
{
    const auto font = uiFont (area.getHeight() * 0.86f, true);
    const float w = juce::jmin (area.getWidth(), juce::GlyphArrangement::getStringWidth (font, text) + 6.0f);
    auto box = area.withSizeKeepingCentre (w, area.getHeight());
    g.setColour (active ? col::accent : (onDark ? col::darkText : col::text));
    g.fillRoundedRectangle (box, 1.5f);
    g.setColour (active ? juce::Colours::white : (onDark ? col::darkPanel : col::panel));
    g.setFont (font);
    g.drawText (text, box, juce::Justification::centred, false);
}

void drawLed (juce::Graphics& g, juce::Point<float> c, float r, float level, juce::Colour colour)
{
    level = juce::jlimit (0.0f, 1.0f, level);
    if (level > 0.02f)
    {
        juce::ColourGradient glow (colour.withAlpha (0.5f * level), c, colour.withAlpha (0.0f), c.translated (r * 3.0f, 0.0f), true);
        g.setGradientFill (glow);
        g.fillEllipse (juce::Rectangle<float> (r * 6.0f, r * 6.0f).withCentre (c));
    }
    const auto body = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.35f));                       // bezel ring
    g.drawEllipse (body.expanded (1.0f), 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawEllipse (body.expanded (1.0f).translated (0.0f, 0.7f), 0.8f);
    const auto off = colour.withSaturation (colour.getSaturation() * 0.4f).withBrightness (0.3f);
    juce::ColourGradient lens (off.interpolatedWith (colour.brighter (0.45f), level), c.x - r * 0.35f, c.y - r * 0.45f,
                               off.interpolatedWith (colour.darker (0.15f), level), c.x + r * 0.8f, c.y + r * 0.9f, true);
    g.setGradientFill (lens);
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha (0.3f * level + 0.13f));        // specular
    g.fillEllipse (body.reduced (r * 0.55f).translated (-r * 0.22f, -r * 0.28f));
}

// ── materials ───────────────────────────────────────────────────────────────────────────────

void drawMetal (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, float lightAmount)
{
    g.setGradientFill (juce::ColourGradient (base.brighter (0.035f), r.getX(), r.getY(), base.darker (0.05f), r.getX(), r.getBottom(), false));
    g.fillRect (r);
    // The photographed bead-blasted surface, mirror-tiled so its edges never show (DD-74). The
    // grain is fine and even enough that the mirroring is invisible.
    if (const auto& tile = art::panelTile(); tile.isValid())
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (r.toNearestInt());
        g.setOpacity (0.5f);
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        const int tw = 360, th = 360;
        for (int ty = 0; ty * th < juce::roundToInt (r.getHeight()) + th; ++ty)
            for (int tx = 0; tx * tw < juce::roundToInt (r.getWidth()) + tw; ++tx)
            {
                auto t = juce::AffineTransform::scale (static_cast<float> (tw) / static_cast<float> (tile.getWidth()) * ((tx % 2) ? -1.0f : 1.0f),
                                                       static_cast<float> (th) / static_cast<float> (tile.getHeight()) * ((ty % 2) ? -1.0f : 1.0f));
                t = t.translated (r.getX() + static_cast<float> (tx * tw) + ((tx % 2) ? static_cast<float> (tw) : 0.0f),
                                  r.getY() + static_cast<float> (ty * th) + ((ty % 2) ? static_cast<float> (th) : 0.0f));
                g.drawImageTransformed (tile, t);
            }
    }
    else
    {
        g.setTiledImageFill (brushed(), 0, 0, 1.0f);
        g.fillRect (r);
    }
    if (lightAmount > 0.0f)
    {
        juce::ColourGradient light (juce::Colours::white.withAlpha (0.11f * lightAmount), r.getX() + r.getWidth() * 0.18f, r.getY(),
                                    juce::Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.75f, r.getBottom() * 0.95f, true);
        g.setGradientFill (light);
        g.fillRect (r);
    }
}

void drawEngravedFrame (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawRoundedRectangle (r.translated (0.0f, 1.0f), corner, 1.0f);
    g.setColour (col::engrave.withAlpha (0.75f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void drawRecess (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour fill)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::black.withAlpha (0.13f));
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.0f), r.getX() + 2.0f, r.getRight() - 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.07f));
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 2.0f), r.getX() + 2.0f, r.getRight() - 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawHorizontalLine (juce::roundToInt (r.getBottom() - 1.0f), r.getX() + 2.0f, r.getRight() - 2.0f);
    g.setColour (col::engrave.withAlpha (0.55f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 0.8f);
}

void drawDarkPanel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::white.withAlpha (0.5f));                        // lip of the cut-out
    g.drawRoundedRectangle (r.translated (0.0f, 1.2f), 3.5f, 1.2f);
    g.setGradientFill (juce::ColourGradient (col::darkPanel.brighter (0.09f), r.getX(), r.getY(), col::darkPanel.darker (0.16f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setTiledImageFill (brushed(), 0, 0, 1.0f);
    g.fillRoundedRectangle (r, 3.0f);
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.05f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.0f),
                                r.getX() + r.getWidth() * 0.5f, r.getY() + r.getHeight() * 0.6f, true);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));                        // inner shadow, top
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.0f), r.getX() + 3.0f, r.getRight() - 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}

void drawScrew (juce::Graphics& g, juce::Point<float> c, float radius)
{
    const auto b = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.fillEllipse (b.expanded (0.8f).translated (0.4f, 0.9f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2f3), b.getX(), b.getY(), juce::Colour (0xff8c8e91), b.getRight(), b.getBottom(), false));
    g.fillEllipse (b);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.drawEllipse (b, 0.6f);
    g.setColour (juce::Colour (0xff5c5e61));
    const float a = radius * 0.6f;
    g.drawLine (c.x - a, c.y + a * 0.3f, c.x + a, c.y - a * 0.3f, 1.0f);
}

void drawCheek (juce::Graphics& g, juce::Rectangle<float> r, bool left)
{
    // The photographed cheek, stretched down the body (DD-74); mirrored on the right so the light
    // falls the same way on both.
    if (const auto& photo = art::cheek(); photo.isValid())
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (1.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        if (left)
        {
            art::drawNineSlice (g, photo, r, 0.12f);
        }
        else
        {
            g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (r.getX() + r.getRight(), 0.0f));
            art::drawNineSlice (g, photo, r, 0.12f);
        }
        return;
    }

    // Dark anodised end cheek: the outer edge rolls away from the light, the inner edge catches it.
    const float outer = left ? r.getX() : r.getRight();
    const float inner = left ? r.getRight() : r.getX();
    const juce::Colour base (0xff2e3033);
    juce::ColourGradient grad (base.darker (0.75f), outer, r.getY(), base.brighter (0.30f), inner, r.getY(), false);
    grad.addColour (0.22, base.darker (0.35f));
    grad.addColour (0.70, base.brighter (0.10f));
    g.setGradientFill (grad);
    juce::Path p;
    p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 9.0f, 9.0f, left, ! left, left, ! left);
    g.fillPath (p);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (p);
        g.setTiledImageFill (brushed(), 0, 0, 0.9f);
        g.fillRect (r);
        // shallow fluting, as milled into the cheeks
        for (float x = r.getX() + 11.0f; x < r.getRight() - 7.0f; x += 11.0f)
        {
            juce::ColourGradient flute (juce::Colours::black.withAlpha (0.16f), x - 3.0f, r.getCentreY(),
                                        juce::Colours::white.withAlpha (0.05f), x + 3.0f, r.getCentreY(), false);
            flute.addColour (0.5, juce::Colours::transparentBlack);
            g.setGradientFill (flute);
            g.fillRect (juce::Rectangle<float> (x - 3.0f, r.getY() + 4.0f, 6.0f, r.getHeight() - 8.0f));
        }
        // the panel above throws light onto the top of the cheek
        juce::ColourGradient top (juce::Colours::white.withAlpha (0.16f), r.getCentreX(), r.getY(),
                                  juce::Colours::transparentBlack, r.getCentreX(), r.getY() + 120.0f, false);
        g.setGradientFill (top);
        g.fillRect (r.withHeight (120.0f));
        juce::ColourGradient bot (juce::Colours::black.withAlpha (0.45f), r.getCentreX(), r.getBottom(),
                                  juce::Colours::transparentBlack, r.getCentreX(), r.getBottom() - 150.0f, false);
        g.setGradientFill (bot);
        g.fillRect (r.withTop (r.getBottom() - 150.0f));
    }
    g.setColour (juce::Colours::white.withAlpha (0.30f));                       // top edge catch-light
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 0.8f), r.getX() + (left ? 7.0f : 1.0f), r.getRight() - (left ? 1.0f : 7.0f));
    g.setColour (juce::Colours::white.withAlpha (0.14f));                       // inner edge against the panel
    g.drawVerticalLine (juce::roundToInt (inner + (left ? -1.0f : 1.0f)), r.getY(), r.getBottom());
    g.setColour (juce::Colours::black.withAlpha (0.55f));                       // seam
    g.drawVerticalLine (juce::roundToInt (inner + (left ? 0.0f : 0.0f)), r.getY(), r.getBottom());
}

void drawLip (juce::Graphics& g, juce::Rectangle<float> r)
{
    // The panel rolls over into the keybed: highlight along the crest, shade underneath.
    juce::ColourGradient grad (col::panel.brighter (0.1f), r.getX(), r.getY(), juce::Colour (0xff56585b), r.getX(), r.getBottom(), false);
    grad.addColour (0.16, juce::Colours::white.withAlpha (0.95f).interpolatedWith (col::panel, 0.25f));
    grad.addColour (0.45, col::panel.darker (0.06f));
    grad.addColour (0.78, col::panel.darker (0.42f));
    g.setGradientFill (grad);
    g.fillRect (r);
    g.setTiledImageFill (brushed(), 0, 0, 0.5f);
    g.fillRect (r);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (r.withTop (r.getBottom() - 2.0f));
}

void drawKeybed (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff121314), r.getX(), r.getY(), juce::Colour (0xff26272a), r.getX(), r.getBottom(), false));
    g.fillRect (r);
    juce::ColourGradient sh (juce::Colours::black.withAlpha (0.65f), r.getX(), r.getY(), juce::Colours::black.withAlpha (0.0f), r.getX(), r.getY() + 14.0f, false);
    g.setGradientFill (sh);
    g.fillRect (r.withHeight (14.0f));
}

// ── controls ────────────────────────────────────────────────────────────────────────────────

namespace
{
// Live modulation: an arc from where the knob is set to where modulation has pushed it.
void drawKnobModulation (juce::Graphics& g, juce::Point<float> c, float R, float norm, float modNorm, float start, float end)
{
    if (modNorm < 0.0f || std::abs (modNorm - norm) <= 0.004f) return;
    const float angle = start + (end - start) * juce::jlimit (0.0f, 1.0f, norm);
    const float modAngle = start + (end - start) * juce::jlimit (0.0f, 1.0f, modNorm);
    const float ring = R + 1.6f;
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, ring, ring, 0.0f, angle, modAngle, true);
    g.setColour (col::accent.withAlpha (0.35f));
    g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::accent);
    g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillEllipse (juce::Rectangle<float> (3.4f, 3.4f).withCentre (c.getPointOnCircumference (ring, modAngle)));
}
} // namespace

void drawHwKnob (juce::Graphics& g, juce::Rectangle<float> circle, float norm, Tone tone, int steps, bool bipolar,
                 bool onDark, bool enabled, float modNorm)
{
    constexpr float start = -2.618f, end = 2.618f;   // ±150°
    const auto c = circle.getCentre();
    const float R = circle.getWidth() * 0.5f;

    // printed scale
    const int n = steps > 1 && steps <= 12 ? steps : 11;
    g.setColour ((onDark ? juce::Colour (0xffc5c6c9) : col::tick).withMultipliedAlpha (enabled ? 1.0f : 0.5f));
    for (int i = 0; i < n; ++i)
    {
        const float a = start + (end - start) * static_cast<float> (i) / static_cast<float> (n - 1);
        const bool major = steps > 1 || i == 0 || i == n - 1 || i == n / 2;
        g.drawLine ({ c.getPointOnCircumference (R + 2.6f, a), c.getPointOnCircumference (R + (major ? 6.2f : 4.4f), a) }, major ? 1.2f : 0.8f);
    }
    if (bipolar)
        g.fillEllipse (juce::Rectangle<float> (2.6f, 2.6f).withCentre (c.getPointOnCircumference (R + 8.4f, 0.0f)));

    juce::Colour skirtTop, skirtBottom, hi, base, shade, pointer;
    switch (tone)
    {
        case Tone::White:
            skirtTop = juce::Colour (0xffe3e4e3); skirtBottom = juce::Colour (0xffa8a9aa);
            hi = juce::Colours::white; base = juce::Colour (0xfff2f2f0); shade = juce::Colour (0xffc2c3c2); pointer = juce::Colour (0xff303133);
            break;
        case Tone::Orange:
            skirtTop = juce::Colour (0xffe4521e); skirtBottom = juce::Colour (0xff9d3110);
            hi = juce::Colour (0xffffa079); base = col::accent; shade = juce::Colour (0xffbe3f18); pointer = juce::Colour (0xfffff4ed);
            break;
        case Tone::Black:
            skirtTop = juce::Colour (0xff3e3f42); skirtBottom = juce::Colour (0xff0f1011);
            hi = juce::Colour (0xff6b6c70); base = juce::Colour (0xff272829); shade = juce::Colour (0xff0d0e0f); pointer = juce::Colour (0xfff6f6f6);
            break;
    }

    // Photographed knob (DD-74): the body is fixed, so its highlight stays where the light is, and
    // only the pointer sweeps — around the ellipse the tilted top face makes on screen.
    const auto& photo = art::knob (tone);
    if (photo.isValid())
    {
        const float aw = static_cast<float> (photo.getWidth()), ah = static_cast<float> (photo.getHeight());
        const float s = circle.getWidth() / aw;                       // the body spans the knob's circle
        const juce::Rectangle<float> dest (c.x - art::kKnobFaceCx * aw * s, c.y - art::kKnobFaceCy * ah * s, aw * s, ah * s);

        softShadowEllipse (g, circle, { R * 0.09f, R * 0.2f }, R * 0.26f, onDark ? 0.5f : 0.33f);
        {
            juce::Graphics::ScopedSaveState state (g);
            g.setOpacity (1.0f);                      // drawImage is modulated by the current fill
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (photo, dest);
        }

        const float angle = start + (end - start) * juce::jlimit (0.0f, 1.0f, norm);
        const float prx = art::kKnobFaceRx * aw * s, pry = art::kKnobFaceRy * ah * s;
        auto onFace = [&] (float k) { return juce::Point<float> (c.x + prx * k * std::sin (angle), c.y - pry * k * std::cos (angle)); };
        juce::Path ptr;
        ptr.startNewSubPath (onFace (0.18f));
        ptr.lineTo (onFace (0.88f));
        const float thickness = juce::jmax (1.6f, R * 0.115f);
        if (tone != Tone::Black)                                      // seated in the surface
        {
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            g.strokePath (ptr, juce::PathStrokeType (thickness + 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (pointer);
        g.strokePath (ptr, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        drawKnobModulation (g, c, R, norm, modNorm, start, end);
        if (! enabled)
        {
            g.setColour ((onDark ? col::darkPanel : col::panel).withAlpha (0.5f));
            g.fillRect (dest.expanded (1.0f));
        }
        return;
    }

    softShadowEllipse (g, circle, { R * 0.09f, R * 0.17f }, R * 0.26f, onDark ? 0.5f : 0.33f);

    // skirt
    g.setGradientFill (juce::ColourGradient (skirtTop, c.x, c.y - R, skirtBottom, c.x, c.y + R, false));
    g.fillEllipse (circle);
    {   // fine knurl around the rim
        const int teeth = juce::jmax (26, juce::roundToInt (R * 2.4f));
        g.setColour ((tone == Tone::Black ? juce::Colours::white : juce::Colours::black).withAlpha (tone == Tone::Black ? 0.09f : 0.07f));
        for (int i = 0; i < teeth; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (teeth);
            g.drawLine ({ c.getPointOnCircumference (R * 0.87f, a), c.getPointOnCircumference (R * 0.99f, a) }, 0.7f);
        }
    }
    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.drawEllipse (circle.reduced (0.4f), 0.9f);

    // cap
    const auto cap = circle.reduced (R * 0.16f);
    juce::ColourGradient body (hi, c.x - R * 0.42f, c.y - R * 0.5f, shade, c.x + R * 0.55f, c.y + R * 0.72f, true);
    body.addColour (0.44, base);
    g.setGradientFill (body);
    g.fillEllipse (cap);
    g.setColour (shade.darker (0.35f).withAlpha (0.6f));
    g.drawEllipse (cap, 0.9f);
    juce::Path rim;
    rim.addCentredArc (c.x, c.y, cap.getWidth() * 0.5f - 0.9f, cap.getHeight() * 0.5f - 0.9f, 0.0f, -2.35f, 0.5f, true);
    g.setColour (juce::Colours::white.withAlpha (tone == Tone::Black ? 0.16f : 0.6f));
    g.strokePath (rim, juce::PathStrokeType (1.1f));

    // pointer
    const float angle = start + (end - start) * juce::jlimit (0.0f, 1.0f, norm);
    juce::Path ptr;
    ptr.startNewSubPath (c.getPointOnCircumference (R * 0.24f, angle));
    ptr.lineTo (c.getPointOnCircumference (R * 0.82f, angle));
    g.setColour (pointer);
    g.strokePath (ptr, juce::PathStrokeType (juce::jmax (1.7f, R * 0.13f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    drawKnobModulation (g, c, R, norm, modNorm, start, end);

    if (! enabled)
    {
        g.setColour ((onDark ? col::darkPanel : col::panel).withAlpha (0.5f));
        g.fillEllipse (circle.expanded (1.0f));
    }
}

void drawHwFader (juce::Graphics& g, juce::Rectangle<float> slot, float capY, float capWidth, Tone tone, bool scale,
                  bool bipolar, bool onDark, bool enabled, float modY)
{
    constexpr float capH = 11.0f;
    const float top = slot.getY() + capH * 0.5f, bottom = slot.getBottom() - capH * 0.5f;
    const auto tickColour = (onDark ? juce::Colour (0xffb6b7ba) : col::tick).withMultipliedAlpha (enabled ? 1.0f : 0.5f);
    if (scale)
    {
        g.setColour (tickColour);
        for (int i = 0; i <= 10; ++i)
        {
            const float y = bottom - (bottom - top) * static_cast<float> (i) / 10.0f;
            const float len = (i % 5 == 0) ? 6.5f : 3.5f;
            g.fillRect (juce::Rectangle<float> (slot.getX() - 4.0f - len, y - 0.5f, len, 1.0f));
        }
    }
    if (bipolar)
    {
        g.setColour (tickColour);
        g.fillRect (juce::Rectangle<float> (slot.getX() - 11.0f, slot.getCentreY() - 0.6f, 22.0f + slot.getWidth(), 1.2f));
    }

    // recessed slot — the photographed track if it is there (DD-74)
    if (const auto& track = art::faderTrack(); track.isValid())
    {
        art::drawNineSlice (g, track, slot.expanded (slot.getWidth() * 0.55f, 3.0f), 0.30f);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (onDark ? 0.09f : 0.62f));
        g.fillRoundedRectangle (slot.translated (0.0f, 1.0f).expanded (0.9f, 0.6f), slot.getWidth() * 0.5f);
        juce::ColourGradient well (juce::Colour (0xff030303), slot.getX(), slot.getY(), juce::Colour (0xff232426), slot.getRight(), slot.getY(), false);
        g.setGradientFill (well);
        g.fillRoundedRectangle (slot, slot.getWidth() * 0.5f);
    }

    // live modulation marker
    if (modY >= 0.0f && std::abs (modY - capY) > 1.0f)
    {
        g.setColour (col::accent.withAlpha (0.4f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (slot.getX() - 0.5f, juce::jmin (modY, capY), slot.getRight() + 0.5f, juce::jmax (modY, capY)));
        juce::Path tri;
        tri.addTriangle (slot.getRight() + 8.0f, modY, slot.getRight() + 2.5f, modY - 3.2f, slot.getRight() + 2.5f, modY + 3.2f);
        g.setColour (col::accent);
        g.fillPath (tri);
    }

    // cap
    const auto cap = juce::Rectangle<float> (capWidth, capH).withCentre ({ slot.getCentreX(), capY });
    softShadow (g, cap, 2.0f, { 1.1f, 2.4f }, 2.2f, onDark ? 0.58f : 0.36f);

    if (const auto& photo = art::faderCap (tone); photo.isValid())
    {
        const float aspect = static_cast<float> (photo.getHeight()) / static_cast<float> (photo.getWidth());
        const auto dest = juce::Rectangle<float> (capWidth * 1.5f, capWidth * 1.5f * aspect).withCentre ({ slot.getCentreX(), capY });
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (1.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (photo, dest);
        if (! enabled)
        {
            g.setColour ((onDark ? col::darkPanel : col::panel).withAlpha (0.55f));
            g.fillRect (dest);
        }
        return;
    }

    juce::Colour top1, mid, bot, groove;
    switch (tone)
    {
        case Tone::White:  top1 = juce::Colours::white;      mid = juce::Colour (0xffe9e9e7); bot = juce::Colour (0xffb2b3b4); groove = juce::Colour (0xff4e5053); break;
        case Tone::Orange: top1 = juce::Colour (0xffff9d72); mid = col::accent;              bot = juce::Colour (0xffaf3a15); groove = juce::Colour (0xff671f08); break;
        case Tone::Black:  top1 = juce::Colour (0xff5e5f62); mid = juce::Colour (0xff2b2c2e); bot = juce::Colour (0xff141516); groove = col::accent;            break;
    }
    juce::ColourGradient body (top1, cap.getX(), cap.getY(), bot, cap.getX(), cap.getBottom(), false);
    body.addColour (0.4, mid);
    g.setGradientFill (body);
    g.fillRoundedRectangle (cap, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (cap.reduced (0.4f), 2.0f, 0.8f);
    g.setColour (juce::Colours::white.withAlpha (tone == Tone::Black ? 0.22f : 0.8f));
    g.drawHorizontalLine (juce::roundToInt (cap.getY() + 1.0f), cap.getX() + 2.0f, cap.getRight() - 2.0f);
    g.setColour (groove);
    g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 4.0f, 1.5f));
    g.setColour (juce::Colours::white.withAlpha (tone == Tone::Black ? 0.12f : 0.38f));
    g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 4.0f, 1.0f).translated (0.0f, 1.5f));

    if (! enabled)
    {
        g.setColour ((onDark ? col::darkPanel : col::panel).withAlpha (0.55f));
        g.fillRect (cap.expanded (1.0f));
    }
}

namespace
{
// The lamp window in the top of a panel push-key.
void drawKeyLed (juce::Graphics& g, juce::Rectangle<float> r, bool lit, bool enabled)
{
    const float w = juce::jlimit (7.0f, 13.0f, r.getWidth() * 0.4f);
    const auto window = juce::Rectangle<float> (w, 3.4f).withCentre ({ r.getCentreX(), r.getY() + 5.4f });
    if (lit && enabled)
    {
        juce::ColourGradient glow (col::accent.withAlpha (0.55f), window.getCentre(), col::accent.withAlpha (0.0f),
                                   window.getCentre().translated (w * 0.9f, 0.0f), true);
        g.setGradientFill (glow);
        g.fillEllipse (window.expanded (w * 0.55f, 6.0f));
    }
    g.setColour (lit && enabled ? juce::Colour (0xffff5030) : juce::Colour (0xff43231b).withAlpha (enabled ? 1.0f : 0.5f));
    g.fillRoundedRectangle (window, 1.4f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (window, 1.4f, 0.6f);
    if (lit && enabled)
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillRect (window.withHeight (1.0f).reduced (1.5f, 0.0f));
    }
}
} // namespace

void drawKey (juce::Graphics& g, juce::Rectangle<float> r, bool lit, bool enabled, bool hover, bool pressed)
{
    // Panel push-key with an LED window, as on the instrument.
    const bool down = lit || pressed;
    softShadow (g, r, 2.5f, { 0.6f, down ? 1.0f : 2.0f }, down ? 1.0f : 1.8f, enabled ? 0.26f : 0.1f);

    // Photographed key face (DD-74), nine-sliced so its moulded corners keep their shape.
    if (const auto& face = art::button (false, down); face.isValid())
    {
        art::drawNineSlice (g, face, r, 0.34f);
        if (hover && enabled)
        {
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRoundedRectangle (r.reduced (1.5f), 2.5f);
        }
        drawKeyLed (g, r, lit, enabled);
        if (! enabled)
        {
            g.setColour (col::panel.withAlpha (0.45f));
            g.fillRoundedRectangle (r, 2.5f);
        }
        return;
    }

    juce::Colour top, bottom, edge;
    if (down) { top = juce::Colour (0xffe6e7e7); bottom = juce::Colour (0xffc6c7c9); edge = juce::Colour (0xff85878a); }
    else      { top = juce::Colour (0xfffcfcfb); bottom = juce::Colour (0xffd5d6d7); edge = juce::Colour (0xff8d8f92); }
    if (hover && enabled) { top = top.brighter (0.04f); bottom = bottom.brighter (0.03f); }
    if (! enabled)
    {
        top = top.interpolatedWith (col::panel, 0.5f);
        bottom = bottom.interpolatedWith (col::panel, 0.5f);
        edge = edge.interpolatedWith (col::panel, 0.45f);
    }
    g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (down ? 0.4f : 0.9f));
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.3f), r.getX() + 2.5f, r.getRight() - 2.5f);
    g.setColour (juce::Colours::black.withAlpha (down ? 0.16f : 0.07f));
    g.drawHorizontalLine (juce::roundToInt (r.getBottom() - 2.0f), r.getX() + 2.5f, r.getRight() - 2.5f);

    drawKeyLed (g, r, lit, enabled);
}

void drawKeyLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, bool lit, bool enabled)
{
    drawText (g, text, r.withTrimmedTop (7.0f).reduced (2.0f, 0.0f), juce::jmin (10.0f, (r.getHeight() - 7.0f) * 0.72f),
              juce::Justification::centred, enabled ? col::text : col::dim, lit);
}

void drawButtonFace (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool enabled, bool hover)
{
    juce::Colour base = on ? col::accent : col::buttonOff;
    if (! enabled) base = base.interpolatedWith (col::section, 0.55f);
    else if (hover) base = base.brighter (0.06f);
    softShadow (g, r, 2.5f, { 0.4f, 1.0f }, 1.2f, 0.2f);
    g.setGradientFill (juce::ColourGradient (base.brighter (0.09f), r.getX(), r.getY(), base.darker (0.09f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (on ? col::accent.darker (0.4f) : col::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.3f : 0.75f));
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.2f), r.getX() + 2.0f, r.getRight() - 2.0f);
}

void drawSectionFrame (juce::Graphics& g, juce::Rectangle<float> r)
{
    softShadow (g, r, 4.0f, { 0.8f, 2.4f }, 5.0f, 0.26f);
    g.setGradientFill (juce::ColourGradient (col::section.brighter (0.035f), r.getX(), r.getY(), col::section.darker (0.035f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setTiledImageFill (brushed(), 0, 0, 0.7f);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.5f), r.getX() + 4.0f, r.getRight() - 4.0f);
}

void drawInsetBox (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawRecess (g, r, 3.0f, col::panel.darker (0.035f));
}

void drawDarkBox (juce::Graphics& g, juce::Rectangle<float> r, bool enabled)
{
    g.setColour (juce::Colours::white.withAlpha (0.62f));
    g.drawRoundedRectangle (r.translated (0.0f, 0.9f), 3.0f, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff191a1c), r.getX(), r.getY(), juce::Colour (0xff2d2e31), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.0f), r.getX() + 2.0f, r.getRight() - 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 1.0f);
    if (! enabled)
    {
        g.setColour (col::panel.withAlpha (0.45f));
        g.fillRoundedRectangle (r, 2.5f);
    }
}

// ── screens ─────────────────────────────────────────────────────────────────────────────────

void drawScreen (juce::Graphics& g, juce::Rectangle<float> r)
{
    // Frosted glass panel (DD-76): the live trace is drawn inside it, so the interior stays clear.
    if (const auto& panel = art::glass(); panel.isValid())
    {
        softShadow (g, r, 6.0f, { 0.6f, 1.8f }, 4.0f, 0.18f);
        art::drawNineSlice (g, panel, r, 0.30f);
        // Smoke the interior so the lit trace reads against it, leaving the bevel clear.
        const auto inner = r.reduced (juce::jmin (4.0f, r.getHeight() * 0.10f));
        juce::ColourGradient smoke (juce::Colour (0xff313b49).withAlpha (0.88f), inner.getX(), inner.getY(),
                                    juce::Colour (0xff141b24).withAlpha (0.94f), inner.getX(), inner.getBottom(), false);
        g.setGradientFill (smoke);
        g.fillRoundedRectangle (inner, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawHorizontalLine (juce::roundToInt (inner.getY() + 0.5f), inner.getX() + 2.0f, inner.getRight() - 2.0f);
        return;
    }

    // black bezel
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.drawRoundedRectangle (r.translated (0.0f, 1.0f), 4.5f, 1.2f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2c2e), r.getX(), r.getY(), juce::Colour (0xff141516), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    // glass
    const auto inner = r.reduced (3.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff101112), inner.getX(), inner.getY(), col::screen, inner.getX(), inner.getBottom(), false));
    g.fillRoundedRectangle (inner, 2.0f);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (inner.toNearestInt());
        g.setColour (juce::Colours::white.withAlpha (0.016f));
        for (float y = inner.getY() + 1.0f; y < inner.getBottom(); y += 2.0f)
            g.drawHorizontalLine (juce::roundToInt (y), inner.getX(), inner.getRight());
    }
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (inner.reduced (0.5f), 2.0f, 1.0f);
}

void drawScreenGlass (juce::Graphics& g, juce::Rectangle<float> r)
{
    // The photographed panel already carries its own gloss; only the drawn fallback needs this.
    const float amount = art::glass().isValid() ? 0.018f : 0.055f;
    juce::ColourGradient glass (juce::Colours::white.withAlpha (amount), r.getX(), r.getY(),
                                juce::Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.45f, r.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.6f), 2.0f);
}

void drawLcd (juce::Graphics& g, juce::Rectangle<float> r)
{
    if (const auto& panel = art::glass(); panel.isValid())
    {
        softShadow (g, r, 5.0f, { 0.5f, 1.4f }, 3.0f, 0.16f);
        art::drawNineSlice (g, panel, r, 0.30f);
        return;
    }
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawRoundedRectangle (r.translated (0.0f, 1.0f), 3.5f, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff151617), r.getX(), r.getY(), col::screen, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.toNearestInt());
        g.setColour (juce::Colours::white.withAlpha (0.018f));
        for (float y = r.getY() + 1.0f; y < r.getBottom(); y += 2.0f) g.drawHorizontalLine (juce::roundToInt (y), r.getX(), r.getRight());
        juce::ColourGradient glass (juce::Colours::white.withAlpha (0.06f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.0f),
                                    r.getX() + r.getWidth() * 0.35f, r.getCentreY(), false);
        g.setGradientFill (glass);
        g.fillRect (r.withHeight (r.getHeight() * 0.55f));
    }
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.2f);
}

void drawLcdText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float height, juce::Justification just, float brightness)
{
    // Legends are printed on the glass: a soft light edge under a cool grey face.
    g.setFont (uiFont (height, true));
    g.setColour (juce::Colours::black.withAlpha (0.45f * brightness));
    g.drawText (text, r.translated (0.0f, 0.9f), just, true);
    g.setColour (col::glassText.withAlpha (0.5f + 0.5f * brightness));
    g.drawText (text, r, just, true);
}

void strokeTrace (juce::Graphics& g, const juce::Path& path, float alpha, float thickness)
{
    // A lit filament under the glass: a wide cool bloom, then a bright white core.
    g.setColour (juce::Colour (0xff8fa6c4).withAlpha (0.22f * alpha));
    g.strokePath (path, juce::PathStrokeType (thickness * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xffd6e2f2).withAlpha (0.42f * alpha));
    g.strokePath (path, juce::PathStrokeType (thickness * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::lcdText.withAlpha (0.95f * alpha));
    g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ── menus / tooltips ────────────────────────────────────────────────────────────────────────

SgLookAndFeel::SgLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, col::dark);
    setColour (juce::PopupMenu::textColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, col::lcdText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, col::dark);
    setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::outlineColourId, col::accent);
}

juce::Font SgLookAndFeel::getPopupMenuFont()
{
    return uiFont (15.0f, false);
}

juce::Rectangle<int> SgLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const auto font = uiFont (14.0f, false);
    const float textW = juce::GlyphArrangement::getStringWidth (font, tipText);
    const int w = juce::jmin (420, juce::roundToInt (textW) + 18);
    const int lines = juce::jmax (1, juce::roundToInt (std::ceil (textW / 400.0f)));
    const int h = lines * 17 + 8;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

void SgLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto b = juce::Rectangle<float> (static_cast<float> (width), static_cast<float> (height));
    g.setColour (col::dark);
    g.fillRoundedRectangle (b, 3.0f);
    g.setColour (col::accent);
    g.fillRect (b.withHeight (2.0f).reduced (3.0f, 0.0f));
    g.setColour (juce::Colours::white);
    g.setFont (uiFont (14.0f, false));
    g.drawFittedText (text, b.reduced (8.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 4, 0.9f);
}

void drawGlyph (juce::Graphics& g, Glyph glyph, juce::Rectangle<float> r, juce::Colour colour, float thickness)
{
    juce::Path p;
    const float x0 = r.getX(), x1 = r.getRight(), w = r.getWidth();
    const float yc = r.getCentreY(), a = r.getHeight() * 0.5f;
    switch (glyph)
    {
        case Glyph::Sine:
        case Glyph::Hf:
        {
            const int cycles = glyph == Glyph::Hf ? 3 : 1;
            for (int i = 0; i <= 48; ++i)
            {
                const float t = static_cast<float> (i) / 48.0f;
                const float y = yc - a * std::sin (juce::MathConstants<float>::twoPi * t * static_cast<float> (cycles));
                if (i == 0) p.startNewSubPath (x0 + t * w, y); else p.lineTo (x0 + t * w, y);
            }
            break;
        }
        case Glyph::Saw:        // falling ramp (DD-36)
            p.startNewSubPath (x0, yc); p.lineTo (x0, yc - a); p.lineTo (x1, yc + a); p.lineTo (x1, yc);
            break;
        case Glyph::RevSaw:
            p.startNewSubPath (x0, yc); p.lineTo (x0, yc + a); p.lineTo (x1, yc - a); p.lineTo (x1, yc);
            break;
        case Glyph::Square:
        case Glyph::Pulse:
        {
            const float mid = x0 + w * (glyph == Glyph::Pulse ? 0.3f : 0.5f);
            p.startNewSubPath (x0, yc + a); p.lineTo (x0, yc - a); p.lineTo (mid, yc - a);
            p.lineTo (mid, yc + a); p.lineTo (x1, yc + a); p.lineTo (x1, yc - a);
            break;
        }
        case Glyph::Triangle:
            p.startNewSubPath (x0, yc); p.lineTo (x0 + w * 0.25f, yc - a); p.lineTo (x0 + w * 0.75f, yc + a); p.lineTo (x1, yc);
            break;
        case Glyph::Noise:
        {
            juce::Random rnd (7);
            for (int i = 0; i <= 14; ++i)
            {
                const float x = x0 + w * static_cast<float> (i) / 14.0f;
                const float y = yc + a * (rnd.nextFloat() * 2.0f - 1.0f);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            break;
        }
        case Glyph::SampleHold:
        {
            const float lv[] { 0.3f, -0.8f, 0.6f, -0.2f, 0.9f };
            p.startNewSubPath (x0, yc - a * lv[0]);
            for (int i = 0; i < 5; ++i)
            {
                const float xa = x0 + w * static_cast<float> (i) / 5.0f, xb = x0 + w * static_cast<float> (i + 1) / 5.0f;
                p.lineTo (xa, yc - a * lv[i]); p.lineTo (xb, yc - a * lv[i]);
            }
            break;
        }
        case Glyph::Alt:
            g.setColour (colour);
            g.setFont (uiFont (r.getHeight() * 1.15f, true));
            g.drawText ("ALT", r.expanded (4.0f, 2.0f), juce::Justification::centred, false);
            return;
    }
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace sgui
