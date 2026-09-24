#include "Artwork.h"

#include <BinaryData.h>

#include <array>
#include <cmath>
#include <vector>

namespace sgui::art
{

namespace
{
// How far a pixel is from the magenta ground the parts were generated on. Every part is far from
// it in some channel, so a short ramp keys the ground out and feathers the anti-aliased edge.
float groundDistance (juce::Colour c) noexcept
{
    const float dr = 255.0f - static_cast<float> (c.getRed());
    const float dg = static_cast<float> (c.getGreen());
    const float db = 255.0f - static_cast<float> (c.getBlue());
    return std::sqrt (dr * dr + dg * dg + db * db);
}

// A sheet arrives either on the magenta ground or already cut out; take it as it comes.
juce::Image keyOut (const juce::Image& src)
{
    const int w = src.getWidth(), h = src.getHeight();
    juce::Image out (juce::Image::ARGB, w, h, true);
    const juce::Image::BitmapData s (src, juce::Image::BitmapData::readOnly);

    bool alreadyCut = false;
    if (src.hasAlphaChannel())
        for (int y = 0; y < h && ! alreadyCut; y += 4)
            for (int x = 0; x < w && ! alreadyCut; x += 4)
                alreadyCut = s.getPixelColour (x, y).getAlpha() < 200;

    juce::Image::BitmapData d (out, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const auto c = s.getPixelColour (x, y);
            if (alreadyCut) { d.setPixelColour (x, y, c); continue; }
            const float a = juce::jlimit (0.0f, 1.0f, (groundDistance (c) - 40.0f) / 60.0f);
            d.setPixelColour (x, y, a <= 0.0f ? juce::Colours::transparentBlack : c.withAlpha (a));
        }
    return out;
}

juce::Image loadSheet (const char* data, int size)
{
    const auto sheet = juce::ImageFileFormat::loadFrom (data, static_cast<size_t> (size));
    return sheet.isValid() ? keyOut (sheet) : juce::Image();
}

// Split a sheet into its parts, left to right, each trimmed to its own bounds. `floor` is the
// alpha a pixel needs to count as part of an object — raise it to ignore a ragged cut-out fringe.
std::vector<juce::Image> splitColumns (const juce::Image& sheet, int expected, int floorAlpha = 24)
{
    std::vector<juce::Image> parts;
    if (! sheet.isValid()) return parts;
    const int w = sheet.getWidth(), h = sheet.getHeight();
    const juce::Image::BitmapData bits (sheet, juce::Image::BitmapData::readOnly);

    std::vector<int> filled (static_cast<size_t> (w), 0);
    std::vector<int> top (static_cast<size_t> (w), h), bottom (static_cast<size_t> (w), -1);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (bits.getPixelColour (x, y).getAlpha() > floorAlpha)
            {
                const auto i = static_cast<size_t> (x);
                ++filled[i];
                top[i] = juce::jmin (top[i], y);
                bottom[i] = juce::jmax (bottom[i], y);
            }

    for (int x = 0; x < w; )
    {
        if (filled[static_cast<size_t> (x)] <= 3) { ++x; continue; }
        const int x0 = x;
        int y0 = h, y1 = -1;
        while (x < w && filled[static_cast<size_t> (x)] > 3)
        {
            y0 = juce::jmin (y0, top[static_cast<size_t> (x)]);
            y1 = juce::jmax (y1, bottom[static_cast<size_t> (x)]);
            ++x;
        }
        parts.push_back (sheet.getClippedImage ({ x0, y0, x - x0, y1 - y0 + 1 }).createCopy());
    }

    if (expected > 0 && static_cast<int> (parts.size()) != expected) parts.clear();
    return parts;
}

// The lowest band of a part, used to take the key out of a button that has its LED above it.
juce::Image bottomBand (const juce::Image& part)
{
    if (! part.isValid()) return part;
    const int w = part.getWidth(), h = part.getHeight();
    const juce::Image::BitmapData bits (part, juce::Image::BitmapData::readOnly);
    std::vector<bool> row (static_cast<size_t> (h), false);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w && ! row[static_cast<size_t> (y)]; ++x)
            row[static_cast<size_t> (y)] = bits.getPixelColour (x, y).getAlpha() > 40;

    int y1 = h - 1;
    while (y1 >= 0 && ! row[static_cast<size_t> (y1)]) --y1;
    int y0 = y1;
    while (y0 > 0 && row[static_cast<size_t> (y0 - 1)]) --y0;
    if (y1 <= y0) return part;
    return part.getClippedImage ({ 0, y0, w, y1 - y0 + 1 }).createCopy();
}

template <typename Fn>
const juce::Image& cached (Fn&& make)
{
    static const juce::Image image = make();
    return image;
}

const std::vector<juce::Image>& knobParts()
{
    static const std::vector<juce::Image> parts = splitColumns (loadSheet (BinaryData::knobs_png, BinaryData::knobs_pngSize), 3);
    return parts;
}

const std::vector<juce::Image>& faderParts()
{
    static const std::vector<juce::Image> parts = splitColumns (loadSheet (BinaryData::faders_png, BinaryData::faders_pngSize), 4);
    return parts;
}

const std::vector<juce::Image>& buttonParts (bool onDark)
{
    static const std::vector<juce::Image> light = []
    {
        auto p = splitColumns (loadSheet (BinaryData::buttons_png, BinaryData::buttons_pngSize), 2);
        for (auto& i : p) i = bottomBand (i);
        return p;
    }();
    static const std::vector<juce::Image> dark = []
    {
        auto p = splitColumns (loadSheet (BinaryData::buttons_dark_png, BinaryData::buttons_dark_pngSize), 2);
        for (auto& i : p) i = bottomBand (i);
        return p;
    }();
    return onDark ? dark : light;
}

const juce::Image& empty()
{
    static const juce::Image none;
    return none;
}
} // namespace

