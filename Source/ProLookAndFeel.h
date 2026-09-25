#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// ============================================================================
// ProLookAndFeel
// Perillas con degradado metálico, sombra y aguja con brillo (efecto 3D),
// y toggles como interruptores deslizantes tipo hardware en vez de checkboxes.
// ============================================================================
class ProLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ProLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    // Dibuja un panel tipo "pantalla hundida" (bisel oscuro + sombra interior),
    // reutilizable para el analizador de espectro y el medidor de GR.
    static void drawInsetScreen (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius = 6.0f);

    // Dibuja un panel tipo "placa elevada" (chasis), para agrupar controles.
    static void drawRaisedPanel (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius = 8.0f);
};
