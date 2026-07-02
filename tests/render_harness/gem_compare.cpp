/*
    ==============================================================================

        gem_compare.cpp
        Author: Noah Baxter

        Bakes procedural gem art and pixel-diffs it against the original
        generated PNGs (BinaryData). This is the success metric for the
        procedural gem work: per-asset mean/max abs diff plus a stacked
        original | bake | amplified-diff compare sheet.

        Usage:
            gem_compare [--out DIR]
            gem_compare --sample <asset> <x> <y>     print original RGBA at pixel
            gem_compare --column <asset> <x>         print original column profile
            gem_compare --selftest                   blend-math sanity checks

    ==============================================================================
*/

#include <JuceHeader.h>
#include <cstdio>
#include <functional>
#include <vector>

#include "Visual/Renderers/Gems/GemArtCommon.h"

struct CompareEntry
{
    const char* name;
    const void* pngData;
    int pngSize;
    // Bakes the procedural version; arg is the original's alpha bbox.
    // Null bake = compare original against itself (sanity: diff 0).
    std::function<juce::Image(juce::Rectangle<int>)> bake;
};

static juce::Rectangle<int> alphaBBox(const juce::Image& img)
{
    int minX = img.getWidth(), minY = img.getHeight(), maxX = -1, maxY = -1;
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::readOnly);
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (bd.getPixelColour(x, y).getAlpha() > 8)
            {
                minX = juce::jmin(minX, x); minY = juce::jmin(minY, y);
                maxX = juce::jmax(maxX, x); maxY = juce::jmax(maxY, y);
            }
    if (maxX < 0) return {};
    return { minX, minY, maxX - minX + 1, maxY - minY + 1 };
}

