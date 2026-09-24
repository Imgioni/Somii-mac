#include "Parameters.h"

#include "core/Clock.h"
#include "core/Tapers.h"
#include "core/WaveTable.h"
#include "FxDefs.h"

namespace sgp
{

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using API = juce::AudioParameterInt;
using Attr = juce::AudioParameterFloatAttributes;

const juce::StringArray kModLayerGroup { "bender.ddsAmt", "bender.vcfAmt", "dest.osc", "lfo2.wave", "lfo2.rate", "lfo2.delay",
                                         "lfo2.trigger", "lfo2.rateMod", "lfo2.ddsAmt", "lfo2.vcfAmt", "lfo2.vcaAmt" };
const juce::StringArray kPortaLayerGroup { "porta.time", "octave" };

namespace
{
constexpr int kVersion = 1;

juce::String noteName (int n)
{
    static const char* names[] { "C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B" };
    return juce::String::fromUTF8 (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);   // 60 = C4
}

Attr semitoneAttr()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 2) + " st"; });
}

Attr percentAttr()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
                 .withValueFromStringFunction ([] (const juce::String& t) { return juce::jlimit (-1.0f, 1.0f, t.getFloatValue() / 100.0f); });
}

juce::String fmtTime (float seconds)
{
    if (seconds <= 0.0f) return "Off";
    if (seconds < 0.01f)  return juce::String (seconds * 1000.0f, 2) + " ms";
    if (seconds < 1.0f)   return juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms";   // (String (x, 0) = full precision)
    return juce::String (seconds, 2) + " s";
}

float parseSeconds (const juce::String& t)
{
    const float v = t.getFloatValue();
    return t.containsIgnoreCase ("ms") ? v * 0.001f : v;
}

juce::String fmtHz (float hz)
{
    if (hz >= 1000.0f) return juce::String (hz / 1000.0f, 2) + " kHz";
    return (hz < 10.0f ? juce::String (hz, 2) : juce::String (juce::roundToInt (hz))) + " Hz";
}

juce::String fmtDb (float gain)
{
    if (gain <= 1.0e-6f) return "-inf dB";
    return juce::String (20.0f * std::log10 (gain), 1) + " dB";
}

// 0–10 panel legend
Attr fader010()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v * 10.0f, 1); })
                 .withValueFromStringFunction ([] (const juce::String& t) { return juce::jlimit (0.0f, 1.0f, t.getFloatValue() / 10.0f); });
}

Attr envTimeAttr()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return fmtTime (sg::taper::envTime (v)); })
                 .withValueFromStringFunction ([] (const juce::String& t) {
                     const float s = juce::jlimit (0.001f, 10.0f, parseSeconds (t));
                     return std::log10 (s / 0.001f) / 4.0f; });
}

Attr holdTimeAttr()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return fmtTime (sg::taper::holdTime (v)); })
                 .withValueFromStringFunction ([] (const juce::String& t) {
                     const float s = parseSeconds (t);
                     return s <= 0.0f ? 0.0f : std::log10 (juce::jlimit (0.001f, 10.0f, s) / 0.001f) / 4.0f; });
}

Attr levelAttr()
{
    return Attr().withStringFromValueFunction ([] (float v, int) { return fmtDb (sg::taper::levelGain (v)); })
                 .withValueFromStringFunction ([] (const juce::String& t) {
                     return sg::taper::levelFromGain (sg::dbToGain (t.getFloatValue())); });
}

std::unique_ptr<APF> fader (const juce::String& id, const juce::String& name, float def, Attr attr = fader010())
{
    return std::make_unique<APF> (juce::ParameterID { id, kVersion }, name, juce::NormalisableRange<float> (0.0f, 1.0f), def, attr);
}

