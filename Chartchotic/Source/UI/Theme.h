#pragma once

#include <JuceHeader.h>

//==============================================================================
// Theme — single source of truth for all UI styling.
//
// Colors and fixed constants are constexpr. Scaled values (fonts, sizes) are
// recalculated once per resize via setControlHeight(). Components access
// everything the same way: Theme::coral, Theme::controlFont, Theme::arrowZone.

class Theme {
public:
    //==========================================================================
    // Colors

    static constexpr juce::uint32 coral         = 0xFFE8513A;
    static constexpr juce::uint32 darkBg        = 0xFF1A1A1A;
    static constexpr juce::uint32 darkBgLighter = 0xFF2A2A2A;
    static constexpr juce::uint32 darkBgHover   = 0xFF333333;
    static constexpr juce::uint32 textWhite     = 0xFFEEEEEE;
    static constexpr juce::uint32 textDim       = 0xFFAAAAAA;
    static constexpr juce::uint32 toolbarBg     = 0xCC111111;
    static constexpr juce::uint32 sceneBg      = 0xFF000000;

    // Accent palette (matches logo dot cluster)
    static constexpr juce::uint32 green  = 0xFF5CB85C;
    static constexpr juce::uint32 red    = coral;
    static constexpr juce::uint32 yellow = 0xFFF0C030;
    static constexpr juce::uint32 blue   = 0xFF3A7BD5;
    static constexpr juce::uint32 orange = 0xFFE8853A;
    static constexpr juce::uint32 purple = 0xFF9B59B6;

    //==========================================================================
    // Overlay parent — returns the AudioProcessorEditor so overlays inherit
    // the host DPI scale transform. Falls back to getTopLevelComponent() for
    // standalone mode where the editor IS the top-level.

    static inline juce::Component* getOverlayParent(juce::Component* child)
    {
        if (auto* editor = child->findParentComponentOfClass<juce::AudioProcessorEditor>())
            return editor;
        return child->getTopLevelComponent();
    }

    //==========================================================================
    // Fixed constants (not scale-dependent)

    static constexpr float panelBgAlpha    = 0.88f;
    static constexpr float overlayDimAlpha = 0.65f;
    static constexpr float borderAlpha     = 0.12f;
    static constexpr int   hoverDismissMs  = 120;

    //==========================================================================
    // Typeface

    static inline juce::Typeface::Ptr& getUITypefaceRef()
    {
        static juce::Typeface::Ptr tf;
#ifndef CHARTCHOTIC_NO_BINARY_DATA
        if (tf == nullptr)
            tf = juce::Typeface::createSystemTypefaceFor(
                BinaryData::ChakraPetchMedium_ttf, BinaryData::ChakraPetchMedium_ttfSize);
#endif
        return tf;
    }

    static inline juce::Typeface::Ptr getUITypeface() { return getUITypefaceRef(); }

    static inline void clearTypefaces()
    {
        controlFont = {};
        pillFont = {};
        buttonFont = {};
        headerFont = {};
        smallFont = {};
        getUITypefaceRef() = nullptr;
    }

    // For one-off font sizes not covered by the scaled fonts below
    static inline juce::Font getUIFont(float height)
    {
        return juce::Font(getUITypeface()).withHeight(height * uiFontScale_);
    }


    //==========================================================================
    // Scaled values — set once per resize, read everywhere like constants.
    //
    // Usage:
    //   Theme::setControlHeight(h);   // call once in toolbar resized()
    //   g.setFont(Theme::controlFont); // use anywhere

    static inline juce::Font controlFont {};  // steppers, segments
    static inline juce::Font pillFont {};     // pill toggles
    static inline juce::Font buttonFont {};   // text buttons
    static inline juce::Font headerFont {};   // section headers
    static inline juce::Font smallFont {};    // compact pills, small buttons
    static inline float arrowZone     = 0;    // arrow hit-zone width (px)
    static inline float arrowFontSize = 0;    // arrow glyph size (px)
    static inline float pillCorner    = 0;    // pill/stepper corner radius (px)
    static inline float fontSize      = 0;    // label/slider text font size (px)
    static inline float cornerRadius  = 0;    // small controls (buttons, text editors)
    static inline float panelRadius   = 0;    // popup panel corner radius
    static inline float cardRadius    = 0;    // modal card corner radius

    static inline void setControlHeight(float h)
    {
        controlFont  = getUIFont(h * controlFontR_);
        pillFont     = getUIFont(h * pillFontR_);
        buttonFont   = getUIFont(h * buttonFontR_);
        headerFont   = getUIFont(h * headerFontR_);
        smallFont    = getUIFont(h * smallFontR_);
        arrowZone    = h * arrowZoneR_;
        arrowFontSize = h * arrowFontR_;
        pillCorner   = h * pillRadiusR_;
        fontSize     = h * fontSizeR_;
        cornerRadius = h * cornerRadiusR_;
        panelRadius  = h * panelRadiusR_;
        cardRadius   = h * cardRadiusR_;
    }

private:
    Theme() = delete;

    //==========================================================================
    // Design ratios — tune these, components never reference them directly.

    static constexpr float uiFontScale_   = 1.2f;
    static constexpr float controlFontR_  = 0.393f;
    static constexpr float pillFontR_     = 0.5f;
    static constexpr float buttonFontR_   = 0.46f;
    static constexpr float headerFontR_   = 0.625f;
    static constexpr float arrowZoneR_    = 0.571f;
    static constexpr float arrowFontR_    = 0.321f;
    static constexpr float smallFontR_    = 0.36f;
    static constexpr float pillRadiusR_   = 0.35f;
    static constexpr float fontSizeR_     = 0.393f;  // 11/28
    static constexpr float cornerRadiusR_ = 0.107f;  // 3/28
    static constexpr float panelRadiusR_  = 0.214f;  // 6/28
    static constexpr float cardRadiusR_   = 0.357f;  // 10/28
};
