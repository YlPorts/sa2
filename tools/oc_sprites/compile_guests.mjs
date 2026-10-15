#!/usr/bin/env node
/* Pack newly authored guest artwork. No existing art is drawn, repaired or regenerated. */
import fs from 'node:fs';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {sharp,sha256,rgb555,components,encodeFrame,assertPreserved} from './revised_codec.mjs';

const root=path.resolve(path.dirname(new URL(import.meta.url).pathname),'../..');
const assets=path.join(root,'graphics/sa2/ocs'),dst=path.join(assets,'guests');
const previews=process.env.OC_GUEST_PREVIEW_DIR||path.join(root,'../guest-verification');
const names=['sans','spiderman'];
const families={base:{count:32,cols:8,rows:4},motion:{count:24,cols:6,rows:4},rear:{count:8,cols:4,rows:2},run:{count:8,cols:4,rows:2}};
const labels={base:['idle','blink','breathe','run_a','run_b','run_c','run_d','run_e','run_f','crouch','launch','rise','tuck','fall','land','brake_a','brake_b','brake_c','hurt_a','hurt_b','death','balance_a','balance_b','hang','push','grind','swim','victory','wave','attack','curl_a','curl_b'],motion:['contact_a','down_a','passing_a','flight_a','contact_b','down_b','passing_b','flight_b','jump_launch','jump_rise','jump_tuck','jump_fall','anticipation_0','anticipation_1','active_0','active_1','active_2','recovery_0','recovery_1','recovery_2','victory_0','victory_1','victory_2','victory_3'],rear:['rear_run_a','rear_run_b','rear_run_c','rear_run_d','rear_turn_left','rear_turn_right','rear_jump','rear_hurt']};
const actionMap=[0,1,2,3,4,5,6,7,0,1,2,3,8,9,10,11,12,14,16,18,20,21,22,23];
const exportsMap={frame:{source:'base',map:Array.from({length:32},(_,i)=>i),cols:8},special:{source:'rear',map:Array.from({length:8},(_,i)=>i),cols:4},action:{source:'motion',map:actionMap,cols:6},revised_run:{source:'motion',map:Array.from({length:8},(_,i)=>i),cols:4},revised_attack:{source:'motion',map:Array.from({length:8},(_,i)=>12+i),cols:4}};
labels.run=labels.motion.slice(0,8);
function exportSources(family,m,hasRun) {
  return m.map.map((index,i)=>hasRun&&((family==='frame'&&i>=3&&i<=8)||(family==='action'&&i<12)||family==='revised_run')?{family:'run',index:family==='frame'?i-3:i%8}:{family:m.source,index});
}
function exportFrames(name,family){return manifest.characters[name].exports[family].sourceFrames.map(f=>output[name][f.family][f.index]);}
const args=process.argv.slice(2),inspect=args.includes('--inspect'),emit=args.includes('--emit');
const approvalIndex=args.indexOf('--approval'),approvalPath=approvalIndex>=0?path.resolve(args[approvalIndex+1]||''):null;
if(args.some(a=>!['--inspect','--preview','--emit','--approval',args[approvalIndex+1]].includes(a)))throw new Error('Usage: compile_guests.mjs [--inspect|--preview|--emit --approval APPROVED.json]');
if(emit&&!approvalPath)throw new Error('Publishing requires an exact reviewed preview manifest: --emit --approval APPROVED.json');
fs.mkdirSync(dst,{recursive:true});fs.mkdirSync(previews,{recursive:true});
const configPath=path.join(dst,'alignment.json'),config=JSON.parse(fs.readFileSync(configPath,'utf8'));
const frozenPath=path.join(dst,'preserved_sha256.json');
if(!fs.existsSync(frozenPath)) {
  const entries={};
  function scan(p){for(const e of fs.readdirSync(p,{withFileTypes:true})){const q=path.join(p,e.name);if(q===dst)continue;if(e.isDirectory())scan(q);else entries[path.relative(root,q)]=sha256(fs.readFileSync(q));}}
  scan(assets);fs.writeFileSync(frozenPath,JSON.stringify(entries,null,2)+'\n');
}
const frozen=JSON.parse(fs.readFileSync(frozenPath,'utf8'));assertPreserved(root,frozen);
const rgbTo555=(r,g,b)=>Math.round(r*31/255)|(Math.round(g*31/255)<<5)|(Math.round(b*31/255)<<10);
const dist=(a,b)=>(a[0]-b[0])**2*.32+(a[1]-b[1])**2*.4+(a[2]-b[2])**2*.28;
function paletteFor(frames,data,explicit) {
  if(explicit) {
    if(explicit.length!==15)throw new Error('Explicit palettes require exactly fifteen opaque RGB555 colors');
    const p=[0,...explicit.map(v=>typeof v==='number'?v:parseInt(v.replace(/^0x/,''),16))];
    if(p.some(v=>!Number.isInteger(v)||v<0||v>32767)||new Set(p.slice(1)).size!==15)throw new Error('Invalid or duplicated opaque RGB555 colors');
    return p;
  }
  const histogram=new Map();for(const f of frames)for(const p of f.bodyPixels){const v=rgbTo555(data[p*4],data[p*4+1],data[p*4+2]);histogram.set(v,(histogram.get(v)||0)+1);}
  const colors=[...histogram].map(([v,n])=>({v,n,rgb:rgb555(v)}));
  if(colors.length<15)throw new Error(`Base artwork contains only ${colors.length} RGB555 colors; provide an explicit identity palette`);
  const darkest=colors.reduce((a,b)=>a.rgb.reduce((x,y)=>x+y)<b.rgb.reduce((x,y)=>x+y)?a:b);
  const centers=[darkest.rgb];
  while(centers.length<15){let best,score=-1;for(const c of colors){const s=Math.min(...centers.map(p=>dist(c.rgb,p)))*Math.pow(c.n,.45);if(s>score){best=c;score=s;}}centers.push([...best.rgb]);}
  for(let pass=0;pass<24;pass++) {
    const sums=centers.map(()=>[0,0,0,0]);
    for(const c of colors){let index=0,d=Infinity;centers.forEach((p,i)=>{const q=dist(c.rgb,p);if(q<d){index=i;d=q;}});for(let k=0;k<3;k++)sums[index][k]+=c.rgb[k]*c.n;sums[index][3]+=c.n;}
    for(let i=1;i<15;i++)if(sums[i][3])centers[i]=sums[i].slice(0,3).map(v=>v/sums[i][3]);
  }
  const palette=[0,...centers.map(p=>rgbTo555(...p))];
  if(new Set(palette.slice(1)).size!==15)throw new Error('Palette quantization collapsed entries; configure explicit palette');return palette;
}
function extractCells(data,w,h,spec,opts) {
  const all=components(data,w,h),out=[],unassigned=[];
  const cells=Array.from({length:spec.count},()=>[]);
  for(const c of all) {
    if(c.area<2){unassigned.push({bounds:[c.l,c.t,c.r,c.b],area:c.area});continue;}
    const col=Math.min(spec.cols-1,Math.floor(c.cx*spec.cols/w)),row=Math.min(spec.rows-1,Math.floor(c.cy*spec.rows/h));
    /* A generated atlas has nominal cells, not painted cell borders. Retain complete
     * components even when a pose extends beyond an imaginary equal-size boundary. */
    cells[row*spec.cols+col].push(c);
  }
  for(let i=0;i<spec.count;i++) {
    const found=cells[i].sort((a,b)=>b.area-a.area),align=opts.frames?.[i]||{};
    if(!found.length)throw new Error(`Grid cell ${i} contains no actor`);
    const bodyIndices=align.bodyComponents||[0];
    if(bodyIndices.some(n=>!found[n]))throw new Error(`Cell ${i} body component index does not exist`);
    const bodyPixels=bodyIndices.flatMap(n=>found[n].pixels),bodyBounds=align.bodyBounds||[Math.min(...bodyIndices.map(n=>found[n].l)),Math.min(...bodyIndices.map(n=>found[n].t)),Math.max(...bodyIndices.map(n=>found[n].r)),Math.max(...bodyIndices.map(n=>found[n].b))];
    const renderIndices=align.includeComponents||found.map((_,n)=>n),render=renderIndices.map(n=>found[n]);if(render.some(c=>!c))throw new Error(`Cell ${i}: invalid complete component selection`);
    const actor={l:Math.min(...render.map(c=>c.l)),t:Math.min(...render.map(c=>c.t)),r:Math.max(...render.map(c=>c.r)),b:Math.max(...render.map(c=>c.b)),pixels:render.flatMap(c=>c.pixels),bodyBounds,bodyPixels,secondaryComponents:render.filter(c=>!bodyIndices.map(n=>found[n]).includes(c)).map(c=>({bounds:[c.l,c.t,c.r,c.b],area:c.area})),sourceComponents:found,selectedCompleteComponents:renderIndices,excludedCompleteComponents:found.map((_,n)=>n).filter(n=>!renderIndices.includes(n))};
    out.push(actor);
  }
  return {frames:out,unassigned};
}
function extractRectangles(data,w,h,spec,opts) {
  if(opts.rectangles.length!==spec.count)throw new Error(`Expected ${spec.count} complete source rectangles, received ${opts.rectangles.length}`);
  const frames=[];
  for(let i=0;i<spec.count;i++) {
    const [l,t,cw,ch]=opts.rectangles[i],align=opts.frames?.[i]||{};
    if(![l,t,cw,ch].every(Number.isInteger)||l<0||t<0||cw<1||ch<1||l+cw>w||t+ch>h)throw new Error(`Source rectangle ${i} is outside the original sheet`);
    const crop=Buffer.alloc(cw*ch*4);
    for(let y=0;y<ch;y++)data.copy(crop,y*cw*4,((t+y)*w+l)*4,((t+y)*w+l+cw)*4);
    /* Remove only source background connected to the crop boundary. Enclosed
     * white eye lenses and highlights are original artwork and remain opaque. */
    const key=opts.colorkeyRGB;
    if(key) {
      const visited=new Uint8Array(cw*ch),pending=[];
      const add=p=>{if(p<0||p>=cw*ch||visited[p])return;visited[p]=1;if(key.every((v,k)=>crop[p*4+k]===v))pending.push(p);};
      for(let x=0;x<cw;x++){add(x);add((ch-1)*cw+x);}for(let y=0;y<ch;y++){add(y*cw);add(y*cw+cw-1);}
      while(pending.length){const p=pending.pop(),x=p%cw,y=Math.floor(p/cw);crop[p*4+3]=0;if(x)add(p-1);if(x+1<cw)add(p+1);if(y)add(p-cw);if(y+1<ch)add(p+cw);}
    }
    const found=components(crop,cw,ch).filter(c=>c.area>=2).sort((a,b)=>b.area-a.area);
    if(!found.length)throw new Error(`Source rectangle ${i} contains no complete actor`);
    for(let x=0;x<cw;x++)if(crop[x*4+3]>=200||crop[((ch-1)*cw+x)*4+3]>=200)throw new Error(`Source rectangle ${i} clips its top/bottom; use the whole pose with transparent margin`);
    for(let y=0;y<ch;y++)if(crop[(y*cw)*4+3]>=200||crop[(y*cw+cw-1)*4+3]>=200)throw new Error(`Source rectangle ${i} clips its left/right; use the whole pose with transparent margin`);
    const mapped=found.map(c=>({...c,l:c.l+l,t:c.t+t,r:c.r+l,b:c.b+t,pixels:c.pixels.map(p=>(t+Math.floor(p/cw))*w+l+p%cw)}));
    const bodyIndices=align.bodyComponents||[0];if(bodyIndices.some(n=>!mapped[n]))throw new Error(`Source rectangle ${i}: invalid body component`);
    const bodyPixels=bodyIndices.flatMap(n=>mapped[n].pixels),bodyBounds=align.bodyBounds||[Math.min(...bodyIndices.map(n=>mapped[n].l)),Math.min(...bodyIndices.map(n=>mapped[n].t)),Math.max(...bodyIndices.map(n=>mapped[n].r)),Math.max(...bodyIndices.map(n=>mapped[n].b))];
    const renderIndices=align.includeComponents||mapped.map((_,n)=>n),render=renderIndices.map(n=>mapped[n]);if(render.some(c=>!c))throw new Error(`Source rectangle ${i}: invalid complete component selection`);
    frames.push({l:Math.min(...render.map(c=>c.l)),t:Math.min(...render.map(c=>c.t)),r:Math.max(...render.map(c=>c.r)),b:Math.max(...render.map(c=>c.b)),pixels:render.flatMap(c=>c.pixels),bodyBounds,bodyPixels,sourceComponents:mapped,secondaryComponents:render.filter(c=>!bodyIndices.map(n=>mapped[n]).includes(c)).map(c=>({bounds:[c.l,c.t,c.r,c.b],area:c.area})),sourceRectangle:[l,t,cw,ch],selectedCompleteComponents:renderIndices,excludedCompleteComponents:mapped.map((_,n)=>n).filter(n=>!renderIndices.includes(n))});
  }
  return {frames,unassigned:[]};
}
function anchor(f,w) {
  const height=f.bodyBounds[3]-f.bodyBounds[1]+1,xs=[];
  for(const p of f.bodyPixels){const y=Math.floor(p/w),v=(y-f.bodyBounds[1])/height;if(v>=.57&&v<=.72)xs.push(p%w);}
  xs.sort((a,b)=>a-b);return xs.length?xs[Math.floor(xs.length/2)]:(f.bodyBounds[0]+f.bodyBounds[2])/2;
}
async function writeAtlas(frames,cols,file,scale=1) {
  const rows=Math.ceil(frames.length/cols),raw=Buffer.alloc(cols*64*rows*64*4);
  frames.forEach((f,i)=>{for(let y=0;y<64;y++)f.rgba.copy(raw,((Math.floor(i/cols)*64+y)*cols*64+i%cols*64)*4,y*256,(y+1)*256);});
  let image=sharp(raw,{raw:{width:cols*64,height:rows*64,channels:4}});if(scale!==1)image=image.resize(cols*64*scale,rows*64*scale,{kernel:'nearest'});await image.png().toFile(file);
}
function cInclude(characters,exportName) {
  let text='/* New guest artwork packed by compile_guests.mjs; old identities remain in their original include. */\n';
  for(const name of names){text+=`{ /* ${name} */\n`;characters[name].forEach((f,i)=>{text+=`    { /* ${i}: ${exportName} */\n`;for(let p=0;p<f.tiles.length;p+=32)text+='        '+[...f.tiles.subarray(p,p+32)].map(v=>'0x'+v.toString(16).padStart(2,'0')).join(',')+',\n';text+='    },\n';});text+='},\n';}return text;
}
async function animations(name,kind,frames) {
  const files=[];
  for(let i=0;i<frames.length;i++){const file=path.join(previews,`${name}-${kind}-${i}.png`);await sharp(frames[i].rgba,{raw:{width:64,height:64,channels:4}}).resize(256,256,{kernel:'nearest'}).png().toFile(file);files.push(file);}
  for(const [suffix,list,delays] of [['review',files,files.map(()=>10)],...(kind==='run'?[['30fps',[...files,...files,...files],Array.from({length:24},(_,i)=>[3,3,4][i%3])]]:[])]) {
    const call=['-loop','0','-dispose','background','-background','none'];list.forEach((p,i)=>call.push('-delay',String(delays[i]),p));call.push(path.join(previews,`${name}-${kind}-${suffix}.gif`));
    const r=spawnSync('convert',call,{encoding:'utf8'});if(r.status!==0)throw new Error(`GIF conversion failed: ${r.stderr}`);
  }
}
function packProjectile(f,data,w,palette,maxSize=[6,6],paletteIndices=Array.from({length:15},(_,i)=>i+1)) {
  if(maxSize.length!==2||maxSize.some(n=>!Number.isInteger(n)||n<1||n>8))throw new Error('Projectile size must fit one complete 8x8 tile');
  const fw=f.r-f.l+1,fh=f.b-f.t+1,scale=Math.min(maxSize[0]/fw,maxSize[1]/fh),dw=Math.max(1,Math.round(fw*scale)),dh=Math.max(1,Math.round(fh*scale)),ox=Math.floor((8-dw)/2),oy=Math.floor((8-dh)/2),own=new Set(f.pixels),indices=new Uint8Array(64),rgba=Buffer.alloc(256),colors=palette.map(rgb555);
  if(!paletteIndices.length||paletteIndices.some(i=>!Number.isInteger(i)||i<1||i>15))throw new Error('Projectile palette subset must use existing opaque identity entries');
  for(let y=0;y<dh;y++)for(let x=0;x<dw;x++){const sx=f.l+Math.min(fw-1,Math.floor((x+.5)/scale)),sy=f.t+Math.min(fh-1,Math.floor((y+.5)/scale)),p=sy*w+sx;if(!own.has(p))continue;const rgb=[data[p*4],data[p*4+1],data[p*4+2]];let best=paletteIndices[0],d=Infinity;for(const i of paletteIndices){const q=dist(rgb,colors[i]);if(q<d){d=q;best=i;}}const q=(oy+y)*8+ox+x;indices[q]=best;for(let k=0;k<3;k++)rgba[q*4+k]=colors[best][k];rgba[q*4+3]=255;}
  const tiles=Buffer.alloc(32);for(let p=0;p<64;p+=2)tiles[p/2]=indices[p]|(indices[p+1]<<4);return {rgba,indices,tiles,scale,size:[dw,dh],sourceBounds:[f.l,f.t,f.r,f.b],maxSize,tileEdgeAllowed:maxSize.includes(8),paletteIndices};
}
const manifest={version:1,alphaCutoff:200,frameSize:[64,64],pivot:[32,48],order:names,palettePolicy:'Fifteen fixed opaque RGB555 entries per identity selected from base artwork or explicit identity palette. All body families use that palette; a projectile may quantize to an explicitly recorded subset of its existing neutral entries.',packingPolicy:'Only nearest-neighbor scale, pose translation, complete-component selection and fixed palette quantization. No redraw, synthetic poses, mirrored poses, or original-character replacements.',frameRemaps:exportsMap,runPlayback:{reviewPoseDelayMs:100,thirtyFpsGif:{frameSequence:[0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7],delayCentiseconds:Array.from({length:24},(_,i)=>[3,3,4][i%3]),cycleMilliseconds:800,poseChangesPerSecond:30}},preservedAssets:path.relative(root,frozenPath),preservedAssetsSHA256:sha256(fs.readFileSync(frozenPath)),configSHA256:sha256(fs.readFileSync(configPath)),characters:{}};
const output={},projectiles={};
for(const name of names) {
  const cfg=config[name],sources={};
  for(const [family,spec] of Object.entries(families)) {
    const opts=cfg.sources?.[family]||{},file=opts.source?path.resolve(root,opts.source):path.join(dst,'source',`${name}_${family}.png`);
    if(family==='run'&&!cfg.sources?.run&&!fs.existsSync(file))continue;
    if(!fs.existsSync(file)){if(emit)throw new Error(`Missing approved source: ${file}`);console.log(`${name} ${family}: awaiting artwork`);continue;}
    const bytes=fs.readFileSync(file),{data,info}=await sharp(bytes).ensureAlpha().raw().toBuffer({resolveWithObject:true});
    const found=opts.rectangles?extractRectangles(data,info.width,info.height,spec,opts):extractCells(data,info.width,info.height,spec,opts);
    if(opts.frameOrder){if(opts.frameOrder.length!==spec.count||new Set(opts.frameOrder).size!==spec.count||opts.frameOrder.some(i=>!Number.isInteger(i)||i<0||i>=spec.count))throw new Error(`${name} ${family}: frameOrder must contain every complete source pose once`);found.frames=opts.frameOrder.map(i=>({...found.frames[i],authoredSourceIndex:i}));}
    sources[family]={file,bytes,data,info,opts,...found};
    if(inspect)console.log(JSON.stringify({name,family,size:[info.width,info.height],unassigned:found.unassigned,frames:found.frames.map((f,i)=>({index:i,label:labels[family][i],bodyBounds:f.bodyBounds,fullBounds:[f.l,f.t,f.r,f.b],bodyAnchorX:anchor(f,info.width),components:f.sourceComponents.map(c=>({bounds:[c.l,c.t,c.r,c.b],area:c.area}))}))},null,2));
  }
  if(inspect||!sources.base)continue;
  const palette=paletteFor(sources.base.frames,sources.base.data,cfg.palette),meta={referenceBodyHeight:cfg.bodyHeight,palette:palette.map(v=>'0x'+v.toString(16).padStart(4,'0')),sources:{},frames:{},exports:{}};manifest.characters[name]=meta;output[name]={};
  for(const [family,s] of Object.entries(sources)) {
    const {opts,frames,info,data}=s,reference=frames[opts.referenceFrame??0],referenceHeight=opts.referenceBodyHeight??(reference.bodyBounds[3]-reference.bodyBounds[1]+1),scale=opts.scale??cfg.bodyHeight/referenceHeight;
    const encoded=frames.map((f,i)=>{const sourceIndex=f.authoredSourceIndex??i,a=opts.frames?.[sourceIndex]||{},hip=a.bodyAnchorX??anchor(f,info.width),ground=a.groundY??f.bodyBounds[3];let e;try{e=encodeFrame(f,data,info.width,info.height,scale,hip,ground,palette,name);}catch(error){throw new Error(`${name} ${family}${i}: ${error.message}`);}e.bodyHeight=Math.round((f.bodyBounds[3]-f.bodyBounds[1]+1)*scale);e.selectedCompleteComponents=f.selectedCompleteComponents;e.excludedCompleteComponents=f.excludedCompleteComponents;e.authoredSourceIndex=sourceIndex;return e;});
    output[name][family]=encoded;
    meta.sources[family]={source:path.relative(root,s.file),sourceSHA256:sha256(s.bytes),sourceSize:[info.width,info.height],scale,referenceFrame:opts.referenceFrame??0,referenceBodyHeight:referenceHeight,unassignedComponents:s.unassigned,...(opts.frameOrder?{authoredPoseOrder:opts.frameOrder,gaitDescription:opts.gaitDescription}:{}),...(opts.rectangles?{completePoseRectangles:opts.rectangles,colorkeyPolicy:'Only exact source background RGB connected to each rectangle boundary is made transparent; enclosed eye/highlight pixels are preserved.',colorkeyRGB:opts.colorkeyRGB}:{}),...(opts.poseRoles?{poseRoles:opts.poseRoles}:{}),...(opts.attribution?{attribution:opts.attribution}:{})};
    meta.frames[family]=encoded.map((f,i)=>{const{rgba,indices,tiles,...m}=f;return {index:i,name:opts.outputLabels?.[i]??labels[family][i],...m,sha256:sha256(tiles)};});
    await writeAtlas(encoded,families[family].cols,path.join(previews,`${name}-${family}-native-4x.png`),4);
    if(family==='motion')await animations(name,'attack',encoded.slice(12,20));
    if(family==='run'||(family==='motion'&&!sources.run)){await animations(name,'run',encoded.slice(0,8));const unique=new Set(encoded.slice(0,8).map(f=>sha256(f.tiles))).size;if(unique!==8)throw new Error(`${name}: only ${unique}/8 distinct run poses`);meta.distinctRunFrames=unique;meta.runSourceFamily=family;}
    console.log(`${name} ${family}: ${encoded.length} poses, constant scale ${scale.toFixed(6)}, body reference ${cfg.bodyHeight}px`);
  }
  if(cfg.projectile&&(cfg.projectile.source||sources[cfg.projectile.family])) {
    let s,frame,sourceMeta={};
    if(cfg.projectile.source) {
      const file=path.resolve(root,cfg.projectile.source),bytes=fs.readFileSync(file),{data,info}=await sharp(bytes).ensureAlpha().raw().toBuffer({resolveWithObject:true});
      const f=extractRectangles(data,info.width,info.height,{count:1},{rectangles:[cfg.projectile.rectangle],colorkeyRGB:cfg.projectile.colorkeyRGB});
      s={data,info};frame=f.frames[0];sourceMeta={source:path.relative(root,file),sourceSHA256:sha256(bytes),sourceSize:[info.width,info.height],completeSourceRectangle:cfg.projectile.rectangle,colorkeyRGB:cfg.projectile.colorkeyRGB};
    }else{s=sources[cfg.projectile.family];frame=s.frames[cfg.projectile.frame];}
    const selected=(cfg.projectile.components??[cfg.projectile.component??0]).map(i=>frame.sourceComponents[i]);
    if(!selected.length||selected.some(c=>!c))throw new Error(`${name}: projectile component selection invalid`);
    const selectedFigure={l:Math.min(...selected.map(c=>c.l)),t:Math.min(...selected.map(c=>c.t)),r:Math.max(...selected.map(c=>c.r)),b:Math.max(...selected.map(c=>c.b)),pixels:selected.flatMap(c=>c.pixels)};
    const p=packProjectile(selectedFigure,s.data,s.info.width,palette,cfg.projectile.maxSize,cfg.projectile.paletteIndices);projectiles[name]=p;
    meta.projectile={sourceFamily:cfg.projectile.family,sourceFrame:cfg.projectile.frame,...sourceMeta,completeSourceComponents:cfg.projectile.components??[cfg.projectile.component??0],...Object.fromEntries(Object.entries(p).filter(([k])=>!['rgba','indices','tiles'].includes(k))),sha256:sha256(p.tiles)};
    await sharp(p.rgba,{raw:{width:8,height:8,channels:4}}).resize(128,128,{kernel:'nearest'}).png().toFile(path.join(previews,`${name}-projectile-native-16x.png`));
  }
  for(const [family,m] of Object.entries(exportsMap))if(output[name][m.source]){const sourceFrames=exportSources(family,m,Boolean(output[name].run));meta.exports[family]={sourceFamily:m.source,sourceFrameIndices:m.map,sourceFrames,frameSHA256:sourceFrames.map(f=>sha256(output[name][f.family][f.index].tiles))};}
}
assertPreserved(root,frozen);
if(inspect)process.exit(0);
const previewManifest=path.join(previews,'guest_asset_manifest.preview.json'),manifestBytes=Buffer.from(JSON.stringify(manifest,null,2)+'\n'),manifestSHA256=sha256(manifestBytes);
fs.writeFileSync(previewManifest,manifestBytes);console.log(`Preview manifest SHA256: ${manifestSHA256}`);
if(!emit){console.log('Preview only: no guest production arrays published.');process.exit(0);}
for(const name of names)if(!['base','motion','rear'].every(f=>output[name]?.[f])||!projectiles[name])throw new Error(`All six source families and both complete projectile glyphs must be reviewed before publication (${name} incomplete)`);
const approval=JSON.parse(fs.readFileSync(approvalPath,'utf8'));if(approval.manifestSHA256!==manifestSHA256)throw new Error('Artwork/config changed since the reviewed preview; root must inspect and approve this exact manifest');
const stage=path.join(dst,'.generated-stage');fs.rmSync(stage,{recursive:true,force:true});fs.mkdirSync(stage);
for(const name of names) {
  const pal=Buffer.alloc(32);manifest.characters[name].palette.forEach((v,i)=>pal.writeUInt16LE(parseInt(v.slice(2),16),i*2));fs.writeFileSync(path.join(stage,`${name}.gbapal`),pal);
  for(const [family,m] of Object.entries(exportsMap)) {
    const frames=exportFrames(name,family);fs.writeFileSync(path.join(stage,`${name}_${family}.4bpp`),Buffer.concat(frames.map(f=>f.tiles)));await writeAtlas(frames,m.cols,path.join(stage,`${name}_${family}.png`));
  }
  fs.writeFileSync(path.join(stage,`${name}_projectile.4bpp`),projectiles[name].tiles);await sharp(projectiles[name].rgba,{raw:{width:8,height:8,channels:4}}).png().toFile(path.join(stage,`${name}_projectile.png`));
}
for(const [family,m] of Object.entries(exportsMap))fs.writeFileSync(path.join(stage,`${family}_tiles.inc`),cInclude(Object.fromEntries(names.map(n=>[n,exportFrames(n,family)])),family));
fs.writeFileSync(path.join(stage,'palettes.inc'),names.map(n=>`    { ${manifest.characters[n].palette.join(', ')} }, /* ${n} */`).join('\n')+'\n');
fs.writeFileSync(path.join(stage,'projectile_tiles.inc'),names.map(n=>`    { ${[...projectiles[n].tiles].map(v=>'0x'+v.toString(16).padStart(2,'0')).join(', ')} }, /* ${n} */`).join('\n')+'\n');
fs.writeFileSync(path.join(stage,'guest_asset_manifest.json'),manifestBytes);fs.writeFileSync(path.join(stage,'approval.json'),JSON.stringify({manifestSHA256},null,2)+'\n');
for(const name of names)for(const s of Object.values(manifest.characters[name].sources))if(sha256(fs.readFileSync(path.join(root,s.source)))!==s.sourceSHA256)throw new Error(`Source changed while staging: ${s.source}`);
for(const name of names){const p=manifest.characters[name].projectile;if(p.source&&sha256(fs.readFileSync(path.join(root,p.source)))!==p.sourceSHA256)throw new Error(`Projectile artwork changed while staging: ${p.source}`);}
assertPreserved(root,frozen);
const generated=path.join(dst,'generated'),backup=path.join(dst,'.generated-backup');fs.rmSync(backup,{recursive:true,force:true});if(fs.existsSync(generated))fs.renameSync(generated,backup);
try{fs.renameSync(stage,generated);}catch(e){if(fs.existsSync(backup))fs.renameSync(backup,generated);throw e;}
fs.rmSync(backup,{recursive:true,force:true});console.log('Published both reviewed guest identities atomically. Every existing OC asset retains its original SHA256.');
