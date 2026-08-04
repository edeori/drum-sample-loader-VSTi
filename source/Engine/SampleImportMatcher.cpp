#include "SampleImportMatcher.h"
#include <algorithm>
#include <vector>

namespace dsl
{

namespace
{
    juce::String canonical(juce::String value)
    {
        value = value.toUpperCase();
        value = value.replace("OVERHEADS", "OH").replace("OVERHEAD", "OH");
        value = value.replace("ROOMS", "RM").replace("ROOM", "RM");

        juce::String result;
        for (auto c : value)
            if (juce::CharacterFunctions::isLetterOrDigit(c))
                result += juce::String::charToString(c);
        return result;
    }

    bool containsOnlyDigits(const juce::String& value)
    {
        if (value.isEmpty())
            return false;

        for (auto c : value)
            if (! juce::CharacterFunctions::isDigit(c))
                return false;
        return true;
    }

    bool containsToken(const juce::String& value, const juce::String& wantedToken)
    {
        const auto upper = value.toUpperCase();
        juce::String token;
        for (auto c : upper)
        {
            if (juce::CharacterFunctions::isLetterOrDigit(c))
            {
                token += juce::String::charToString(c);
            }
            else
            {
                if (token == wantedToken)
                    return true;
                token.clear();
            }
        }
        return token == wantedToken;
    }

    juce::StringArray nameTokens(const juce::String& value)
    {
        juce::String separated;
        for (auto c : value.toUpperCase())
            separated += juce::CharacterFunctions::isLetterOrDigit(c)
                ? juce::String::charToString(c) : " ";

        juce::StringArray tokens;
        tokens.addTokens(separated, " ", {});
        tokens.removeEmptyStrings();
        return tokens;
    }

    struct Alias
    {
        juce::String text;
        juce::String channelId;
    };

    void addAlias(std::vector<Alias>& aliases, const juce::String& value, const juce::String& channelId)
    {
        const auto text = canonical(value);
        if (text.isEmpty())
            return;

        for (auto& alias : aliases)
            if (alias.text == text && alias.channelId == channelId)
                return;

        aliases.push_back({ text, channelId });
    }
}

std::optional<SampleImportMatch> SampleImportMatcher::match(const juce::String& fileNameWithoutExtension,
                                                             const Instrument& instrument)
{
    std::vector<Alias> aliases;
    juce::String primarySpotChannel;

    for (auto& mic : instrument.micChannels)
    {
        addAlias(aliases, mic.id, mic.id);
        addAlias(aliases, mic.displayName, mic.id);

        if (mic.role == MicRole::Overhead)
        {
            if (mic.numChannels == 2)
            {
                addAlias(aliases, "OH", mic.id);
                addAlias(aliases, "OH Stereo", mic.id);
                addAlias(aliases, "OH ST", mic.id);
                addAlias(aliases, "Overheads ST", mic.id);
            }
        }
        else if (mic.role == MicRole::Room)
        {
            if (mic.numChannels == 2)
            {
                addAlias(aliases, "Room", mic.id);
                addAlias(aliases, "Room Stereo", mic.id);
                addAlias(aliases, "Room ST", mic.id);
            }
        }
        else if (primarySpotChannel.isEmpty())
        {
            primarySpotChannel = mic.id;
        }
    }

    if (primarySpotChannel.isNotEmpty())
        addAlias(aliases, instrument.name, primarySpotChannel);

    // Common commercial-pack abbreviations.
    for (auto& mic : instrument.micChannels)
    {
        const auto id = canonical(mic.id);
        const auto displayName = canonical(mic.displayName);
        if (id.contains("BOTTOM") || displayName.contains("BOTTOM"))
        {
            addAlias(aliases, "BOT", mic.id);
            addAlias(aliases, "BTM", mic.id);
        }
    }

    // Longest alias wins, so e.g. "OH Stereo" is preferred over "OH" and
    // "Top Spot" over the generic "Spot" suffix.
    std::sort(aliases.begin(), aliases.end(), [](const Alias& a, const Alias& b)
    {
        return a.text.length() > b.text.length();
    });

    const auto fileTag = canonical(fileNameWithoutExtension);
    const auto instrumentTag = canonical(instrument.name);
    auto normalizeVariation = [&instrumentTag](juce::String variation)
    {
        // Packs often repeat the instrument after a take separator:
        // "Snare 02-Snare Spot". Remove only that trailing repetition so it
        // groups with "Snare 02-OH" and "Snare 02-Room".
        if (instrumentTag.isNotEmpty() && variation.endsWith(instrumentTag))
            variation = variation.dropLastCharacters(instrumentTag.length());
        return variation;
    };

    for (auto& alias : aliases)
    {
        if (fileTag.endsWith(alias.text))
        {
            auto variation = normalizeVariation(fileTag.dropLastCharacters(alias.text.length()));
            return SampleImportMatch { alias.channelId, variation };
        }

        // Also accept an RR/take number after the mic name: OH_01,
        // Snare-Room-Stereo-02, etc. Keep that number in the variation key so
        // matching Spot/OH/Room files land in the same slot.
        int occurrence = fileTag.lastIndexOf(alias.text);
        if (occurrence >= 0)
        {
            const auto trailing = fileTag.substring(occurrence + alias.text.length());
            if (containsOnlyDigits(trailing))
            {
                auto variation = normalizeVariation(fileTag.substring(0, occurrence)) + trailing;
                return SampleImportMatch { alias.channelId, variation };
            }
        }
    }

    return std::nullopt;
}

bool SampleImportMatcher::hasStereoMarker(const juce::String& fileNameWithoutExtension)
{
    const auto upper = fileNameWithoutExtension.toUpperCase();
    juce::String token;
    for (auto c : upper)
    {
        if (juce::CharacterFunctions::isLetterOrDigit(c))
        {
            token += juce::String::charToString(c);
        }
        else
        {
            if (token == "ST" || token == "STEREO")
                return true;
            token.clear();
        }
    }

    if (token == "ST" || token == "STEREO")
        return true;

    // Also accept the compact forms OHST / RMST at the end of a pack name.
    const auto compact = canonical(fileNameWithoutExtension);
    return compact.endsWith("OHST") || compact.endsWith("RMST");
}

bool SampleImportMatcher::hasChokeMarker(const juce::String& fileNameWithoutExtension)
{
    return fileNameWithoutExtension.containsIgnoreCase("choke")
        || containsToken(fileNameWithoutExtension, "CHK");
}

bool SampleImportMatcher::hasRoomMidMarker(const juce::String& fileNameWithoutExtension)
{
    const auto tokens = nameTokens(fileNameWithoutExtension);
    for (int i = 0; i + 1 < tokens.size(); ++i)
        if ((tokens[i] == "ROOM" || tokens[i] == "RM") && tokens[i + 1] == "M")
            return true;
    return false;
}

juce::String SampleImportMatcher::targetArticulationId(
    const juce::String& fileNameWithoutExtension,
    const Instrument& instrument,
    const juce::String& selectedArticulationId)
{
    if (instrument.findArticulation("bell") != nullptr
        && fileNameWithoutExtension.containsIgnoreCase("bell"))
        return "bell";

    if (instrument.findArticulation("choke") != nullptr
        && hasChokeMarker(fileNameWithoutExtension))
        return "choke";

    return selectedArticulationId;
}

} // namespace dsl
