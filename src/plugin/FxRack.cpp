#include "FxRack.h"
#include "FxDsp.h"

namespace fx
{
namespace
{
juce::String slotId (int s, const char* leaf) { return "fx" + juce::String (s + 1) + "." + leaf; }
}

void Rack::bind (juce::AudioProcessorValueTreeState& state)
{
    mode = state.getRawParameterValue ("fx.mode");
    for (int s = 0; s < kSlots; ++s)
    {
        auto& sl = slots[static_cast<size_t> (s)];
        sl.type = state.getRawParameterValue (slotId (s, "type"));
        sl.on = state.getRawParameterValue (slotId (s, "on"));
        sl.mix = state.getRawParameterValue (slotId (s, "mix"));
        sl.layer = state.getRawParameterValue (slotId (s, "layer"));
        sl.typeParam = state.getParameter (slotId (s, "type"));
        sl.mixParam = state.getParameter (slotId (s, "mix"));
        sl.onParam = state.getParameter (slotId (s, "on"));
        sl.layerParam = state.getParameter (slotId (s, "layer"));
        for (int i = 0; i < fxdefs::kNP; ++i)
        {
            const auto id = slotId (s, ("p" + juce::String (i + 1)).toRawUTF8());
            sl.p[static_cast<size_t> (i)] = state.getRawParameterValue (id);
            sl.pParam[static_cast<size_t> (i)] = state.getParameter (id);
        }
    }
    makeDefaultImpulse();
}

Rack::~Rack()
{
    for (auto& s : slots)
        for (int l = 0; l < 2; ++l)
        {
            auto* a = s.active[static_cast<size_t> (l)];
            auto* p = s.pending[static_cast<size_t> (l)].exchange (nullptr);
            auto* r = s.retired[static_cast<size_t> (l)].exchange (nullptr);
            delete a;
            if (p != a) delete p;
            if (r != a && r != p) delete r;
        }
}

void Rack::takePending (Slot& s) noexcept
{
    for (size_t l = 0; l < 2; ++l)
        if (auto* u = s.pending[l].exchange (nullptr))
        {
            s.retired[l].store (s.active[l]);   // the message thread only publishes once `retired` is empty
            s.active[l] = u;
            s.fade[l] = 0.0f;
        }
}

void Rack::prepare (double sampleRate, int block)
{
    fs = sampleRate;
    maxBlock = std::max (1, block);
    for (auto& v : dry) v.assign (static_cast<size_t> (maxBlock), 0.0f);
    for (auto& v : tmp) v.assign (static_cast<size_t> (maxBlock), 0.0f);
    for (auto& s : slots)
    {
        takePending (s);
        for (size_t l = 0; l < 2; ++l)
        {
            delete s.retired[l].exchange (nullptr);
            if (auto* u = s.active[l]) { u->prepare (fs, maxBlock); u->reset(); }
            s.amt[l] = 0.0f;
        }
    }
    irChanged = true;
}

void Rack::resetToDefaults (Slot& s, int type)
{
    auto set = [] (juce::RangedAudioParameter* p, float v)
    {
        if (p == nullptr) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (v);
        p->endChangeGesture();
    };
    for (int i = 0; i < fxdefs::kNP; ++i)
        set (s.pParam[static_cast<size_t> (i)], fxdefs::kParams[type][i].def);
    set (s.mixParam, fxdefs::kDefaultMix[type]);
    // a new effect: its own extra values, and no modulation (the old routes meant other controls)
    setExtDefaults (s, type);
    for (auto& m : s.mod) m.store (0.0f, std::memory_order_relaxed);
    for (auto& m : s.modNow) m.store (0.0f, std::memory_order_relaxed);
    countMods (s);
    ++dataVersion;
}

void Rack::setExtDefaults (Slot& s, int type) noexcept
{
    const float* d = fxdefs::kExtDefaults[type];
    for (int i = 0; i < fxdefs::kExt; ++i)
        s.ext[static_cast<size_t> (i)].store (d != nullptr && i < fxdefs::kExtLen[type] ? d[i] : 0.0f, std::memory_order_relaxed);
    s.extSet = true;
}

void Rack::countMods (Slot& s) noexcept
{
    int n = 0;
    for (const auto& m : s.mod) n += m.load (std::memory_order_relaxed) != 0.0f ? 1 : 0;
    s.mods.store (n);
}

void Rack::setExt (int slot, int i, float v) noexcept
{
    if (slot < 0 || slot >= kSlots || i < 0 || i >= fxdefs::kExt || ! std::isfinite (v)) return;
    slots[static_cast<size_t> (slot)].ext[static_cast<size_t> (i)].store (v, std::memory_order_relaxed);
}

float Rack::getExt (int slot, int i) const noexcept
{
    if (slot < 0 || slot >= kSlots || i < 0 || i >= fxdefs::kExt) return 0.0f;
    return slots[static_cast<size_t> (slot)].ext[static_cast<size_t> (i)].load (std::memory_order_relaxed);
}

void Rack::setMod (int slot, int target, int src, float amount) noexcept
{
    if (slot < 0 || slot >= kSlots || target < 0 || target >= kTargets || src < 0 || src >= fxdefs::kModSources || ! std::isfinite (amount)) return;
    auto& s = slots[static_cast<size_t> (slot)];
    s.mod[static_cast<size_t> (target * fxdefs::kModSources + src)].store (juce::jlimit (-1.0f, 1.0f, amount), std::memory_order_relaxed);
    countMods (s);
}

float Rack::getMod (int slot, int target, int src) const noexcept
{
    if (slot < 0 || slot >= kSlots || target < 0 || target >= kTargets || src < 0 || src >= fxdefs::kModSources) return 0.0f;
    return slots[static_cast<size_t> (slot)].mod[static_cast<size_t> (target * fxdefs::kModSources + src)].load (std::memory_order_relaxed);
}

float Rack::getModNow (int slot, int target) const noexcept
{
    if (slot < 0 || slot >= kSlots || target < 0 || target >= kTargets) return 0.0f;
    return slots[static_cast<size_t> (slot)].modNow[static_cast<size_t> (target)].load (std::memory_order_relaxed);
}

bool Rack::hasMods (int slot) const noexcept
{
    return slot >= 0 && slot < kSlots && slots[static_cast<size_t> (slot)].mods.load (std::memory_order_relaxed) > 0;
}

void Rack::swapSlots (int a, int b)
{
    if (a == b || a < 0 || b < 0 || a >= kSlots || b >= kSlots) return;
    auto& x = slots[static_cast<size_t> (a)];
    auto& y = slots[static_cast<size_t> (b)];
    x.noReset = y.noReset = true;   // the moved settings must survive the type change
    auto swap = [] (juce::RangedAudioParameter* p, juce::RangedAudioParameter* q)
    {
        if (p == nullptr || q == nullptr) return;
        const float vp = p->getValue(), vq = q->getValue();
        p->beginChangeGesture(); p->setValueNotifyingHost (vq); p->endChangeGesture();
        q->beginChangeGesture(); q->setValueNotifyingHost (vp); q->endChangeGesture();
    };
    swap (x.typeParam, y.typeParam);
    swap (x.onParam, y.onParam);
    swap (x.mixParam, y.mixParam);
    swap (x.layerParam, y.layerParam);
    for (size_t i = 0; i < fxdefs::kNP; ++i) swap (x.pParam[i], y.pParam[i]);
    // the extra values and the modulation routes travel with their effect
    auto trade = [] (auto& p, auto& q) { for (size_t i = 0; i < p.size(); ++i) { const float a = p[i].load(); p[i].store (q[i].load()); q[i].store (a); } };
    trade (x.ext, y.ext);
    trade (x.mod, y.mod);
    std::swap (x.extSet, y.extSet);
    countMods (x); countMods (y);
    ++dataVersion;
}

void Rack::buildSlot (Slot& s, int type, bool resetParams)
{
    // wait until the audio thread has taken the previous hand-over and we have deleted its leftovers
    for (size_t l = 0; l < 2; ++l)
        if (s.pending[l].load() != nullptr || s.retired[l].load() != nullptr) return;

    if (resetParams && ! s.noReset && s.builtType >= 0) resetToDefaults (s, type);
    else if (! s.extSet) { setExtDefaults (s, type); ++dataVersion; }   // a state that carried none
    s.noReset = false;
    s.builtType = type;

    std::array<float, fxdefs::kNP> v {};
    for (int i = 0; i < fxdefs::kNP; ++i)
        v[static_cast<size_t> (i)] = fxdefs::value (type, i, s.p[static_cast<size_t> (i)]->load());
    for (size_t l = 0; l < 2; ++l)
    {
        auto u = makeUnit (type);
        u->prepare (fs, maxBlock);
        u->reset();
        u->messageTick (v.data(), ir, true);
        s.current[l] = u.get();
        s.pending[l].store (u.release());
    }
}

void Rack::messageTick (bool resetParams)
{
    for (auto& s : slots)
    {
        for (size_t l = 0; l < 2; ++l)
            delete s.retired[l].exchange (nullptr);

        const int type = juce::jlimit (0, fxdefs::kNumTypes - 1, static_cast<int> (s.type->load() + 0.5f));
        if (type != s.builtType)
            buildSlot (s, type, resetParams);
        else
            s.noReset = false;

        std::array<float, fxdefs::kNP> v {};
        for (int i = 0; i < fxdefs::kNP; ++i)
            v[static_cast<size_t> (i)] = fxdefs::value (s.builtType < 0 ? 0 : s.builtType, i, s.p[static_cast<size_t> (i)]->load());
        for (auto* u : s.current)
            if (u != nullptr) u->messageTick (v.data(), ir, irChanged);
    }
    irChanged = false;
}

void Rack::process (float* uL, float* uR, float* lL, float* lR, int n, double bpm, double ppq, bool playing, const LayerIn* in) noexcept
{
    if (in != nullptr)
        for (size_t L = 0; L < 2; ++L)
        {
            std::copy (in[L].src, in[L].src + kLayerSources, srcNext[L].begin());
            noteOns[L] = in[L].noteOns;
        }
    else srcNext = srcPrev;
    // with modulation in use, the block is cut into 64-sample pieces and the sources move between them
    bool modded = false;
    for (const auto& s : slots) modded = modded || s.mods.load (std::memory_order_relaxed) > 0;
    const int step = modded ? std::min (maxBlock, 64) : maxBlock;
    for (int off = 0; off < n; off += step)   // scratch is sized by prepare(); split oversized blocks
    {
        const int len = std::min (step, n - off);
        const float t = static_cast<float> (off + len) / static_cast<float> (std::max (1, n));
        for (size_t L = 0; L < 2; ++L)
            for (size_t i = 0; i < kLayerSources; ++i) srcNow[L][i] = srcPrev[L][i] + (srcNext[L][i] - srcPrev[L][i]) * t;
        processChunk (uL + off, uR + off, lL + off, lR + off, len, bpm, ppq + off * bpm / (60.0 * fs), playing);
    }
    srcPrev = srcNext;
}

void Rack::processChunk (float* uL, float* uR, float* lL, float* lR, int n, double bpm, double ppq, bool playing) noexcept
{
    const bool parallel = mode != nullptr && mode->load() >= 0.5f;
    float* io[2][2] = { { uL, uR }, { lL, lR } };

    // RANDOM: a new value on every beat (the host's, or the tempo's while it is stopped), glided over ~50 ms
    rndClock = playing ? ppq : rndClock + n * bpm / (60.0 * fs);
    if (std::floor (rndClock) != rndBeat)
    {
        rndBeat = std::floor (rndClock);
        for (auto& r : rndTo) { rndState ^= rndState << 13; rndState ^= rndState >> 17; rndState ^= rndState << 5; r = static_cast<float> (rndState) * 4.6566129e-10f - 1.0f; }
    }
    const float glide = 1.0f - std::exp (-static_cast<float> (n) / static_cast<float> (0.05 * fs));
    for (size_t L = 0; L < 2; ++L) { rnd[L] += (rndTo[L] - rnd[L]) * glide; srcNow[L][fxdefs::kModSources - 1] = rnd[L]; }
    const float envFall = std::exp (-static_cast<float> (n) / static_cast<float> (0.15 * fs));

    std::array<std::array<float, fxdefs::kNP>, kSlots> norms, vals;
    std::array<int, kSlots> types {};
    std::array<float, kSlots> mix0 {}, mix1 {};
    for (int s = 0; s < kSlots; ++s)
    {
        auto& sl = slots[static_cast<size_t> (s)];
        takePending (sl);
        const int type = sl.builtType < 0 ? 0 : juce::jlimit (0, fxdefs::kNumTypes - 1, static_cast<int> (sl.type->load() + 0.5f));
        types[static_cast<size_t> (s)] = type;
        for (int i = 0; i < fxdefs::kNP; ++i)
        {
            const float nv = sl.p[static_cast<size_t> (i)]->load();
            norms[static_cast<size_t> (s)][static_cast<size_t> (i)] = nv;
            vals[static_cast<size_t> (s)][static_cast<size_t> (i)] = fxdefs::value (type, i, nv);
        }
        for (int i = 0; i < fxdefs::kExtLen[type]; ++i)
            extNow[static_cast<size_t> (s)][static_cast<size_t> (i)] = sl.ext[static_cast<size_t> (i)].load (std::memory_order_relaxed);
        mix0[static_cast<size_t> (s)] = sl.mixSm;
        mix1[static_cast<size_t> (s)] = sl.mix->load();
        sl.mixSm = mix1[static_cast<size_t> (s)];
    }

    std::array<float, fxdefs::kNP> modVals;
    for (int L = 0; L < 2; ++L)
    {
        float* l = io[L][0];
        float* r = io[L][1];
        float* dl = dry[static_cast<size_t> (L * 2)].data();
        float* dr = dry[static_cast<size_t> (L * 2 + 1)].data();
        float* tl = tmp[0].data();
        float* tr = tmp[1].data();
        if (parallel) { std::copy (l, l + n, dl); std::copy (r, r + n, dr); }
        auto& src = srcNow[static_cast<size_t> (L)];

        for (int s = 0; s < kSlots; ++s)
        {
            auto& sl = slots[static_cast<size_t> (s)];
            Unit* u = sl.active[static_cast<size_t> (L)];
            const float f = sl.layer->load();
            float target = sl.on->load() >= 0.5f ? (L == 0 ? std::min (1.0f, 2.0f * (1.0f - f)) : std::min (1.0f, 2.0f * f)) : 0.0f;
            const int type = static_cast<int> (sl.type->load() + 0.5f);
            if (u == nullptr) continue;
            if (type == fxdefs::None || u->type != type) target = 0.0f;   // not built yet: fade to dry
            float& amt = sl.amt[static_cast<size_t> (L)];
            if (target <= 0.0f && amt <= 0.0f) continue;   // idle: the unit was reset when it faded out

            const float* inL = parallel ? dl : l;
            const float* inR = parallel ? dr : r;
            std::copy (inL, inL + n, tl);
            std::copy (inR, inR + n, tr);

            // modulation: every routed control moves by amount x source, in its own normalised range
            const float* v = vals[static_cast<size_t> (s)].data();
            float mixOff = 0.0f;
            const bool shown = L == (f > 0.5f ? 1 : 0);
            if (sl.mods.load (std::memory_order_relaxed) > 0)
            {
                float pk = 0.0f;
                for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (inL[i]), std::abs (inR[i])));
                float& e = sl.inEnv[static_cast<size_t> (L)];
                e = std::max (pk, e * envFall);
                src[fxdefs::kModSources - 2] = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (e, -60.0f) + 60.0f) / 60.0f);   // INPUT
                const int t = types[static_cast<size_t> (s)];
                for (int tg = 0; tg < kTargets; ++tg)
                {
                    float off = 0.0f;
                    for (int k = 0; k < fxdefs::kModSources; ++k)
                        off += sl.mod[static_cast<size_t> (tg * fxdefs::kModSources + k)].load (std::memory_order_relaxed) * src[static_cast<size_t> (k)];
                    if (shown) sl.modNow[static_cast<size_t> (tg)].store (off, std::memory_order_relaxed);
                    if (tg == fxdefs::kNP) { mixOff = off; continue; }
                    const auto ti = static_cast<size_t> (tg);
                    modVals[ti] = off == 0.0f || fxdefs::kParams[t][tg].curve == 2 ? vals[static_cast<size_t> (s)][ti]
                                                                                   : fxdefs::value (t, tg, juce::jlimit (0.0f, 1.0f, norms[static_cast<size_t> (s)][ti] + off));
                }
                v = modVals.data();
            }
            Ctx c { v, bpm, fs, ppq, playing, extNow[static_cast<size_t> (s)].data(), noteOns[static_cast<size_t> (L)] };
            u->process (tl, tr, n, c);

            float& fade = sl.fade[static_cast<size_t> (L)];
            const float fadeStep = 1.0f / static_cast<float> (0.010 * fs);
            const float a0 = amt, da = (target - amt) / static_cast<float> (n);
            const float m0 = juce::jlimit (0.0f, 1.0f, mix0[static_cast<size_t> (s)] + mixOff), dm = (juce::jlimit (0.0f, 1.0f, mix1[static_cast<size_t> (s)] + mixOff) - m0) / static_cast<float> (n);
            for (int i = 0; i < n; ++i)
            {
                fade = std::min (1.0f, fade + fadeStep);
                const float a = (a0 + da * static_cast<float> (i + 1)) * fade;
                const float m = (m0 + dm * static_cast<float> (i + 1)) * (kPi * 0.5f);
                const float gd = std::cos (m), gw = std::sin (m);
                const float xl = inL[i], xr = inR[i];
                const float ol = xl * gd + tl[i] * gw, or_ = xr * gd + tr[i] * gw;   // equal-power DRY / WET
                // serial: replace the running signal; parallel: add this slot's difference
                l[i] = (parallel ? l[i] : xl) + a * (ol - xl);
                r[i] = (parallel ? r[i] : xr) + a * (or_ - xr);
            }
            if (shown)   // feed the analyser from the layer the display follows
            {
                auto& sc = scopes[static_cast<size_t> (s)];
                int p = sc.pos.load (std::memory_order_relaxed);
                float pk = 0.0f;
                for (int i = 0; i < n; ++i)
                {
                    const float x = 0.5f * (tl[i] + tr[i]);
                    pk = std::max (pk, std::abs (x));
                    sc.buf[static_cast<size_t> (p)].store (x, std::memory_order_relaxed);
                    p = (p + 1) & (kScope - 1);
                }
                sc.pos.store (p, std::memory_order_release);
                sc.peak.store (std::max (pk, sc.peak.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
            }
            amt = target;
            if (amt <= 0.0f) u->reset();
        }
    }
}

