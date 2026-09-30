#!/usr/bin/env node
/* Converts rear-view artwork to native special-stage tiles using the main palette. */
import fs from 'node:fs';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {createRequire} from 'node:module';
const require=createRequire(import.meta.url);
const sharp=require(require.resolve('sharp',{paths:[process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES||'',process.cwd()]}));
const root=path.resolve(path.dirname(new URL(import.meta.url).pathname),'../..');
const dst=path.join(root,'graphics/sa2/ocs');
const previews=process.env.OC_PREVIEW_DIR||path.join(root,'../oc-verification');
const names=['elizabeth','jude','kiro','yuliana','kura'];
const labels=['rear_run_a','rear_run_b','rear_run_c','rear_run_d','rear_turn_left','rear_turn_right','rear_jump','rear_hurt'];
const args=process.argv.slice(2),previewOnly=args.includes('--preview'),inputs=args.filter(arg=>arg!=='--preview');
const opts=inputs.length?inputs:names.map(name=>path.join(dst,'source',`${name}_rear.png`));
if(opts.length!==names.length)throw new Error('Usage: node tools/oc_sprites/compile_special.mjs ELIZABETH.png JUDE.png KIRO.png YULIANA.png KURA.png');
/* Validate all inputs before publishing any generated arrays. */
for(const p of opts)if(!fs.existsSync(p))throw new Error(`Missing artwork: ${p}`);
fs.mkdirSync(path.join(dst,'source'),{recursive:true});fs.mkdirSync(previews,{recursive:true});
function figures(data,w,h) {
  const mask=new Uint8Array(w*h),all=[],stack=[];
  for(let p=0;p<mask.length;p++)mask[p]=data[p*4+3]>=200;
  for(let p=0;p<mask.length;p++) {
    if(!mask[p])continue;
    const c={l:w,t:h,r:0,b:0,pixels:[]};stack.push(p);mask[p]=0;
    while(stack.length) {
      const q=stack.pop(),x=q%w,y=Math.floor(q/w);c.pixels.push(q);
      c.l=Math.min(c.l,x);c.r=Math.max(c.r,x);c.t=Math.min(c.t,y);c.b=Math.max(c.b,y);
      for(let yy=y-1;yy<=y+1;yy++)for(let xx=x-1;xx<=x+1;xx++){
        if(xx<0||yy<0||xx>=w||yy>=h)continue;
        const n=yy*w+xx;if(mask[n]){mask[n]=0;stack.push(n);}
      }
    }
    c.area=c.pixels.length;c.cx=(c.l+c.r)/2;c.cy=(c.t+c.b)/2;all.push(c);
  }
  const primary=all.filter(c=>c.area>=2000).sort((a,b)=>a.cy-b.cy);
  if(primary.length!==8)throw new Error(`Expected 8 rear figures, found ${primary.length}`);
  const frames=[];
  for(let row=0;row<2;row++)frames.push(...primary.slice(row*4,row*4+4).sort((a,b)=>a.cx-b.cx));
  for(const c of all.filter(c=>c.area<2000&&c.area>=16)) {
    let best=null,dist=Infinity;
    for(const f of frames){const dx=Math.max(f.l-c.r,c.l-f.r,0),dy=Math.max(f.t-c.b,c.t-f.b,0),d=dx*dx+dy*dy;if(d<dist){best=f;dist=d;}}
    if(dist<=40*40){best.pixels.push(...c.pixels);best.l=Math.min(best.l,c.l);best.r=Math.max(best.r,c.r);best.t=Math.min(best.t,c.t);best.b=Math.max(best.b,c.b);}
  }
  return frames;
}
const rgb=v=>[(v&31)*255/31,((v>>5)&31)*255/31,((v>>10)&31)*255/31];
function distance(a,b){return (a[0]-b[0])**2*.32+(a[1]-b[1])**2*.4+(a[2]-b[2])**2*.28;}
function pack(indices) {
  const bytes=Buffer.alloc(2048);let off=0;
  for(let ty=0;ty<8;ty++)for(let tx=0;tx<8;tx++)for(let y=0;y<8;y++)for(let x=0;x<8;x+=2){const p=(ty*8+y)*64+tx*8+x;bytes[off++]=indices[p]|indices[p+1]<<4;}
  return bytes;
}
const characters=[],manifest={alphaCutoff:200,frameSize:[64,64],pivot:[32,48],order:names,frames:labels,characters:{}};
for(let char=0;char<names.length;char++) {
  const filename=path.resolve(opts[char]),{data,info}=await sharp(filename).ensureAlpha().raw().toBuffer({resolveWithObject:true});
  if(info.width!==1536||info.height!==1024)throw new Error('Rear artwork must be 1536x1024');
  const sourcePath=path.join(dst,'source',`${names[char]}_rear.png`);
  if(filename!==sourcePath)fs.copyFileSync(filename,sourcePath);
  const components=figures(data,info.width,info.height),height=char===3?38:36;
  const scale=height/(components[0].b-components[0].t+1),pal=fs.readFileSync(path.join(dst,`${names[char]}.gbapal`));
  const palette=Array.from({length:16},(_,i)=>pal.readUInt16LE(i*2)),prgb=palette.map(rgb),frames=[];
  for(let frame=0;frame<8;frame++) {
    const f=components[frame],fw=Math.round((f.r-f.l+1)*scale),fh=Math.round((f.b-f.t+1)*scale),own=new Set(f.pixels);
    const ox=32-Math.floor(fw/2),bottom=frame>=6?43:48,oy=bottom-fh+1;
    if(ox<1||oy<1||ox+fw>63||oy+fh>63)throw new Error(`${names[char]} ${labels[frame]} exceeds 64x64`);
    const rgba=Buffer.alloc(4096*4),indices=new Uint8Array(4096);
    for(let dy=0;dy<fh;dy++)for(let dx=0;dx<fw;dx++) {
      const sx=Math.min(f.r,f.l+Math.floor((dx+.5)/scale)),sy=Math.min(f.b,f.t+Math.floor((dy+.5)/scale)),sp=sy*info.width+sx;
      if(!own.has(sp))continue;
      const input=[data[sp*4],data[sp*4+1],data[sp*4+2]];let index=1,d=Infinity;
      for(let i=1;i<16;i++){const q=distance(input,prgb[i]);if(q<d){d=q;index=i;}}
      const p=(oy+dy)*64+ox+dx;indices[p]=index;
      for(let k=0;k<3;k++)rgba[p*4+k]=Math.round(prgb[index][k]);rgba[p*4+3]=255;
    }
    frames.push({rgba,tiles:pack(indices),sourceBounds:[f.l,f.t,f.r,f.b],size:[fw,fh],offset:[ox,oy]});
  }
  characters.push(frames);
  const atlas=Buffer.alloc(256*128*4);
  for(let f=0;f<8;f++)for(let y=0;y<64;y++)frames[f].rgba.copy(atlas,((Math.floor(f/4)*64+y)*256+(f%4)*64)*4,y*64*4,(y+1)*64*4);
  await sharp(atlas,{raw:{width:256,height:128,channels:4}}).png().toFile(path.join(dst,`${names[char]}_special.png`));
  await sharp(atlas,{raw:{width:256,height:128,channels:4}}).resize(1024,512,{kernel:'nearest'}).png().toFile(path.join(previews,`${names[char]}-special-native-4x.png`));
  for(let f=0;f<8;f++)await sharp(frames[f].rgba,{raw:{width:64,height:64,channels:4}}).resize(256,256,{kernel:'nearest'}).png().toFile(path.join(previews,`${names[char]}-special-${f}.png`));
  fs.writeFileSync(path.join(dst,`${names[char]}_special.4bpp`),Buffer.concat(frames.map(f=>f.tiles)));
  manifest.characters[names[char]]={source:path.relative(root,sourcePath),sourceSHA256:createHash('sha256').update(fs.readFileSync(filename)).digest('hex'),scale,referenceHeight:height,paletteSource:`graphics/sa2/ocs/${names[char]}.gbapal`,frames:frames.map((f,i)=>({index:i,name:labels[i],sourceBounds:f.sourceBounds,size:f.size,offset:f.offset}))};
  console.log(`${names[char]} rear: 8 poses, ${height}px reference, same main palette`);
}
if(previewOnly) {
  fs.writeFileSync(path.join(dst,'special_asset_manifest.preview.json'),JSON.stringify(manifest,null,2)+'\n');
  console.log('Preview mode: combined C arrays remain unchanged.');
  process.exit(0);
}
/* Only emit the combined C data after all five sets have successfully compiled. */
const fmt=v=>'0x'+v.toString(16).padStart(2,'0');let inc='/* Generated by tools/oc_sprites/compile_special.mjs. */\n';
for(let c=0;c<names.length;c++) {
  inc+=`{ /* ${names[c]} */\n`;
  for(let f=0;f<8;f++){inc+=`    { /* ${f}: ${labels[f]} */\n`;for(let p=0;p<2048;p+=32)inc+='        '+[...characters[c][f].tiles.slice(p,p+32)].map(fmt).join(',')+',\n';inc+='    },\n';}
  inc+='},\n';
}
fs.writeFileSync(path.join(dst,'special_tiles.inc'),inc);
fs.writeFileSync(path.join(dst,'special_asset_manifest.json'),JSON.stringify(manifest,null,2)+'\n');
const cpath=path.join(root,'src/data/sa2/oc_sprite_data.c');
let cfile=fs.readFileSync(cpath,'utf8');
if(!cfile.includes('gOcSpecialFrameTiles'))cfile+='\nconst u8 ALIGNED(4) gOcSpecialFrameTiles[OC_CHARACTER_COUNT][OC_SPECIAL_FRAME_COUNT][OC_FRAME_TILE_BYTES] = {\n#include "../../../graphics/sa2/ocs/special_tiles.inc"\n};\n';
fs.writeFileSync(cpath,cfile);
