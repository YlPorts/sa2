<h1 align="center">Sonic Advance Trilogy</h1>
<p align="center">
  <img height="240" src=".github/media/titlescreen.png" alt="Sonic Advance 2 title screen">
</p>
<div align="center">

  |     |     |
  | --- | --- |
  | SA1 | ![Decompilation Progress](https://img.shields.io/endpoint?url=https://sat-r.github.io/sa2/reports/progress-sa1-shield.json) ![Decompilation Matching Progress](https://img.shields.io/endpoint?url=https://sat-r.github.io/sa2/reports/progress-sa1-shield-matching.json) |
  | SA2 | ![Decompilation Progress](https://img.shields.io/endpoint?url=https://sat-r.github.io/sa2/reports/progress-sa2-shield.json) ![Decompilation Matching Progress](https://img.shields.io/endpoint?url=https://sat-r.github.io/sa2/reports/progress-sa2-shield-matching.json)  |

  <a href="https://github.com/SAT-R/sa2/actions/workflows/build.yml"><img src="https://github.com/SAT-R/sa2/actions/workflows/build.yml/badge.svg" alt="CI status"></a>
  <a href="https://github.com/SAT-R/sa2/graphs/contributors"><img src="https://img.shields.io/github/contributors/SAT-R/sa2" alt="Contributors"></a>
  <a href="https://discord.gg/vZTvVH3gA9"><img src="https://img.shields.io/discord/1052347299457671200" alt="Discord"></a>
</div>

> :warning: **This project is not completed and still under active development**

This is a work in progress matching decompilation of Sonic Advance 1, 2, [& eventually 3](https://github.com/SAT-R/sa3/)

It builds the following ROMs:
* [**sa1.gba**](https://datomatic.no-intro.org/index.php?page=show_record&s=23&n=0330) `sha1: eb00f101af23d728075ac2117e27ecd8a4b4c3e9` (Europe) (En,Ja,Fr,De,Es)
* [**sa2.gba**](https://datomatic.no-intro.org/index.php?page=show_record&s=23&n=0890) `sha1: 7bcd6a07af7c894746fa28073fe0c0e34408022d` (USA) (En,Ja,Fr,De,Es,It)
* [**sa2_europe.gba**](https://datomatic.no-intro.org/index.php?page=show_record&s=23&n=0900) `sha1: b0f64bdca097f2de8f05ac4c8caea2b80c5faeb1` (Europe) (En,Ja,Fr,De,Es,It)
* [**sa2_japan.gba**](https://datomatic.no-intro.org/index.php?page=show_record&s=23&n=0799) `sha1: dffd0188fc78154b42b401398a224ae0713edf23` (Japan) (En,Ja,Fr,De,Es,It)
* [**sa2_japan_vc.gba**](https://datomatic.no-intro.org/index.php?page=show_record&s=23&n=x380) `sha1: 2aa6ee2cf2b0ebfafbea6d6d24165b252a7e329e` (Japan) (En,Ja,Fr,De,Es,It) (Virtual Console)
* [**sa2_beta.gba**](https://datomatic.no-intro.org/index.php?page=show_record&s=23&n=x149) `sha1: 3368642fc4157824af63367e2a685b7d6ee9b09d` (USA) (Beta) (2002-10-25)

It can also build:
* **sa2.sdl** `make sdl` (Linux/MacOS SDL 64bit port)
* **sa2.sdl_win32.exe** `make sdl_win32` (Windows SDL 64bit port)
* :construction: **sa2.win32.exe** `make win32` (Win32 OpenGL port, not functional)
* **sa2.ps2.iso** `make ps2` (PlayStation 2 port, requires [PS2DEV](https://github.com/ps2dev/ps2dev))
* **EBOOT.PBP** `make sdl_psp` (PlayStation Portable SDL port, requires [PSPDEV](https://github.com/pspdev/pspdev))

## Current state (SA2)

- :tada: The build is 100% from C files with [~60 functions which currently don't match](./asm/non_matching)
- All assembly code extracted, disassembled, and decompiled by hand to their C equivilent
- All songs have been extracted, and documented as [matching MIDI files](./sound/sa2/songs/midi)
- All sprite animation frames have been [extracted to PNGs and are used to build the matching rom](./graphics/sa2/obj_tiles)
- All tilemaps (backgrounds) have been documented and [had their tiles extracted](./data/sa2/tilemaps)
- Most aspects of core game play have been documented, but >600 functions are still unnamed
- The game compiles to a widescreen port (*426x240*) for multiple platforms

## Sub games

Within the Sonic Advance Trilogy roms there are some "Sub games" which we have begun to decompile as part of this project. These sub games are automatically compiled and bundled during the main build process, as they are necessary to create a matching binary.

| Game | Multiboot rom | Decompilation |
| --- | --- | --- |
| SA2 | [**mb_chao_garden.gba**](./chao_garden) | ![Decompilation Progress](https://img.shields.io/endpoint?url=https://sat-r.github.io/sa2/reports/progress-mb_chao_garden-shield.json) |
| SA2 | [**collect_rings.gba**](./multi_boot/roms/collect_rings/) | ![Decompilation Progress](https://img.shields.io/endpoint?url=https://sat-r.github.io/sa2/reports/progress-collect_rings-shield.json) |


## Setting up the repo

Please see follow [these instructions](./INSTALL.md)

## Contributing

Join us on [discord](https://discord.gg/vZTvVH3gA9) to get started in helping out. Otherwise, we are always looking for people to document the code base, and there are [many non matching functions](./asm/non_matching) which need human input to resolve. 

### Strict No LLM / No AI Policy

- No LLMs for issues.
- No LLMs for pull requests.

This project was created as a learning excerise, I (freshollie) like learning new things. Of course it also has real world outcomes (having the ability to play, mod, port this game), but we as maintainers are not interested in submissions from LLM origin.

We want humans to use this project to practice their skills. Reverse engineering, documenting, understanding, being part of the process. If you submit a pull request we want you to have accountability as well as knowledge of what you've done. We want you to develop on those skills and continue to contribute. If "the computer" did all the work for you, we are not interested.

Our policy is based off [Zig's](https://ziglang.org/code-of-conduct/)

### Layout
```
├── asm
│   ├── macros # Common macros used in asm files
│   └── non_matching # Non matching assembly pulled in the C compilation process
├── chao_garden # The tiny chao garden project (based on the SA2 version)
├── data # Raw data pulled into the rom via ASM files
├── graphics # Graphics related assets
├── include # Headers
├── libagbsyscall # The agbsyscall library included as part of the nintendo SDK
├── multi_boot # Any multiboot roms (used for singlepak multiplayer)
├── scripts # Scripts used in the decompilation process
├── sound # Sound related assets
├── src
│   ├── data # C files which pull in assets as part of the compilation process
│   ├── game
│   │   ├── sa1
│   │   ├── sa2
│   │   └── shared # C files used across all 3 games
│   ├── lib # libraries which were originally statically linked
│   │   ├── agb_flash
│   │   └── m4a
│   ├── platform # port related code
│   └── . # src root contains Dimps "engine" C files
└── tools # tools used for compilation or extraction
```

### Notable info

- The [Kirby & The Amazing Mirror](https://github.com/jiangzhengwenjz/katam/) decompilation uses the same "engine", as it was written by the same dev team (Dimps)
- https://decomp.me is a great resource for helping to create matching function
- For more info, see the [FAQs section](https://zelda64.dev/games/tmc) of TMC

## Credits

- [JaceCear](https://github.com/JaceCear) for his dedication to understanding the internals of the graphics engine, writing [tools to extract this data](https://github.com/JaceCear/SA-Trilogy-Animation-Exporter), as well as massive effort in contributing towards the decompilation process (including decompiling the whole of sa1!), *and* setting up the PC ports
- Shout out to [@froggestspirit](https://github.com/froggestspirit) for the drive to set this project up
- Special thanks to [@normmatt](https://github.com/normmatt) for the initial repo setup and sounds disassembly
- [Pokemon Reverse Engineering Tools](https://github.com/pret) community for their help with the project, and tooling for GBA decompilations
- [Kermalis](https://github.com/Kermalis) for [their tool](https://github.com/Kermalis/VGMusicStudio) which was used to dump the game midis
- [琪姬](https://github.com/laqieer) for their exellent work [documenting](https://github.com/FireEmblemUniverse/fireemblem8u/pull/137) all the quirks of matching midis

## Android OCS4: Sans and Spider-Man

This fork adds Sans and classic red-and-blue Spider-Man as separate selectable identities alongside Elizabeth, Jude, Kiro, Yuliana and Kura. Original Sonic character/save arrays still have their original five entries. The existing OC artwork, including the OCS3 revisions, is retained byte for byte: `graphics/sa2/ocs/guests/preserved_sha256.json` freezes all 86 pre-existing asset files. The earlier OCS2/OCS3 tools and manifests describe those historical revisions; OCS4 exports only new guest includes.

| Array | Identity order | Frames per identity |
| --- | --- | --- |
| Main, special-stage, supplementary actions | Elizabeth, Jude, Kiro, Yuliana, Kura, Sans, Spider-Man | 32, 8, 24 |
| Revised runs | Jude, Kiro, Yuliana, Sans, Spider-Man | 8 |
| Revised abilities | Jude, Kiro, Yuliana, Kura, Sans, Spider-Man | 8 |
| Guest projectile tiles | Sans bone, Spider-Man web | One 8×8 tile each |

Body tiles use a 64×64 canvas and the shared feet pivot `(32,48)`. The standing reference heights are 32 pixels for Sans and 36 for Spider-Man. One constant scale applies to each artwork source; compressed and airborne poses keep their authored proportions and vertical offsets. Every identity has one fixed RGB555 palette with transparency plus 15 opaque entries. Index 1 is black and index 2 is neutral white for the HUD and stage-introduction lettering. Spider-Man keeps all 14 quantized original costume colors; the additional white entry comes from his original web artwork.

Sans has new transparent PNG artwork: the base atlas is 8×4 poses, motion is 6×4, rear is 4×2, and a separate 4×2 run atlas supplies the final eight-pose asymmetric shuffle. Its approved whole-pose order is `[0,1,2,4,5,6,7,3]`, with contact floors retained separately for the two source rows. This is an authored Sans shuffle with alternating support, rather than a claim of a mirrored human running cycle. The standalone bone source supplies the projectile. The base atlas retains its 32-frame semantic order; its six run slots and the supplementary run slots reuse complete poses from the same approved shuffle.

Spider-Man uses complete original GBA frames from **Ultimate Spider-Man**, credited on the archived sheet to **Naruto[NU] and RundownAngel**. The original GIF and its pixel-identical full-sheet PNG are retained under `graphics/sa2/ocs/guests/source/`. Source: [SpriteDatabase, file 6088](https://spritedatabase.net/file/6088), [original sheet](https://spritedatabase.net/files/gba/1185/Sprite/UltimateSpiderman.gif). `spiderman_original_frame_map.json` records the complete source rectangles and roles. Only exact background colors connected to crop boundaries become transparent, preserving the original enclosed eye lenses and highlights. All three families share the scale `36/34`; no limbs, poses, mirrors or painted replacement pixels are synthesized. The bonus-stage family uses available authored side/front three-quarter poses; this source contains no true back-view running loop.

Detached source bones/web effects are excluded from player-body tiles and packed separately. The projectile silhouettes are complete source components fitted within one 8×8 tile. The web quantizes to the existing neutral black/white palette entries so its original gray/white effect stays readable without changing costume colors. Supplementary action slots map the motion source as run `[0..7,0..3]`, jump `[8..11]`, ability `[12,14,16,18]`, victory `[20..23]`; revised abilities retain all eight source poses `[12..19]`. The emitted manifest records every complete-frame mapping and the Sans run override.

Preview and verify without modifying any production guest arrays:

```sh
node tools/oc_sprites/compile_guests.mjs --inspect
node tools/oc_sprites/compile_guests.mjs --preview
node tools/oc_sprites/verify_guests.mjs --preview
```

The default review directory is `../guest-verification`; set `OC_GUEST_PREVIEW_DIR` to override it. Inspect the native contact sheets, 100 ms-per-pose GIFs, projectile previews, and the run GIFs containing 24 poses over exactly 800 ms (30 pose changes per second). After review, write `approved.json` containing `{"manifestSHA256":"EXACT_REVIEWED_PREVIEW_MANIFEST_SHA256"}` and publish the exact reviewed result:

```sh
node tools/oc_sprites/compile_guests.mjs --emit --approval ../guest-verification/approved.json
node tools/oc_sprites/verify_guests.mjs
```

Publication requires both guests' complete base/motion/bonus families and both projectile sources. A changed source or alignment/palette configuration invalidates the reviewed SHA256. All outputs stage together and replace `graphics/sa2/ocs/guests/generated/` as one directory after conversion succeeds; the old OC includes remain untouched. Verification checks source hashes, frozen asset hashes, transparent body borders, palette colors, every PNG/4bpp pixel, exact C include bytes, eight distinct running poses, and GIF timing.