const juce::Image& knob (Tone tone)
{
    const auto& p = knobParts();
    if (p.size() != 3) return empty();
    switch (tone)
    {
        case Tone::White:  return p[0];
        case Tone::Orange: return p[1];
        case Tone::Black:  return p[2];
    }
    return p[0];
}

const juce::Image& faderCap (Tone tone)
{
    const auto& p = faderParts();
    if (p.size() != 4) return empty();
    switch (tone)
    {
        case Tone::White:  return p[0];
        case Tone::Orange: return p[1];
        case Tone::Black:  return p[2];
    }
    return p[0];
}

const juce::Image& faderTrack()
{
    const auto& p = faderParts();
    return p.size() == 4 ? p[3] : empty();
}

const juce::Image& button (bool onDark, bool lit)
{
    const auto& p = buttonParts (onDark);
    if (p.size() != 2) return empty();
    return lit ? p[1] : p[0];
}

const juce::Image& cheek()
{
    return cached ([]
    {
        const auto p = splitColumns (loadSheet (BinaryData::cheek_png, BinaryData::cheek_pngSize), 1);
        return p.empty() ? juce::Image() : p.front();
    });
}

const juce::Image& ribbon()
{
    return cached ([]
    {
        const auto p = splitColumns (loadSheet (BinaryData::ribbon_png, BinaryData::ribbon_pngSize), 1);
        return p.empty() ? juce::Image() : p.front();
    });
}

const juce::Image& glass()
{
    return cached ([]
    {
        // a strict floor, so the ragged fringe left by the cut-out is trimmed away
        const auto p = splitColumns (loadSheet (BinaryData::glass_png, BinaryData::glass_pngSize), 1, 200);
        return p.empty() ? juce::Image() : p.front();
    });
}

const juce::Image& panelTile()
{
    return cached ([] { return juce::ImageFileFormat::loadFrom (BinaryData::panel_surface_png, static_cast<size_t> (BinaryData::panel_surface_pngSize)); });
}

const juce::Image& shading()
{
    return cached ([]() -> juce::Image
    {
        // The map is neutral where there is no light: lighter than neutral becomes a warm
        // highlight, darker becomes a cool shadow, and neutral itself becomes transparent.
        const auto src = juce::ImageFileFormat::loadFrom (BinaryData::shading_png, static_cast<size_t> (BinaryData::shading_pngSize));
        if (! src.isValid()) return {};
        const int w = src.getWidth(), h = src.getHeight();
        juce::Image out (juce::Image::ARGB, w, h, true);
        const juce::Image::BitmapData s (src, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData d (out, juce::Image::BitmapData::writeOnly);

        double sum = 0.0;                                     // the map's own neutral point
        for (int y = 0; y < h; y += 3)
            for (int x = 0; x < w; x += 3) sum += s.getPixelColour (x, y).getPerceivedBrightness();
        const float neutral = static_cast<float> (sum / juce::jmax (1.0, static_cast<double> (((h + 2) / 3) * ((w + 2) / 3))));

        const juce::Colour warm (0xfffff2df), cool (0xff222a36);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const float v = s.getPixelColour (x, y).getPerceivedBrightness() - neutral;
                const float a = juce::jlimit (0.0f, 1.0f, std::abs (v) * 1.6f);
                d.setPixelColour (x, y, (v >= 0.0f ? warm : cool).withAlpha (a));
            }
        return out;
    });
}

void drawNineSlice (juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> r, float inset)
{
    if (! image.isValid()) return;
    const int iw = image.getWidth(), ih = image.getHeight();
    const int c = juce::jmax (1, juce::roundToInt (static_cast<float> (juce::jmin (iw, ih)) * juce::jlimit (0.05f, 0.49f, inset)));
    const int dc = juce::jmax (1, juce::jmin (c, juce::roundToInt (juce::jmin (r.getWidth(), r.getHeight()) * 0.45f)));

    const auto dest = r.toNearestInt();
    const int sx[4] { 0, c, iw - c, iw };
    const int dx[4] { dest.getX(), dest.getX() + dc, dest.getRight() - dc, dest.getRight() };
    const int sy[4] { 0, c, ih - c, ih };
    const int dy[4] { dest.getY(), dest.getY() + dc, dest.getBottom() - dc, dest.getBottom() };

    juce::Graphics::ScopedSaveState state (g);
    g.setOpacity (1.0f);                                      // drawImage is modulated by the fill
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
        {
            const int dw = dx[col + 1] - dx[col], dh = dy[row + 1] - dy[row];
            const int sw = sx[col + 1] - sx[col], sh = sy[row + 1] - sy[row];
            if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) continue;
            g.drawImage (image, dx[col], dy[row], dw, dh, sx[col], sy[row], sw, sh);
        }
}

} // namespace sgui::art
