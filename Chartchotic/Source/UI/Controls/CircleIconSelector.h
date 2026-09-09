#pragma once

#include <set>
#include <JuceHeader.h>
#include "../Theme.h"
#include "../MenuGroup.h"

struct CircleItem
{
    juce::String label;    // short text shown in circle ("E", "M", etc.)
    juce::Image image;     // optional PNG (instruments)
    juce::Colour colour;   // optional per-item colour (default = unset/black)
    juce::String tooltip;  // longer name shown in expanded panel ("Easy", "Guitar", etc.)
};

// Circle icon selector — hover to open, move away to close.
// Click an item in the expanded panel to select it. Scroll to cycle.
// Assign to a MenuGroup for mutual exclusion with other circle selectors.
//
// Multi-select mode: multiple items can be enabled. Main circle stacks enabled items.
// Panel shows all items with enabled/disabled state + optional "All" row.
class CircleIconSelector : public juce::Component,
                           private juce::ComponentListener
{
public:
    static constexpr float multiStackOffset = 0.15f; // fraction of circle diameter between stacked icons

    CircleIconSelector() {}

    ~CircleIconSelector() override
    {
        dismissPanel();
    }

    // Bottom of the header strip. The panel hangs from there so its top
    // edge lands on a real edge; overlapping the sub-toolbar below is fine.
    void setPanelTopMargin(int y) { panelTopMargin = y; }

    void setMenuGroup(MenuGroup* group) { menuGroup = group; }

    void setItems(std::vector<CircleItem> newItems)
    {
        items = std::move(newItems);
        itemEnabled.assign(items.size(), true);
        repaint();
    }

    //==========================================================================
    // Single-select mode (default)

    void setSelectedIndex(int index, juce::NotificationType notification = juce::dontSendNotification)
    {
        if (selectedIndex == index) return;
        selectedIndex = index;
        repaint();
        if (notification != juce::dontSendNotification && onSelectionChanged)
            onSelectionChanged(selectedIndex);
    }

    int getSelectedIndex() const { return selectedIndex; }

    void setItemEnabled(int index, bool enabled)
    {
        if (index >= 0 && index < (int)itemEnabled.size())
        {
            itemEnabled[(size_t)index] = enabled;
            if (panel) panel->repaint();
        }
    }

    bool isItemEnabled(int index) const
    {
        if (index >= 0 && index < (int)itemEnabled.size())
            return itemEnabled[(size_t)index];
        return true;
    }

    std::function<void(int index)> onSelectionChanged;

    //==========================================================================
    // Multi-select mode

    void setMultiSelectMode(bool enabled, bool showAll = false)
    {
        multiSelectMode = enabled;
        showAllOption = showAll;
        repaint();
    }

    bool isMultiSelectMode() const { return multiSelectMode; }

    void setMultiSelectedIndices(const std::set<int>& indices)
    {
        multiSelectedIndices = indices;
        repaint();
        if (panel) panel->repaint();
    }

    const std::set<int>& getMultiSelectedIndices() const { return multiSelectedIndices; }

    bool isAllSelected() const
    {
        for (int i = 0; i < (int)items.size(); ++i)
            if (multiSelectedIndices.count(i) == 0) return false;
        return !items.empty();
    }

    // Callback: (index, modifierHeld). index = -1 for "All" row.
    std::function<void(int index, bool modifierHeld)> onMultiSelectionChanged;

    //==========================================================================

    void dismissPanel()
    {
        hoverTimer.stopTimer();
        if (menuGroup != nullptr)
            menuGroup->deactivate(this);
        if (panel != nullptr)
        {
            if (auto* topLevel = Theme::getOverlayParent(this))
                topLevel->removeComponentListener(this);
            panel.reset();
        }
    }

    bool isPanelVisible() const { return panel != nullptr; }

    void paint(juce::Graphics& g) override
    {
        if (items.empty()) return;
        auto bounds = getLocalBounds().toFloat();
        float d = juce::jmin(bounds.getWidth(), bounds.getHeight());

        if (multiSelectMode)
        {
            // Stack enabled circles with slight X offsets
            std::vector<int> enabled;
            for (int i = 0; i < (int)items.size(); ++i)
                if (multiSelectedIndices.count(i) > 0)
                    enabled.push_back(i);

            if (enabled.empty())
            {
                // Nothing selected — draw dim empty circle
                auto circle = juce::Rectangle<float>(
                    bounds.getCentreX() - d / 2.0f,
                    bounds.getCentreY() - d / 2.0f, d, d);
                g.setColour(juce::Colour(Theme::darkBg));
                g.fillEllipse(circle);
                g.setColour(juce::Colour(Theme::textDim).withAlpha(0.3f));
                g.drawEllipse(circle.reduced(0.5f), 1.0f);
            }
            else if (enabled.size() == 1)
            {
                auto circle = juce::Rectangle<float>(
                    bounds.getCentreX() - d / 2.0f,
                    bounds.getCentreY() - d / 2.0f, d, d);
                paintCircle(g, circle, items[(size_t)enabled[0]], true, false);
            }
            else
            {
                // Multiple: stack with X offset, back-to-front
                float stackOffset = d * multiStackOffset;
                float totalStackW = d + stackOffset * ((float)enabled.size() - 1.0f);
                float startX = bounds.getCentreX() - totalStackW / 2.0f;
                float cy = bounds.getCentreY() - d / 2.0f;

                for (int i = 0; i < (int)enabled.size(); ++i)
                {
                    auto circle = juce::Rectangle<float>(
                        startX + stackOffset * (float)i, cy, d, d);
                    paintCircle(g, circle, items[(size_t)enabled[i]], true, false);
                }
            }

        }
        else
        {
            // Single-select: draw the selected item
            auto circle = juce::Rectangle<float>(
                bounds.getCentreX() - d / 2.0f,
                bounds.getCentreY() - d / 2.0f, d, d);
            paintCircle(g, circle, items[(size_t)selectedIndex], true, false);

            if (mouseHovered || panel != nullptr)
            {
                g.setColour(juce::Colour(Theme::coral));
                g.drawEllipse(circle.reduced(0.5f), 1.5f);
            }
        }
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        mouseHovered = true;
        repaint();
        hoverTimer.stopTimer();
        if (panel == nullptr)
            showPanel();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        mouseHovered = false;
        repaint();
        startHoverTimer();
    }

    // Scroll: cycle through items without wrapping (only over the main circle icon)
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (items.empty() || !isMouseOver(false)) return;
        int dir = (wheel.deltaY > 0) ? -1 : 1;

        if (multiSelectMode)
        {
            // Scroll = exclusive select (single item), cycling through items
            // Find current single-selected index, or start from first enabled
            int current = -1;
            for (int i = 0; i < (int)items.size(); ++i)
                if (multiSelectedIndices.count(i)) { current = i; break; }
            if (current < 0) current = 0;

            int newIndex = juce::jlimit(0, (int)items.size() - 1, current + dir);
            if (newIndex == current) return;
            std::set<int> single = { newIndex };
            setMultiSelectedIndices(single);
            if (onMultiSelectionChanged)
                onMultiSelectionChanged(newIndex, false);
        }
        else
        {
            int newIndex = juce::jlimit(0, (int)items.size() - 1, selectedIndex + dir);
            if (newIndex == selectedIndex) return;
            setSelectedIndex(newIndex, juce::sendNotification);
        }
    }

    void showPanel()
    {
        if (panel != nullptr) return;

        if (menuGroup != nullptr)
            menuGroup->activate(this);

        auto* topLevel = Theme::getOverlayParent(this);
        if (topLevel == nullptr) return;

        float d = (float)juce::jmin(getWidth(), getHeight());
        int padding = 4;
        // Width follows the longest label rather than a fixed multiple of the
        // circle, so a two-row list like Drums does not trail a band of empty
        // panel to its right.
        float textW = 0.0f;
        for (const auto& item : items)
        {
            auto text = item.tooltip.isNotEmpty() ? item.tooltip : item.label;
            textW = juce::jmax(textW, Theme::controlFont.getStringWidthFloat(text));
        }
        int labelW = juce::roundToInt(textW) + padding * 2;
        int rowCount = (int)items.size() + (multiSelectMode && showAllOption ? 1 : 0);
        int panelW = (int)d + padding * 2 + labelW;
        int panelH = (int)(d * rowCount) + padding * (rowCount + 1);

        panel = std::make_unique<ExpandedPanel>(*this, d, (float)padding, labelW);
        panel->setSize(panelW, panelH);

        topLevel->addAndMakeVisible(panel.get());
        repositionPanel();
        topLevel->addComponentListener(this);
    }

