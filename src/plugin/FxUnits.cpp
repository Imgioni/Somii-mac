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

// ── 2. REV OCEAN ──────────────────────────────────────────────────────────────────────────────
// FDN tank with three motion modes: ABYSS (reversed octave-up tail folded back in), TIDE (a slow
// band-pass sweeping the tail), FOAM (diffusion before the tank, so the attack swells).
struct RevOcean final : Unit
{
    double fs = 48000.0;
    Fdn fdn;
    sg::DelayLine pre[2];
    OnePole lowCut[2];
    Allpass diff[2][4];
    PitchShifter abyss[2];
    Svf tide[2];
    EnvFollower duck;
    float tidePh = 0.0f, last[2] {};

    void prepare (double s, int) override
    {
        fs = s;
        fdn.prepare (fs);
        static constexpr float dl[4] = { 0.0047f, 0.0081f, 0.0119f, 0.0163f };
        for (int c = 0; c < 2; ++c)
        {
            pre[c].prepare (static_cast<int> (0.26 * fs));
            abyss[c].prepare (fs, 180.0f);
            for (int k = 0; k < 4; ++k) { diff[c][k].prepare (static_cast<int> (0.02 * fs)); diff[c][k].len = dl[k] * static_cast<float> (fs) * (c ? 1.07f : 1.0f); }
        }
        duck.set (0.005f, 0.25f, fs);
    }
    void reset() override
    {
        fdn.reset();
        for (int c = 0; c < 2; ++c) { pre[c].clear(); lowCut[c].reset(); abyss[c].reset(); tide[c].reset(); for (auto& a : diff[c]) a.reset(); last[c] = 0.0f; }
        duck.env = 0.0f;
    }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const int mode = static_cast<int> (v[8]);
        const bool freeze = v[9] >= 0.5f;
        const float mac = v[3] * 0.01f, pd = std::max (1.0f, v[4] * 0.001f * static_cast<float> (fs));
        const float wid = v[5] * 0.01f * 1.5f, dk = v[6] * 0.01f;
        fdn.set (v[0] * 0.01f, v[1], v[2], 0.3f + (mode == 1 ? mac : 0.0f), freeze);
        for (int ch = 0; ch < 2; ++ch) lowCut[ch].setHz (v[7], fs);
        for (int ch = 0; ch < 2; ++ch) for (auto& a : diff[ch]) a.g = mode == 2 ? 0.5f + 0.25f * mac : 0.0f;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float dry = 0.5f * (l[i] + r[i]);
            const float env = duck.process (dry);
            float in[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                pre[ch].push (lowCut[ch].hp (io[ch][i]));
                float x = freeze ? 0.0f : pre[ch].read (pd);
                if (mode == 2) for (auto& a : diff[ch]) x = a.process (x);                 // FOAM
                if (mode == 0) x += softClip (abyss[ch].process (last[ch], -2.0f) * mac * 0.45f);   // ABYSS
                in[ch] = x;
            }
            float ol, orr;
            fdn.process (in[0], in[1], ol, orr);
            last[0] = ol; last[1] = orr;
            if (mode == 1)   // TIDE: a band-pass drifting over two octaves, crossfaded in by MACRO
            {
                tidePh += 0.08f / static_cast<float> (fs); if (tidePh >= 1.0f) tidePh -= 1.0f;
                if ((i & 15) == 0)
                    for (int ch = 0; ch < 2; ++ch) tide[ch].set (900.0f * std::exp2 (2.0f * std::sin (kTwoPi * (tidePh + 0.25f * ch))), 0.55f, fs);
                tide[0].process (ol); tide[1].process (orr);
                ol += mac * (tide[0].bp * 2.0f - ol); orr += mac * (tide[1].bp * 2.0f - orr);
            }
            width (ol, orr, wid);
            const float g = 1.0f - dk * std::min (1.0f, env * 4.0f);
            l[i] = ol * g; r[i] = orr * g;
        }
    }
};

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

