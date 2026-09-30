#!/usr/bin/env node
/* Encode supplementary poses. All frames retain the character's main RGB555 palette. */
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
const labels=[...Array.from({length:12},(_,i)=>`run_${i}`),...Array.from({length:4},(_,i)=>`jump_${i}`),...Array.from({length:4},(_,i)=>`attack_${i}`),...Array.from({length:4},(_,i)=>`victory_${i}`)];
const args=process.argv.slice(2),previewOnly=args.includes('--preview'),inputs=args.filter(arg=>arg!=='--preview');
const opts=inputs.length?inputs:names.map(name=>path.join(dst,'source',`${name}_action_v2.png`));
if(opts.length!==names.length)throw new Error('Usage: node tools/oc_sprites/compile_actions.mjs ELIZABETH.png JUDE.png KIRO.png YULIANA.png KURA.png');
for(const p of opts)if(!previewOnly&&!fs.existsSync(p))throw new Error(`Missing artwork: ${p}`);
fs.mkdirSync(path.join(dst,'source'),{recursive:true});fs.mkdirSync(previews,{recursive:true});
const configPath=path.join(dst,'action_alignment.json');
const config=fs.existsSync(configPath)?JSON.parse(fs.readFileSync(configPath,'utf8')):{};
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
  const candidates=all.filter(c=>c.area>=2000).sort((a,b)=>b.area-a.area);
  if(candidates.length<24)throw new Error(`Expected 24 action figures, found ${candidates.length}`);
  /* Large detached muzzle flashes remain secondary to the 24 character bodies. */
  const primary=candidates.slice(0,24).sort((a,b)=>a.cy-b.cy),frames=[];
  for(let row=0;row<4;row++)frames.push(...primary.slice(row*6,row*6+6).sort((a,b)=>a.cx-b.cx));
  for(const f of frames){f.bodyBounds=[f.l,f.t,f.r,f.b];f.bodyPixels=[...f.pixels];}
  const selected=new Set(primary);
  for(const c of all.filter(c=>!selected.has(c)&&c.area>=4)) {
    let best=null,dist=Infinity;
    for(const f of frames){const dx=Math.max(f.l-c.r,c.l-f.r,0),dy=Math.max(f.t-c.b,c.t-f.b,0),d=dx*dx+dy*dy;if(d<dist){best=f;dist=d;}}
    if(dist<=70*70){best.pixels.push(...c.pixels);best.l=Math.min(best.l,c.l);best.r=Math.max(best.r,c.r);best.t=Math.min(best.t,c.t);best.b=Math.max(best.b,c.b);}
  }
  return frames;
}
const rgb=v=>[(v&31)*255/31,((v>>5)&31)*255/31,((v>>10)&31)*255/31];
const rgba555=(r,g,b)=>Math.round(r*31/255)|Math.round(g*31/255)<<5|Math.round(b*31/255)<<10;
function distance(a,b){return (a[0]-b[0])**2*.32+(a[1]-b[1])**2*.4+(a[2]-b[2])**2*.28;}
function hipAnchor(f,data,w,char) {
  const xs=[],height=f.bodyBounds[3]-f.bodyBounds[1]+1;
  for(const p of f.bodyPixels) {
    const y=Math.floor(p/w),v=(y-f.bodyBounds[1])/height;
    if(v<(char===4?.58:.57)||v>(char===4?.67:.74))continue;
    const r=data[p*4],g=data[p*4+1],b=data[p*4+2];
    const garment=char===3?(g>=r&&g>b+8&&g<190):Math.max(r,g,b)<90;
    if(garment)xs.push(p%w);
  }
  if(xs.length<30)return f.cx;
  xs.sort((a,b)=>a-b);
  return xs[Math.floor(xs.length/2)];
}
function pack(indices) {
  const bytes=Buffer.alloc(2048);let off=0;
  for(let ty=0;ty<8;ty++)for(let tx=0;tx<8;tx++)for(let y=0;y<8;y++)for(let x=0;x<8;x+=2){const p=(ty*8+y)*64+tx*8+x;bytes[off++]=indices[p]|indices[p+1]<<4;}
  return bytes;
}
const characters=[],manifest={alphaCutoff:200,frameSize:[64,64],pivot:[32,48],order:names,frames:labels,characters:{}};
for(let char=0;char<names.length;char++) {
  if(previewOnly&&!fs.existsSync(opts[char]))continue;
  const filename=path.resolve(opts[char]),{data,info}=await sharp(filename).ensureAlpha().raw().toBuffer({resolveWithObject:true});
  if(info.width!==1536||info.height!==1024)throw new Error('Action artwork must be 1536x1024');
  const sourcePath=path.join(dst,'source',`${names[char]}_action_v2.png`);
  if(filename!==sourcePath)fs.copyFileSync(filename,sourcePath);
  const components=figures(data,info.width,info.height),height=char===3?38:36,override=config[names[char]]||{};
  const runHeights=components.slice(0,12).map(c=>c.bodyBounds[3]-c.bodyBounds[1]+1).sort((a,b)=>a-b);
  /* One scale per entire character sheet. Median gait height is the default reference. */
  const scale=override.scale||height/((runHeights[5]+runHeights[6])/2);
  const pal=fs.readFileSync(path.join(dst,`${names[char]}.gbapal`));
  const palette=Array.from({length:16},(_,i)=>pal.readUInt16LE(i*2)),prgb=palette.map(rgb),frames=[];
  let colorError=0,occupied=0,severeColorError=0;
  for(let frame=0;frame<24;frame++) {
    const f=components[frame],own=new Set(f.pixels),align=(override.frames||[])[frame]||{};
    const hipX=align.hipX??hipAnchor(f,data,info.width,char),groundY=align.groundY??f.bodyBounds[3];
    const fw=Math.round((f.r-f.l+1)*scale),fh=Math.round((f.b-f.t+1)*scale);
    const pivotY=align.pivotY??48,ox=32-Math.round((hipX-f.l)*scale),oy=pivotY+1-Math.round((groundY-f.t+1)*scale);
    if(ox<1||oy<1||ox+fw>63||oy+fh>63)throw new Error(`${names[char]} ${labels[frame]} exceeds 64x64: size${fw}x${fh} offset${ox},${oy}. Adjust action_alignment.json.`);
    const rgba=Buffer.alloc(4096*4),indices=new Uint8Array(4096);
    for(let dy=0;dy<fh;dy++)for(let dx=0;dx<fw;dx++) {
      const sx=Math.min(f.r,f.l+Math.floor((dx+.5)/scale)),sy=Math.min(f.b,f.t+Math.floor((dy+.5)/scale)),sp=sy*info.width+sx;
      if(!own.has(sp))continue;
      const input=rgb(rgba555(data[sp*4],data[sp*4+1],data[sp*4+2]));let index=1,d=Infinity;
      for(let i=1;i<16;i++){const q=distance(input,prgb[i]);if(q<d){d=q;index=i;}}
      colorError+=d;occupied++;if(d>2000)severeColorError++;
      const p=(oy+dy)*64+ox+dx;indices[p]=index;
      for(let k=0;k<3;k++)rgba[p*4+k]=Math.round(prgb[index][k]);rgba[p*4+3]=255;
    }
    frames.push({rgba,indices,tiles:pack(indices),sourceBounds:[f.l,f.t,f.r,f.b],size:[fw,fh],offset:[ox,oy],hipX,groundY,pivotY});
  }
  characters.push(frames);
  const atlas=Buffer.alloc(384*256*4);
  for(let f=0;f<24;f++)for(let y=0;y<64;y++)frames[f].rgba.copy(atlas,((Math.floor(f/6)*64+y)*384+(f%6)*64)*4,y*64*4,(y+1)*64*4);
  await sharp(atlas,{raw:{width:384,height:256,channels:4}}).png().toFile(path.join(dst,`${names[char]}_actions.png`));
  await sharp(atlas,{raw:{width:384,height:256,channels:4}}).resize(1536,1024,{kernel:'nearest'}).png().toFile(path.join(previews,`${names[char]}-actions-native-4x.png`));
  for(let f=0;f<24;f++)await sharp(frames[f].rgba,{raw:{width:64,height:64,channels:4}}).resize(256,256,{kernel:'nearest'}).png().toFile(path.join(previews,`${names[char]}-action-${String(f).padStart(2,'0')}.png`));
  fs.writeFileSync(path.join(dst,`${names[char]}_actions.4bpp`),Buffer.concat(frames.map(f=>f.tiles)));
  const runHashes=frames.slice(0,12).map(f=>createHash('sha256').update(f.tiles).digest('hex'));
  const changes=frames.slice(0,12).map((f,i)=>{const prev=frames[(i+11)%12];let colors=0,silhouette=0;for(let p=0;p<4096;p++){if(f.indices[p]!==prev.indices[p])colors++;if(Boolean(f.indices[p])!==Boolean(prev.indices[p]))silhouette++;}return {from:(i+11)%12,to:i,changedPixels:colors,changedSilhouette:silhouette};});
  manifest.characters[names[char]]={source:path.relative(root,sourcePath),sourceSHA256:createHash('sha256').update(fs.readFileSync(filename)).digest('hex'),scale,referenceHeight:height,paletteSource:`graphics/sa2/ocs/${names[char]}.gbapal`,meanColorError:colorError/occupied,severeColorErrorFraction:severeColorError/occupied,distinctRunFrames:new Set(runHashes).size,runChanges:changes,frames:frames.map((f,i)=>({index:i,name:labels[i],sourceBounds:f.sourceBounds,size:f.size,offset:f.offset,hipX:f.hipX,groundY:f.groundY,pivotY:f.pivotY}))};
  console.log(`${names[char]} actions: 24 poses, run distinct${new Set(runHashes).size}/12, ${height}px reference, fixed palette; severe color error ${(100*severeColorError/occupied).toFixed(2)}%`);
}
if(previewOnly) {
  fs.writeFileSync(path.join(dst,'action_asset_manifest.preview.json'),JSON.stringify(manifest,null,2)+'\n');
  console.log('Preview mode: combined C arrays remain unchanged.');
  process.exit(0);
}
const fmt=v=>'0x'+v.toString(16).padStart(2,'0');let inc='/* Generated by tools/oc_sprites/compile_actions.mjs. */\n';
for(let c=0;c<names.length;c++) {
  inc+=`{ /* ${names[c]} */\n`;
  for(let f=0;f<24;f++){inc+=`    { /* ${f}: ${labels[f]} */\n`;for(let p=0;p<2048;p+=32)inc+='        '+[...characters[c][f].tiles.slice(p,p+32)].map(fmt).join(',')+',\n';inc+='    },\n';}
  inc+='},\n';
}
fs.writeFileSync(path.join(dst,'action_tiles.inc'),inc);
fs.writeFileSync(path.join(dst,'action_asset_manifest.json'),JSON.stringify(manifest,null,2)+'\n');
const cpath=path.join(root,'src/data/sa2/oc_sprite_data.c');let cfile=fs.readFileSync(cpath,'utf8');
if(!cfile.includes('gOcActionFrameTiles'))cfile+='\nconst u8 ALIGNED(4) gOcActionFrameTiles[OC_CHARACTER_COUNT][OC_ACTION_FRAME_COUNT][OC_FRAME_TILE_BYTES] = {\n#include "../../../graphics/sa2/ocs/action_tiles.inc"\n};\n';
fs.writeFileSync(cpath,cfile);
