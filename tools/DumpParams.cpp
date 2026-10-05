// Prints every APVTS parameter as id | kind | default | choices, so the web UI's binding
// table and the HTML ids can be generated from the plugin itself rather than transcribed.
//
//   SGDumpParams            all parameters, tab separated
//   SGDumpParams upper.vcf  only ids starting with that prefix

#include "plugin/PluginProcessor.h"

#include <iostream>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::String filter = argc > 1 ? juce::String (argv[1]) : juce::String();

    SuperGeminiProcessor proc;
    auto& apvts = proc.apvts;

    // SGDumpParams --patch values.txt out.gpatch "NAME" "TYPE": a patch file from "id normalised-value"
    // lines on top of the init sound, saved exactly as the plugin saves one (used for sound-design
    // patches matched offline with SGRender)
    if (filter == "--patch" && argc > 4)
    {
        juce::StringArray lines;
        juce::File { juce::String (argv[2]) }.readLines (lines);
        for (auto line : lines)
        {
            line = line.upToFirstOccurrenceOf ("#", false, false).trim();
            if (line.isEmpty()) continue;
            auto* p = apvts.getParameter (line.upToFirstOccurrenceOf (" ", false, false));
            if (p == nullptr) { std::cerr << "unknown parameter: " << line << std::endl; return 1; }
            p->setValueNotifyingHost (line.fromFirstOccurrenceOf (" ", false, false).trim().getFloatValue());
        }
        apvts.state.setProperty ("patchName", juce::String (argv[4]), nullptr);
        apvts.state.setProperty ("patchType", argc > 5 ? juce::String (argv[5]) : juce::String(), nullptr);
        juce::MemoryBlock data;
        proc.getStateInformation (data);
        const juce::File out { juce::String (argv[3]) };
        out.getParentDirectory().createDirectory();
        const bool ok = out.replaceWithData (data.getData(), data.getSize());
        std::cerr << (ok ? "wrote " : "could not write ") << out.getFullPathName() << std::endl;
        return ok ? 0 : 1;
    }

    // SGDumpParams --load file.gpatch [prefix]: the parameters a saved patch sets, as the plugin loads it
    // (shows old patches being converted, e.g. the envelope curve)
    if (filter == "--load" && argc > 2)
    {
        juce::MemoryBlock data;
        if (! juce::File { juce::String (argv[2]) }.loadFileAsData (data)) { std::cerr << "cannot read" << std::endl; return 1; }
        proc.setStateInformation (data.getData(), static_cast<int> (data.getSize()));
        const juce::String want = argc > 3 ? juce::String (argv[3]) : juce::String();
        for (auto* p : proc.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p); r != nullptr && r->paramID.startsWith (want))
                std::cout << r->paramID << "	" << r->getValue() << "	" << r->getCurrentValueAsText() << std::endl;
        return 0;
    }

    int n = 0;
    for (auto* p : proc.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr) continue;

        const auto id = ranged->paramID;
        if (filter.isNotEmpty() && ! id.startsWith (filter)) continue;

        juce::String kind = "slider", extra;
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (ranged))
        {
            kind = "combo";
            extra = juce::String (choice->choices.size()) + "\t" + choice->choices.joinIntoString ("|");
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr)
        {
            kind = "toggle";
            extra = "2\ton|off";
        }
        else
        {
            const auto r = ranged->getNormalisableRange();
            extra = "0\t" + juce::String (r.start, 4) + ".." + juce::String (r.end, 4);
        }

        std::cout << id << "\t" << kind << "\t"
                  << juce::String (ranged->getDefaultValue(), 5) << "\t"
                  << extra << std::endl;
        ++n;
    }
    std::cerr << n << " parameters" << std::endl;
    return 0;
}
