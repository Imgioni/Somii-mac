// Renders a patch straight from the DSP core to a WAV, so a sound can be compared with a
// recording of the real instrument. Calibration harness: no JUCE, no plugin, no UI.
//
//   SGRender patch.txt out.wav
//
// The patch file is one "name value" per line. Continuous controls take the panel legend 0-10
// (what you read off a fader or knob), switches take their index, and these extra lines set up
// the performance:
//
//   notes 50 54 57 62      MIDI notes to hold
//   seconds 4              how long to render
//   hold 3                 how long the keys stay down
//   layer upper|lower      which layer the following lines address (default upper)
//
// Anything not named keeps the init-patch value.

#include "core/LayerEngine.h"
#include "core/Performance.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using sg::LayerParams;

// 0-10 off the panel -> the 0..1 the DSP uses
float legend (float v) { return sg::clampf (v / 10.0f, 0.0f, 1.0f); }
int index (float v) { return static_cast<int> (std::lround (v)); }

bool apply (LayerParams& p, const std::string& key, float v)
{
    // continuous controls, named as the panel names them
    static const std::map<std::string, float LayerParams::*> cont {
        { "lfo1.rate", &LayerParams::lfo1Rate }, { "lfo1.delay", &LayerParams::lfo1Delay },
        { "lfo1.lrPhase", &LayerParams::lfo1LrPhase },
        { "ddsMod.lfo1Amt", &LayerParams::pitchLfo1Amt }, { "ddsMod.env1Amt", &LayerParams::pitchEnv1Amt },
        { "ddsMod.pwDetune", &LayerParams::pwDetune }, { "ddsMod.drift", &LayerParams::drift },
        { "ddsMod.pwmWave", &LayerParams::pwmWave }, { "ddsMod.crossMod", &LayerParams::crossMod },
        { "mixer.mix", &LayerParams::mix }, { "mixer.pan", &LayerParams::pan },
        { "vcf.hpf", &LayerParams::hpf }, { "vcf.lpf", &LayerParams::lpf }, { "vcf.res", &LayerParams::res },
        { "vcf.envAmt", &LayerParams::vcfEnvAmt }, { "vcf.lfo1Amt", &LayerParams::vcfLfo1Amt },
        { "vcf.dds2Amt", &LayerParams::vcfDds2Amt }, { "vcf.saturation", &LayerParams::vcfSat },
        { "vcf.velocity", &LayerParams::vcfVelocity },
        { "svf.cutoff", &LayerParams::svfCutoff }, { "svf.res", &LayerParams::svfRes }, { "svf.mode", &LayerParams::svfModeMix },
        { "vca.envLevel", &LayerParams::vcaLevel }, { "vca.lfo1Amt", &LayerParams::vcaLfo1Amt },
        { "vca.dds2Amt", &LayerParams::vcaDds2Amt },
        { "env1.attack", &LayerParams::e1Attack }, { "env1.attackHold", &LayerParams::e1AttackHold },
        { "env1.decay", &LayerParams::e1Decay }, { "env1.decayHold", &LayerParams::e1DecayHold },
        { "env1.sustain", &LayerParams::e1Sustain }, { "env1.release", &LayerParams::e1Release },
        { "env2.attack", &LayerParams::e2Attack }, { "env2.decay", &LayerParams::e2Decay },
        { "env2.decayHold", &LayerParams::e2DecayHold },
        { "env2.sustain", &LayerParams::e2Sustain }, { "env2.release", &LayerParams::e2Release },
        { "lfo2.rate", &LayerParams::lfo2Rate }, { "lfo2.delay", &LayerParams::lfo2Delay },
        { "lfo2.ddsAmt", &LayerParams::lfo2Dds }, { "lfo2.vcfAmt", &LayerParams::lfo2Vcf }, { "lfo2.vcaAmt", &LayerParams::lfo2Vca },
        { "porta.time", &LayerParams::portaTime },
        { "arp.rate", &LayerParams::arpRate },
        { "fx.delaySend", &LayerParams::delaySend }, { "fx.delayTime", &LayerParams::delayTime },
        { "fx.delayFeedback", &LayerParams::delayFeedback }
    };
    if (auto it = cont.find (key); it != cont.end()) { p.*(it->second) = legend (v); return true; }

    if (key == "dds1.wave")   { p.dds1Wave = static_cast<sg::Dds1Wave> (index (v)); return true; }
    if (key == "dds1.range")  { p.dds1Range = index (v); return true; }
    if (key == "dds1.altA")   { p.altA = index (v); return true; }
    if (key == "dds2.wave")   { p.dds2Wave = static_cast<sg::Dds2Wave> (index (v)); return true; }
    if (key == "dds2.range")  { p.dds2Range = index (v); return true; }
    if (key == "dds2.mode")   { p.dds2Mode = static_cast<sg::Dds2Mode> (index (v)); return true; }
    if (key == "dds2.tune")   { p.dds2Tune = v; return true; }                       // semitones
    if (key == "ddsMod.super"){ p.superMode = static_cast<sg::Tri> (index (v)); return true; }
    if (key == "ddsMod.dest") { p.pitchDest = static_cast<sg::OscDest> (index (v)); return true; }
    if (key == "ddsMod.pwmSource") { p.pwmSource = static_cast<sg::PwmSource> (index (v)); return true; }
    if (key == "vcf.drive")   { p.drive = static_cast<sg::Drive> (index (v)); return true; }
    if (key == "vcf.envSource") { p.envSource = static_cast<sg::EnvSource> (index (v)); return true; }
    if (key == "vcf.keytrack") { p.vcfKeytrack = static_cast<sg::Tri> (index (v)); return true; }
    if (key == "vcf.style")   { p.vcfStyle = static_cast<sg::VcfStyle> (index (v)); return true; }
    if (key == "svf.on")      { p.svfOn = v >= 0.5f; return true; }
    if (key == "svf.band")    { p.svfBand = v >= 0.5f; return true; }
    if (key == "vca.envMode") { p.vcaEnv = static_cast<sg::VcaEnv> (index (v)); return true; }
    if (key == "vca.dynamics"){ p.dynamics = static_cast<sg::Tri> (index (v)); return true; }
    if (key == "env1.mode")   { p.e1Mode = static_cast<sg::Env1Mode> (index (v)); return true; }
    if (key == "env1.keytrack") { p.e1Keytrack = static_cast<sg::Tri> (index (v)); return true; }
    if (key == "lfo1.wave")   { p.lfo1Wave = static_cast<sg::Lfo1Wave> (index (v)); return true; }
    if (key == "lfo1.mode")   { p.lfo1Mode = static_cast<sg::Lfo1Mode> (index (v)); return true; }
    if (key == "lfo1.phaseMode") { p.lfo1PhaseMode = static_cast<sg::Lfo1Phase> (index (v)); return true; }
    if (key == "lfo2.wave")   { p.lfo2Wave = static_cast<sg::Lfo2Wave> (index (v)); return true; }
    if (key == "lfo2.trigger"){ p.lfo2Trigger = static_cast<sg::Lfo2Trigger> (index (v)); return true; }
    if (key == "voice.mode")  { p.voiceMode = static_cast<sg::VoiceMode> (index (v)); return true; }
    if (key == "voice.unison"){ p.unison = v >= 0.5f; return true; }
    if (key == "voice.unisonSize") { p.unisonSize = index (v); return true; }
    if (key == "voice.binaural") { p.binaural = v >= 0.5f; return true; }
    if (key == "arp.on")      { p.arpOn = v >= 0.5f; return true; }
    if (key == "arp.mode")    { p.arpMode = static_cast<sg::ArpMode> (index (v)); return true; }
    if (key == "arp.sync")    { p.arpSync = v >= 0.5f; return true; }
    if (key == "arp.range")   { p.arpRange = index (v) + 1; return true; }
    if (key == "arp.swing")   { p.arpSwing = index (v); return true; }
    if (key == "arp.clockDiv"){ p.arpClockDiv = index (v); return true; }
    if (key == "fx.chorus")   { p.chorus = index (v); return true; }
    if (key == "octave")      { p.octave = index (v); return true; }
    return false;
}

