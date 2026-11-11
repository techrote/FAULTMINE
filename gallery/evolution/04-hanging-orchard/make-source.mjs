// ART-EVOL-004. Deterministic original source. No downloads, fonts, libraries or stochastic seeds.
import {readFileSync,writeFileSync,existsSync} from 'node:fs';
import {dirname,join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
function makeBmp() {
  const W=320,H=224,stride=(W*3+3)&~3;
  const pixels=new Uint8Array(W*H*3);
  const C={
    bg:[9,17,24], ruled:[25,39,48], leaf:[89,159,129],
    reed:[47,103,108], pallid:[230,220,176], copper:[204,113,79],
    dark:[14,31,38], blush:[168,72,82], shadow:[35,62,65]
  };
  function pixel(x,y,c) {
    x=Math.round(x); y=Math.round(y);
    if(x<0||y<0||x>=W||y>=H) return;
    const p=(y*W+x)*3;pixels[p]=c[0];pixels[p+1]=c[1];pixels[p+2]=c[2];
  }
  function rect(x0,y0,x1,y1,c) {
    for(let y=Math.max(0,y0);y<Math.min(H,y1);y++)for(let x=Math.max(0,x0);x<Math.min(W,x1);x++)pixel(x,y,c);
  }
  function line(x0,y0,x1,y1,c) {
    x0=Math.round(x0); y0=Math.round(y0);x1=Math.round(x1);y1=Math.round(y1);
    let dx=Math.abs(x1-x0),dy=-Math.abs(y1-y0),sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx+dy;
    for(let n=0;n<Math.max(W,H)*3;n++){
      pixel(x0,y0,c); if(x0===x1&&y0===y1)break;
      let e=2*err;if(e>=dy){err+=dy;x0+=sx}if(e<=dx){err+=dx;y0+=sy}
    }
  }
  function ellipse(cx,cy,rx,ry,c) {
    for(let y=Math.floor(cy-ry);y<=Math.ceil(cy+ry);y++)
      for(let x=Math.floor(cx-rx);x<=Math.ceil(cx+rx);x++){
        const dx=x-cx,dy=y-cy;
        if(dx*dx*ry*ry+dy*dy*rx*rx<=rx*rx*ry*ry)pixel(x,y,c);
      }
  }
  for(let y=0;y<H;y++)for(let x=0;x<W;x++){
    const n=(x*29+y*17+(x*y)%19)%107; let c=C.bg;
    if(n<7)c=[10,18,25];pixel(x,y,c);
  }
  // Sparse engraved-paper coordinates and intentional unoccupied lower register.
  for(let x=14;x<307;x+=17){
    for(let y=11;y<200;y+=11)if((x*5+y*3)%7<3)pixel(x,y,C.ruled);
  }
  line(8,13,312,13,C.ruled);line(8,210,312,210,C.ruled);
  for(let x=15;x<306;x+=5)if((x/5)%3!==0){pixel(x,21,C.ruled);pixel(x,204,C.ruled);}
  // Continuous overhead living arch; asymmetrical repairs and dark structural seams.
  const canopy=y=>24+Math.floor(((y-156)*(y-156))/1120);
  for(let x=12;x<=306;x++){
    const y=canopy(x);
    pixel(x,y,C.leaf);
    pixel(x,y+1,C.reed);
    if(x>26&&x<292&&x%11<5)pixel(x,y-2,C.pallid);
  }
  line(11,61,34,42,C.copper);line(286,41,310,63,C.copper);
  line(49,48,67,29,C.reed);line(245,30,272,47,C.reed);
  // Seven pendant botanical specimens; the central void stays almost untouched.
  const pods=[
    [34,114,14,20],[75,151,20,28],[117,105,13,23],
    [157,173,19,24],[209,116,18,30],[248,158,14,20],[292,107,15,25]
  ];
  for(let i=0;i<pods.length;i++){
    let [x,y,rx,ry]=pods[i], stem=canopy(x);
    line(x,stem+2,x-(i%3-1)*5,y-ry-3,C.reed);
    line(x+1,stem+5,x+2,y-ry-2,C.ruled);
    ellipse(x,y,rx+2,ry+2,C.ruled);
    ellipse(x,y,rx,ry,i%3===1?C.copper:C.leaf);
    ellipse(x+1,y+1,rx-4,ry-4,C.dark);
    ellipse(x-2,y-4,Math.max(3,rx-8),Math.max(7,ry-12),i%3===0?C.blush:C.shadow);
    line(x-rx+3,y,x+rx-3,y,C.pallid);
    for(let s=-2;s<=2;s++){
      const yy=y+s*5; let reach=Math.floor((rx-5)*(5-Math.abs(s))/5);
      line(x-reach,yy,x+reach,yy,s===0?C.pallid:C.reed);
    }
    for(let t=0;t<7;t++){
      let xx=x-rx+4+(t*6)%(rx*2-7), yy=y-ry+8+((t*11+i*7)%(ry*2-15));
      rect(xx,yy,xx+2,yy+2,t%3===0?C.pallid:C.copper);
    }
    line(x-2,y+ry+2,x-2,y+ry+10,C.leaf);
    ellipse(x-8,y+ry+7,6,3,C.leaf);
    ellipse(x+5,y+ry+11,5,2,C.reed);
    // Localized interruption across an otherwise disciplined capsule.
    if(i===1||i===4){rect(x+4,y-3,x+rx+3,y+4,C.bg);line(x+6,y-5,x+rx+10,y-10,C.copper);}
  }
  // Leaf-lace rather than uniform raster noise.
  for(let i=0;i<15;i++){
    const x=19+i*19, y=45+(i*13)%32;
    if(i%4===2)continue;
    line(x,y,x+8,y-8,C.reed);
    ellipse(x+2,y-6,8,3,i%3===0?C.pallid:C.leaf);
  }
  // A handful of detached seeds suggests gravity without making a waterfall.
  for(let i=0;i<34;i++){
    const x=24+(i*83)%275,y=174+(i*23)%26;
    if(x>124&&x<201&&i%3!==0)continue;
    rect(x,y,x+1+(i%3===0?1:0),y+2,i%5===0?C.copper:C.reed);
  }
  // Letterpress-like indexing bars, with no borrowed typography.
  for(let k=0;k<5;k++){
    rect(12+k*3,191-(k%2)*4,13+k*3,198,C.copper);
    rect(288+k*4,191-(k%3)*4,290+k*4,198,C.pallid);
  }
  const bmp=new Uint8Array(54+stride*H),v=new DataView(bmp.buffer);
  bmp[0]=66;bmp[1]=77;v.setUint32(2,bmp.length,true);v.setUint32(10,54,true);
  v.setUint32(14,40,true);v.setInt32(18,W,true);v.setInt32(22,H,true);
  v.setUint16(26,1,true);v.setUint16(28,24,true);v.setUint32(34,stride*H,true);
  v.setInt32(38,2835,true);v.setInt32(42,2835,true);
  for(let y=0;y<H;y++)for(let x=0;x<W;x++){
    let src=(y*W+x)*3,dst=54+(H-1-y)*stride+x*3;
    bmp[dst]=pixels[src+2];bmp[dst+1]=pixels[src+1];bmp[dst+2]=pixels[src];
  }
  return bmp;
}
const filename=join(dirname(fileURLToPath(import.meta.url)),'source.bmp');
const bytes=Buffer.from(makeBmp());
const digest=createHash('sha256').update(bytes).digest('hex');
if(process.argv.includes('--verify')) {
  if(!existsSync(filename)||!readFileSync(filename).equals(bytes))throw new Error('source.bmp is not exact generator output');
  console.log('PASS: authored RGB24 source identity sha256='+digest);
} else if(process.argv.includes('--write')) {
  if(existsSync(filename))throw new Error('Refusing to overwrite source.bmp; use --verify');
  writeFileSync(filename,bytes);
  console.log('Created new source.bmp sha256='+digest);
} else {
  console.log('Use --verify to check committed bytes, or --write only if source.bmp is absent');
}