std::unique_ptr<APC> choice (const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
{
    return std::make_unique<APC> (juce::ParameterID { id, kVersion }, name, items, def);
}

const juce::StringArray kTri { "OFF", "1/2", "ON" };

juce::StringArray altWaveNames()
{
    juce::StringArray names;
    const auto& bank = sg::AltWaveBank::factory();
    for (int i = 0; i < sg::AltWaveBank::kSlots; ++i)
        names.add ("W" + juce::String (i + 1) + " " + juce::String (bank.get (i)->getName()));
    return names;
}

juce::StringArray noteNames()
{
    juce::StringArray names;
    for (int n = 0; n < 128; ++n) names.add (noteName (n));
    return names;
}

void addLayer (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& L, const juce::String& title)
{
    auto group = std::make_unique<juce::AudioProcessorParameterGroup> (L, title, " | ");
    auto id = [&L] (const char* s) { return L + "." + s; };
    auto nm = [&title] (const char* s) { return title + " " + s; };

    // DDS 1 [pp.31–34]
    group->addChild (choice (id ("dds1.wave"),  nm ("DDS1 Waveform"), { "SINE", "SAW", "SQUARE", "TRIANGLE", "NOISE", "ALT" }, 1)); // p.32; init = saw p.22
    group->addChild (choice (id ("dds1.range"), nm ("DDS1 Range"), { "64'", "32'", "16'", "8'", "4'", "2'" }, 3));                  // p.33
    group->addChild (choice (id ("dds1.altA"),  nm ("DDS1 Alt A"), altWaveNames(), 0));                                               // p.33–34
    group->addChild (choice (id ("dds1.altB"),  nm ("DDS1 Alt B"), altWaveNames(), 1));                                               // p.33–34
    // DDS 1 CUSTOM (SPKR addition): a dropped sample plays in place of the waveform
    const auto pct = Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v * 100.0f, 1) + " %"; });
    group->addChild (std::make_unique<APB> (juce::ParameterID { id ("dds1.smpOn"), kVersion }, nm ("DDS1 Custom On"), false));
    group->addChild (choice (id ("dds1.smpLoop"), nm ("DDS1 Custom Mode"), { "ONE SHOT", "LOOP" }, 0));
    group->addChild (fader (id ("dds1.smpStart"), nm ("DDS1 Custom Start"), 0.0f, pct));
    group->addChild (fader (id ("dds1.smpEnd"), nm ("DDS1 Custom End"), 1.0f, pct));
    group->addChild (fader (id ("dds1.smpLoopStart"), nm ("DDS1 Custom Loop Start"), 0.0f, pct));
    group->addChild (fader (id ("dds1.smpLevel"), nm ("DDS1 Custom Level"), 0.8f, levelAttr()));
    group->addChild (choice (id ("dds1.smpRoot"), nm ("DDS1 Custom Root Key"), noteNames(), 60));
    group->addChild (std::make_unique<APF> (juce::ParameterID { id ("dds1.smpFine"), kVersion }, nm ("DDS1 Custom Fine"),
                                            juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f,
                                            Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct"; })));

    // DDS 2 [pp.35–39]
    group->addChild (choice (id ("dds2.wave"),  nm ("DDS2 Waveform"), { "SINE", "SAW", "SQUARE", "TRIANGLE", "NOISE", "PULSE" }, 2)); // p.36
    group->addChild (choice (id ("dds2.range"), nm ("DDS2 Range"), { "LFO", "32'", "16'", "8'", "4'", "2'" }, 3));                   // p.36
    group->addChild (std::make_unique<APF> (juce::ParameterID { id ("dds2.tune"), kVersion }, nm ("DDS2 Tune"),                     // p.36, p.38
                                            juce::NormalisableRange<float> (-7.0f, 7.0f), 0.0f,
                                            Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " st"; })));
    group->addChild (choice (id ("dds2.mode"),  nm ("DDS2 Mode"), { "NORM / SUB OFF", "RING / SUB SQUARE", "SYNC / SUB SINE" }, 0)); // p.36–39, 112

    // Mixer [p.40]
    group->addChild (fader (id ("mixer.mix"), nm ("Mix"), 0.0f));
    group->addChild (std::make_unique<APF> (juce::ParameterID { id ("mixer.pan"), kVersion }, nm ("Pan"),
                                            juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                                            Attr().withStringFromValueFunction ([] (float v, int) {
                                                if (std::abs (v) < 0.005f) return juce::String ("C");
                                                return (v < 0 ? "L" : "R") + juce::String (juce::roundToInt (std::abs (v) * 100.0f)); })));

    // VCF [pp.41–43]
    group->addChild (choice (id ("vcf.drive"), nm ("VCF Drive"), { "OFF", "1", "2" }, 0));                                             // p.42
    group->addChild (fader (id ("vcf.hpf"), nm ("HPF"), 0.0f,
                            Attr().withStringFromValueFunction ([] (float v, int) { return fmtHz (sg::taper::hpfHz (v)); })));      // p.42
    group->addChild (fader (id ("vcf.lpf"), nm ("LPF"), 1.0f));                                                                        // p.42
    group->addChild (fader (id ("vcf.res"), nm ("Resonance"), 0.0f));                                                                  // p.42
    group->addChild (choice (id ("vcf.envSource"), nm ("VCF Env Source"), { "ENV 1", "1+2", "ENV 2" }, 0));                           // p.42
    group->addChild (choice (id ("vcf.keytrack"), nm ("VCF Keytrack"), kTri, 0));                                                       // p.43
    group->addChild (fader (id ("vcf.envAmt"), nm ("VCF Env"), 0.0f));                                                                // p.42
    group->addChild (fader (id ("vcf.lfo1Amt"), nm ("VCF LFO 1"), 0.0f));                                                              // p.42
    group->addChild (fader (id ("vcf.dds2Amt"), nm ("VCF DDS 2"), 0.0f));                                                              // p.42

    // VCA [pp.44–45]
    group->addChild (fader (id ("vca.envLevel"), nm ("VCA Env Level"), 0.8f, levelAttr()));                                           // p.44–45
    group->addChild (fader (id ("vca.lfo1Amt"), nm ("VCA LFO 1"), 0.0f));                                                              // p.45
    group->addChild (fader (id ("vca.dds2Amt"), nm ("VCA DDS 2"), 0.0f));                                                              // p.45
    group->addChild (choice (id ("vca.envMode"), nm ("VCA Envelope"), { "ENV 2", "GATE", "GATE + REL" }, 0));                         // p.45
    // Deviation (user, 2026-09-20): DYNAMICS defaults to ON so note velocity is heard at once (the
    // hardware's init is OFF, which made velocity from a DAW piano roll look broken)
    group->addChild (choice (id ("vca.dynamics"), nm ("VCA Dynamics"), kTri, 2));                                                       // p.45

    // ENV 1 [pp.46–51]
    group->addChild (fader (id ("env1.attackHold"), nm ("ENV1 Attack Hold"), 0.0f, holdTimeAttr()));                                  // p.47
    group->addChild (fader (id ("env1.attack"), nm ("ENV1 Attack"), 0.0f, envTimeAttr()));                                           // p.47
    group->addChild (fader (id ("env1.decayHold"), nm ("ENV1 Decay Hold"), 0.0f, holdTimeAttr()));                                    // p.47
    group->addChild (fader (id ("env1.decay"), nm ("ENV1 Decay"), 0.6747f, envTimeAttr()));                                          // p.48
    group->addChild (fader (id ("env1.sustain"), nm ("ENV1 Sustain"), 0.0f));                                                          // p.48
    group->addChild (fader (id ("env1.release"), nm ("ENV1 Release"), 0.6747f, envTimeAttr()));                                      // p.48
    group->addChild (choice (id ("env1.mode"), nm ("ENV1 Mode"), { "NORMAL", "INVERTED", "LOOP" }, 0));                               // p.48
    group->addChild (choice (id ("env1.keytrack"), nm ("ENV1 Keytrack"), kTri, 0));                                                     // p.49

    // ENV 2 [pp.52–53]
    group->addChild (fader (id ("env2.attack"), nm ("ENV2 Attack"), 0.0f, envTimeAttr()));
    group->addChild (fader (id ("env2.decayHold"), nm ("ENV2 Decay Hold"), 0.0f, holdTimeAttr()));
    group->addChild (fader (id ("env2.decay"), nm ("ENV2 Decay"), 0.6747f, envTimeAttr()));
    group->addChild (fader (id ("env2.sustain"), nm ("ENV2 Sustain"), 1.0f));
    group->addChild (fader (id ("env2.release"), nm ("ENV2 Release"), 0.5f, envTimeAttr()));

    // LFO 1 [pp.54–59]
    group->addChild (choice (id ("lfo1.wave"), nm ("LFO1 Waveform"), { "TRIANGLE", "REV SAW", "S&H", "SQUARE", "HF", "HF TRK" }, 0)); // p.58–59
    group->addChild (fader (id ("lfo1.rate"), nm ("LFO1 Rate"), 0.6667f,                                                              // p.54–56
                            Attr().withStringFromValueFunction ([] (float v, int) {
                                return fmtHz (sg::taper::lfoLowHz (v)) + " | sync " + sg::clockdiv::kLfo1Names[static_cast<size_t> (sg::clockdiv::index (v, 16))]; })));
    group->addChild (fader (id ("lfo1.delay"), nm ("LFO1 Delay"), 0.0f,
                            Attr().withStringFromValueFunction ([] (float v, int) { return fmtTime (sg::taper::lfo1Delay (v)); }))); // p.56
    group->addChild (fader (id ("lfo1.lrPhase"), nm ("LFO1 LR Phase/Spread"), 0.0f,
                            Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }))); // p.56–58
    group->addChild (choice (id ("lfo1.mode"), nm ("LFO1 Mode"), { "FREE / NORM", "ONCE / DDS 1", "RESET / DDS 2" }, 0));            // p.59

    // DDS Modulator [pp.60–63]
    group->addChild (fader (id ("ddsMod.lfo1Amt"), nm ("Pitch LFO 1"), 0.0f));                                                        // p.60
    group->addChild (fader (id ("ddsMod.env1Amt"), nm ("Pitch ENV 1"), 0.0f));                                                        // p.60
    group->addChild (choice (id ("ddsMod.dest"), nm ("Pitch Dest"), { "DDS 1", "1+2", "DDS 2" }, 1));                                // p.60
    group->addChild (choice (id ("ddsMod.super"), nm ("Super (DDS 1)"), kTri, 0));                                                     // p.61
    group->addChild (fader (id ("ddsMod.pwDetune"), nm ("PW / Detune"), 0.0f));                                                       // p.62
    group->addChild (fader (id ("ddsMod.drift"), nm ("Drift"), 0.0f));                                                                // p.62
    group->addChild (fader (id ("ddsMod.pwmWave"), nm ("PWM / Wave"), 0.0f));                                                         // p.62
    group->addChild (choice (id ("ddsMod.pwmSource"), nm ("PWM / Wave Source"), { "MANUAL", "LFO 1", "ENV 1" }, 0));                  // p.62
    group->addChild (fader (id ("ddsMod.crossMod"), nm ("Cross Mod"), 0.0f));                                                         // p.63

    // Performance-section values stored per patch [pp.68–76]
    group->addChild (fader (id ("bender.ddsAmt"), nm ("Bender DDS"), 2.0f / 12.0f,                                                   // p.69: max one octave
                            Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (sg::taper::benderSemis (v), 1) + " st"; })));
    group->addChild (fader (id ("bender.vcfAmt"), nm ("Bender VCF"), 0.0f));                                                          // p.69
    group->addChild (choice (id ("dest.osc"), nm ("Dest Osc"), { "DDS 1", "1+2", "DDS 2" }, 1));                                      // p.72
    group->addChild (choice (id ("lfo2.wave"), nm ("LFO2 Waveform"), { "SINE", "REV SAW", "S&H", "SQUARE", "SAW", "NOISE" }, 0));      // p.70
    group->addChild (fader (id ("lfo2.rate"), nm ("LFO2 Rate"), 0.6667f,                                                              // p.70–71
                            Attr().withStringFromValueFunction ([] (float v, int) { return fmtHz (sg::taper::lfoLowHz (v)); })));
    group->addChild (fader (id ("lfo2.delay"), nm ("LFO2 Delay"), 0.0f,                                                               // p.71
                            Attr().withStringFromValueFunction ([] (float v, int) { return fmtTime (sg::taper::lfo2Delay (v)); })));
    group->addChild (choice (id ("lfo2.trigger"), nm ("LFO2 Trigger"), { "TRIG", "AT + TRIG", "ON (AT > BEND)" }, 0));               // p.71
    group->addChild (fader (id ("lfo2.rateMod"), nm ("LFO2 Rate Mod"), 0.0f));                                                        // p.71
    group->addChild (fader (id ("lfo2.ddsAmt"), nm ("LFO2 DDS"), 0.2f));                                                              // p.71
    group->addChild (fader (id ("lfo2.vcfAmt"), nm ("LFO2 VCF"), 0.0f));                                                              // p.71
    group->addChild (fader (id ("lfo2.vcaAmt"), nm ("LFO2 VCA"), 0.0f));                                                              // p.72
    group->addChild (fader (id ("porta.time"), nm ("Portamento"), 0.0f,                                                               // p.74
                            Attr().withStringFromValueFunction ([] (float v, int) {
                                const float s = sg::taper::portaSecondsPerOctave (v);
                                return s <= 0.0f ? juce::String ("Off") : fmtTime (s) + "/oct"; })));
    group->addChild (choice (id ("octave"), nm ("Octave"), { "-2", "-1", "0", "+1", "+2" }, 2));                                     // p.75

    // Voice assign [pp.90–91]
    group->addChild (choice (id ("voice.mode"), nm ("Voice Mode"), { "SOLO", "LEGATO", "POLY 1", "POLY 2" }, 2));
    group->addChild (std::make_unique<APB> (juce::ParameterID { id ("voice.unison"), kVersion }, nm ("Unison"), false));
    group->addChild (choice (id ("voice.unisonSize"), nm ("Unison Size"), { "1 HALF", "2 ALL", "3 OCTAVE", "4 FIFTH + OCT" }, 1));
    group->addChild (std::make_unique<APB> (juce::ParameterID { id ("voice.binaural"), kVersion }, nm ("Binaural"), true));

    // HOLD [p.81] — stored with the performance
    group->addChild (std::make_unique<APB> (juce::ParameterID { id ("hold"), kVersion }, nm ("Hold"), false));
    // MANUAL [p.26, CC 59 p.119]: detaches the layer from its memory slot (DD-10); no effect on the sound
    group->addChild (std::make_unique<APB> (juce::ParameterID { id ("manual"), kVersion }, nm ("Manual"), false,
                                            juce::AudioParameterBoolAttributes().withAutomatable (false)));

    // Effects [pp.64–67]
    group->addChild (choice (id ("fx.chorus"), nm ("Chorus"), { "OFF", "I", "II", "I+II" }, 0));                                     // p.64
    group->addChild (fader (id ("fx.delayTime"), nm ("Delay Time"), 0.8481f,                                                          // p.65–66
                            Attr().withStringFromValueFunction ([] (float v, int) {
                                return fmtTime (sg::taper::delayTime (v)) + " | sync " + sg::clockdiv::kDelayNames[static_cast<size_t> (sg::clockdiv::index (v, 16))]; })));
    group->addChild (fader (id ("fx.delayFeedback"), nm ("Delay Feedback"), 0.3f));                                                  // p.66
    group->addChild (fader (id ("fx.delaySend"), nm ("Delay Send"), 0.0f));                                                          // p.67
    group->addChild (std::make_unique<APB> (juce::ParameterID { id ("fx.freeze"), kVersion }, nm ("Delay Freeze"), false));          // p.67

    // Arpeggiator / sequencer [pp.92–98]
    juce::StringArray divs;
    for (auto* d : sg::clockdiv::kClockDivNames) divs.add (d);
    juce::StringArray slots;
    for (int i = 1; i <= 16; ++i) slots.add (juce::String (i));
    group->addChild (std::make_unique<APB> (juce::ParameterID { id ("arp.on"), kVersion }, nm ("Arp/Seq On"), false));              // p.94, 96
    group->addChild (choice (id ("arp.clockDiv"), nm ("Clock Div"), divs, 4));                                                        // p.92
    group->addChild (std::make_unique<APB> (juce::ParameterID { id ("arp.sync"), kVersion }, nm ("Sync (LFO 1 / Delay)"), false));   // p.92
    // Deviation (user request): with SYNC off the arp runs free at this step length instead of CLK DIV.
    group->addChild (fader (id ("arp.rate"), nm ("Arp Rate (free)"), 0.5f,
                            Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (sg::taper::arpStepMs (v))) + " ms"; })));
    group->addChild (choice (id ("arp.range"), nm ("Arp Range"), { "1 OCT", "2 OCT", "3 OCT", "4 OCT" }, 0));                         // p.94
    group->addChild (choice (id ("arp.swing"), nm ("Swing"), { "OFF", "1", "2", "3", "4" }, 0));                                     // p.93
    group->addChild (choice (id ("arp.mode"), nm ("Arp Mode"), { "UP", "DOWN", "U&D", "RANDOM", "SEQ" }, 0));                         // p.94
    group->addChild (choice (id ("seq.slot"), nm ("Sequence"), slots, 0));                                                            // p.98

    layout.add (std::move (group));
}

