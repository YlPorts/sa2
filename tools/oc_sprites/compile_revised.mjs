#!/usr/bin/env node
/* Isolated OCS3 exports. Legacy tiles/palettes are never regenerated. */
import fs from 'node:fs';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {sharp,sha256,figures,hipAnchor,encodeFrame,writeAtlas,cInclude,assertPreserved} from './revised_codec.mjs';
const root=path.resolve(path.dirname(new URL(import.meta.url).pathname),'../..');
const dst=path.join(root,'graphics/sa2/ocs');
const previews=process.env.OC_PREVIEW_DIR||path.join(root,'../oc-verification/ocs3');
const names=['jude','kiro','yuliana','kura'],runNames=names.slice(0,3);
const args=process.argv.slice(2),previewOnly=args.includes('--preview'),inspectOnly=args.includes('--inspect');
const custom=args.filter(a=>!a.startsWith('--'));
const defaults=[...runNames.map(n=>path.join(dst,'source',`${n}_run_source_v3.png`)),...runNames.map(n=>path.join(dst,'source',`${n}_motion_v3.png`)),path.join(dst,'source','kura_ability_v3.png')];
const sources=custom.length?custom:defaults;
if(sources.length!==7)throw new Error('Usage: compile_revised.mjs [--preview|--inspect] [JUDE_RUN KIRO_RUN YULI_RUN JUDE_ATTACK KIRO_ATTACK YULI_ATTACK KURA_ABILITY]');
const configs=Object.fromEntries(['run','attack'].map(f=>[f,JSON.parse(fs.readFileSync(path.join(dst,`revised_${f}_alignment.json`),'utf8'))]));
const frozen=JSON.parse(fs.readFileSync(path.join(dst,'ocs2_preserved_sha256.json'),'utf8'));
assertPreserved(root,frozen);fs.mkdirSync(previews,{recursive:true});
if(!previewOnly&&!inspectOnly)for(const p of sources)if(!fs.existsSync(p))throw new Error(`Missing final artwork: ${p}`);
const labels={run:['contact_a','down_a','passing_a','flight_a','contact_b','down_b','passing_b','flight_b'],attack:['anticipation_0','anticipation_1','active_0','active_1','active_2','recovery_0','recovery_1','recovery_2']};
const manifest={version:3,alphaCutoff:200,frameSize:[64,64],pivot:[32,48],palettePolicy:'unchanged OCS2 RGB555 palette',bodyAnchorCriterion:'Explicit pose anchor calibrated from neck/torso, allowing small graphical waist offsets; not a claim of an exact anatomical pelvis.',runOrder:runNames,attackOrder:names,runFrames:labels.run,attackFrames:labels.attack,characters:{}};
const output={run:{},attack:{}},sourceCopies=[];
for(const family of ['run','attack'])for(let c=0;c<(family==='run'?3:4);c++) {
  const name=names[c],inputIndex=family==='run'?c:3+c,filename=path.resolve(sources[inputIndex]);
  if(!fs.existsSync(filename)){console.log(`${name} ${family}: awaiting source artwork`);continue;}
  const sourceBytes=fs.readFileSync(filename);
  const {data,info}=await sharp(sourceBytes).ensureAlpha().raw().toBuffer({resolveWithObject:true});
  if(info.width<512||info.height<400)throw new Error(`${name}: source too small for body-preserving conversion`);
  const count=family==='run'||name==='kura'?8:16,found=figures(data,info.width,info.height,count);
  const components=count===16?found.frames.slice(8):found.frames;
  if(inspectOnly) {
    console.log(JSON.stringify({name,family,source:[info.width,info.height],unassigned:found.unassigned,frames:components.map((f,i)=>({index:i,bodyBounds:f.bodyBounds,fullBounds:[f.l,f.t,f.r,f.b],hipCandidate:hipAnchor(f,data,info.width,name),skinPixels:f.skin,secondary:f.secondaryComponents}))},null,2));continue;
  }
  const opts=configs[family][name];
  if(!opts?.scale||!opts.frames?.every(f=>Number.isFinite(f.bodyAnchorX??f.hipX)&&Number.isFinite(f.groundY))||opts.frames.length!==8) {
    if(previewOnly){console.log(`${name} ${family}: awaiting explicit body scale and eight pelvis/ground anchors`);continue;}
    throw new Error(`${name} ${family}: calibrate one body scale and all eight anchors in revised_${family}_alignment.json; weapons/effects must not define them.`);
  }
  const palFile=path.join(dst,`${name}.gbapal`),pal=fs.readFileSync(palFile),palette=Array.from({length:16},(_,i)=>pal.readUInt16LE(i*2));
  const overrides=new Map();
  for(let i=0;i<8;i++)if(opts.frames[i].sourceOverride) {
    const align=opts.frames[i],overridePath=path.resolve(root,align.sourceOverride),bytes=fs.readFileSync(overridePath);
    const override=await sharp(bytes).ensureAlpha().raw().toBuffer({resolveWithObject:true});
    const sourceFrame=align.sourceFrame,sourceCount=align.sourceFrameCount??8;
    if(!Number.isInteger(sourceFrame)||sourceFrame<0||sourceFrame>=sourceCount||!Number.isFinite(align.sourceScale))throw new Error(`${name} ${family}${i}: source override needs explicit sourceFrame and constant sourceScale`);
    const selected=figures(override.data,override.info.width,override.info.height,sourceCount).frames[sourceFrame];
    overrides.set(i,{figure:selected,data:override.data,info:override.info,scale:align.sourceScale,meta:{index:i,source:path.relative(root,overridePath),sourceSHA256:sha256(bytes),sourceSize:[override.info.width,override.info.height],sourceFrame,sourceFrameCount:sourceCount,scale:align.sourceScale,bodyAnchorX:align.bodyAnchorX??align.hipX,groundY:align.groundY}});
  }
  const frames=components.map((f,i)=> {
    const override=overrides.get(i),frameInfo=override?.info??info;
    try{return encodeFrame(override?.figure??f,override?.data??data,frameInfo.width,frameInfo.height,override?.scale??opts.scale,opts.frames[i].bodyAnchorX??opts.frames[i].hipX,opts.frames[i].groundY,palette,name);}
    catch(err){throw new Error(`${name} ${family}${i}: ${err.message}`);}
  });
  output[family][name]=frames;
  const contactPath=path.join(previews,`${name}-${family}-v3-native-4x.png`);
  await writeAtlas(frames,contactPath,4);
  if(family==='run') {
    let guides='';
    for(let f=0;f<8;f++) {
      const x=(f%4)*256,y=Math.floor(f/4)*256;
      guides+=`<path d="M${x+128} ${y+32}V${y+224}" stroke="#00b5cb" stroke-width="1" stroke-dasharray="4 4" opacity=".8"/><path d="M${x+56} ${y+196}H${x+208}" stroke="#81939d" stroke-width="1"/><text x="${x+12}" y="${y+24}" fill="#d2d8de" font-size="16" font-family="sans-serif">${f}</text>`;
    }
    await sharp(contactPath).composite([{input:Buffer.from(`<svg width="1024" height="512">${guides}</svg>`)}]).png().toFile(path.join(previews,`${name}-run-v3-alignment-inspection.png`));
  }
  for(let f=0;f<8;f++)await sharp(frames[f].rgba,{raw:{width:64,height:64,channels:4}}).resize(256,256,{kernel:'nearest'}).png().toFile(path.join(previews,`${name}-${family}-v3-${f}.png`));
  const images=Array.from({length:8},(_,i)=>path.join(previews,`${name}-${family}-v3-${i}.png`));
  for(const [tag,delay] of family==='run'?[['review',10],['30fps',null]]:[['review',10]]) {
    /* GIF stores centiseconds: alternate 3/4cs for a cycle below the 30fps cap. */
    const gifFrames=images.flatMap((image,i)=>['-delay',String(delay??[3,3,4,3,3,4,3,4][i]),image]);
    const gif=spawnSync('convert',['-loop','0','-dispose','background','-background','none',...gifFrames,path.join(previews,`${name}-${family}-v3-${tag}.gif`)],{encoding:'utf8'});
    if(gif.status!==0)throw new Error(`Cannot render GIF: ${gif.stderr}`);
  }
  const meta=manifest.characters[name]??={referenceHeight:name==='yuliana'?38:36,paletteSource:path.relative(root,palFile),paletteSHA256:sha256(pal),sources:{},frames:{}};
  const canonical=defaults[inputIndex];
  meta.sources[family]={source:path.relative(root,previewOnly?filename:canonical),sourceSHA256:sha256(sourceBytes),sourceSize:[info.width,info.height],scale:opts.scale,selection:count===16?'bottom eight of sixteen':'all eight',unassignedComponents:found.unassigned};
  if(overrides.size)meta.sources[family].frameOverrides=[...overrides.values()].map(v=>v.meta);
  meta.frames[family]=frames.map((f,i)=>{const{rgba,indices,tiles,...m}=f;return {index:i,name:labels[family][i],...m,...(overrides.has(i)?{sourceOverride:overrides.get(i).meta}:{}),sha256:sha256(tiles)};});
  if(filename!==canonical)sourceCopies.push([filename,canonical]);
  if(family==='run') {
    meta.gait=frames.map((f,i)=> {
      const prior=frames[(i+7)%8];let colors=0,silhouette=0;
      for(let p=0;p<4096;p++){if(f.indices[p]!==prior.indices[p])colors++;if(Boolean(f.indices[p])!==Boolean(prior.indices[p]))silhouette++;}
      return {index:i,phase:labels.run[i],nativeBounds:f.nativeBounds,faceCentroid:f.faceCentroid,soleRange:f.soleRange,changedPixels:colors,changedSilhouette:silhouette,sha256:sha256(f.tiles)};
    });
    meta.distinctRunFrames=new Set(meta.gait.map(f=>f.sha256)).size;
    if(meta.distinctRunFrames!==8)throw new Error(`${name}: ${meta.distinctRunFrames}/8 unique run poses; source needs genuine distinct animation.`);
    const xs=frames.map(f=>f.faceCentroid?.[0]).filter(Number.isFinite),ys=frames.map(f=>f.faceCentroid?.[1]).filter(Number.isFinite);
    meta.faceMotionSpan=[Math.max(...xs)-Math.min(...xs),Math.max(...ys)-Math.min(...ys)];
    if(opts.preservedFramesManifest) {
      const expected=JSON.parse(fs.readFileSync(path.join(root,opts.preservedFramesManifest),'utf8'));
      for(const [index,hash] of Object.entries(expected))if(sha256(frames[Number(index)].tiles)!==hash)throw new Error(`${name} run${index}: approved untouched pose changed`);
      meta.preservedFramesManifest=opts.preservedFramesManifest;
    }
  }
  console.log(`${name} ${family}: 8 poses, scale ${opts.scale.toFixed(6)}, palette frozen${family==='run'?`; face span ${JSON.stringify(meta.faceMotionSpan)}`:''}`);
}
if(inspectOnly)process.exit(0);
assertPreserved(root,frozen);
if(previewOnly) {
  fs.writeFileSync(path.join(previews,'revised_asset_manifest.preview.json'),JSON.stringify(manifest,null,2)+'\n');
  console.log('Preview only: production atlases and C arrays remain unchanged.');process.exit(0);
}
if(Object.keys(output.run).length!==3||Object.keys(output.attack).length!==4)throw new Error('All seven revised families must be complete before emitting any production arrays.');
for(const family of ['run','attack'])for(let c=0;c<(family==='run'?3:4);c++) {
  const input=sources[family==='run'?c:3+c];
  if(sha256(fs.readFileSync(input))!==manifest.characters[names[c]].sources[family].sourceSHA256)throw new Error(`Artwork changed during conversion: ${input}. Run compiler again after sources are stable.`);
  for(const override of manifest.characters[names[c]].sources[family].frameOverrides||[])if(sha256(fs.readFileSync(path.join(root,override.source)))!==override.sourceSHA256)throw new Error(`Override artwork changed during conversion: ${override.source}`);
}
/* Stage the full revision after all source conversion/validation has succeeded. */
const stage=path.join(previews,'production-stage');fs.mkdirSync(stage,{recursive:true});
const staged=[];
for(const family of ['run','attack']) {
  const order=family==='run'?runNames:names;
  for(const name of order) {
    const stem=`${name}_${family}_v3`,frames=output[family][name];
    await writeAtlas(frames,path.join(stage,`${stem}.png`));
    fs.writeFileSync(path.join(stage,`${stem}.4bpp`),Buffer.concat(frames.map(f=>f.tiles)));staged.push(`${stem}.png`,`${stem}.4bpp`);
  }
  const inc=`revised_${family}_tiles.inc`;
  fs.writeFileSync(path.join(stage,inc),cInclude(order,order.map(n=>output[family][n]),labels[family]));staged.push(inc);
}
fs.writeFileSync(path.join(stage,'revised_asset_manifest.json'),JSON.stringify(manifest,null,2)+'\n');staged.push('revised_asset_manifest.json');
for(const [from,to] of sourceCopies)fs.copyFileSync(from,to);
for(const file of staged)fs.copyFileSync(path.join(stage,file),path.join(dst,file));
const cpath=path.join(root,'src/data/sa2/oc_sprite_data.c');let code=fs.readFileSync(cpath,'utf8');
if(!code.includes('gOcRevisedRunTiles'))code+='\nconst u8 ALIGNED(4) gOcRevisedRunTiles[OC_REVISED_RUN_CHARACTER_COUNT][OC_REVISED_RUN_FRAME_COUNT][OC_FRAME_TILE_BYTES] = {\n#include "../../../graphics/sa2/ocs/revised_run_tiles.inc"\n};\n';
if(!code.includes('gOcRevisedAttackTiles'))code+='\nconst u8 ALIGNED(4) gOcRevisedAttackTiles[OC_REVISED_ATTACK_CHARACTER_COUNT][OC_REVISED_ATTACK_FRAME_COUNT][OC_FRAME_TILE_BYTES] = {\n#include "../../../graphics/sa2/ocs/revised_attack_tiles.inc"\n};\n';
fs.writeFileSync(cpath,code);assertPreserved(root,frozen);
console.log('OCS3 arrays complete; all 38 frozen OCS2 assets retain their SHA256.');
