# Guest sprite generation prompts · OCS4

Sans was generated with the built-in image generation tool. Spider-Man uses the complete original GBA sprite atlas credited in the asset manifest. The compiler only extracts complete poses, scales, aligns, quantizes and packs artwork; it never paints new artwork. Generated sources have transparent alpha even where RGB behind alpha zero retains a preview backdrop.

## Sans base · 32 poses

Use case: stylized-concept. Asset type: a production sprite sheet for a Sonic Advance 2 GBA platform game.
Create exactly 32 separate sprites of the SAME classic Sans from Undertale, on a truly transparent RGBA background. Canvas1536x1024, eight columns by four rows; read left to right, top to bottom. No labels/grid/text.
Sans is a short squat white skeleton, round large skull, black eye sockets, toothy friendly grin, unzipped cyan-blue hoodie over a white shirt, black shorts with a pale side stripe, white bony legs, pink-gray house slippers. NO hair, human skin, Sonic ears/quills, red eye, big weapon or ball.
Adapt him to authentic compact Sonic Advance pixel art: dark crisp one-pixel-style outlines, bold legible blocks, 15 flat colors plus transparent, simple two-level cel shading; absolutely no soft gradients, smooth illustration edges or blur. Body proportions must remain short and charming: head about42% of standing height, distinct small torso, short legs. If reduced to native resolution the standing character must read at32 pixels high. Keep the SAME skull, hoodie shape and limb thickness in all32poses.
Use192x256 cells; idle standing fullbody height about190 sourcepixels, feet near y218 of each cell, centered around x96. Generous transparent margins, no figure touching another cell. All fullfigures face RIGHT, slightly three-quarter but readable side silhouette. Action poses crouch/raise naturally, never individually zoom the subject. Never turn the whole character into Sonic's spinning ball.
32 frame order EXACTLY:
row1: idle hands in pockets; blink eye sockets subtly closed; breathe lifted shoulders; running right A near-leg contact; running B down/compressed; running C passing; running D airborne flight; running E far-leg contact.
row2: running F far-leg passing; crouch; jump launch crouched ready; rising jump upright arms lifted; jump tuck knees forward while skull upright; falling jump legs extend; landing flexed knees; braking A leanback planted heel.
row3: braking B skid; braking C settle; hurt A recoiling; hurt B wincing; death collapsing gracefully; balancing A on tiptoe; balancing B recover; hanging with both hands above.
row4: pushing with palms forward; grinding crouched balance; swimming relaxed kick; victory shrug; wave with friendly grin; attack palm extended forward; compact airborne humanoid tuck A; compact tuck B.
The six running drawings must change actual knees, ankles and opposite arm position, with both legs visible as clear alternate silhouettes; no rigid sliding. Exactly ONE complete figure percell, no missing body parts, no detached props or decorative effects.

## Sans motion · 24 source poses

