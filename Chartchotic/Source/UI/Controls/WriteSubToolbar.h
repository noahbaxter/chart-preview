#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
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

        snapToggle.setToggleState(ic.snapEnabled(), juce::dontSendNotification);
        addAndMakeVisible(snapToggle);

        divisionStepper.setLabelRatio(0.0f);
        addAndMakeVisible(divisionStepper);

        tupletButtons.setItems({"Off", "3", "5", "7"});
        addAndMakeVisible(tupletButtons);

        barToggle.setToggleState(false, juce::dontSendNotification);
        addAndMakeVisible(barToggle);

        guitarForceButtons.setItems({"None", "HOPO", "Strum", "Tap"});
        addChildComponent(guitarForceButtons);

        drumDynamicButtons.setItems({"Normal", "Ghost", "Accent"});
        addChildComponent(drumDynamicButtons);

        cymbalToggle.setToggleState(false, juce::dontSendNotification);
        addChildComponent(cymbalToggle);
    }

    std::function<void(SubMode)> onSubModeChanged;
    std::function<void(bool)> onSnapChanged;
    std::function<void(int delta)> onStepDivisionStep;
    std::function<void(int index)> onTupletChanged;
    std::function<void(bool)> onBarModeChanged;
    std::function<void(GuitarForce)> onGuitarForceChanged;
    std::function<void(DrumDynamic)> onDrumDynamicChanged;
    std::function<void(bool)> onCymbalModeChanged;

    std::function<void(const juce::String&)> onHoverHelp;
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

        tupletButtons.onSelectionChanged = [this](int idx) {
            if (onTupletChanged) onTupletChanged(idx);
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
    }

    void refreshFromController()
    {
        bool drums = isDrumLike(interactionController.activePart());

        int modeIdx = interactionController.subMode() == SubMode::Draw ? 0 : 1;
        modeButtons.setSelectedIndex(modeIdx);

        snapToggle.setToggleState(interactionController.snapEnabled());

        int div = interactionController.stepDivision();
        divisionStepper.setDisplayValue("1/" + juce::String(div));

        int tuplet = interactionController.tuplet();
        int tupIdx = 0;
        if (tuplet == 3) tupIdx = 1;
        else if (tuplet == 5) tupIdx = 2;
        else if (tuplet == 7) tupIdx = 3;
        tupletButtons.setSelectedIndex(tupIdx);

        barToggle.setToggleState(interactionController.barMode());

        guitarForceButtons.setVisible(!drums);
        drumDynamicButtons.setVisible(drums);
        cymbalToggle.setVisible(drums);

        if (drums)
        {
            auto dyn = interactionController.drumDynamic();
            int dynIdx = 0;
            if (dyn == DrumDynamic::Ghost) dynIdx = 1;
            else if (dyn == DrumDynamic::Accent) dynIdx = 2;
            drumDynamicButtons.setSelectedIndex(dynIdx);
            cymbalToggle.setToggleState(interactionController.cymbalMode());
        }
        else
        {
            auto force = interactionController.guitarForce();
            int forceIdx = 0;
            if (force == GuitarForce::Hopo) forceIdx = 1;
            else if (force == GuitarForce::Strum) forceIdx = 2;
            else if (force == GuitarForce::Tap) forceIdx = 3;
            guitarForceButtons.setSelectedIndex(forceIdx);
        }

        resized();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const bool drawMode = interactionController.subMode() == SubMode::Draw;
        const juce::Colour modeColour = drawMode
            ? juce::Colour(Theme::green)
            : juce::Colour(Theme::coral);

        g.setColour(juce::Colour(Theme::darkBgLighter).withAlpha(0.85f));
        g.fillRect(getLocalBounds());
        g.setColour(modeColour.withAlpha(0.18f));
        g.fillRect(getLocalBounds());

        g.setColour(modeColour.withAlpha(0.55f));
        g.drawHorizontalLine(0, 0.0f, (float)getWidth());
    }

    void resized() override
    {
        int h = getHeight();
        if (h <= 0) return;

        int margin = juce::jmax(8, h / 2);
        int gap = juce::jmax(4, h / 6);
        int controlH = juce::jmax(1, h - juce::jmax(4, h / 5));
        int controlY = (h - controlH) / 2;

        bool drums = isDrumLike(interactionController.activePart());

        int modeW = juce::jmax(80, h * 3);
        int snapW = juce::jmax(40, h * 2);
        int divW = juce::jmax(55, h * 2);
        int tupW = juce::jmax(100, h * 4);
        int barW = juce::jmax(40, h * 2);

        int modW = 0;
        if (drums)
            modW = juce::jmax(130, h * 5) + gap + juce::jmax(40, h * 2);
        else
            modW = juce::jmax(160, h * 6);

        int totalW = modeW + gap + snapW + gap + divW + gap + tupW + gap + barW + gap * 3 + modW;
        int startX = juce::jmax(margin, (getWidth() - totalW) / 2);
        int x = startX;

        modeButtons.setBounds(x, controlY, modeW, controlH);
        x += modeW + gap;

        snapToggle.setBounds(x, controlY, snapW, controlH);
        x += snapW + gap;

        divisionStepper.setBounds(x, controlY, divW, controlH);
        x += divW + gap;

        tupletButtons.setBounds(x, controlY, tupW, controlH);
        x += tupW + gap;

        barToggle.setBounds(x, controlY, barW, controlH);
        x += barW + gap * 3;

        if (drums)
        {
            int dynW = juce::jmax(130, h * 5);
            int cymW = juce::jmax(40, h * 2);
            drumDynamicButtons.setBounds(x, controlY, dynW, controlH);
            x += dynW + gap;
            cymbalToggle.setBounds(x, controlY, cymW, controlH);
        }
        else
        {
            guitarForceButtons.setBounds(x, controlY, modW, controlH);
        }
    }

private:
    InteractionController& interactionController;

    SegmentedButtons modeButtons;
    PillToggle       snapToggle     {"Snap"};
    ValueStepper     divisionStepper{"", ""};
    SegmentedButtons tupletButtons;
    PillToggle       barToggle      {"BAR"};

    SegmentedButtons guitarForceButtons;

    SegmentedButtons drumDynamicButtons;
    PillToggle       cymbalToggle   {"Cym"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WriteSubToolbar)
};
