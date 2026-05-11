#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// Labeled value selector: Label  [< value >]
// Supports mouse wheel and click on arrows to step through values.
// The label portion takes ~42% width, value area takes the rest.
// Double-click the value to type an exact number (when onValueEdited is set).
class ValueStepper : public juce::Component
{
public:
    ValueStepper(const juce::String& labelText, const juce::String& unitSuffix = {})
        : name(labelText), unit(unitSuffix) {}

    void setDisplayValue(const juce::String& val) { displayValue = val; repaint(); }
    void setDisplayValue(int val) { setDisplayValue(juce::String(val) + unit); }
    void setDisplayValue(float val) { setDisplayValue(juce::String(val) + unit); }
    const juce::String& getDisplayValue() const { return displayValue; }

    std::function<void(int delta)> onStep;
    std::function<void()> onFolderClick;

    // Set this to enable double-click-to-type. Called with the raw text the user typed.
    // The parent is responsible for parsing, clamping, and calling setDisplayValue().
    std::function<void(const juce::String&)> onValueEdited;

    void setFolderIconVisible(bool show) { folderIconVisible = show; repaint(); }
    void setValueClickable(bool clickable) { valueClickOpensFolder = clickable; repaint(); }
    void setAccentColour(juce::Colour c) { accent = c; repaint(); }
    void setCornerRadius(float r) { cornerRadius = r; repaint(); }
    void setAtMin(bool v) { atMin = v; repaint(); }
    void setAtMax(bool v) { atMax = v; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        float h = (float)getHeight();

        int folderW = folderIconVisible ? (int)(h * 0.85f) : 0;
        int nameW = (int)(bounds.getWidth() * labelRatio);

        bool hovering = isMouseOver() && !folderHoverZone;

        // Label
        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::controlFont);
        if (labelOnRight)
            g.drawText(name, bounds.getWidth() - nameW, 0, nameW, getHeight(), juce::Justification::centredRight);
        else
            g.drawText(name, 0, 0, nameW - folderW, getHeight(), juce::Justification::centredLeft);

        // Folder icon — at right edge of label area, just left of pill
        if (folderIconVisible)
        {
            bool folderHover = isMouseOver() && folderHoverZone;
            int iconX = labelOnRight ? (bounds.getWidth() - nameW - folderW) : (nameW - folderW);
            auto iconArea = juce::Rectangle<float>((float)iconX, 0.0f, (float)folderW, h).reduced(h * 0.2f);
            g.setColour(accent.withAlpha(folderHover ? 1.0f : 0.45f));

            float ix = iconArea.getX(), iy = iconArea.getY();
            float iw = iconArea.getWidth(), ih = iconArea.getHeight();
            float tabW = iw * 0.4f, tabH = ih * 0.2f;
            juce::Path folder;
            folder.startNewSubPath(ix, iy + tabH);
            folder.lineTo(ix, iy + ih);
            folder.lineTo(ix + iw, iy + ih);
            folder.lineTo(ix + iw, iy + tabH);
            folder.lineTo(ix + tabW + tabH, iy + tabH);
            folder.lineTo(ix + tabW, iy);
            folder.lineTo(ix, iy);
            folder.closeSubPath();
            g.strokePath(folder, juce::PathStrokeType(1.2f));
        }

        // Value pill — same position as non-folder steppers
        auto valueRect = (labelOnRight ? bounds.withRight(bounds.getWidth() - nameW)
                                       : bounds.withLeft(nameW)).toFloat().reduced(1.0f);

        float cr = (cornerRadius >= 0.0f) ? cornerRadius : Theme::pillCorner;

        g.setColour(juce::Colour(Theme::darkBg));
        g.fillRoundedRectangle(valueRect, cr);

        g.setColour(hovering ? accent.withAlpha(0.5f)
                             : juce::Colour(Theme::textDim).withAlpha(0.3f));
        g.drawRoundedRectangle(valueRect, cr, 1.0f);

        // Don't draw arrows/value text while editing
        if (editor != nullptr)
            return;

        // Arrows
        g.setFont(juce::Font(Theme::arrowFontSize));

