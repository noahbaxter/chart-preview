#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../HelpTypes.h"
#include "PillToggle.h"
#include "SegmentedButtons.h"
#include "ValueStepper.h"
#include "../../Editor/InteractionController.h"

class WriteSubToolbar : public juce::Component
{
public:
    explicit WriteSubToolbar(InteractionController& ic) : interactionController(ic)
    {
        modeButtons.setItems({"Draw", "Edit"});
        addAndMakeVisible(modeButtons);

        barToggle.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(barToggle);

        snapToggle.setToggleState(ic.snapEnabled(), juce::dontSendNotification);
        snapToggle.setIcon(juce::Drawable::createFromImageData(
            BinaryData::icon_snap_magnet_svg, BinaryData::icon_snap_magnet_svgSize));
        addAndMakeVisible(snapToggle);

        divisionStepper.setLabelRatio(0.0f);
        addAndMakeVisible(divisionStepper);

        tupletStepper.setLabelRatio(0.0f);
        addAndMakeVisible(tupletStepper);

        guitarForceButtons.setItems({"Auto", "HOPO", "Strum", "Tap"});
        addChildComponent(guitarForceButtons);

        drumDynamicButtons.setItems({"Normal", "Ghost", "Accent"});
        addChildComponent(drumDynamicButtons);

        cymbalToggle.setToggleState(false, juce::dontSendNotification);
        addChildComponent(cymbalToggle);

        for (auto* seg : { &modeButtons, &guitarForceButtons, &drumDynamicButtons })
            seg->setCornerRadius(3.0f);
        for (auto* pill : { &snapToggle, &barToggle, &cymbalToggle })
            pill->setCornerRadius(3.0f);
        divisionStepper.setCornerRadius(3.0f);
        tupletStepper.setCornerRadius(3.0f);

        addHoverZone(modeButtons,        {"Q"},      "draw places notes, edit selects & modifies");
        barHelpZone = addHoverZone(barToggle, {"B"}, "only interact with open notes");
        addHoverZone(snapToggle,         {"S"},      "lock note placement to grid");
        addHoverZone(divisionStepper,    {"[", "]"}, "grid spacing (1/4, 1/8, 1/16...)");
        addHoverZone(tupletStepper,      {"T"},      "tuplet subdivision (3, 5, 7)");
        addHoverZone(guitarForceButtons, {},         "force HOPO, strum, or tap (overrides auto)");
        addHoverZone(drumDynamicButtons, {},         "ghost (soft) or accent (loud) hit");
        addHoverZone(cymbalToggle,       {},         "cymbal placement (PRO drums)");
    }

    std::function<void(SubMode)> onSubModeChanged;
    std::function<void(bool)> onSnapChanged;
    std::function<void(int delta)> onStepDivisionStep;
    std::function<void(int delta)> onTupletStep;
    std::function<void(bool)> onBarModeChanged;
    std::function<void(GuitarForce)> onGuitarForceChanged;
    std::function<void(DrumDynamic)> onDrumDynamicChanged;
    std::function<void(bool)> onCymbalModeChanged;

    std::function<void(const HelpText&)> onHoverHelp;
    std::function<void()> onHoverHelpClear;

    void wireCallbacks()
    {
        modeButtons.onSelectionChanged = [this](int idx) {
            if (onSubModeChanged)
                onSubModeChanged(idx == 0 ? SubMode::Draw : SubMode::Edit);
        };

        snapToggle.onClick = [this]() {
            if (onSnapChanged) onSnapChanged(snapToggle.getToggleState());
        };

        divisionStepper.onStep = [this](int delta) {
            if (onStepDivisionStep) onStepDivisionStep(delta);
        };

        tupletStepper.onStep = [this](int delta) {
            if (onTupletStep) onTupletStep(delta);
        };

        barToggle.onClick = [this]() {
            if (onBarModeChanged) onBarModeChanged(barToggle.getToggleState());
        };

        guitarForceButtons.onSelectionChanged = [this](int idx) {
            static const GuitarForce forces[] = { GuitarForce::None, GuitarForce::Hopo,
                                                   GuitarForce::Strum, GuitarForce::Tap };
            if (onGuitarForceChanged) onGuitarForceChanged(forces[idx]);
        };

        drumDynamicButtons.onSelectionChanged = [this](int idx) {
            static const DrumDynamic dynamics[] = { DrumDynamic::Normal, DrumDynamic::Ghost,
                                                     DrumDynamic::Accent };
            if (onDrumDynamicChanged) onDrumDynamicChanged(dynamics[idx]);
        };

        cymbalToggle.onClick = [this]() {
            if (onCymbalModeChanged) onCymbalModeChanged(cymbalToggle.getToggleState());
        };

        for (auto& hz : hoverZones)
        {
            hz->onHelp = &onHoverHelp;
            hz->onClear = &onHoverHelpClear;
        }
    }

