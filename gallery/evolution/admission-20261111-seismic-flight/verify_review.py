#!/usr/bin/env python3
"""Offline, read-only verification for the pinned canonical comparison HTML."""
import base64
import hashlib
from html.parser import HTMLParser
from pathlib import Path
import struct
import sys
import zlib
EXPECTED = {
 "seismic": ("19629f3ccf873f8eb39cac7f8729c58ddb6a71e27b07039720f0766f32a9b337","dbe8a3825d681031ed212a90143363b19fb40c5d"),
 "atlas": ("b0f17a3dfdd8574a04424860ae2b48a355904802dfdde5f9e53d0a5ff4580cc1","978b371034adab4f9ae2e21ce9e03e4bd2471b0c"),
 "shearwing": ("41a2e7863eecd7ef51d460dd7d7909b220b915ee60941cd2077d5df0bb2a4cf5","048d77768bc6823367703a2ffa8dd3305f385233"),
 "glass": ("b96d3031d751fda3e012856889da718ababea40774f596c7dd5a3b99da6cdbf6","f121ea53006586a07788f9114d9e3c20c7ff9346"),
 "ossuary": ("d492bd7a49c74f6ab495a7075889d96fc91336f11fbe7a1bf0aab42f423af81a","78c4c0f68f5a32c7ec911f5cd1c0c981a7a2e678")
}
def check_png(b):
 assert b.startswith(b"\x89PNG\r\n\x1a\n"), "not a PNG"
 pos=8
 seen=False
 while pos<len(b):
  assert pos+12<=len(b),"truncated chunk"
  length=struct.unpack_from(">I",b,pos)[0]
  typ=b[pos+4:pos+8]
  assert pos+12+length<=len(b),"bad chunk length"
  data=b[pos+8:pos+8+length]
  assert zlib.crc32(typ+data)&0xffffffff==struct.unpack_from(">I",b,pos+8+length)[0], "bad CRC"
  if pos==8:
   assert typ==b"IHDR" and length==13, "missing IHDR"
   assert struct.unpack_from(">II",data) in ((384,240),(384,256),(1280,720)), "wrong size"
  pos+=12+length
  if typ==b"IEND":
   assert pos==len(b), "trailing bytes"
   seen=True
   break
 assert seen,"missing IEND"
class Reader(HTMLParser):
 def __init__(self): super().__init__();self.found=set()
 def handle_starttag(self,tag,attrs):
  a=dict(attrs)
  assert tag not in ("script","iframe","object","embed","link","source"),"unsafe element"
  if tag!="img":return
  key=a["data-key"]
  assert key in EXPECTED and key not in self.found, "unrecognized/repeated image"
  assert a["src"].startswith("data:image/png;base64,"),"external image"
  b=base64.b64decode(a["src"].split(",",1)[1],validate=True)
  sha,blob=EXPECTED[key]
  assert hashlib.sha256(b).hexdigest()==sha==a["data-sha256"], "PNG SHA mismatch"
  assert hashlib.sha1(b"blob "+str(len(b)).encode()+b"\0"+b).hexdigest()==blob==a["data-gitblob"], "Git blob mismatch"
  check_png(b)
  print(f"PASS {key}: {len(b)} bytes, {sha}")
  self.found.add(key)
def main():
 content=(Path(__file__).parent/"admission-desk.html").read_text(encoding="utf-8")
 assert "default-src 'none'" in content and "img-src data:" in content
 p=Reader();p.feed(content);p.close()
 assert p.found==set(EXPECTED),"incomplete evidence"
 print("PASS all five authentic source-pinned images; offline/no executable media")
if __name__=="__main__":
 try:main()
 except (AssertionError,ValueError,KeyError) as exc:sys.exit(f"FAIL {exc}")
