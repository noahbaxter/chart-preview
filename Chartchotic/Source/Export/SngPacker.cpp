#include "SngPacker.h"

namespace
{
    const char* const kIdentifier = "SNGPKG";
    constexpr int kIdentifierLength = 6;
    constexpr int kMaskLength = 16;

    // The name length field is a single byte, so this is a hard format limit
    // rather than a chosen one.
    constexpr size_t kMaxNameLength = 255;

    /**
        The index resets per file. Running it across the whole data section
        instead produces a container that unpacks to garbage for every file
        after the first, which no reader reports as a format error.
    */
    void maskInPlace(juce::MemoryBlock& data, const juce::uint8* mask)
    {
        auto* bytes = static_cast<juce::uint8*>(data.getData());
        for (size_t i = 0; i < data.getSize(); ++i)
            bytes[i] ^= (juce::uint8)(mask[i % kMaskLength] ^ (i & 0xFF));
    }
}

SngPacker::Result SngPacker::pack(const juce::File& output,
                                  const juce::StringPairArray& metadata,
                                  const std::vector<Entry>& files)
{
    Result result;
    result.output = output;

    if (files.empty())
    {
        result.message = "nothing to pack";
        return result;
    }

    // Everything is read and assembled before a byte is written, so a failure
    // partway leaves the old file alone instead of a truncated container that
    // still looks like a chart.
    struct Loaded
    {
        juce::String name;
        juce::MemoryBlock data;
    };

    std::vector<Loaded> loaded;
    for (const auto& entry : files)
    {
        if (!entry.source.existsAsFile())
        {
            result.message = "missing " + entry.source.getFullPathName();
            return result;
        }

        Loaded item;
        item.name = entry.name;
        if (item.name.getNumBytesAsUTF8() > kMaxNameLength)
        {
            result.message = "name too long to pack: " + item.name;
            return result;
        }
        if (!entry.source.loadFileAsData(item.data))
        {
            result.message = "could not read " + entry.source.getFullPathName();
            return result;
        }
        loaded.push_back(std::move(item));
    }

    juce::StringPairArray pairs;
    for (const auto& key : metadata.getAllKeys())
        if (key.isNotEmpty() && metadata[key].isNotEmpty())
            pairs.set(key, metadata[key]);

    // Both section lengths count the entry count that follows them but not
    // their own eight bytes.
    juce::int64 metadataLength = (juce::int64)sizeof(juce::uint64);
    for (const auto& key : pairs.getAllKeys())
        metadataLength += 4 + (juce::int64)key.getNumBytesAsUTF8()
                        + 4 + (juce::int64)pairs[key].getNumBytesAsUTF8();

    juce::int64 indexLength = (juce::int64)sizeof(juce::uint64);
    juce::int64 dataLength = 0;
    for (const auto& item : loaded)
    {
        indexLength += 1 + (juce::int64)item.name.getNumBytesAsUTF8() + 8 + 8;
        dataLength += (juce::int64)item.data.getSize();
    }

    // Where the first byte of file data lands. Index offsets are absolute, so
    // this has to be right or every file in the container reads from the
    // wrong place.
    const juce::int64 headerLength = kIdentifierLength + (juce::int64)sizeof(juce::uint32) + kMaskLength
                                   + (juce::int64)sizeof(juce::uint64) + metadataLength
                                   + (juce::int64)sizeof(juce::uint64) + indexLength
                                   + (juce::int64)sizeof(juce::uint64);

    juce::uint8 mask[kMaskLength];
    auto& random = juce::Random::getSystemRandom();
    for (auto& byte : mask)
        byte = (juce::uint8)random.nextInt(256);

    juce::MemoryOutputStream out;
    out.write(kIdentifier, kIdentifierLength);
    out.writeInt((int)kFormatVersion);
    out.write(mask, kMaskLength);

    out.writeInt64(metadataLength);
    out.writeInt64((juce::int64)pairs.size());
    for (const auto& key : pairs.getAllKeys())
    {
        const auto value = pairs[key];
        out.writeInt((int)key.getNumBytesAsUTF8());
        out.write(key.toRawUTF8(), key.getNumBytesAsUTF8());
        out.writeInt((int)value.getNumBytesAsUTF8());
        out.write(value.toRawUTF8(), value.getNumBytesAsUTF8());
    }

    out.writeInt64(indexLength);
    out.writeInt64((juce::int64)loaded.size());
    juce::int64 offset = headerLength;
    for (const auto& item : loaded)
    {
        out.writeByte((char)item.name.getNumBytesAsUTF8());
        out.write(item.name.toRawUTF8(), item.name.getNumBytesAsUTF8());
        out.writeInt64((juce::int64)item.data.getSize());
        out.writeInt64(offset);
        offset += (juce::int64)item.data.getSize();
    }

    out.writeInt64(dataLength);

    // Checked rather than asserted: a debug build would catch it, and a
    // release build would ship a chart that silently unpacks to nothing.
    if ((juce::int64)out.getDataSize() != headerLength)
    {
        result.message = "internal error, header is " + juce::String((int)out.getDataSize())
                       + " bytes but offsets assume " + juce::String((int)headerLength);
        return result;
    }

    for (auto& item : loaded)
    {
        maskInPlace(item.data, mask);
        out.write(item.data.getData(), item.data.getSize());
    }

    if (!output.replaceWithData(out.getData(), out.getDataSize()))
    {
        result.message = "could not write " + output.getFullPathName();
        return result;
    }

    result.ok = true;
    result.message = "packed " + juce::String((int)loaded.size()) + " files into "
                   + output.getFileName() + ", "
                   + juce::File::descriptionOfSizeInBytes((juce::int64)out.getDataSize());
    return result;
}
