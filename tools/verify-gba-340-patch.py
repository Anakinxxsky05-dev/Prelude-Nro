from pathlib import Path
import hashlib,struct
root=Path(__file__).resolve().parents[1]
p=root/'romfs/sd/atmosphere/exefs_patches/nsogbacertbypass/C4744859A0EA4C05A67629274CD930ABC1B97DA2000000000000000000000000.ips'
raw=p.read_bytes()
assert len(raw)==39 and hashlib.sha256(raw).hexdigest()=='9834d5399a4d3d64b00aa3b9d8bd5fcc73dadc49cfd151c879e2e76ef362c393'
assert raw[:5]==b'IPS32' and raw[-4:]==b'EEOF'
expected=[(0x28FD80,bytes.fromhex('F4031F2A')),(0x2901B0,bytes.fromhex('1F2003D5')),(0x299B20,bytes.fromhex('2A008052'))]
pos=5
for address,data in expected:
 offset,size=struct.unpack_from('>IH',raw,pos);pos+=6
 assert offset==address and size==4 and raw[pos:pos+size]==data
 pos+=size
assert pos==len(raw)-4
assert (p.parent/'06980CD0757C5020C6EF73E937C5C16595964612000000000000000000000000.ips').is_file()
print('PASS: exact GBA 3.4.0 IPS32 framing, hash and edits; 3.3.0 retained')
