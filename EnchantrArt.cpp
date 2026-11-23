#include "EnchantrArt.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <initializer_list>

namespace ench
{
//==============================================================================
const KnobDef kKnobs[] = {
    // main screen (black & white, next to the wizard)
    { "mix",       "MIX",     0, 28,  174, Tab::Main },
    { "glide",     "GLIDE",   0, 138, 174, Tab::Main },
    { "vibrato",   "VIBRATO", 0, 168, 174, Tab::Main },
    // tweak: effects
    { "sat",       "SAT",     1, 28,  69,  Tab::Tweak },
    { "tape",      "TAPE",    2, 57,  69,  Tab::Tweak },
    { "chorus",    "CHORUS",  3, 86,  69,  Tab::Tweak },
    { "trem",      "TREM",    4, 115, 69,  Tab::Tweak },
    { "delay",     "ECHO",    5, 144, 69,  Tab::Tweak },
    { "reverb",    "VERB",    6, 173, 69,  Tab::Tweak },
    // tweak: tone
    { "eqLow",     "LOW",     7, 28,  107, Tab::Tweak },
    { "eqMid",     "MID",     1, 57,  107, Tab::Tweak },
    { "eqMidFreq", "FREQ",    2, 86,  107, Tab::Tweak },
    { "eqHigh",    "HIGH",    3, 115, 107, Tab::Tweak },
    { "warmth",    "WARM",    4, 144, 107, Tab::Tweak },
    { "air",       "AIR",     5, 173, 107, Tab::Tweak },
    // tweak: output
    { "width",     "WIDTH",   6, 28,  145, Tab::Tweak },
    { "attack",    "ATK",     7, 57,  145, Tab::Tweak },
    { "release",   "REL",     1, 86,  145, Tab::Tweak },
    { "root",      "ROOT",    2, 115, 145, Tab::Tweak },
    { "mix",       "MIX",     3, 144, 145, Tab::Tweak },
    { "out",       "OUT",     4, 173, 145, Tab::Tweak },
};
static_assert ((int) (sizeof (kKnobs) / sizeof (kKnobs[0])) == kNumKnobs, "kNumKnobs must match the kKnobs table");

namespace
{
constexpr int SW = 172, SH = 170, SX = 14, SY = 34, M = 8, LW = SW + 2 * M;

inline int rnd (double v) { return (int) std::floor (v + 0.5); }

//==============================================================================
const uint32_t DARK = "#14102a"_c, DARK2 = "#241d4a"_c;
const uint32_t GOLD = "#f6d36a"_c, GOLD_L = "#fff0a8"_c, GOLD_D = "#d9a63a"_c;
const uint32_t WHITE = "#ffffff"_c;

struct Pal { uint32_t c[3]; uint32_t t; };
const Pal PAL_BW    { { "#ffffff"_c, "#c4c4c4"_c, "#5c5c5c"_c }, "#241d4a"_c };
const Pal PAL_ROSE  { { "#ffc4d4"_c, "#ff7fa0"_c, "#c24566"_c }, "#7a1f3b"_c };
const Pal PAL_SUN   { { "#fff0a8"_c, "#f6d36a"_c, "#c9962c"_c }, "#7a5410"_c };
const Pal PAL_AQUA  { { "#c8fff0"_c, "#5fe0c0"_c, "#2e9e86"_c }, "#15594a"_c };
const Pal PAL_LIL   { { "#e6d4ff"_c, "#b48cf0"_c, "#7a52c0"_c }, "#40206e"_c };
const Pal PAL_SKY   { { "#d0ecff"_c, "#6cb8f0"_c, "#3a7ac0"_c }, "#16406e"_c };
const Pal PAL_MINT  { { "#d8ffc8"_c, "#82e068"_c, "#3f9a40"_c }, "#1d5a22"_c };
const Pal PAL_PEACH { { "#ffe0c8"_c, "#ffa56c"_c, "#c86a30"_c }, "#6e3010"_c };
const Pal PAL_GREY  { { "#d0d0e0"_c, "#a4a4c0"_c, "#74748e"_c }, "#3a3a58"_c };

const Pal& palFor (int i)
{
    switch (i)
    {
        case 1: return PAL_ROSE;
        case 2: return PAL_SUN;
        case 3: return PAL_AQUA;
        case 4: return PAL_LIL;
        case 5: return PAL_SKY;
        case 6: return PAL_MINT;
        case 7: return PAL_PEACH;
        default: return PAL_BW;
    }
}

const uint32_t CP[5]   = { "#b99ad0"_c, "#e7a9c0"_c, "#ffd0c8"_c, "#fff1d2"_c, "#ffffff"_c };
const uint32_t FAR1[5] = { "#7fb0b4"_c, "#92c0bc"_c, "#a6cfc4"_c, "#bddfce"_c, "#d6efdb"_c };
const uint32_t FAR2[5] = { "#5e9c92"_c, "#70ae9c"_c, "#86c2a6"_c, "#a3d8b4"_c, "#c4eec6"_c };
const uint32_t MID[5]  = { "#1f5c3c"_c, "#2f7f4a"_c, "#48a455"_c, "#7fd163"_c, "#c2f088"_c };
const uint32_t GP1[5]  = { "#10352c"_c, "#1d5a3a"_c, "#2f8a48"_c, "#5cbc58"_c, "#a6e676"_c };
const uint32_t GP2[5]  = { "#12402f"_c, "#216b3d"_c, "#37a04b"_c, "#6ccf5c"_c, "#b9f27c"_c };
const uint32_t BARK[5] = { "#3a2418"_c, "#5a3a26"_c, "#8a5a38"_c, "#b9814f"_c, "#e0a86c"_c };
const uint32_t SKIN[3] = { "#ffe6cc"_c, "#ffd9b8"_c, "#f0b894"_c };

const int B4[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
inline bool dith (int x, int y, double f) { return (B4[y & 3][x & 3] + 0.5) / 16.0 < f; }

inline double hash2 (int x, int y)
{
    uint32_t h = ((uint32_t) x * 374761393u) ^ ((uint32_t) y * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return (double) (h ^ (h >> 16)) / 4294967296.0;
}

struct Mulberry
{
    uint32_t a;
    explicit Mulberry (uint32_t seed) : a (seed) {}
    double operator() ()
    {
        a += 0x6D2B79F5u;
        uint32_t t = (a ^ (a >> 15)) * (1u | a);
        t = (t + (t ^ (t >> 7)) * (61u | t)) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
};

//==============================================================================
// 3x5 pixel font
struct Glyph { char ch; const char* r[5]; };
const Glyph kFont[] = {
    { 'A', { ".#.", "#.#", "###", "#.#", "#.#" } }, { 'B', { "##.", "#.#", "##.", "#.#", "##." } },
    { 'C', { ".##", "#..", "#..", "#..", ".##" } }, { 'D', { "##.", "#.#", "#.#", "#.#", "##." } },
    { 'E', { "###", "#..", "##.", "#..", "###" } }, { 'F', { "###", "#..", "##.", "#..", "#.." } },
    { 'G', { ".##", "#..", "#.#", "#.#", ".##" } }, { 'H', { "#.#", "#.#", "###", "#.#", "#.#" } },
    { 'I', { "###", ".#.", ".#.", ".#.", "###" } }, { 'J', { "..#", "..#", "..#", "#.#", ".#." } },
    { 'K', { "#.#", "#.#", "##.", "#.#", "#.#" } }, { 'L', { "#..", "#..", "#..", "#..", "###" } },
    { 'M', { "#.#", "###", "###", "#.#", "#.#" } }, { 'N', { "##.", "#.#", "#.#", "#.#", "#.#" } },
    { 'O', { ".#.", "#.#", "#.#", "#.#", ".#." } }, { 'P', { "##.", "#.#", "##.", "#..", "#.." } },
    { 'Q', { ".#.", "#.#", "#.#", "##.", ".##" } }, { 'R', { "##.", "#.#", "##.", "#.#", "#.#" } },
    { 'S', { ".##", "#..", ".#.", "..#", "##." } }, { 'T', { "###", ".#.", ".#.", ".#.", ".#." } },
    { 'U', { "#.#", "#.#", "#.#", "#.#", "###" } }, { 'V', { "#.#", "#.#", "#.#", "#.#", ".#." } },
    { 'W', { "#.#", "#.#", "###", "###", "#.#" } }, { 'X', { "#.#", "#.#", ".#.", "#.#", "#.#" } },
    { 'Y', { "#.#", "#.#", ".#.", ".#.", ".#." } }, { 'Z', { "###", "..#", ".#.", "#..", "###" } },
    { '0', { "###", "#.#", "#.#", "#.#", "###" } }, { '1', { ".#.", "##.", ".#.", ".#.", "###" } },
    { '2', { "##.", "..#", ".#.", "#..", "###" } }, { '3', { "##.", "..#", ".#.", "..#", "##." } },
    { '4', { "#.#", "#.#", "###", "..#", "..#" } }, { '5', { "###", "#..", "##.", "..#", "##." } },
    { '6', { ".##", "#..", "###", "#.#", "###" } }, { '7', { "###", "..#", ".#.", ".#.", ".#." } },
    { '8', { "###", "#.#", "###", "#.#", "###" } }, { '9', { "###", "#.#", "###", "..#", "##." } },
    { '-', { "...", "...", "###", "...", "..." } }, { '?', { "##.", "..#", ".#.", "...", ".#." } },
    { '!', { ".#.", ".#.", ".#.", "...", ".#." } }, { '.', { "...", "...", "...", "...", ".#." } },
    { ':', { "...", ".#.", "...", ".#.", "..." } }, { '#', { "#.#", "###", "#.#", "###", "#.#" } },
    { '+', { "...", ".#.", "###", ".#.", "..." } }, { '%', { "#.#", "..#", ".#.", "#..", "#.#" } },
};

const Glyph* findGlyph (char c)
{
    c = (char) std::toupper ((unsigned char) c);
    for (const auto& g : kFont)
        if (g.ch == c)
            return &g;
    return nullptr;
}

// 5x7 title font (only the letters of the plugin name)
struct TGlyph { char ch; const char* r[7]; };
const TGlyph kTitle[] = {
    { 'E', { "#####", "#....", "#....", "####.", "#....", "#....", "#####" } },
    { 'N', { "#...#", "##..#", "##..#", "#.#.#", "#..##", "#..##", "#...#" } },
    { 'C', { ".####", "#....", "#....", "#....", "#....", "#....", ".####" } },
    { 'H', { "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } },
    { 'A', { ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } },
    { 'T', { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." } },
    { 'R', { "####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#" } },
};

//==============================================================================
struct Painter
{
    Canvas* g = nullptr;
    int ox = 0;
    double ga = 1.0;

    void use (Canvas& c, int offx = 0) { g = &c; ox = offx; ga = 1.0; }

    static uint32_t over (uint32_t dst, uint32_t src, double a)
    {
        const double sa = ((src >> 24) / 255.0) * a, da = (dst >> 24) / 255.0;
        const double oa = sa + da * (1.0 - sa);
        if (oa <= 0.0)
            return 0u;
        auto ch = [&] (int sh)
        {
            const double sc = (double) ((src >> sh) & 255), dc = (double) ((dst >> sh) & 255);
            return (uint32_t) std::lround (std::min (255.0, (sc * sa + dc * da * (1.0 - sa)) / oa));
        };
        return ((uint32_t) std::lround (oa * 255.0) << 24) | (ch (16) << 16) | (ch (8) << 8) | ch (0);
    }

    void Pi (int x, int y, uint32_t c)
    {
        x += ox;
        if (x < 0 || y < 0 || x >= g->w || y >= g->h)
            return;
        uint32_t& d = g->d[(size_t) y * (size_t) g->w + (size_t) x];
        if (ga >= 0.999 && (c >> 24) == 255)
            d = c;
        else
            d = over (d, c, ga);
    }
    void P (double x, double y, uint32_t c) { Pi (rnd (x), rnd (y), c); }
    void R (double x, double y, double w, double h, uint32_t c)
    {
        const int X = rnd (x), Y = rnd (y), Wd = rnd (w), Ht = rnd (h);
        for (int j = 0; j < Ht; ++j)
            for (int i = 0; i < Wd; ++i)
                Pi (X + i, Y + j, c);
    }
    void clearRect (int x, int y, int w, int h)
    {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
            {
                const int px = x + i + ox, py = y + j;
                if (px >= 0 && py >= 0 && px < g->w && py < g->h)
                    g->d[(size_t) py * (size_t) g->w + (size_t) px] = 0u;
            }
    }

    static int halfW (int r, int y) { return (int) std::floor (std::sqrt ((double) (r * r - y * y) + r * 0.5)); }

    void disc (int cx, int cy, int r, uint32_t c)
    {
        for (int y = -r; y <= r; ++y)
        {
            const int w = halfW (r, y);
            R (cx - w, cy + y, w * 2 + 1, 1, c);
        }
    }
    void sdisc (int cx, int cy, int r, const uint32_t* c)
    {
        for (int y = -r; y <= r; ++y)
        {
            const int w = halfW (r, y);
            for (int x = -w; x <= w; ++x)
            {
                const double v = (x - y) / (r * 1.5);
                P (cx + x, cy + y, v > 0.45 ? c[0] : (v < -0.25 ? c[2] : c[1]));
            }
        }
    }
    void ddisc (int cx, int cy, int r, uint32_t c)
    {
        for (int y = -r; y <= r; ++y)
        {
            const int w = halfW (r, y);
            for (int x = -w; x <= w; ++x)
                if (((cx + x + cy + y) & 1) == 0)
                    P (cx + x, cy + y, c);
        }
    }
    void dell (int cx, int cy, double rx, double ry, uint32_t c, double dens)
    {
        const int iry = (int) ry;
        for (int y = -iry; y <= iry; ++y)
        {
            const int w = (int) std::floor (rx * std::sqrt (std::max (0.0, 1.0 - (y * y) / (ry * ry + 0.01))));
            for (int x = -w; x <= w; ++x)
                if (dith (cx + x, cy + y, dens))
                    P (cx + x, cy + y, c);
        }
    }
    static int rrIns (int j, int h, int r)
    {
        int k = j;
        if (j >= h - r)
            k = h - 1 - j;
        if (k >= r)
            return 0;
        const double d = r - k - 0.5;
        return rnd (r - std::sqrt (std::max (0.0, (double) r * r - d * d)));
    }
    void rrect (int x, int y, int w, int h, int r, uint32_t c)
    {
        for (int j = 0; j < h; ++j)
        {
            const int i = rrIns (j, h, r);
            R (x + i, y + j, w - 2 * i, 1, c);
        }
    }
    void vgrad (int x, int y, int w, int h, std::initializer_list<uint32_t> colsIn, int rr = 0)
    {
        const std::vector<uint32_t> cols (colsIn);
        const int n = (int) cols.size() - 1;
        for (int j = 0; j < h; ++j)
        {
            const double t = ((double) j / std::max (1, h - 1)) * n;
            const int i = std::min (n - 1, (int) std::floor (t));
            const double f = t - i;
            const int ins = rr ? rrIns (j, h, rr) : 0;
            for (int k = ins; k < w - ins; ++k)
                P (x + k, y + j, cols[(size_t) (i + (dith (x + k, y + j, f) ? 1 : 0))]);
        }
    }
    void tri (int cx, int top, int bot, int hw, uint32_t cl, uint32_t cr)
    {
        for (int y = top; y <= bot; ++y)
        {
            const double t = (double) (y - top) / std::max (1, bot - top);
            const int w = rnd (t * hw);
            if (w > 0) { R (cx - w, y, w, 1, cl); R (cx, y, w + 1, 1, cr); }
            else P (cx, y, cr);
        }
    }
    void line (double x0, double y0, double x1, double y1, int wd, uint32_t c)
    {
        const int n = (int) std::max (std::fabs (x1 - x0), std::fabs (y1 - y0));
        if (n == 0) { R (x0, y0, wd, wd, c); return; }
        for (int i = 0; i <= n; ++i)
            R (x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, wd, wd, c);
    }

    //--------------------------------------------------------------------------
    static int tw (const std::string& s, int k = 1) { return (int) s.size() * 4 * k - k; }
    void txt (const std::string& s, int x, int y, uint32_t c, int k = 1)
    {
        int cx = x;
        for (char ch : s)
        {
            if (const Glyph* gl = findGlyph (ch))
                for (int r = 0; r < 5; ++r)
                    for (int q = 0; q < 3; ++q)
                        if (gl->r[r][q] == '#')
                            R (cx + q * k, y + r * k, k, k, c);
            cx += 4 * k;
        }
    }
    void txtO (const std::string& s, double cx, int y, uint32_t c, uint32_t o, int k = 1)
    {
        static const int off[8][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
        const int x = rnd (cx - tw (s, k) / 2.0);
        for (auto& d : off)
            txt (s, x + d[0], y + d[1], o, k);
        txt (s, x, y, c, k);
    }
    void title3d (const std::string& s, double cx, int y, int k, int ext)
    {
        static const int off[8][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
        const int x = rnd (cx - tw (s, k) / 2.0);
        for (int d = ext; d >= 1; --d)
            txt (s, x + d, y + d, "#262a63"_c, k);
        for (auto& d : off)
            txt (s, x + d[0], y + d[1], DARK, k);
        txt (s, x, y, GOLD_L, k);
        txt (s, x, y + 1, GOLD, k);
    }

    // large, easy-to-read title in the 5x7 font
    void bigTitle (const std::string& s, double cx, int y, int k)
    {
        const int n = (int) s.size();
        const int totalW = n * 6 * k - k;
        const int x0 = rnd (cx - totalW / 2.0);
        auto each = [&] (int dx, int dy, auto&& colourForRow)
        {
            for (int i = 0; i < n; ++i)
            {
                const TGlyph* gl = nullptr;
                for (const auto& t : kTitle)
                    if (t.ch == s[(size_t) i])
                        gl = &t;
                if (! gl)
                    continue;
                for (int r = 0; r < 7; ++r)
                    for (int q = 0; q < 5; ++q)
                        if (gl->r[r][q] == '#')
                            R (x0 + i * 6 * k + q * k + dx, y + r * k + dy, k, k, colourForRow (r));
            }
        };
        for (int d = 3; d >= 1; --d)
            each (d, d, [] (int) { return "#262a63"_c; });
        for (int oy = -1; oy <= 1; ++oy)
            for (int oxx = -1; oxx <= 1; ++oxx)
                if (oxx || oy)
                    each (oxx * (k > 1 ? 1 : 1), oy, [] (int) { return DARK; });
        each (0, 0, [] (int r) { return r < 3 ? "#fffbe8"_c : r < 5 ? "#ffe9a0"_c : GOLD; });
    }

    //--------------------------------------------------------------------------
    void canopy (int cx, int cy, double rx, double ry, const uint32_t* pal, uint32_t seed, int n,
                 double rmin, double rmax, int spk)
    {
        Mulberry rng (seed);
        struct Cl { int x, y; double r; };
        std::vector<Cl> cl;
        for (int i = 0; i < n; ++i)
        {
            const double t = rng() * 6.283, d = std::sqrt (rng());
            Cl c;
            c.x = rnd (cx + std::cos (t) * d * rx);
            c.y = rnd (cy + std::sin (t) * d * ry);
            c.r = rmin + rng() * (rmax - rmin);
            cl.push_back (c);
        }
        std::stable_sort (cl.begin(), cl.end(), [] (const Cl& a, const Cl& b) { return a.y < b.y; });
        for (const auto& c : cl)
        {
            const double r = c.r;
            const int ri = (int) std::ceil (r) + 1;
            for (int dy = -ri; dy <= ri; ++dy)
            {
                const double jr = r + (rng() - 0.5) * 1.3;
                const int w = (int) std::floor (std::sqrt (std::max (0.0, jr * jr - (double) dy * dy)));
                for (int dx = -w; dx <= w; ++dx)
                {
                    const int px = c.x + dx, py = c.y + dy;
                    double v = (dx * 0.55 + dy * 0.85) / r;
                    v += (B4[py & 3][px & 3] / 16.0 - 0.47) * 0.42;
                    const int ti = v < -0.62 ? 4 : v < -0.2 ? 3 : v < 0.28 ? 2 : v < 0.66 ? 1 : 0;
                    P (px, py, pal[ti]);
                }
            }
            const int k = spk ? spk : 4;
            for (int i = 0; i < k; ++i)
            {
                const double a = 3.14 + rng() * 1.7;
                P (c.x + std::cos (a) * (r + 1), c.y + std::sin (a) * (r + 1), pal[3 + (rng() > 0.6 ? 1 : 0)]);
            }
            for (int i = 0; i < (int) std::ceil (k / 2.0); ++i)
            {
                const double a = rng() * 1.6;
                P (c.x + std::cos (a) * (r + 1), c.y + std::sin (a) * (r + 1), pal[0]);
            }
        }
    }

    void trunk (int cx, int top, int bot, double wTop, double wBot, const uint32_t* pal, double seed, double flare)
    {
        for (int y = top; y <= bot; ++y)
        {
            const double t = (double) (y - top) / (bot - top);
            double w = wTop + (wBot - wTop) * t;
            if (flare > 0 && t > 0.82)
                w += flare * std::pow ((t - 0.82) / 0.18, 2);
            const int left = rnd (cx + std::sin (y * 0.06 + seed) * 1.2 - w / 2);
            const int W2 = std::max (2, rnd (w));
            for (int x = 0; x < W2; ++x)
            {
                const double u = (double) x / (W2 - 1);
                int ti = u < 0.16 ? 4 : u < 0.42 ? 3 : u < 0.7 ? 2 : u < 0.88 ? 1 : 0;
                const double hh = hash2 (left + x, y >> 1);
                if (hh < 0.16 && ti > 0) ti--;
                else if (hh > 0.93 && ti < 4) ti++;
                if ((x + ((y >> 3) & 1) * 3) % 5 == 0 && hash2 (x, y >> 2) < 0.5 && ti > 0)
                    ti--;
                P (left + x, y, pal[ti]);
                if (u < 0.3 && hash2 (left + x, y) > 0.985)
                    P (left + x, y, "#8fd36a"_c);
            }
        }
        if (flare > 0)
        {
            const int half = rnd ((wBot + flare) / 2);
            line (cx - half + 3, bot - 6, cx - half - 3, bot + 1, 3, pal[2]);
            line (cx + half - 4, bot - 6, cx + half + 2, bot + 1, 3, pal[0]);
        }
    }

    void castle (int cx, int by)
    {
        R (cx - 13, by - 16, 26, 16, "#cdc7de"_c); R (cx + 5, by - 16, 8, 16, "#aaa3c8"_c); R (cx - 13, by - 16, 2, 16, "#e8e3f4"_c);
        for (int i = 0; i < 7; ++i) R (cx - 13 + i * 4, by - 18, 2, 2, "#cdc7de"_c);
        R (cx - 4, by - 34, 9, 34, "#d3cde3"_c); R (cx + 1, by - 34, 4, 34, "#aea7ca"_c); R (cx - 4, by - 34, 1, 34, "#eee9f7"_c);
        tri (cx, by - 46, by - 34, 6, "#e8808c"_c, "#c4566a"_c); R (cx - 6, by - 35, 13, 1, "#8c4a62"_c);
        R (cx, by - 53, 1, 7, WHITE); R (cx + 1, by - 53, 4, 2, "#ffd25a"_c);
        R (cx - 17, by - 26, 7, 26, "#d0cadf"_c); R (cx - 13, by - 26, 3, 26, "#aea7ca"_c); tri (cx - 14, by - 34, by - 26, 5, "#e8808c"_c, "#c4566a"_c);
        R (cx + 9, by - 24, 7, 24, "#d0cadf"_c); R (cx + 13, by - 24, 3, 24, "#aea7ca"_c); tri (cx + 12, by - 32, by - 24, 5, "#e8808c"_c, "#c4566a"_c);
        const int win[6][2] = { { cx - 1, by - 28 }, { cx - 1, by - 21 }, { cx - 15, by - 20 }, { cx + 11, by - 18 }, { cx - 8, by - 10 }, { cx + 6, by - 10 } };
        for (int i = 0; i < 6; ++i) R (win[i][0], win[i][1], 1, 2, i % 3 == 0 ? "#ffe9a0"_c : "#5d5584"_c);
        R (cx - 2, by - 6, 5, 6, "#4b4470"_c); P (cx - 1, by - 7, "#4b4470"_c); P (cx + 1, by - 7, "#4b4470"_c);
        ga = 0.18;
        for (int y = by - 54; y <= by; ++y)
            for (int x = cx - 18; x <= cx + 17; ++x)
                if (dith (x, y, 0.5))
                    P (x, y, "#fff0e6"_c);
        ga = 1.0;
    }
};

double pathC (int y) { return 86 - (y - 140) * 1.5 + std::sin (y * 0.15) * 4; }
double pathW (int y) { return 8 + (y - 140) * 0.5; }

void blit (Canvas& dst, const Canvas& src, int dx, int dy, double alpha)
{
    Painter p;
    p.use (dst);
    for (int y = 0; y < src.h; ++y)
        for (int x = 0; x < src.w; ++x)
        {
            const uint32_t c = src.d[(size_t) y * (size_t) src.w + (size_t) x];
            if ((c >> 24) == 0)
                continue;
            const int X = x + dx, Y = y + dy;
            if (X < 0 || Y < 0 || X >= dst.w || Y >= dst.h)
                continue;
            uint32_t& d = dst.d[(size_t) Y * (size_t) dst.w + (size_t) X];
            if (alpha >= 0.999 && (c >> 24) == 255)
                d = c;
            else
                d = Painter::over (d, c, alpha);
        }
}
} // namespace

//==============================================================================
struct Art::Impl
{
    Painter p;
    Canvas shell { kW, kH };
    Canvas panel { 176, 174 };
    Canvas lSky { LW, SH }, lFar { LW, SH }, lGround { LW, SH }, lFx { LW, SH }, lFg { LW, SH };
    Canvas scr { SW, SH };

    Impl()
    {
        buildSky(); buildFar(); buildGround(); buildFx(); buildFg();
        buildShell(); buildPanel();
    }

    //--------------------------------------------------------------------------
    void buildSky()
    {
        p.use (lSky, M);
        p.vgrad (-M, 0, LW, 104, { "#68b8f8"_c, "#80c6fa"_c, "#9ed4fb"_c, "#bde3fb"_c, "#dbeaf2"_c, "#f3ecdf"_c, "#ffeccb"_c });
        p.ddisc (92, 28, 22, "#fff6c8"_c); p.ddisc (92, 28, 17, "#fff2b0"_c); p.disc (92, 28, 12, "#ffeea0"_c);
        const uint32_t sun[3] = { WHITE, "#fffbe0"_c, "#fff3b0"_c };
        p.sdisc (92, 28, 8, sun);
        p.canopy (66, 58, 30, 6, CP, 11, 18, 4, 8, 4);
        p.canopy (138, 60, 24, 6, CP, 12, 14, 4, 8, 4);
        p.canopy (30, 78, 20, 4, CP, 13, 10, 3, 6, 3);
        p.canopy (150, 30, 16, 4, CP, 14, 8, 3, 6, 3);
        const uint32_t bd = "#3a3340"_c;
        p.P (124, 22, bd); p.P (125, 23, bd); p.P (126, 22, bd); p.P (127, 23, bd); p.P (128, 22, bd);
        p.P (106, 34, bd); p.P (107, 35, bd); p.P (108, 34, bd);
    }

    void buildFar()
    {
        p.use (lFar, M);
        for (int x = -M; x < SW + M; ++x)
        {
            const int yH = 84 + rnd (4 * std::sin (x * 0.05 + 1) + 2 * std::sin (x * 0.13));
            p.R (x, yH, 1, 30, "#b3d6c8"_c);
        }
        p.dell (112, 96, 34, 9, "#9fd0a8"_c, 1); p.dell (112, 96, 34, 9, "#b4e0b4"_c, 0.4);
        p.castle (112, 96);
        for (int x = -10; x < SW + 14; x += 10)
        {
            if (x > 92 && x < 134) continue;
            p.canopy (x, 84 + rnd (hash2 (x, 3) * 4), 9, 6, FAR1, (uint32_t) (100 + x), 5, 3, 6, 3);
        }
        for (int x = -6; x < SW + 14; x += 8)
            p.canopy (x, 92 + rnd (hash2 (x, 5) * 3), 8, 5, FAR2, (uint32_t) (200 + x), 5, 3, 5, 2);
    }

    void buildGround()
    {
        p.use (lGround, M);
        const uint32_t mc[5] = { "#c3e68f"_c, "#add97f"_c, "#94cc70"_c, "#7dbd62"_c, "#66aa52"_c };
        for (int x = -M; x < SW + M; ++x)
        {
            const int y0 = 97 + rnd (2 * std::sin (x * 0.06) + 1.5 * std::sin (x * 0.15 + 1));
            for (int y = y0; y < SH; ++y)
            {
                const double t = std::min (3.99, ((double) (y - 96) / (SH - 96)) * 4);
                const int i = (int) std::floor (t);
                p.P (x, y, mc[i + (dith (x, y, t - i) ? 1 : 0)]);
            }
        }
        p.dell (60, 112, 40, 5, "#d8f4a6"_c, 0.5); p.dell (120, 126, 34, 5, "#d8f4a6"_c, 0.45); p.dell (86, 150, 50, 6, "#d8f4a6"_c, 0.35);
        p.dell (52, 108, 22, 4, "#2e7b46"_c, 0.6); p.dell (146, 104, 20, 4, "#2e7b46"_c, 0.6); p.dell (30, 134, 34, 7, "#2a6e40"_c, 0.45);

        for (int y = 134; y < SH; ++y)
        {
            const double c = pathC (y), w = pathW (y);
            for (int x = (int) std::floor (c - w / 2 - 1); x <= (int) std::ceil (c + w / 2 + 1); ++x)
            {
                const double u = std::fabs (x - c) / (w / 2);
                if (u > 1.12) continue;
                if (u > 0.95 && ! dith (x, y, 0.5)) continue;
                uint32_t col = u < 0.5 ? "#d9b680"_c : u < 0.82 ? "#c59c60"_c : "#92723f"_c;
                const double hh = hash2 (x, y);
                if (hh < 0.05) col = "#f0e2c0"_c;
                else if (hh > 0.96) col = "#8a6a3a"_c;
                p.P (x, y, col);
            }
        }
        // pond
        const int px = 142, py = 121;
        for (int y = -4; y <= 4; ++y)
        {
            const int w = (int) std::floor (14 * std::sqrt (std::max (0.0, 1.0 - (y * y) / 16.0)));
            for (int x = -w; x <= w; ++x)
            {
                const bool edge = x == -w || x == w || y == -4;
                uint32_t col = y < 0 ? "#6fb4e8"_c : "#a4d6f6"_c;
                if (edge) col = "#3d7a9a"_c;
                else if (hash2 (px + x, py + y) < 0.1) col = WHITE;
                p.P (px + x, py + y, col);
            }
        }
        for (int i = 0; i < 5; ++i)
        {
            p.R (px - 14 + i * 7, py - 8 - (i % 2) * 2, 1, 5, "#3f8a3a"_c);
            p.P (px - 14 + i * 7, py - 9 - (i % 2) * 2, "#b8f080"_c);
        }
        // mid trees
        const int midT[2][3] = { { 50, 106, 21 }, { 154, 102, 22 } };
        for (auto& m : midT)
        {
            p.trunk (m[0], m[1] - 22, m[1], 4, 6, BARK, m[2], 0);
            p.canopy (m[0], m[1] - 28, 14, 9, MID, (uint32_t) (m[2] * 3), 16, 4, 7, 4);
        }
        p.canopy (12, 114, 14, 5, MID, 31, 8, 3, 6, 3); p.canopy (162, 120, 12, 5, MID, 32, 8, 3, 6, 3);

        // flowers
        Mulberry rng (77);
        const uint32_t fc[6] = { "#ff8fb4"_c, WHITE, "#c4a8ff"_c, "#ffe36b"_c, "#ff6a6a"_c, "#8fd0ff"_c };
        for (int i = 0; i < 150; ++i)
        {
            const int x = rnd (rng() * SW);
            const int y = 100 + rnd (rng() * 68);
            const uint32_t c = fc[(int) std::floor (rng() * 6)];
            if (y > 134 && std::fabs (x - pathC (y)) < pathW (y) / 2 + 2) continue;
            if (y > 118 && x > 126 && x < 158 && y < 126) continue;
            if (y > 128)
            {
                p.P (x, y - 1, c); p.P (x - 1, y, c); p.P (x + 1, y, c); p.P (x, y + 1, c);
                p.P (x, y, "#ffe36b"_c); p.P (x, y + 2, "#3f8a3a"_c);
            }
            else p.P (x, y, c);
        }
        // grass tufts
        const uint32_t gt[3] = { "#4a9a44"_c, "#7fd45a"_c, "#b8f080"_c };
        for (int i = 0; i < 320; ++i)
        {
            const int x = rnd (rng() * LW) - M;
            const int y = 100 + rnd (rng() * 68);
            if (y > 134 && std::fabs (x - pathC (y)) < pathW (y) / 2) continue;
            const int h = 2 + (int) std::floor ((y - 96) / 22.0);
            for (int k = 0; k < h; ++k)
                p.P (x + (k > 2 ? 1 : 0), y - k, k == h - 1 ? gt[2] : gt[1]);
            p.P (x, y + 1, "#2f7a3a"_c);
        }
        // glow ring under the wizard
        p.dell (86, 138, 31, 6, "#fff6c0"_c, 0.5);
        for (double a = 0; a < 6.283; a += 0.03) p.P (86 + rnd (32 * std::cos (a)), 138 + rnd (7 * std::sin (a)), GOLD);
        for (double a = 0; a < 6.283; a += 0.05) p.P (86 + rnd (23 * std::cos (a)), 138 + rnd (5 * std::sin (a)), GOLD_D);
    }

    void buildFx()
    {
        p.use (lFx, M);
        const int rays[3][2] = { { 52, 12 }, { 76, 8 }, { 102, 14 } };
        for (auto& r : rays)
            for (int y = 0; y < 150; ++y)
            {
                const int x0 = rnd (r[0] + y * 0.5);
                for (int x = x0; x < x0 + r[1]; ++x)
                {
                    const double edge = std::min (x - x0, x0 + r[1] - 1 - x) / (r[1] / 2.0);
                    if (dith (x, y, 0.25 + edge * 0.5))
                        p.P (x, y, "#fff7c0"_c);
                }
            }
    }

    void buildFg()
    {
        p.use (lFg, M);
        p.trunk (36, 30, 126, 12, 14, BARK, 3, 12);
        p.line (36, 44, 14, 28, 3, BARK[1]); p.line (40, 40, 66, 30, 3, BARK[2]);
        p.canopy (28, 18, 56, 28, GP1, 7, 74, 6, 11, 6);
        p.canopy (20, 46, 28, 8, GP1, 8, 14, 4, 7, 5);
        p.trunk (162, 38, 128, 10, 12, BARK, 4, 9);
        p.line (160, 52, 140, 38, 3, BARK[1]);
        p.canopy (158, 16, 34, 24, GP2, 9, 50, 6, 10, 6);
        p.canopy (166, 40, 16, 7, GP2, 10, 8, 4, 7, 5);
        p.canopy (98, -6, 16, 8, GP2, 15, 10, 4, 7, 4);
        Mulberry rng (55);
        const uint32_t gb[4] = { "#2f7a3a"_c, "#4aa044"_c, "#7fd45a"_c, "#b8f080"_c };
        for (int i = 0; i < 190; ++i)
        {
            const int x = rnd (rng() * LW) - M;
            if (x > 62 && x < 112) continue;
            const int y = 150 + rnd (rng() * 22);
            const int h = 4 + (int) std::floor (rng() * 6);
            for (int k = 0; k < h; ++k)
            {
                const uint32_t c = k > h - 2 ? gb[3] : k > h / 2.0 ? gb[2] : k > 1 ? gb[1] : gb[0];
                p.P (x + (k > h - 3 ? 1 : 0), y - k, c);
                if (rng() > 0.7) p.P (x + 1, y - k, gb[1]);
            }
        }
        p.ga = 0.5;
        for (int y = 0; y < SH; ++y)
            for (int x = -M; x < SW + M; ++x)
            {
                const double dx = (x - 86) / 112.0, dy = (y - 85) / 108.0;
                const double d = std::sqrt (dx * dx + dy * dy);
                const double f = std::min (0.7, std::max (0.0, (d - 0.78) * 1.5));
                if (f > 0 && dith (x, y, f))
                    p.P (x, y, "#16322e"_c);
            }
        p.ga = 1.0;
    }

    //--------------------------------------------------------------------------
    void btn (int cx, int cy, int r, const Pal& pal, const char* label, bool pressed)
    {
        const int th = pressed ? 1 : 3;
        p.disc (cx, cy + 4, r + 1, "#2c3280"_c);
        p.disc (cx, cy + th, r + 1, DARK);
        p.disc (cx, cy + th, r, pal.c[2]);
        p.disc (cx, cy, r + 1, DARK);
        p.sdisc (cx, cy, r, pal.c);
        p.R (cx - 4, cy - r + 3, 5, 1, WHITE); p.R (cx - r + 3, cy - 4, 1, 3, WHITE);
        p.txtO (label, cx, cy - 3, WHITE, pal.t, 1);
    }

    void rbtn (int x, int y, int w, int h, const Pal& pal, const char* label)
    {
        p.rrect (x, y + 2, w, h, 3, DARK);
        p.rrect (x, y + 1, w, h, 3, pal.c[2]);
        p.rrect (x, y, w, h, 3, DARK);
        p.vgrad (x + 1, y + 1, w - 2, h - 2, { pal.c[0], pal.c[1] }, 2);
        p.txtO (label, x + w / 2.0, y + (h - 5) / 2, WHITE, pal.t, 1);
    }

    void knob (int cx, int cy, int r, const Pal& pal, double pos, const char* label, uint32_t shadow, uint32_t tickOff, bool hover)
    {
        p.disc (cx + 1, cy + 3, r + 1, shadow);
        p.disc (cx, cy + 2, r + 1, DARK);
        p.disc (cx, cy + 2, r, pal.c[2]);
        p.disc (cx, cy, r + 1, hover ? WHITE : DARK);
        p.disc (cx, cy, r, DARK);
        p.sdisc (cx, cy, r, pal.c);
        p.P (cx - 2, cy - r + 1, WHITE); p.P (cx - 1, cy - r + 1, WHITE);
        for (int i = 0; i < 7; ++i)
        {
            const double a = ((216 + i * 48) * 3.14159265358979 / 180.0);
            p.P (cx + std::sin (a) * (r + 3), cy - std::cos (a) * (r + 3), i / 6.0 <= pos + 1e-4 ? WHITE : tickOff);
        }
        const double a = ((216 + pos * 288) * 3.14159265358979 / 180.0);
        for (int s = 0; s <= r - 1; ++s)
            p.P (cx + std::sin (a) * s, cy - std::cos (a) * s, DARK);
        p.txtO (label, cx, cy + r + 4, WHITE, DARK2, 1);
    }

    void buildShell()
    {
        p.use (shell);
        p.rrect (0, 0, kW, kH, 5, DARK);
        p.rrect (2, 2, 196, 276, 4, "#2f3478"_c);
        p.rrect (2, 2, 194, 274, 4, "#b8c2ff"_c);
        p.vgrad (4, 4, 192, 272, { "#8f9bf0"_c, "#8590e8"_c, "#7a86e0"_c, "#6f7bd8"_c, "#6570cc"_c, "#5b66c0"_c }, 3);
        const uint32_t gl[3] = { GOLD_L, GOLD, GOLD_D };
        const int sc[4][2] = { { 10, 11 }, { 190, 11 }, { 10, 269 }, { 190, 269 } };
        for (auto& s : sc) { p.disc (s[0], s[1], 2, DARK); p.sdisc (s[0], s[1], 1, gl); }
        p.bigTitle ("ENCHANTR", 100, 8, 2);
        p.rrect (8, 28, 184, 182, 6, "#a9b4ff"_c);
        p.rrect (8, 28, 182, 180, 6, DARK);
        p.rrect (12, 32, 176, 174, 5, "#3a3f8f"_c);
        p.R (8, 212, 184, 1, "#3a3f8f"_c); p.R (8, 213, 184, 1, "#9aa6f5"_c);
    }

    void buildPanel()
    {
        p.use (panel);
        p.vgrad (0, 0, 176, 174, { "#3a3f8f"_c, "#323787"_c, "#2a2f78"_c }, 5);
        for (int i = 0; i < 46; ++i)
        {
            const int x = 2 + (int) std::floor (hash2 (i, 1) * 172), y = 2 + (int) std::floor (hash2 (i, 2) * 170);
            if (hash2 (i, 3) > 0.55) p.P (x, y, "#6f78d8"_c);
        }
    }

    //--------------------------------------------------------------------------
    void wizard (int ox, int oy, int up, int frame, double glowLvl)
    {
        auto Rr = [&] (int x, int y, int w, int h, uint32_t c) { p.R (ox + x, oy + y, w, h, c); };
        auto Pp = [&] (int x, int y, uint32_t c) { p.P (ox + x, oy + y, c); };
        const uint32_t RD = "#4a53ad"_c, RM = "#6f7bd8"_c, RL = "#93a0f4"_c, RLL = "#bcc5ff"_c, FOLD = "#3f4799"_c;
        Rr (-9, -3, 7, 3, "#7a4a30"_c); Rr (2, -3, 7, 3, "#7a4a30"_c); Rr (-9, -3, 7, 1, "#9a6540"_c); Rr (2, -3, 7, 1, "#9a6540"_c);
        for (int y = -36; y <= -4; ++y)
        {
            const int hw = rnd (8 + (y + 36) * 0.46);
            const int n = 2 * hw + 1;
            const int d = rnd (n * 0.3), m = rnd (n * 0.4), l = rnd (n * 0.22), ll = n - d - m - l;
            int x = -hw;
            Rr (x, y, d, 1, RD); x += d;
            Rr (x, y, m, 1, RM); x += m;
            Rr (x, y, l, 1, RL); x += l;
            Rr (x, y, ll, 1, RLL);
        }
        Rr (-5, -26, 1, 22, FOLD); Rr (4, -24, 1, 20, FOLD); Rr (-12, -18, 1, 14, FOLD); Rr (11, -16, 1, 12, RD);
        Rr (-23, -5, 47, 1, GOLD); Rr (-23, -4, 47, 1, GOLD_D);
        Rr (-13, -25, 27, 3, "#b9814f"_c); Rr (-13, -23, 27, 1, "#8a5a3c"_c);
        Rr (-2, -26, 5, 5, GOLD); Rr (-1, -25, 3, 3, "#b9814f"_c); Pp (-2, -26, GOLD_L);
        for (int i = 0; i <= 16; ++i)
        {
            const int x = -10 - (int) std::floor (i * 0.45);
            Rr (x, -34 + i, 5, 1, RD); Rr (x + 5, -34 + i, 1, 1, RM);
        }
        Rr (-18, -18, 7, 2, GOLD);
        p.sdisc (ox - 16, oy - 15, 2, SKIN);
        const int top = -56 - up;
        Rr (24, top, 1, -top, "#8a5a3c"_c); Rr (25, top, 1, -top, "#d9a86e"_c);
        const int dy = 12 - up;
        for (int i = 0; i <= 13; ++i) Rr (9 + i, -34 + (int) std::floor ((double) (i * dy) / 13), 5, 1, RL);
        Rr (21, -22 - up, 4, 2, GOLD);
        p.sdisc (ox + 24, oy - 21 - up, 2, SKIN);
        for (int y = -51; y <= -41; ++y)
        {
            const int w = rnd (6 * std::sqrt (std::max (0.0, 1 - std::pow ((y + 46) / 5.5, 2))));
            Rr (-w, y, 2 * w + 1, 1, SKIN[1]); Rr (-w, y, 2, 1, SKIN[2]);
        }
        const uint32_t BWd[3] = { WHITE, "#fffaf2"_c, "#e0d9ee"_c };
        p.sdisc (ox, oy - 27, 4, BWd); p.sdisc (ox, oy - 32, 5, BWd); p.sdisc (ox - 5, oy - 38, 4, BWd); p.sdisc (ox + 5, oy - 38, 4, BWd); p.sdisc (ox, oy - 38, 6, BWd);
        Rr (-5, -43, 11, 2, "#fffaf2"_c);
        Pp (-3, -47, "#2b2250"_c); Pp (-3, -46, "#2b2250"_c); Pp (3, -47, "#2b2250"_c); Pp (3, -46, "#2b2250"_c);
        Pp (-4, -49, "#fffaf2"_c); Pp (-3, -49, "#fffaf2"_c); Pp (3, -49, "#fffaf2"_c); Pp (4, -49, "#fffaf2"_c);
        Pp (-5, -44, "#ff9fb0"_c); Pp (5, -44, "#ff9fb0"_c); Pp (0, -45, "#f2b394"_c);
        Rr (-13, -51, 27, 1, FOLD); Rr (-12, -52, 25, 1, RD); Rr (-10, -53, 21, 1, RM);
        for (int k = 0; k <= 15; ++k)
        {
            const int y = -54 - k;
            const int hw = std::max (0, rnd (8 * (1 - k / 16.0)));
            const int off = rnd (std::pow (k / 15.0, 2) * 6);
            const int n = 2 * hw + 1;
            const int d = rnd (n * 0.35), m = rnd (n * 0.4), l = n - d - m;
            int x = -hw + off;
            Rr (x, y, d, 1, RD); x += d;
            Rr (x, y, m, 1, RM); x += m;
            Rr (x, y, l, 1, RL);
        }
        Rr (-8, -55, 17, 1, GOLD); Rr (-7, -56, 15, 1, GOLD); Rr (-8, -54, 17, 1, GOLD_D);
        Pp (-1, -61, GOLD); Pp (-2, -60, GOLD); Pp (0, -60, GOLD); Pp (-1, -59, GOLD); Pp (-1, -60, GOLD_L); Pp (3, -65, GOLD);
        const int cy = -60 - up;
        const int pulse = (frame % 8 < 4) ? 1 : 0;
        p.ddisc (ox + 25, oy + cy, 8 + pulse + rnd (glowLvl * 2), "#8ff0ff"_c);
        p.disc (ox + 25, oy + cy, 5, "#5fdcf0"_c);
        const uint32_t orb[3] = { WHITE, "#8ff0ff"_c, "#4fd6f0"_c };
        p.sdisc (ox + 25, oy + cy, 4, orb);
        Pp (24, cy - 2, WHITE); Rr (22, -57 - up, 7, 1, GOLD);
    }

    //--------------------------------------------------------------------------
    void plus (int x, int y, uint32_t c)
    {
        p.P (x, y, c); p.P (x - 1, y, c); p.P (x + 1, y, c); p.P (x, y - 1, c); p.P (x, y + 1, c);
    }

    void composeScreen (const UiState& s)
    {
        scr.d.assign (scr.d.size(), 0u);
        blit (scr, lSky, -M, 0, 1.0);
        const int oFar = rnd (s.par * 2), oFx = rnd (s.par), oFg = rnd (-s.par * 4);
        blit (scr, lFar, -M + oFar, 0, 1.0);
        blit (scr, lGround, -M, 0, 1.0);
        blit (scr, lFx, -M + oFx, 0, 0.5);

        p.use (scr);
        p.ga = 0.28;
        for (int i = 0; i < 43; ++i)
        {
            const int h = rnd (std::min (1.0f, s.bars[i]) * 30);
            if (h > 0)
                p.R (3 + i * 4, 78 - h, 3, 2 * h + 1, WHITE);
        }
        p.ga = 1.0;

        const bool casting = s.castT >= 0.f;
        const int hop = casting ? rnd (std::fabs (std::sin (s.castT * 9.42)) * 5 * (1 - s.castT)) : 0;
        const int bob = std::sin (s.timeSec * 2.5) > 0.3 ? -1 : 0;

        p.dell (86, 136, 17, 3, "#2f7a3a"_c, 0.9);
        wizard (86, 134 + bob - hop, casting ? 7 : 0, s.frame, casting ? 1.0 : std::min (1.0f, s.peak * 3.f));

        const int tw2[4][2] = { { 104, 62 }, { 66, 84 }, { 112, 100 }, { 58, 96 } };
        for (int k = 0; k < 4; ++k)
            if (((s.frame + k * 3) % 10) < 6)
                plus (tw2[k][0], tw2[k][1], k % 2 ? WHITE : GOLD);
        for (int k = 0; k < 2; ++k)
        {
            const int bx = rnd (40 + k * 74 + std::sin (s.timeSec * 0.7 + k * 2) * 28);
            const int by = rnd (70 + k * 20 + std::sin (s.timeSec * 1.3 + k) * 8);
            const uint32_t wing = k ? "#ff9a4a"_c : "#ff8fb4"_c;
            if ((s.frame + k * 2) % 6 < 3)
            {
                p.P (bx - 2, by - 1, wing); p.P (bx + 2, by - 1, wing); p.P (bx - 1, by - 1, wing);
                p.P (bx + 1, by - 1, wing); p.P (bx - 1, by, wing); p.P (bx + 1, by, wing);
            }
            else { p.P (bx - 1, by, wing); p.P (bx + 1, by, wing); }
            p.P (bx, by, "#3a2418"_c);
        }
        for (const auto& sp : s.sparks)
            p.R (sp.x, sp.y, sp.big ? 2 : 1, sp.big ? 2 : 1, sp.col);

        blit (scr, lFg, -M + oFg, 0, 1.0);

        // rounded corners of the screen
        for (int j = 0; j < 5; ++j)
        {
            const int i = Painter::rrIns (j, SH, 5);
            p.clearRect (0, j, i, 1); p.clearRect (SW - i, j, i, 1);
            p.clearRect (0, SH - 1 - j, i, 1); p.clearRect (SW - i, SH - 1 - j, i, 1);
        }
    }

    //--------------------------------------------------------------------------
    void drawIcons (const UiState& s)
    {
        (void) s;
        // IMPORT: folder
        p.R (20, 58, 8, 3, DARK); p.R (21, 59, 6, 2, GOLD);
        p.R (19, 60, 18, 13, DARK); p.R (20, 61, 16, 11, GOLD);
        p.R (20, 61, 16, 2, GOLD_L); p.R (20, 70, 16, 2, GOLD_D); p.R (20, 65, 16, 1, GOLD_D);
        // SAVE: floppy
        p.R (20, 81, 17, 17, DARK); p.R (21, 82, 15, 15, "#7ea8ec"_c); p.R (21, 82, 15, 2, "#a9c8f7"_c); p.R (21, 95, 15, 2, "#5a85d6"_c);
        p.R (24, 83, 9, 6, "#f4f0e4"_c); p.R (25, 91, 8, 6, "#2a2d6b"_c); p.R (30, 92, 2, 4, "#f4f0e4"_c);
    }

    std::string trimName (const std::string& n, size_t maxChars) { return n.size() > maxChars ? n.substr (0, maxChars) : n; }

    void drawPlaque (const std::string& label)
    {
        p.rrect (52, 36, 96, 11, 4, DARK); p.rrect (53, 37, 94, 9, 3, DARK2);
        p.txt (label, rnd (100 - Painter::tw (label) / 2.0), 39, GOLD_L, 1);
        p.R (60, 41, 3, 1, GOLD); p.R (137, 41, 3, 1, GOLD);
    }
    void drawSection (const char* label, int y)
    {
        p.txt (label, 18, y, GOLD, 1);
        p.R (18 + Painter::tw (label) + 4, y + 2, 164 - Painter::tw (label) - 4, 1, "#4a50a8"_c);
    }

    void drawEq (const UiState& s)
    {
        auto val = [&] (const char* id)
        {
            for (int i = 0; i < kNumKnobs; ++i)
                if (kKnobs[i].tab == Tab::Tweak && std::strcmp (kKnobs[i].id, id) == 0)
                    return (double) s.knob[i];
            return 0.5;
        };
        p.rrect (18, 166, 164, 36, 3, DARK); p.rrect (19, 167, 162, 34, 3, "#1a1d4d"_c);
        for (int x = 24; x < 180; x += 8)
            for (int y = 170; y < 200; y += 6)
                p.P (x, y, "#2e3380"_c);
        p.R (20, 184, 160, 1, "#4a50a8"_c);
        p.txt ("EQ", 22, 169, "#8f9bf0"_c, 1);
        const double lo = (val ("eqLow") - 0.5) * 2, mi = (val ("eqMid") - 0.5) * 2, hi = (val ("eqHigh") - 0.5) * 2, fr = val ("eqMidFreq");
        int prev = -1000;
        for (int x = 0; x < 160; ++x)
        {
            const double u = x / 159.0;
            const double s1 = 1 / (1 + std::exp ((u - 0.2) * 14)), s2 = 1 / (1 + std::exp (-(u - 0.8) * 14));
            const double b = std::exp (-std::pow ((u - (0.25 + fr * 0.5)) / 0.14, 2));
            int y = rnd (184 - (lo * s1 + mi * b + hi * s2) * 12);
            y = std::max (170, std::min (198, y));
            if (prev > -1000)
                for (int yy = std::min (prev, y); yy <= std::max (prev, y); ++yy)
                    p.P (20 + x, yy, "#8ff0ff"_c);
            p.P (20 + x, y, WHITE); p.P (20 + x, y + 1, "#4fd6f0"_c);
            prev = y;
        }
    }

    void drawKnobs (const UiState& s, bool panelStyle)
    {
        for (int i = 0; i < kNumKnobs; ++i)
        {
            const auto& k = kKnobs[i];
            if (k.tab != s.tab) continue;
            knob (k.x, k.y, 5, palFor (k.pal), s.knob[i], k.label, panelStyle ? "#1c1f55"_c : "#3f8a4a"_c,
                  panelStyle ? "#6a72c8"_c : "#7c7c8a"_c, i == s.hoverKnob);
        }
    }

    void drawTweak (const UiState& s)
    {
        blit (*p.g, panel, 12, 32, 1.0);
        drawPlaque (s.hint.empty() ? "ADVANCED" : s.hint);
        drawSection ("EFFECTS", 51); drawSection ("TONE", 89); drawSection ("OUTPUT", 127);
        drawKnobs (s, true);
        drawEq (s);
    }

    void drawRowIcon (int x, int y, bool factory, uint32_t c)
    {
        static const char* star[5] = { "..#..", ".###.", "#####", ".###.", ".#.#." };
        static const char* heart[5] = { ".#.#.", "#####", "#####", ".###.", "..#.." };
        const char** g = factory ? star : heart;
        for (int r = 0; r < 5; ++r)
            for (int q = 0; q < 5; ++q)
                if (g[r][q] == '#')
                    p.P (x + q, y + r, c);
    }

    void drawPresets (const UiState& s)
    {
        blit (*p.g, panel, 12, 32, 1.0);
        drawPlaque ("PRESET BANK");
        p.rrect (16, 50, 160, 108, 4, DARK); p.rrect (17, 51, 158, 106, 3, "#1a1d4d"_c);
        const int n = (int) s.presets.size();
        for (int i = 0; i < layout::rowCount; ++i)
        {
            const int idx = s.scroll + i;
            if (idx >= n) break;
            const auto& row = s.presets[(size_t) idx];
            const int y = layout::rowY0 + i * layout::rowH;
            const bool on = idx == s.selected;
            p.rrect (19, y + 1, 148, 14, 3, on ? GOLD_D : "#262b6e"_c);
            p.rrect (19, y, 148, 14, 3, on ? GOLD : "#343a8c"_c);
            if (on) p.R (22, y + 1, 142, 1, GOLD_L);
            const uint32_t tc = on ? "#4a2f06"_c : "#fff3d6"_c;
            drawRowIcon (24, y + 4, row.factory, on ? "#7a5410"_c : (row.factory ? GOLD : "#ff8fb4"_c));
            p.txt (trimName (row.name, 22), 34, y + 4, tc, 1);
            const std::string lv = "LV" + std::to_string (row.level);
            p.txt (lv, 163 - Painter::tw (lv), y + 4, on ? "#7a5410"_c : GOLD, 1);
        }
        p.rrect (169, 53, 5, 102, 2, DARK2);
        if (n > layout::rowCount)
        {
            const int thumbH = std::max (10, 102 * layout::rowCount / n);
            const int range = n - layout::rowCount;
            const int ty = 53 + (102 - thumbH) * std::min (s.scroll, range) / range;
            p.rrect (169, ty, 5, thumbH, 2, "#8f9bf0"_c);
        }
        else
            p.rrect (169, 53, 5, 102, 2, "#4a50a8"_c);

        const bool canDelete = s.selected >= 0 && s.selected < n && ! s.presets[(size_t) s.selected].factory;
        rbtn (layout::loadBtn.x, layout::loadBtn.y, layout::loadBtn.w, layout::loadBtn.h, PAL_SUN, "LOAD");
        rbtn (layout::saveBtn.x, layout::saveBtn.y, layout::saveBtn.w, layout::saveBtn.h, PAL_AQUA, "SAVE");
        rbtn (layout::delBtn.x, layout::delBtn.y, layout::delBtn.w, layout::delBtn.h, canDelete ? PAL_ROSE : PAL_GREY, "DELETE");

        p.R (16, 184, 160, 1, "#4a50a8"_c);
        if (! s.message.empty())
            p.txtO (s.message, 100, 191, WHITE, DARK2, 1);
        else
        {
            const std::string c = std::to_string (n) + " PRESETS";
            p.txt (c, rnd (100 - Painter::tw (c) / 2.0), 191, "#c4cbff"_c, 1);
        }
    }

    void drawMainOverlay (const UiState& s)
    {
        // preset strip
        p.rrect (54, 40, 92, 10, 4, DARK2);
        p.R (65, 43, 1, 5, "#fff3d6"_c); p.R (64, 44, 1, 3, "#fff3d6"_c); p.R (63, 45, 1, 1, "#fff3d6"_c);
        p.R (134, 43, 1, 5, "#fff3d6"_c); p.R (135, 44, 1, 3, "#fff3d6"_c); p.R (136, 45, 1, 1, "#fff3d6"_c);
        const std::string nm = trimName (s.presetName.empty() ? "UNTITLED PRESET" : s.presetName, 17);
        p.txt (nm, rnd (100 - Painter::tw (nm) / 2.0), 43, "#fff3d6"_c, 1);
        if (s.hasSample && ! s.sampleName.empty())
            p.txtO (trimName (s.sampleName, 30), 100, 53, "#e8ecff"_c, DARK2, 1);

        drawIcons (s);
        drawKnobs (s, false);

        // bottom bar: level, hint or drop message
        p.rrect (17, 190, 166, 12, 4, DARK2);
        std::string t;
        uint32_t col = "#fff3d6"_c;
        bool star = false;
        if (! s.hint.empty()) t = s.hint;
        else if (! s.hasSample)
        {
            t = "DROP A SOUND OR CLICK IMPORT";
            col = GOLD;
            if ((s.frame / 8) % 2) t.clear();
        }
        else
        {
            t = "LEVEL " + std::to_string (s.level) + " - " + s.levelName;
            star = true;
        }
        if (! t.empty())
        {
            const int x0 = rnd (100 - Painter::tw (t) / 2.0);
            p.txt (t, x0, 193, col, 1);
            if (star)
            {
                static const int st[12][2] = { { 2, 0 }, { 2, 1 }, { 0, 2 }, { 1, 2 }, { 2, 2 }, { 3, 2 }, { 4, 2 }, { 1, 3 }, { 2, 3 }, { 3, 3 }, { 1, 4 }, { 3, 4 } };
                for (auto& q : st)
                    p.P (x0 + Painter::tw (t) + 4 + q[0], 193 + q[1], GOLD);
            }
        }
        if (s.dragOver)
        {
            p.R (14, 34, 172, 2, GOLD); p.R (14, 202, 172, 2, GOLD); p.R (14, 34, 2, 170, GOLD); p.R (184, 34, 2, 170, GOLD);
        }
    }

    void render (Canvas& out, const UiState& s)
    {
        if (out.w != kW || out.h != kH)
            out = Canvas (kW, kH);
        out.d = shell.d;
        p.use (out);

        if (s.tab == Tab::Main)
        {
            composeScreen (s);
            blit (out, scr, SX, SY, 1.0);
            p.use (out);
            drawMainOverlay (s);
        }
        else if (s.tab == Tab::Tweak)
            drawTweak (s);
        else
            drawPresets (s);

        p.use (out);
        btn (layout::tweakX, layout::tweakY, 15, PAL_SUN, "TWEAK", s.tab == Tab::Tweak);
        btn (layout::presetsX, layout::presetsY, 15, PAL_AQUA, "PRESETS", s.tab == Tab::Presets);
        btn (layout::levelX, layout::levelY, 17, PAL_ROSE, "LEVEL UP", s.levelPressed);

        if (s.castT >= 0.f)
        {
            if (s.castT < 0.25f)
            {
                p.ga = std::floor ((1.0 - s.castT / 0.25) * 4) / 4 * 0.6;
                p.R (SX, SY, SW, SH, WHITE);
                p.ga = 1.0;
            }
            const double fade = s.castT > 0.8 ? 1 - (s.castT - 0.8) / 0.2 : 1;
            if (fade > 0.5)
                p.title3d ("LEVEL UP!", SX + 86, SY + 34 - rnd (s.castT * 14), 2, 2);
        }
    }
};

Art::Art() : impl (new Impl()) {}
Art::~Art() { delete impl; }
void Art::render (Canvas& out, const UiState& s) { impl->render (out, s); }
} // namespace ench