// ── 18. AMBIENT ───────────────────────────────────────────────────────────────────────────────
// A mode "tone" processor into a modulated FDN space; TONE × SPACE from the XY pad.
struct Ambient final : Unit
{
    double fs = 48000.0;
    Fdn fdn;
    sg::DelayLine pre[2], woven[2];
    OnePole lc[2], hc[2];
    PitchShifter rev[2], shim[2], octDn[2], octUp[2];
    Svf sunk[2];
    EnvFollower duck;
    float wph = 0.0f, last[2] {}, held[2] {}, cnt = 0.0f;
    void prepare (double s, int) override
    {
        fs = s; fdn.prepare (fs);
        for (int c = 0; c < 2; ++c)
        {
            pre[c].prepare (static_cast<int> (0.26 * fs)); woven[c].prepare (static_cast<int> (0.05 * fs));
            rev[c].prepare (fs, 300.0f); shim[c].prepare (fs, 120.0f); octDn[c].prepare (fs, 90.0f); octUp[c].prepare (fs, 90.0f);
        }
        duck.set (0.005f, 0.25f, fs);
    }
    void reset() override
    {
        fdn.reset(); duck.env = 0.0f;
        for (int c = 0; c < 2; ++c) { pre[c].clear(); woven[c].clear(); lc[c].reset(); hc[c].reset(); rev[c].reset(); shim[c].reset(); octDn[c].reset(); octUp[c].reset(); sunk[c].reset(); last[c] = held[c] = 0.0f; }
    }
    void process (float* l, float* r, int n, const Ctx& c) override
    {
        const float* v = c.v;
        const int mode = static_cast<int> (v[8]);
        const float T = v[11] * 0.01f, S = v[12] * 0.01f;
        fdn.set (v[0] * 0.01f * (0.5f + 0.5f * S), v[1] * (0.5f + S), 9000.0f, v[3] * 0.01f, false);
        const float pd = std::max (1.0f, v[4] * 0.001f * static_cast<float> (fs)), dk = v[5] * 0.01f, send = 0.3f + 0.7f * S;
        for (int ch = 0; ch < 2; ++ch) { lc[ch].setHz (v[6], fs); hc[ch].setHz (v[7], fs); sunk[ch].set (200.0f + 2400.0f * (1.0f - T), 0.7f, fs); }
        const float step = 2.0f / std::exp2 (16.0f - 12.0f * T), hold = 1.0f + T * 8.0f;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float env = duck.process (0.5f * (l[i] + r[i]));
            wph += 0.3f / static_cast<float> (fs); wph -= std::floor (wph);
            cnt -= 1.0f; const bool take = cnt <= 0.0f; if (take) cnt += hold;
            float in[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                pre[ch].push (hc[ch].lp (lc[ch].hp (io[ch][i])));
                float x = pre[ch].read (pd);
                switch (mode)
                {
                    case 0: x += T * rev[ch].process (x, -1.0f); break;                                             // REFLECT
                    case 1:                                                                                         // WOVEN
                        woven[ch].push (x);
                        for (int k = 0; k < 3; ++k) x += T * 0.4f * woven[ch].read ((0.011f + 0.009f * k) * static_cast<float> (fs) * (1.0f + 0.2f * std::sin (kTwoPi * (wph + 0.33f * k + 0.17f * ch))));
                        break;
                    case 2: x += softClip (T * 0.5f * shim[ch].process (last[ch], 2.0f)); break;                     // SIREN
                    case 3: x += T * 0.5f * (octDn[ch].process (x, 0.5f) + octUp[ch].process (x, 2.0f)); break;      // ORGANIST
                    case 4: if (take) held[ch] = std::round (x / step) * step; x = x + T * (held[ch] - x); break;   // CODEC
                    default: break;
                }
                in[ch] = x * send;
            }
            float ol, orr;
            fdn.process (in[0], in[1], ol, orr);
            last[0] = ol; last[1] = orr;
            if (mode == 5) { sunk[0].process (ol); sunk[1].process (orr); ol += T * (sunk[0].lo - ol); orr += T * (sunk[1].lo - orr); }   // SUNKEN
            width (ol, orr, v[2] * 0.01f * 1.5f);
            const float g = 1.0f - dk * std::min (1.0f, env * 4.0f);
            l[i] = ol * g; r[i] = orr * g;
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
} // namespace

static std::unique_ptr<Unit> createUnit (int type)
{
    switch (type)
    {
        case fxdefs::Psdelay:    return std::make_unique<PsDelay>();
        case fxdefs::Revocean:   return std::make_unique<RevOcean>();
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
        case fxdefs::Ambient:    return std::make_unique<Ambient>();
        case fxdefs::Valleyverb: return std::make_unique<ValleyVerb>();
        case fxdefs::Imager:     return std::make_unique<Imager>();
        case fxdefs::Nudestort:  return std::make_unique<Nudestort>();
        case fxdefs::Parlour:    return std::make_unique<Parlour>();
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