    void refreshFromController()
    {
        bool drums = isDrumLike(interactionController.activePart());
        bool drawMode = interactionController.subMode() == SubMode::Draw;
        bool editMode = !drawMode;
        bool hasSel = editMode && interactionController.hasEditSelection();
        juce::Colour modeAccent = drawMode ? juce::Colour(Theme::purple) : juce::Colour(Theme::blue);

        if (barHelpZone)
            barHelpZone->setDescription({"B"}, drums ? "only interact with kick notes"
                                                     : "only interact with open notes");

        modeButtons.setAccentColour(modeAccent);
        snapToggle.setAccentColour(modeAccent);
        divisionStepper.setAccentColour(modeAccent);
        tupletStepper.setAccentColour(modeAccent);
        barToggle.setAccentColour(modeAccent);
        guitarForceButtons.setAccentColour(modeAccent);
        drumDynamicButtons.setAccentColour(modeAccent);
        cymbalToggle.setAccentColour(modeAccent);

        int modeIdx = drawMode ? 0 : 1;
        modeButtons.setSelectedIndex(modeIdx);

        bool snapOn = interactionController.snapEnabled();
        snapToggle.setToggleState(snapOn);

        int div = interactionController.stepDivision();
        divisionStepper.setDisplayValue("1/" + juce::String(div));
        divisionStepper.setAtMin(div <= 1);
        divisionStepper.setAtMax(div >= 64);
        divisionStepper.setAlpha(snapOn ? 1.0f : 0.35f);
        divisionStepper.setInterceptsMouseClicks(snapOn, snapOn);

        int tuplet = interactionController.tuplet();
        tupletStepper.setDisplayValue(tuplet == 0 ? "Off" : juce::String(tuplet));
        tupletStepper.setAtMin(tuplet == 0);
        tupletStepper.setAtMax(tuplet == 7);
        tupletStepper.setAlpha(snapOn ? 1.0f : 0.35f);
        tupletStepper.setInterceptsMouseClicks(snapOn, snapOn);

        barToggle.setToggleState(interactionController.barMode());
        barToggle.setButtonText(drums ? "KICK" : "OPEN");

        guitarForceButtons.setVisible(!drums);
        drumDynamicButtons.setVisible(drums);
        cymbalToggle.setVisible(drums);

        bool modifiersEnabled = drawMode || hasSel;

        if (drums)
        {
            drumDynamicButtons.setAlpha(modifiersEnabled ? 1.0f : 0.35f);
            drumDynamicButtons.setInterceptsMouseClicks(modifiersEnabled, modifiersEnabled);
            cymbalToggle.setDisabled(!modifiersEnabled);

            if (modifiersEnabled)
            {
                auto dyn = interactionController.drumDynamic();
                int dynIdx = 0;
                if (dyn == DrumDynamic::Ghost) dynIdx = 1;
                else if (dyn == DrumDynamic::Accent) dynIdx = 2;
                drumDynamicButtons.setSelectedIndex(dynIdx);
                cymbalToggle.setToggleState(interactionController.cymbalMode());
            }
        }
        else
        {
            guitarForceButtons.setAlpha(modifiersEnabled ? 1.0f : 0.35f);
            guitarForceButtons.setInterceptsMouseClicks(modifiersEnabled, modifiersEnabled);

            if (modifiersEnabled)
            {
                auto force = interactionController.guitarForce();
                int forceIdx = 0;
                if (force == GuitarForce::Hopo) forceIdx = 1;
                else if (force == GuitarForce::Strum) forceIdx = 2;
                else if (force == GuitarForce::Tap) forceIdx = 3;
                guitarForceButtons.setSelectedIndex(forceIdx);
            }
        }

        resized();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const bool drawMode = interactionController.subMode() == SubMode::Draw;
        const juce::Colour modeColour = drawMode
            ? juce::Colour(Theme::purple)
            : juce::Colour(Theme::blue);

        g.setColour(juce::Colour(Theme::darkBgLighter).withAlpha(0.85f));
        g.fillRect(getLocalBounds());
        g.setColour(modeColour.withAlpha(0.12f));
        g.fillRect(getLocalBounds());

        g.setColour(modeColour.withAlpha(0.55f));
        g.drawHorizontalLine(0, 0.0f, (float)getWidth());

        if (SUBTOOLBAR_SHOW_LABELS && labelH > 0)
        {
            float fs = juce::jmax(7.0f, (float)labelH * 0.55f);
            juce::Font labelFont = Theme::getUIFont(fs)
                .withHorizontalScale(1.25f)
                .withExtraKerningFactor(0.12f);
            g.setFont(labelFont);
            g.setColour(juce::Colours::white.withAlpha(0.25f));

            bool drums = isDrumLike(interactionController.activePart());
            auto draw = [&](const char* text, juce::Rectangle<int> r) {
                g.drawText(text, r.getX(), 0, r.getWidth(), labelH,
                           juce::Justification::centred, false);
            };
            draw("MODE",     labelGroupLeft);
            draw("GRID",     labelGroupCenter);
            draw("NOTE TYPE", labelGroupRight);
        }
    }

