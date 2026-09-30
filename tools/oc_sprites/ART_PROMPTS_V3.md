# OCS3 raster prompt specification

Status: the final pack of 56 poses across seven animation families is frozen and approved. Validation passed 21 host ASan checks, 21 ARM32/O3 checks and three weapon visual checks; the final engine captures were reviewed. These results do not constitute physical-device testing. This document records the final normalized specification; it is not a claim that every sentence below was used verbatim in an earlier generation.

The raster sheets are created or edited with the built-in `image_gen` tool. The repository compiler subsequently crops, aligns, quantizes and packs those images into the engine's native 4bpp tiles and RGB555 palettes. The raster generation is an artistic step; the packing step is reproducible from checked-in source images and compiler settings.

## Shared identity and adaptation prompt

Use the supplied character reference and the approved native sprite palette. Preserve the character's recognizable hair, clothing, silhouette, accessories and weapon identity. Adapt the drawing to readable Sonic Advance-scale pixel art with consistent contours, facial detail and body proportions. Keep the head, feet and equipment coherent from frame to frame. Do not invent a different outfit, remove distinctive features, recolor an identifying accessory, or replace the character with Sonic-shaped anatomy.

Use a transparent background. Every cell contains exactly one complete figure; no labels, grid lines, extra characters or detached decoration. Keep generous transparent clearance around weapons, wings and hair. Preserve the approved palette: avoid gradients, antialiasing, soft shadows and new colors. All frames face right. Preserve the character reference's chibi head-to-body proportions; the guide supplies joint angles and pose order, not replacement anatomy. Target a native body height of 36 pixels, or 38 for Yuliana. Frame crops must remain compatible with the native 64×64 canvas and its feet anchor at (32, 48); final alignment is applied by the compiler, not inferred from arbitrary padding in the generated raster. Manual neck/torso anchors calibrate the imported frames; this does not claim perfect pelvis alignment. The raster source characters are not repainted by the packing code.

## Eight-pose run-only prompt

Use `tools/oc_sprites/references/run_gait_guide.png` as the primary structural reference for joint angles, near/far roles and pose order. Its editable technical diagram is `tools/oc_sprites/references/run_gait_guide.svg`; neither file is final character artwork. Use the character reference separately for identity, style and head-to-body proportions. Distinguish the near leg from the far leg throughout the entire cycle. Produce these eight ordered poses, with readable changes in knee position, foot contact and weight transfer:

1. Near-leg contact: near foot forward on the ground, far foot trailing.
2. Near-leg down: weight settles onto the near leg, opposite knee advances.
3. Near-leg passing: far leg passes beneath the torso; near leg supports the body.
4. Near-leg flight: both feet airborne, far knee advances toward its contact.
5. Far-leg contact: far foot forward on the ground, near foot trailing.
6. Far-leg down: weight settles onto the far leg, opposite knee advances.
7. Far-leg passing: near leg passes beneath the torso; far leg supports the body.
8. Far-leg flight: both feet airborne, near knee advances toward the next contact.

Make the second half an actual change of support leg, not a repeated drawing of the first half. Follow the guide's near/far markings and retain the character's own silhouette. Coordinate the opposite arm with each leg. Preserve head scale and equipment; allow a small natural vertical body movement without changing the feet anchor. Produce only this eight-pose run cycle, in a grid of four columns and two rows, ordered left to right and then top to bottom. The game controls cadence separately, with a maximum of 30 visual pose changes per second.

Apply this run-only prompt separately to Jude, Kiro and Yuliana. The guide files and the final source hashes accompany the frozen asset architecture.

## Weapon motion sheets

Each motion source has sixteen cells arranged as four columns and four rows. The last two rows contain the eight-phase weapon action used by the compiler, ordered left to right and then top to bottom. Keep the first two rows' run/reference material separate from the new run-only sources; do not use it as proof of the accepted alternating-leg gait.

### Jude: yellow frying pan

Show a clearly visible yellow frying pan, with a white or cream rim and a dark handle. Keep its identity and colors across all eight phases. Draw preparation, lift, strike, follow-through and recovery as distinct readable poses. Preserve the hand-to-handle connection. The pan must not become a blade, an indistinct yellow streak or a different-colored object.

