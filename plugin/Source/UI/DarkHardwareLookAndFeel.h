#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class DarkHardwareLookAndFeel : public juce::LookAndFeel_V4 {
public:
    DarkHardwareLookAndFeel() {
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0b0d10));
        setColour(juce::TextEditor::textColourId, juce::Colour(0xffe6edf3));
        setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2a303c));
        setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff00e5ff));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider& slider) override {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(8.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto centre = bounds.getCentre();

        // 1. Anello esterno incassato (Chassis)
        g.setColour(juce::Colour(0xff090b0e));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        // 2. Traccia LED spenta
        float arcRadius = radius - 3.0f;
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff232832));
        g.strokePath(backgroundArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Traccia LED attiva (Ciano o Ambra in base al colore del slider)
        auto accentColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
        if (accentColour.isTransparent()) accentColour = juce::Colour(0xff00e5ff);

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
        g.setColour(accentColour);
        g.strokePath(valueArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 4. Manopola metallica spazzolata
        float knobRadius = radius - 9.0f;
        juce::ColourGradient metalGrad(juce::Colour(0xff343a46), centre.x, centre.y - knobRadius,
                                       juce::Colour(0xff181b22), centre.x, centre.y + knobRadius, false);
        g.setGradientFill(metalGrad);
        g.fillEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

        g.setColour(juce::Colour(0xff4a5262));
        g.drawEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

        // 5. Tacca luminosa indicatore
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
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        auto baseCol = backgroundColour;
        if (shouldDrawButtonAsDown) baseCol = baseCol.darker(0.2f);
        else if (shouldDrawButtonAsHighlighted) baseCol = baseCol.brighter(0.15f);

        g.setColour(baseCol);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(baseCol.brighter(0.35f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }
};
