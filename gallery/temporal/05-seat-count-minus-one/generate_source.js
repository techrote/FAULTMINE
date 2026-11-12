/* ART-TEMP-005: original deterministic 336x224 RGB BMP. Node 18+; no packages. */
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const W=336,H=224, pix=new Uint8Array(W*H*3);
const P={night:[8,15,27], black:[4,9,17], grid:[21,34,45], chalk:[231,226,194], light:[245,190,103], copper:[208,81,67], aqua:[80,209,197], slate:[50,82,94], darkwood:[30,42,54], haze:[39,54,58]};
function dot(x,y,c){if(x<0||y<0||x>=W||y>=H)return;let i=3*(y*W+x);pix[i]=c[0];pix[i+1]=c[1];pix[i+2]=c[2];}
function rect(x0,y0,x1,y1,c){for(let y=Math.max(0,y0);y<Math.min(H,y1);y++)for(let x=Math.max(0,x0);x<Math.min(W,x1);x++)dot(x,y,c);}
function line(x0,y0,x1,y1,c,w=1){let dx=Math.abs(x1-x0),dy=-Math.abs(y1-y0),sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx+dy;for(;;){for(let j=0;j<w;j++)for(let i=0;i<w;i++)dot(x0+i-(w>>1),y0+j-(w>>1),c);if(x0===x1&&y0===y1)break;let e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}}
function fillpoly(poly,c){let miny=Math.max(0,Math.min(...poly.map(p=>p[1]))),maxy=Math.min(H-1,Math.max(...poly.map(p=>p[1])));for(let y=miny;y<=maxy;y++)for(let x=0;x<W;x++){let inside=false;for(let i=0;i<poly.length;i++){let a=poly[i],b=poly[(i+1)%poly.length];if((a[1]>y)!==(b[1]>y) && x<a[0]+(y-a[1])*(b[0]-a[0])/(b[1]-a[1]))inside=!inside;}if(inside)dot(x,y,c);}}
function outline(poly,c,w=1){for(let i=0;i<poly.length;i++){let a=poly[i],b=poly[(i+1)%poly.length];line(a[0],a[1],b[0],b[1],c,w);}}
for(let y=0;y<H;y++)for(let x=0;x<W;x++){let v=((x*11+y*7+((x*y)>>4))%47===0);dot(x,y,v?P.grid:P.night);}
rect(0,180,W,H,P.black);
for(let y=183;y<223;y+=8)line(6,y,330,y,P.grid);
line(9,181,328,181,P.slate,2);line(9,208,328,208,P.haze);
for(let x=14;x<336;x+=23){line(x,14,x,177,P.grid);line(x,188,x,210,P.grid);}
for(let y=21;y<176;y+=19){line(8,y,328,y,P.grid);}
// Five tall light shafts with broken vertical rulings.
for(let k=0;k<5;k++){let x=40+k*64;rect(x-15,16,x+18,20,k===2?P.copper:P.slate);line(x-17,22,x-17,175,P.haze);line(x+20,22,x+20,175,P.haze);for(let j=0;j<5;j++)rect(x-15+(j%2)*3,26+j*30,x-12+(j%2)*3,34+j*30,k===2?P.copper:P.light);}
// The central seat is deliberately empty; others are drawn filled.
function chair(cx,top,s,c,empty=false){
 const x=n=>cx+Math.round(n*s), y=n=>top+Math.round(n*s);
 const back=[[x(-21),y(1)],[x(21),y(1)],[x(19),y(62)],[x(-18),y(62)]];
 if(!empty){fillpoly(back,P.darkwood);outline(back,c,2);for(let v=8;v<58;v+=8)line(x(-16),y(v),x(17),y(v),v%16?P.grid:c);}
 else {outline(back,P.copper,2);line(x(-13),y(7),x(12),y(56),P.copper);line(x(12),y(7),x(-13),y(56),P.copper);}
 const seat=[[x(-25),y(64)],[x(25),y(64)],[x(34),y(78)],[x(-34),y(78)]];
 if(!empty)fillpoly(seat,c);outline(seat,empty?P.copper:P.chalk,2);
 line(x(-24),y(81),x(-27),y(136),empty?P.copper:P.chalk,3);line(x(23),y(81),x(27),y(136),empty?P.copper:P.chalk,3);
 line(x(-26),y(134),x(-34),y(147),P.slate);line(x(27),y(134),x(34),y(147),P.slate);
 line(x(-22),y(45),x(-29),y(78),empty?P.copper:P.light,2);line(x(22),y(45),x(29),y(78),empty?P.copper:P.light,2);
 if(!empty){for(let z=0;z<4;z++){let a=z*10-14;line(x(a),y(5),x(a+8),y(57),z%2?P.slate:P.light);}}
}
chair(40,59,0.78,P.chalk);chair(103,43,0.9,P.aqua);chair(168,24,1.05,P.copper,true);chair(233,43,0.9,P.light);chair(296,59,0.78,P.chalk);
// Broken registration marks and a floor index that carries no textual glyphs.
for(let y=34;y<173;y+=9){line(145,y,148,y,P.copper);line(188,y+4,192,y+4,P.copper);}
for(let i=0;i<23;i++){
 const x=14+i*14,y=197+((i*5)%12);
 line(x,y,x+((i%4===0)?6:2),y,i%6===0?P.copper:i%3===0?P.aqua:P.slate);
}
for(const [x,y,c] of [[18,11,P.aqua],[316,12,P.light],[167,12,P.copper],[18,169,P.light],[317,170,P.aqua]]){rect(x-3,y-3,x+4,y+4,c);rect(x-1,y-1,x+2,y+2,P.black);}
for(let i=0;i<7;i++){line(149+i*6,5,149+i*6,9,i%2?P.slate:P.copper);}
function encode(){const stride=(W*3+3)&~3,body=Buffer.alloc(stride*H);for(let y=0;y<H;y++)for(let x=0;x<W;x++){let p=3*(y*W+x),o=(H-1-y)*stride+3*x;body[o]=pix[p+2];body[o+1]=pix[p+1];body[o+2]=pix[p];}const h=Buffer.alloc(54);h.write('BM',0);h.writeUInt32LE(54+body.length,2);h.writeUInt32LE(54,10);h.writeUInt32LE(40,14);h.writeInt32LE(W,18);h.writeInt32LE(H,22);h.writeUInt16LE(1,26);h.writeUInt16LE(24,28);h.writeUInt32LE(body.length,34);h.writeInt32LE(2835,38);h.writeInt32LE(2835,42);return Buffer.concat([h,body]);}
const out=encode();const here=__dirname;const target=path.join(here,'source.bmp');
if(fs.existsSync(target)&&!fs.readFileSync(target).equals(out))throw new Error('Refusing to replace differing source.bmp');
fs.writeFileSync(target,out);
const h=crypto.createHash('sha256');h.update(Buffer.from('FAULTMINE-SOURCE-RGBA8-v1'));let dims=Buffer.alloc(8);dims.writeUInt32LE(W,0);dims.writeUInt32LE(H,4);h.update(dims);for(let y=0;y<H;y++)for(let x=0;x<W;x++){let p=3*(y*W+x);h.update(Buffer.from([pix[p],pix[p+1],pix[p+2],255]));}
console.log(JSON.stringify({dimensions:[W,H],bytes:out.length,source_sha256:crypto.createHash('sha256').update(out).digest('hex'),source_identity:h.digest('hex')}));
