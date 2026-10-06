// The FX rack's effects. Each follows its prompt in docs/fx/FX_PROMPTS.md; `v` holds the real
// parameter values in ui/fxdefs.mjs order (p1 = v[0], p9 = v[8], p12 = v[11] ...).

#include "FxRack.h"
#include "FxDsp.h"

namespace fx
{
namespace
{
using fxdefs::kDivBeats;

struct Bypass final : Unit
{
    void prepare (double, int) override {}
    void reset() override {}
    void process (float*, float*, int, const Ctx&) override {}
};

// normalised position of a log-mapped value, for turning a TIME knob into a note division
float logNorm (float v, float lo, float hi) { return std::log (v / lo) / std::log (hi / lo); }
float syncedSeconds (float knobValue, float lo, float hi, double bpm)
{
    const int i = juce::jlimit (0, fxdefs::kNumDivs - 1, juce::roundToInt (logNorm (knobValue, lo, hi) * (fxdefs::kNumDivs - 1)));
    return beatsToSeconds (kDivBeats[i], bpm);
}
float semis (float st) { return std::exp2 (st / 12.0f); }

// ── 1. PS DELAY ───────────────────────────────────────────────────────────────────────────────
// Stereo delay with a pitch shifter inside the feedback loop: each repeat climbs or falls.
struct PsDelay final : Unit
{
    double fs = 48000.0;
    sg::DelayLine line[2];
    PitchShifter up[2], down[2];
    OnePole hp[2], lp[2], timeSm[2];
    Rng rng;
    float spray[2] {}, sprayTarget[2] {}, sprayCount = 0.0f;
    float lvl = 0.0f;

    void prepare (double s, int) override
    {
        fs = s;
        for (int c = 0; c < 2; ++c)
        {
            line[c].prepare (static_cast<int> (3.5 * fs));
            up[c].prepare (fs, 70.0f);
            down[c].prepare (fs, 70.0f);
            timeSm[c].setHz (6.0f, fs);
        }
    }
    void reset() override
    {
        for (int c = 0; c < 2; ++c)
        {
            line[c].clear(); up[c].reset(); down[c].reset(); hp[c].reset(); lp[c].reset();
            timeSm[c].z = 0.0f; spray[c] = sprayTarget[c] = 0.0f;
        }
        lvl = 0.0f;
    }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        float t = v[9] >= 0.5f ? syncedSeconds (v[0], 10.0f, 2000.0f, c.bpm) : v[0] * 0.001f;
        t = clampf (t, 0.01f, 3.0f);
        const float tl = t, tr = t * (1.0f + v[1] * 0.01f);   // STEREO OFFSET: right = left ±50 %
        const float fb = v[2] * 0.0095f;                        // 0–95 %
        const float det = v[3];
        const float amt = v[4] * 0.01f;
        const float spr = v[5] * 0.01f;
        const int mode = static_cast<int> (v[8]);
        static constexpr float interval[] = { 12.0f, -12.0f, 7.0f, 12.0f, 0.0f };
        const float baseSt = interval[juce::jlimit (0, 4, mode)];
        const float rUp[2] = { semis (baseSt + det / 100.0f), semis (baseSt - det / 100.0f) };
        const float rDn[2] = { semis (-12.0f + det / 100.0f), semis (-12.0f - det / 100.0f) };
        const bool both = mode == 3;
        for (int ch = 0; ch < 2; ++ch) { hp[ch].setHz (v[6], fs); lp[ch].setHz (v[7], fs); }

        const float target[2] = { tl * static_cast<float> (fs), tr * static_cast<float> (fs) };
        float* io[2] = { l, r };
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            // SPRAY: a new random time offset per repeat period
            sprayCount -= 1.0f;
            if (sprayCount <= 0.0f)
            {
                sprayCount = target[0];
                for (int ch = 0; ch < 2; ++ch) sprayTarget[ch] = rng.bi() * spr * 0.2f * target[ch];
            }
            for (int ch = 0; ch < 2; ++ch)
            {
                spray[ch] += 0.0005f * (sprayTarget[ch] - spray[ch]);
                if (timeSm[ch].z <= 0.0f) timeSm[ch].z = target[ch];
                const float d = std::max (8.0f, timeSm[ch].lp (target[ch]) + spray[ch]);
                const float wet = line[ch].read (d);
                float sh = up[ch].process (wet, rUp[ch]);
                if (both) sh = 0.5f * (sh + down[ch].process (wet, rDn[ch]));
                float loop = wet + amt * (sh - wet);
                loop = lp[ch].lp (hp[ch].hp (loop));
                line[ch].push (fixDenorm (io[ch][i] + softClip (loop * fb)));
                io[ch][i] = wet;
                peak = std::max (peak, std::abs (wet));
            }
        }
        lvl = std::max (peak, lvl * 0.9f);
        vis[0].store (lvl, std::memory_order_relaxed);
    }
};

// Shared: 2× oversampling around a per-sample shaper (distortion family).
struct Oversampled
{
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    void prepare (int maxBlock)
    {
        os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing (static_cast<size_t> (maxBlock));
    }
    void reset() { if (os) os->reset(); }
    template <typename Fn> void run (float* l, float* r, int n, Fn&& perSample)
    {
        float* ch[2] = { l, r };
        juce::dsp::AudioBlock<float> block (ch, 2, static_cast<size_t> (n));
        auto up = os->processSamplesUp (block);
        for (size_t c = 0; c < 2; ++c)
        {
            float* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = perSample (static_cast<int> (c), d[i]);
        }
        os->processSamplesDown (block);
    }
};

// M/S width: 1 = unchanged, 0 = mono, 2 = double side
inline void width (float& l, float& r, float w) noexcept
{
    const float m = 0.5f * (l + r), s = 0.5f * (l - r) * w;
    l = m + s; r = m - s;
}

// ── 3. ECHO DELAY ─────────────────────────────────────────────────────────────────────────────
// A clean, regular stereo delay (user, 2026-09-19): STEREO, PING-PONG or MONO; time free or
// synced; feedback through low / high cut, optional drive and a gentle chorus-like MOD; ducking.
struct EchoDelay final : Unit
{
    double fs = 48000.0;
    sg::DelayLine line[2];
    Biquad lo[2], hi[2];
    OnePole timeSm;
    EnvFollower duck;
    float ph = 0.0f, lvl = 0.0f;
    void prepare (double s, int) override { fs = s; for (auto& d : line) d.prepare (static_cast<int> (4.2 * fs)); timeSm.setHz (4.0f, fs); duck.set (0.005f, 0.3f, fs); }
    void reset() override { for (int c = 0; c < 2; ++c) { line[c].clear(); lo[c].reset(); hi[c].reset(); } timeSm.z = 0.0f; duck.env = 0.0f; lvl = 0.0f; }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const float t = clampf (v[9] >= 0.5f ? syncedSeconds (v[0], 10.0f, 2000.0f, c.bpm) : v[0] * 0.001f, 0.01f, 4.0f);
        const float fb = v[1] * 0.0098f, w = v[2] * 0.01f, dk = v[5] * 0.01f, mod = v[6] * 0.01f, drv = v[7] * 0.01f;
        const int mode = juce::jlimit (0, 2, static_cast<int> (v[8] + 0.5f));
        for (int ch = 0; ch < 2; ++ch) { lo[ch].set (Biquad::HP, v[3], 0.7071f, 0, fs); hi[ch].set (Biquad::LP, v[4], 0.7071f, 0, fs); }
        const float target = t * static_cast<float> (fs), dg = 1.0f + drv * 4.0f, dn = 1.0f / std::tanh (dg);
        if (timeSm.z <= 0.0f) timeSm.z = target;
        auto shape = [&] (float x) { return drv > 0.001f ? std::tanh (x * dg) * dn : x; };
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            ph += 0.6f / static_cast<float> (fs); ph -= std::floor (ph);
            const float d = std::max (8.0f, timeSm.lp (target) * (1.0f + mod * 0.004f * std::sin (kTwoPi * ph)));
            const float in = 0.5f * (l[i] + r[i]), env = duck.process (in);
            float wl = line[0].read (d), wr = line[1].read (d);
            const float fl = lo[0].process (hi[0].process (wl)), fr = lo[1].process (hi[1].process (wr));
            if (mode == 1)        // PING-PONG: the input starts on the left, every repeat crosses over
            {
                line[0].push (fixDenorm (in + softClip (shape (fr) * fb)));
                line[1].push (fixDenorm (softClip (shape (fl) * fb)));
            }
            else if (mode == 2)   // MONO: one loop, heard in both
            {
                line[0].push (fixDenorm (in + softClip (shape (fl) * fb)));
                line[1].push (0.0f);
                wr = wl;
            }
            else
            {
                line[0].push (fixDenorm (l[i] + softClip (shape (fl) * fb)));
                line[1].push (fixDenorm (r[i] + softClip (shape (fr) * fb)));
            }
            width (wl, wr, w);
            const float g = 1.0f - dk * std::min (1.0f, env * 4.0f);
            l[i] = wl * g; r[i] = wr * g;
            peak = std::max (peak, std::max (std::abs (wl), std::abs (wr)));
        }
        lvl = std::max (peak, lvl * 0.9f);
        vis[0].store (lvl, std::memory_order_relaxed);
    }
};

// ── 4. CONVOLVER ──────────────────────────────────────────────────────────────────────────────
// juce::dsp::Convolution with the rack's impulse (dropped WAV or the default hall). LENGTH and
// REVERSE rebuild the impulse on the message thread; the convolution loads it in the background.
struct Convolver final : Unit
{
    double fs = 48000.0;
    // two-stage non-uniform partitions, still zero latency: a 512-sample head, longer partitions for
    // the tail. The default uniform engine partitions at the host's block size, so its cost grew as
    // the buffer shrank (2.2 s IR, 64-sample buffer: 10.8 % of a core per instance, now 3.3 %).
    juce::dsp::Convolution conv { juce::dsp::Convolution::NonUniform { 512 } };
    sg::DelayLine pre[2];
    Biquad lo[2], hi[2];
    float lastLen = -1.0f, lastRev = -1.0f;

    void prepare (double s, int maxBlock) override
    {
        fs = s;
        conv.prepare ({ fs, static_cast<juce::uint32> (maxBlock), 2 });
        for (auto& p : pre) p.prepare (static_cast<int> (0.21 * fs));
    }
    void reset() override { conv.reset(); for (int c = 0; c < 2; ++c) { pre[c].clear(); lo[c].reset(); hi[c].reset(); } }
    void messageTick (const float* v, const ImpulseResponse& ir, bool irChanged) override
    {
        if (! irChanged && std::abs (v[1] - lastLen) < 0.5f && v[8] == lastRev) return;
        lastLen = v[1]; lastRev = v[8];
        const int total = ir.buffer.getNumSamples();
        const int len = std::max (64, static_cast<int> (total * v[1] * 0.01f));
        juce::AudioBuffer<float> b (2, len);
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto* src = ir.buffer.getReadPointer (std::min (ch, ir.buffer.getNumChannels() - 1));
            auto* d = b.getWritePointer (ch);
            const int fade = std::min (len / 4, static_cast<int> (0.05 * ir.rate));
            for (int i = 0; i < len; ++i) d[i] = src[i] * (i >= len - fade ? static_cast<float> (len - i) / static_cast<float> (fade) : 1.0f);
            if (v[8] >= 0.5f) std::reverse (d, d + len);
        }
        conv.loadImpulseResponse (std::move (b), ir.rate, juce::dsp::Convolution::Stereo::yes,
                                  juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const float pd = std::max (1.0f, v[0] * 0.001f * static_cast<float> (fs));
        float* io[2] = { l, r };
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i) { pre[ch].push (io[ch][i]); io[ch][i] = pre[ch].read (pd); }
        juce::dsp::AudioBlock<float> block (io, 2, static_cast<size_t> (n));
        conv.process (juce::dsp::ProcessContextReplacing<float> (block));
        const float g = dbToGain (v[5]), w = v[4] * 0.01f;
        for (int ch = 0; ch < 2; ++ch) { lo[ch].set (Biquad::HP, v[2], 0.7071f, 0, fs); hi[ch].set (Biquad::LP, v[3], 0.7071f, 0, fs); }
        for (int i = 0; i < n; ++i)
        {
            float a = hi[0].process (lo[0].process (l[i])), b = hi[1].process (lo[1].process (r[i]));
            width (a, b, w);
            l[i] = a * g; r[i] = b * g;
        }
    }
};

