#include <juce_core/juce_core.h>

// Every *Tests.cpp file registers its juce::UnitTest subclasses via a static
// instance at file scope; JUCE's UnitTest constructor adds itself to the
// global test list, so simply linking those translation units in is enough
// for runAllTests() below to discover and run them.

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runAllTests();

    int numFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        numFailures += runner.getResult(i)->failures;

    if (numFailures > 0)
    {
        juce::Logger::writeToLog(juce::String(numFailures) + " test failure(s).");
        return 1;
    }

    juce::Logger::writeToLog("All tests passed.");
    return 0;
}
