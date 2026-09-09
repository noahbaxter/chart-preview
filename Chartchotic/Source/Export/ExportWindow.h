#pragma once

#include <JuceHeader.h>
#include <memory>

#include "ExportDialogComponent.h"

/**
    A real window for the export dialog.

    It started as an overlay inside the plugin editor, which was wrong: the
    editor is whatever size the host gives it, often docked and narrow, and the
    export UI is a song list plus two columns of fields that simply does not
    fit inside it. An overlay can only ever scale down until it is unusable.

    A window of its own is also the honest shape for the job: you are setting
    up an album, not adjusting the highway, and it is worth being able to
    resize it and put it where you like.

    Kept above the plugin editor, because REAPER's FX window floats and a
    window that opens behind it looks like nothing happened.
*/
class ExportWindow : public juce::DocumentWindow
{
public:
    ExportWindow(ExportDialogComponent::Context context, std::function<void()> onClosed)
        : juce::DocumentWindow("Export Chart",
                               juce::Colour(Theme::darkBg),
                               juce::DocumentWindow::closeButton),
          closed(std::move(onClosed))
    {
        auto* content = new ExportDialogComponent(std::move(context));
        dialog = content;

        setUsingNativeTitleBar(true);
        setContentOwned(content, true);
        setResizable(true, false);
        setResizeLimits(ExportDialogComponent::kMinimumWidth,
                        ExportDialogComponent::kMinimumHeight, 2400, 1600);
        setAlwaysOnTop(true);
        centreWithSize(juce::jmax(ExportDialogComponent::kMinimumWidth, 1020),
                       juce::jmax(ExportDialogComponent::kMinimumHeight, 700));
        setVisible(true);
    }

    /** The dialog inside, for wiring up its callbacks. */
    ExportDialogComponent& content() { return *dialog; }

    // The title bar's close button, which has to behave exactly like Cancel.
    void closeButtonPressed() override { if (closed) closed(); }

private:
    ExportDialogComponent* dialog = nullptr;
    std::function<void()> closed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportWindow)
};