        auto leftArrow = valueRect.withWidth(Theme::arrowZone);
        auto rightArrow = valueRect.withLeft(valueRect.getRight() - Theme::arrowZone);
        g.setColour(atMin ? juce::Colour(Theme::textDim).withAlpha(0.2f) : accent.withAlpha(hovering ? 1.0f : 0.7f));
        g.drawText(juce::CharPointer_UTF8("\xe2\x97\x80"), leftArrow, juce::Justification::centred);
        g.setColour(atMax ? juce::Colour(Theme::textDim).withAlpha(0.2f) : accent.withAlpha(hovering ? 1.0f : 0.7f));
        g.drawText(juce::CharPointer_UTF8("\xe2\x96\xb6"), rightArrow, juce::Justification::centred);

        // Value text
        auto textRect = valueRect.withLeft(valueRect.getX() + Theme::arrowZone)
                                  .withRight(valueRect.getRight() - Theme::arrowZone);
        g.setColour(valueClickOpensFolder ? accent.withAlpha(0.7f)
                                          : juce::Colour(Theme::textWhite));
        g.setFont(Theme::controlFont);
        g.drawText(displayValue, textRect, juce::Justification::centred);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (editor != nullptr) return;
        if (!getLocalBounds().contains(e.getPosition())) return;
        float h = (float)getHeight();
        int folderW = folderIconVisible ? (int)(h * 0.85f) : 0;
        int nameW = (int)(getWidth() * labelRatio);
        int folderLeft = labelOnRight ? (getWidth() - nameW - folderW) : (nameW - folderW);

        // Folder icon click
        if (folderIconVisible && e.x >= folderLeft && e.x < folderLeft + folderW)
        {
            if (onFolderClick) onFolderClick();
            return;
        }

        int pillLeft = labelOnRight ? 0 : nameW;
        int pillRight = labelOnRight ? (getWidth() - nameW) : getWidth();

        if (e.x >= pillLeft && e.x < pillLeft + Theme::arrowZone)
        {
            if (onStep) onStep(-1);
        }
        else if (e.x > pillRight - Theme::arrowZone && e.x <= pillRight)
        {
            if (onStep) onStep(1);
        }
        else if (valueClickOpensFolder && onFolderClick)
        {
            onFolderClick();
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (!onValueEdited) return;
        if (editor != nullptr) return;

        int nameW = (int)(getWidth() * labelRatio);
        int pillLeft = labelOnRight ? 0 : nameW;
        int pillRight = labelOnRight ? (getWidth() - nameW) : getWidth();

        // Only trigger on the value area (between arrows)
        if (e.x < pillLeft + Theme::arrowZone || e.x > pillRight - Theme::arrowZone)
            return;

        showEditor();
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (editor != nullptr) return;
        int delta = (wheel.deltaY > 0) ? 1 : -1;
        if (onStep) onStep(delta);
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        if (folderIconVisible)
        {
            float h = (float)getHeight();
            int folderW = (int)(h * 0.85f);
            int nameW = (int)(getWidth() * labelRatio);
            int folderLeft = labelOnRight ? (getWidth() - nameW - folderW) : (nameW - folderW);
            bool inFolder = e.x >= folderLeft && e.x < folderLeft + folderW;
            if (inFolder != folderHoverZone)
            {
                folderHoverZone = inFolder;
                repaint();
            }
        }
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        repaint();
        if (tooltip.isNotEmpty())
            showTooltip();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        folderHoverZone = false;
        repaint();
        hideTooltip();
    }

    void setLabelRatio(float ratio) { labelRatio = ratio; }
    void setLabelOnRight(bool right) { labelOnRight = right; }
    void setTooltip(const juce::String& text) { tooltip = text; }

private:
    juce::String name;
    juce::String displayValue;
    juce::String unit;
    juce::String tooltip;
    float labelRatio = 0.42f;
    bool labelOnRight = false;
    bool folderIconVisible = false;
    bool folderHoverZone = false;
    bool valueClickOpensFolder = false;
    juce::Colour accent { Theme::coral };
    float cornerRadius = -1.0f;
    bool atMin = false;
    bool atMax = false;

    // TextEditor subclass that uses Desktop global mouse listener to detect
    // clicks outside and give away focus. This is the JUCE-recommended pattern —
    // giveAwayKeyboardFocus() triggers onFocusLost without modifying any listener
    // list during iteration, avoiding the crash that happens with component-level
    // mouse listeners that destroy themselves in their own callback.
    struct ClickDismissTextEditor : public juce::TextEditor
    {
        void focusGained(FocusChangeType cause) override
        {
            juce::Desktop::getInstance().addGlobalMouseListener(this);
            juce::TextEditor::focusGained(cause);
        }