// ── 5. VALVE ──────────────────────────────────────────────────────────────────────────────────
// Tube mic / line amp: asymmetric tube curve (MIC) or symmetric (LINE), −20 dB pad, broad shelves.
// GAIN only adds saturation (user, 2026-10-05): the drive stage's output is matched to its input's
// loudness - RMS over ~300 ms, measured on both at once so note starts do not pump - and held through
// silence. LF / HF and OUTPUT come after the match, so they change the level as they should.
struct Tuba final : Unit
{
    double fs = 48000.0;
    Oversampled os;
    OnePole dc[2];
    Biquad lf[2], hf[2];
    float lvl = 0.0f, pIn = 0.0f, pOut = 0.0f, match = 1.0f;
    void prepare (double s, int maxBlock) override { fs = s; os.prepare (maxBlock); for (auto& d : dc) d.setHz (8.0f, fs); }
    void reset() override { os.reset(); pIn = pOut = 0.0f; match = 1.0f; for (int c = 0; c < 2; ++c) { dc[c].reset(); lf[c].reset(); hf[c].reset(); } }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const float k = dbToGain (v[0]) * (0.3f + v[1] * 0.014f) * (v[9] >= 0.5f ? 0.1f : 1.0f);
        const float b = v[8] >= 0.5f ? 0.0f : 0.25f, tb = std::tanh (b), norm = 1.0f / std::max (0.3f, std::tanh (k));
        float pk = 0.0f;
        double eIn = 0.0;
        for (int i = 0; i < n; ++i) { pk = std::max (pk, std::max (std::abs (l[i]), std::abs (r[i]))); eIn += l[i] * l[i] + r[i] * r[i]; }
        lvl = std::max (pk, lvl * 0.85f); vis[0].store (lvl, std::memory_order_relaxed);
        const float amt = clampf (v[0] / 6.0f, 0.0f, 1.0f);   // GAIN 0 dB is clean; the valve fades in over the first 6 dB
        os.run (l, r, n, [&] (int, float x) { return x + ((std::tanh (k * x + b) - tb) * norm - x) * amt; });
        float* io[2] = { l, r };
        double eOut = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i) { io[ch][i] = dc[ch].hp (io[ch][i]); eOut += io[ch][i] * io[ch][i]; }
        // the loudness match: both powers follow over ~300 ms; the gain glides across the block
        const float a = std::exp (-static_cast<float> (n) / (0.3f * static_cast<float> (fs)));
        pIn = a * pIn + (1.0f - a) * static_cast<float> (eIn / (2 * n));
        pOut = a * pOut + (1.0f - a) * static_cast<float> (eOut / (2 * n));
        const float from = match;
        if (pIn > 1.0e-8f && pOut > 1.0e-10f) match = clampf (std::sqrt (pIn / pOut), 0.02f, 4.0f);   // silence: hold
        const float out = dbToGain (v[4]);
        for (int ch = 0; ch < 2; ++ch)
        {
            lf[ch].set (Biquad::LowShelf, 100.0f, 0.5f, v[2], fs);
            hf[ch].set (Biquad::HighShelf, 8000.0f, 0.5f, v[3], fs);
            for (int i = 0; i < n; ++i)
            {
                const float g = from + (match - from) * static_cast<float> (i + 1) / static_cast<float> (n);
                io[ch][i] = hf[ch].process (lf[ch].process (io[ch][i] * g)) * out;
            }
        }
    }
};

// ── 6. SATURN ─────────────────────────────────────────────────────────────────────────────────
// Three LR4 bands, each driven into the chosen style, with transient DYNAMICS and a TONE tilt.
float saturnShape (int style, float x) noexcept
{
    switch (style)
    {
        case 0:  return std::tanh (x + 0.2f) - 0.19738f;                   // warm tube: asymmetric
        case 1:  return x / (1.0f + std::abs (x));                          // clean tube
        case 2:  return std::atan (x) * 0.6366f * 1.2f;                     // tape
        case 3:  { const float y = clampf (x, -1.5f, 1.5f); return y - y * y * y / 6.75f; }   // transformer (cubic)
        case 4:  return std::tanh (1.6f * x + 0.1f * x * x);               // amp
        case 5:  return 0.5f * (std::tanh (x) + std::abs (std::tanh (x)));   // rectify (DC removed after)
        default: return std::sin (clampf (x, -8.0f, 8.0f));                // break: fold
    }
}
struct Saturn final : Unit
{
    double fs = 48000.0;
    Oversampled os;
    LR4 x1[2], x2[2];
    EnvFollower fast[2][3], slow[2][3];
    Biquad tl[2], th[2];
    OnePole dc[2];
    float f1 = -1.0f, f2 = -1.0f;
    void prepare (double s, int maxBlock) override
    {
        fs = s; os.prepare (maxBlock);
        for (int c = 0; c < 2; ++c) for (int b = 0; b < 3; ++b) { fast[c][b].set (0.001f, 0.03f, fs * 2); slow[c][b].set (0.03f, 0.2f, fs * 2); }
        for (auto& d : dc) d.setHz (8.0f, fs);
    }
    void reset() override { os.reset(); f1 = f2 = -1.0f; for (int c = 0; c < 2; ++c) { tl[c].reset(); th[c].reset(); dc[c].reset(); for (int b = 0; b < 3; ++b) fast[c][b].env = slow[c][b].env = 0.0f; } }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const double ofs = fs * 2.0;
        if (std::abs (v[0] - f1) > 0.5f || std::abs (v[1] - f2) > 0.5f)
        {
            const bool first = f1 < 0.0f;
            f1 = v[0]; f2 = std::max (v[1], v[0] * 1.5f);
            for (int ch = 0; ch < 2; ++ch)
                if (first) { x1[ch].set (f1, ofs); x2[ch].set (f2, ofs); } else { x1[ch].setKeep (f1, ofs); x2[ch].setKeep (f2, ofs); }
        }
        const int style = static_cast<int> (v[8]);
        float g[3], comp[3], amt[3];   // a band at 0 dB drive is clean; its style fades in over the first 6 dB
        for (int b = 0; b < 3; ++b) { g[b] = dbToGain (v[2 + b]); comp[b] = 1.0f / std::sqrt (g[b]); amt[b] = clampf (v[2 + b] / 6.0f, 0.0f, 1.0f); }
        const float dyn = v[5] * 0.01f;
        os.run (l, r, n, [&] (int ch, float x)
        {
            float lo, rest, mid, hiB;
            x1[ch].split (x, lo, rest);
            x2[ch].split (rest, mid, hiB);
            float band[3] = { lo, mid, hiB }, y = 0.0f;
            for (int b = 0; b < 3; ++b)
            {
                float d = g[b];
                if (dyn != 0.0f)   // + restores transients, − squashes them
                {
                    const float ef = fast[ch][b].process (band[b]), es = slow[ch][b].process (band[b]);
                    d *= clampf (std::pow ((ef + 1.0e-5f) / (es + 1.0e-5f), dyn * 0.8f), 0.25f, 4.0f);
                }
                y += band[b] + (saturnShape (style, band[b] * d) * comp[b] - band[b]) * amt[b];
            }
            return y;
        });
        const float out = dbToGain (v[7]);
        float* io[2] = { l, r };
        for (int ch = 0; ch < 2; ++ch)
        {
            tl[ch].set (Biquad::LowShelf, 800.0f, 0.5f, -v[6] * 0.5f, fs);
            th[ch].set (Biquad::HighShelf, 800.0f, 0.5f, v[6] * 0.5f, fs);
            for (int i = 0; i < n; ++i) io[ch][i] = th[ch].process (tl[ch].process (dc[ch].hp (io[ch][i]))) * out;
        }
    }
};

// ── 7. DISTORTION ─────────────────────────────────────────────────────────────────────────────
float distShape (int type, float x) noexcept
{
    switch (type)
    {
        case 0:  return std::tanh (x);
        case 1:  return clampf (x, -1.0f, 1.0f);
        case 2:  return std::sin (clampf (x, -12.0f, 12.0f));
        case 3:  return x > 0.0f ? std::tanh (x) : std::tanh (x * 0.4f) * 0.6f;
        default: return x > 0.0f ? 1.0f - std::exp (-x) : -(1.0f - std::exp (x)) * 0.8f;
    }
}
struct Distortion final : Unit
{
    double fs = 48000.0;
    Oversampled os;
    Biquad pre[2], post[2];
    OnePole dc[2];
    EnvFollower gate[2];
    float lvl = 0.0f;
    void prepare (double s, int maxBlock) override { fs = s; os.prepare (maxBlock); for (auto& d : dc) d.setHz (8.0f, fs); for (auto& g : gate) g.set (0.001f, 0.05f, fs * 2); }
    void reset() override { os.reset(); for (int c = 0; c < 2; ++c) { pre[c].reset(); post[c].reset(); dc[c].reset(); gate[c].env = 0.0f; } }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        float* io[2] = { l, r };
        float pk = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            pre[ch].set (Biquad::HP, v[3], 0.7071f, 0, fs);
            for (int i = 0; i < n; ++i) { io[ch][i] = pre[ch].process (io[ch][i]); pk = std::max (pk, std::abs (io[ch][i])); }
        }
        lvl = std::max (pk, lvl * 0.85f); vis[0].store (lvl, std::memory_order_relaxed);
        const float k = dbToGain (v[0]), bias = v[1] * 0.005f;
        const int type = static_cast<int> (v[8]);
        const float amt = clampf (v[0] / 6.0f, 0.0f, 1.0f);
        const float b0 = distShape (type, bias), norm = 1.0f / std::max (0.25f, std::abs (distShape (type, k * 0.5f + bias) - b0) * 2.0f);
        os.run (l, r, n, [&] (int ch, float x)
        {
            float y = (distShape (type, k * x + bias) - b0) * std::min (1.0f, norm * 0.5f + 0.5f);
            if (type == 3) y *= clampf (gate[ch].process (x) * 60.0f, 0.0f, 1.0f);   // FUZZ gates its tail
            return x + (y - x) * amt;   // DRIVE 0 dB is clean; the shape fades in over the first 6 dB
        });
        const float out = dbToGain (v[4]);
        for (int ch = 0; ch < 2; ++ch)
        {
            post[ch].set (Biquad::LP, v[2], 0.7071f, 0, fs);
            for (int i = 0; i < n; ++i) io[ch][i] = post[ch].process (dc[ch].hp (io[ch][i])) * out;
        }
    }
};

// ── 8. BITCRUSHER ─────────────────────────────────────────────────────────────────────────────
struct Bitcrusher final : Unit
{
    double fs = 48000.0;
    Biquad tone[2];
    Rng rng;
    float held[2] {}, count = 0.0f, period = 1.0f;
    void prepare (double s, int) override { fs = s; }
    void reset() override { held[0] = held[1] = 0.0f; count = 0.0f; tone[0].reset(); tone[1].reset(); }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const float step = 2.0f / std::exp2 (v[0]), drive = dbToGain (v[3]), out = dbToGain (v[5]);
        const float base = std::max (1.0f, static_cast<float> (fs) / v[1]), jit = v[2] * 0.01f;
        for (auto& t : tone) t.set (Biquad::LP, v[4], 0.7071f, 0, fs);
        for (int i = 0; i < n; ++i)
        {
            count -= 1.0f;
            if (count <= 0.0f)
            {
                period = base * (1.0f + jit * rng.bi() * 0.9f);
                count += std::max (1.0f, period);
                held[0] = std::round (clampf (l[i] * drive, -1.0f, 1.0f) / step) * step;
                held[1] = std::round (clampf (r[i] * drive, -1.0f, 1.0f) / step) * step;
            }
            l[i] = tone[0].process (held[0]) * out;
            r[i] = tone[1].process (held[1]) * out;
        }
    }
};

// Feed-forward compressor gain computer, soft knee, in dB.
inline float grDb (float levelDb, float thr, float ratio, float knee) noexcept
{
    const float over = levelDb - thr;
    if (over <= -knee * 0.5f) return 0.0f;
    if (over >= knee * 0.5f) return over * (1.0f - 1.0f / ratio);
    const float t = over + knee * 0.5f;
    return (1.0f - 1.0f / ratio) * t * t / (2.0f * knee);
}

