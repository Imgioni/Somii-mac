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
