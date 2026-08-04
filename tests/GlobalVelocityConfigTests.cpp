#include "Engine/GlobalVelocityConfig.h"
#include <juce_core/juce_core.h>

using namespace dsl;

class GlobalVelocityConfigTests : public juce::UnitTest
{
public:
    GlobalVelocityConfigTests() : juce::UnitTest("GlobalVelocityConfig", "Engine") {}

    void runTest() override
    {
        beginTest("Default 4-layer boundaries match docs/03-editor-ui.md");
        {
            GlobalVelocityConfig config;
            expectEquals(config.getLayerCount(), 4);
            expectEquals(config.selectLayerIndex(0), 0);
            expectEquals(config.selectLayerIndex(31), 0);
            expectEquals(config.selectLayerIndex(32), 1);
            expectEquals(config.selectLayerIndex(63), 1);
            expectEquals(config.selectLayerIndex(64), 2);
            expectEquals(config.selectLayerIndex(95), 2);
            expectEquals(config.selectLayerIndex(96), 3);
            expectEquals(config.selectLayerIndex(127), 3);
        }

        beginTest("Resizing layer count keeps thresholds strictly ascending");
        {
            GlobalVelocityConfig config;
            config.setLayerCount(2);
            expectEquals(config.getLayerCount(), 2);
            expectEquals((int) config.thresholds.size(), 1);

            config.setLayerCount(6);
            expectEquals(config.getLayerCount(), 6);
            expectEquals((int) config.thresholds.size(), 5);
            for (size_t i = 1; i < config.thresholds.size(); ++i)
                expect(config.thresholds[i] > config.thresholds[i - 1], "thresholds must stay strictly ascending");
        }

        beginTest("Preview velocities fall inside the selected layer");
        {
            GlobalVelocityConfig config;
            expectEquals(config.previewVelocityForLayer(0), 15);
            expectEquals(config.previewVelocityForLayer(1), 47);
            expectEquals(config.previewVelocityForLayer(2), 79);
            expectEquals(config.previewVelocityForLayer(3), 111);
            for (int layer = 0; layer < config.getLayerCount(); ++layer)
                expectEquals(config.selectLayerIndex(config.previewVelocityForLayer(layer)), layer);
        }

        beginTest("A single layer always selects index 0 regardless of velocity");
        {
            GlobalVelocityConfig config;
            config.setLayerCount(1);
            expectEquals(config.selectLayerIndex(0), 0);
            expectEquals(config.selectLayerIndex(64), 0);
            expectEquals(config.selectLayerIndex(127), 0);
        }
    }
};

static GlobalVelocityConfigTests globalVelocityConfigTestsInstance;
