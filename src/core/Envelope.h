#pragma once

// ENV 1 / ENV 2 [manual pp.46–53]: attack hold (ENV 1 only), attack, decay hold, decay,
// sustain, release; ENV 1 adds INVERTED and LOOP modes [p.48].
// Segments are exponential like an analog RC envelope. Timing convention (DD-13):
//   attack  = time to reach the peak (overshoot target 1.3, clamped at 1.0)
//   decay / release = time to cover 99 % of the distance to the target

#include "DspMath.h"

namespace sg
{

// ENV 1 KEYTRACK [p.49]: decay and release get shorter up the keyboard in quarter-tone (½) or
// semitone (ON) steps, i.e. time × 2^(−k·(note − C4)/12) with k = 0, 0.5, 1 (DD-26).
inline float envKeytrackScale (float k, int note) noexcept
{
    return fastExp2 (-k * static_cast<float> (note - 60) / 12.0f);
}

class Envelope
{
public:
    enum class Stage { Idle, AttackHold, Attack, DecayHold, Decay, Release };

    void setSampleRate (float fs) noexcept { sampleRate = fs; }

    // Times in seconds; sustain 0..1. Call whenever parameters change (once per control block).
    void setParams (float attackHoldS, float attackS, float decayHoldS, float decayS,
                    float sustainLevel, float releaseS, bool loopMode) noexcept
    {
        // Called once per control block; skip the exp() work when nothing changed.
        if (attackHoldS == lastIn[0] && attackS == lastIn[1] && decayHoldS == lastIn[2] && decayS == lastIn[3]
            && sustainLevel == lastIn[4] && releaseS == lastIn[5] && loopMode == loop && sampleRate == lastIn[6])
            return;
        lastIn[0] = attackHoldS; lastIn[1] = attackS; lastIn[2] = decayHoldS; lastIn[3] = decayS;
        lastIn[4] = sustainLevel; lastIn[5] = releaseS; lastIn[6] = sampleRate;

        attackHoldSamples = attackHoldS * sampleRate;
        decayHoldSamples  = decayHoldS * sampleRate;
        decaySamples      = decayS * sampleRate;
        sustain = clampf (sustainLevel, 0.0f, 1.0f);
        loop = loopMode;

        constexpr float kAttackTarget = 1.3f;
        const float attackTau = std::max (attackS, 1.0e-4f) / std::log (kAttackTarget / (kAttackTarget - 1.0f));
        attackCoeff  = onePoleCoeff (attackTau, sampleRate);
        decayCoeff   = onePoleCoeff (std::max (decayS, 1.0e-4f) / 4.605f, sampleRate);
        releaseCoeff = onePoleCoeff (std::max (releaseS, 1.0e-4f) / 4.605f, sampleRate);
    }

    void gateOn() noexcept
    {
        gate = true;
        stageCounter = 0.0f;
        stage = attackHoldSamples > 0.0f ? Stage::AttackHold : Stage::Attack;
        ++cycleCount;
    }

    void gateOff() noexcept
    {
        gate = false;
        if (stage != Stage::Idle) stage = Stage::Release;
    }

    void kill() noexcept { stage = Stage::Idle; level = 0.0f; gate = false; }

    float tick() noexcept
    {
        switch (stage)
        {
            case Stage::Idle:
                break;

            case Stage::AttackHold:
                if (++stageCounter >= attackHoldSamples) { stage = Stage::Attack; stageCounter = 0.0f; }
                break;

            case Stage::Attack:
                level += (1.3f - level) * attackCoeff;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stageCounter = 0.0f;
                    stage = decayHoldSamples > 0.0f ? Stage::DecayHold : Stage::Decay;
                }
                break;

            case Stage::DecayHold:
                if (++stageCounter >= decayHoldSamples) { stage = Stage::Decay; stageCounter = 0.0f; }
                break;

            case Stage::Decay:
                level += (sustain - level) * decayCoeff;
                if (loop && ++stageCounter >= decaySamples)
                {
                    // LOOP: attack → decay hold → decay repeat while the key is held; the sustain
                    // level is the floor the loop rises from and falls to [p.48].
                    stage = Stage::Attack;
                    stageCounter = 0.0f;
                    ++cycleCount;
                }
                break;

            case Stage::Release:
                level += (0.0f - level) * releaseCoeff;
                if (level < 1.0e-5f) { level = 0.0f; stage = Stage::Idle; }
                break;
        }
        return level;
    }

    float getLevel() const noexcept { return level; }
    Stage getStage() const noexcept { return stage; }
    bool isIdle() const noexcept    { return stage == Stage::Idle; }
    bool isGateOn() const noexcept  { return gate; }
    unsigned getCycleCount() const noexcept { return cycleCount; }   // LOOP LED

private:
    float sampleRate = 48000.0f;
    float lastIn[7] { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    float attackCoeff = 1.0f, decayCoeff = 1.0f, releaseCoeff = 1.0f;
    float attackHoldSamples = 0.0f, decayHoldSamples = 0.0f, decaySamples = 0.0f;
    float sustain = 1.0f;
    bool loop = false, gate = false;

    Stage stage = Stage::Idle;
    float level = 0.0f;
    float stageCounter = 0.0f;
    unsigned cycleCount = 0;
};

} // namespace sg