// Mean/max abs diff over pixels where either image has alpha; diff image 8x amplified.
static void diffImages(const juce::Image& a, const juce::Image& b,
                       juce::Image& outDiff, double& outMean, int& outMax)
{
    outDiff = juce::Image(juce::Image::ARGB, a.getWidth(), a.getHeight(), true);
    juce::Image::BitmapData da(a, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData db(b, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dd(outDiff, juce::Image::BitmapData::writeOnly);

    long long total = 0, count = 0;
    outMax = 0;
    for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x)
        {
            auto ca = da.getPixelColour(x, y);
            auto cb = (x < b.getWidth() && y < b.getHeight()) ? db.getPixelColour(x, y)
                                                              : juce::Colour();
            if (ca.getAlpha() < 8 && cb.getAlpha() < 8) continue;
            int d = (std::abs((int) ca.getRed()   - cb.getRed())
                   + std::abs((int) ca.getGreen() - cb.getGreen())
                   + std::abs((int) ca.getBlue()  - cb.getBlue())
                   + std::abs((int) ca.getAlpha() - cb.getAlpha())) / 4;
            total += d; ++count;
            outMax = juce::jmax(outMax, d);
            auto v = (juce::uint8) juce::jmin(255, d * 8);
            dd.setPixelColour(x, y, juce::Colour(v, v, v).withAlpha((juce::uint8) 255));
        }
    outMean = count > 0 ? (double) total / (double) count : 0.0;
}

static std::vector<CompareEntry> buildEntries()
{
    return {
        // Baked entries get registered per family as they land.
        { "note_blue", BinaryData::note_blue_png, BinaryData::note_blue_pngSize, nullptr },
    };
}

static const CompareEntry* findEntry(const std::vector<CompareEntry>& entries,
                                     const juce::String& name)
{
    for (auto& e : entries)
        if (name == e.name) return &e;
    return nullptr;
}

static int runSelftest();

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto entries = buildEntries();

    juce::String outDirName = "out";
    for (int i = 1; i < argc; ++i)
    {
        juce::String a(argv[i]);
        if (a == "--selftest")
            return runSelftest();

        if (a == "--sample" && i + 3 < argc)
        {
            auto* e = findEntry(entries, argv[i + 1]);
            if (e == nullptr) { std::printf("unknown asset %s\n", argv[i + 1]); return 1; }
            auto img = juce::ImageCache::getFromMemory(e->pngData, e->pngSize);
            int x = juce::String(argv[i + 2]).getIntValue();
            int y = juce::String(argv[i + 3]).getIntValue();
            auto c = img.getPixelAt(x, y);
            std::printf("%s %d,%d rgba %d %d %d %d hex %s\n", e->name, x, y,
                        c.getRed(), c.getGreen(), c.getBlue(), c.getAlpha(),
                        c.toDisplayString(true).toRawUTF8());
            return 0;
        }

        if (a == "--column" && i + 2 < argc)
        {
            auto* e = findEntry(entries, argv[i + 1]);
            if (e == nullptr) { std::printf("unknown asset %s\n", argv[i + 1]); return 1; }
            auto img = juce::ImageCache::getFromMemory(e->pngData, e->pngSize);
            int x = juce::String(argv[i + 2]).getIntValue();
            for (int y = 0; y < img.getHeight(); ++y)
            {
                auto c = img.getPixelAt(x, y);
                if (c.getAlpha() < 8) continue;
                std::printf("%4d  %3d %3d %3d %3d\n", y,
                            c.getRed(), c.getGreen(), c.getBlue(), c.getAlpha());
            }
            return 0;
        }

        if (a == "--out" && i + 1 < argc)
            outDirName = argv[++i];
    }

    auto stamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H%M%S");
    auto out = juce::File::getCurrentWorkingDirectory()
                   .getChildFile(outDirName)
                   .getChildFile(stamp + "_gem_compare");
    out.createDirectory();

    bool anyFail = false;
    for (auto& e : entries)
    {
        auto original = juce::ImageCache::getFromMemory(e.pngData, e.pngSize);
        auto bbox = alphaBBox(original);
        juce::Image baked = e.bake ? e.bake(bbox) : original;

        juce::Image diff;
        double mean = 0.0; int mx = 0;
        diffImages(original, baked, diff, mean, mx);
        std::printf("%-16s mean %7.3f  max %3d  bbox %d,%d %dx%d\n",
                    e.name, mean, mx,
                    bbox.getX(), bbox.getY(), bbox.getWidth(), bbox.getHeight());
        if (mean >= 2.0) anyFail = true;

        juce::Image sheet(juce::Image::ARGB, original.getWidth(),
                          original.getHeight() * 3, true);
        {
            juce::Graphics g(sheet);
            g.fillAll(juce::Colour(0xff202020));
            g.drawImageAt(original, 0, 0);
            g.drawImageAt(baked, 0, original.getHeight());
            g.drawImageAt(diff, 0, original.getHeight() * 2);
        }
        juce::PNGImageFormat png;
        juce::File sheetFile = out.getChildFile(juce::String(e.name) + "_compare.png");
        juce::FileOutputStream os(sheetFile);
        if (os.openedOk())
            png.writeImageToStream(sheet, os);
    }

    std::printf("sheets: %s\n", out.getFullPathName().toRawUTF8());
    return anyFail ? 1 : 0;
}

static bool near(float a, float b) { return std::abs(a - b) < 1e-5f; }

static int runSelftest()
{
    // Overlay: identity at s=0.5, saturates at s=1 for b>=0.5
    if (! near(GemArt::blendOverlay(0.25f, 0.5f), 0.25f)) { std::printf("overlay fail 1\n"); return 1; }
    if (! near(GemArt::blendOverlay(0.75f, 0.5f), 0.75f)) { std::printf("overlay fail 2\n"); return 1; }
    if (! near(GemArt::blendOverlay(0.5f, 1.0f), 1.0f))   { std::printf("overlay fail 3\n"); return 1; }
    if (! near(GemArt::blendOverlay(0.0f, 1.0f), 0.0f))   { std::printf("overlay fail 4\n"); return 1; }

    // Colour blend keeps the backdrop's luminosity
    auto grey = juce::Colour::fromFloatRGBA(0.5f, 0.5f, 0.5f, 1.0f);
    auto red  = juce::Colours::red;
    auto r    = GemArt::blendColour(grey, red);
    float l   = 0.3f * r.getFloatRed() + 0.59f * r.getFloatGreen() + 0.11f * r.getFloatBlue();
    if (std::abs(l - 0.5f) > 0.01f) { std::printf("colour blend lum fail (%f)\n", l); return 1; }
    if (r.getFloatRed() <= r.getFloatGreen()) { std::printf("colour blend hue fail\n"); return 1; }

    std::printf("selftest ok\n");
    return 0;
}
