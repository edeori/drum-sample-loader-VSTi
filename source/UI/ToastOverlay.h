#pragma once

#include "DrumVaultLookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace dsl::ui
{

/** Fixed bottom-right toast notifications, mirroring the mockup's
    .toast-region. Lives as an always-on-top child of the editor; other
    panels are given a reference and call show() directly.
*/
class ToastOverlay : public juce::Component, private juce::Timer
{
public:
    ToastOverlay() { setInterceptsMouseClicks(false, false); }

    void show(const juce::String& title, const juce::String& detail = {})
    {
        entries.push_back({ title, detail, juce::Time::getMillisecondCounter() });
        if (! isTimerRunning())
            startTimerHz(30);
        resized();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        int y = area.getBottom() - 8;

        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        {
            const auto age = juce::Time::getMillisecondCounter() - it->createdAtMs;
            const float alpha = age > 2500 ? juce::jmax(0.0f, 1.0f - (float) (age - 2500) / 250.0f) : 1.0f;

            const int h = it->detail.isEmpty() ? 34 : 50;
            juce::Rectangle<int> box(area.getRight() - 270, y - h, 250, h);
            y -= h + 8;

            g.setColour(Colours::panel.withAlpha(alpha));
            g.fillRoundedRectangle(box.toFloat(), 4.0f);
            g.setColour(Colours::blue.withAlpha(alpha));
            g.fillRect(box.getX(), box.getY(), 3, box.getHeight());
            g.setColour(juce::Colour(0xff3b4b57).withAlpha(alpha));
            g.drawRoundedRectangle(box.toFloat(), 4.0f, 1.0f);

            g.setColour(Colours::text.withAlpha(alpha));
            g.setFont(DrumVaultLookAndFeel::displayFont(13.0f));
            g.drawText(it->title, box.reduced(12, 6).removeFromTop(16), juce::Justification::centredLeft);

            if (! it->detail.isEmpty())
            {
                g.setColour(Colours::muted.withAlpha(alpha));
                g.setFont(DrumVaultLookAndFeel::bodyFont(10.5f));
                g.drawText(it->detail, box.reduced(12, 6).removeFromBottom(16), juce::Justification::centredLeft);
            }
        }
    }

private:
    struct Entry { juce::String title, detail; juce::uint32 createdAtMs; };
    std::vector<Entry> entries;

    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounter();
        entries.erase(std::remove_if(entries.begin(), entries.end(),
                                      [now](const Entry& e) { return now - e.createdAtMs > 2900; }),
                      entries.end());
        if (entries.empty())
            stopTimer();
        repaint();
    }
};

} // namespace dsl::ui
