#!/usr/bin/env node
/* Verify the PNG is exactly the decoded 4bpp used by the renderer. */
import fs from 'node:fs';
import path from 'node:path';
import {createRequire} from 'node:module';
const require=createRequire(import.meta.url);
const sharp=require(require.resolve('sharp',{paths:[process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES||'',process.cwd()]}));
const root=path.resolve(path.dirname(new URL(import.meta.url).pathname),'../..');
for(const name of ['elizabeth','jude','kiro','yuliana','kura']) {
  const dir=path.join(root,'graphics/sa2/ocs'),pal=fs.readFileSync(path.join(dir,`${name}.gbapal`));
  for(const mode of [{suffix:'',count:32,columns:8},{suffix:'_special',count:8,columns:4},{suffix:'_actions',count:24,columns:6}]) {
  const tiles=fs.readFileSync(path.join(dir,`${name}${mode.suffix}.4bpp`));
  const {data,info}=await sharp(path.join(dir,`${name}${mode.suffix}.png`)).raw().toBuffer({resolveWithObject:true});
  if(tiles.length!==mode.count*2048||pal.length!==32||info.width!==mode.columns*64||info.height!==mode.count/mode.columns*64||info.channels!==4)throw new Error(`${name}${mode.suffix}: invalid dimensions`);
  let off=0,opaque=0;
  for(let f=0;f<mode.count;f++) {
    let occupied=0;
    for(let ty=0;ty<8;ty++)for(let tx=0;tx<8;tx++)for(let y=0;y<8;y++)for(let x=0;x<8;x+=2) {
      const byte=tiles[off++];
      for(let dx=0;dx<2;dx++) {
        const index=dx?(byte>>4):(byte&15),px=tx*8+x+dx,py=ty*8+y;
        const p=((Math.floor(f/mode.columns)*64+py)*info.width+(f%mode.columns)*64+px)*4;
        const alpha=data[p+3];
        if(alpha!==0&&alpha!==255)throw new Error(`${name} ${f}: nonbinary alpha`);
        if((index!==0)!==(alpha===255))throw new Error(`${name} ${f}: opacity mismatch`);
        if(!index)continue;
        if(px===0||px===63||py===0||py===63)throw new Error(`${name} ${f}: clipped edge`);
        const v=pal.readUInt16LE(index*2),rgb=[v&31,(v>>5)&31,(v>>10)&31].map(v=>Math.round(v*255/31));
        for(let k=0;k<3;k++)if(data[p+k]!==rgb[k])throw new Error(`${name} ${f}: decoded RGB mismatch`);
        occupied++;
      }
    }
    if(occupied<150)throw new Error(`${name} ${f}: lost figure`);
    opaque+=occupied;
  }
  console.log(`${name}${mode.suffix}: all ${mode.count} tile frames match PNG, binary alpha, clear borders (${opaque} opaque pixels)`);
  }
}
