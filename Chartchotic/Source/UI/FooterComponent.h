#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "StatusBarComponent.h"
#include "UpdateBannerComponent.h"

class FooterComponent : public juce::Component
{
public:
    static constexpr float footerRatio = 0.04f;
    static constexpr int maxFooterHeight = 40;
    static constexpr float paddingRatio = 0.2f;
    static constexpr float badgeScale = 0.65f;
    static constexpr int badgeGap = 4;
    static constexpr int labelPad = 12;
    FooterComponent(UpdateBannerComponent& b) : banner(b)
    {
        addAndMakeVisible(versionGroup);
        versionGroup.addAndMakeVisible(banner);
        versionGroup.addAndMakeVisible(label);
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(statusBar);
        helpLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.45f));
        helpLabel.setJustificationType(juce::Justification::centred);
        helpLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(helpLabel);
    }

    void init(const juce::String& versionText)
    {
        label.setText(versionText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        normalColour = juce::Colours::white.withAlpha(0.6f);
        hoverColour = juce::Colour(Theme::coral);
        label.setColour(juce::Label::textColourId, normalColour);
    }

    void setUpdateAvailable()
    {
        normalColour = juce::Colours::white.withAlpha(0.8f);
        label.setColour(juce::Label::textColourId, normalColour);
        resized(); // re-layout to show badge and resize group
    }

    void setDawIcon(juce::Drawable* icon) { dawIcon = icon; resized(); repaint(); }

    StatusBarComponent& getStatusBar() { return statusBar; }

    void resized() override
    {
        auto area = getLocalBounds();
        int pad = juce::roundToInt(area.getHeight() * paddingRatio);

        // Version group: sized to fit content, left-aligned
        int h = area.getHeight();
        int bs = juce::roundToInt(h * badgeScale);
        int badgeW = banner.hasUpdate() ? bs + badgeGap : 0;
        int labelW = (int)label.getFont().getStringWidthFloat(label.getText()) + labelPad;
        int groupW = badgeW + labelW;
        versionGroup.setBounds(pad, 0, groupW, h);

        // Status bar: right-aligned, leaving room for DAW icon
        int iconRoom = dawIcon ? h + pad : 0;
        int statusW = juce::jmax(0, area.getWidth() / 4);
        int helpX = groupW + pad;
        int helpW = area.getWidth() - helpX - statusW - iconRoom - pad;
        if (helpW > 0)
            helpLabel.setBounds(helpX, 0, helpW, h);
        helpLabel.setFont(Theme::getUIFont(Theme::fontSize));

        if (statusW > 0)
            statusBar.setBounds(area.getWidth() - statusW - iconRoom - pad, 0, statusW, h);
    }

    void paint(juce::Graphics& g) override
    {
        if (dawIcon)
        {
            int pad = juce::roundToInt(getHeight() * paddingRatio);
            int iconSize = getHeight() - pad * 2;
            juce::Rectangle<float> iconBounds(
                (float)(getWidth() - iconSize - pad), (float)((getHeight() - iconSize) / 2),
                (float)iconSize, (float)iconSize);
            dawIcon->drawWithin(g, iconBounds, juce::RectanglePlacement::centred, 0.8f);
        }
    }

    juce::Label label;
    juce::Label helpLabel;
    juce::Colour normalColour, hoverColour;

    void setHelpText(const juce::String& text)
    {
        helpOverride = text;
        helpLabel.setText(text, juce::dontSendNotification);
    }

    void clearHelpText()
    {
        helpOverride = {};
        helpLabel.setText(helpDefault, juce::dontSendNotification);
    }

    void setDefaultHelpText(const juce::String& text)
    {
        helpDefault = text;
        if (helpOverride.isEmpty())
            helpLabel.setText(helpDefault, juce::dontSendNotification);
    }

private:
    UpdateBannerComponent& banner;
    StatusBarComponent statusBar;
    juce::Drawable* dawIcon = nullptr;
    juce::String helpDefault;
    juce::String helpOverride;

    // Groups badge + label with unified hover/click for update prompt
    struct VersionGroup : public juce::Component
    {
        FooterComponent& owner;
        VersionGroup(FooterComponent& o) : owner(o) {}

        void resized() override
        {
            auto area = getLocalBounds();
            int x = 0;
            if (owner.banner.hasUpdate())
            {
                int bs = juce::roundToInt(area.getHeight() * badgeScale);
                int badgeY = (area.getHeight() - bs) / 2;
                owner.banner.setBounds(0, badgeY, bs, bs);
                owner.banner.setVisible(true);
                x = bs + badgeGap;
            }
            else
            {
                owner.banner.setVisible(false);
            }
            owner.label.setBounds(x, 0, area.getWidth() - x, area.getHeight());
        }

        void mouseEnter(const juce::MouseEvent&) override
        {
            if (owner.banner.hasUpdate())
            {
                owner.label.setColour(juce::Label::textColourId, owner.hoverColour);
                owner.label.repaint();
                owner.banner.setBadgeHovered(true);
                setMouseCursor(juce::MouseCursor::PointingHandCursor);
            }
        }
        void mouseExit(const juce::MouseEvent&) override
        {
            owner.label.setColour(juce::Label::textColourId, owner.normalColour);
            owner.label.repaint();
            owner.banner.setBadgeHovered(false);
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }
        void mouseDown(const juce::MouseEvent&) override
        {
            if (owner.banner.hasUpdate())
                owner.banner.showPrompt();
        }
    };
    VersionGroup versionGroup { *this };
};
