#include "PluginEditor.h"

namespace
{
const juce::Colour kBg { 0xff161618 };
const juce::Colour kPanel { 0xff242428 };
const juce::Colour kAmber { 0xffe0a020 };
const juce::Colour kText { 0xffe6e4dc };
const juce::Colour kMuted { 0xff8a8680 };
const juce::Colour kWell { 0xff1a1a1c };
const juce::Colour kLine { 0xff3a3a40 };
constexpr int kModeRadioGroup = 1;

class IncDecArrowButton final : public juce::Button
{
public:
    explicit IncDecArrowButton (bool up) : juce::Button ({}), isUp (up) {}

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (kWell);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (highlighted || down ? kAmber : kLine);
        g.drawRoundedRectangle (r, 2.0f, 1.0f);

        juce::Path p;
        const auto c = r.getCentre();
        constexpr float w = 5.0f;
        constexpr float h = 3.6f;
        if (isUp)
            p.addTriangle (c.x, c.y - h, c.x + w, c.y + h * 0.55f, c.x - w, c.y + h * 0.55f);
        else
            p.addTriangle (c.x, c.y + h, c.x + w, c.y - h * 0.55f, c.x - w, c.y - h * 0.55f);

        g.setColour (down ? kAmber : kText);
        g.fillPath (p);
    }

private:
    bool isUp;
};
} // namespace

PcfLookAndFeel::PcfLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ToggleButton::textColourId, kText);
    setColour (juce::ToggleButton::tickColourId, kAmber);
    setColour (juce::ToggleButton::tickDisabledColourId, kLine);
    setColour (juce::CaretComponent::caretColourId, kAmber);
}

void PcfLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                       juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (kWell);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (kLine);
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.4f);

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (kAmber);
    g.strokePath (arc, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path tick;
    tick.addRectangle (-1.1f, -radius + 8.0f, 2.2f, radius * 0.42f);
    g.setColour (kText);
    g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void PcfLookAndFeel::drawTickBox (juce::Graphics& g, juce::Component& component,
                                  float x, float y, float w, float h,
                                  bool ticked, bool isEnabled, bool, bool)
{
    auto bounds = juce::Rectangle<float> (x, y, w, h).reduced (1.0f);
    g.setColour ((isEnabled ? kLine : kLine.withAlpha (0.5f)));

    if (auto* button = dynamic_cast<juce::Button*> (&component); button != nullptr && button->getRadioGroupId() != 0)
    {
        g.drawEllipse (bounds, 1.4f);
        if (ticked)
        {
            g.setColour (isEnabled ? kAmber : kAmber.withAlpha (0.5f));
            g.fillEllipse (bounds.reduced (3.4f));
        }
        return;
    }

    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
    if (ticked)
    {
        g.setColour (isEnabled ? kAmber : kAmber.withAlpha (0.5f));
        auto tick = getTickShape (0.75f);
        g.fillPath (tick, tick.getTransformToScaleToFit (bounds.reduced (3.0f, 4.0f), false));
    }
}

juce::Button* PcfLookAndFeel::createSliderButton (juce::Slider&, bool isIncrement)
{
    return new IncDecArrowButton (isIncrement);
}

juce::Font PcfLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::FontOptions (12.0f);
}

PcfEditor::PcfEditor (PcfProcessor& p)
    : AudioProcessorEditor (&p),
      freqA (p.apvts, "freq", freq),
      resA (p.apvts, "res", res),
      amountA (p.apvts, "amount", amount),
      decayA (p.apvts, "decay", decay),
      patternA (p.apvts, "pattern", pattern)
{
    setLookAndFeel (&lnf);
    setSize (780, 260);
    setResizable (false, false);

    title.setText ("OpenRb PCF", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setColour (juce::Label::textColourId, kAmber);
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    addAndMakeVisible (title);

    auto setupName = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, kMuted);
        l.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        addAndMakeVisible (l);
    };

    setupKnob (freq);
    setupKnob (res);
    setupKnob (amount);
    setupKnob (decay);
    setupPattern();
    setupModeButtons (p);

    setupName (freqN, "FREQ");
    setupName (resN, "RES");
    setupName (amountN, "AMT");
    setupName (decayN, "DEC");
    setupName (patternN, "PATTERN");
    setupName (modeN, "MODE");
    patternN.setJustificationType (juce::Justification::centredLeft);
    modeN.setJustificationType (juce::Justification::centredLeft);
}

PcfEditor::~PcfEditor()
{
    setLookAndFeel (nullptr);
}

void PcfEditor::setupKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
    s.setNumDecimalPlacesToDisplay (0);
    s.setMouseDragSensitivity (140);
    addAndMakeVisible (s);
}

void PcfEditor::setupPattern()
{
    pattern.setSliderStyle (juce::Slider::IncDecButtons);
    pattern.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 56, 28);
    pattern.setNumDecimalPlacesToDisplay (0);
    pattern.setColour (juce::Slider::textBoxBackgroundColourId, kWell);
    pattern.setColour (juce::Slider::textBoxOutlineColourId, kLine);
    addAndMakeVisible (pattern);
}

void PcfEditor::setupModeButtons (PcfProcessor& p)
{
    auto* modeParam = p.apvts.getParameter ("mode");
    jassert (modeParam != nullptr);

    for (auto* b : { &lowPass, &bandPass })
    {
        b->setRadioGroupId (kModeRadioGroup);
        b->setClickingTogglesState (true);
        b->setColour (juce::ToggleButton::textColourId, kText);
        addAndMakeVisible (b);
    }

    modeA = std::make_unique<juce::ParameterAttachment> (
        *modeParam,
        [this] (float value)
        {
            const juce::ScopedValueSetter<bool> svs (ignoreModeCallbacks, true);
            const int index = juce::roundToInt (value);
            lowPass.setToggleState (index == 0, juce::dontSendNotification);
            bandPass.setToggleState (index == 1, juce::dontSendNotification);
        });

    auto onClick = [this]
    {
        if (ignoreModeCallbacks || modeA == nullptr)
            return;
        modeA->setValueAsCompleteGesture (bandPass.getToggleState() ? 1.0f : 0.0f);
    };

    lowPass.onClick = onClick;
    bandPass.onClick = onClick;
    modeA->sendInitialUpdate();
}

void PcfEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    auto panel = getLocalBounds().toFloat().reduced (12.0f);
    g.setColour (kPanel);
    g.fillRoundedRectangle (panel, 8.0f);
    g.setColour (kLine);
    g.drawRoundedRectangle (panel, 8.0f, 1.0f);
}

void PcfEditor::resized()
{
    auto r = getLocalBounds().reduced (24);
    title.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);

    auto right = r.removeFromRight (168);
    r.removeFromRight (16);

    const int n = 4;
    const int w = r.getWidth() / n;
    const int y = r.getY();
    const int h = r.getHeight();

    auto slot = [&] (int i) { return juce::Rectangle<int> (r.getX() + i * w, y, w, h); };

    auto place = [&] (juce::Slider& s, juce::Label& name, int i)
    {
        auto b = slot (i).reduced (6, 0);
        name.setBounds (b.removeFromTop (18));
        s.setBounds (b);
    };

    place (freq, freqN, 0);
    place (res, resN, 1);
    place (amount, amountN, 2);
    place (decay, decayN, 3);

    modeN.setBounds (right.removeFromTop (18));
    lowPass.setBounds (right.removeFromTop (24));
    bandPass.setBounds (right.removeFromTop (24));
    right.removeFromTop (16);
    patternN.setBounds (right.removeFromTop (18));
    pattern.setBounds (right.removeFromTop (48).removeFromLeft (96));
}
