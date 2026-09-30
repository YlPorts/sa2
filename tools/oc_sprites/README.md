# Original-character graphics codec

`compile.mjs` converts five image-generation source sheets, in the order Elizabeth,
Jude, Kiro, Yuliana, Kura, into the exact graphics rendered by the game. Each source has
32 poses in eight columns and four rows and is RGBA 1536 × 1024. The compiler uses
connected figures because generated artwork can cross the nominal cell boundary.
It does not draw or invent poses.

```sh
node tools/oc_sprites/compile.mjs Elizabeth.png Jude.png Kiro.png Yuliana.png Kura.png
node tools/oc_sprites/compile_special.mjs ElizabethRear.png JudeRear.png KiroRear.png YulianaRear.png KuraRear.png
node tools/oc_sprites/compile_actions.mjs ElizabethAction.png JudeAction.png KiroAction.png YulianaAction.png KuraAction.png
node tools/oc_sprites/verify.mjs
```

With no arguments all compilers use the committed source PNGs automatically.
All three accept `--preview` to produce native previews without changing the
combined C arrays; action previews can process the sources that are already
available while the remaining characters are being prepared.

Node.js and `sharp` are required. In the Codex runtime the compiler resolves the
installed dependency through `CODEX_PRIMARY_RUNTIME_NODE_MODULES`; otherwise a
local `sharp` package is sufficient.

Each character uses one constant scale for all 32 poses. The idle height is 36
pixels, or 38 for Yuliana. Every frame is 64 × 64 with its feet reference at
(32, 48); airborne compact poses are aligned around the body instead. Alpha is
thresholded at 200 and has only transparent or opaque output pixels. A fixed
15-color RGB555 palette per character preserves the darkest outline entry.
Frames contain 8 × 8 tiles in row-major tile order, with the left pixel in the
low nibble. `verify.mjs` decodes those files and compares every pixel to the PNG.

`graphics/sa2/ocs/asset_manifest.json` records source hashes, scale, source bounds,
frame labels, and output positions. The PNGs are the actual final native assets,
not the higher-resolution source art. The generated C includes are committed
because Android builds use plain C arrays and need no host image tooling.

Frame order: idle, blink, breathe, run × 6, crouch, launch, rise, tuck, fall,
land, brake × 3, hurt × 2, death, balance × 2, hang, push, grind, swim, victory,
wave, attack, curl × 2.

The rear-view sheets have eight poses in four columns and two rows. They use the
existing main palette without changing it. Their frame order is run × 4, turn
left, turn right, jump, hurt. Source PNGs are saved under
`graphics/sa2/ocs/source/` so both converters can be reproduced from the checkout.

The action sheet has 24 poses in six columns and four rows: run × 12, humanoid
jump × 4, attack × 4, victory × 4. It shares the main palette. Run and jump frames
use a hip X anchor and a constant feet reference at (32,48). The compiler can read
`graphics/sa2/ocs/action_alignment.json`: each character may define one `scale`
for all 24 poses and a `frames` array with absolute source `hipX`, `groundY`, and
native `pivotY` overrides. This aligns body joints independently of hair, capes,
weapons, and other features that change the artwork's bounding box.

The action manifest also records native gait changes, distinct frame counts,
and color approximation error for visual review. `verify.mjs` checks all three
atlas families, including exact decoded colors, binary opacity, and clear borders.

The OCS3 revision is isolated from those three families. Run sheets for Jude,
Kiro and Yuliana each have eight chronological gait poses in four columns and
two rows. Their attacks are the bottom eight poses of separate sixteen-pose
motion sheets. Kura has an eight-pose ability sheet. Existing jumps, victories, rear views,
Elizabeth's animation and Kura's running animation remain byte-identical.

```sh
node tools/oc_sprites/compile_revised.mjs --inspect
node tools/oc_sprites/compile_revised.mjs --preview
node tools/oc_sprites/compile_revised.mjs
node tools/oc_sprites/verify_revised.mjs
```

Sources default to `source/{jude,kiro,yuliana}_run_source_v3.png`, attack sources
`source/{jude,kiro,yuliana}_motion_v3.png`, and `source/kura_ability_v3.png`.
Seven positional arguments can override those inputs in that order.
The revised codec accepts varying source dimensions
and identifies bodies with connected opaque components and face/hand colors;
it does not crop blindly along nominal grid divisions. Detached smoke, flashes
and sword arcs are attached to their nearest body without changing its anchor.
`revised_run_alignment.json` and `revised_attack_alignment.json` require one
constant body scale per source and explicit `bodyAnchorX` and `groundY` for every
pose (`hipX` remains an accepted input alias). The horizontal animation anchor
is calibrated manually from the neck and torso so the body stays steady while
arms and legs change pose. A small graphical waist offset is allowed; this
does not claim every generated pose has an anatomically identical pelvis.
Separately generated source densities are calibrated to the same native body
proportions. Weapons, hair and effects cannot determine scale.
Airborne gait phases share the row's contact floor, preserving their small lift.

Revised atlases are `*_run_v3.png/.4bpp` and `*_attack_v3.png/.4bpp`, each with
eight 64 × 64 frames in four columns. C arrays are `gOcRevisedRunTiles[3][8][2048]`
(Jude, Kiro, Yuliana) and `gOcRevisedAttackTiles[4][8][2048]` (those three, Kura).
The eight gait phases are contact/down/passing/flight on one support leg, then
the same four phases on the opposite leg. Attack order is two anticipation
poses, three active poses and three recovery poses; gameplay assigns durations.

All existing palettes are frozen. Source art must supply visibly separated
limbs and readable weapons before conversion: a pan head about 9 × 8 native
pixels with a 3-pixel handle; a blade at least 16 pixels long with a 2-pixel
bright core; a pistol about 14 × 4 pixels with a distinct grip and metallic edge.
These are source-art review targets, not geometry synthesized by the codec.
Eight unique frame hashes alone do not establish a coherent gait: native
contact sheets and GIFs must also show alternate supports, stable proportions
and readable silhouettes at phone scale.

Preview mode writes outside the repository and changes no production bytes.
The manifest records native bounds, face motion, silhouette changes, secondary
components, source hashes and palette hashes. `verify_revised.mjs` proves exact
PNG/4bpp/C agreement, eight distinct gait poses, safe borders, and every frozen
OCS2 hash listed in `ocs2_preserved_sha256.json`.
Face motion is a diagnostic: the largest connected skin region in the upper
part of each figure is measured separately from the fists. Its bounds and the
sizes of other skin regions are recorded so the selected face can be inspected.
It never changes the image or automatically defines the body's anchor.
Separate alignment inspection sheets add an X=32 guide and a line below sole
Y=48; these annotations are outside the repository and outside the game assets.

Kiro's down-B pose (run index 5) uses a complete generated pose from
`source/kiro_down_source_v3.png`. Its frame entry explicitly names the override
source, source frame 5, constant source scale, body anchor and ground reference.
The other seven Kiro poses come from the original run source and their approved
tile hashes are checked against `kiro_run_preserved_sha256.json`. This imports
one whole generated pose without repainting or resampling the other seven.