// ── 9. VULF COMP ──────────────────────────────────────────────────────────────────────────────
// Aggressive pumping compressor with vinyl wow/flutter and a lo-fi stage; colour even at 0 %.
struct Vulf final : Unit
{
    double fs = 48000.0;
    EnvFollower det;
    OnePole grSm, lofiLp[2];
    sg::DelayLine wob[2];
    Rng rng;
    float wowPh = 0.0f, flPh = 0.0f, held[2] {}, cnt = 0.0f, grShow = 0.0f;
    void prepare (double s, int) override { fs = s; for (auto& w : wob) w.prepare (static_cast<int> (0.02 * fs)); }
    void reset() override { det.env = 0.0f; grSm.z = 0.0f; for (int c = 0; c < 2; ++c) { wob[c].clear(); lofiLp[c].reset(); held[c] = 0.0f; } }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const float in = dbToGain (v[0]), amt = v[1] * 0.01f, out = dbToGain (v[6]);
        const float thr = -8.0f - amt * 32.0f, ratio = 1.0f + amt * 19.0f;   // COMPRESS 0 % = ratio 1: no reduction
        det.set (v[2] * 0.001f, v[3] * 0.001f, fs);
        grSm.setHz (200.0f, fs);
        const float wf = v[4] * 0.01f, lofi = v[5] * 0.01f;
        const float hold = 1.0f + lofi * (static_cast<float> (fs) / 7000.0f - 1.0f);
        for (auto& f : lofiLp) f.setHz (18000.0f * std::exp2 (-2.8f * lofi), fs);
        float grMax = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float x[2] = { l[i] * in, r[i] * in };
            const float e = det.process (std::max (std::abs (x[0]), std::abs (x[1])));
            const float gr = grSm.lp (grDb (gainToDb (e), thr, ratio, 6.0f));
            grMax = std::max (grMax, gr);
            const float g = dbToGain (-gr) * (1.0f + amt * 0.5f);   // a little built-in make-up
            wowPh += 0.5f / static_cast<float> (fs); wowPh -= std::floor (wowPh);
            flPh += 7.0f / static_cast<float> (fs); flPh -= std::floor (flPh);
            const float d = 0.004f * static_cast<float> (fs) * (1.0f + wf * (0.8f * std::sin (kTwoPi * wowPh) + 0.2f * std::sin (kTwoPi * flPh)));
            cnt -= 1.0f;
            const bool take = cnt <= 0.0f;
            if (take) cnt += hold;
            float* io[2] = { l, r };
            for (int ch = 0; ch < 2; ++ch)
            {
                wob[ch].push (x[ch] * g);
                float y = wob[ch].read (d);
                // colour grows with COMPRESS and GRIT; at 0 % both, the signal passes clean
                const float tone = std::min (1.0f, amt * 2.0f + lofi * 4.0f);
                y += (std::tanh (y * (1.1f + lofi * 2.0f)) / (1.1f + lofi * 0.8f) - y) * tone;
                if (take) held[ch] = y;
                if (lofi > 0.01f) y = lofiLp[ch].lp (held[ch]) + rng.bi() * lofi * 0.004f;
                io[ch][i] = y * out;
            }
        }
        grShow = std::max (grMax, grShow * 0.8f);
        vis[1].store (grShow, std::memory_order_relaxed);
    }
};

// ── 10. FARADAY LIMITER ───────────────────────────────────────────────────────────────────────
// Vari-mu style limiter: soft knee, program-dependent release, colour that grows with reduction.
struct Faraday final : Unit
{
    double fs = 48000.0;
    float env = 0.0f, gr = 0.0f, grShow = 0.0f;
    Biquad lo[2], hi[2];
    OnePole dc[2];
    void prepare (double s, int) override { fs = s; for (auto& d : dc) d.setHz (8.0f, fs); }
    void reset() override { env = gr = 0.0f; for (int c = 0; c < 2; ++c) { lo[c].reset(); hi[c].reset(); dc[c].reset(); } }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const float thr = v[0], ratio = v[1], att = smoothCoeff (v[2] * 0.001f, fs), rel0 = v[3] * 0.001f;
        const float color = v[4] * 0.01f, warm = v[5] * 0.01f, vibe = v[6] * 0.01f;
        const float makeup = v[8] >= 0.5f ? -thr * (1.0f - 1.0f / ratio) * 0.6f : 0.0f;
        const float out = dbToGain (v[7] + makeup);
        for (int ch = 0; ch < 2; ++ch) { lo[ch].set (Biquad::LowShelf, 200.0f, 0.6f, warm * 4.0f, fs); hi[ch].set (Biquad::HighShelf, 5000.0f, 0.6f, -warm * 4.0f, fs); }
        float grMax = 0.0f;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float pk = std::max (std::abs (l[i]), std::abs (r[i]));
            const float target = grDb (gainToDb (pk), thr, ratio, 6.0f);
            const float rel = smoothCoeff (rel0 * (1.0f + gr / 12.0f), fs);   // slower after deep reduction
            gr += (target > gr ? att : rel) * (target - gr);
            grMax = std::max (grMax, gr);
            const float g = dbToGain (-gr), sat = 1.0f + color * gr / 6.0f;
            for (int ch = 0; ch < 2; ++ch)
            {
                float y = io[ch][i] * g;
                y += (std::tanh (y * sat) / sat - y) * std::min (1.0f, color * 4.0f);   // COLOR 0 % = a clean limiter
                y = dc[ch].hp (y + vibe * 0.25f * y * y);
                io[ch][i] = hi[ch].process (lo[ch].process (y)) * out;
            }
        }
        grShow = std::max (grMax, grShow * 0.8f);
        vis[1].store (grShow, std::memory_order_relaxed);
    }
};

// ── 11. MULTIBAND ─────────────────────────────────────────────────────────────────────────────
// Three LR4 bands, each compressed downward above and/or upward below its thresholds (OTT-like).
struct Multiband final : Unit
{
    double fs = 48000.0;
    LR4 x1[2], x2[2];
    EnvFollower env[3];
    float gain[3] {}, show[3] {}, f1 = -1.0f, f2 = -1.0f;
    void prepare (double s, int) override { fs = s; }
    void reset() override { f1 = f2 = -1.0f; for (int b = 0; b < 3; ++b) { env[b].env = 0.0f; gain[b] = show[b] = 0.0f; } }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        if (std::abs (v[11] - f1) > 0.5f || std::abs (v[12] - f2) > 0.5f)
        {
            const bool first = f1 < 0.0f;
            f1 = v[11]; f2 = std::max (v[12], v[11] * 1.5f);
            for (int ch = 0; ch < 2; ++ch)
                if (first) { x1[ch].set (f1, fs); x2[ch].set (f2, fs); } else { x1[ch].setKeep (f1, fs); x2[ch].setKeep (f2, fs); }
        }
        const int mode = static_cast<int> (v[8]);
        const bool down = mode != 2, up = mode != 1;
        const float amt = v[3] * 0.01f, in = dbToGain (v[4]), out = dbToGain (v[7]);
        const float outB[3] = { dbToGain (v[0]), dbToGain (v[1]), dbToGain (v[2]) };
        for (auto& e : env) e.set (v[5] * 0.001f, v[6] * 0.001f, fs);
        const float gs = smoothCoeff (0.003f, fs);
        float mx[3] = {};
        for (int i = 0; i < n; ++i)
        {
            float b[2][3];
            for (int ch = 0; ch < 2; ++ch)
            {
                float lo, rest;
                x1[ch].split ((ch ? r[i] : l[i]) * in, lo, rest);
                x2[ch].split (rest, b[ch][1], b[ch][2]);
                b[ch][0] = lo;
            }
            float yl = 0.0f, yr = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                const float lvl = gainToDb (env[k].process (std::max (std::abs (b[0][k]), std::abs (b[1][k]))));
                float target = 0.0f;
                if (down && lvl > -20.0f) target -= (lvl + 20.0f) * (1.0f - 1.0f / (1.0f + 5.0f * amt));
                if (up && lvl < -38.0f && lvl > -80.0f) target += std::min (18.0f * amt, (-38.0f - lvl) * (1.0f - 1.0f / (1.0f + 3.0f * amt)));
                gain[k] += gs * (target - gain[k]);
                mx[k] = std::abs (gain[k]) > std::abs (mx[k]) ? gain[k] : mx[k];
                const float g = dbToGain (gain[k]) * outB[k];
                yl += b[0][k] * g; yr += b[1][k] * g;
            }
            l[i] = yl * out; r[i] = yr * out;
        }
        for (int k = 0; k < 3; ++k) { show[k] = mx[k]; vis[static_cast<size_t> (1 + k)].store (show[k], std::memory_order_relaxed); }
    }
};

// ── 12. PHASER PAN ────────────────────────────────────────────────────────────────────────────
// Stereo phaser (4–12 stages, feedback, L/R spread) into an auto-panner, free or on the phaser LFO.
struct PhaserPan final : Unit
{
    double fs = 48000.0;
    Allpass1 ap[2][12];
    float fbv[2] {}, ph = 0.0f, panPh = 0.0f;
    void prepare (double s, int) override { fs = s; }
    void reset() override { for (auto& c : ap) for (auto& a : c) a.z = 0.0f; fbv[0] = fbv[1] = 0.0f; }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        static constexpr int stagesOf[4] = { 4, 6, 8, 12 };
        const int st = stagesOf[juce::jlimit (0, 3, static_cast<int> (v[8]))];
        const float rate = v[0], depth = v[1] * 0.01f, fb = v[2] * 0.0095f, centre = v[3], spread = v[4] / 360.0f;
        const float panRate = v[5], panDepth = v[6] * 0.01f;
        const bool linked = v[9] >= 0.5f;
        float a[2] = {};
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            ph += rate / static_cast<float> (fs); ph -= std::floor (ph);
            panPh += panRate / static_cast<float> (fs); panPh -= std::floor (panPh);
            if ((i & 7) == 0)
                for (int ch = 0; ch < 2; ++ch)
                    a[ch] = Allpass1::coeff (centre * std::exp2 (2.0f * depth * std::sin (kTwoPi * (ph + spread * ch))), fs);
            for (int ch = 0; ch < 2; ++ch)
            {
                float y = io[ch][i] + softClip (fbv[ch] * fb);
                for (int k = 0; k < st; ++k) y = ap[ch][k].process (y, a[ch]);
                fbv[ch] = y;
                io[ch][i] = 0.5f * (io[ch][i] + y);
            }
            float gl, gr;
            panGains (panDepth * std::sin (kTwoPi * (linked ? ph : panPh)), gl, gr);
            l[i] *= gl; r[i] *= gr;
        }
    }
};

// ── 13. FLANGER ───────────────────────────────────────────────────────────────────────────────
struct Flanger final : Unit
{
    double fs = 48000.0;
    sg::DelayLine line[2];
    OnePole hp[2], lp[2];
    float ph = 0.0f, fbv[2] {};
    void prepare (double s, int) override { fs = s; for (auto& d : line) d.prepare (static_cast<int> (0.025 * fs)); }
    void reset() override { for (int c = 0; c < 2; ++c) { line[c].clear(); hp[c].reset(); lp[c].reset(); fbv[c] = 0.0f; } }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const float rate = v[0], base = v[1] * 0.001f * static_cast<float> (fs), depth = v[2] * 0.01f, fb = v[3] * 0.0095f;
        const float off = v[8] >= 0.5f ? 0.25f : 0.0f;
        const bool tri = v[9] >= 0.5f;
        for (int ch = 0; ch < 2; ++ch) { hp[ch].setHz (v[4], fs); lp[ch].setHz (v[5], fs); }
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            ph += rate / static_cast<float> (fs); ph -= std::floor (ph);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float p = ph + off * ch - std::floor (ph + off * ch);
                const float lfo = tri ? 1.0f - 4.0f * std::abs (p - 0.5f) : std::sin (kTwoPi * p);
                const float d = std::max (2.0f, base * (1.0f + depth * 0.95f * lfo));
                const float y = line[ch].read (d);
                line[ch].push (fixDenorm (io[ch][i] + softClip (lp[ch].lp (hp[ch].hp (y)) * fb)));
                io[ch][i] = y;
            }
        }
    }
};

// ── 14. STEREO PAN ────────────────────────────────────────────────────────────────────────────
struct StereoPan final : Unit
{
    double fs = 48000.0;
    sg::DelayLine haas;
    float ph = 0.0f;
    void prepare (double s, int) override { fs = s; haas.prepare (static_cast<int> (0.021 * fs)); }
    void reset() override { haas.clear(); }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const float rate = v[8] >= 0.5f ? 1.0f / syncedSeconds (v[0], 0.05f, 20.0f, c.bpm) : v[0];
        const float depth = v[1] * 0.01f, sh = v[2] * 0.01f, phase = v[3] / 360.0f, w = v[4] * 0.01f;
        const float hd = v[5] * 0.001f * static_cast<float> (fs);
        for (int i = 0; i < n; ++i)
        {
            ph += rate / static_cast<float> (fs); ph -= std::floor (ph);
            float p = ph + phase; p -= std::floor (p);
            const float sn = std::sin (kTwoPi * p), tr = 1.0f - 4.0f * std::abs (p - 0.5f) , sq = sn >= 0.0f ? 1.0f : -1.0f;
            const float lfo = sh < 0.5f ? sn + (tr - sn) * sh * 2.0f : tr + (sq - tr) * (sh - 0.5f) * 2.0f;
            float a = l[i], b = r[i];
            width (a, b, w);
            haas.push (b);
            if (hd > 0.5f) b = haas.read (hd);
            float gl, gr;
            // PHASE offsets the LFO; with DEPTH at 0 it places the sound by hand
            panGains (depth * lfo + (1.0f - depth) * std::sin (kTwoPi * phase), gl, gr);
            l[i] = a * gl; r[i] = b * gr;
        }
    }
};