void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    const uint32_t n = static_cast<uint32_t> (l.size());
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&f] (uint32_t v) { f.write (reinterpret_cast<const char*> (&v), 4); };
    auto u16 = [&f] (uint16_t v) { f.write (reinterpret_cast<const char*> (&v), 2); };
    f.write ("RIFF", 4); u32 (36 + n * 4); f.write ("WAVE", 4);
    f.write ("fmt ", 4); u32 (16); u16 (1); u16 (2); u32 (static_cast<uint32_t> (sr));
    u32 (static_cast<uint32_t> (sr) * 4); u16 (4); u16 (16);
    f.write ("data", 4); u32 (n * 4);
    for (uint32_t i = 0; i < n; ++i)
        for (float v : { l[i], r[i] })
        {
            const int s = static_cast<int> (std::lround (sg::clampf (v, -1.0f, 1.0f) * 32767.0f));
            u16 (static_cast<uint16_t> (static_cast<int16_t> (s)));
        }
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 3) { std::cout << "usage: SGRender patch.txt out.wav\n"; return 1; }
    std::ifstream in (argv[1]);
    if (! in) { std::cout << "cannot read " << argv[1] << "\n"; return 1; }

    LayerParams upper, lower;
    std::vector<int> notes { 62 };
    double seconds = 4.0, hold = 3.0, tempo = 120.0;
    bool dual = false;
    std::string line, which = "upper";
    int unknown = 0;
    while (std::getline (in, line))
    {
        if (auto h = line.find('#'); h != std::string::npos) line = line.substr (0, h);
        std::istringstream ls (line);
        std::string key; if (! (ls >> key)) continue;
        if (key == "layer") { ls >> which; if (which == "lower") dual = true; continue; }
        if (key == "notes") { notes.clear(); int n; while (ls >> n) notes.push_back (n); continue; }
        if (key == "seconds") { ls >> seconds; continue; }
        if (key == "hold") { ls >> hold; continue; }
        if (key == "tempo") { ls >> tempo; continue; }
        float v; if (! (ls >> v)) continue;
        if (! apply (which == "lower" ? lower : upper, key, v)) { std::cout << "  (ignored: " << key << ")\n"; ++unknown; }
    }

    const double sr = 44100.0;
    const int total = static_cast<int> (seconds * sr), holdN = static_cast<int> (hold * sr);
    sg::PerformanceEngine engine;
    engine.prepare (sr, 512, 2);
    sg::PerformanceParams perf;
    perf.mode = dual ? sg::KeyboardMode::Dual : sg::KeyboardMode::Single;
    perf.singleLayer = 0;
    engine.setParams (perf, upper, lower);

    std::vector<float> uL (512), uR (512), lL (512), lR (512), outL, outR;
    outL.reserve (static_cast<size_t> (total)); outR.reserve (static_cast<size_t> (total));
    for (int n : notes) engine.noteOn (1, n, 0.8f);
    int pos = 0;
    bool released = false;
    while (pos < total)
    {
        const int len = std::min (512, total - pos);
        if (! released && pos >= holdN) { for (int n : notes) engine.noteOff (1, n); released = true; }
        sg::ClockInfo ck; ck.bpm = tempo; ck.hostPlaying = false;
        engine.setClock (ck);
        engine.process (uL.data(), uR.data(), lL.data(), lR.data(), len);
        for (int i = 0; i < len; ++i) { outL.push_back (uL[i] + lL[i]); outR.push_back (uR[i] + lR[i]); }
        pos += len;
    }
    writeWav (argv[2], outL, outR, static_cast<int> (sr));
    float peak = 0.0f; for (float v : outL) peak = std::max (peak, std::abs (v));
    std::cout << "wrote " << argv[2] << "  " << seconds << " s, peak " << peak
              << (unknown ? "  (" + std::to_string (unknown) + " unknown names)" : "") << "\n";
    return 0;
}
