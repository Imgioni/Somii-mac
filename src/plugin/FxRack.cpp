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
}

void Rack::buildSlot (Slot& s, int type, bool resetParams)
{
    // wait until the audio thread has taken the previous hand-over and we have deleted its leftovers
    for (size_t l = 0; l < 2; ++l)
        if (s.pending[l].load() != nullptr || s.retired[l].load() != nullptr) return;

    if (resetParams && ! s.noReset && s.builtType >= 0) resetToDefaults (s, type);
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

void Rack::process (float* uL, float* uR, float* lL, float* lR, int n, double bpm) noexcept
{
    for (int off = 0; off < n; off += maxBlock)   // scratch is sized by prepare(); split oversized blocks
        processChunk (uL + off, uR + off, lL + off, lR + off, std::min (maxBlock, n - off), bpm);
}

void Rack::processChunk (float* uL, float* uR, float* lL, float* lR, int n, double bpm) noexcept
{
    const bool parallel = mode != nullptr && mode->load() >= 0.5f;
    float* io[2][2] = { { uL, uR }, { lL, lR } };

    std::array<std::array<float, fxdefs::kNP>, kSlots> vals;
    std::array<float, kSlots> mix0 {}, mix1 {};
    for (int s = 0; s < kSlots; ++s)
    {
        auto& sl = slots[static_cast<size_t> (s)];
        takePending (sl);
        const int type = sl.builtType < 0 ? 0 : juce::jlimit (0, fxdefs::kNumTypes - 1, static_cast<int> (sl.type->load() + 0.5f));
        for (int i = 0; i < fxdefs::kNP; ++i)
            vals[static_cast<size_t> (s)][static_cast<size_t> (i)] = fxdefs::value (type, i, sl.p[static_cast<size_t> (i)]->load());
        mix0[static_cast<size_t> (s)] = sl.mixSm;
        mix1[static_cast<size_t> (s)] = sl.mix->load();
        sl.mixSm = mix1[static_cast<size_t> (s)];
    }

    for (int L = 0; L < 2; ++L)
    {
        float* l = io[L][0];
        float* r = io[L][1];
        float* dl = dry[static_cast<size_t> (L * 2)].data();
        float* dr = dry[static_cast<size_t> (L * 2 + 1)].data();
        float* tl = tmp[0].data();
        float* tr = tmp[1].data();
        if (parallel) { std::copy (l, l + n, dl); std::copy (r, r + n, dr); }

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
            Ctx c { vals[static_cast<size_t> (s)].data(), bpm, fs };
            u->process (tl, tr, n, c);

            float& fade = sl.fade[static_cast<size_t> (L)];
            const float fadeStep = 1.0f / static_cast<float> (0.010 * fs);
            const float a0 = amt, da = (target - amt) / static_cast<float> (n);
            const float m0 = mix0[static_cast<size_t> (s)], dm = (mix1[static_cast<size_t> (s)] - m0) / static_cast<float> (n);
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
            amt = target;
            if (amt <= 0.0f) u->reset();
        }
    }
}

float Rack::getVis (int slot, int i) const noexcept
{
    const auto& s = slots[static_cast<size_t> (juce::jlimit (0, kSlots - 1, slot))];
    const auto* u = s.current[s.layer != nullptr && s.layer->load() > 0.5f ? 1 : 0];
    return u != nullptr ? u->vis[static_cast<size_t> (juce::jlimit (0, 7, i))].load (std::memory_order_relaxed) : 0.0f;
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
    return t;
}

void Rack::fromTree (const juce::ValueTree& t)
{
    juce::MemoryBlock b;
    if (t.isValid() && b.fromBase64Encoding (t.getProperty ("data").toString()) && b.getSize() > 0)
        loadImpulse (b.getData(), b.getSize(), t.getProperty ("name").toString());
    else
        makeDefaultImpulse();
}
} // namespace fx
