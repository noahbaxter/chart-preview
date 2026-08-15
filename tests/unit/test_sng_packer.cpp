#include <catch2/catch_test_macros.hpp>
#include <JuceHeader.h>

#include "Export/SngPacker.h"

namespace
{
    // CHARTCHOTIC_TEST_DIR puts the container somewhere an external decoder
    // can be pointed at, which is how the format gets checked against
    // something other than the code that wrote it.
    juce::File scratch()
    {
        auto override_ = juce::SystemStats::getEnvironmentVariable("CHARTCHOTIC_TEST_DIR", {});
        auto dir = override_.isNotEmpty()
                     ? juce::File(override_)
                     : juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("chartchotic_sng_test");
        dir.createDirectory();
        return dir;
    }

    juce::File writeFile(const juce::String& name, const juce::MemoryBlock& data)
    {
        auto file = scratch().getChildFile(name);
        file.replaceWithData(data.getData(), data.getSize());
        return file;
    }

    /** Bytes that exercise the mask: every value, past the 16-byte wrap and the 256 wrap. */
    juce::MemoryBlock rampBytes(size_t length)
    {
        juce::MemoryBlock block(length);
        auto* bytes = static_cast<juce::uint8*>(block.getData());
        for (size_t i = 0; i < length; ++i)
            bytes[i] = (juce::uint8)(i * 7 + 3);
        return block;
    }
}

TEST_CASE("sng container round-trips its files", "[sng]")
{
    const auto midi = rampBytes(1000);
    const auto audio = rampBytes(517);

    std::vector<SngPacker::Entry> entries {
        { "notes.mid", writeFile("notes.mid", midi) },
        { "song.opus", writeFile("song.opus", audio) },
    };

    juce::StringPairArray metadata;
    metadata.set("name", "Gray World");
    metadata.set("artist", "Lockslip");
    metadata.set("pro_drums", "1");
    metadata.set("blank", "");          // dropped: the format cannot express it

    auto output = scratch().getChildFile("packed.sng");
    auto result = SngPacker::pack(output, metadata, entries);

    INFO(result.message);
    REQUIRE(result.ok);
    REQUIRE(output.existsAsFile());

    juce::MemoryBlock packed;
    REQUIRE(output.loadFileAsData(packed));
    const auto* raw = static_cast<const juce::uint8*>(packed.getData());

    auto readU64 = [&raw](size_t at) {
        juce::uint64 v = 0;
        for (int i = 7; i >= 0; --i) v = (v << 8) | raw[at + (size_t)i];
        return v;
    };
    auto readU32 = [&raw](size_t at) {
        juce::uint32 v = 0;
        for (int i = 3; i >= 0; --i) v = (v << 8) | raw[at + (size_t)i];
        return v;
    };

    SECTION("header")
    {
        REQUIRE(juce::String::fromUTF8((const char*)raw, 6) == "SNGPKG");
        REQUIRE(readU32(6) == SngPacker::kFormatVersion);
    }

    juce::uint8 mask[16];
    std::memcpy(mask, raw + 10, 16);

    size_t pos = 26;
    const auto metadataLength = readU64(pos);
    const auto metadataCount = readU64(pos + 8);

    SECTION("empty metadata values are dropped")
    {
        REQUIRE(metadataCount == 3);
    }

    pos += 8 + metadataLength;
    const auto indexLength = readU64(pos);
    const auto fileCount = readU64(pos + 8);
    REQUIRE(fileCount == 2);

    // Walk the index, then unmask each file and compare against what went in.
    size_t entry = pos + 16;
    std::vector<std::pair<juce::String, juce::MemoryBlock>> unpacked;
    for (juce::uint64 i = 0; i < fileCount; ++i)
    {
        const size_t nameLength = raw[entry];
        auto name = juce::String::fromUTF8((const char*)raw + entry + 1, (int)nameLength);
        const auto contentsLength = readU64(entry + 1 + nameLength);
        const auto contentsIndex = readU64(entry + 1 + nameLength + 8);
        entry += 1 + nameLength + 16;

        REQUIRE(contentsIndex + contentsLength <= packed.getSize());

        juce::MemoryBlock decoded(contentsLength);
        auto* out = static_cast<juce::uint8*>(decoded.getData());
        for (size_t b = 0; b < (size_t)contentsLength; ++b)
            out[b] = (juce::uint8)(raw[contentsIndex + b] ^ (mask[b % 16] ^ (b & 0xFF)));

        unpacked.emplace_back(name, std::move(decoded));
    }

    REQUIRE(unpacked[0].first == "notes.mid");
    REQUIRE(unpacked[0].second == midi);
    REQUIRE(unpacked[1].first == "song.opus");
    REQUIRE(unpacked[1].second == audio);

    // The index sits where the section lengths say it does.
    REQUIRE(entry == pos + 8 + indexLength);
}

TEST_CASE("sng packing refuses what it cannot write", "[sng]")
{
    auto output = scratch().getChildFile("bad.sng");

    SECTION("nothing to pack")
    {
        REQUIRE_FALSE(SngPacker::pack(output, {}, {}).ok);
    }

    SECTION("missing source leaves no output")
    {
        output.deleteFile();
        std::vector<SngPacker::Entry> entries { { "notes.mid", scratch().getChildFile("nope.mid") } };
        REQUIRE_FALSE(SngPacker::pack(output, {}, entries).ok);
        REQUIRE_FALSE(output.existsAsFile());
    }
}
