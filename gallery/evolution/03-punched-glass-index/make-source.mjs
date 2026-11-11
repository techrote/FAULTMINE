// Deterministic authored 384x240 source, no external imagery or dependencies.
// Source is an original diagonal procession of 13 pseudo-glyph stamp tablets.
import {readFileSync,writeFileSync,existsSync} from 'node:fs';
import {dirname,join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
function generateBMP() {
 const W=384,H=240, ink=new Uint8Array(W*H*3);
 const INK=[9,16,25], GRID=[21,34,43], EMBER=[185,96,63], GOLD=[235,172,98], CYAN=[83,189,193], PALE=[213,221,197], BLUE=[49,88,107];
 function pixel(x,y,c){ if(x>=0&&x<W&&y>=0&&y<H){let p=(y*W+x)*3;ink[p]=c[0];ink[p+1]=c[1];ink[p+2]=c[2];}}
 function rect(x0,y0,x1,y1,c){x0=Math.max(0,x0);x1=Math.min(W,x1);y0=Math.max(0,y0);y1=Math.min(H,y1);for(let y=y0;y<y1;y++)for(let x=x0;x<x1;x++)pixel(x,y,c);}
 function line(x0,y0,x1,y1,c){let dx=Math.abs(x1-x0),dy=-Math.abs(y1-y0),sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx+dy;while(true){pixel(x0,y0,c);if(x0===x1&&y0===y1)break;let e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}}
 for(let y=0;y<H;y++)for(let x=0;x<W;x++){let grain=(x*29+y*11+(x*y)%17)%97,level=grain<8?1:0;pixel(x,y,[INK[0]+level,INK[1]+level,INK[2]+level]);}
 for(let y=16;y<H-14;y+=28)line(7,y,376,y,GRID);
 for(let x=12;x<W;x+=32)for(let y=5;y<H;y+=7)if((x+3*y)%13<9)pixel(x,y,GRID);
 [BLUE,CYAN,GOLD,EMBER,BLUE].forEach((c,j)=>{let shift=j*9-18;line(-28,204+shift,412,24+shift,c);if(j%2===0)line(-26,207+shift,410,27+shift,GRID);});
 for(let i=0;i<15;i++){if(i===5||i===10)continue;let x=20+i*23,y=184-i*10+(i%3-1)*7,w=19+(i%4)*2,h=30+(i%3)*3;let rim=(i%4===0||i%4===1)?CYAN:GOLD;if(i===3||i===9||i===13)rim=EMBER;
 rect(x-2,y-3,x+w+2,y+h+2,GRID);rect(x,y,x+w,y+h,rim);rect(x+2,y+2,x+w-2,y+h-2,[12,22,30]);
 for(let gy=0;gy<5;gy++)for(let gx=0;gx<4;gx++){let on=((i*19+gy*7+gx*11+(i+gx*gy)*3)%13)<6;if((gx===0&&gy===0)||(gx===3&&gy===4))on=true;if(on)rect(x+4+gx*3,y+5+gy*4,x+6+gx*3,y+7+gy*4,(gx+gy+i)%4===0?PALE:rim);}
 line(x+3,y+h-5,x+w-3,y+h-5,rim);
 if(i%3===0){line(x+Math.floor(w/2),y+h+2,x+Math.floor(w/2),y+h+12,BLUE);rect(x+Math.floor(w/2)-2,y+h+12,x+Math.floor(w/2)+3,y+h+14,CYAN);}
 }
 for(let row=0;row<4;row++){let y=27+row*57,x0=row%2===0?28:46;for(let col=0;col<14;col++){let x=x0+col*23;if((row+col)%7===0||(row+col)%7===3)continue;let length=3+((col*7+row*5)%11),c=row%2?BLUE:GRID;rect(x,y,x+length,y+2,c);if(col%3===0)rect(x,y+4,x+2,y+8,row===0?CYAN:EMBER);}}
 for(let s=0;s<12;s++){let xx=301+s*6,yy=15+(s%4)*3;rect(xx,yy,xx+3,yy+2,s%5===0?PALE:EMBER);}
 for(let s=0;s<12;s++){let xx=18+s*6,yy=215+(s%3)*3;rect(xx,yy,xx+3,yy+2,s%4===0?CYAN:GOLD);}
 rect(130,146,143,170,INK);rect(258,74,270,100,INK);
 for(const [a,b] of [[129,145],[257,73]]){line(a,b,a+14,b,GOLD);line(a,b,a,b+26,CYAN);}
 let stride=(W*3+3)&~3, bmp=new Uint8Array(54+stride*H),v=new DataView(bmp.buffer);
 bmp[0]=66;bmp[1]=77;v.setUint32(2,bmp.length,true);v.setUint32(10,54,true);v.setUint32(14,40,true);v.setInt32(18,W,true);v.setInt32(22,H,true);v.setUint16(26,1,true);v.setUint16(28,24,true);v.setUint32(34,stride*H,true);v.setInt32(38,2835,true);v.setInt32(42,2835,true);
 for(let y=H-1;y>=0;y--)for(let x=0;x<W;x++){let src=(y*W+x)*3,dst=54+((H-1-y)*stride)+x*3;bmp[dst]=ink[src+2];bmp[dst+1]=ink[src+1];bmp[dst+2]=ink[src];}
 return bmp;
}
const path=join(dirname(fileURLToPath(import.meta.url)),'source.bmp');
const result=Buffer.from(generateBMP());
const hash=createHash('sha256').update(result).digest('hex');
if(process.argv.includes('--verify')){
  if(!existsSync(path)||!readFileSync(path).equals(result))throw new Error('Committed source.bmp differs from generator');
  console.log('PASS exact authored source sha256='+hash);
}else{
  if(existsSync(path))throw new Error('Refusing to overwrite committed source.bmp; use --verify');
  writeFileSync(path,result);console.log('Wrote source.bmp sha256='+hash);
}
