#!/usr/bin/env node
/* Verify artwork provenance, exact tile/PNG/C equivalence, and legacy preservation. */
import fs from 'node:fs';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {sharp,rgb555,sha256,pack,assertPreserved} from './revised_codec.mjs';
const root=path.resolve(path.dirname(new URL(import.meta.url).pathname),'../..');
const dst=path.join(root,'graphics/sa2/ocs/guests'),generated=path.join(dst,'generated');
const previews=process.env.OC_GUEST_PREVIEW_DIR||path.join(root,'../guest-verification');
const preview=process.argv.includes('--preview'),names=['sans','spiderman'];
const file=preview?path.join(previews,'guest_asset_manifest.preview.json'):path.join(generated,'guest_asset_manifest.json');
const manifestBytes=fs.readFileSync(file),manifest=JSON.parse(manifestBytes);
const frozenBytes=fs.readFileSync(path.join(root,manifest.preservedAssets));
if(sha256(frozenBytes)!==manifest.preservedAssetsSHA256)throw new Error('Frozen legacy manifest changed');
assertPreserved(root,JSON.parse(frozenBytes));
if(sha256(fs.readFileSync(path.join(dst,'alignment.json')))!==manifest.configSHA256)throw new Error('Alignment/palette config changed after conversion');
if(!preview) {
  const approval=JSON.parse(fs.readFileSync(path.join(generated,'approval.json'),'utf8'));
  if(approval.manifestSHA256!==sha256(manifestBytes))throw new Error('Production artwork lacks approval for this exact manifest');
}
const dimensions={base:{cols:8,count:32},motion:{cols:6,count:24},rear:{cols:4,count:8},run:{cols:4,count:8}};
function indicesFor(raw,w,cols,index,palette,size=64,tileEdgeAllowed=false) {
  const indices=new Uint8Array(size*size),lookup=new Map(palette.slice(1).map((v,i)=>[rgb555(v).join(','),i+1]));let occupied=0;
  for(let y=0;y<size;y++)for(let x=0;x<size;x++) {
    const p=((Math.floor(index/cols)*size+y)*w+(index%cols)*size+x)*4,alpha=raw[p+3];
    if(alpha!==0&&alpha!==255)throw new Error(`Frame ${index}: nonbinary native alpha`);
    if(!alpha)continue;
    if(!tileEdgeAllowed&&(x===0||y===0||x===size-1||y===size-1))throw new Error(`Frame ${index}: occupied native border`);
    const rgb=[raw[p],raw[p+1],raw[p+2]].join(','),color=lookup.get(rgb);
    if(!color)throw new Error(`Frame ${index}: color not present in fixed identity palette (${rgb})`);
    indices[y*size+x]=color;occupied++;
  }
  if(occupied<(size===8?3:100))throw new Error(`Frame ${index}: visible actor/glyph lost (${occupied} pixels)`);
  return indices;
}
function decodeTile(tiles,palette,size=64) {
  const raw=Buffer.alloc(size*size*4);let off=0;
  for(let ty=0;ty<size/8;ty++)for(let tx=0;tx<size/8;tx++)for(let y=0;y<8;y++)for(let x=0;x<8;x+=2) {
    const b=tiles[off++];for(let dx=0;dx<2;dx++){const c=dx?b>>4:b&15,p=((ty*8+y)*size+tx*8+x+dx)*4;if(!c)continue;const rgb=rgb555(palette[c]);for(let k=0;k<3;k++)raw[p+k]=rgb[k];raw[p+3]=255;}
  }
  return raw;
}
function parseInclude(file){return Buffer.from([...fs.readFileSync(file,'utf8').matchAll(/0x([0-9a-f]{2})\b/gi)].map(m=>parseInt(m[1],16)));}
const exported={},palettes=[],glyphs=[];
for(const name of names) {
  const meta=manifest.characters[name];if(!meta){if(preview)continue;throw new Error(`${name}: missing compiled identity`);}
  const palette=meta.palette.map(v=>parseInt(v.slice(2),16));
  if(palette.length!==16||palette[0]!==0||new Set(palette.slice(1)).size!==15)throw new Error(`${name}: palette is not fifteen fixed opaque entries plus transparency`);
  const palBytes=Buffer.alloc(32);palette.forEach((v,i)=>palBytes.writeUInt16LE(v,i*2));palettes.push(palBytes);
  if(!preview&&!fs.readFileSync(path.join(generated,`${name}.gbapal`)).equals(palBytes))throw new Error(`${name}: binary palette drift`);
  for(const [family,s] of Object.entries(meta.sources)) {
    if(sha256(fs.readFileSync(path.join(root,s.source)))!==s.sourceSHA256)throw new Error(`${name} ${family}: source does not match compiled artwork`);
    const bodyFrames=meta.frames[family];if(bodyFrames.length!==dimensions[family].count)throw new Error(`${name} ${family}: wrong source frame count`);
    if(bodyFrames.some(f=>f.sourceTouchesEdge))throw new Error(`${name} ${family}: source artwork touches image edge`);
    if(new Set(bodyFrames.map(()=>s.scale)).size!==1||!(s.scale>0))throw new Error(`${name} ${family}: invalid constant source scale`);
    if(preview) {
      const spec=dimensions[family],{data,info}=await sharp(path.join(previews,`${name}-${family}-native-4x.png`)).resize(spec.cols*64,Math.ceil(spec.count/spec.cols)*64,{kernel:'nearest'}).ensureAlpha().raw().toBuffer({resolveWithObject:true});
      for(let i=0;i<spec.count;i++){const tile=pack(indicesFor(data,info.width,spec.cols,i,palette));if(sha256(tile)!==bodyFrames[i].sha256)throw new Error(`${name} ${family}${i}: preview does not match source conversion hash`);}
      console.log(`${name} ${family}: ${spec.count} exact preview hashes, palette and transparent borders verified`);
    }
  }
  if(meta.sources.motion) {
    if(meta.distinctRunFrames!==8||new Set(meta.frames[meta.runSourceFamily??'motion'].slice(0,8).map(f=>f.sha256)).size!==8)throw new Error(`${name}: run uses repeated poses`);
    for(const kind of ['run','attack']) {
      const gifs=[`${name}-${kind}-review.gif`,...(kind==='run'?[`${name}-${kind}-30fps.gif`]:[])];
      for(const gif of gifs) {
        const p=path.join(previews,gif);if(!fs.existsSync(p))continue;
        const result=spawnSync('identify',['-format','%T\n',p],{encoding:'utf8'});if(result.status!==0)throw new Error(`Cannot inspect GIF: ${result.stderr}`);
        const delays=result.stdout.trim().split('\n').map(Number),expected=gif.endsWith('30fps.gif')?Array.from({length:24},(_,i)=>[3,3,4][i%3]):Array.from({length:8},()=>10);
        if(JSON.stringify(delays)!==JSON.stringify(expected))throw new Error(`${gif}: wrong pose timing`);
        console.log(`${gif}: ${delays.length} poses, total ${delays.reduce((a,b)=>a+b,0)*10}ms verified`);
      }
    }
  }
  if(meta.projectile?.source&&sha256(fs.readFileSync(path.join(root,meta.projectile.source)))!==meta.projectile.sourceSHA256)throw new Error(`${name}: projectile source changed`);
  if(preview)continue;
  if(!['base','motion','rear'].every(f=>meta.sources[f])||!meta.projectile)throw new Error(`${name}: incomplete reviewed source/artwork`);
  for(const [family,e] of Object.entries(meta.exports)) {
    const spec=manifest.frameRemaps[family],count=spec.map.length,tiles=fs.readFileSync(path.join(generated,`${name}_${family}.4bpp`));
    if(tiles.length!==count*2048)throw new Error(`${name} ${family}: wrong tile length`);
    if(JSON.stringify(e.sourceFrameIndices)!==JSON.stringify(spec.map)||e.sourceFamily!==spec.source)throw new Error(`${name} ${family}: unauthorized pose remap`);
    const expectedSources=spec.map.map((index,i)=>meta.sources.run&&((family==='frame'&&i>=3&&i<=8)||(family==='action'&&i<12)||family==='revised_run')?{family:'run',index:family==='frame'?i-3:i%8}:{family:spec.source,index});
    if(JSON.stringify(e.sourceFrames)!==JSON.stringify(expectedSources))throw new Error(`${name} ${family}: unauthorized complete-source override`);
    const {data,info}=await sharp(path.join(generated,`${name}_${family}.png`)).ensureAlpha().raw().toBuffer({resolveWithObject:true});
    if(info.width!==spec.cols*64||info.height!==Math.ceil(count/spec.cols)*64)throw new Error(`${name} ${family}: invalid atlas dimensions`);
    for(let i=0;i<count;i++) {
      const frame=tiles.subarray(i*2048,(i+1)*2048),source=e.sourceFrames[i],expected=meta.frames[source.family][source.index].sha256;
      if(sha256(frame)!==expected||e.frameSHA256[i]!==expected)throw new Error(`${name} ${family}${i}: tiles differ from reviewed source pose`);
      if(!pack(indicesFor(data,info.width,spec.cols,i,palette)).equals(frame))throw new Error(`${name} ${family}${i}: binary tile/native PNG mismatch`);
    }
    (exported[family]??=[]).push(tiles);console.log(`${name} ${family}: ${count} exact source/4bpp/PNG matches`);
  }
  const tiles=fs.readFileSync(path.join(generated,`${name}_projectile.4bpp`)),{data,info}=await sharp(path.join(generated,`${name}_projectile.png`)).ensureAlpha().raw().toBuffer({resolveWithObject:true});
  if(tiles.length!==32||info.width!==8||info.height!==8)throw new Error(`${name}: projectile dimensions invalid`);
  indicesFor(data,8,1,0,palette,8,meta.projectile.tileEdgeAllowed);
  if(sha256(tiles)!==meta.projectile.sha256||!decodeTile(tiles,palette,8).equals(data))throw new Error(`${name}: projectile 4bpp/PNG mismatch`);
  glyphs.push(tiles);console.log(`${name}: complete source effect packed into one exact 8x8 projectile`);
}
if(!preview) {
  for(const [family,tiles] of Object.entries(exported))if(!parseInclude(path.join(generated,`${family}_tiles.inc`)).equals(Buffer.concat(tiles)))throw new Error(`${family}: C include differs from ordered guest tile binaries`);
  if(!parseInclude(path.join(generated,'projectile_tiles.inc')).equals(Buffer.concat(glyphs)))throw new Error('Projectile C include differs from glyph binaries');
  const paletteValues=[...fs.readFileSync(path.join(generated,'palettes.inc'),'utf8').matchAll(/0x([0-9a-f]{4})\b/gi)].map(m=>parseInt(m[1],16)),paletteBytes=Buffer.alloc(paletteValues.length*2);paletteValues.forEach((v,i)=>paletteBytes.writeUInt16LE(v,i*2));
  if(!paletteBytes.equals(Buffer.concat(palettes)))throw new Error('Palette C include differs from binary palettes');
}
assertPreserved(root,JSON.parse(frozenBytes));
console.log(`Verified ${Object.keys(JSON.parse(frozenBytes)).length} unchanged legacy assets; ${preview?'review previews':'both approved guest exports'} match exact artwork and packed bytes.`);
