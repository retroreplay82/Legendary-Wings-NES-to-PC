# Legendary-Wings-NES-to-PC
A native Windows port of Legendary Wings with revamped artwork and faithful gameplay. Requires your own compatible NES ROM; no ROM or extracted ROM assets included.

Legendary Wings PC

A Windows port of Legendary Wings with refreshed artwork and the original gameplay.

**Author:** dmb062082  
**Website:** [Retro Replay](https://retro-replay.com)

## Work in progress

This is an early beta. The remaster currently focuses on the first level; later levels have not received the same artwork overhaul and may still show original graphics. Future updates will expand the remaster to later levels and continue improving artwork, transitions, and compatibility.

## Getting started

1. Extract the Windows release into its own folder.
2. Place your compatible **Legendary Wings (USA).nes** file in the **roms** folder beside **LegendaryWings.exe**.
3. Launch **LegendaryWings.exe**. Keep **SDL2.dll** and the **assets** folder beside it.

You must supply your own ROM. No ROM files or extracted ROM assets are included in the public distribution.

## Keyboard controls

| Key | Action |
| --- | --- |
| Arrow keys | Move |
| X | Fire |
| Z | Bomb / jump, depending on the section |
| Enter | Start / pause |
| Backslash (`\`) | Select |
| **F5** | Toggle water animation on / off (starts off) |
| **F6** | Switch between original graphics and the remastered presentation |
| **F8** | Save state |
| **F9** | Load the saved state |
| F11 | Toggle fullscreen |
| F12 | Take a screenshot |

Water animation starts **off**. Press **F5** to turn it on; press F5 again to turn it off.

F6 changes the presentation while keeping your current gameplay progress. Remastered artwork is available where it has been completed.

Save states use one slot per ROM and are stored in the **saves** folder beside the executable. Saving again replaces the previous state. Loading returns you to the saved point, replacing your current progress. Back up your saves before updating; compatibility between builds is not guaranteed.

## Feedback

Please report problems through GitHub Issues. Include the build version, the level or section, and the steps needed to reproduce the problem.

## Credits and licensing

Built with [NESRecomp](https://github.com/mstan/nesrecomp) and SDL2. Their license notices are retained with the distribution. The original game and its intellectual property belong to their respective owners.