private:
    MenuGroup* menuGroup = nullptr;

    std::vector<CircleItem> items;
    std::vector<bool> itemEnabled;
    int selectedIndex = 0;
    int panelHoverIndex = -1;
    int panelTopMargin = 0;
    bool mouseHovered = false;

    // Multi-select state
    bool multiSelectMode = false;
    bool showAllOption = false;
    std::set<int> multiSelectedIndices;

    //==========================================================================
    class ExpandedPanel : public juce::Component
    {
    public:
        ExpandedPanel(CircleIconSelector& o, float circleD, float pad, int lw)
            : owner(o), d(circleD), padding(pad), labelW(lw)
        {
            setAlwaysOnTop(true);
        }

        void paint(juce::Graphics& g) override
        {
            // Same layer as the Settings panel: translucent over the highway,
            // stepper's textDim edge rather than coral, which the circles use.
            g.setColour(juce::Colour(Theme::darkBg).withAlpha(Theme::panelBgAlpha));
            g.fillRoundedRectangle(getLocalBounds().toFloat(), Theme::panelRadius);

            g.setColour(juce::Colour(Theme::textDim).withAlpha(0.3f));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f),
                                   Theme::panelRadius, 1.0f);

            for (int i = 0; i < (int)owner.items.size(); i++)
            {
                auto circle = getCircleBounds(i);
                bool hovering = (i == owner.panelHoverIndex);

                if (owner.multiSelectMode)
                {
                    bool selected = owner.multiSelectedIndices.count(i) > 0;
                    owner.paintCircle(g, circle, owner.items[(size_t)i], selected, hovering, !selected);

                    auto& item = owner.items[(size_t)i];
                    auto text = item.tooltip.isNotEmpty() ? item.tooltip : item.label;
                    if (text.isNotEmpty())
                    {
                        auto labelBounds = juce::Rectangle<float>(
                            circle.getRight() + padding, circle.getY(),
                            (float)labelW - padding, circle.getHeight());
                        g.setColour(hovering ? juce::Colour(Theme::textWhite)
                                             : (selected ? juce::Colour(Theme::textDim)
                                                         : juce::Colour(Theme::textDim).withAlpha(0.35f)));
                        g.setFont(Theme::controlFont);
                        g.drawText(text, labelBounds.toNearestInt(),
                                   juce::Justification::centredLeft);
                    }
                }
                else
                {
                    bool selected = (i == owner.selectedIndex);
                    bool enabled = owner.isItemEnabled(i);
                    owner.paintCircle(g, circle, owner.items[(size_t)i], selected, hovering && enabled, !enabled);

                    auto& item = owner.items[(size_t)i];
                    auto text = item.tooltip.isNotEmpty() ? item.tooltip : item.label;
                    if (text.isNotEmpty())
                    {
                        auto labelBounds = juce::Rectangle<float>(
                            circle.getRight() + padding, circle.getY(),
                            (float)labelW - padding, circle.getHeight());
                        g.setColour((hovering && enabled) ? juce::Colour(Theme::textWhite)
                                                          : juce::Colour(Theme::textDim));
                        g.setFont(Theme::controlFont);
                        g.drawText(text, labelBounds.toNearestInt(),
                                   juce::Justification::centredLeft);
                    }
                }
            }

            // "All" option at bottom (multi-select only)
            if (owner.multiSelectMode && owner.showAllOption)
            {
                int allIdx = (int)owner.items.size();
                auto circle = getCircleBounds(allIdx);
                bool hovering = (allIdx == owner.panelHoverIndex);
                bool allOn = owner.isAllSelected();

                // Separator line above "All"
                float sepY = circle.getY() - padding * 0.5f;
                g.setColour(juce::Colour(Theme::textDim).withAlpha(0.2f));
                g.drawHorizontalLine((int)sepY, padding, (float)getWidth() - padding);

                // "All" circle — coral outline always to distinguish from regular items
                g.setColour(allOn ? juce::Colour(Theme::coral)
                                  : (hovering ? juce::Colour(Theme::darkBgHover)
                                              : juce::Colour(Theme::darkBg)));
                g.fillEllipse(circle);

                g.setColour(juce::Colour(Theme::coral));
                g.drawEllipse(circle.reduced(0.5f), 1.5f);

                g.setColour(juce::Colour(Theme::textWhite));
                auto typeface = juce::Typeface::createSystemTypefaceFor(
                    BinaryData::RussoOneRegular_ttf, BinaryData::RussoOneRegular_ttfSize);
                auto font = juce::Font(typeface).withHeight(circle.getHeight() * 0.55f);
                g.setFont(font);
                g.drawText("All", circle.toNearestInt(), juce::Justification::centred);
            }
        }

        void mouseMove(const juce::MouseEvent& e) override
        {
            if (e.eventComponent != this) return;
            int idx = hitTestItem(e.y);
            if (idx != owner.panelHoverIndex) { owner.panelHoverIndex = idx; repaint(); }
        }

        void mouseEnter(const juce::MouseEvent& e) override
        {
            if (e.eventComponent != this) return;
            owner.hoverTimer.stopTimer();
        }

        void mouseExit(const juce::MouseEvent& e) override
        {
            if (e.eventComponent != this) return;
            owner.panelHoverIndex = -1;
            repaint();
            owner.startHoverTimer();
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            if (e.eventComponent != this) return;
            int idx = hitTestItem(e.y);
            int totalRows = (int)owner.items.size() + (owner.multiSelectMode && owner.showAllOption ? 1 : 0);

            if (owner.multiSelectMode)
            {
                if (idx >= 0 && idx < totalRows)
                {
                    bool mod = juce::ModifierKeys::currentModifiers.isShiftDown()
                            || juce::ModifierKeys::currentModifiers.isAltDown();
                    int callbackIdx = (idx == (int)owner.items.size()) ? -1 : idx; // -1 for "All"
                    if (owner.onMultiSelectionChanged)
                        owner.onMultiSelectionChanged(callbackIdx, mod);
                    repaint();
                }
                // Don't dismiss panel in multi-select — let user toggle multiple
            }
            else
            {
                if (idx >= 0 && idx < (int)owner.items.size() && idx != owner.selectedIndex
                    && owner.isItemEnabled(idx))
                {
                    owner.selectedIndex = idx;
                    owner.repaint();
                    if (owner.onSelectionChanged)
                        owner.onSelectionChanged(owner.selectedIndex);
                }
                owner.dismissPanel();
            }
        }

    private:
        CircleIconSelector& owner;
        float d, padding;
        int labelW;

        juce::Rectangle<float> getCircleBounds(int index) const
        {
            float cy = padding + index * (d + padding);
            return { padding, cy, d, d };
        }

        int hitTestItem(int mouseY) const
        {
            int totalRows = (int)owner.items.size() + (owner.multiSelectMode && owner.showAllOption ? 1 : 0);
            float stride = d + padding;
            int idx = (int)((mouseY - padding) / stride);
            if (idx < 0 || idx >= totalRows) return -1;
            float itemTop = padding + idx * stride;
            if (mouseY >= itemTop && mouseY <= itemTop + d) return idx;
            return -1;
        }
    };

    std::unique_ptr<ExpandedPanel> panel;

    //==========================================================================
    void paintCircle(juce::Graphics& g, juce::Rectangle<float> bounds,
                     const CircleItem& item, bool selected, bool hovering,
                     bool dimmed = false) const
    {
        // Render to temp image when dimmed so opacity applies uniformly
        if (dimmed)
        {
            int imgW = (int)std::ceil(bounds.getWidth()) + 2;
            int imgH = (int)std::ceil(bounds.getHeight()) + 2;
            juce::Image temp(juce::Image::ARGB, imgW, imgH, true);
            {
                juce::Graphics tg(temp);
                auto localBounds = juce::Rectangle<float>(1.0f, 1.0f, bounds.getWidth(), bounds.getHeight());
                // Draw as if selected so dimming is the only visual difference
                paintCircleContent(tg, localBounds, item, true, hovering);
            }
            g.setOpacity(hovering ? 0.8f : 0.3f);
            g.drawImageAt(temp, (int)bounds.getX() - 1, (int)bounds.getY() - 1);
            g.setOpacity(1.0f);
        }
        else
        {
            paintCircleContent(g, bounds, item, selected, hovering);
        }
    }

    void paintCircleContent(juce::Graphics& g, juce::Rectangle<float> bounds,
                            const CircleItem& item, bool selected, bool hovering) const
    {
        bool hasColour = item.colour != juce::Colour();

        if (hasColour)
            g.setColour(item.colour);
        else if (selected)
            g.setColour(juce::Colour(Theme::coral));
        else if (hovering)
            g.setColour(juce::Colour(Theme::darkBgHover));
        else
            g.setColour(juce::Colour(Theme::darkBg));

        g.fillEllipse(bounds);

        if (item.image.isValid())
        {
            auto reduced = bounds.reduced(bounds.getWidth() * 0.05f);
            g.saveState();
            juce::Path clipPath;
            clipPath.addEllipse(bounds);
            g.reduceClipRegion(clipPath);
            auto imgBounds = juce::Rectangle<float>(0, 0, (float)item.image.getWidth(), (float)item.image.getHeight());
            auto transform = juce::RectanglePlacement(juce::RectanglePlacement::centred)
                .getTransformToFit(imgBounds, reduced);
            g.drawImageTransformed(item.image, transform);
            g.restoreState();
        }
        else if (item.label.isNotEmpty())
        {
            g.setColour(juce::Colour(Theme::textWhite));
            auto typeface = juce::Typeface::createSystemTypefaceFor(
                BinaryData::RussoOneRegular_ttf, BinaryData::RussoOneRegular_ttfSize);
            auto font = juce::Font(typeface).withHeight(bounds.getHeight() * 0.72f);
            g.setFont(font);
            g.drawText(item.label, bounds.translated(0.5f, 0.0f).toNearestInt(), juce::Justification::centred);
            g.drawText(item.label, bounds.toNearestInt(), juce::Justification::centred);
        }

        if (!hasColour)
        {
            g.setColour(juce::Colour(Theme::coral).withAlpha(0.5f));
            g.drawEllipse(bounds.reduced(0.5f), 1.0f);
        }
    }

    void repositionPanel()
    {
        if (panel == nullptr) return;
        auto* topLevel = Theme::getOverlayParent(this);
        if (topLevel == nullptr) return;

        float d = (float)juce::jmin(getWidth(), getHeight());
        int padding = 4;
        auto btnBottom = topLevel->getLocalPoint(this, juce::Point<int>(getWidth() / 2, getHeight()));
        int panelX = btnBottom.x - padding - (int)(d / 2.0f);
        int panelY = (panelTopMargin > 0 ? panelTopMargin : btnBottom.y) + 4;

        panelX = juce::jmax(0, juce::jmin(panelX, topLevel->getWidth() - panel->getWidth()));
        panelY = juce::jmin(panelY, topLevel->getHeight() - panel->getHeight());

        panel->setTopLeftPosition(panelX, panelY);
    }

    void componentMovedOrResized(juce::Component&, bool, bool wasResized) override
    {
        if (wasResized && panel != nullptr)
            repositionPanel();
    }

    struct HoverTimer : public juce::Timer
    {
        CircleIconSelector* owner = nullptr;
        void timerCallback() override
        {
            if (!owner || !owner->panel) { stopTimer(); return; }
            if (owner->isMouseOver(false) || owner->panel->isMouseOver(true))
                return;
            stopTimer();
            owner->dismissPanel();
        }
    };

    HoverTimer hoverTimer;

    void startHoverTimer()
    {
        hoverTimer.owner = this;
        hoverTimer.startTimer(Theme::hoverDismissMs);
    }
};