Use case: identity-preserve. Asset type: transparent production GAME SPRITE SHEET. Make a new 24-pose animation atlas of Sans from the first input, maintaining his exact short skeleton proportions, round skull/grin, cyan hoodie, white shirt, black shorts and pink-gray house slippers.
Image1 is the identity and pixel-art style reference ONLY; do not reuse its poses rigidly. Image2 is ONLY a guide to alternate-leg running geometry; do not copy the guide's human head, long legs, colors, lines or background. Keep Sans' big round head and short legs.
Canvas1536x1024 exactly, SIX columns by FOUR rows, twentyfour complete sprites. Genuine transparent background; no labels, letters, grid, floor or scenery. Hard crisp GBA pixel art, black pixel outlines, fifteen flat colors+alpha, no gradients/blur. Standing body height equivalent32 nativepixels with skull≈42%. Same constant size everywhere. Each256x256cell fullbody facesRIGHT, feet referencey218, centeredx128, safe margins aroundhead/feet/FX.
Frame order and required poses:
Row1: run0 near/light bony leg forward contact on heel, far/gray leg trails; run1 near leg bent compressed with planted sole UNDER pelvis, far leg folded behind; run2 near supporting leg behind hip while far knee passes forward; run3 airborne flight with near leg trailing and far knee leading; run4 far/gray leg forward contact while near/light leg trails; run5 far leg down/compressed soleUNDER pelvis, near leg bent trailing.
Row2: run6 far support behind hip, near knee passes forward; run7 flight with far leg trailing/nearlegleading; rising jump armsup; upright knee-tucked jump; descending jump legsextend; landing kneescompressed.
For runs0..7 inverse arm swing follows legs, sleeves/hands separated, heel position and knee flexion clearly different. The second half changes support leg; NEVER repeat the same front leg twice. Skeleton legsremainwhite/gray, slipperslightpink near / mutedpink far, no painted colored human legs. Skull/bodyanchor stable except1-2nativepx naturalbob, shortlegsnotstretched.
Row3: attack0 handstartsleavingpocket noeffect; attack1 palmraises forward preparing; attack2 forwardpalmaim with faintcyanfront eye, nofiredobject yet; attack3 cyanfront eye bright and ONE small WHITE horizontal BONE appears DETACHED just aheadpalm; attack4 keepforwardpalm while bone moves slightlyforward; attack5 armstartsrelaxing with fadingcyaneye and NO bone.
Row4: attack6 loweringarm; attack7 handsbackinpockets, eyeordinaryblack; victory0 relaxedshouldersbothhandpocket; victory1 cheekyshrug palmsup; victory2 friendlywink withslightsmile; victory3 calmhandinpockets.
Bone is readable slender white centralbar with roundeddoubleends and darkgray outline, length equivalent8nativepixels,height3nativepixels. It must be ONE COMPLETE separate connected object, about12sourcepx GAP fromwrist, staying safelyinitsowncell, suitabletoextract as8x8projectilesprite. Onlyattack3/4 containthisonebone each. No big beam, skeletonskullcanon, magiccircles or multiple boneprojectiles. Preserve identity; change only the animationposes as specified.

## Sans rear · 8 poses

Use case: identity-preserve. Create a transparent eight-pose BACK VIEW animation sprite sheet of the exact classic Sans skeleton in the supplied identity reference. Preserve the same big roundwhite skull, short stocky proportions, cyanbluehoodie, blackshorts, whitebonelegs and pink-gray slippers. GBA hardpixelart, crispblackpixeloutline, fifteenflatcolors plusalpha, no blur/gradients/text/grid/backdrop. The bluehoodie has a plain BLUE BACK, no whitefrontshirt or zipperdrawnontheback. Skullfrombackshowsroundwhiteback ofhead andNO face excepta smallfaceedgeinexplicitturns.
Canvas1536x1024, FOUR columns by TWO rows. Exactly8fullfigures, onepercell, consistent scale, safe transparentmargins. Native standing body32pixelshigh, skull42% ofheight, neverstretchlegs or shrinkhead.
Readlefttorighttopbottom. Row1 frames0..3: back view runningAWAY fromcamera, fourconsecutivegaitposes: leftfoot planted/rightleglifted, leftleg compressed/rightfootpasses, rightfootplanted/leftleglifted, rightlegcompressed/leftfootpasses. Armscounter-swing, legsandfeet visiblyalternate actualgeometry ratherthan sliding samepose. We see theBACK ofhoodie/skull; bothhandsandfeet remaincomplete.
Row2: frame4 rearbodyturnheadleft with smallleftedgeeye/grin visible; frame5 rearbodyturnheadright with smallrightedgeeye/grinvisible; frame6 jumpfrombackwithbothkneesbentandhandslifted; frame7 recoilingfrombackwithonehandnearheadandfeetbraced.
Noeffectorprojectile. Keepidentityandpixelsilhouette unchanged, generate onlythe8rearposes; no duplicatefront-facingcharacters.

## Sans dedicated run · 8 poses

