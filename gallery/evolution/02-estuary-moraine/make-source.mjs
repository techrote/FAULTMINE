// FAULTMINE Estuary Moraine original source; deterministic BMP authoring tool.
// No externally sourced images, fonts, RNG streams, packages or online inputs.
import { readFileSync, writeFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { createHash } from "node:crypto";
function generateBmp(){
  const W=384,H=240, p=new Uint8Array(W*H*3);
  const rust=[229,122,88], chalk=[242,223,183], tide=[102,216,193], blue=[81,143,165], dark=[8,15,20], coral=[240,169,124];
  function pixel(x,y,c,a=255){
    x=Math.round(x);y=Math.round(y);
    if(x<0||x>=W||y<0||y>=H)return;
    const i=(y*W+x)*3;
    if(a===255){p[i]=c[0];p[i+1]=c[1];p[i+2]=c[2];return;}
    for(let j=0;j<3;j++)p[i+j]=((p[i+j]*(255-a)+c[j]*a+127)/255)|0;
  }
  function stroke(x0,y0,x1,y1,c,a=255,r=0){
    x0=Math.round(x0);y0=Math.round(y0);x1=Math.round(x1);y1=Math.round(y1);
    let dx=Math.abs(x1-x0),sx=x0<x1?1:-1,dy=-Math.abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    for(let guard=0;guard<1000;guard++){
      for(let yy=-r;yy<=r;yy++)for(let xx=-r;xx<=r;xx++)if(xx*xx+yy*yy<=r*r+0.2)pixel(x0+xx,y0+yy,c,a);
      if(x0===x1&&y0===y1)break;
      const e2=2*err;
      if(e2>=dy){err+=dy;x0+=sx}
      if(e2<=dx){err+=dx;y0+=sy}
    }
  }
  function hash(x,y,k=0){let h=(Math.imul(x+273,374761393)^Math.imul(y+917,668265263)^Math.imul(k+91,2246822519))>>>0;h^=h>>>13;h=Math.imul(h,1274126177)>>>0;return (h^(h>>>16))>>>0;}
  for(let y=0;y<H;y++)for(let x=0;x<W;x++){
    const n=hash(x>>1,y>>1)%8;
    let col=[9+(n>>2)+((y/70)|0),20+(n>>1)+((y/80)|0),27+n+((y/90)|0)];
    if((x%24===0||y%24===0)&&hash(x,y)%4!==0)col=[17,36,42];
    pixel(x,y,col);
  }
  // Salt-deposition fields: stippled regions are intentionally concentrated, not all-over noise.
  for(let y=20;y<220;y++)for(let x=14;x<368;x++){
    const n=hash(x,y,3);
    const flood=(x<145&&y>105+0.28*x)||(x>216&&y>160-0.20*(x-216));
    if(flood&&(n%100<8))pixel(x,y, n%5===0?rust:tide,70+(n%90));
  }
  // Two displaced families of sediment contour lines.
  for(let k=0;k<13;k++){
    const c=[chalk,tide,rust,blue,coral][k%5], major=(k%4===1);
    let prev=null;
    for(let x=18;x<371;x++){
      const base=x<180?
        193-k*10-Math.floor(x*0.27)+Math.round(Math.sin(x/24+k*.71)*5):
        176-k*10-Math.floor((x-180)*0.18)+Math.round(Math.sin(x/19+k*.73)*6);
      const fault=183+Math.round(10*Math.sin(base/29))+Math.round((base-120)*.10);
      const broken=Math.abs(x-fault)<(16+((k*3)%10)) || ((x+7*k)%109<7 && k%3===2);
      const y=base+(x>fault? (k%2===0?13:-12):0);
      if(!broken){
        if(prev && prev.x===x-1) stroke(prev.x,prev.y,x,y,c,major?242:174,major?1:0);
        else pixel(x,y,c,major?242:174);
        prev={x,y};
      }else prev=null;
      if(x%19===0&&!broken)pixel(x,y-3,c,118);
    }
  }
  // Intermittent meridian, dark erasure core, and a hot incision on the right edge.
  for(let y=18;y<226;y++){
    const seam=183+Math.round(10*Math.sin(y/29))+Math.round((y-120)*.10);
    const w=8+Math.floor(4*(1+Math.sin(y/11)));
    for(let dx=-w;dx<w;dx++)pixel(seam+dx,y,dark,240);
    if(y%5!==0)pixel(seam+w+2,y,rust,205);
    if(y%13<4)stroke(seam-w-4,y,seam-w-2,y+2,chalk,170);
    if(y%31===0)stroke(seam+w+2,y,seam+w+18,y-11,coral,210);
  }
  // Four large, intentionally incomplete surveying traces.
  for(let t=0;t<4;t++){
    const c=[chalk,rust,tide,blue][t];
    for(let x=28;x<350;x++){
      const y=43+t*47+Math.round(9*Math.sin((x+t*18)/33))+Math.floor((x-180)*.09);
      if(x>153&&x<218)continue;
      if((x+17*t)%71<10)continue;
      if(hash(x,y,t)%100<12)continue;
      pixel(x,y,c,198);
      if(x%7===0)pixel(x,y+1,c,110);
    }
  }
  // A pair of annotation combs, drawn as geometry, not external typefaces.
  for(let k=0;k<27;k++){
    const x=24+k*5, h=2+(hash(k,18)%12);
    stroke(x,18,x,18+h,k%5===0?coral:blue,165);
  }
  for(let k=0;k<29;k++){
    const x=219+k*5, h=2+(hash(k,23)%14);
    stroke(x,224-h,x,224,k%4===0?chalk:tide,165);
  }
  // Three large transverse rake marks create rhythm across the field.
  for(const [ax,ay,bx,by] of [[28,201,122,168],[236,53,349,36],[237,204,348,166]]){
    stroke(ax,ay,bx,by,coral,100);
    for(let j=1;j<10;j++){
      const x=ax+(bx-ax)*j/11, y=ay+(by-ay)*j/11;
      stroke(x-4,y-3,x+4,y+3,chalk,130);
    }
  }
  const stride=(W*3+3)&~3;
  const b=new Uint8Array(54+stride*H);
  const write16=(o,v)=>{b[o]=v&255;b[o+1]=(v>>>8)&255};
  const write32=(o,v)=>{b[o]=v&255;b[o+1]=(v>>>8)&255;b[o+2]=(v>>>16)&255;b[o+3]=(v>>>24)&255};
  b[0]=66;b[1]=77;write32(2,b.length);write32(10,54);write32(14,40);
  write32(18,W);write32(22,H);write16(26,1);write16(28,24);write32(34,stride*H);
  for(let y=0;y<H;y++)for(let x=0;x<W;x++){
    const q=(y*W+x)*3, o=54+(H-1-y)*stride+x*3;
    b[o]=p[q+2];b[o+1]=p[q+1];b[o+2]=p[q];
  }
  return b;
}
const output=join(dirname(fileURLToPath(import.meta.url)), "source.bmp");
const bytes=Buffer.from(generateBmp());
const args=process.argv.slice(2);
if(args.length!==1 || !["--verify","--write"].includes(args[0]))throw new Error("Usage: node make-source.mjs --verify | --write");
if(args[0]==="--write"){
  if(existsSync(output))throw new Error("Source exists: --write refuses overwrite");
  writeFileSync(output,bytes);
}else{
  if(!existsSync(output)||!readFileSync(output).equals(bytes))throw new Error("Authored BMP differs from deterministic source recipe");
}
console.log("source_bmp_sha256="+createHash("sha256").update(bytes).digest("hex"));
