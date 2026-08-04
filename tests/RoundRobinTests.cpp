#include "Engine/RoundRobinSelector.h"
#include <juce_core/juce_core.h>

using namespace dsl;

class RoundRobinTests : public juce::UnitTest
{
public:
    RoundRobinTests() : juce::UnitTest("RoundRobinSelector", "Engine") {}

    void runTest() override
    {
        juce::Random rng(12345);

        beginTest("Zero slots never plays");
        {
            expectEquals(selectRoundRobinSlot(0, -1, rng), -1);
        }

        beginTest("A single slot always returns index 0");
        {
            for (int i = 0; i < 20; ++i)
                expectEquals(selectRoundRobinSlot(1, 0, rng), 0);
        }

        beginTest("Never repeats the previous slot, and eventually covers every slot");
        {
            const int numSlots = 4;
            int previous = -1;
            bool seen[numSlots] = {};

            for (int i = 0; i < 500; ++i)
            {
                const int chosen = selectRoundRobinSlot(numSlots, previous, rng);
                expect(chosen >= 0 && chosen < numSlots, "chosen index out of range");
                if (previous >= 0)
                    expect(chosen != previous, "round-robin repeated the immediately previous slot");

                seen[chosen] = true;
                previous = chosen;
            }

            for (int i = 0; i < numSlots; ++i)
                expect(seen[i], "slot " + juce::String(i) + " was never chosen over 500 trials");
        }

        beginTest("With no previous slot, index 0 is reachable (no hidden exclusion bias)");
        {
            bool sawZero = false;
            for (int i = 0; i < 200 && ! sawZero; ++i)
                if (selectRoundRobinSlot(5, -1, rng) == 0)
                    sawZero = true;

            expect(sawZero, "index 0 was never selected when there was no previous slot to exclude");
        }
    }
};

static RoundRobinTests roundRobinTestsInstance;