Use case: identity-preserve. Make exactly EIGHT complete running poses of classic Sans from Undertale on a truly transparent background, 4 columns by 2 rows, canvas1536x1024. No text, grid, ground, blur or painted backdrop.
FIRST reference is the dominant structural guide for the eight leg and arm positions. Transfer its exact sequence and opposite-leg alternation into Sans' short skeleton body. Do NOT draw the guide's lines, colors or tall proportions. SECOND reference is the identity/style: large round white skull, friendly grin, cyan-blue hoodie with white shirt, black shorts, white bony legs and pink-gray slippers. Keep skull about42% of totalbody height and short legs, body32 nativepixels tall. Crisp GBA pixel blocks, black outlines, fifteen flat colors plusalpha. All fullfigures face RIGHT, same scale and feetbaseline, safe margins, slight1nativepixel naturalbob.
Make the NEAR leg bright WHITE and its slipper LIGHT PINK in every pose; the FAR leg medium neutral GRAY and its slipper darker muted PINK in every pose. Keep these colors attached to the SAME physical legs, visibly separate both legs. Do not swap their brightness to fake motion. Actual leg geometry must switch the supporting leg halfway through.
Exactly onepose percell lefttorighttopbottom:
0 near WHITE leg extends forward heel contact, far GRAY leg behind; near arm behind/far arm forward.
1 near WHITE leg compresses planted sole UNDER pelvis; far GRAY knee folded behind.
2 near WHITE support leg moves behind pelvis; far GRAY knee passes forward aboveground.
3 near WHITE leg trails bent behind, far GRAY knee leads forward, both soles airborne.
4 far GRAY leg extends forward heel contact, near WHITE leg trails behind; near arm forward/far arm behind.
5 far GRAY leg compresses planted sole UNDER pelvis; near WHITE knee folded behind.
6 far GRAY support leg moves behind pelvis; near WHITE knee passes forward aboveground.
7 far GRAY leg trails bent behind, near WHITE knee leads forward, both soles airborne.
Pose0 and4 must visibly have OPPOSITE feet in front. Pose1 and5 must have opposite feet supporting underpelvis. Opposite sleeves/hands swing on the same cycle. No cloned or nearly repeated poses, no sliding rigid legs, no Sonic spin-ball. Only eight full running figures, no jump, magic, prop or detached object.

## Sans run refinement

Edit the FIRST sprite sheet, keeping Sans identity, exact GBA pixel style, transparentalpha background, 4x2 layout, constantfigure scale and all figures0,1,4,5,6,7 unchanged. ONLY revise top-row THIRD and FOURTH figures (indices2 and3), using the SECOND reference guide's corresponding third/fourth running skeleton geometry.
In these TWO poses the FAR medium-GRAY bony leg and its muted darkPINK slipper must be IN FRONT OF THE BODY, with the knee high leading toward RIGHT. The NEAR brightWHITE bony leg with brightlightPINK slipper must be bent trailing BEHIND toward LEFT. This is actual legposition reversal, not simply recoloring an unchangedpose. Show both complete legs separated. The top-row thirdpose is passing: brightWHITE supportfoot behind pelvis, mediumGRAY knee forward. The top-row fourthpose is flight: brightWHITE leg folded rearward, mediumGRAY leg extended forward with bothsoles airborne. Put the NEAR sleeve/hand forward and FAR sleeve backward to counter-swing with these legs.
Do not modify skull/face/hoodie/shirt/shorts proportions. Keep allsix otherdrawings as theyappear inFIRST reference. No new effects, text, scenery, grid or outlines aroundcells. Exactly eight fullrunning figures with the two targetedgeometry corrections.

## Sans separate projectile

Create ONE standalone horizontal BONE projectile sprite for a GBA pixel-art game. Truly transparentbackground. White cartoon skeletonbone with four obvious round knobs: two atLEFT end, two atRIGHT end, a thin straight WHITE shaft joining them. Dark charcoal pixeloutline, small gray loweredge shadow. No skull, text, hand, character or backdrop.
Design explicitly for an EIGHT by EIGHT logicalpixel tile: occupiedarea eight pixelswide and five pixels tall, centered vertically in the tile with transparentrowsabove/below. The LEFT/RIGHT ends should read as rounded DOUBLE LOBES, while the center shaft is narrower. Both ends white, centerwhite, darkoutline perimeter. Highcontrast, hard-squarepixelblocks, no antialiasing, gradient or blur. The finished8x8sprite must unmistakably looklikea bone andnotlikea bulletor capsule.
Return the logicalpixelart greatly enlarged nearestneighbor, canvas1024x1024transparent. Exactly ONE complete connected bone, fully within thecanvas, nothingcropped. Keepa transparentmargin aroundit.