        void focusLost(FocusChangeType cause) override
        {
            juce::Desktop::getInstance().removeGlobalMouseListener(this);
            juce::TextEditor::focusLost(cause);
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (getScreenBounds().contains(e.getScreenPosition()))
                juce::TextEditor::mouseDown(e);
            else
                giveAwayKeyboardFocus();
        }
    };

    std::unique_ptr<ClickDismissTextEditor> editor;

    void showEditor()
    {
        auto bounds = getLocalBounds();
        int nameW = (int)(bounds.getWidth() * labelRatio);
        auto valueRect = (labelOnRight ? bounds.withRight(bounds.getWidth() - nameW)
                                       : bounds.withLeft(nameW)).reduced(1);

        editor = std::make_unique<ClickDismissTextEditor>();
        editor->setBounds(valueRect);
        editor->setFont(Theme::controlFont);
        editor->setJustification(juce::Justification::centred);
        editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(Theme::darkBg));
        editor->setColour(juce::TextEditor::textColourId, juce::Colour(Theme::textWhite));
        editor->setColour(juce::TextEditor::outlineColourId, juce::Colour(Theme::coral));
        editor->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(Theme::coral));
        editor->setColour(juce::CaretComponent::caretColourId, juce::Colour(Theme::textWhite));

        // Strip unit suffix for easier editing (e.g. "100%" -> "100", "-50 ms" -> "-50")
        juce::String editText = displayValue.trimEnd();
        if (unit.isNotEmpty() && editText.endsWith(unit.trimEnd()))
            editText = editText.dropLastCharacters(unit.trimEnd().length()).trim();
        editor->setText(editText, false);
        editor->selectAll();

        editor->onReturnKey = [this]() { commitEdit(); };
        editor->onEscapeKey = [this]() { dismissEdit(); };
        editor->onFocusLost = [this]() { commitEdit(); };

        addAndMakeVisible(editor.get());
        editor->grabKeyboardFocus();
        repaint();
    }

    void commitEdit()
    {
        if (!editor) return;
        auto text = editor->getText().trim();
        // Hide immediately but defer destruction — destroying during JUCE's
        // mouse/focus dispatch causes use-after-free in mouseEnter routing.
        editor->setVisible(false);
        auto editorToDelete = std::move(editor);
        juce::MessageManager::callAsync([e = std::move(editorToDelete)]() mutable { e.reset(); });
        if (onValueEdited && text.isNotEmpty())
            onValueEdited(text);
        repaint();
    }

    void dismissEdit()
    {
        if (!editor) return;
        editor->setVisible(false);
        auto editorToDelete = std::move(editor);
        juce::MessageManager::callAsync([e = std::move(editorToDelete)]() mutable { e.reset(); });
        repaint();
    }

    struct TooltipLabel : public juce::Component
    {
        juce::String text;
        void paint(juce::Graphics& g) override
        {
            float h = (float)getHeight();
            g.setColour(juce::Colour(Theme::darkBg).withAlpha(0.92f));
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.0f);
            g.setColour(juce::Colour(Theme::textDim));
            g.setFont(Theme::getUIFont(h * 0.6f));
            g.drawText(text, getLocalBounds(), juce::Justification::centred);
        }
    };

    std::unique_ptr<TooltipLabel> tooltipLabel;

    void showTooltip()
    {
        auto* topLevel = Theme::getOverlayParent(this);
        if (!topLevel) return;

        tooltipLabel = std::make_unique<TooltipLabel>();
        tooltipLabel->text = tooltip;
        tooltipLabel->setAlwaysOnTop(true);

        float h = (float)getHeight();
        int tipH = juce::roundToInt(h * 0.6f);
        int tipW = juce::roundToInt(Theme::getUIFont(tipH * 0.6f).getStringWidthFloat(tooltip)) + tipH;

        auto pos = topLevel->getLocalPoint(this, juce::Point<int>(getWidth() / 2, getHeight()));
        int tipX = pos.x - tipW / 2;
        int tipY = pos.y + 4;
        tipX = juce::jmax(0, juce::jmin(tipX, topLevel->getWidth() - tipW));

        tooltipLabel->setBounds(tipX, tipY, tipW, tipH);
        topLevel->addAndMakeVisible(tooltipLabel.get());
    }

    void hideTooltip()
    {
        tooltipLabel.reset();
    }
};
