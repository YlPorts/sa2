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
