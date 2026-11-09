#include "PluginEditor.h"

#include <functional>

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
constexpr int kEditorW = 780;
constexpr int kCollapsedH = 260;
constexpr int kExpandedH = 370;
constexpr int kMainRowH = 176;
constexpr int kFoldSize = 20;
constexpr int kGearSize = 24;
const juce::Colour kError { 0xffd06060 };

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
    setColour (juce::TextEditor::backgroundColourId, kWell);
    setColour (juce::TextEditor::textColourId, kText);
    setColour (juce::TextEditor::outlineColourId, kLine);
    setColour (juce::TextEditor::focusedOutlineColourId, kAmber);
    setColour (juce::TextEditor::highlightColourId, kAmber);
    setColour (juce::TextEditor::highlightedTextColourId, kBg);
    setColour (juce::TextButton::buttonColourId, kWell);
    setColour (juce::TextButton::textColourOffId, kText);
    setColour (juce::TextButton::textColourOnId, kText);
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

    auto* button = dynamic_cast<juce::Button*> (&component);
    const bool radio = (button != nullptr && button->getRadioGroupId() != 0)
                       || (bool) component.getProperties()["radioLook"];
    if (radio)
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

void PcfLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                           const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (kWell);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (highlighted || down ? kAmber : kLine);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

juce::Button* PcfLookAndFeel::createSliderButton (juce::Slider&, bool isIncrement)
{
    return new IncDecArrowButton (isIncrement);
}

juce::Font PcfLookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}

GearButton::GearButton() : juce::Button ({})
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void GearButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (kWell);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (highlighted || down ? kAmber : kLine);
    g.drawRoundedRectangle (r, 2.0f, 1.0f);

    const auto c = r.getCentre();
    constexpr float outer = 6.6f;
    constexpr float inner = 4.2f;
    constexpr float hole = 1.8f;
    constexpr float toothW = 2.3f;
    g.setColour (down ? kAmber : kText);
    juce::Path gear;
    for (int i = 0; i < 4; ++i)
    {
        juce::Path tooth;
        tooth.addRoundedRectangle (-toothW * 0.5f, -outer, toothW, outer * 2.0f, 0.45f);
        gear.addPath (tooth, juce::AffineTransform::rotation ((float) i * juce::MathConstants<float>::pi * 0.25f)
                                 .translated (c.x, c.y));
    }
    gear.addEllipse (c.x - inner, c.y - inner, inner * 2.0f, inner * 2.0f);
    g.fillPath (gear);
    g.setColour (kWell);
    g.fillEllipse (c.x - hole, c.y - hole, hole * 2.0f, hole * 2.0f);
}

FoldArrow::FoldArrow() : juce::Button ({}) {}

void FoldArrow::setOpen (bool shouldBeOpen)
{
    if (open == shouldBeOpen)
        return;
    open = shouldBeOpen;
    repaint();
}

void FoldArrow::paintButton (juce::Graphics& g, bool highlighted, bool down)
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
    if (open)
        p.addTriangle (c.x, c.y - h, c.x + w, c.y + h * 0.55f, c.x - w, c.y + h * 0.55f);
    else
        p.addTriangle (c.x, c.y + h, c.x + w, c.y - h * 0.55f, c.x - w, c.y - h * 0.55f);

    g.setColour (down ? kAmber : kText);
    g.fillPath (p);
}

StepPad::StepPad() : juce::Button ({})
{
    setClickingTogglesState (false);
}

void StepPad::setLevel (int newLevel)
{
    newLevel = juce::jlimit (0, 2, newLevel);
    if (level == newLevel)
        return;
    level = newLevel;
    repaint();
}

void StepPad::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (kWell);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (highlighted || down ? kAmber : kLine);
    g.drawRoundedRectangle (r, 2.0f, 1.0f);

    if (level <= 0)
        return;

    auto fill = r.reduced (2.4f);
    if (level == 1)
        fill = fill.removeFromBottom (fill.getHeight() * 0.55f);
    g.setColour (kAmber.withAlpha (level == 1 ? 0.55f : 1.0f));
    g.fillRoundedRectangle (fill, 1.5f);
}

