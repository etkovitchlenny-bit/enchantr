#pragma once
#include "PluginProcessor.h"
#include "EnchantrArt.h"
#include <vector>

//==============================================================================
// The whole UI is drawn by ench::Art into a 200x280 pixel framebuffer, which is
// scaled up with nearest-neighbour so every pixel stays crisp.
class EnchantrEditor : public juce::AudioProcessorEditor,
                       public juce::FileDragAndDropTarget,
                       public juce::TooltipClient,
                       private juce::Timer
{
public:
    explicit EnchantrEditor (EnchantrProcessor&);
    ~EnchantrEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }

    juce::String getTooltip() override;

private:
    void timerCallback() override;
    void buildState();
    juce::Point<double> toLogical (const juce::MouseEvent&) const;
    int knobAt (juce::Point<double> p) const;
    juce::String knobHint (int index) const;

    void startLevelUp();
    void burst();
    void openChooser();
    void loadFile (const juce::File& file);
    void showMessage (const juce::String& m);

    void promptSave();
    void promptDelete();
    void stepPreset (int dir);
    void loadSelected();
    void ensureSelectedVisible();

    EnchantrProcessor& proc;
    ench::Art art;
    ench::Canvas canvas;
    ench::UiState ui;
    juce::Image image { juce::Image::ARGB, ench::kW, ench::kH, true };
    juce::TooltipWindow tooltipWindow { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::RangedAudioParameter* knobParams[ench::kNumKnobs] {};
    juce::Random rng;

    ench::Tab tab = ench::Tab::Main;
    bool dragOver = false, levelDown = false;
    int frame = 0;
    float peak = 0.f, par = 0.f, ptr = 0.f;
    double lastMoveMs = -1e9, castStartMs = -1.0;
    float castT = -1.f;
    std::vector<ench::Spark> sparks;

    int hoverKnob = -1, dragKnob = -1;
    double dragStartY = 0.0, dragStartVal = 0.0;
    juce::Point<double> lastLogical;

    int selected = -1, scroll = 0;
    juce::String message;
    int messageFrames = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnchantrEditor)
};
