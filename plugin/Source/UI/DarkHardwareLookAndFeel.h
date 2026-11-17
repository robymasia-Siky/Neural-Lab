#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DarkHardwareLookAndFeel : public juce::LookAndFeel_V4 {
public:
    DarkHardwareLookAndFeel() {
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff090c10));
        setColour(juce::TextEditor::textColourId, juce::Colour(0xfff1f5f9));
        setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff1e293b));
        setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff00e5ff));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider& slider) override {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(8.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto centre = bounds.getCentre();

        g.setColour(juce::Colour(0xff090b0e));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        float arcRadius = radius - 3.0f;
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff232832));
        g.strokePath(backgroundArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto accentColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
        if (accentColour.isTransparent()) accentColour = juce::Colour(0xff00e5ff);

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
        g.setColour(accentColour);
        g.strokePath(valueArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        float knobRadius = radius - 9.0f;
        juce::ColourGradient metalGrad(juce::Colour(0xff343a46), centre.x, centre.y - knobRadius,
                                       juce::Colour(0xff181b22), centre.x, centre.y + knobRadius, false);
        g.setGradientFill(metalGrad);
        g.fillEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

        g.setColour(juce::Colour(0xff4a5262));
        g.drawEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

        juce::Path pointer;
        float pointerLength = knobRadius * 0.65f;
        float pointerThickness = 2.5f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -knobRadius + 3.0f, pointerThickness, pointerLength, 1.0f);
        pointer.applyTransform(juce::AffineTransform::rotation(toAngle).translated(centre.x, centre.y));
        g.setColour(accentColour);
        g.fillPath(pointer);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto accent = button.findColour(juce::TextButton::textColourOffId);
        if (accent.isTransparent() || accent == juce::Colours::white)
            accent = juce::Colour(0xff38bdf8);
        bool enabled = button.isEnabled();

        // 1. Glow esterno sottile in hover
        if (enabled && shouldDrawButtonAsHighlighted) {
            g.setColour(accent.withAlpha(0.18f));
            g.fillRoundedRectangle(bounds.expanded(1.5f), 7.5f);
        }

        // 2. Ombra di profondità
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 1.2f), 6.0f);

        // 3. Superficie Obsidian Glass con tinta accento
        auto topCol = backgroundColour.interpolatedWith(accent, 0.14f).brighter(0.18f);
        auto botCol = backgroundColour.darker(0.45f);
        if (shouldDrawButtonAsDown && enabled) {
            topCol = backgroundColour.darker(0.30f);
            botCol = backgroundColour.interpolatedWith(accent, 0.18f);
        } else if (shouldDrawButtonAsHighlighted && enabled) {
            topCol = backgroundColour.interpolatedWith(accent, 0.24f).brighter(0.28f);
            botCol = backgroundColour.darker(0.20f);
        }

        juce::ColourGradient glassGrad(topCol, bounds.getX(), bounds.getY(),
                                       botCol, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(bounds, 6.0f);

        // 4. Riflesso vetro superiore (specular highlight)
        g.setColour(juce::Colours::white.withAlpha(enabled ? 0.16f : 0.04f));
        g.drawHorizontalLine(static_cast<int>(bounds.getY() + 1.0f), bounds.getX() + 5.0f, bounds.getRight() - 5.0f);

        // 5. Bordo perimetrale luminoso
        float rimAlpha = !enabled ? 0.20f : (shouldDrawButtonAsHighlighted ? 0.95f : 0.58f);
        g.setColour(accent.withAlpha(rimAlpha));
        g.drawRoundedRectangle(bounds, 6.0f, shouldDrawButtonAsHighlighted ? 1.5f : 1.1f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override {
        float fontSize = (button.getWidth() < 60) ? 10.0f : 11.0f;
        g.setFont(juce::FontOptions(fontSize, juce::Font::bold));
        auto textCol = button.findColour(juce::TextButton::textColourOffId);
        if (textCol.isTransparent()) textCol = juce::Colours::white;
        if (!button.isEnabled()) textCol = textCol.withAlpha(0.40f);
        g.setColour(textCol);
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 1),
                         juce::Justification::centred, 1);
    }
};