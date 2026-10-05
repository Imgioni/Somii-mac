#include "Dds1.h"
#include "Tapers.h"

#if defined(_M_X64) || defined(_M_AMD64) || defined(__SSE2__)
 #define SG_DDS1_SIMD 1
 #include <emmintrin.h>
#else
 #define SG_DDS1_SIMD 0
#endif

namespace sg
{

namespace
{
// Sister frequency offsets (fraction of the outer spread). Slightly irregular so the beating
// pattern never lines up; the right channel mirrors them for stereo de-phasing [p.61].
constexpr std::array<float, Dds1::kOscCount> kOffsets { 0.0f, -1.0f, -0.63f, -0.29f, 0.31f, 0.65f, 0.98f };
// SUPER ½ level profile read from the p.61 spectrum figure: descending outward.
constexpr std::array<float, Dds1::kOscCount> kHalfLevels { 1.0f, 0.25f, 0.37f, 0.5f, 0.5f, 0.37f, 0.25f };
} // namespace

void Dds1::prepare (float osRate, float hostRate, uint32_t seed)
{
    osFs = osRate;
    hostFs = hostRate;
    // Harmonics above headroom/inc would alias into the audible band even after decimation:
    // anything between the host Nyquist and osFs − 0.6·hostFs folds into the decimator's stopband.
    headroom = 1.0f - 0.6f * hostFs / osFs;
    rng.seed (seed);
    reset();
}

void Dds1::reset()
{
    phase[0] = 0.0f;                                   // centroid phase shared by L/R voices
    for (int i = 1; i < kOscCount; ++i) phase[static_cast<size_t> (i)] = rng.unipolar();
    delay.clear();
    smpRestart = true;
}

void Dds1::noteOn (bool resetPhases)
{
    if (resetPhases) phase.fill (0.0f);
    smpRestart = true;                                 // a sample always starts from START
}

Dds1::Source Dds1::sourceFor (Dds1Wave w, bool second, const WaveTable* a, const WaveTable* b) noexcept
{
    // The morph target is "the next waveform to the right" [p.32].
    int idx = static_cast<int> (w) + (second ? 1 : 0);
    const bool altB = (w == Dds1Wave::Alt && second);
    if (idx > static_cast<int> (Dds1Wave::Alt)) idx = static_cast<int> (Dds1Wave::Alt);

    Source s;
    switch (static_cast<Dds1Wave> (idx))
    {
        case Dds1Wave::Sine:     s.shape = Shape::Sine; break;
        case Dds1Wave::Saw:      s.shape = Shape::Saw; break;
        case Dds1Wave::Square:   s.shape = Shape::Square; break;
        case Dds1Wave::Triangle: s.shape = Shape::Triangle; break;
        case Dds1Wave::Noise:    s.kind = Source::Noise; break;
        case Dds1Wave::Alt:
        {
            const WaveTable* t = altB ? b : a;
            if (t != nullptr) { s.kind = Source::Table; s.table = t; }
            else              { s.shape = Shape::Saw; }
            break;
        }
    }
    return s;
}

void Dds1::setShape (Dds1Wave wave, const WaveTable* altA, const WaveTable* altB) noexcept
{
    srcA = sourceFor (wave, false, altA, altB);
    srcB = sourceFor (wave, true, altA, altB);
}

void Dds1::setSample (const Sample* s, bool loop, float start, float end, float loopStart, float level,
                      int root, float fineCents, int ch) noexcept
{
    if (s != smp) smpRestart = true;
    smp = s;
    if (s == nullptr) return;
    smpLoop = loop;
    smpCh = ch;
    const double last = static_cast<double> (s->frames - 1);
    smpLo = static_cast<double> (clampf (start, 0.0f, 1.0f)) * last;
    smpHi = std::max (smpLo + 1.0, static_cast<double> (clampf (end, 0.0f, 1.0f)) * last);
    smpLoopLo = std::max (0.0, std::min (static_cast<double> (clampf (loopStart, 0.0f, 1.0f)) * last, smpHi - 4.0));
    smpGain = taper::levelGain (level);
    // De-click: LOOP crossfades the last smpXf frames into LOOP START; ONE SHOT fades out over
    // them when END cuts into the sound. 30 ms (5 ms one shot), never more than a quarter of the region.
    smpXf = std::max (1.0, std::min (static_cast<double> (s->rate) * (loop ? 0.030 : 0.005), 0.25 * (smpHi - (loop ? smpLoopLo : smpLo))));
    // FINE raises the pitch by lowering the root frequency the sample is heard at.
    smpScale = s->rate / noteToHz (static_cast<float> (root) - fineCents * 0.01f);
    smpRoot = root; smpFineCents = fineCents;
}

// SLICE (user, 2026-10-05: chop a dropped loop or song across the keys): the key's slice of
// START..END becomes the play region, heard at the file's own speed on every key; keys below
// ROOT KEY or past the last slice are silent. AUTO cuts at the transients at least as strong as
// SENSITIVITY asks (Sample::sliceThreshold, at least 30 ms apart); 4..64 cut equal slices.
// ui/geminus.js draws the slices with the same rule.
void Dds1::setSlice (bool on, int slices, float sense, int note) noexcept
{
    if (! on || smp == nullptr) return;
    smpLoop = false;
    const int k = note - smpRoot;
    const double lo = smpLo, hi = smpHi;
    double a = 0.0, b = 0.0;
    if (k >= 0 && slices > 0)
    {
        if (k < slices) { a = lo + (hi - lo) * k / slices; b = lo + (hi - lo) * (k + 1) / slices; }
    }
    else if (k >= 0)
    {
        const float thr = Sample::sliceThreshold (sense);
        const double minGap = 0.03 * smp->rate;
        double prev = lo;
        int idx = 0;
        for (size_t j = 0; j < smp->onsetPos.size() && b <= a; ++j)
        {
            const double p = smp->onsetPos[j];
            if (p <= prev + minGap || p >= hi - minGap || smp->onsetStr[j] < thr) continue;
            if (idx == k) { a = prev; b = p; }
            prev = p; ++idx;
        }
        if (b <= a && idx == k) { a = prev; b = hi; }
    }
    if (b <= a) { smpLo = smpHi = 0.0; return; }     // no slice for this key: silent
    smpLo = a; smpHi = b;
    smpXf = std::max (1.0, std::min (static_cast<double> (smp->rate) * 0.004, 0.25 * (b - a)));
    smpScale = smp->rate / noteToHz (static_cast<float> (note) - smpFineCents * 0.01f);
}

void Dds1::setSuper (Tri superMode, float detuneAmount, bool mirrored) noexcept
{
    if (superMode == lastSuper && detuneAmount == lastDetune && mirrored == lastMirrored) return;
    lastSuper = superMode; lastDetune = detuneAmount; lastMirrored = mirrored;

    const float depth = superMode == Tri::On ? 1.0f : (superMode == Tri::Half ? 0.5f : 0.0f);
    activeCount = superMode == Tri::Off ? 1 : kOscCount;

    const float spreadCents = taper::superSpreadCents (clampf (detuneAmount, 0.0f, 1.0f)) * depth;
    float sumSq = 0.0f, sum = 0.0f;
    for (int i = 0; i < activeCount; ++i)
    {
        const auto k = static_cast<size_t> (i);
        const float off = (mirrored ? -kOffsets[k] : kOffsets[k]) * spreadCents;
        ratio[k] = fastExp2 (off * (1.0f / 1200.0f));
        gain[k] = superMode == Tri::Half ? kHalfLevels[k] : 1.0f;
        sumSq += gain[k] * gain[k];
        sum += gain[k];
    }
    const float norm = 1.0f / std::sqrt (sumSq);
    for (int i = 0; i < activeCount; ++i) gain[static_cast<size_t> (i)] *= norm;
    noiseGainSum = sum * norm;
}

void Dds1::setFrequency (float baseInc, float m) noexcept
{
    morph = clampf (m, 0.0f, 1.0f);

    const bool tables = srcA.kind == Source::Table || srcB.kind == Source::Table;
    for (int i = 0; i < activeCount; ++i)
    {
        const auto k = static_cast<size_t> (i);
        inc[k] = clampf (baseInc * ratio[k], 0.0f, 0.45f);
        if (tables)
        {
            const float maxHarm = inc[k] > 0.0f ? headroom / inc[k] : 1.0e6f;
            levelA[k] = srcA.kind == Source::Table ? WaveTable::levelFor (maxHarm) : 0;
            levelB[k] = srcB.kind == Source::Table ? WaveTable::levelFor (maxHarm) : 0;
        }
    }
}

float Dds1::readSource (const Source& s, int osc, int level, float naiveNoise) const noexcept
{
    switch (s.kind)
    {
        case Source::Classic: return naiveShape (s.shape, phase[static_cast<size_t> (osc)], 0.5f);
        case Source::Noise:   return naiveNoise / noiseGainSum;
        case Source::Table:   return s.table->read (phase[static_cast<size_t> (osc)], level);
    }
    return 0.0f;
}

namespace
{
// Phase of the next discontinuity after p for each classic shape (1 = the wrap).
template <Shape S>
inline float nextEdge (float p) noexcept
{
    if constexpr (S == Shape::Square)   return p < 0.5f ? 0.5f : 1.0f;
    if constexpr (S == Shape::Triangle) return p < 0.25f ? 0.25f : (p < 0.75f ? 0.75f : 1.0f);
    return 1.0f;
}
} // namespace

#if SG_DDS1_SIMD
namespace
{
inline __m128 select (__m128 mask, __m128 a, __m128 b) noexcept
{
    return _mm_or_ps (_mm_and_ps (mask, a), _mm_andnot_ps (mask, b));
}

template <Shape S>
inline __m128 nextEdgeV (__m128 p) noexcept
{
    const __m128 one = _mm_set1_ps (1.0f);
    if constexpr (S == Shape::Square)
        return select (_mm_cmplt_ps (p, _mm_set1_ps (0.5f)), _mm_set1_ps (0.5f), one);
    if constexpr (S == Shape::Triangle)
        return select (_mm_cmplt_ps (p, _mm_set1_ps (0.25f)), _mm_set1_ps (0.25f),
                       select (_mm_cmplt_ps (p, _mm_set1_ps (0.75f)), _mm_set1_ps (0.75f), one));
    return one;
}

// naiveShape() for four phases in [0,1).
template <Shape S>
inline __m128 shapeV (__m128 p) noexcept
{
    const __m128 one = _mm_set1_ps (1.0f);
    if constexpr (S == Shape::Saw)
        return _mm_sub_ps (one, _mm_add_ps (p, p));
    if constexpr (S == Shape::Square)
        return select (_mm_cmplt_ps (p, _mm_set1_ps (0.5f)), one, _mm_set1_ps (-1.0f));
    if constexpr (S == Shape::Triangle)
    {
        const __m128 f = _mm_mul_ps (p, _mm_set1_ps (4.0f));
        return select (_mm_cmplt_ps (p, _mm_set1_ps (0.25f)), f,
                       select (_mm_cmplt_ps (p, _mm_set1_ps (0.75f)), _mm_sub_ps (_mm_set1_ps (2.0f), f),
                               _mm_sub_ps (f, _mm_set1_ps (4.0f))));
    }
    // Sine: same fold and polynomial as sin2pi().
    __m128 u = _mm_sub_ps (p, _mm_set1_ps (0.5f));
    u = select (_mm_cmpgt_ps (u, _mm_set1_ps (0.25f)), _mm_sub_ps (_mm_set1_ps (0.5f), u), u);
    u = select (_mm_cmplt_ps (u, _mm_set1_ps (-0.25f)), _mm_sub_ps (_mm_set1_ps (-0.5f), u), u);
    const __m128 x = _mm_mul_ps (u, _mm_set1_ps (kTwoPi));
    const __m128 x2 = _mm_mul_ps (x, x);
    __m128 poly = _mm_add_ps (_mm_set1_ps (-1.98412698e-4f), _mm_mul_ps (x2, _mm_set1_ps (2.75573192e-6f)));
    poly = _mm_add_ps (_mm_set1_ps (8.33333333e-3f), _mm_mul_ps (x2, poly));
    poly = _mm_add_ps (_mm_set1_ps (-1.66666667e-1f), _mm_mul_ps (x2, poly));
    poly = _mm_add_ps (one, _mm_mul_ps (x2, poly));
    return _mm_sub_ps (_mm_setzero_ps(), _mm_mul_ps (x, poly));
}
} // namespace

template <Shape S>
bool Dds1::simdPass (float fm, float& sum) noexcept
{
    const __m128 vfm = _mm_set1_ps (fm), vmax = _mm_set1_ps (0.45f);
    const __m128 p0 = _mm_load_ps (&phase[0]), p1 = _mm_load_ps (&phase[4]);
    const __m128 n0 = _mm_add_ps (p0, _mm_min_ps (_mm_mul_ps (_mm_load_ps (&inc[0]), vfm), vmax));
    const __m128 n1 = _mm_add_ps (p1, _mm_min_ps (_mm_mul_ps (_mm_load_ps (&inc[4]), vfm), vmax));
    if ((_mm_movemask_ps (_mm_cmpge_ps (n0, nextEdgeV<S> (p0)))
         | _mm_movemask_ps (_mm_cmpge_ps (n1, nextEdgeV<S> (p1)))) != 0)
        return false;

    _mm_store_ps (&phase[0], n0);
    _mm_store_ps (&phase[4], n1);
    __m128 s = _mm_add_ps (_mm_mul_ps (_mm_load_ps (&gain[0]), shapeV<S> (n0)),
                           _mm_mul_ps (_mm_load_ps (&gain[4]), shapeV<S> (n1)));
    s = _mm_add_ps (s, _mm_movehl_ps (s, s));
    s = _mm_add_ss (s, _mm_shuffle_ps (s, s, 1));
    sum = _mm_cvtss_f32 (s);
    return true;
}
#endif

template <Shape S>
float Dds1::classicPass (float fm, BlepAcc& acc, float& centroidWrap) noexcept
{
    float sum = 0.0f;
#if SG_DDS1_SIMD
    if (activeCount == kOscCount && simdPass<S> (fm, sum))
        return sum;
#endif
    for (int i = 0; i < activeCount; ++i)
    {
        const auto k = static_cast<size_t> (i);
        float incI = inc[k] * fm;
        if (incI > 0.45f) incI = 0.45f;
        float p = phase[k];
        const float pn = p + incI;
        if (pn < nextEdge<S> (p))
        {
            p = pn;
        }
        else
        {
            const float w = advancePhase (p, incI, 0.0f, 1.0f, S, 0.5f, gain[k], acc);
            if (i == 0) centroidWrap = w;
        }
        phase[k] = p;
        sum += gain[k] * naiveShape (S, p, 0.5f);
    }
    return sum;
}

float Dds1::tick (float fm, float& centroidWrap) noexcept
{
    BlepAcc acc;
    centroidWrap = -1.0f;
    if (smp != nullptr) return delay.push (samplePass (fm, acc, centroidWrap), acc);
    float naiveSum = 0.0f;

    if (morph <= 0.0f && srcA.kind == Source::Classic)
    {
        switch (srcA.shape)
        {
            case Shape::Saw:      naiveSum = classicPass<Shape::Saw> (fm, acc, centroidWrap); break;
            case Shape::Square:   naiveSum = classicPass<Shape::Square> (fm, acc, centroidWrap); break;
            case Shape::Triangle: naiveSum = classicPass<Shape::Triangle> (fm, acc, centroidWrap); break;
            case Shape::Sine:     naiveSum = classicPass<Shape::Sine> (fm, acc, centroidWrap); break;
            case Shape::Pulse:
            case Shape::Noise:    naiveSum = generalPass (fm, acc, centroidWrap); break;
        }
    }
    else
    {
        naiveSum = generalPass (fm, acc, centroidWrap);
    }
    return delay.push (naiveSum, acc);
}

float Dds1::generalPass (float fm, BlepAcc& acc, float& centroidWrap) noexcept
{
    float naiveSum = 0.0f;

    const bool needNoise = srcA.kind == Source::Noise || (morph > 0.0f && srcB.kind == Source::Noise);
    const float nz = needNoise ? rng.bipolar() : 0.0f;
    const float wAm = 1.0f - morph;

    for (int i = 0; i < activeCount; ++i)
    {
        const auto k = static_cast<size_t> (i);
        const float incI = clampf (inc[k] * fm, 0.0f, 0.45f);
        const float wA = wAm * gain[k];
        const float wB = morph * gain[k];
        const float p0 = phase[k];

        float p = p0;
        const Shape shA = srcA.kind == Source::Classic ? srcA.shape : Shape::Sine;
        const float wrap = advancePhase (p, incI, 0.0f, 1.0f, shA,
                                         0.5f, srcA.kind == Source::Classic ? wA : 0.0f, acc);
        if (wB > 0.0f && srcB.kind == Source::Classic)
        {
            float pB = p0;
            advancePhase (pB, incI, 0.0f, 1.0f, srcB.shape, 0.5f, wB, acc);
        }
        phase[k] = p;
        if (i == 0) centroidWrap = wrap;

        naiveSum += wA * readSource (srcA, i, levelA[k], nz);
        if (wB > 0.0f) naiveSum += wB * readSource (srcB, i, levelB[k], nz);
    }
    return naiveSum;
}

// ponytail: linear interpolation, no band-limiting - fine at and below the root, aliases when a
// sample is played octaves above it; switch to a polyphase / mip-mapped reader if that matters.
float Dds1::samplePass (float fm, BlepAcc& acc, float& centroidWrap) noexcept
{
    if (smpRestart) { spos.fill (smpLo); smpRestart = false; }
    const double len = smpHi - smpLoopLo;
    float sum = 0.0f;
    for (int i = 0; i < activeCount; ++i)
    {
        const auto k = static_cast<size_t> (i);
        const float incI = clampf (inc[k] * fm, 0.0f, 0.45f);
        // the phase keeps running: DDS 2 SYNC and the SUB OSC follow the centroid
        const float wrap = advancePhase (phase[k], incI, 0.0f, 1.0f, Shape::Sine, 0.5f, 0.0f, acc);
        if (i == 0) centroidWrap = wrap;

        double& x = spos[k];
        const double into = x - (smpHi - smpXf);       // > 0: inside the fade before END
        float v;
        if (! smpLoop)
        {
            if (x >= smpHi) continue;                  // ONE SHOT: this oscillator has finished
            v = smp->read (x, smpCh);
            if (into > 0.0) v *= static_cast<float> (1.0 - into / smpXf);
        }
        else
        {
            // LOOP START's first smpXf frames fade in under its last ones (equal power), so the
            // wrap lands on LOOP START + smpXf, where the faded-in copy already is: no jump, no click.
            if (x >= smpHi) { x = smpLoopLo + smpXf + std::fmod (x - smpHi, len - smpXf); }
            v = smp->read (x, smpCh);
            if (into > 0.0)
            {
                const float t = static_cast<float> (into / smpXf) * (0.5f * kPi);
                v = v * std::cos (t) + smp->read (smpLoopLo + into, smpCh) * std::sin (t);
            }
        }
        sum += gain[k] * v;
        x += static_cast<double> (incI * smpScale);
    }
    return sum * smpGain;
}

} // namespace sg