SequencerStrip::SequencerStrip (PcfProcessor& p)
    : enableA (p.apvts, "seq_enable", enable),
      lengthA (p.apvts, "seq_length", length),
      gridA (p.apvts, "seq_res", grid)
{
    enable.getProperties().set ("radioLook", true);
    enable.setClickingTogglesState (true);
    enable.setColour (juce::ToggleButton::textColourId, kText);
    addAndMakeVisible (enable);

    auto setupName = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setColour (juce::Label::textColourId, kMuted);
        l.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        addAndMakeVisible (l);
    };
    setupName (lengthN, "STEPS");
    setupName (gridN, "GRID");

    setupIncDec (length);
    setupIncDec (grid);
    grid.textFromValueFunction = [] (double v)
    {
        static constexpr int res[] = { 8, 16, 32 };
        return juce::String (res[juce::jlimit (0, 2, juce::roundToInt (v))]);
    };
    grid.valueFromTextFunction = [] (const juce::String& t)
    {
        const int n = t.getIntValue();
        if (n <= 8)
            return 0.0;
        if (n <= 16)
            return 1.0;
        return 2.0;
    };
    grid.updateText();
    length.onValueChange = [this] { resized(); };

    for (int i = 0; i < 32; ++i)
    {
        auto* param = p.apvts.getParameter (juce::String::formatted ("seq_%02d", i));
        jassert (param != nullptr);
        addAndMakeVisible (pads[(size_t) i]);
        padA[(size_t) i] = std::make_unique<juce::ParameterAttachment> (
            *param,
            [this, i] (float value)
            {
                pads[(size_t) i].setLevel (juce::roundToInt (value));
            });
        pads[(size_t) i].onClick = [this, i]
        {
            if (padA[(size_t) i] == nullptr)
                return;
            const int next = (pads[(size_t) i].getLevel() + 1) % 3;
            padA[(size_t) i]->setValueAsCompleteGesture ((float) next);
        };
        padA[(size_t) i]->sendInitialUpdate();
    }
}

SequencerStrip::~SequencerStrip() = default;

void SequencerStrip::setupIncDec (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::IncDecButtons);
    s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 28);
    s.setNumDecimalPlacesToDisplay (0);
    s.setColour (juce::Slider::textBoxBackgroundColourId, kWell);
    s.setColour (juce::Slider::textBoxOutlineColourId, kLine);
    addAndMakeVisible (s);
}

void SequencerStrip::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (36);
    enable.setBounds (top.removeFromLeft (96).withSizeKeepingCentre (96, 28));
    lengthN.setBounds (top.removeFromLeft (48).withSizeKeepingCentre (48, 18));
    length.setBounds (top.removeFromLeft (96).withSizeKeepingCentre (96, 32));
    top.removeFromLeft (12);
    gridN.setBounds (top.removeFromLeft (40).withSizeKeepingCentre (40, 18));
    grid.setBounds (top.removeFromLeft (96).withSizeKeepingCentre (96, 32));

    r.removeFromTop (8);
    r.removeFromRight (kFoldSize + 8);
    const int n = juce::jlimit (1, 32, juce::roundToInt (length.getValue()));
    const int gap = 3;
    const int w = juce::jmax (8, (r.getWidth() - gap * (n - 1)) / n);
    const int h = juce::jmin (36, r.getHeight());
    const int y = r.getY() + (r.getHeight() - h) / 2;
    for (int i = 0; i < 32; ++i)
    {
        const bool show = i < n;
        pads[(size_t) i].setVisible (show);
        if (show)
            pads[(size_t) i].setBounds (r.getX() + i * (w + gap), y, w, h);
    }
}

class RecallImportPanel final : public juce::Component
{
public:
    RecallImportPanel (PcfProcessor& p, std::function<void()> onCloseIn)
        : owner (p), onClose (std::move (onCloseIn))
    {
        heading.setText ("Import recall", juce::dontSendNotification);
        heading.setJustificationType (juce::Justification::centredLeft);
        heading.setColour (juce::Label::textColourId, kAmber);
        heading.setFont (juce::FontOptions().withHeight (16.0f).withStyle ("Bold"));
        addAndMakeVisible (heading);

        hint.setText ("Paste OpenRB PCF settings copied from Modloader.", juce::dontSendNotification);
        hint.setJustificationType (juce::Justification::centredLeft);
        hint.setColour (juce::Label::textColourId, kMuted);
        hint.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (hint);

        editor.setMultiLine (true);
        editor.setReturnKeyStartsNewLine (true);
        editor.setFont (juce::FontOptions (13.0f));
        editor.setTextToShowWhenEmpty ("openrb-pcf 1\nfrequency: 64\nresonance: 9\n...", kMuted);
        addAndMakeVisible (editor);

        status.setJustificationType (juce::Justification::centredLeft);
        status.setColour (juce::Label::textColourId, kError);
        status.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (status);

        paste.setButtonText ("Paste");
        paste.onClick = [this]
        {
            editor.setText (juce::SystemClipboard::getTextFromClipboard());
            status.setText ({}, juce::dontSendNotification);
        };
        addAndMakeVisible (paste);

        import.setButtonText ("Import");
        import.onClick = [this]
        {
            const auto err = owner.importRecall (editor.getText());
            if (err.isEmpty())
            {
                if (onClose)
                    onClose();
                return;
            }
            status.setColour (juce::Label::textColourId, kError);
            status.setText (err, juce::dontSendNotification);
        };
        addAndMakeVisible (import);

        close.setButtonText ("Close");
        close.onClick = [this]
        {
            if (onClose)
                onClose();
        };
        addAndMakeVisible (close);
        setWantsKeyboardFocus (true);
    }

