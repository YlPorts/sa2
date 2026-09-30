# Original-character graphics codec

`compile.mjs` converts four image-generation source sheets, in the order Elizabeth,
Jude, Kiro, Yuliana, into the exact graphics rendered by the game. Each source has
32 poses in eight columns and four rows and is RGBA 1536 × 1024. The compiler uses
connected figures because generated artwork can cross the nominal cell boundary.
It does not draw or invent poses.

```sh
node tools/oc_sprites/compile.mjs Elizabeth.png Jude.png Kiro.png Yuliana.png
node tools/oc_sprites/compile_special.mjs ElizabethRear.png JudeRear.png KiroRear.png YulianaRear.png
node tools/oc_sprites/verify.mjs
```

With no arguments both compilers use the committed source PNGs automatically.

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