    void resized() override
    {
        int h = getHeight();
        if (h <= 0) return;

        bool drums = isDrumLike(interactionController.activePart());

        labelH = SUBTOOLBAR_SHOW_LABELS ? juce::jmax(10, juce::roundToInt(h * 0.3f)) : 0;
        int controlArea = h - labelH;
        int padTop = juce::roundToInt(controlArea * SUBTOOLBAR_PAD_TOP);
        int padBot = juce::roundToInt(controlArea * SUBTOOLBAR_PAD_BOTTOM);
        int available = controlArea - padTop - padBot;
        int rowH = juce::jmax(8, juce::roundToInt(available * SUBTOOLBAR_HEIGHT_SCALE));
        int controlY = labelH + padTop + (available - rowH) / 2;

        float u = (float)juce::jmax(8, juce::roundToInt(controlArea * SUBTOOLBAR_HEIGHT_SCALE)) * SUBTOOLBAR_WIDTH_SCALE;
        int gap = juce::jmax(2, (int)(u * 0.15f));
        int colGap = (int)(u * SUBTOOLBAR_COL_GAP);

        int modeW = (int)(u * SUBTOOLBAR_MODE_W);
        int barW  = (int)(u * SUBTOOLBAR_BAR_W);
        int snapW = (int)(u * SUBTOOLBAR_SNAP_W);
        int stepW = (int)(u * SUBTOOLBAR_STEP_W);
        int modW  = (int)(u * SUBTOOLBAR_MOD_W);

        int intraGap = juce::jmax(3, (int)(u * 0.25f));
        int leftGrpW   = modeW + intraGap + barW;
        int centerGrpW = snapW + gap + stepW + gap + stepW;
        int rightGrpW  = modW;
        int totalW = leftGrpW + colGap + centerGrpW + colGap + rightGrpW;
        int startX = (getWidth() - totalW) / 2;

        int leftX   = startX;
        int centerX = leftX + leftGrpW + colGap;
        int rightX  = centerX + centerGrpW + colGap;

        modeButtons.setBounds(leftX, controlY, modeW, rowH);
        barToggle.setBounds(leftX + modeW + intraGap, controlY, barW, rowH);

        snapToggle.setBounds(centerX, controlY, snapW, rowH);
        divisionStepper.setBounds(centerX + snapW + gap, controlY, stepW, rowH);
        tupletStepper.setBounds(centerX + snapW + gap + stepW + gap, controlY, stepW, rowH);

        if (drums)
        {
            int cymW = (int)(u * SUBTOOLBAR_CYM_W);
            int dynW = modW - cymW - intraGap;
            drumDynamicButtons.setBounds(rightX, controlY, dynW, rowH);
            cymbalToggle.setBounds(rightX + dynW + intraGap, controlY, cymW, rowH);
        }
        else
        {
            guitarForceButtons.setBounds(rightX, controlY, modW, rowH);
        }

        labelGroupLeft   = { leftX,   0, leftGrpW,   labelH };
        labelGroupCenter = { centerX, 0, centerGrpW, labelH };
        labelGroupRight  = { rightX,  0, rightGrpW,  labelH };
    }

private:
    struct HoverHelpZone : public juce::MouseListener
    {
        juce::Component& target;
        HelpText helpText;
        std::function<void(const HelpText&)>* onHelp = nullptr;
        std::function<void()>* onClear = nullptr;

        HoverHelpZone(juce::Component& t, std::initializer_list<juce::String> keys, const juce::String& desc)
            : target(t)
        {
            for (const auto& k : keys)
                helpText.push_back(key(k));
            helpText.push_back(dim(desc));
            target.addMouseListener(this, false);
        }

        ~HoverHelpZone() override { target.removeMouseListener(this); }

        void setDescription(std::initializer_list<juce::String> newKeys, const juce::String& desc)
        {
            helpText.clear();
            for (const auto& k : newKeys)
                helpText.push_back(key(k));
            helpText.push_back(dim(desc));
        }

        void mouseEnter(const juce::MouseEvent&) override
        {
            if (onHelp && *onHelp) (*onHelp)(helpText);
        }

        void mouseExit(const juce::MouseEvent&) override
        {
            if (onClear && *onClear) (*onClear)();
        }
    };

    HoverHelpZone* addHoverZone(juce::Component& target, std::initializer_list<juce::String> keys, const juce::String& desc)
    {
        auto zone = std::make_unique<HoverHelpZone>(target, keys, desc);
        auto* ptr = zone.get();
        hoverZones.push_back(std::move(zone));
        return ptr;
    }

    InteractionController& interactionController;

    SegmentedButtons modeButtons;
    PillToggle       barToggle      {"BAR"};

    PillToggle       snapToggle     {"Snap"};
    ValueStepper     divisionStepper{"", ""};
    ValueStepper     tupletStepper  {"", ""};

    SegmentedButtons guitarForceButtons;
    SegmentedButtons drumDynamicButtons;
    PillToggle       cymbalToggle   {"Cym"};

    std::vector<std::unique_ptr<HoverHelpZone>> hoverZones;
    HoverHelpZone* barHelpZone = nullptr;

    int labelH = 0;
    juce::Rectangle<int> labelGroupLeft, labelGroupCenter, labelGroupRight;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WriteSubToolbar)
};