// Modulation matrix [pp.84–89]: 8 × 8 matrix destinations + 8 × 24 direct destinations.
void addMatrix (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& L, const juce::String& title)
{
    auto group = std::make_unique<juce::AudioProcessorParameterGroup> (L + ".mtx", title + " Matrix", " | ");
    for (int s = 0; s < sg::kMatrixSources; ++s)
    {
        const juce::String src = sg::kMatrixSourceIds[static_cast<size_t> (s)];
        const juce::String srcName = sg::kMatrixSourceNames[static_cast<size_t> (s)];
        for (int d = 0; d < sg::kMatrixDests; ++d)
        {
            if (sg::matrixExcluded (s, d)) continue;
            const juce::String pid = d < sg::kFixedDests
                ? L + ".mtx." + src + "." + sg::kMatrixFixedIds[static_cast<size_t> (d)]
                : L + ".mtxd." + src + "." + juce::String (d - sg::kFixedDests + 1);
            const juce::String name = title + " MTX " + srcName + " > " + sg::kMatrixDestNames[static_cast<size_t> (d)];
            group->addChild (std::make_unique<APF> (juce::ParameterID { pid, kVersion }, name,
                                                    juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f, percentAttr()));
        }
    }
    layout.add (std::move (group));
}
// FX rack (docs/fx/FX_PROMPTS.md). The meaning of p1…p28 depends on the slot's type (ui/fxdefs.mjs).
void addFxRack (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("fx", "FX Rack", " | ");
    group->addChild (choice ("fx.mode", "FX Routing", { "SERIAL", "PARALLEL" }, 0));
    juce::StringArray types;
    for (auto* t : fxdefs::kTypeNames) types.add (t);
    for (int s = 1; s <= 3; ++s)
    {
        const juce::String p = "fx" + juce::String (s) + ".", n = "FX " + juce::String (s) + " ";
        group->addChild (choice (p + "type", n + "Type", types, 0));
        group->addChild (std::make_unique<APB> (juce::ParameterID { p + "on", kVersion }, n + "On", true));
        group->addChild (fader (p + "mix", n + "Dry/Wet", 1.0f,
                                Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));
        // layer fader: 0 = upper only, 0.5 = both layers fully, 1 = lower only
        group->addChild (fader (p + "layer", n + "Layer Mix", 0.5f,
                                Attr().withStringFromValueFunction ([] (float v, int) {
                                    return "U " + juce::String (juce::roundToInt (std::min (1.0f, 2.0f * (1.0f - v)) * 100.0f))
                                         + " / L " + juce::String (juce::roundToInt (std::min (1.0f, 2.0f * v) * 100.0f)); })));
        for (int i = 1; i <= fxdefs::kNP; ++i)
            group->addChild (fader (p + "p" + juce::String (i), n + "Param " + juce::String (i), 0.0f,
                                    Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 3); })));
    }
    layout.add (std::move (group));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ── Global (never stored in a performance or patch) ──
    layout.add (fader ("global.masterVolume", "Master Volume", 0.8f, levelAttr()));                                                    // p.79
    layout.add (std::make_unique<API> (juce::ParameterID { "global.transpose", kVersion }, "Global Transpose", -12, 12, 0));           // p.75
    layout.add (std::make_unique<APF> (juce::ParameterID { "global.fineTune", kVersion }, "Global Fine Tune",                          // p.76
                                       juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f,
                                       Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ct"; })));
    layout.add (std::make_unique<API> (juce::ParameterID { "global.midiChannel", kVersion }, "MIDI Channel", 1, 16, 1,               // p.99
                                       juce::AudioParameterIntAttributes().withAutomatable (false)));
    layout.add (std::make_unique<APB> (juce::ParameterID { "global.ccRx", kVersion }, "MIDI CC Receive", true,                        // p.99
                                       juce::AudioParameterBoolAttributes().withAutomatable (false)));
    layout.add (std::make_unique<APB> (juce::ParameterID { "global.clockRx", kVersion }, "Sync to Host Clock", true,                  // p.92, DD-3
                                       juce::AudioParameterBoolAttributes().withAutomatable (false)));

    // ── Performance [pp.79–83] ──
    auto perf = std::make_unique<juce::AudioProcessorParameterGroup> ("perf", "Performance", " | ");
    perf->addChild (choice ("perf.keyboardMode", "Keyboard Mode", { "SINGLE", "DUAL", "SPLIT" }, 0));                                 // p.82
    perf->addChild (choice ("perf.singleLayer", "Single Layer", { "UPPER", "LOWER" }, 0));                                            // p.82
    perf->addChild (std::make_unique<API> (juce::ParameterID { "perf.splitPoint", kVersion }, "Split Point", 0, 127, 60,               // p.82
                                           juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v); })));
    perf->addChild (std::make_unique<APF> (juce::ParameterID { "perf.lowerDetune", kVersion }, "Lower Detune",                        // p.80
                                           juce::NormalisableRange<float> (-7.0f, 7.0f), 0.0f, semitoneAttr()));
    perf->addChild (std::make_unique<APF> (juce::ParameterID { "perf.perfDetune", kVersion }, "Perf Detune",                          // p.80
                                           juce::NormalisableRange<float> (-7.0f, 7.0f), 0.0f, semitoneAttr()));
    perf->addChild (std::make_unique<APF> (juce::ParameterID { "perf.tempo", kVersion }, "Tempo",                                     // p.79
                                           juce::NormalisableRange<float> (30.0f, 300.0f), 120.0f,
                                           Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " BPM"; })));
    // the ribbon as a host parameter, so a DAW can automate it and link a MIDI controller to it:
    // 0…1 with 0.5 = no bend (PerformanceEngine::ribbonAbsolute)
    perf->addChild (std::make_unique<APF> (juce::ParameterID { "perf.ribbon", kVersion }, "Ribbon",
                                           juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                           Attr().withStringFromValueFunction ([] (float v, int) {
                                               const int st = juce::roundToInt ((v - 0.5f) * 24.0f);
                                               return st == 0 ? juce::String ("0 st") : (st > 0 ? "+" : "") + juce::String (st) + " st"; })));
    perf->addChild (choice ("perf.modLayer", "Mod Layer (DEST)", { "BOTH", "LOWER", "UPPER" }, 0));                                   // p.73
    perf->addChild (choice ("perf.portaLayer", "Portamento Layer", { "BOTH", "LOWER", "UPPER" }, 0));                                  // p.74
    layout.add (std::move (perf));

    addLayer (layout, "upper", "Upper");
    addLayer (layout, "lower", "Lower");
    addMatrix (layout, "upper", "Upper");
    addMatrix (layout, "lower", "Lower");
    addFxRack (layout);
    return layout;
}

