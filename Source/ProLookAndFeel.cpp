#include "ProLookAndFeel.h"

ProLookAndFeel::ProLookAndFeel()
{
    setColour (juce::Slider::thumbColourId, juce::Colours::white);
}

void ProLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                                        juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centre = bounds.getCentre();
    auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    auto accentColour = slider.findColour (juce::Slider::rotarySliderFillColourId);

    auto knobBounds = juce::Rectangle<float> (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    // --- Sombra proyectada (da sensación de que el knob flota sobre el panel) ---
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (knobBounds.translated (0.0f, radius * 0.14f).expanded (radius * 0.03f));

    // --- Pista del arco (fondo, detrás del cuerpo del knob) ---
    juce::Path arcTrack;
    arcTrack.addCentredArc (centre.x, centre.y, radius + 5.0f, radius + 5.0f, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (arcTrack, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // --- Cuerpo del knob: degradado radial metálico (simula volumen/curvatura) ---
    juce::ColourGradient bodyGradient (juce::Colour (0xff53535c), centre.x - radius * 0.35f, centre.y - radius * 0.55f,
                                        juce::Colour (0xff17171b), centre.x, centre.y + radius * 0.9f, true);
    bodyGradient.addColour (0.55, juce::Colour (0xff2e2e33));
    g.setGradientFill (bodyGradient);
    g.fillEllipse (knobBounds);

    // --- Bisel exterior (borde oscuro definiendo el canto del knob) ---
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (knobBounds, 1.2f);

    // --- Reflejo superior (luz simulada cayendo desde arriba) ---
    juce::Path highlight;
    highlight.addPieSegment (knobBounds.reduced (radius * 0.12f), juce::degreesToRadians (200.0f),
                              juce::degreesToRadians (345.0f), 0.35);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillPath (highlight);

    // --- Arco de valor (color de acento, muestra la posición actual) ---
    juce::Path arcValue;
    arcValue.addCentredArc (centre.x, centre.y, radius + 5.0f, radius + 5.0f, 0.0f,
                             rotaryStartAngle, angle, true);
    g.setColour (accentColour);
    g.strokePath (arcValue, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // --- Aguja indicadora ---
    juce::Path pointer;
    auto pointerLength = radius * 0.62f;
    auto pointerThickness = 2.6f;
    pointer.addRoundedRectangle (-pointerThickness * 0.5f, -radius * 0.82f, pointerThickness, pointerLength,
                                  pointerThickness * 0.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillPath (pointer);

    // --- Punto de brillo en la punta (efecto "LED") ---
    auto tipX = centre.x + std::sin (angle) * radius * 0.82f;
    auto tipY = centre.y - std::cos (angle) * radius * 0.82f;
    g.setColour (accentColour);
    g.fillEllipse (tipX - 2.2f, tipY - 2.2f, 4.4f, 4.4f);
    g.setColour (accentColour.withAlpha (0.35f));
    g.fillEllipse (tipX - 4.5f, tipY - 4.5f, 9.0f, 9.0f);
}

void ProLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();
    const float switchWidth = 36.0f;
    const float switchHeight = 18.0f;
    auto switchBounds = juce::Rectangle<float> (0.0f, bounds.getCentreY() - switchHeight * 0.5f,
                                                 switchWidth, switchHeight);

    bool on = button.getToggleState();
    auto accent = button.findColour (juce::ToggleButton::tickColourId);

    // Sombra interior de la pista (efecto hundido, como un carril real)
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (switchBounds, switchHeight * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (switchBounds, switchHeight * 0.5f, 1.0f);

    if (on)
    {
        juce::ColourGradient trackGrad (accent.brighter (0.15f), switchBounds.getX(), switchBounds.getY(),
                                         accent.darker (0.4f), switchBounds.getX(), switchBounds.getBottom(), false);
        g.setGradientFill (trackGrad);
        g.fillRoundedRectangle (switchBounds.reduced (1.5f), (switchHeight - 3.0f) * 0.5f);
    }

    // Perilla deslizante con sombra propia (efecto 3D)
    auto knobDiameter = switchHeight - 4.0f;
    auto knobX = on ? switchBounds.getRight() - knobDiameter - 2.0f : switchBounds.getX() + 2.0f;
    auto knobBounds = juce::Rectangle<float> (knobX, switchBounds.getY() + 2.0f, knobDiameter, knobDiameter);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (knobBounds.translated (0.0f, 0.8f));
    juce::ColourGradient knobGrad (juce::Colours::white, knobBounds.getX(), knobBounds.getY(),
                                    juce::Colour (0xffcfcfcf), knobBounds.getX(), knobBounds.getBottom(), false);
    g.setGradientFill (knobGrad);
    g.fillEllipse (knobBounds);

    // Texto de la etiqueta
    g.setColour (button.findColour (juce::ToggleButton::textColourId));
    g.setFont (13.0f);
    g.drawText (button.getButtonText(),
                (int) switchBounds.getRight() + 8, 0,
                (int) (bounds.getWidth() - switchWidth - 8.0f), (int) bounds.getHeight(),
                juce::Justification::centredLeft);
}

void ProLookAndFeel::drawInsetScreen (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius)
{
    // Bisel exterior (marco ligeramente más claro, simula el borde físico del chasis)
    g.setColour (juce::Colour (0xff3a3a40));
    g.fillRoundedRectangle (bounds, cornerRadius);

    auto inner = bounds.reduced (2.0f);

    // Pantalla hundida: degradado oscuro + sombra superior para efecto "hueco"
    juce::ColourGradient screenGrad (juce::Colour (0xff05070a), inner.getX(), inner.getY(),
                                      juce::Colour (0xff11151a), inner.getX(), inner.getBottom(), false);
    g.setGradientFill (screenGrad);
    g.fillRoundedRectangle (inner, cornerRadius * 0.8f);

    // Sombra interior arriba (simula que el borde superior "tapa" la pantalla)
    juce::ColourGradient topShadow (juce::Colours::black.withAlpha (0.5f), inner.getX(), inner.getY(),
                                     juce::Colours::black.withAlpha (0.0f), inner.getX(), inner.getY() + inner.getHeight() * 0.25f, false);
    g.setGradientFill (topShadow);
    g.fillRoundedRectangle (inner, cornerRadius * 0.8f);
}

void ProLookAndFeel::drawRaisedPanel (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius)
{
    // Sombra proyectada debajo del panel
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), cornerRadius);

    // Cuerpo del panel: degradado sutil de arriba (más claro) a abajo (más oscuro)
    juce::ColourGradient neutralGrad (juce::Colour (0xff2b2b32), bounds.getX(), bounds.getY(),
                                       juce::Colour (0xff1a1a1f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (neutralGrad);
    g.fillRoundedRectangle (bounds, cornerRadius);

    // Línea de brillo superior (borde superior más claro = luz cayendo desde arriba)
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawLine (bounds.getX() + cornerRadius, bounds.getY() + 0.75f, bounds.getRight() - cornerRadius, bounds.getY() + 0.75f, 1.0f);

    // Borde general sutil
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);
}