// ── 15. PRO-Q ─────────────────────────────────────────────────────────────────────────────────
// Six band slots, all off until added from the graph. Each band has its own shape (ui/fxdefs.mjs
// PQ_SHAPES): bell, shelves, low / high cut at 12, 24 or 48 dB/oct, notch, band pass.
struct ProQ final : Unit
{
    double fs = 48000.0;
    static constexpr int kMax = 24;
    Biquad sec[2][kMax];
    Biquad next[kMax];
    int count = 0;
    void prepare (double s, int) override { fs = s; }
    void reset() override { for (auto& ch : sec) for (auto& b : ch) b.reset(); }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        // FREQ / GAIN / Q and SHAPE of each band, as 0-based parameter indices (p12 = 11 ...)
        static constexpr int P[6][3] = { { 11, 12, 13 }, { 14, 15, 16 }, { 17, 18, 19 }, { 20, 21, 22 }, { 23, 24, 25 }, { 0, 1, 2 } };
        static constexpr int SHAPE[6] = { 4, 5, 6, 7, 9, 10 };
        const int on = static_cast<int> (v[26] + 0.5f);
        int k = 0;
        for (int b = 0; b < 6; ++b)
        {
            if (! (on & (1 << b))) continue;
            const float f = v[P[b][0]], g = v[P[b][1]], q = v[P[b][2]];
            const int shape = juce::jlimit (0, 10, static_cast<int> (v[SHAPE[b]] + 0.5f));
            if (shape >= 3 && shape <= 8)   // cuts: 1, 2 or 4 Butterworth-ish sections, the first at the band's Q
            {
                const int sections = 1 << ((shape - 3) % 3);
                for (int j = 0; j < sections; ++j) next[k++].set (shape <= 5 ? Biquad::HP : Biquad::LP, f, j ? 0.7071f : q, 0, fs);
            }
            else
                next[k++].set (shape == 0 ? Biquad::Bell : shape == 1 ? Biquad::LowShelf : shape == 2 ? Biquad::HighShelf
                                                          : shape == 9 ? Biquad::Notch : Biquad::BP, f, q, g, fs);
        }
        // keep each section's state when only its coefficients change
        for (int ch = 0; ch < 2; ++ch)
            for (int j = 0; j < k; ++j)
            {
                auto& s = sec[ch][j];
                if (j >= count) s.reset();
                s.b0 = next[j].b0; s.b1 = next[j].b1; s.b2 = next[j].b2; s.a1 = next[j].a1; s.a2 = next[j].a2;
            }
        count = k;
        const float out = dbToGain (v[3]);
        float* io[2] = { l, r };
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                float y = io[ch][i];
                for (int j = 0; j < count; ++j) y = sec[ch][j].process (y);
                io[ch][i] = y * out;
            }
    }
};

// ── 16. FILTER ────────────────────────────────────────────────────────────────────────────────
struct Filter final : Unit
{
    double fs = 48000.0;
    Svf a[2], b[2];
    EnvFollower env;
    float ph = 0.0f;
    void prepare (double s, int) override { fs = s; env.set (0.01f, 0.15f, fs); }
    void reset() override { for (int c = 0; c < 2; ++c) { a[c].reset(); b[c].reset(); } env.env = 0.0f; }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const float res = v[1] * 0.01f, drive = dbToGain (v[2]), comp = 1.0f / std::sqrt (drive);
        const int type = static_cast<int> (v[8]);
        const bool steep = v[9] >= 0.5f;
        float* io[2] = { l, r };
        float e = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            e = env.process (0.5f * (l[i] + r[i]));
            ph += v[3] / static_cast<float> (fs); ph -= std::floor (ph);
            if ((i & 15) == 0)
            {
                const float oct = v[4] * std::sin (kTwoPi * ph) + v[5] * std::min (1.0f, e * 3.0f);
                const float fc = clampf (v[0] * std::exp2 (oct), 20.0f, 20000.0f);
                for (int ch = 0; ch < 2; ++ch) { a[ch].set (fc, res, fs); b[ch].set (fc, steep ? res * 0.5f : res, fs); }
            }
            for (int ch = 0; ch < 2; ++ch)
            {
                float x = drive > 1.001f ? std::tanh (io[ch][i] * drive) * comp : io[ch][i];
                auto pick = [type] (const Svf& f) { return type == 0 ? f.lo : type == 1 ? f.hi : type == 2 ? f.bp : f.lo + f.hi; };   // NOTCH = low + high
                a[ch].process (x);
                float y = pick (a[ch]);
                if (steep) { b[ch].process (y); y = pick (b[ch]); }
                io[ch][i] = y;
            }
        }
        vis[0].store (std::min (1.0f, e * 3.0f), std::memory_order_relaxed);
    }
};

// ── 17. AUTOCHROMA ────────────────────────────────────────────────────────────────────────────
// One grain stream (user, 2026-09-18) reading a 3 s buffer: pitch + fine, size, density, spray,
// feedback, level, reverse probability, stereo spread, envelope shape, tone.
struct Autochroma final : Unit
{
    double fs = 48000.0;
    static constexpr int kGrains = 64;
    struct Grain { bool on = false, rev = false; float start = 0, pos = 0, len = 1, ratio = 1, gl = 1, gr = 1; };
    std::vector<float> buf;
    int mask = 0, w = 0;
    Grain grains[kGrains];
    float due = 0.0f;
    Rng rng;
    OnePole tone[2];
    void prepare (double s, int) override
    {
        fs = s;
        int size = 1; while (size < static_cast<int> (3.2 * fs)) size <<= 1;
        buf.assign (static_cast<size_t> (size), 0.0f); mask = size - 1;
    }
    void reset() override { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; due = 0.0f; for (auto& g : grains) g.on = false; tone[0].reset(); tone[1].reset(); }
    float at (float p) const noexcept
    {
        const float fl = std::floor (p); const int i = static_cast<int> (fl); const float f = p - fl;
        return buf[static_cast<size_t> (i & mask)] * (1.0f - f) + buf[static_cast<size_t> ((i + 1) & mask)] * f;
    }
    static float envelope (int shape, float t) noexcept
    {
        switch (shape)
        {
            case 1:  return 1.0f - std::abs (2.0f * t - 1.0f);                                   // triangle
            case 2:  return t < 0.05f ? t / 0.05f : std::exp (-(t - 0.05f) * 6.0f);            // perc
            case 3:  return std::min (1.0f, std::min (t, 1.0f - t) * 20.0f);                    // gate
            default: { const float s = std::sin (kPi * t); return s * s; }                      // smooth
        }
    }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        static constexpr float pitches[9] = { -12, -7, -5, 0, 5, 7, 12, 19, 24 };
        const float len = v[0] * 0.001f * static_cast<float> (fs), dens = v[1], spray = v[2] * 0.001f * static_cast<float> (fs), fb = v[3] * 0.01f;
        const float level = v[4] * 0.01f, revP = v[5] * 0.01f, spread = v[6] * 0.01f;
        const float ratio = std::exp2 ((pitches[juce::jlimit (0, 8, static_cast<int> (v[8] + 0.5f))] + v[10] * 0.01f) / 12.0f);
        const int shape = juce::jlimit (0, 3, static_cast<int> (v[9] + 0.5f));
        const float norm = level / std::sqrt (std::max (1.0f, dens * v[0] * 0.001f));
        for (auto& t : tone) t.setHz (v[7], fs);
        for (int i = 0; i < n; ++i)
        {
            due -= dens / static_cast<float> (fs);
            if (due <= 0.0f)
            {
                due += 1.0f + rng.bi() * 0.3f;
                for (auto& g : grains)
                    if (! g.on)
                    {
                        g = { true, rng.next() < revP, static_cast<float> (w) - (len * std::max (1.0f, ratio) + 4.0f + rng.next() * spray), 0.0f, len, ratio, 1, 1 };
                        panGains (rng.bi() * spread, g.gl, g.gr);
                        break;
                    }
            }
            float yl = 0.0f, yr = 0.0f;
            for (auto& g : grains)
            {
                if (! g.on) continue;
                const float t = g.pos / g.len;
                const float x = at (g.start + (g.rev ? g.len - g.pos : g.pos) * g.ratio) * envelope (shape, t);
                yl += x * g.gl; yr += x * g.gr;
                if ((g.pos += 1.0f) >= g.len) g.on = false;
            }
            yl = tone[0].lp (yl * norm); yr = tone[1].lp (yr * norm);
            buf[static_cast<size_t> (w & mask)] = fixDenorm (0.5f * (l[i] + r[i]) + softClip (0.5f * (yl + yr) * fb));
            w = (w + 1) & mask;
            l[i] = yl; r[i] = yr;
        }
    }
};

// ── 19. SPACES ────────────────────────────────────────────────────────────────────────────
// Algorithmic reverb. MODE picks the space (its scale, early reflections,
// diffusion and brightness); COLOR picks the era: 1970s dark and grainy, 1980s bright with a
// little grit, NOW clean (shown as AGED / DIGITAL / CLEAN). Pre-delay -> early reflections + attack diffusion -> FDN -> late diffusion.
struct ValleyVerb final : Unit
{
    struct Space { float scale, er, diff, bright, build; };
    static constexpr Space kSpaces[6] = {
        { 0.45f, 0.60f, 0.60f, 0.85f, 0.2f },   // ROOM: small, strong early reflections, a touch dark
        { 0.62f, 0.40f, 0.75f, 0.95f, 0.3f },   // CHAMBER: dense and transparent
        { 0.55f, 0.00f, 0.95f, 1.25f, 0.0f },   // PLATE: no early reflections, instant dense build, bright
        { 1.00f, 0.30f, 0.70f, 1.00f, 0.4f },   // HALL
        { 1.60f, 0.25f, 0.80f, 0.90f, 0.7f },   // CATHEDRAL: huge, slow build
        { 0.30f, 0.80f, 0.50f, 1.00f, 0.1f } }; // AMBIENCE: short, mostly early reflections
    double fs = 48000.0;
    sg::DelayLine pre[2];
    Allpass inDiff[2][4], build[2][2], outDiff[2][2];
    Fdn fdn;
    Biquad hc[2], lc[2], bass[2], era[2];
    float held[2] {}, cnt = 0.0f;
    void prepare (double s, int) override
    {
        fs = s; fdn.prepare (fs);
        static constexpr float dIn[4] = { 0.0043f, 0.0071f, 0.0109f, 0.0151f }, dBuild[2] = { 0.047f, 0.083f }, dOut[2] = { 0.0089f, 0.0133f };
        for (int c = 0; c < 2; ++c)
        {
            pre[c].prepare (static_cast<int> (0.5 * fs));
            const float sp = c ? 1.071f : 1.0f;
            for (int k = 0; k < 4; ++k) { inDiff[c][k].prepare (static_cast<int> (0.02 * fs)); inDiff[c][k].len = dIn[k] * sp * static_cast<float> (fs); }
            for (int k = 0; k < 2; ++k) { build[c][k].prepare (static_cast<int> (0.1 * fs)); build[c][k].len = dBuild[k] * sp * static_cast<float> (fs); }
            for (int k = 0; k < 2; ++k) { outDiff[c][k].prepare (static_cast<int> (0.02 * fs)); outDiff[c][k].len = dOut[k] * sp * static_cast<float> (fs); }
        }
    }
    void reset() override
    {
        fdn.reset(); cnt = 0.0f;
        for (int c = 0; c < 2; ++c)
        {
            pre[c].clear(); hc[c].reset(); lc[c].reset(); bass[c].reset(); era[c].reset(); held[c] = 0.0f;
            for (auto& a : inDiff[c]) a.reset(); for (auto& a : build[c]) a.reset(); for (auto& a : outDiff[c]) a.reset();
        }
    }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const auto& sp = kSpaces[juce::jlimit (0, 5, static_cast<int> (v[8] + 0.5f))];
        const int colour = juce::jlimit (0, 2, static_cast<int> (v[9] + 0.5f));
        const float size = v[2] * 0.01f, scale = sp.scale * (0.35f + 0.65f * size);
        const float fdnSize = clampf ((scale * 1.15f - 0.3f) / 1.7f, 0.0f, 1.0f);
        const float damp = v[13] * sp.bright * (colour == 0 ? 0.6f : 1.0f);
        const float modDepth = v[7] * 0.01f * (colour == 0 ? 0.3f : 1.0f);
        fdn.set (fdnSize, v[1], damp, modDepth, false, v[6]);
        const float pd = std::max (1.0f, v[0] * 0.001f * static_cast<float> (fs));
        const float erLen = 0.004f + 0.035f * scale, attack = clampf (v[3] * 0.01f * 0.6f + sp.build * 0.4f, 0.0f, 1.0f);
        const float gIn = 0.3f + 0.45f * v[4] * 0.01f * sp.diff, gOut = 0.2f + 0.5f * v[5] * 0.01f;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (auto& a : inDiff[ch]) a.g = gIn;
            for (auto& a : build[ch]) a.g = 0.7f * attack;
            for (auto& a : outDiff[ch]) a.g = gOut;
            hc[ch].set (Biquad::LP, v[11], 0.7071f, 0, fs);
            lc[ch].set (Biquad::HP, v[12], 0.7071f, 0, fs);
            bass[ch].set (Biquad::LowShelf, 250.0f, 0.6f, 20.0f * std::log10 (v[14]) * 0.8f, fs);   // BASS MULT as a low shelf on the tail
            era[ch].set (Biquad::LP, colour == 0 ? 7000.0f : colour == 1 ? 12500.0f : 20000.0f, 0.7071f, 0, fs);
        }
        const float hold = colour == 0 ? static_cast<float> (fs) / 20000.0f : colour == 1 ? static_cast<float> (fs) / 32000.0f : 1.0f;
        static constexpr float erTap[6] = { 0.11f, 0.23f, 0.37f, 0.52f, 0.71f, 0.93f }, erGain[6] = { 0.9f, -0.7f, 0.6f, -0.5f, 0.4f, -0.3f };
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            float in[2], er[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                pre[ch].push (io[ch][i]);
                const float x = pre[ch].read (pd);
                float e = 0.0f;   // early reflections: taps spread over the space's first few tens of ms
                for (int k = 0; k < 6; ++k) e += erGain[k] * pre[ch].read (pd + erTap[(k + ch) % 6] * erLen * static_cast<float> (fs));
                er[ch] = e * 0.35f * sp.er;
                float y = x;
                for (auto& a : inDiff[ch]) y = a.process (y);
                for (auto& a : build[ch]) y = a.process (y);
                in[ch] = y;
            }
            float ol, orr;
            fdn.process (in[0], in[1], ol, orr);
            float o[2] = { ol, orr };
            cnt -= 1.0f; const bool take = cnt <= 0.0f; if (take) cnt += hold;
            for (int ch = 0; ch < 2; ++ch)
            {
                float y = o[ch];
                for (auto& a : outDiff[ch]) y = a.process (y);
                y = bass[ch].process (y) + er[ch];
                y = era[ch].process (y);
                if (hold > 1.0f) { if (take) held[ch] = y; y = held[ch]; }   // the older eras' lower sample rate
                io[ch][i] = lc[ch].process (hc[ch].process (y));
            }
        }
    }
};
constexpr ValleyVerb::Space ValleyVerb::kSpaces[6];