namespace
{
std::atomic<float>* raw (juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    auto* p = s.getRawParameterValue (id);
    jassert (p != nullptr);
    return p;
}
} // namespace

void LayerParamRefs::bind (juce::AudioProcessorValueTreeState& s, const juce::String& L)
{
    auto g = [&] (const char* suffix) { return raw (s, L + "." + suffix); };
    dds1Wave = g ("dds1.wave"); dds1Range = g ("dds1.range"); altA = g ("dds1.altA"); altB = g ("dds1.altB");
    smpOn = g ("dds1.smpOn"); smpLoop = g ("dds1.smpLoop"); smpStart = g ("dds1.smpStart"); smpEnd = g ("dds1.smpEnd");
    smpLoopStart = g ("dds1.smpLoopStart"); smpLevel = g ("dds1.smpLevel"); smpRoot = g ("dds1.smpRoot"); smpFine = g ("dds1.smpFine");
    dds2Wave = g ("dds2.wave"); dds2Range = g ("dds2.range"); dds2Tune = g ("dds2.tune"); dds2Mode = g ("dds2.mode");
    mix = g ("mixer.mix"); pan = g ("mixer.pan");
    drive = g ("vcf.drive"); hpf = g ("vcf.hpf"); lpf = g ("vcf.lpf"); res = g ("vcf.res");
    envSource = g ("vcf.envSource"); keytrack = g ("vcf.keytrack");
    vcfEnv = g ("vcf.envAmt"); vcfLfo = g ("vcf.lfo1Amt"); vcfDds2 = g ("vcf.dds2Amt");
    vcaLevel = g ("vca.envLevel"); vcaLfo = g ("vca.lfo1Amt"); vcaDds2 = g ("vca.dds2Amt");
    vcaEnv = g ("vca.envMode"); dynamics = g ("vca.dynamics");
    e1AH = g ("env1.attackHold"); e1A = g ("env1.attack"); e1DH = g ("env1.decayHold"); e1D = g ("env1.decay");
    e1S = g ("env1.sustain"); e1R = g ("env1.release"); e1Mode = g ("env1.mode"); e1Kt = g ("env1.keytrack");
    e2A = g ("env2.attack"); e2DH = g ("env2.decayHold"); e2D = g ("env2.decay"); e2S = g ("env2.sustain"); e2R = g ("env2.release");
    lfoWave = g ("lfo1.wave"); lfoRate = g ("lfo1.rate"); lfoDelay = g ("lfo1.delay"); lfoLr = g ("lfo1.lrPhase"); lfoMode = g ("lfo1.mode");
    pLfo = g ("ddsMod.lfo1Amt"); pEnv = g ("ddsMod.env1Amt"); pDest = g ("ddsMod.dest"); superMode = g ("ddsMod.super");
    pw = g ("ddsMod.pwDetune"); drift = g ("ddsMod.drift"); pwm = g ("ddsMod.pwmWave"); pwmSrc = g ("ddsMod.pwmSource");
    xmod = g ("ddsMod.crossMod");
    benderDds = g ("bender.ddsAmt"); benderVcf = g ("bender.vcfAmt"); destOsc = g ("dest.osc");
    lfo2Wave = g ("lfo2.wave"); lfo2Rate = g ("lfo2.rate"); lfo2Delay = g ("lfo2.delay"); lfo2Trig = g ("lfo2.trigger");
    lfo2RateMod = g ("lfo2.rateMod"); lfo2Dds = g ("lfo2.ddsAmt"); lfo2Vcf = g ("lfo2.vcfAmt"); lfo2Vca = g ("lfo2.vcaAmt");
    porta = g ("porta.time"); octave = g ("octave");
    voiceMode = g ("voice.mode"); unison = g ("voice.unison"); unisonSize = g ("voice.unisonSize"); binaural = g ("voice.binaural");
    holdParam = g ("hold");
    chorus = g ("fx.chorus"); delayTime = g ("fx.delayTime"); delayFeedback = g ("fx.delayFeedback");
    delaySend = g ("fx.delaySend"); freeze = g ("fx.freeze");
    arpOn = g ("arp.on"); arpDiv = g ("arp.clockDiv"); arpSync = g ("arp.sync"); arpRate = g ("arp.rate"); arpRange = g ("arp.range");
    arpSwing = g ("arp.swing"); arpMode = g ("arp.mode"); seqSlot = g ("seq.slot");

    for (int src = 0; src < sg::kMatrixSources; ++src)
        for (int d = 0; d < sg::kMatrixDests; ++d)
        {
            auto& slot = matrix[static_cast<size_t> (src)][static_cast<size_t> (d)];
            if (sg::matrixExcluded (src, d)) { slot = nullptr; continue; }
            const juce::String sid = sg::kMatrixSourceIds[static_cast<size_t> (src)];
            slot = d < sg::kFixedDests ? raw (s, L + ".mtx." + sid + "." + sg::kMatrixFixedIds[static_cast<size_t> (d)])
                                       : raw (s, L + ".mtxd." + sid + "." + juce::String (d - sg::kFixedDests + 1));
        }
}