float Rack::getVis (int slot, int i) const noexcept
{
    const auto& s = slots[static_cast<size_t> (juce::jlimit (0, kSlots - 1, slot))];
    const auto* u = s.current[s.layer != nullptr && s.layer->load() > 0.5f ? 1 : 0];
    return u != nullptr ? u->vis[static_cast<size_t> (juce::jlimit (0, Unit::kVis - 1, i))].load (std::memory_order_relaxed) : 0.0f;
}

void Rack::spectrum (int slot, float* out) const
{
    const auto si = static_cast<size_t> (juce::jlimit (0, kSlots - 1, slot));
    const auto& sc = scopes[si];
    const int p = sc.pos.load (std::memory_order_acquire);
    stale[si] = p == seenPos[si] ? stale[si] + 1 : 0;   // nothing written for a while: the slot is idle
    seenPos[si] = p;
    if (stale[si] > 6) { std::fill (out, out + kBands, -90.0f); return; }
    static juce::dsp::FFT fft (kScopeOrder);
    static juce::dsp::WindowingFunction<float> win (kScope, juce::dsp::WindowingFunction<float>::hann, false);
    std::array<float, 2 * kScope> d {};
    for (int i = 0; i < kScope; ++i) d[static_cast<size_t> (i)] = sc.buf[static_cast<size_t> ((p + i) & (kScope - 1))].load (std::memory_order_relaxed);
    win.multiplyWithWindowingTable (d.data(), kScope);
    fft.performFrequencyOnlyForwardTransform (d.data());
    const double binHz = fs / kScope;
    for (int b = 0; b < kBands; ++b)
    {
        const double f0 = 20.0 * std::pow (1000.0, b / double (kBands)), f1 = 20.0 * std::pow (1000.0, (b + 1) / double (kBands));
        const int i0 = juce::jlimit (1, kScope / 2 - 1, static_cast<int> (f0 / binHz)), i1 = juce::jlimit (i0, kScope / 2 - 1, static_cast<int> (f1 / binHz));
        float m = 0.0f;
        for (int i = i0; i <= i1; ++i) m = std::max (m, d[static_cast<size_t> (i)]);
        out[b] = juce::Decibels::gainToDecibels (m * 4.0f / kScope, -90.0f);   // ~0 dB for a full-scale sine
    }
}