// ── 20. STEREO IMAGER ─────────────────────────────────────────────────────────────────────────
// Three LR4 bands with their own M/S width, a mono-bass cutoff, balance. Publishes the output's
// L/R correlation (vis[0], −1…+1) and its side-to-mid ratio (vis[1]) for the display.
struct Imager final : Unit
{
    double fs = 48000.0;
    LR4 x1[2], x2[2], xm[2];
    float f1 = -1, f2 = -1, fm = -1, corr = 1.0f, ratio = 0.0f;
    void prepare (double s, int) override { fs = s; }
    void reset() override { f1 = f2 = fm = -1.0f; corr = 1.0f; ratio = 0.0f; }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const bool first = f1 < 0.0f;
        if (std::abs (v[3] - f1) > 0.5f || std::abs (v[4] - f2) > 0.5f || std::abs (v[5] - fm) > 0.5f)
        {
            f1 = v[3]; f2 = std::max (v[4], v[3] * 1.5f); fm = v[5];
            for (int ch = 0; ch < 2; ++ch)
                if (first) { x1[ch].set (f1, fs); x2[ch].set (f2, fs); xm[ch].set (fm, fs); }
                else { x1[ch].setKeep (f1, fs); x2[ch].setKeep (f2, fs); xm[ch].setKeep (fm, fs); }
        }
        const float w[3] = { v[0] * 0.01f, v[1] * 0.01f, v[2] * 0.01f };
        const bool monoBass = v[5] > 21.0f;
        const float bal = v[6] * 0.01f, gl = std::min (1.0f, 1.0f - bal), gr = std::min (1.0f, 1.0f + bal), out = dbToGain (v[7]);
        double sLR = 0, sLL = 0, sRR = 0, sM = 0, sS = 0;
        for (int i = 0; i < n; ++i)
        {
            float b[2][3];
            for (int ch = 0; ch < 2; ++ch)
            {
                float lo, rest;
                x1[ch].split (ch ? r[i] : l[i], lo, rest);
                x2[ch].split (rest, b[ch][1], b[ch][2]);
                b[ch][0] = lo;
            }
            float yl = 0.0f, yr = 0.0f;
            for (int k = 0; k < 3; ++k) { float a = b[0][k], d = b[1][k]; width (a, d, w[k]); yl += a; yr += d; }
            if (monoBass)   // below the cutoff: the side goes, the mid stays
            {
                float ll, lh, rl, rh;
                xm[0].split (yl, ll, lh); xm[1].split (yr, rl, rh);
                const float m = 0.5f * (ll + rl);
                yl = m + lh; yr = m + rh;
            }
            yl *= gl * out; yr *= gr * out;
            l[i] = yl; r[i] = yr;
            sLR += yl * yr; sLL += yl * yl; sRR += yr * yr;
            sM += 0.25 * (yl + yr) * (yl + yr); sS += 0.25 * (yl - yr) * (yl - yr);
        }
        if (sLL > 1e-9 && sRR > 1e-9) corr += 0.3f * (static_cast<float> (sLR / std::sqrt (sLL * sRR)) - corr);
        if (sM + sS > 1e-9) ratio += 0.3f * (static_cast<float> (std::sqrt (sS / (sM + 1e-12))) - ratio);
        vis[0].store (corr, std::memory_order_relaxed);
        vis[1].store (ratio, std::memory_order_relaxed);
    }
};

// ── 21. NUDESTORT ─────────────────────────────────────────────────────────────────────────────
// Nudistort-style: input damping and GRAIN (micro-timing jitter) into one of four odd drives,
// TRADITIONAL or UNPREDICTABLE (drive and bias wander), mu-law companding and amp BUZZ, a TONE
// tilt, and a small delay with ABNORMALITY (its time and pitch wobble). vis[0] = input level.
struct Nudestort final : Unit
{
    double fs = 48000.0;
    Oversampled os;
    OnePole damp[2], dc[2], buzzLp;
    sg::DelayLine grain[2], dly[2];
    Biquad tl[2], th[2], scream[2];
    EnvFollower env;
    Rng rng;
    float gTarget[2] {}, gNow[2] {}, gCount = 0.0f, wander = 0.0f, wanderT = 0.0f, wCount = 0.0f, buzzPh = 0.0f, abPh = 0.0f, abJump = 0.0f, abT = 0.0f, lvl = 0.0f;
    void prepare (double s, int maxBlock) override
    {
        fs = s; os.prepare (maxBlock);
        for (int c = 0; c < 2; ++c) { grain[c].prepare (static_cast<int> (0.01 * fs)); dly[c].prepare (static_cast<int> (1.2 * fs)); dc[c].setHz (8.0f, fs); }
        env.set (0.002f, 0.08f, fs); buzzLp.setHz (900.0f, fs);
    }
    void reset() override
    {
        os.reset(); env.env = 0.0f; wander = wanderT = abJump = abT = 0.0f;
        for (int c = 0; c < 2; ++c) { damp[c].reset(); dc[c].reset(); grain[c].clear(); dly[c].clear(); tl[c].reset(); th[c].reset(); scream[c].reset(); gNow[c] = gTarget[c] = 0.0f; }
    }
    static float drive (int type, float x) noexcept
    {
        switch (type)
        {
            case 0:  return x > 0.0f ? std::tanh (x * 1.2f) : std::tanh (x * 0.5f) * 0.8f;                 // FUZZ
            case 1:  return std::sin (clampf (x, -10.0f, 10.0f) * 1.3f);                                    // FOLD
            case 2:  return std::round (std::tanh (x) * 6.0f) / 6.0f;                                       // CRUSH
            default: return clampf (x * 1.8f, -1.0f, 1.0f);                                                 // SCREAM
        }
    }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const int type = juce::jlimit (0, 3, static_cast<int> (v[8] + 0.5f));
        const bool wild = v[9] >= 0.5f;
        const float inG = dbToGain (v[13]), outG = dbToGain (v[14]), grainAmt = v[1] * 0.01f, mu = v[11] * 0.01f, buzz = v[12] * 0.01f;
        for (int ch = 0; ch < 2; ++ch)
        {
            damp[ch].setHz (20000.0f * std::exp2 (-4.3f * v[2] * 0.01f), fs);
            tl[ch].set (Biquad::LowShelf, 800.0f, 0.5f, -v[7] * 0.06f, fs);
            th[ch].set (Biquad::HighShelf, 800.0f, 0.5f, v[7] * 0.06f, fs);
            scream[ch].set (Biquad::Bell, 1500.0f, 1.2f, type == 3 ? 9.0f : 0.0f, fs);
        }
        float* io[2] = { l, r };
        float pk = 0.0f;
        // pre: gain, damping, grain (random micro-delays), scream's mid push
        for (int i = 0; i < n; ++i)
        {
            gCount -= 1.0f;
            if (gCount <= 0.0f) { gCount = 0.004f * static_cast<float> (fs) * (0.5f + rng.next()); for (auto& t : gTarget) t = rng.next() * grainAmt * 0.004f * static_cast<float> (fs); }
            for (int ch = 0; ch < 2; ++ch)
            {
                float x = v[2] > 0.0f ? damp[ch].lp (io[ch][i] * inG) : io[ch][i] * inG;   // DAMPING 0 % is no filter
                pk = std::max (pk, std::abs (x));
                gNow[ch] += 0.01f * (gTarget[ch] - gNow[ch]);
                grain[ch].push (x);
                io[ch][i] = scream[ch].process (grainAmt > 0.001f ? grain[ch].read (1.0f + gNow[ch]) : x);
            }
        }
        lvl = std::max (pk, lvl * 0.85f); vis[0].store (lvl, std::memory_order_relaxed);
        // UNPREDICTABLE: the drive and its bias wander on their own
        wCount -= static_cast<float> (n);
        if (wCount <= 0.0f) { wCount = 0.15f * static_cast<float> (fs); wanderT = wild ? rng.bi() : 0.0f; }
        wander += 0.2f * (wanderT - wander);
        const float k = dbToGain (v[0] + wander * 6.0f), bias = wander * 0.3f, b0 = drive (type, bias);
        const float amt = clampf (v[0] / 6.0f + std::abs (wander), 0.0f, 1.0f);   // DRIVE 0 dB is clean; it fades in over 6 dB
        os.run (l, r, n, [&] (int, float x) { return x + (drive (type, k * x + bias) - b0 - x) * amt; });
        const float dlyMix = v[3] * 0.01f, fb = v[5] * 0.0095f, ab = v[6] * 0.01f, t = v[4] * 0.001f * static_cast<float> (fs);
        for (int i = 0; i < n; ++i)
        {
            const float e = env.process (0.5f * (l[i] + r[i]));
            buzzPh += 120.0f / static_cast<float> (fs); buzzPh -= std::floor (buzzPh);
            const float hum = buzzLp.lp (buzzPh * 2.0f - 1.0f) * buzz * 0.25f * std::min (1.0f, e * 6.0f);   // amp buzz that rides the playing
            abPh += 3.7f / static_cast<float> (fs); abPh -= std::floor (abPh);
            abT -= 1.0f; if (abT <= 0.0f) { abT = 0.3f * static_cast<float> (fs); abJump = rng.bi() * ab * 0.25f; }
            const float d = std::max (4.0f, t * (1.0f + ab * 0.03f * std::sin (kTwoPi * abPh) + abJump));
            for (int ch = 0; ch < 2; ++ch)
            {
                float y = io[ch][i] + hum;
                if (mu > 0.001f)   // mu-law: compress, quantise to 8 bits, expand
                {
                    const float a = std::log1p (255.0f * std::min (1.0f, std::abs (y))) / std::log1p (255.0f), q = std::round (a * 127.0f) / 127.0f;
                    const float back = std::copysign ((std::pow (256.0f, q) - 1.0f) / 255.0f, y);
                    y += mu * (back - y);
                }
                y = th[ch].process (tl[ch].process (dc[ch].hp (y)));
                const float echo = dly[ch].read (d);
                dly[ch].push (fixDenorm (y + softClip (echo * fb)));
                io[ch][i] = (y + dlyMix * echo) * outG;
            }
        }
    }
};