void PerformanceParamRefs::bind (juce::AudioProcessorValueTreeState& s)
{
    master = raw (s, "global.masterVolume"); transpose = raw (s, "global.transpose"); fineTune = raw (s, "global.fineTune");
    midiChannel = raw (s, "global.midiChannel");
    mode = raw (s, "perf.keyboardMode"); single = raw (s, "perf.singleLayer"); split = raw (s, "perf.splitPoint");
    lowerDetune = raw (s, "perf.lowerDetune"); perfDetune = raw (s, "perf.perfDetune");
    modLayerP = raw (s, "perf.modLayer"); portaLayerP = raw (s, "perf.portaLayer");
    tempoP = raw (s, "perf.tempo"); ccRx = raw (s, "global.ccRx"); clockRx = raw (s, "global.clockRx");
}

void PerformanceParamRefs::read (sg::PerformanceParams& p) const noexcept
{
    auto i = [] (const std::atomic<float>* a) { return static_cast<int> (std::lround (a->load (std::memory_order_relaxed))); };
    p.mode = static_cast<sg::KeyboardMode> (i (mode));
    p.singleLayer = i (single);
    p.splitPoint = i (split);
    p.lowerDetune = lowerDetune->load();
    p.perfDetune = perfDetune->load();
    p.transpose = static_cast<float> (i (transpose));
    p.fineTuneCents = fineTune->load();
    p.baseChannel = i (midiChannel);
}