    void reset()
    {
        editor.clear();
        status.setText ({}, juce::dontSendNotification);
        editor.grabKeyboardFocus();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBg.withAlpha (0.82f));
        auto card = cardBounds().toFloat();
        g.setColour (kPanel);
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (kLine);
        g.drawRoundedRectangle (card, 8.0f, 1.0f);
    }

    void resized() override
    {
        auto r = cardBounds().reduced (16);
        heading.setBounds (r.removeFromTop (22));
        hint.setBounds (r.removeFromTop (18));
        r.removeFromTop (8);
        auto buttons = r.removeFromBottom (28);
        r.removeFromBottom (8);
        status.setBounds (r.removeFromBottom (18));
        r.removeFromBottom (8);
        editor.setBounds (r);

        paste.setBounds (buttons.removeFromLeft (80));
        buttons.removeFromLeft (8);
        import.setBounds (buttons.removeFromLeft (80));
        close.setBounds (buttons.removeFromRight (80));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! cardBounds().contains (e.getPosition()) && onClose)
            onClose();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onClose)
                onClose();
            return true;
        }
        return false;
    }

private:
    juce::Rectangle<int> cardBounds() const
    {
        return getLocalBounds().reduced (40, 24).withSizeKeepingCentre (
            juce::jmin (440, getWidth() - 48),
            juce::jmin (208, getHeight() - 32));
    }

    PcfProcessor& owner;
    std::function<void()> onClose;
    juce::Label heading, hint, status;
    juce::TextEditor editor;
    juce::TextButton paste, import, close;
};

PcfEditor::PcfEditor (PcfProcessor& p)
    : AudioProcessorEditor (&p),
      owner (p),
      freqA (p.apvts, "freq", freq),
      resA (p.apvts, "res", res),
      amountA (p.apvts, "amount", amount),
      decayA (p.apvts, "decay", decay),
      patternA (p.apvts, "pattern", pattern)
{
    setLookAndFeel (&lnf);
    setSize (kEditorW, kCollapsedH);
    setResizable (false, false);

    title.setText ("OpenRB PCF", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setColour (juce::Label::textColourId, kAmber);
    title.setFont (juce::FontOptions().withHeight (18.0f).withStyle ("Bold"));
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

    seqStrip = std::make_unique<SequencerStrip> (p);
    addChildComponent (*seqStrip);

    fold.onClick = [this] { setSeqOpen (! seqOpen); };
    addAndMakeVisible (fold);

    gear.onClick = [this] { setImportOpen (importPanel == nullptr || ! importPanel->isVisible()); };
    addAndMakeVisible (gear);

    if (auto* seqParam = p.apvts.getParameter ("seq_enable"))
    {
        seqEnableWatch = std::make_unique<juce::ParameterAttachment> (
            *seqParam,
            [this] (float value)
            {
                const bool on = value >= 0.5f;
                const float a = on ? 0.45f : 1.0f;
                pattern.setEnabled (! on);
                patternN.setEnabled (! on);
                pattern.setAlpha (a);
                patternN.setAlpha (a);
            });
        seqEnableWatch->sendInitialUpdate();
    }
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

void PcfEditor::setSeqOpen (bool open)
{
    if (seqOpen == open)
        return;
    seqOpen = open;
    fold.setOpen (open);
    if (seqStrip != nullptr)
        seqStrip->setVisible (open);
    setSize (kEditorW, open ? kExpandedH : kCollapsedH);
}

void PcfEditor::setImportOpen (bool open)
{
    if (open)
    {
        if (importPanel == nullptr)
        {
            importPanel = std::make_unique<RecallImportPanel> (owner, [this] { setImportOpen (false); });
            addAndMakeVisible (*importPanel);
        }
        importPanel->setBounds (getLocalBounds());
        importPanel->setVisible (true);
        importPanel->toFront (false);
        gear.toFront (false);
        importPanel->reset();
        return;
    }

    if (importPanel != nullptr)
        importPanel->setVisible (false);
}

void PcfEditor::resized()
{
    auto bounds = getLocalBounds();
    if (importPanel != nullptr)
        importPanel->setBounds (bounds);

    auto top = bounds.reduced (16);
    gear.setBounds (top.removeFromTop (kGearSize).removeFromRight (kGearSize));
    gear.toFront (false);

    auto corner = getLocalBounds().reduced (16);
    fold.setBounds (corner.removeFromBottom (kFoldSize).removeFromRight (kFoldSize));
    fold.toFront (false);

    auto r = getLocalBounds().reduced (24);
    auto titleRow = r.removeFromTop (28);
    titleRow.removeFromRight (kGearSize);
    title.setBounds (titleRow);
    r.removeFromTop (8);

    auto main = r.removeFromTop (kMainRowH);
    if (seqOpen && seqStrip != nullptr)
    {
        r.removeFromTop (8);
        seqStrip->setBounds (r);
    }

    auto right = main.removeFromRight (168);
    main.removeFromRight (16);

    const int n = 4;
    const int w = main.getWidth() / n;
    const int y = main.getY();
    const int h = main.getHeight();

    auto slot = [&] (int i) { return juce::Rectangle<int> (main.getX() + i * w, y, w, h); };

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