float Rack::outputLevel (int slot) const noexcept
{
    const auto si = static_cast<size_t> (juce::jlimit (0, kSlots - 1, slot));
    return stale[si] > 6 ? 0.0f : scopes[si].peak.load (std::memory_order_relaxed);
}

// ── impulse responses ─────────────────────────────────────────────────────────────────────────

void Rack::makeDefaultImpulse()
{
    // synthetic 2.2 s stereo hall: decorrelated noise, exponential decay, darkening as it goes
    const double rate = 48000.0;
    const int len = static_cast<int> (2.2 * rate);
    ir.buffer.setSize (2, len);
    ir.rate = rate;
    ir.name = "DEFAULT HALL";
    Rng rng;
    for (int ch = 0; ch < 2; ++ch)
    {
        rng.s = ch ? 0x1234567u : 0x7654321u;
        OnePole lp;
        auto* d = ir.buffer.getWritePointer (ch);
        for (int i = 0; i < len; ++i)
        {
            const float t = static_cast<float> (i) / static_cast<float> (rate);
            lp.setHz (12000.0f * std::exp (-t * 1.6f) + 400.0f, rate);
            const float onset = std::min (1.0f, t / 0.012f);
            d[i] = lp.lp (rng.bi()) * onset * std::pow (10.0f, -3.0f * t / 2.2f);
        }
    }
    irFile.reset();
    irChanged = true;
    ++irVersion;
}

