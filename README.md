# Enchantr

A pixel-art "ear candy" sampler (VST3) for FL Studio and other DAWs.
Drop in any one-shot, press LEVEL UP, and the wizard re-rolls a gentle effect chain.

## Get the .vst3 (no coding, no installs)

1. Create a free account on github.com and make a **new repository** (private is fine).
2. Upload the contents of this folder to it (drag everything in, including the hidden `.github` folder).
3. Open the **Actions** tab. A build named "Build Enchantr VST3" starts by itself (about 10 minutes).
   If it doesn't, click it and press **Run workflow**.
4. When it turns green, open it and download **Enchantr-VST3-Windows** (or **-macOS**) from the Artifacts list at the bottom.
5. Unzip it. You get a folder named `Enchantr.vst3`.

## Install in FL Studio

- **Windows:** copy `Enchantr.vst3` into `C:\Program Files\Common Files\VST3\`
- **macOS:** copy it into `/Library/Audio/Plug-Ins/VST3/` (if macOS blocks it, run `xattr -cr Enchantr.vst3` in Terminal first)

Then in FL Studio: **Options > Manage plugins > Find plugins** (or Start verify). Enchantr shows up under Installed > Generators.
Add it to a channel, drag a sample onto the window (or click the folder icon), and play it from the piano roll.

## Controls

- **Folder icon**: import a sound (or drop a file anywhere on the window)
- **Floppy icon**: save the current sound as a preset
- **< name >** strip: previous / next preset
- **MIX / GLIDE / VIBRATO** knobs next to the wizard
- **LEVEL UP**: re-roll the effects (never repeats a recent roll)
- **TWEAK**: all effect, EQ, warmth, envelope and output knobs
- **PRESETS**: 6 factory presets plus your own (LOAD / SAVE / DELETE). Your presets live in `%AppData%\Enchantr\Presets`
- Knobs: drag up/down (hold Shift for fine), double-click to reset, mouse wheel also works
- The window can be resized from its corner

Presets remember the path of your sample, not the audio itself, so keep your sample files where they are.

## Building locally

Needs CMake 3.22+ and a C++17 compiler (Visual Studio 2022 on Windows, Xcode on macOS):

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

The plugin is created in `build/Enchantr_artefacts/Release/VST3/`.

`tests/render_test.cpp` renders the UI screens to image files without JUCE (see the comment at its top).