### Kiro: katana draw and cut

Keep the sword and sheath readable. Start with a visible draw from the sheath, progress through preparation and a clear katana cut, then recover the blade and body into the resting stance. Keep the grip and blade orientation anatomically coherent. Any cut accent must remain compact and must not hide the weapon, body or face. No separate floating swords or large screen-filling effects.

### Yuliana: handgun and restrained recoil

Keep one handgun clearly visible in the firing hand. Prepare, aim, fire one bright shot, then recover. Use a compact muzzle flash for the single shot. The barrel remains nearly horizontal: recoil may rotate it by no more than 5°. Show restrained wrist and arm recoil without swapping hands, changing the gun's shape or introducing additional weapons. Preserve the hand-to-grip connection in every phase.

## Kura: eight-phase wing flutter

Preserve the black ears and tail, magenta/teal hair, pale flower, black/violet striped sweater, skirt, violet boots and small wings from the reference. Produce eight cells in a four-column, two-row grid of a wing-driven lift: preparation, wings lifting, opening, downstroke, lift follow-through, a lighter flutter, settling and recovery. Order the cells left to right and then top to bottom. The wings drive the gesture; keep the torso upright and the feet readable. Avoid an arm-pushing pose or an invented hand-held propulsion device.

The game uses B for a vertical wing lift once per flight. The action lasts 30 engine ticks in total, including lift and the gentle descent limit while the action remains active; it does not add a separate 30-tick descent afterward. Do not paint movement trails that imply a different direction or an unlimited flight mode.

## Seven source families

All source paths are repository-relative, under `graphics/sa2/ocs/source/`. The frozen `graphics/sa2/ocs/revised_asset_manifest.json` records source hashes, selected frames and conversion settings. Seven families use eight source images, because Kiro has a one-frame source override:

| Family | Source filename convention | Layout used by the compiler |
| --- | --- | --- |
| Jude run | `jude_run_source_v3.png` | 4 columns × 2 rows |
| Kiro run | `kiro_run_source_v3.png` | 4 columns × 2 rows; frame 5 overridden below |
| Yuliana run | `yuliana_run_source_v3.png` | 4 columns × 2 rows |
| Jude weapon | `jude_motion_v3.png` | 4 columns × 4 rows; last two rows used |
| Kiro weapon | `kiro_motion_v3.png` | 4 columns × 4 rows; last two rows used |
| Yuliana weapon | `yuliana_motion_v3.png` | 4 columns × 4 rows; last two rows used |
| Kura wing action | `kura_ability_v3.png` | 4 columns × 2 rows |

The auxiliary `graphics/sa2/ocs/source/kiro_down_source_v3.png` contains eight generated poses, but only the complete zero-based frame 5 is imported. The other seven calibrated Kiro run frames remain from the base source. The override, its hash, scale and anchors are recorded in the manifest/configuration and pass through the same palette codec; no character pixels are painted by code.

All seven final native atlases have four columns and two rows of 64×64 cells, producing 256×128 images. Inspection GIFs named `*-v3-review.gif` intentionally show 100 ms per pose. The additional run `*-v3-30fps.gif` files approximate maximum visual cadence with GIF centisecond timing; neither kind reports phone FPS. Real weapon actions use their own phase durations over 14, 22 or 24 engine ticks. Actual engine VBlank captures are separate from these art previews. Accepted source sheets, successful 4bpp packing and passing real-engine visual checks are separate recorded checks.

## Frozen structural references and per-pose override

The run guide is checked in at `tools/oc_sprites/references/run_gait_guide.svg` and `.png`. It defines the eight contact/down/passing/flight phases on alternating support legs. Character body height remains 36 native pixels, or 38 for Yuliana.

Kiro additionally uses `source/kiro_down_source_v3.png`: only complete source frame 5 replaces run frame 5. The other seven native run poses remain byte-identical to their approved hashes. `revised_run_alignment.json` records the explicit source, scale and body anchor; `revised_asset_manifest.json` records all eight source hashes and the final poses.
