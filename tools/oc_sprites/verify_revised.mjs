#!/usr/bin/env node
/* Verify isolated OCS3 exports and prove OCS2 bytes were preserved. */
import fs from 'node:fs';
import path from 'node:path';
import {sharp,rgb555,sha256,assertPreserved} from './revised_codec.mjs';
const root=path.resolve(path.dirname(new URL(import.meta.url).pathname),'../..'),dir=path.join(root,'graphics/sa2/ocs');
assertPreserved(root,JSON.parse(fs.readFileSync(path.join(dir,'ocs2_preserved_sha256.json'),'utf8')));
const manifest=JSON.parse(fs.readFileSync(path.join(dir,'revised_asset_manifest.json'),'utf8'));
const all={run:[],attack:[]};
for(const name of ['jude','kiro','yuliana','kura']) {
  const meta=manifest.characters[name],pal=fs.readFileSync(path.join(dir,`${name}.gbapal`));
  if(sha256(pal)!==meta.paletteSHA256)throw new Error(`${name}: palette drift`);
  for(const family of name==='kura'?['attack']:['run','attack']) {
    const source=meta.sources[family];
    if(sha256(fs.readFileSync(path.join(root,source.source)))!==source.sourceSHA256)throw new Error(`${name} ${family}: source no longer matches compiled artwork`);
    for(const override of source.frameOverrides||[])if(sha256(fs.readFileSync(path.join(root,override.source)))!==override.sourceSHA256)throw new Error(`${name} ${family}: override source no longer matches compiled artwork`);
    const tiles=fs.readFileSync(path.join(dir,`${name}_${family}_v3.4bpp`));all[family].push(tiles);
    const {data,info}=await sharp(path.join(dir,`${name}_${family}_v3.png`)).raw().toBuffer({resolveWithObject:true});
    if(tiles.length!==8*2048||info.width!==256||info.height!==128||info.channels!==4)throw new Error(`${name} ${family}: invalid dimensions`);
    let off=0;const hashes=[];
    for(let f=0;f<8;f++) {
      let occupied=0,minY=64,maxY=-1;
      for(let ty=0;ty<8;ty++)for(let tx=0;tx<8;tx++)for(let y=0;y<8;y++)for(let x=0;x<8;x+=2) {
        const byte=tiles[off++];
        for(let dx=0;dx<2;dx++) {
          const index=dx?byte>>4:byte&15,px=tx*8+x+dx,py=ty*8+y;
          const p=((Math.floor(f/4)*64+py)*info.width+(f%4)*64+px)*4,alpha=data[p+3];
          if(alpha!==0&&alpha!==255)throw new Error(`${name} ${family}${f}: nonbinary alpha`);
          if((index!==0)!==(alpha===255))throw new Error(`${name} ${family}${f}: opacity mismatch`);
          if(!index)continue;
          if(px===0||px===63||py===0||py===63)throw new Error(`${name} ${family}${f}: clipped tile edge`);
          const rgb=rgb555(pal.readUInt16LE(index*2));
          for(let k=0;k<3;k++)if(data[p+k]!==rgb[k])throw new Error(`${name} ${family}${f}: pixel mismatch`);
          occupied++;minY=Math.min(minY,py);maxY=Math.max(maxY,py);
        }
      }
      if(occupied<150)throw new Error(`${name} ${family}${f}: actor lost`);
      const frame=meta.frames[family][f],hash=sha256(tiles.subarray(f*2048,(f+1)*2048));hashes.push(hash);
      if(hash!==frame.sha256)throw new Error(`${name} ${family}${f}: manifest mismatch`);
      if(family==='run'&&maxY>48)throw new Error(`${name} run${f}: sole below ground reference (${maxY})`);
      if(family==='run'&&maxY<46)console.warn(`${name} run${f}: review airborne gait (${maxY} bottom)`);
    }
    if(family==='run'&&new Set(hashes).size!==8)throw new Error(`${name}: repeated run pose`);
    console.log(`${name} ${family}: 8 exact native/4bpp matches, binary alpha, safe borders`);
  }
  if(meta.faceMotionSpan?.[1]>3)console.warn(`${name}: review face vertical motion ${meta.faceMotionSpan[1].toFixed(2)}px`);
  if(meta.faceMotionSpan?.[0]>1.5)console.warn(`${name}: review face horizontal motion ${meta.faceMotionSpan[0].toFixed(2)}px`);
  if(meta.preservedFramesManifest) {
    const expected=JSON.parse(fs.readFileSync(path.join(root,meta.preservedFramesManifest),'utf8'));
    for(const [index,hash] of Object.entries(expected))if(meta.frames.run[Number(index)].sha256!==hash)throw new Error(`${name} run${index}: approved pose changed during frame replacement`);
    console.log(`${name}: all ${Object.keys(expected).length} untouched run poses retain approved SHA256`);
  }
}
for(const family of ['run','attack']) {
  const inc=fs.readFileSync(path.join(dir,`revised_${family}_tiles.inc`),'utf8');
  const parsed=Buffer.from([...inc.matchAll(/0x([0-9a-f]{2})/g)].map(m=>parseInt(m[1],16)));
  if(!parsed.equals(Buffer.concat(all[family])))throw new Error(`${family}: C include differs from tile binaries`);
  console.log(`${family}: generated C include exactly matches ${parsed.length} bytes`);
}
console.log('All 38 OCS2 asset SHA256 values preserved; seven revised families, 56 new poses verified.');