bool Rack::loadImpulse (const void* data, size_t size, const juce::String& name)
{
    if (data == nullptr || size == 0) { makeDefaultImpulse(); return true; }
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (
        std::make_unique<juce::MemoryInputStream> (data, size, false)));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0) return false;
    const int len = static_cast<int> (std::min<juce::int64> (reader->lengthInSamples, static_cast<juce::int64> (8.0 * reader->sampleRate)));
    const int chans = static_cast<int> (std::min (2u, reader->numChannels));
    juce::AudioBuffer<float> b (chans, len);
    reader->read (&b, 0, len, 0, true, chans > 1);
    ir.buffer = std::move (b);
    ir.rate = reader->sampleRate;
    ir.name = name.isNotEmpty() ? name.toUpperCase() : juce::String ("IMPULSE");
    irFile.replaceAll (data, size);
    irChanged = true;
    ++irVersion;
    return true;
}

juce::ValueTree Rack::toTree() const
{
    juce::ValueTree t ("FXIR");
    if (irFile.getSize() > 0)
    {
        t.setProperty ("name", ir.name, nullptr);
        t.setProperty ("data", irFile.toBase64Encoding(), nullptr);
    }
    // each slot's extra values ("ext", comma separated) and modulation routes ("mods": target:source:amount;...)
    for (int s = 0; s < kSlots; ++s)
    {
        const auto& sl = slots[static_cast<size_t> (s)];
        juce::StringArray ext, mods;
        for (const auto& e : sl.ext) ext.add (juce::String (e.load(), 5));
        for (int tg = 0; tg < kTargets; ++tg)
            for (int k = 0; k < fxdefs::kModSources; ++k)
                if (const float a = sl.mod[static_cast<size_t> (tg * fxdefs::kModSources + k)].load(); a != 0.0f)
                    mods.add (juce::String (tg) + ":" + juce::String (k) + ":" + juce::String (a, 4));
        juce::ValueTree st ("SLOT");
        st.setProperty ("index", s, nullptr);
        st.setProperty ("ext", ext.joinIntoString (","), nullptr);
        st.setProperty ("mods", mods.joinIntoString (";"), nullptr);
        t.appendChild (st, nullptr);
    }
    return t;
}