// ── 22. PARLOUR ───────────────────────────────────────────────────────────────────────────────
// A warm room with a plate's density (after the signal flow of Analog Obsession's ROOM041, our own
// voicing): tube-style DRIVE (±24 dB, asymmetric so it thickens), an HPF before the reverb, a short
// diffuser chain into the FDN for a plate-like instant build, STEREO separation on the wet signal,
// then a post EQ of a low and a high shelf. vis[0] = input level after the preamp.
struct Parlour final : Unit
{
    double fs = 48000.0;
    sg::DelayLine pre[2];
    Allpass diff[2][4];
    Fdn fdn;
    Biquad hpf[2], lo[2], hi[2];
    float lvl = 0.0f;
    void prepare (double s, int) override
    {
        fs = s; fdn.prepare (fs);
        static constexpr float d[4] = { 0.0031f, 0.0047f, 0.0083f, 0.0127f };
        for (int c = 0; c < 2; ++c)
        {
            pre[c].prepare (static_cast<int> (0.26 * fs));
            for (int k = 0; k < 4; ++k) { diff[c][k].prepare (static_cast<int> (0.02 * fs)); diff[c][k].len = d[k] * (c ? 1.063f : 1.0f) * static_cast<float> (fs); diff[c][k].g = 0.68f; }
        }
    }
    void reset() override
    {
        fdn.reset(); lvl = 0.0f;
        for (int c = 0; c < 2; ++c) { pre[c].clear(); hpf[c].reset(); lo[c].reset(); hi[c].reset(); for (auto& a : diff[c]) a.reset(); }
    }
    static float tube (float u) noexcept { return u >= 0.0f ? std::tanh (u) : 1.2f * std::tanh (u / 1.2f); }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const float k = dbToGain (v[0]), stereo = v[2] * 0.01f, pd = std::max (1.0f, v[3] * 0.001f * static_cast<float> (fs));
        fdn.set (0.32f, v[4], 8500.0f, 0.18f, false, 0.6f);
        for (int ch = 0; ch < 2; ++ch)
        {
            hpf[ch].set (Biquad::HP, v[1], 0.7071f, 0, fs);
            lo[ch].set (Biquad::LowShelf, v[5], 0.7071f, v[6], fs);
            hi[ch].set (Biquad::HighShelf, v[7], 0.7071f, v[11], fs);
        }
        float* io[2] = { l, r };
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float in[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                // preamp: below unity the drive is a clean pad; above it the tube curve takes over
                const float x = io[ch][i] * k, y = hpf[ch].process (k > 1.0f ? tube (x) : x);
                peak = std::max (peak, std::abs (y));
                pre[ch].push (y);
                float z = pre[ch].read (pd);
                for (auto& a : diff[ch]) z = a.process (z);
                in[ch] = z;
            }
            float ol, orr;
            fdn.process (in[0], in[1], ol, orr);
            width (ol, orr, stereo);
            io[0][i] = hi[0].process (lo[0].process (ol));
            io[1][i] = hi[1].process (lo[1].process (orr));
        }
        lvl = std::max (peak, lvl * 0.9f);
        vis[0].store (lvl, std::memory_order_relaxed);
    }
};
// ── 23. CARVE ─────────────────────────────────────────────────────────────────────────────────
// ShaperBox 3's way of working, in our own voice (user, 2026-10-05: "everything that the shaper box has").
// Eleven shapers in a chain - PITCH, REVERB, TIME, DRIVE, NOISE, LIQUID, FILTER, CRUSH, VOLUME, PAN, WIDTH -
// each switched on by itself, with its own 16-point wave (ctx.ext: shaper k's points at k*17, its curve at
// k*17+16), rate, trigger, band and mix. The wave is read in time with the host (SYNC; free at the tempo
// while it is stopped), restarted on each new note (NOTE) or each hit (TRANSIENT, then it plays once and
// holds its last point), or by the input level (FOLLOW: quiet left, loud right). BAND keeps a shaper to the
// low, mid or high band of a three-way split; the other bands pass by it.
// TIME has four modes: SHIFT (the wave is how far back to read: stutters, scratches), HALF-TIME (each cycle
// played at 1/RATIO speed), REVERSE (each cycle played backwards) and TAPE STOP (the wave is the speed).
// Params (v[]): shaper k's ON / MIX / RATE / TRIGGER / BAND at 5k..5k+4, then each shaper's own (see fxdefs).
// vis[0..10] = each shaper's position on its wave, vis[11] = input level (0..1).
struct Carve final : Unit
{
    static constexpr int kS = 11;
    enum { Pitch, Reverb, Time, Drive, Noise, Liquid, Filter, Crush, Volume, Pan, Width };
    static constexpr float kBeats[9] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f };
    static constexpr float kRatio[4] = { 1.5f, 2.0f, 3.0f, 4.0f };
    struct Shaper
    {
        double phase = 0.0;
        float w = 1.0f, lastPh = 0.0f;
        bool running = false;
        uint32_t notes = 0;
        LR4 xa[2], xb[2];
        float xaHz = 0.0f, xbHz = 0.0f;
    };
    double fs = 48000.0;
    Shaper sh[kS];
    EnvFollower level, fast, slow;
    bool armed = true;
    // the shapers' own state
    PitchShifter pitch[2];
    Fdn space;
    sg::DelayLine tape[2];
    float tLag = 0.0f, tFrom = 0.0f, tFade = 1.0f, tLast = 0.0f, prevOff = 0.0f;   // TIME: read offsets and the cross-fade
    Svf flt[2][2];
    sg::DelayLine liq[2];
    Allpass1 phs[2][8];
    float liqFb[2] {}, phLast[2] {};
    float held[2] {}, crushCount = 0.0f, crushHold = 1.0f;
    Rng rng;
    float pinkB[2][3] {}, brown[2] {}, crackle[2] {};
    OnePole noiseLp[2], noiseHp[2], driveTilt[2];
    float maxBack = 1.0f;

    void prepare (double s, int) override
    {
        fs = s;
        maxBack = static_cast<float> (8.0 * fs);
        for (int c = 0; c < 2; ++c)
        {
            tape[c].prepare (static_cast<int> (8.5 * fs));
            liq[c].prepare (static_cast<int> (0.05 * fs));
            pitch[c].prepare (fs, 50.0f);
        }
        space.prepare (fs);
        level.set (0.002f, 0.12f, fs); fast.set (0.0005f, 0.03f, fs); slow.set (0.03f, 0.3f, fs);
    }
    void reset() override
    {
        for (auto& x : sh) { x.phase = 0.0; x.w = 1.0f; x.running = false; x.xaHz = x.xbHz = 0.0f; }
        level.env = fast.env = slow.env = 0.0f; armed = true;
        tLag = tFrom = tLast = prevOff = 0.0f; tFade = 1.0f; crushCount = 0.0f;
        space.reset();
        for (int c = 0; c < 2; ++c)
        {
            tape[c].clear(); liq[c].clear(); pitch[c].reset();
            flt[c][0].reset(); flt[c][1].reset();
            for (auto& a : phs[c]) a.z = 0.0f;
            liqFb[c] = phLast[c] = held[c] = brown[c] = crackle[c] = 0.0f;
            pinkB[c][0] = pinkB[c][1] = pinkB[c][2] = 0.0f;
            noiseLp[c].reset(); noiseHp[c].reset(); driveTilt[c].reset();
        }
    }
    static float waveAt (const float* pts, int curve, float ph) noexcept
    {
        const float x = clampf (ph, 0.0f, 0.99999f) * 16.0f;
        const int i = static_cast<int> (x);
        const float f = x - static_cast<float> (i), a = pts[i], b = pts[(i + 1) & 15];
        if (curve == 0) return a;
        return a + (b - a) * (curve == 1 ? f : 0.5f - 0.5f * std::cos (kPi * f));
    }
    static int sel (float v) noexcept { return static_cast<int> (v + 0.5f); }

    // one shaper on one band of the signal (x in/out, stereo)
    void shape (int k, float w, float* x, const float* v, double cycleSamples, float env)
    {
        switch (k)
        {
            case Pitch:
            {
                float st = v[55] * (2.0f * w - 1.0f);
                if (sel (v[56]) == 1) st = std::round (st);
                const float blend = std::min (1.0f, std::abs (st) * 4.0f);   // no comb from the shifter at 0
                if (blend <= 0.0f) { for (int c = 0; c < 2; ++c) pitch[c].process (x[c], 1.0f); break; }
                for (int c = 0; c < 2; ++c) { const float y = pitch[c].process (x[c], std::exp2 (st / 12.0f)); x[c] += blend * (y - x[c]); }
                break;
            }
            case Reverb:
            {
                float l, r;
                space.process (x[0], x[1], l, r);
                x[0] += w * l; x[1] += w * r;
                break;
            }
            case Time:
            {
                const int mode = sel (v[60]);
                const float C = static_cast<float> (cycleSamples), ph = sh[Time].lastPh, e = ph * C;
                float off;
                switch (mode)
                {
                    case 0:  off = v[61] * 0.01f * (1.0f - w) * std::min (C, maxBack); break;            // SHIFT
                    case 1:  off = e * (1.0f - 1.0f / kRatio[sel (v[62]) & 3]); break;                    // HALF-TIME
                    case 2:  off = 2.0f * e; break;                                                     // REVERSE
                    default: tLag += 1.0f - w; off = tLag; break;                                       // TAPE STOP
                }
                // a new cycle jumps the read point back to now: cross-fade from where it was (10 ms)
                if (ph < tLast && mode != 0) { tFrom = prevOff; tFade = 0.0f; tLag = 0.0f; if (mode == 3) off = 0.0f; }
                tLast = ph;
                off = std::min (off, maxBack);
                const float fadeStep = 1.0f / static_cast<float> (0.010 * fs);
                for (int c = 0; c < 2; ++c)
                {
                    tape[c].push (x[c]);
                    float y = tape[c].read (1.0f + off);
                    if (tFade < 1.0f) y = y * tFade + tape[c].read (1.0f + std::min (tFrom, maxBack)) * (1.0f - tFade);
                    x[c] = mode == 0 ? y : x[c] + w * (y - x[c]);
                }
                if (tFade < 1.0f) tFade = std::min (1.0f, tFade + fadeStep);
                prevOff = off;
                break;
            }
            case Drive:
            {
                const float db = v[63] * w;
                if (db <= 0.01f) break;
                const float kk = dbToGain (db), amt = std::min (1.0f, db / 6.0f), tilt = v[65] * 0.01f;
                const int m = sel (v[64]);
                for (int c = 0; c < 2; ++c)
                {
                    const float u = x[c];
                    float y;
                    switch (m)
                    {
                        case 1:  y = clampf (kk * u, -1.0f, 1.0f) * (0.5f / std::min (1.0f, 0.5f * kk)); break;
                        case 2:  y = std::sin (kk * u) / std::sqrt (kk); break;
                        case 3:  { const float z = kk * u; y = (z >= 0.0f ? std::tanh (z) : 1.4f * std::tanh (z / 1.4f)) * (0.5f / std::tanh (0.5f * kk)); break; }
                        default: y = std::tanh (kk * u) * (0.5f / std::tanh (0.5f * kk)); break;
                    }
                    const float lo = driveTilt[c].lp (y), hi = y - lo;   // TONE: tilt about 1 kHz
                    x[c] = u + amt * (lo * (1.0f - tilt) + hi * (1.0f + tilt) - u);
                }
                break;
            }
            case Noise:
            {
                const int type = sel (v[67]), m = sel (v[68]);
                float g = dbToGain (v[66]) * w;
                if (m == 1) g *= std::min (1.0f, env * 2.0f);
                else if (m == 2) g *= 1.0f - std::min (1.0f, env * 4.0f);
                for (int c = 0; c < 2; ++c)
                {
                    noiseLp[c].setHz (v[69], fs);
                    const float wht = rng.bi();
                    float n;
                    auto& b = pinkB[c];
                    b[0] = 0.99765f * b[0] + wht * 0.0990460f; b[1] = 0.96300f * b[1] + wht * 0.2965164f; b[2] = 0.57000f * b[2] + wht * 1.0526913f;
                    const float pink = (b[0] + b[1] + b[2] + wht * 0.1848f) * 0.25f;
                    crackle[c] = crackle[c] * 0.55f + (rng.next() < 0.0006f ? rng.bi() : 0.0f);
                    switch (type)
                    {
                        case 1:  n = pink; break;
                        case 2:  noiseHp[c].setHz (5000.0f, fs); n = noiseHp[c].hp (wht); break;
                        case 3:  n = pink * 0.35f + crackle[c]; break;
                        case 4:  n = crackle[c]; break;
                        case 5:  brown[c] = fixDenorm (brown[c] * 0.995f + wht * 0.06f); n = brown[c] * 2.5f; break;
                        default: n = wht * 0.6f; break;
                    }
                    x[c] += noiseLp[c].lp (n) * g;
                }
                break;
            }
            case Liquid:
            {
                const int m = sel (v[70]);
                const float depth = v[71] * 0.01f, fb = v[72] * 0.01f, st = v[73] * 0.01f;
                for (int c = 0; c < 2; ++c)
                {
                    const float wc = c == 0 ? w : w + (1.0f - 2.0f * w) * st * 0.5f;
                    if (m == 1)   // PHASER: eight stages, the wave moves the centre
                    {
                        const float a = Allpass1::coeff (100.0f * std::exp2 (wc * depth * 6.5f), fs);
                        float y = x[c] + phLast[c] * fb * 0.9f;
                        for (auto& ap : phs[c]) y = ap.process (y, a);
                        phLast[c] = fixDenorm (y);
                        x[c] = 0.5f * (x[c] + y);
                    }
                    else          // FLANGER (short) or CHORUS (longer, no feedback)
                    {
                        const float ms = m == 0 ? 0.3f + wc * depth * 7.7f : 7.0f + wc * depth * 18.0f;
                        liq[c].push (fixDenorm (x[c] + liqFb[c]));
                        const float t = liq[c].read (std::max (1.0f, ms * 0.001f * static_cast<float> (fs)));
                        liqFb[c] = m == 0 ? softClip (t * fb) : 0.0f;
                        x[c] = 0.7f * (x[c] + t);
                    }
                }
                break;
            }
            case Filter:
            {
                const int type = sel (v[74]);
                const float lo = std::max (20.0f, v[75]), hi = std::max (lo, v[76]);
                const float fc = lo * std::pow (hi / lo, w), res = v[77] * 0.01f, drv = 1.0f + 4.0f * v[78] * 0.01f;
                for (int c = 0; c < 2; ++c)
                {
                    const float u = drv > 1.0f ? std::tanh (x[c] * drv) / std::sqrt (drv) : x[c];
                    auto& a = flt[c][0]; auto& b = flt[c][1];
                    a.set (fc, res, fs);
                    a.process (u);
                    float y;
                    switch (type)
                    {
                        case 1:  b.set (fc, res * 0.5f, fs); b.process (a.lo); y = b.lo; break;
                        case 2:  y = a.hi; break;
                        case 3:  b.set (fc, res * 0.5f, fs); b.process (a.hi); y = b.hi; break;
                        case 4:  y = a.bp; break;
                        case 5:  y = a.lo + a.hi; break;
                        case 6:  y = u + a.bp * (1.0f + 3.0f * res); break;
                        default: y = a.lo; break;
                    }
                    x[c] = y;
                }
                break;
            }
            case Crush:
            {
                if (w < 0.001f) break;
                const float bits = 16.0f - (16.0f - v[79]) * w, rate = static_cast<float> (fs) * std::pow (v[80] / static_cast<float> (fs), w);
                if ((crushCount += 1.0f) >= crushHold)
                {
                    crushCount -= crushHold;
                    crushHold = std::max (1.0f, static_cast<float> (fs) / rate * (1.0f + v[81] * 0.01f * 0.5f * rng.bi()));
                    const float steps = std::exp2 (bits - 1.0f);
                    for (int c = 0; c < 2; ++c) held[c] = std::round (x[c] * steps) / steps;
                }
                x[0] = held[0]; x[1] = held[1];
                break;
            }
            case Volume: { const float g = 1.0f - v[82] * 0.01f * (1.0f - w); x[0] *= g; x[1] *= g; break; }
            case Pan:    { float gl, gr; panGains (v[83] * 0.01f * (2.0f * w - 1.0f), gl, gr); x[0] *= gl; x[1] *= gr; break; }
            default:     width (x[0], x[1], 1.0f + v[84] * 0.01f * (2.0f * w - 1.0f)); break;
        }
    }

    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        static const float zero[fxdefs::kExt] = {};
        const float* ext = c.ext != nullptr ? c.ext : zero;
        const double ppqStep = c.bpm / (60.0 * fs);
        const float sens = v[87] * 0.01f, floorDb = -12.0f - 48.0f * sens, trigRatio = dbToGain (12.0f - 10.0f * sens);
        const float outG = dbToGain (v[88]), sm = smoothCoeff (0.0015f, fs);
        bool on[kS];
        double cycle[kS];
        for (int k = 0; k < kS; ++k)
        {
            on[k] = v[5 * k] >= 0.5f;
            cycle[k] = beatsToSeconds (kBeats[juce::jlimit (0, 8, sel (v[5 * k + 2]))], c.bpm) * fs;
            auto& s = sh[k];
            if (sel (v[5 * k + 4]) > 0 && (v[85] != s.xaHz || v[86] != s.xbHz))
            {
                for (int ch = 0; ch < 2; ++ch) { s.xa[ch].setKeep (v[85], fs); s.xb[ch].setKeep (v[86], fs); }
                s.xaHz = v[85]; s.xbHz = v[86];
            }
            if (k == Reverb && on[k]) space.set (0.2f + 0.8f * v[57] * 0.01f, v[58], 1500.0f * std::pow (12.0f, (v[59] + 100.0f) / 200.0f), 0.3f, false, 0.4f);
        }
        float envNow = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float mono = 0.5f * (l[i] + r[i]);
            const float env = level.process (mono), fe = fast.process (mono), se = slow.process (mono);
            envNow = env;
            bool hit = false;
            if (armed && fe > se * trigRatio && fe > 0.003f) { hit = true; armed = false; }
            else if (fe < se * 1.1f) armed = true;
            float x[2] = { l[i], r[i] };
            for (int k = 0; k < kS; ++k)
            {
                auto& s = sh[k];
                // where on its wave this shaper is
                float ph;
                const int trig = sel (v[5 * k + 3]);
                if (trig == 3) ph = clampf ((gainToDb (env) - floorDb) / -floorDb, 0.0f, 0.99999f);
                else
                {
                    const double inc = 1.0 / std::max (1.0, cycle[k]);
                    if (trig == 2)
                    {
                        if (hit) { s.phase = 0.0; s.running = true; }
                        if (s.running && (s.phase += inc) >= 1.0) { s.phase = 0.99999; s.running = false; }
                    }
                    else if (trig == 1)
                    {
                        if (c.noteOns != s.notes) { s.notes = c.noteOns; s.phase = 0.0; }
                        if ((s.phase += inc) >= 1.0) s.phase -= 1.0;
                    }
                    else if (c.playing)
                    {
                        const double q = (c.ppq + i * ppqStep) / kBeats[juce::jlimit (0, 8, sel (v[5 * k + 2]))];
                        s.phase = q - std::floor (q);
                    }
                    else if ((s.phase += inc) >= 1.0) s.phase -= 1.0;
                    ph = static_cast<float> (s.phase);
                }
                s.lastPh = ph;
                if (! on[k]) continue;
                const float* pts = ext + k * 17;
                s.w += sm * (waveAt (pts, juce::jlimit (0, 2, sel (pts[16])), ph) - s.w);

                // its band (the rest passes by), then its own mix
                const int band = sel (v[5 * k + 4]);
                float y[2] = { x[0], x[1] }, rest[2] = { 0.0f, 0.0f };
                if (band > 0)
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        float lo, up, mid, hi;
                        s.xa[ch].split (x[ch], lo, up);
                        s.xb[ch].split (up, mid, hi);
                        y[ch] = band == 1 ? lo : band == 2 ? mid : hi;
                        rest[ch] = band == 1 ? mid + hi : band == 2 ? lo + hi : lo + mid;
                    }
                const float d0 = y[0], d1 = y[1], mix = v[5 * k + 1] * 0.01f;
                shape (k, s.w, y, v, cycle[k], env);
                x[0] = rest[0] + d0 + mix * (y[0] - d0);
                x[1] = rest[1] + d1 + mix * (y[1] - d1);
            }
            l[i] = x[0] * outG;
            r[i] = x[1] * outG;
        }
        for (int k = 0; k < kS; ++k) vis[static_cast<size_t> (k)].store (sh[k].lastPh, std::memory_order_relaxed);
        vis[11].store (clampf ((gainToDb (envNow) + 60.0f) / 60.0f, 0.0f, 1.0f), std::memory_order_relaxed);
    }
};

