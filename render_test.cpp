// Renders the three screens to PPM files so the pixel art can be checked without JUCE.
// Build: g++ -std=c++17 -O1 -I../Source render_test.cpp ../Source/EnchantrArt.cpp -o render_test
#include "EnchantrArt.h"
#include <cmath>
#include <cstdio>
#include <string>

using namespace ench;

static void save (const Canvas& c, const char* path)
{
    FILE* f = std::fopen (path, "wb");
    std::fprintf (f, "P6\n%d %d\n255\n", c.w, c.h);
    for (uint32_t px : c.d)
    {
        const unsigned a = px >> 24;
        const unsigned r = a ? (px >> 16) & 255 : 0, g = a ? (px >> 8) & 255 : 0, b = a ? px & 255 : 0;
        std::fputc ((int) r, f); std::fputc ((int) g, f); std::fputc ((int) b, f);
    }
    std::fclose (f);
}

int main()
{
    Art art;
    Canvas out;
    UiState s;
    s.timeSec = 3.0; s.frame = 20; s.peak = 0.3f; s.level = 3; s.levelName = "NOVICE";
    s.hasSample = true; s.sampleName = "BELL ONE SHOT 01"; s.presetName = "SPARKLE RAIN";
    for (int i = 0; i < 43; ++i)
        s.bars[i] = 0.1f + 0.8f * std::exp (-std::pow ((i - 18) / 5.0, 2)) * (0.6f + 0.4f * std::sin (i * 1.7f));
    const float vals[21] = { 0.9f, 0.3f, 0.5f, 0.4f, 0.5f, 0.3f, 0.2f, 0.45f, 0.55f, 0.6f, 0.5f, 0.4f, 0.65f, 0.7f, 0.35f, 0.6f, 0.1f, 0.4f, 0.5f, 0.8f, 0.75f };
    for (int i = 0; i < 21; ++i) s.knob[i] = vals[i];
    const char* names[] = { "SPARKLE RAIN", "MOON TAPE", "FAIRY DUST", "WARM CHIME", "DEEP FOREST", "PIXIE POP", "MY BELL WITH A VERY LONG NAME" };
    for (int i = 0; i < 7; ++i) s.presets.push_back ({ names[i], 2 + i % 4, i < 6 });
    s.selected = 1;

    s.tab = Tab::Main;
    art.render (out, s); save (out, "main.ppm");

    s.castT = 0.15f; s.par = 0.5f;
    s.sparks.push_back ({ 100, 60, 0, 0, 1, "#f6d36a"_c, true });
    art.render (out, s); save (out, "cast.ppm");
    s.castT = -1.f; s.sparks.clear();

    s.tab = Tab::Tweak; s.hoverKnob = 5; s.hint = "CHORUS 0.30";
    art.render (out, s); save (out, "tweak.ppm");

    s.tab = Tab::Presets; s.message = "SAVED!"; s.hint.clear();
    art.render (out, s); save (out, "presets.ppm");

    s.message.clear(); s.hasSample = false;
    s.tab = Tab::Main; s.dragOver = true; s.frame = 0; s.levelPressed = true;
    art.render (out, s); save (out, "empty.ppm");
    std::puts ("ok");
}
