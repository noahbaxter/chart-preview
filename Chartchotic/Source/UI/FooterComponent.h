#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "ControlConstants.h"
#include "HelpTypes.h"
#include "StatusBarComponent.h"
#include "UpdateBannerComponent.h"

class FooterComponent : public juce::Component
{
public:
    static constexpr float footerRatio = FOOTER_RATIO;
    static constexpr int maxFooterHeight = FOOTER_MAX_HEIGHT;
    static constexpr float paddingRatio = 0.15f;

    FooterComponent(UpdateBannerComponent& b) : banner(b)
    {
        addAndMakeVisible(banner);
        addAndMakeVisible(statusBar);

        versionLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.3f));
        versionLabel.setJustificationType(juce::Justification::centredRight);
        versionLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(versionLabel);
    }

    void init(const juce::String& versionText)
    {
        versionLabel.setText(versionText, juce::dontSendNotification);
    }

    void setUpdateAvailable()
    {
        banner.setVisible(true);
        resized();
    }

    void setDawIcon(juce::Drawable* icon) { dawIcon = icon; resized(); repaint(); }

    StatusBarComponent& getStatusBar() { return statusBar; }

    void setHelpText(const HelpText& segs)
    {
        helpOverride = segs;
        repaint();
    }

    void clearHelpText()
    {
        helpOverride.clear();
        repaint();
    }

    void setDefaultHelpText(const HelpText& segs)
    {
        helpDefault = segs;
        if (helpOverride.empty())
            repaint();
    }

    void setModeAccent(juce::Colour c) { modeAccent = c; repaint(); }

    void resized() override
    {
        auto area = getLocalBounds();
        int pad = juce::roundToInt(area.getHeight() * paddingRatio);
        int h = area.getHeight();

        int iconRoom = dawIcon ? (h / 2 + pad) : 0;
        int rightW = juce::jmax(area.getWidth() / 4, 120);

        helpArea = { pad, 0, area.getWidth() - rightW - iconRoom - pad * 2, h };
        helpFontSize = juce::jmax(11.0f, h * 0.45f);

        int rightX = area.getWidth() - rightW - iconRoom;
        int topH = h / 2;
        statusBar.setBounds(rightX, 0, rightW, topH);

        float versionFontSize = juce::jmax(8.0f, h * 0.25f);
        versionLabel.setFont(Theme::getUIFont(versionFontSize));
        versionLabel.setBounds(rightX, topH, rightW, h - topH);

        if (banner.hasUpdate())
        {
            int bs = juce::roundToInt(h * 0.4f);
            banner.setBounds(rightX - bs - pad, (h - bs) / 2, bs, bs);
            banner.setVisible(true);
        }
        else
        {
            banner.setVisible(false);
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(Theme::sceneBg));

        if (dawIcon)
        {
            int pad = juce::roundToInt(getHeight() * paddingRatio);
            int iconSize = juce::roundToInt(getHeight() * 0.5f);
            int iconX = getWidth() - iconSize - pad;
            int iconY = (getHeight() - iconSize) / 2;
            juce::Rectangle<float> iconBounds((float)iconX, (float)iconY,
                (float)iconSize, (float)iconSize);
            dawIcon->drawWithin(g, iconBounds, juce::RectanglePlacement::centred, 0.8f);
        }

        const auto& segs = helpOverride.empty() ? helpDefault : helpOverride;
        if (!segs.empty() && helpArea.getWidth() > 0)
            paintHelpText(g, segs);
    }

    juce::Label label;
    juce::Colour normalColour, hoverColour;

private:
    void paintHelpText(juce::Graphics& g, const HelpText& segs)
    {
        juce::Font normalFont = Theme::getUIFont(helpFontSize);
        juce::Colour dimColour = juce::Colours::white.withAlpha(0.35f);
        juce::Colour keyColour = modeAccent;

        float spaceW = normalFont.getStringWidthFloat(" ");
        float keyPadX  = spaceW * HELP_KEY_PAD_X;
        float keyGap   = spaceW;
        float groupGap = spaceW * 2.5f;

        auto gapBefore = [&](int i) -> float {
            if (i == 0) return 0.0f;
            bool prevKey = segs[(size_t)i - 1].isKey;
            bool curKey  = segs[(size_t)i].isKey;
            if (prevKey && curKey)   return spaceW * 0.5f;
            if (!prevKey && curKey)  return groupGap;
            return keyGap;
        };

        float totalW = 0.0f;
        for (int i = 0; i < (int)segs.size(); ++i)
        {
            float sw = normalFont.getStringWidthFloat(segs[(size_t)i].text);
            totalW += gapBefore(i) + (segs[(size_t)i].isKey ? sw + keyPadX * 2.0f : sw + 1.0f);
        }

        bool wraps = totalW > (float)helpArea.getWidth();
        float x = (float)helpArea.getX();
        float y = wraps ? (float)helpArea.getY()
                        : (float)(helpArea.getY() + helpArea.getHeight()) - normalFont.getHeight();
        float maxX = (float)(helpArea.getX() + helpArea.getWidth());
        float lineH = normalFont.getHeight();

        g.setFont(normalFont);
        for (int i = 0; i < (int)segs.size(); ++i)
        {
            float gap = gapBefore(i);
            float segW = normalFont.getStringWidthFloat(segs[(size_t)i].text);

            if (x + gap + segW > maxX && wraps)
            {
                x = (float)helpArea.getX();
                y += lineH;
                gap = 0.0f;
            }

            x += gap;
            bool isKey = segs[(size_t)i].isKey;
            float drawW = isKey ? segW + keyPadX * 2.0f : segW + 1.0f;

            if (isKey)
            {
                float keyPadY = lineH * HELP_KEY_PAD_Y;
                auto keyRect = juce::Rectangle<float>(x, y + keyPadY, drawW, lineH - keyPadY * 2.0f);
                g.setColour(juce::Colour(Theme::toolbarBg));
                g.fillRoundedRectangle(keyRect, keyRect.getHeight() * 0.3f);
            }

            g.setColour(isKey ? keyColour : dimColour);
            g.drawText(segs[(size_t)i].text, juce::Rectangle<float>(x, y, drawW, lineH),
                       juce::Justification::centred, false);
            x += drawW;
        }
    }

    UpdateBannerComponent& banner;
    StatusBarComponent statusBar;
    juce::Label versionLabel;
    juce::Drawable* dawIcon = nullptr;
    HelpText helpDefault;
    HelpText helpOverride;
    juce::Rectangle<int> helpArea;
    float helpFontSize = 12.0f;
    juce::Colour modeAccent { Theme::coral };
};
