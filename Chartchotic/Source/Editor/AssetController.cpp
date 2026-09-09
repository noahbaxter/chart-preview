#include "AssetController.h"

void AssetController::init(ToolbarComponent& tb,
                           std::function<void(std::function<void(HighwayComponent&)>)> hwIterator)
{
    toolbar = &tb;
    forAllHighways = std::move(hwIterator);

    backgroundImageDefault = juce::ImageCache::getFromMemory(
        BinaryData::background_png, BinaryData::background_pngSize);
    backgroundImageCurrent = backgroundImageDefault;

    reaperLogo = juce::Drawable::createFromImageData(
        BinaryData::logoreaper_svg, BinaryData::logoreaper_svgSize);

    scanBackgrounds();
    scanHighwayTextures();
}

void AssetController::scanBackgrounds()
{
#if JUCE_MAC
    backgroundDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/Chartchotic/backgrounds");
#elif JUCE_WINDOWS
    backgroundDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Chartchotic/backgrounds");
#endif

    backgroundNames.clear();

    if (!backgroundDirectory.isDirectory())
        backgroundDirectory.createDirectory();

    auto files = backgroundDirectory.findChildFiles(juce::File::findFiles, false, "*.png;*.jpg;*.jpeg");
    files.sort();
    for (auto& f : files)
        backgroundNames.add(f.getFileNameWithoutExtension());

    toolbar->setBackgroundList(backgroundNames);
}

void AssetController::loadBackground(const juce::String& filename)
{
    if (filename.isEmpty())
    {
        backgroundImageCurrent = backgroundImageDefault;
        return;
    }

    for (auto ext : { ".png", ".jpg", ".jpeg" })
    {
        auto file = backgroundDirectory.getChildFile(filename + ext);
        if (file.existsAsFile())
        {
            auto img = juce::ImageFileFormat::loadFrom(file);
            if (img.isValid())
            {
                backgroundImageCurrent = img;
                return;
            }
        }
    }

    backgroundImageCurrent = backgroundImageDefault;
}

void AssetController::scanHighwayTextures()
{
#if JUCE_MAC
    highwayTextureDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/Chartchotic/highways");
#elif JUCE_WINDOWS
    highwayTextureDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Chartchotic/highways");
#endif

    highwayTextureNames.clear();

    if (!highwayTextureDirectory.isDirectory())
        highwayTextureDirectory.createDirectory();

    // Install bundled default textures if missing
    struct BundledTexture { const char* filename; const char* data; int size; };
    const BundledTexture bundled[] = {
        { "kanaizo_amber.png",             BinaryData::kanaizo_amber_png,             BinaryData::kanaizo_amber_pngSize },
        { "kanaizo_darkwood.png",          BinaryData::kanaizo_darkwood_png,          BinaryData::kanaizo_darkwood_pngSize },
        { "gothic_default.png",            BinaryData::gothic_default_png,            BinaryData::gothic_default_pngSize },
        { "kanaizo_ornate.png",            BinaryData::kanaizo_ornate_png,            BinaryData::kanaizo_ornate_pngSize },
        { "kanaizo_BlueCaustic.png",       BinaryData::kanaizo_BlueCaustic_png,       BinaryData::kanaizo_BlueCaustic_pngSize },
        { "kanaizo_BlueMagentaHex.png",    BinaryData::kanaizo_BlueMagentaHex_png,    BinaryData::kanaizo_BlueMagentaHex_pngSize },
        { "kanaizo_FretWood.png",          BinaryData::kanaizo_FretWood_png,          BinaryData::kanaizo_FretWood_pngSize },
        { "kanaizo_IcyMetal.png",          BinaryData::kanaizo_IcyMetal_png,          BinaryData::kanaizo_IcyMetal_pngSize },
        { "kanaizo_BrightRadiated.png",    BinaryData::kanaizo_BrightRadiated_png,    BinaryData::kanaizo_BrightRadiated_pngSize },
        { "kanaizo_ModernHueistic.png",    BinaryData::kanaizo_ModernHueistic_png,    BinaryData::kanaizo_ModernHueistic_pngSize },
        { "kanaizo_RedCaustic.png",        BinaryData::kanaizo_RedCaustic_png,        BinaryData::kanaizo_RedCaustic_pngSize },
        { "kanaizo_ScratchedRegular.png",  BinaryData::kanaizo_ScratchedRegular_png,  BinaryData::kanaizo_ScratchedRegular_pngSize },
    };
    for (auto& t : bundled)
    {
        auto file = highwayTextureDirectory.getChildFile(t.filename);
        if (!file.existsAsFile())
            file.replaceWithData(t.data, t.size);
    }

    auto files = highwayTextureDirectory.findChildFiles(juce::File::findFiles, false, "*.png");
    files.sort();
    for (auto& f : files)
        highwayTextureNames.add(f.getFileNameWithoutExtension());

    toolbar->setHighwayTextureList(highwayTextureNames);
}

void AssetController::loadHighwayTexture(const juce::String& filename)
{
    auto file = highwayTextureDirectory.getChildFile(filename + ".png");
    if (!file.existsAsFile())
    {
        forAllHighways([](auto& hw) { hw.clearTexture(); });
        return;
    }

    auto img = juce::ImageFileFormat::loadFrom(file);
    if (img.isValid())
        forAllHighways([&img](auto& hw) { hw.setTexture(img); });
    else
        forAllHighways([](auto& hw) { hw.clearTexture(); });
}
