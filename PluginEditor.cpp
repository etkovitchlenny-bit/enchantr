#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace ench;

namespace
{
    std::string toStd (const juce::String& s) { return s.toUpperCase().toStdString(); }

    const char* levelNameFor (int level)
    {
        static const char* names[] = { "NOVICE", "ADEPT", "APPRENTICE", "SORCERER", "ARCHMAGE" };
        return names[juce::jlimit (0, 4, (level - 1) / 3)];
    }
}

//==============================================================================
EnchantrEditor::EnchantrEditor (EnchantrProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    for (int i = 0; i < kNumKnobs; ++i)
        knobParams[i] = proc.apvts.getParameter (kKnobs[i].id);

    selected = proc.getCurrentPresetIndex();

    setSize (600, 840);
    setResizable (true, false);
    setResizeLimits (300, 420, 900, 1260);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kW / (double) kH);

    startTimerHz (15);
}

EnchantrEditor::~EnchantrEditor()
{
    stopTimer();
}

void EnchantrEditor::resized() {}

//==============================================================================
void EnchantrEditor::burst()
{
    static const juce::uint32 cols[] = { 0xfff6d36a, 0xff8ff0ff, 0xffffffff, 0xffff9fb8 };
    for (int i = 0; i < 44; ++i)
    {
        const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        const float s = 0.8f + rng.nextFloat() * 2.2f;
        sparks.push_back ({ 111.f, 70.f, std::cos (a) * s, std::sin (a) * s - 1.f, 1.f, cols[i % 4], i % 3 == 0 });
    }
}

void EnchantrEditor::startLevelUp()
{
    castStartMs = juce::Time::getMillisecondCounterHiRes();
    castT = 0.f;
    burst();
}

void EnchantrEditor::timerCallback()
{
    ++frame;

    // audio level from the scope buffer
    float acc = 0.f;
    const int head = proc.getScopeHead();
    for (int k = 1; k <= 12; ++k)
    {
        const int idx = (head - k + EnchantrProcessor::scopeSize) % EnchantrProcessor::scopeSize;
        acc = juce::jmax (acc, std::abs (proc.getScopeSample (idx)));
    }
    peak = juce::jmax (acc, peak * 0.85f);

    if (castStartMs >= 0.0)
    {
        castT = (float) ((juce::Time::getMillisecondCounterHiRes() - castStartMs) / 1400.0);
        if (castT >= 1.f)
        {
            castT = -1.f;
            castStartMs = -1.0;
        }
    }

    // parallax follows the pointer, or drifts when idle
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float tx = (now - lastMoveMs < 2500.0) ? ptr : std::sin ((float) now * 0.0006f) * 0.8f;
    par += (tx - par) * 0.15f;

    if (frame % 6 == 0)
        sparks.push_back ({ 111.f + (rng.nextFloat() - 0.5f) * 8.f, 72.f, (rng.nextFloat() - 0.5f) * 0.4f,
                            -0.5f - rng.nextFloat() * 0.4f, 0.9f, 0xff8ff0ff, false });
    for (auto& s : sparks)
    {
        s.x += s.vx;
        s.y += s.vy;
        s.vy += 0.05f;
        s.life -= 0.025f;
    }
    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const Spark& s) { return s.life <= 0.f; }), sparks.end());

    if (messageFrames > 0 && --messageFrames == 0)
        message = {};

    repaint();
}

//==============================================================================
juce::String EnchantrEditor::knobHint (int i) const
{
    if (i < 0 || knobParams[i] == nullptr)
        return {};

    auto* p = knobParams[i];
    const float real = p->convertFrom0to1 (p->getValue());
    juce::String v;
    if (juce::String (kKnobs[i].id) == "root")
        v = juce::MidiMessage::getMidiNoteName ((int) std::round (real), true, true, 5);
    else if (std::abs (real) >= 100.f)
        v = juce::String (juce::roundToInt (real));
    else
        v = juce::String (real, 2);

    auto unit = p->getLabel();
    return juce::String (kKnobs[i].label) + " " + v + (unit.isNotEmpty() ? " " + unit : juce::String());
}

void EnchantrEditor::buildState()
{
    ui.tab = tab;
    ui.timeSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
    ui.frame = frame / 1;
    ui.peak = peak;
    ui.castT = castT;
    ui.par = par;
    ui.levelPressed = levelDown;
    ui.dragOver = dragOver;
    ui.sparks = sparks;
    ui.level = proc.getLevel();
    ui.levelName = levelNameFor (ui.level);
    ui.presetName = toStd (proc.getPresetName());
    ui.sampleName = toStd (proc.getSampleName());
    ui.hasSample = proc.hasSample();

    const int head = proc.getScopeHead();
    const int step = juce::jmax (1, EnchantrProcessor::scopeSize / 43);
    for (int i = 0; i < 43; ++i)
    {
        const int idx = (head + i * step) % EnchantrProcessor::scopeSize;
        ui.bars[i] = juce::jmin (1.f, std::abs (proc.getScopeSample (idx)) * 2.5f);
    }

    for (int i = 0; i < kNumKnobs; ++i)
        ui.knob[i] = knobParams[i] != nullptr ? knobParams[i]->getValue() : 0.f;

    const int hk = dragKnob >= 0 ? dragKnob : hoverKnob;
    ui.hoverKnob = hk;
    ui.hint = toStd (knobHint (hk));

    ui.presets.clear();
    for (const auto& pr : proc.getPresets())
        ui.presets.push_back ({ toStd (pr.name), pr.level, pr.factory });
    ui.selected = selected;
    ui.scroll = scroll;
    ui.message = toStd (message);
}

void EnchantrEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14102a));

    buildState();
    art.render (canvas, ui);

    {
        juce::Image::BitmapData bd (image, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < kH; ++y)
            std::memcpy (bd.getLinePointer (y), &canvas.d[(size_t) y * (size_t) kW], sizeof (juce::uint32) * (size_t) kW);
    }

    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImage (image, getLocalBounds().toFloat());
}

//==============================================================================
juce::Point<double> EnchantrEditor::toLogical (const juce::MouseEvent& e) const
{
    return { e.position.x * (double) kW / (double) getWidth(), e.position.y * (double) kH / (double) getHeight() };
}

int EnchantrEditor::knobAt (juce::Point<double> p) const
{
    int best = -1;
    double bd = 11.0;
    for (int i = 0; i < kNumKnobs; ++i)
    {
        if (kKnobs[i].tab != tab)
            continue;
        const double d = std::hypot (p.x - kKnobs[i].x, p.y - kKnobs[i].y);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

juce::String EnchantrEditor::getTooltip()
{
    using namespace layout;
    if (tab == Tab::Main)
    {
        if (folder.contains (lastLogical.x, lastLogical.y))  return "Import a sound (or drop a file anywhere)";
        if (floppy.contains (lastLogical.x, lastLogical.y))  return "Save as preset";
        if (arrowL.contains (lastLogical.x, lastLogical.y))  return "Previous preset";
        if (arrowR.contains (lastLogical.x, lastLogical.y))  return "Next preset";
    }
    return {};
}

void EnchantrEditor::mouseMove (const juce::MouseEvent& e)
{
    lastLogical = toLogical (e);
    ptr = (float) ((lastLogical.x / kW - 0.5) * 2.0);
    lastMoveMs = juce::Time::getMillisecondCounterHiRes();
    const int h = knobAt (lastLogical);
    if (h != hoverKnob) { hoverKnob = h; repaint(); }
}

void EnchantrEditor::mouseExit (const juce::MouseEvent&)
{
    hoverKnob = -1;
    repaint();
}

void EnchantrEditor::mouseDown (const juce::MouseEvent& e)
{
    using namespace layout;
    const auto p = toLogical (e);
    lastLogical = p;

    auto inCircle = [&] (int cx, int cy, int r) { return std::hypot (p.x - cx, p.y - cy) <= r; };

    if (inCircle (levelX, levelY, levelR))
    {
        levelDown = true;
        proc.levelUp();
        startLevelUp();
        repaint();
        return;
    }
    if (inCircle (tweakX, tweakY, tweakR))
    {
        tab = (tab == Tab::Tweak) ? Tab::Main : Tab::Tweak;
        hoverKnob = -1;
        repaint();
        return;
    }
    if (inCircle (presetsX, presetsY, presetsR))
    {
        tab = (tab == Tab::Presets) ? Tab::Main : Tab::Presets;
        hoverKnob = -1;
        selected = proc.getCurrentPresetIndex();
        ensureSelectedVisible();
        repaint();
        return;
    }

    if (tab == Tab::Presets)
    {
        const int n = (int) proc.getPresets().size();
        for (int i = 0; i < rowCount; ++i)
        {
            const int y = rowY0 + i * rowH;
            if (p.x >= rowX && p.x < rowX + rowW && p.y >= y && p.y < y + 14 && scroll + i < n)
            {
                selected = scroll + i;
                if (e.getNumberOfClicks() > 1)
                    loadSelected();
                repaint();
                return;
            }
        }
        if (loadBtn.contains (p.x, p.y)) { loadSelected(); return; }
        if (saveBtn.contains (p.x, p.y)) { promptSave(); return; }
        if (delBtn.contains (p.x, p.y))  { promptDelete(); return; }
        return;
    }

    if (tab == Tab::Main)
    {
        if (folder.contains (p.x, p.y)) { openChooser(); return; }
        if (floppy.contains (p.x, p.y)) { promptSave(); return; }
        if (arrowL.contains (p.x, p.y)) { stepPreset (-1); return; }
        if (arrowR.contains (p.x, p.y)) { stepPreset (+1); return; }
    }

    const int k = knobAt (p);
    if (k >= 0 && knobParams[k] != nullptr)
    {
        if (e.getNumberOfClicks() > 1)
        {
            knobParams[k]->beginChangeGesture();
            knobParams[k]->setValueNotifyingHost (knobParams[k]->getDefaultValue());
            knobParams[k]->endChangeGesture();
            return;
        }
        dragKnob = k;
        dragStartY = p.y;
        dragStartVal = knobParams[k]->getValue();
        knobParams[k]->beginChangeGesture();
    }
}

void EnchantrEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragKnob < 0)
        return;
    const auto p = toLogical (e);
    const double sens = e.mods.isShiftDown() ? 160.0 : 40.0; // logical pixels for the full range
    const double v = juce::jlimit (0.0, 1.0, dragStartVal + (dragStartY - p.y) / sens);
    knobParams[dragKnob]->setValueNotifyingHost ((float) v);
    repaint();
}

void EnchantrEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragKnob >= 0 && knobParams[dragKnob] != nullptr)
        knobParams[dragKnob]->endChangeGesture();
    dragKnob = -1;
    levelDown = false;
    repaint();
}

void EnchantrEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (tab == Tab::Presets)
    {
        const int n = (int) proc.getPresets().size();
        const int maxScroll = juce::jmax (0, n - layout::rowCount);
        scroll = juce::jlimit (0, maxScroll, scroll + (w.deltaY > 0 ? -1 : 1));
        repaint();
        return;
    }

    const int k = knobAt (toLogical (e));
    if (k >= 0 && knobParams[k] != nullptr)
    {
        auto* p = knobParams[k];
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, p->getValue() + (w.deltaY > 0 ? 0.03f : -0.03f)));
        p->endChangeGesture();
        repaint();
    }
}

//==============================================================================
void EnchantrEditor::showMessage (const juce::String& m)
{
    message = m;
    messageFrames = 15;
    repaint();
}

void EnchantrEditor::ensureSelectedVisible()
{
    const int n = (int) proc.getPresets().size();
    const int maxScroll = juce::jmax (0, n - layout::rowCount);
    if (selected >= 0)
    {
        if (selected < scroll) scroll = selected;
        if (selected >= scroll + layout::rowCount) scroll = selected - layout::rowCount + 1;
    }
    scroll = juce::jlimit (0, maxScroll, scroll);
}

void EnchantrEditor::stepPreset (int dir)
{
    const int n = (int) proc.getPresets().size();
    if (n == 0)
        return;
    int cur = proc.getCurrentPresetIndex();
    cur = cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
    if (proc.loadPreset (cur))
    {
        selected = cur;
        ensureSelectedVisible();
        startLevelUp();
    }
    repaint();
}

void EnchantrEditor::loadSelected()
{
    if (selected >= 0 && proc.loadPreset (selected))
    {
        showMessage ("LOADED!");
        startLevelUp();
    }
}

void EnchantrEditor::promptSave()
{
    auto* w = new juce::AlertWindow ("Save preset", "Give your preset a name:", juce::MessageBoxIconType::NoIcon);
    const auto current = proc.getPresetName();
    w->addTextEditor ("name", current.isNotEmpty() ? current : "MY PRESET");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<EnchantrEditor> safe (this);
    w->enterModalState (true,
                        juce::ModalCallbackFunction::create ([safe, w] (int result)
                        {
                            if (result != 1 || safe == nullptr)
                                return;
                            const auto name = w->getTextEditorContents ("name");
                            if (safe->proc.saveUserPreset (name))
                            {
                                safe->selected = safe->proc.getCurrentPresetIndex();
                                safe->ensureSelectedVisible();
                                safe->showMessage ("SAVED!");
                            }
                        }),
                        true);
}

void EnchantrEditor::promptDelete()
{
    const auto& list = proc.getPresets();
    if (selected < 0 || selected >= (int) list.size() || list[(size_t) selected].factory)
    {
        showMessage ("CAN'T DELETE FACTORY");
        return;
    }

    juce::Component::SafePointer<EnchantrEditor> safe (this);
    const int idx = selected;
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (
                                      juce::MessageBoxIconType::QuestionIcon, "Delete preset",
                                      "Delete \"" + list[(size_t) selected].name + "\"?", "Delete", "Cancel"),
                                  [safe, idx] (int result)
                                  {
                                      if (result != 1 || safe == nullptr)
                                          return;
                                      if (safe->proc.deleteUserPreset (idx))
                                      {
                                          safe->selected = juce::jmin (idx, (int) safe->proc.getPresets().size() - 1);
                                          safe->ensureSelectedVisible();
                                          safe->showMessage ("DELETED!");
                                      }
                                  });
}

//==============================================================================
bool EnchantrEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg" || ext == ".mp3")
            return true;
    }
    return false;
}

void EnchantrEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
    {
        if (isInterestedInFileDrag (juce::StringArray (f)))
        {
            loadFile (juce::File (f));
            break;
        }
    }
    repaint();
}

void EnchantrEditor::loadFile (const juce::File& file)
{
    if (proc.loadSample (file))
    {
        // first sound: roll an ear candy right away
        if (proc.getLevel() == 0)
        {
            proc.levelUp();
            startLevelUp();
        }
    }
}

void EnchantrEditor::openChooser()
{
    chooser = std::make_unique<juce::FileChooser> ("Pick a one-shot", juce::File(),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<EnchantrEditor> safe (this);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f.existsAsFile())
                                  safe->loadFile (f);
                          });
}