// ── 24. POISE ─────────────────────────────────────────────────────────────────────────────────
// An adaptive tone shaper (a near-identical remake of oeksound bloom's behaviour, our own code). Twelve
// bands, ~0.8 octave apart, listen to the sound and lean it towards a balanced target: equal energy per
// band (pink) with a little more at both ends, plus the four TONE handles. The correction keeps the
// loudness: the gains are moved so that, weighted by where the energy is, they average 0 dB. AMOUNT
// (0-10) is how hard it leans; past 7 each band's fast level is also pulled towards its slow level above
// SQUASH CAL (upward and downward). STEREO detects on the sum and moves both sides together; MID / SIDE
// shapes each on its own. DELTA plays only what POISE changed. AMOUNT 0 leaves the sound untouched.
// vis[0..11] = each band's gain now (dB, mid/stereo), vis[12] = squash at work (dB), vis[13] = level 0..1.
struct Poise final : Unit
{
    static constexpr int kB = 12;
    double fs = 48000.0;
    float fc[kB] {};
    Biquad det[2][kB], eq[2][kB];
    float slowP[2][kB] {}, fastP[2][kB] {}, gain[2][kB] {};
    float lvl = 0.0f, squashVis = 0.0f, pIn = 0.0f, pOut = 0.0f, match = 1.0f;
    int tick = 0;
    void prepare (double s, int) override
    {
        fs = s;
        for (int k = 0; k < kB; ++k)
        {
            fc[k] = 30.0f * std::exp2 (0.82f * static_cast<float> (k));
            for (auto& d : det) d[k].set (Biquad::BP, fc[k], 1.75f, 0.0f, fs);
        }
        reset();
    }
    void reset() override
    {
        tick = 0; lvl = squashVis = pIn = pOut = 0.0f; match = 1.0f;
        for (int g = 0; g < 2; ++g)
            for (int k = 0; k < kB; ++k)
            {
                det[g][k].reset(); slowP[g][k] = fastP[g][k] = gain[g][k] = 0.0f;
                eq[g][k].set (Biquad::Bell, fc[k], 1.1f, 0.0f, fs); eq[g][k].reset();
            }
    }
    // where the target sits for each band (dB, relative): a gentle smile plus the four handles
    static float shape (float f, const float* v) noexcept
    {
        float s = 0.45f * std::abs (std::log2 (f / 1000.0f));
        for (int h = 0; h < 4; ++h)
        {
            const float o = std::log2 (f / v[11 + 2 * h]);
            s += v[12 + 2 * h] * std::exp (-o * o / (2.0f * 0.8f * 0.8f));
        }
        return s;
    }
    void update (int g, const float* v)
    {
        const float amount = v[0], strength = std::min (amount, 7.0f) / 7.0f * 0.6f, squash = std::max (0.0f, (amount - 7.0f) / 3.0f);
        float L[kB], T[kB], sum = 0.0f, top = -200.0f, meanL = 0.0f, meanT = 0.0f;
        for (int k = 0; k < kB; ++k) { L[k] = 10.0f * std::log10 (slowP[g][k] + 1.0e-12f); sum += slowP[g][k]; top = std::max (top, L[k]); }
        if (sum < 1.0e-8f) return;   // silence: hold the last correction rather than boost the noise floor
        for (int k = 0; k < kB; ++k) { L[k] = std::max (L[k], top - 45.0f); T[k] = shape (fc[k], v); meanL += L[k]; meanT += T[k]; }
        meanL /= kB; meanT /= kB;
        float target[kB], avg = 0.0f;
        for (int k = 0; k < kB; ++k)
        {
            target[k] = clampf (strength * (meanL + T[k] - meanT - L[k]), -12.0f, 12.0f);
            avg += target[k] * slowP[g][k] / sum;
        }
        float sq = 0.0f;
        for (int k = 0; k < kB; ++k)
        {
            float t = target[k] - avg;   // the loudest bands stay put; the match below keeps the rest honest
            if (squash > 0.0f)
            {
                const float F = 10.0f * std::log10 (fastP[g][k] + 1.0e-12f);
                if (F > v[3]) { const float d = clampf (-squash * 0.7f * (F - L[k]), -12.0f, 6.0f); t += d; sq += d; }
            }
            gain[g][k] += 0.5f * (clampf (t, -12.0f, 9.0f) - gain[g][k]);
        }
        if (g == 0) squashVis = sq / kB;
    }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const bool ms = v[8] >= 0.5f, delta = v[9] >= 0.5f, on = v[0] > 0.0f;
        const float trim = dbToGain (v[4]);
        const float att = 0.002f * std::pow (250.0f, v[1] * 0.1f), rel = 0.02f * std::pow (100.0f, v[2] * 0.1f);
        const float ca = smoothCoeff (att, fs), cr = smoothCoeff (rel, fs), fa = smoothCoeff (0.005f, fs), fr = smoothCoeff (0.06f, fs);
        const float cm = smoothCoeff (0.3f, fs);   // loudness match, ~300 ms
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float xl = l[i], xr = r[i];
            peak = std::max (peak, std::max (std::abs (xl), std::abs (xr)));
            if (! on) { l[i] = delta ? 0.0f : xl * trim; r[i] = delta ? 0.0f : xr * trim; continue; }
            float a = xl, b = xr;
            if (ms) { a = 0.5f * (xl + xr); b = 0.5f * (xl - xr); }
            const float dsig[2] = { ms ? a : 0.5f * (xl + xr), b };
            for (int g = 0; g < (ms ? 2 : 1); ++g)
                for (int k = 0; k < kB; ++k)
                {
                    const float y = det[g][k].process (dsig[g]), p = y * y;
                    slowP[g][k] = fixDenorm (slowP[g][k] + (p > slowP[g][k] ? ca : cr) * (p - slowP[g][k]));
                    fastP[g][k] = fixDenorm (fastP[g][k] + (p > fastP[g][k] ? fa : fr) * (p - fastP[g][k]));
                }
            if ((tick++ & 31) == 0)
            {
                update (0, v);
                if (ms) update (1, v);
                for (int ch = 0; ch < 2; ++ch)
                    for (int k = 0; k < kB; ++k)
                    {
                        // keep the filter's state, change only its curve
                        Biquad nb; nb.set (Biquad::Bell, fc[k], 1.1f, gain[ms ? ch : 0][k], fs);
                        eq[ch][k].b0 = nb.b0; eq[ch][k].b1 = nb.b1; eq[ch][k].b2 = nb.b2; eq[ch][k].a1 = nb.a1; eq[ch][k].a2 = nb.a2;
                    }
            }
            for (int k = 0; k < kB; ++k) { a = eq[0][k].process (a); b = eq[1][k].process (b); }
            float yl = a, yr = b;
            if (ms) { yl = a + b; yr = a - b; }
            // keep the loudness: output RMS against input RMS (held through silence), as VALVE does
            const float ei = xl * xl + xr * xr, eo = yl * yl + yr * yr;
            pIn = fixDenorm (pIn + cm * (ei - pIn)); pOut = fixDenorm (pOut + cm * (eo - pOut));
            if (pIn > 1.0e-8f && pOut > 1.0e-12f) match += 0.001f * (clampf (std::sqrt (pIn / pOut), 0.25f, 4.0f) - match);
            yl *= match; yr *= match;
            l[i] = (delta ? yl - xl : yl) * trim;
            r[i] = (delta ? yr - xr : yr) * trim;
        }
        lvl = std::max (peak, lvl * 0.9f);
        for (int k = 0; k < kB; ++k) vis[static_cast<size_t> (k)].store (on ? gain[0][k] : 0.0f, std::memory_order_relaxed);
        vis[12].store (on ? squashVis : 0.0f, std::memory_order_relaxed);
        vis[13].store (std::min (1.0f, lvl), std::memory_order_relaxed);
    }
};

