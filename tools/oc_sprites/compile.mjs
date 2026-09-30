#!/usr/bin/env node
/** Deterministic image-to-GBA asset conversion. This does not draw new artwork. */
import fs from 'node:fs';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {createRequire} from 'node:module';
const require = createRequire(import.meta.url);
const sharp = require(require.resolve('sharp', {paths: [process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES || '', process.cwd()]}));
const root = path.resolve(path.dirname(new URL(import.meta.url).pathname), '../..');
const names = ['elizabeth', 'jude', 'kiro', 'yuliana'];
const labels = ['idle','blink','breathe','run_a','run_b','run_c','run_d','run_e','run_f','crouch','launch','rise','tuck','fall','land','brake_a','brake_b','brake_c','hurt_a','hurt_b','death','balance_a','balance_b','hang','push','grind','swim','victory','wave','attack','curl_a','curl_b'];
const opts = process.argv.length>2 ? process.argv.slice(2) : names.map(name=>path.join(root,'graphics/sa2/ocs/source',`${name}_front.png`));
if (opts.length !== 4) throw new Error('Usage: node tools/oc_sprites/compile.mjs ELIZABETH.png JUDE.png KIRO.png YULIANA.png');
const dst = path.join(root, 'graphics/sa2/ocs');
fs.mkdirSync(dst, {recursive:true});
fs.mkdirSync(path.join(dst,'source'), {recursive:true});
const previewDir = process.env.OC_PREVIEW_DIR || path.join(root,'../oc-verification');
fs.mkdirSync(previewDir,{recursive:true});
const cutoff = 200;
const rgba555 = (r,g,b) => Math.round(r * 31 / 255) | Math.round(g * 31 / 255) << 5 | Math.round(b * 31 / 255) << 10;
const rgb = (v) => [(v&31)*255/31,((v>>5)&31)*255/31,((v>>10)&31)*255/31];
function components(data,w,h) {
  const mask = new Uint8Array(w*h), all=[];
  for(let p=0;p<mask.length;p++)mask[p]=data[p*4+3]>=cutoff;
  const stack=[];
  for(let p=0;p<mask.length;p++) {
    if(!mask[p])continue;
    const c={l:w,t:h,r:0,b:0,pixels:[]}; stack.push(p);mask[p]=0;
    while(stack.length) {
      const q=stack.pop(),x=q%w,y=Math.floor(q/w);c.pixels.push(q);
      c.l=Math.min(c.l,x);c.r=Math.max(c.r,x);c.t=Math.min(c.t,y);c.b=Math.max(c.b,y);
      for(let yy=y-1;yy<=y+1;yy++)for(let xx=x-1;xx<=x+1;xx++) {
        if(xx<0||yy<0||xx>=w||yy>=h)continue;
        const n=yy*w+xx;if(mask[n]){mask[n]=0;stack.push(n);}
      }
    }
    c.area=c.pixels.length;c.cx=(c.l+c.r)/2;c.cy=(c.t+c.b)/2;all.push(c);
  }
  const primary=all.filter(c=>c.area>=2000).sort((a,b)=>a.cy-b.cy);
  if(primary.length!==32)throw new Error(`Expected 32 distinct connected figures; found ${primary.length}`);
  const frames=[];
  for(let row=0;row<4;row++)frames.push(...primary.slice(row*8,row*8+8).sort((a,b)=>a.cx-b.cx));
  for(const c of all.filter(c=>c.area<2000&&c.area>=16)) {
    let best=null,dist=Infinity;
    for(const f of frames) {
      const dx=Math.max(f.l-c.r,c.l-f.r,0),dy=Math.max(f.t-c.b,c.t-f.b,0),d=dx*dx+dy*dy;
      if(d<dist){best=f;dist=d;}
    }
    if(dist<=40*40) {best.pixels.push(...c.pixels);best.l=Math.min(best.l,c.l);best.r=Math.max(best.r,c.r);best.t=Math.min(best.t,c.t);best.b=Math.max(best.b,c.b);}
  }
  return frames;
}
function distance(a,b) {
  const dr=a[0]-b[0],dg=a[1]-b[1],db=a[2]-b[2];
  return dr*dr*0.32+dg*dg*0.4+db*db*0.28;
}
function quantize(frames) {
  const hist=new Map();
  for(const f of frames)for(let p=0;p<4096;p++)if(f.rgba[p*4+3]){
    const v=rgba555(...f.rgba.slice(p*4,p*4+3));hist.set(v,(hist.get(v)||0)+1);
  }
  const colors=[...hist].map(([v,n])=>({v,n,rgb:rgb(v)}));
  const centers=[colors.reduce((a,b)=>a.rgb.reduce((n,v)=>n+v,0)<b.rgb.reduce((n,v)=>n+v,0)?a:b).rgb];
  while(centers.length<15) {
    let best=null,score=-1;
    for(const c of colors) {
      const s=Math.min(...centers.map(v=>distance(c.rgb,v)))*Math.pow(c.n,0.45);
      if(s>score){score=s;best=c;}
    }
    centers.push([...best.rgb]);
  }
  /* The darkest entry is fixed so the silhouette and facial outlines stay dark. */
  for(let pass=0;pass<24;pass++) {
    const sum=centers.map(()=>[0,0,0,0]);
    for(const c of colors){let ix=0,d=Infinity;for(let i=0;i<15;i++){const t=distance(c.rgb,centers[i]);if(t<d){d=t;ix=i;}}for(let k=0;k<3;k++)sum[ix][k]+=c.rgb[k]*c.n;sum[ix][3]+=c.n;}
    for(let i=1;i<15;i++)if(sum[i][3])centers[i]=sum[i].slice(0,3).map(v=>v/sum[i][3]);
  }
  const palette=[0,...centers.map(v=>rgba555(...v))];
  if(new Set(palette.slice(1)).size!==15)throw new Error('Palette collapsed distinct entries');
  const prgb=palette.map(rgb);
  for(const f of frames) {
    f.indices=new Uint8Array(4096);
    for(let p=0;p<4096;p++)if(f.rgba[p*4+3]){
      const color=rgb(rgba555(...f.rgba.slice(p*4,p*4+3)));let ix=1,d=Infinity;
      for(let i=1;i<16;i++){const t=distance(color,prgb[i]);if(t<d){d=t;ix=i;}}
      f.indices[p]=ix;
      for(let k=0;k<3;k++)f.rgba[p*4+k]=Math.round(prgb[ix][k]);
    }
  }
  return palette;
}
function pack(indices) {
  const bytes=Buffer.alloc(2048);let off=0;
  for(let ty=0;ty<8;ty++)for(let tx=0;tx<8;tx++)for(let y=0;y<8;y++)for(let x=0;x<8;x+=2) {
    const p=(ty*8+y)*64+tx*8+x;bytes[off++]=indices[p]|indices[p+1]<<4;
  }
  return bytes;
}
const characterFrames=[],characterPalettes=[],report={alphaCutoff:cutoff,frameSize:[64,64],pivot:[32,48],order:names,frames:labels,characters:{}};
for(let char=0;char<4;char++) {
  const filename=path.resolve(opts[char]), {data,info}=await sharp(filename).ensureAlpha().raw().toBuffer({resolveWithObject:true});
  const sourcePath=path.join(dst,'source',`${names[char]}_front.png`);
  if(filename!==sourcePath)fs.copyFileSync(filename,sourcePath);
  if(info.width!==1536||info.height!==1024)throw new Error('Main artwork must be 1536x1024');
  const figures=components(data,info.width,info.height);
  const targetHeight=char===3?38:36;
  const scale=targetHeight/(figures[0].b-figures[0].t+1);
  const frames=[];
  for(let frame=0;frame<32;frame++) {
    const f=figures[frame],fw=Math.round((f.r-f.l+1)*scale),fh=Math.round((f.b-f.t+1)*scale);
    const rgba=Buffer.alloc(64*64*4),own=new Set(f.pixels);
    let bottom=48;
    if([11,12,18,19].includes(frame))bottom=43;
    if(frame===13)bottom=46;
    if([26,30,31].includes(frame))bottom=Math.round(31+fh/2);
    const ox=32-Math.floor(fw/2),oy=bottom-fh+1;
    if(ox<1||oy<1||ox+fw>63||oy+fh>63)throw new Error(`${names[char]} ${labels[frame]} needs a larger frame (${fw}x${fh})`);
    for(let dy=0;dy<fh;dy++)for(let dx=0;dx<fw;dx++) {
      const sx=Math.min(f.r,f.l+Math.floor((dx+.5)/scale)),sy=Math.min(f.b,f.t+Math.floor((dy+.5)/scale)),sp=sy*info.width+sx;
      if(!own.has(sp))continue;
      const dp=((oy+dy)*64+ox+dx)*4;
      rgba[dp]=data[sp*4];rgba[dp+1]=data[sp*4+1];rgba[dp+2]=data[sp*4+2];rgba[dp+3]=255;
    }
    frames.push({rgba,sourceBounds:[f.l,f.t,f.r,f.b],size:[fw,fh],offset:[ox,oy]});
  }
  const palette=quantize(frames);characterPalettes.push(palette);
  const tileFrames=frames.map(f=>pack(f.indices));characterFrames.push(tileFrames);
  fs.writeFileSync(path.join(dst,`${names[char]}.4bpp`),Buffer.concat(tileFrames));
  const palBytes=Buffer.alloc(32);palette.forEach((v,i)=>palBytes.writeUInt16LE(v,i*2));fs.writeFileSync(path.join(dst,`${names[char]}.gbapal`),palBytes);
  const atlas=Buffer.alloc(512*256*4);
  for(let frame=0;frame<32;frame++)for(let y=0;y<64;y++){
    const dp=((Math.floor(frame/8)*64+y)*512+(frame%8)*64)*4;frames[frame].rgba.copy(atlas,dp,y*64*4,(y+1)*64*4);
  }
  await sharp(atlas,{raw:{width:512,height:256,channels:4}}).png().toFile(path.join(dst,`${names[char]}.png`));
  await sharp(atlas,{raw:{width:512,height:256,channels:4}}).resize(2048,1024,{kernel:'nearest'}).png().toFile(path.join(previewDir,`${names[char]}-native-4x.png`));
  for(let f=0;f<32;f++)await sharp(frames[f].rgba,{raw:{width:64,height:64,channels:4}}).resize(256,256,{kernel:'nearest'}).png().toFile(path.join(previewDir,`${names[char]}-${String(f).padStart(2,'0')}.png`));
  report.characters[names[char]]={source:path.relative(root,sourcePath),sourceSHA256:createHash('sha256').update(fs.readFileSync(filename)).digest('hex'),scale,standingHeight:targetHeight,palette:palette.map(v=>`0x${v.toString(16).padStart(4,'0')}`),frames:frames.map((f,i)=>({index:i,name:labels[i],sourceBounds:f.sourceBounds,size:f.size,offset:f.offset}))};
  console.log(`${names[char]}: ${frames.length} frames, standing ${targetHeight}px, scale ${scale.toFixed(6)}, 15 opaque colors`);
}
const fmt = v=>`0x${v.toString(16).padStart(2,'0')}`;
let inc='/* Generated by tools/oc_sprites/compile.mjs; tile order is row-major 8x8. */\n';
for(let c=0;c<4;c++) {
  inc+=`{ /* ${names[c]} */\n`;
  for(let f=0;f<32;f++) {
    inc+=`    { /* ${f}: ${labels[f]} */\n`;
    const bytes=characterFrames[c][f];for(let p=0;p<bytes.length;p+=32)inc+=`        ${[...bytes.slice(p,p+32)].map(fmt).join(',')},\n`;
    inc+='    },\n';
  }
  inc+='},\n';
}
fs.writeFileSync(path.join(dst,'frame_tiles.inc'),inc);
const cfile='#include "data/sa2/oc_sprite_data.h"\n\nconst u8 ALIGNED(4) gOcFrameTiles[OC_CHARACTER_COUNT][OC_FRAME_COUNT][OC_FRAME_TILE_BYTES] = {\n#include "../../../graphics/sa2/ocs/frame_tiles.inc"\n};\n\nconst u16 gOcPalettes[OC_CHARACTER_COUNT][16] = {\n'+characterPalettes.map((p,i)=>`    { ${p.map(v=>'0x'+v.toString(16).padStart(4,'0')).join(', ')} }, /* ${names[i]} */`).join('\n')+'\n};\n';
const special=fs.existsSync(path.join(dst,'special_tiles.inc'))?'\nconst u8 ALIGNED(4) gOcSpecialFrameTiles[OC_CHARACTER_COUNT][OC_SPECIAL_FRAME_COUNT][OC_FRAME_TILE_BYTES] = {\n#include "../../../graphics/sa2/ocs/special_tiles.inc"\n};\n':'';
fs.writeFileSync(path.join(root,'src/data/sa2/oc_sprite_data.c'),cfile+special);
fs.writeFileSync(path.join(dst,'asset_manifest.json'),JSON.stringify(report,null,2)+'\n');
console.log(`Verification PNGs: ${previewDir}`);
