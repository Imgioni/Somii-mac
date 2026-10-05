#include "SuperVoice.h"

namespace sg
{

void SuperVoice::prepare (float hostRate, int oversampling, uint32_t seed, bool analogTolerance)
{
    voices[0].prepare (hostRate, oversampling, seed * 2u + 1u, analogTolerance);
    voices[1].prepare (hostRate, oversampling, seed * 2u + 2u, analogTolerance);
    voices[1].setMirrored (true);
    lfo.prepare (hostRate, seed * 7u + 3u);
    // MODE 2: this card's own starting phase and rate, so voices do not step in lockstep.
    Rng r; r.seed (seed * 2654435761u + 17u);
    lfo1Offset = r.unipolar();
    lfo1RateTol = 1.0f + 0.025f * r.bipolar();
}

void SuperVoice::reset()
{
    voices[0].reset();
    voices[1].reset();
}

void SuperVoice::startNote (int slot, int note, float velocity, float unisonSemis, const LayerControl& c,
                            bool declick, float glide)
{
    // An idle super voice hasn't seen the current LFO settings yet; apply them before note-on so
    // ONCE / RESET use the right mode.
    lfo.setRate (c.lfo1PerVoice ? c.lfo1Hz * lfo1RateTol : c.lfo1Hz);
    lfo.setShape (c.lfo1Wave, c.lfo1Mode, c.lfo1ShNoise);

    if (binaural)
    {
        voices[0].setMirrored (false);
        voices[1].setMirrored (true);
        voices[0].setSampleChannel (0);   // BINAURAL: a stereo CUSTOM sample keeps its image
        voices[1].setSampleChannel (1);
        if (! voices[1].isActive()) voices[1].setHfPhase (voices[0].getHfPhase());   // keep the pair phase-locked
        voices[0].start (note, velocity, unisonSemis, declick, glide);
        voices[1].start (note, velocity, unisonSemis, declick, glide);
    }
    else
    {
        const int s = slot & 1;
        voices[s].setMirrored (false);
        voices[s].setSampleChannel (2);
        voices[s].start (note, velocity, unisonSemis, declick, glide);
    }
    lfo.noteOn();   // ONCE / RESET restart LFO 1 [p.59]
}

void SuperVoice::changeNote (int slot, int note, float unisonSemis) noexcept
{
    if (binaural) { voices[0].changeNote (note, unisonSemis); voices[1].changeNote (note, unisonSemis); }
    else          voices[slot & 1].changeNote (note, unisonSemis);
}

void SuperVoice::releaseSlot (int slot) noexcept
{
    if (binaural) { voices[0].release(); voices[1].release(); }
    else          voices[slot & 1].release();
}

void SuperVoice::releaseNote (int note) noexcept
{
    for (auto& v : voices)
        if (v.isActive() && v.isGateOn() && v.getNote() == note) v.release();
}

void SuperVoice::releaseAll() noexcept
{
    for (auto& v : voices)
        if (v.isActive()) v.release();
}

void SuperVoice::killAll() noexcept
{
    voices[0].kill();
    voices[1].kill();
}

void SuperVoice::setPolyAftertouch (int note, float v) noexcept
{
    for (auto& voice : voices)
        if (voice.isActive() && voice.getNote() == note) voice.setPolyAftertouch (v);
}

void SuperVoice::setupBlock (const LayerControl& c, float blockSeconds) noexcept
{
    // LFO 1 rate and LR PHASE can be matrix destinations; the pair follows its first active voice.
    const Voice& lead = voices[0].isActive() ? voices[0] : voices[1];
    float hz = c.lfo1Hz;
    if (c.routes.has (MDest::Lfo1Rate))
    {
        const float x = clampf (c.norm->lfo1Rate + lead.getMod (MDest::Lfo1Rate), 0.0f, 1.0f);
        hz = (c.lfo1Wave == Lfo1Wave::HF || c.lfo1Wave == Lfo1Wave::HFTrk) ? taper::lfoHighHz (x) : c.lfo1LowHzFor (x);
    }
    lrPhase = c.routes.has (MDest::Lfo1LrPhase)
                  ? clampf (c.norm->lfo1LrPhase + lead.getMod (MDest::Lfo1LrPhase), 0.0f, 1.0f)
                  : c.lfo1LrPhase;

    lfo.setRate (c.lfo1PerVoice ? hz * lfo1RateTol : hz);
    lfo.setShape (c.lfo1Wave, c.lfo1Mode, c.lfo1ShNoise);
    for (auto& v : voices)
        if (v.isActive()) v.setupBlock (c, blockSeconds);
}

void SuperVoice::render (const LayerControl& c, const LayerMods& m, float* osL, float* osR,
                         int numHost, int os, float spread) noexcept
{
    float off[2] { 0.0f, 0.0f };
    if (binaural)      off[1] = lrPhase;
    else if (monoMode) { off[0] = monoOff[0]; off[1] = monoOff[1]; }
    if (c.lfo1PerVoice && c.lfo1Mode == Lfo1Mode::FreeNorm)
    {
        // LR PHASE still sets the pair's own relationship; this shifts the whole card.
        off[0] += lfo1Offset; off[1] += lfo1Offset;
        for (auto& o : off) if (o >= 1.0f) o -= 1.0f;
    }
    voices[0].setHfOffset (off[0]);
    voices[1].setHfOffset (off[1]);

    // Binaural: left voice → L, right voice → R. Non-binaural poly: slot 0 left, slot 1 right,
    // scaled by SPREAD (centre = unity per side). Non-binaural mono: centred.
    float gL[2], gR[2];
    if (binaural)      { gL[0] = 1.0f; gR[0] = 0.0f; gL[1] = 0.0f; gR[1] = 1.0f; }
    else if (monoMode) { gL[0] = gR[0] = gL[1] = gR[1] = 1.0f; }
    else
    {
        gL[0] = 1.0f;                          gR[0] = std::min (1.0f, 1.0f - spread);
        gL[1] = std::min (1.0f, 1.0f - spread); gR[1] = 1.0f;
    }

    // Render in groups of kControlDivider host samples: shared LFO 1 first, then each voice runs
    // its control and one tight render loop for the whole group.
    constexpr int group = Voice::kControlDivider;
    float lv[2][Voice::kMaxGroup], lu[2][Voice::kMaxGroup];
    for (int i = 0; i < numHost; i += group)
    {
        const int n = std::min (group, numHost - i);
        for (int j = 0; j < n; ++j)
        {
            lfo.tick();
            lv[0][j] = lfo.value (off[0]); lu[0][j] = lfo.unipolar (off[0]);
            lv[1][j] = lfo.value (off[1]); lu[1][j] = lfo.unipolar (off[1]);
        }

        float* l = osL + i * os;
        float* r = osR + i * os;
        const int len = n * os;
        const bool on0 = voices[0].isActive(), on1 = voices[1].isActive();
        if (on0 && on1)
            Voice::processPair (voices[0], voices[1], c, m, i, n, lv[0], lu[0], lv[1], lu[1], tmp[0], tmp[1], os);
        else if (on0)
            voices[0].process (c, m, i, n, lv[0], lu[0], tmp[0], os);
        else if (on1)
            voices[1].process (c, m, i, n, lv[1], lu[1], tmp[1], os);

        for (int s = 0; s < 2; ++s)
        {
            if (! (s == 0 ? on0 : on1)) continue;
            const float* t = tmp[s];
            for (int k = 0; k < len; ++k) { l[k] += gL[s] * t[k]; r[k] += gR[s] * t[k]; }
        }
    }
}

} // namespace sg