// ── 25. RIFT ──────────────────────────────────────────────────────────────────────────────────
// A granular portal (after Output's Portal, made our own): the input goes through a DELAY (free or in
// sync) into a 9 s buffer that FREEZE stops writing; grains are read from it at DENSITY, SIZE long, up to
// SPRAY into the past, pitched by PITCH and kept to a SCALE, some REVERSED, panned within SPREAD. The
// grains are filtered (LOW CUT / HIGH CUT), fed back into the delay (FEEDBACK) and sent into a SPACE.
// The pad: SCATTER randomises pitch (within the scale), pan, size and position together; BLOOM feeds the
// grains back on themselves and opens and lengthens the space.
// vis[0] = grains sounding (0..1), vis[1] = output level, vis[2] = frozen.
struct Rift final : Unit
{
    static constexpr int kGrains = 96;
    struct Grain { bool on = false, rev = false; float start = 0, pos = 0, len = 1, ratio = 1, gl = 1, gr = 1; };
    double fs = 48000.0;
    std::vector<float> buf;
    int mask = 0, w = 0;
    Grain grains[kGrains];
    sg::DelayLine dly[2];
    float due = 0.0f, fb[2] {}, lvl = 0.0f;
    Rng rng;
    Biquad lc[2], hc[2];
    Fdn space;
    void prepare (double s, int) override
    {
        fs = s;
        int size = 1; while (size < static_cast<int> (9.0 * fs)) size <<= 1;
        buf.assign (static_cast<size_t> (size), 0.0f); mask = size - 1;
        for (auto& d : dly) d.prepare (static_cast<int> (2.1 * fs));
        space.prepare (fs);
    }
    void reset() override
    {
        std::fill (buf.begin(), buf.end(), 0.0f); w = 0; due = 0.0f; lvl = 0.0f;
        for (auto& g : grains) g.on = false;
        for (int c = 0; c < 2; ++c) { dly[c].clear(); lc[c].reset(); hc[c].reset(); fb[c] = 0.0f; }
        space.reset();
    }
    float at (float p) const noexcept
    {
        const float fl = std::floor (p); const int i = static_cast<int> (fl); const float f = p - fl;
        return buf[static_cast<size_t> (i & mask)] * (1.0f - f) + buf[static_cast<size_t> ((i + 1) & mask)] * f;
    }
    // the nearest semitone the scale allows (root C = 0)
    static float toScale (float st, int scale) noexcept
    {
        static constexpr uint16_t masks[6] = { 0xFFF, 0xAB5, 0x5AD, 0x295, 0x001, 0x081 };   // chromatic, major, minor, pentatonic, octaves, fifths
        const int base = static_cast<int> (std::lround (st));
        for (int d = 0; d <= 12; ++d)
            for (const int s : { base - d, base + d })
                if (masks[scale] & (1u << (((s % 12) + 12) % 12))) return static_cast<float> (s);
        return static_cast<float> (base);
    }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const float fsf = static_cast<float> (fs), scatter = v[11] * 0.01f, bloom = v[12] * 0.01f;
        const float len = v[0] * 0.001f * fsf, dens = v[1], spray = v[3] * 0.001f * fsf, revP = v[13] * 0.01f;
        const float fbAmt = std::min (0.95f, v[4] * 0.01f + bloom * 0.35f), send = std::min (1.0f, v[7] * 0.01f + bloom * 0.5f);
        const float spread = std::min (1.0f, v[6] * 0.01f + scatter * 0.4f);
        const float delayS = v[10] >= 0.5f ? syncedSeconds (v[5], 10.0f, 2000.0f, c.bpm) : v[5] * 0.001f;
        const float dSamp = clampf (delayS * fsf, 1.0f, 2.05f * fsf);
        const int scale = juce::jlimit (0, 5, static_cast<int> (v[8] + 0.5f)), shape = juce::jlimit (0, 3, static_cast<int> (v[9] + 0.5f));
        const bool frozen = v[16] >= 0.5f;
        const float norm = 1.0f / std::sqrt (std::max (1.0f, dens * v[0] * 0.001f));
        for (int ch = 0; ch < 2; ++ch) { lc[ch].set (Biquad::HP, v[14], 0.7071f, 0, fs); hc[ch].set (Biquad::LP, v[15], 0.7071f, 0, fs); }
        space.set (0.85f, 1.2f + bloom * 7.0f, 7000.0f, 0.6f, false, 0.3f);
        int sounding = 0;
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            dly[0].push (fixDenorm (l[i] + fb[0])); dly[1].push (fixDenorm (r[i] + fb[1]));
            if (! frozen) { buf[static_cast<size_t> (w)] = 0.5f * (dly[0].read (dSamp) + dly[1].read (dSamp)); w = (w + 1) & mask; }
            due -= dens / fsf;
            if (due <= 0.0f)
            {
                due += 1.0f + rng.bi() * (0.15f + scatter * 0.6f);
                for (auto& g : grains)
                    if (! g.on)
                    {
                        const float gl = std::max (32.0f, len * (1.0f + rng.bi() * scatter * 0.6f));
                        const float ratio = std::exp2 (toScale (v[2] + rng.bi() * scatter * 12.0f, scale) / 12.0f);
                        const float back = std::min (static_cast<float> (mask) - 8.0f, gl * std::max (1.0f, ratio) + 4.0f + rng.next() * spray * (1.0f + scatter));
                        g = { true, rng.next() < revP, static_cast<float> (w) - back, 0.0f, gl, ratio, 1, 1 };
                        panGains (rng.bi() * spread, g.gl, g.gr);
                        break;
                    }
            }
            float y[2] = { 0.0f, 0.0f };
            for (auto& g : grains)
            {
                if (! g.on) continue;
                const float x = at (g.start + (g.rev ? g.len - g.pos : g.pos) * g.ratio) * Autochroma::envelope (shape, g.pos / g.len);
                y[0] += x * g.gl; y[1] += x * g.gr;
                if ((g.pos += 1.0f) >= g.len) g.on = false;
            }
            for (int ch = 0; ch < 2; ++ch)
            {
                y[ch] = hc[ch].process (lc[ch].process (y[ch] * norm));
                fb[ch] = softClip (y[ch] * fbAmt);
            }
            float sl, sr;
            space.process (y[0], y[1], sl, sr);
            l[i] = y[0] * (1.0f - 0.3f * send) + send * sl;
            r[i] = y[1] * (1.0f - 0.3f * send) + send * sr;
            peak = std::max (peak, std::max (std::abs (l[i]), std::abs (r[i])));
        }
        for (const auto& g : grains) sounding += g.on ? 1 : 0;
        lvl = std::max (peak, lvl * 0.9f);
        vis[0].store (static_cast<float> (sounding) / kGrains, std::memory_order_relaxed);
        vis[1].store (std::min (1.0f, lvl), std::memory_order_relaxed);
        vis[2].store (frozen ? 1.0f : 0.0f, std::memory_order_relaxed);
    }
};
} // namespace

static std::unique_ptr<Unit> createUnit (int type)
{
    switch (type)
    {
        case fxdefs::Psdelay:    return std::make_unique<PsDelay>();
        case fxdefs::Tapeecho:   return std::make_unique<EchoDelay>();
        case fxdefs::Convolver:  return std::make_unique<Convolver>();
        case fxdefs::Tuba:       return std::make_unique<Tuba>();
        case fxdefs::Saturn:     return std::make_unique<Saturn>();
        case fxdefs::Distortion: return std::make_unique<Distortion>();
        case fxdefs::Bitcrusher: return std::make_unique<Bitcrusher>();
        case fxdefs::Vulf:       return std::make_unique<Vulf>();
        case fxdefs::Faraday:    return std::make_unique<Faraday>();
        case fxdefs::Mbcomp:     return std::make_unique<Multiband>();
        case fxdefs::Phaser:     return std::make_unique<PhaserPan>();
        case fxdefs::Flanger:    return std::make_unique<Flanger>();
        case fxdefs::Stereopan:  return std::make_unique<StereoPan>();
        case fxdefs::Proq:       return std::make_unique<ProQ>();
        case fxdefs::Filter:     return std::make_unique<Filter>();
        case fxdefs::Autochroma: return std::make_unique<Autochroma>();
        case fxdefs::Valleyverb: return std::make_unique<ValleyVerb>();
        case fxdefs::Imager:     return std::make_unique<Imager>();
        case fxdefs::Nudestort:  return std::make_unique<Nudestort>();
        case fxdefs::Parlour:    return std::make_unique<Parlour>();
        case fxdefs::Carve:      return std::make_unique<Carve>();
        case fxdefs::Poise:      return std::make_unique<Poise>();
        case fxdefs::Rift:       return std::make_unique<Rift>();
        default:                 return std::make_unique<Bypass>();
    }
}

std::unique_ptr<Unit> makeUnit (int type)
{
    auto u = createUnit (type);
    u->type = type;
    return u;
}
} // namespace fx