void LayerParamRefs::read (sg::LayerParams& p) const noexcept
{
    auto i = [] (const std::atomic<float>* a) { return static_cast<int> (a->load (std::memory_order_relaxed) + 0.5f); };
    auto f = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };

    p.dds1Wave = static_cast<sg::Dds1Wave> (i (dds1Wave));
    p.dds1Range = i (dds1Range);
    p.altA = i (altA);
    p.altB = i (altB);
    p.smpOn = f (smpOn) >= 0.5f;
    p.smpLoop = i (smpLoop) == 1;
    p.smpStart = f (smpStart);
    p.smpEnd = f (smpEnd);
    p.smpLoopStart = f (smpLoopStart);
    p.smpLevel = f (smpLevel);
    p.smpRoot = i (smpRoot);
    p.smpFine = f (smpFine);
    p.dds2Wave = static_cast<sg::Dds2Wave> (i (dds2Wave));
    p.dds2Range = i (dds2Range);
    p.dds2Tune = f (dds2Tune);
    p.dds2Mode = static_cast<sg::Dds2Mode> (i (dds2Mode));
    p.mix = f (mix);
    p.pan = f (pan);
    p.drive = static_cast<sg::Drive> (i (drive));
    p.hpf = f (hpf); p.lpf = f (lpf); p.res = f (res);
    p.envSource = static_cast<sg::EnvSource> (i (envSource));
    p.vcfKeytrack = static_cast<sg::Tri> (i (keytrack));
    p.vcfEnvAmt = f (vcfEnv); p.vcfLfo1Amt = f (vcfLfo); p.vcfDds2Amt = f (vcfDds2);
    p.vcaLevel = f (vcaLevel); p.vcaLfo1Amt = f (vcaLfo); p.vcaDds2Amt = f (vcaDds2);
    p.vcaEnv = static_cast<sg::VcaEnv> (i (vcaEnv));
    p.dynamics = static_cast<sg::Tri> (i (dynamics));
    p.e1AttackHold = f (e1AH); p.e1Attack = f (e1A); p.e1DecayHold = f (e1DH); p.e1Decay = f (e1D);
    p.e1Sustain = f (e1S); p.e1Release = f (e1R);
    p.e1Mode = static_cast<sg::Env1Mode> (i (e1Mode));
    p.e1Keytrack = static_cast<sg::Tri> (i (e1Kt));
    p.e2Attack = f (e2A); p.e2DecayHold = f (e2DH); p.e2Decay = f (e2D); p.e2Sustain = f (e2S); p.e2Release = f (e2R);
    p.lfo1Wave = static_cast<sg::Lfo1Wave> (i (lfoWave));
    p.lfo1Rate = f (lfoRate); p.lfo1Delay = f (lfoDelay); p.lfo1LrPhase = f (lfoLr);
    p.lfo1Mode = static_cast<sg::Lfo1Mode> (i (lfoMode));
    p.pitchLfo1Amt = f (pLfo); p.pitchEnv1Amt = f (pEnv);
    p.pitchDest = static_cast<sg::OscDest> (i (pDest));
    p.superMode = static_cast<sg::Tri> (i (superMode));
    p.pwDetune = f (pw); p.drift = f (drift); p.pwmWave = f (pwm);
    p.pwmSource = static_cast<sg::PwmSource> (i (pwmSrc));
    p.crossMod = f (xmod);

    p.benderDds = f (benderDds); p.benderVcf = f (benderVcf);
    p.destOsc = static_cast<sg::OscDest> (i (destOsc));
    p.lfo2Wave = static_cast<sg::Lfo2Wave> (i (lfo2Wave));
    p.lfo2Rate = f (lfo2Rate); p.lfo2Delay = f (lfo2Delay);
    p.lfo2Trigger = static_cast<sg::Lfo2Trigger> (i (lfo2Trig));
    p.lfo2RateMod = f (lfo2RateMod); p.lfo2Dds = f (lfo2Dds); p.lfo2Vcf = f (lfo2Vcf); p.lfo2Vca = f (lfo2Vca);
    p.portaTime = f (porta);
    p.octave = i (octave) - 2;
    p.voiceMode = static_cast<sg::VoiceMode> (i (voiceMode));
    p.unison = f (unison) >= 0.5f;
    p.unisonSize = i (unisonSize) + 1;
    p.binaural = f (binaural) >= 0.5f;

    p.chorus = i (chorus);
    p.delayTime = f (delayTime); p.delayFeedback = f (delayFeedback); p.delaySend = f (delaySend);
    p.freeze = f (freeze) >= 0.5f;
    p.arpOn = f (arpOn) >= 0.5f;
    p.arpClockDiv = i (arpDiv);
    p.arpSync = f (arpSync) >= 0.5f;
    p.arpRate = f (arpRate);
    p.arpRange = i (arpRange) + 1;
    p.arpSwing = i (arpSwing);
    p.arpMode = static_cast<sg::ArpMode> (i (arpMode));
    p.seqSlot = i (seqSlot);

    for (int src = 0; src < sg::kMatrixSources; ++src)
        for (int d = 0; d < sg::kMatrixDests; ++d)
        {
            const auto* a = matrix[static_cast<size_t> (src)][static_cast<size_t> (d)];
            p.matrix[src][d] = a != nullptr ? a->load (std::memory_order_relaxed) : 0.0f;
        }
}

} // namespace sgp
