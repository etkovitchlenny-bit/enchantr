#pragma once
// Pixel-art renderer for Enchantr. Pure C++ (std only) so it can be tested without JUCE.
// Everything is drawn into a 200x280 framebuffer that the editor scales up with nearest-neighbour.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ench
{
constexpr int kW = 200, kH = 280;

constexpr unsigned hexv (char c) { return c >= 'a' ? (unsigned) (c - 'a' + 10) : c >= 'A' ? (unsigned) (c - 'A' + 10) : (unsigned) (c - '0'); }
constexpr uint32_t operator""_c (const char* s, std::size_t)
{
    return 0xff000000u | (hexv (s[1]) << 20) | (hexv (s[2]) << 16) | (hexv (s[3]) << 12)
           | (hexv (s[4]) << 8) | (hexv (s[5]) << 4) | hexv (s[6]);
}

struct Canvas
{
    int w = 0, h = 0;
    std::vector<uint32_t> d; // 0xAARRGGBB, not premultiplied
    Canvas() = default;
    Canvas (int W, int H) : w (W), h (H), d ((size_t) W * (size_t) H, 0u) {}
};

enum class Tab { Main, Tweak, Presets };

struct KnobDef
{
    const char* id;    // parameter id
    const char* label; // pixel-font label
    int pal;           // 0 = black/white, 1.. = colours
    int x, y;
    Tab tab;
};
extern const KnobDef kKnobs[];
constexpr int kNumKnobs = 21;

struct Spark { float x, y, vx, vy, life; uint32_t col; bool big; };
struct PresetRow { std::string name; int level; bool factory; };

struct UiState
{
    Tab tab = Tab::Main;
    double timeSec = 0.0;
    int frame = 0;
    float peak = 0.f;
    float castT = -1.f; // <0 = not casting
    float par = 0.f;    // parallax -1..1
    bool levelPressed = false, dragOver = false;
    float bars[43] {};
    std::vector<Spark> sparks;
    float knob[32] {};  // normalised value per kKnobs entry
    int hoverKnob = -1;
    std::string hint;
    int level = 0;
    std::string levelName, presetName, sampleName;
    bool hasSample = false;
    std::vector<PresetRow> presets;
    int selected = -1, scroll = 0;
    std::string message;
};

// geometry shared with the editor for hit-testing (all in 200x280 logical pixels)
namespace layout
{
struct Rect
{
    int x, y, w, h;
    bool contains (double px, double py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
constexpr Rect folder { 18, 58, 22, 17 }, floppy { 19, 80, 19, 19 };
constexpr Rect arrowL { 54, 37, 16, 14 }, arrowR { 130, 37, 16, 14 };
constexpr int tweakX = 34, tweakY = 250, tweakR = 16;
constexpr int presetsX = 166, presetsY = 250, presetsR = 16;
constexpr int levelX = 100, levelY = 246, levelR = 19;
constexpr int rowX = 19, rowY0 = 53, rowH = 17, rowW = 148, rowCount = 6;
constexpr Rect loadBtn { 16, 162, 50, 15 }, saveBtn { 70, 162, 50, 15 }, delBtn { 124, 162, 52, 15 };
constexpr Rect screenRect { 14, 34, 172, 170 };
} // namespace layout

class Art
{
public:
    Art();
    void render (Canvas& out, const UiState& s);

private:
    struct Impl;
    Impl* impl;

public:
    ~Art();
    Art (const Art&) = delete;
    Art& operator= (const Art&) = delete;
};
} // namespace ench
