# Building on Windows x64

Requires Windows x64, PowerShell 7 (`pwsh`), Git, an internet connection for setup, and your own legally obtained compatible **Legendary Wings (USA).nes** ROM.

```powershell
git clone --recurse-submodules https://github.com/retroreplay82/Legendary-Wings-NES-to-PC.git
cd Legendary-Wings-NES-to-PC
pwsh -File ./setup.ps1
```

Setup obtains Zig 0.15.2, verifies the pinned NESRecomp revision, and copies the approved artwork from the published Beta 1 ZIP after checking its SHA256. It does not obtain a ROM. The artwork is kept in the release rather than duplicated in Git. SDL2 headers and libraries are supplied by the pinned framework checkout.

Place your ROM at `roms/Legendary Wings (USA).nes`, then build:

```powershell
pwsh -File ./build.ps1
./build/LegendaryWings.exe
```

The build recompiles the user's ROM locally into `generated/`, then compiles the Windows executable. Original tile data remains a runtime read from the local ROM; the recompiler folds only instruction bytes. Generated files are ignored and must not be committed or redistributed as project source.

Keep `build/assets/` and `build/SDL2.dll` with the executable. The executable can find the ROM in the project's `roms/` folder. For a portable installation, put it in a `roms/` folder beside the executable instead.

The source ZIP from GitHub does not contain submodule contents. `setup.ps1` can clone the pinned dependency when run outside a Git checkout. An already-downloaded release ZIP can be used with `pwsh -File ./setup.ps1 -AssetArchive C:/path/to/LegendaryWings-windows-x64-beta.1.zip`.

## Source layout

- `enhanced-presentation.c` and `bonus-presentation.inc`: remastered rendering, artwork registration, title credit and graphics/water toggles.
- `*-animation.c` / `*.h`: animation selection and sprite registration.
- `artwork-priority.c`: artwork priority helpers.
- `desktop-entry.c`: Windows entry point and local ROM discovery.
- `respawn-collision.c` and `game.toml`: terrain collision hooks during respawn grace.
- `coverage/seeds.txt`: code discovery addresses and execution counts, without ROM bytes.
- `vendor/nesrecomp`: pinned upstream submodule.

## Licensing

See `THIRD-PARTY-NOTICES.txt` for dependency licenses, including NESRecomp's PolyForm Noncommercial license. No separate permissive license has been assigned to the project-specific code or artwork. Publishing this source does not grant rights to the original game's ROM, music or artwork. Do not upload ROMs, extracted ROM assets, generated ROM code, save states, credentials or build output.
