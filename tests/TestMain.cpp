#include <juce_core/juce_core.h>

#include <iostream>

namespace
{
// juce::UnitTestRunner logs to the debugger by default; print to the console instead.
class ConsoleRunner final : public juce::UnitTestRunner
{
    void logMessage (const juce::String& message) override
    {
        std::cout << message.toStdString() << std::endl;
    }
};
} // namespace

int main (int argc, char* argv[])
{
    ConsoleRunner runner;
    runner.setAssertOnFailure (false);

    if (argc > 1) runner.runTestsInCategory (argv[1]);
    else          runner.runAllTests();

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* r = runner.getResult (i);
        failures += r->failures;
        passes += r->passes;
    }
    std::cout << "\n==== " << passes << " checks passed, " << failures << " failed ====\n";
    return failures > 0 ? 1 : 0;
}