void Rack::fromTree (const juce::ValueTree& t)
{
    juce::MemoryBlock b;
    if (t.isValid() && b.fromBase64Encoding (t.getProperty ("data").toString()) && b.getSize() > 0)
        loadImpulse (b.getData(), b.getSize(), t.getProperty ("name").toString());
    else
        makeDefaultImpulse();
    for (int s = 0; s < kSlots; ++s)
    {
        auto& sl = slots[static_cast<size_t> (s)];
        for (auto& m : sl.mod) m.store (0.0f);
        for (auto& m : sl.modNow) m.store (0.0f);
        sl.extSet = false;   // a state without them: the next build gives the type's defaults
        const auto st = t.getChildWithProperty ("index", s);
        if (st.isValid() && st.getProperty ("ext").toString().isNotEmpty())
        {
            const auto ext = juce::StringArray::fromTokens (st.getProperty ("ext").toString(), ",", "");
            for (int i = 0; i < fxdefs::kExt; ++i) sl.ext[static_cast<size_t> (i)].store (i < ext.size() ? ext[i].getFloatValue() : 0.0f);
            sl.extSet = true;
        }
        if (st.isValid())
            for (const auto& m : juce::StringArray::fromTokens (st.getProperty ("mods").toString(), ";", ""))
            {
                const auto f = juce::StringArray::fromTokens (m, ":", "");
                if (f.size() == 3) setMod (s, f[0].getIntValue(), f[1].getIntValue(), f[2].getFloatValue());
            }
        countMods (sl);
    }
    ++dataVersion;
}
} // namespace fx
