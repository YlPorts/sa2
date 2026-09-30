/* Deterministic RGBA-to-RGB555/OBJ codec. This never draws or repairs artwork. */
import fs from 'node:fs';
import {createHash} from 'node:crypto';
import {createRequire} from 'node:module';
const require=createRequire(import.meta.url);
export const sharp=require(require.resolve('sharp',{paths:[process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES||'',process.cwd()]}));
export const sha256=b=>createHash('sha256').update(b).digest('hex');
export const rgb555=v=>[(v&31)*255/31,((v>>5)&31)*255/31,((v>>10)&31)*255/31].map(Math.round);
export const isSkin=(r,g,b)=>r>=140&&g>=75&&b>=50&&r>=g+16&&g>=b+6;
export function components(data,w,h) {
  const mask=new Uint8Array(w*h),all=[],stack=[];
  for(let p=0;p<mask.length;p++)mask[p]=data[p*4+3]>=200;
  for(let p=0;p<mask.length;p++) {
    if(!mask[p])continue;
    const c={l:w,t:h,r:0,b:0,pixels:[],skin:0};stack.push(p);mask[p]=0;
    while(stack.length) {
      const q=stack.pop(),x=q%w,y=Math.floor(q/w);c.pixels.push(q);
      c.l=Math.min(c.l,x);c.r=Math.max(c.r,x);c.t=Math.min(c.t,y);c.b=Math.max(c.b,y);
      if(isSkin(data[q*4],data[q*4+1],data[q*4+2]))c.skin++;
      for(let yy=y-1;yy<=y+1;yy++)for(let xx=x-1;xx<=x+1;xx++) {
        if(xx<0||yy<0||xx>=w||yy>=h)continue;
        const n=yy*w+xx;if(mask[n]){mask[n]=0;stack.push(n);}
      }
    }
    c.area=c.pixels.length;c.cx=(c.l+c.r)/2;c.cy=(c.t+c.b)/2;all.push(c);
  }
  return all;
}
export function figures(data,w,h,count) {
  const all=components(data,w,h),rows=count/4;
  /* Face/hand colors identify the actors independently of detached smoke/arcs. */
  const candidates=all.filter(c=>c.area>=1000&&c.skin>=50).sort((a,b)=>b.area-a.area);
  if(candidates.length<count)throw new Error(`Expected ${count} complete actor components; found ${candidates.length}. Review source alpha/connectivity.`);
  const primary=candidates.slice(0,count).sort((a,b)=>a.cy-b.cy),frames=[];
  for(let row=0;row<rows;row++)frames.push(...primary.slice(row*4,row*4+4).sort((a,b)=>a.cx-b.cx));
  for(const f of frames){f.bodyBounds=[f.l,f.t,f.r,f.b];f.bodyPixels=[...f.pixels];f.secondaryComponents=[];}
  const selected=new Set(primary),unassigned=[];
  for(const c of all.filter(c=>!selected.has(c)&&c.area>=4)) {
    let best=null,dist=Infinity;
    for(const f of frames) {
      const b=f.bodyBounds,dx=Math.max(b[0]-c.r,c.l-b[2],0),dy=Math.max(b[1]-c.b,c.t-b[3],0),d=dx*dx+dy*dy;
      if(d<dist){best=f;dist=d;}
    }
    if(dist<=90*90) {
      best.pixels.push(...c.pixels);best.secondaryComponents.push({bounds:[c.l,c.t,c.r,c.b],area:c.area});
      best.l=Math.min(best.l,c.l);best.r=Math.max(best.r,c.r);best.t=Math.min(best.t,c.t);best.b=Math.max(best.b,c.b);
    }else unassigned.push({bounds:[c.l,c.t,c.r,c.b],area:c.area});
  }
  return {frames,unassigned};
}
export function hipAnchor(f,data,w,name) {
  const xs=[],height=f.bodyBounds[3]-f.bodyBounds[1]+1;
  for(const p of f.bodyPixels) {
    const y=Math.floor(p/w),v=(y-f.bodyBounds[1])/height;
    if(v<(name==='kura'?.58:.57)||v>(name==='kura'?.67:.74))continue;
    const r=data[p*4],g=data[p*4+1],b=data[p*4+2];
    const garment=name==='yuliana'?(g>=r&&g>b+8&&g<190):Math.max(r,g,b)<90;
    if(garment)xs.push(p%w);
  }
  if(xs.length<30)return (f.bodyBounds[0]+f.bodyBounds[2])/2;
  xs.sort((a,b)=>a-b);return xs[Math.floor(xs.length/2)];
}
export function pack(indices) {
  const bytes=Buffer.alloc(2048);let off=0;
  for(let ty=0;ty<8;ty++)for(let tx=0;tx<8;tx++)for(let y=0;y<8;y++)for(let x=0;x<8;x+=2) {
    const p=(ty*8+y)*64+tx*8+x;bytes[off++]=indices[p]|indices[p+1]<<4;
  }
  return bytes;
}
function headSkinClusters(points) {
  const pending=new Set(points.map(([x,y])=>y*64+x)),clusters=[];
  while(pending.size) {
    const start=pending.values().next().value,stack=[start],pixels=[];pending.delete(start);
    while(stack.length) {
      const p=stack.pop(),x=p%64,y=Math.floor(p/64);pixels.push([x,y]);
      for(let yy=y-1;yy<=y+1;yy++)for(let xx=x-1;xx<=x+1;xx++) {
        if(xx<0||yy<0||xx>=64||yy>=64)continue;
        const q=yy*64+xx;if(pending.delete(q))stack.push(q);
      }
    }
    clusters.push(pixels);
  }
  return clusters.sort((a,b)=>b.length-a.length);
}
function faceSkin(r,g,b,name) {
  if(name==='kiro')return r>=150&&g>=75&&b>=25&&r>=g+40&&g>=b+20;
  if(name==='yuliana')return r>=150&&g>=75&&b>=35&&r>=g+40&&g>=b+20;
  if(name==='jude')return isSkin(r,g,b)&&b>=85;
  if(name==='kura')return isSkin(r,g,b)&&r>=g+25;
  return isSkin(r,g,b);
}
export function encodeFrame(f,data,w,h,scale,hipX,groundY,palette,name) {
  const prgb=palette.map(rgb555),own=new Set(f.pixels),fw=Math.round((f.r-f.l+1)*scale),fh=Math.round((f.b-f.t+1)*scale);
  const ox=32-Math.round((hipX-f.l)*scale),oy=49-Math.round((groundY-f.t+1)*scale);
  if(ox<1||oy<1||ox+fw>63||oy+fh>63)throw new Error(`Frame exceeds safe 64x64 bounds: size ${fw}x${fh}, offset ${ox},${oy}. Adjust body anchoring/source; do not rescale individual poses.`);
  const rgba=Buffer.alloc(4096*4),indices=new Uint8Array(4096),skin=[];
  let error=0,severe=0,occupied=0;
  for(let dy=0;dy<fh;dy++)for(let dx=0;dx<fw;dx++) {
    const sx=Math.min(f.r,f.l+Math.floor((dx+.5)/scale)),sy=Math.min(f.b,f.t+Math.floor((dy+.5)/scale)),sp=sy*w+sx;
    if(!own.has(sp))continue;
    const input=[data[sp*4],data[sp*4+1],data[sp*4+2]];let index=1,d=Infinity;
    for(let i=1;i<16;i++) {
      const q=(input[0]-prgb[i][0])**2*.32+(input[1]-prgb[i][1])**2*.4+(input[2]-prgb[i][2])**2*.28;
      if(q<d){d=q;index=i;}
    }
    const x=ox+dx,y=oy+dy,p=y*64+x;indices[p]=index;
    for(let k=0;k<3;k++)rgba[p*4+k]=prgb[index][k];rgba[p*4+3]=255;
    if(faceSkin(...input,name)&&y<oy+fh*.56)skin.push([x,y]);
    occupied++;error+=d;if(d>2000)severe++;
  }
  const xs=[],ys=[],footXs=[];
  for(let p=0;p<4096;p++)if(indices[p]){xs.push(p%64);ys.push(Math.floor(p/64));if(Math.floor(p/64)>=45)footXs.push(p%64);}
  /* Connected face skin is much larger than a detached fist; report both sizes. */
  const clusters=headSkinClusters(skin),facePixels=clusters[0]||[];
  const face=facePixels.length?facePixels.reduce((a,p)=>[a[0]+p[0]/facePixels.length,a[1]+p[1]/facePixels.length],[0,0]):null;
  const faceBounds=facePixels.length?[Math.min(...facePixels.map(p=>p[0])),Math.min(...facePixels.map(p=>p[1])),Math.max(...facePixels.map(p=>p[0])),Math.max(...facePixels.map(p=>p[1]))]:null;
  return {rgba,indices,tiles:pack(indices),sourceBounds:[f.l,f.t,f.r,f.b],bodyBounds:f.bodyBounds,secondaryComponents:f.secondaryComponents,
    size:[fw,fh],offset:[ox,oy],bodyAnchorX:hipX,groundY,sourceTouchesEdge:f.l===0||f.t===0||f.r===w-1||f.b===h-1,
    sourceBorderMargin:[f.l,f.t,w-1-f.r,h-1-f.b],
    nativeBounds:[Math.min(...xs),Math.min(...ys),Math.max(...xs),Math.max(...ys)],faceCentroid:face,faceBounds,headSkinClusterSizes:clusters.map(c=>c.length),
    solePixels:footXs.length,soleRange:footXs.length?[Math.min(...footXs),Math.max(...footXs)]:null,
    occupied,meanColorError:error/occupied,severeColorErrorFraction:severe/occupied};
}
export async function writeAtlas(frames,filename,scale=1) {
  const atlas=Buffer.alloc(256*128*4);
  for(let f=0;f<8;f++)for(let y=0;y<64;y++)frames[f].rgba.copy(atlas,((Math.floor(f/4)*64+y)*256+(f%4)*64)*4,y*64*4,(y+1)*64*4);
  let img=sharp(atlas,{raw:{width:256,height:128,channels:4}});
  if(scale!==1)img=img.resize(256*scale,128*scale,{kernel:'nearest'});
  await img.png().toFile(filename);
}
export function cInclude(names,characters,labels) {
  const fmt=v=>'0x'+v.toString(16).padStart(2,'0');let inc='/* Generated by tools/oc_sprites/compile_revised.mjs. */\n';
  for(let c=0;c<names.length;c++) {
    inc+=`{ /* ${names[c]} */\n`;
    for(let f=0;f<8;f++){inc+=`    { /* ${f}: ${labels[f]} */\n`;for(let p=0;p<2048;p+=32)inc+='        '+[...characters[c][f].tiles.slice(p,p+32)].map(fmt).join(',')+',\n';inc+='    },\n';}
    inc+='},\n';
  }
  return inc;
}
export function assertPreserved(root,manifest) {
  for(const [relative,expected] of Object.entries(manifest))if(sha256(fs.readFileSync(`${root}/${relative}`))!==expected)throw new Error(`Frozen OCS2 asset changed: ${relative}`);
}
