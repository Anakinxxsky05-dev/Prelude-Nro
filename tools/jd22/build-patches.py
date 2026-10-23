"""Rebuild reviewed IPS files with LLVM; needs no game image, account or private key."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,tempfile

def branch(source,target,link=False):
    delta=target-source
    if delta%4 or not -(1<<27)<=delta<(1<<27):
        raise ValueError('Branch target out of range')
    return struct.pack('<I',(0x94000000 if link else 0x14000000)|((delta//4)&0x3ffffff))

def build(root,folder,clang,objcopy):
    source=root/'trust'/folder
    m=json.loads((source/'manifest.json').read_text())
    for i,expected in enumerate(m['anchorHashes']):
        if hashlib.sha256((source/f'anchor-{i}.der').read_bytes()).hexdigest()!=expected:
            raise ValueError('Public anchor changed')
    with tempfile.TemporaryDirectory() as tmp:
        def assemble(name):
            obj=Path(tmp)/(name+'.o');out=Path(tmp)/(name+'.bin')
            subprocess.run([clang,'--target=aarch64-none-elf','-c',name+'.s','-o',str(obj)],cwd=source,check=True)
            subprocess.run([objcopy,'-O','binary','--only-section=.text',str(obj),str(out)],check=True)
            return bytearray(out.read_bytes())
        trust=assemble('trust')
        for offset,target in zip(m['callSlots'],m['calls'],strict=True):
            if trust[offset:offset+4]!=bytes.fromhex('1f2003d5'):
                raise ValueError('Expected reviewed call slot')
            trust[offset:offset+4]=branch(m['stubStart']+offset,target,True)
        pins=assemble('sdk-pin')
        offset=pins.index(bytes.fromhex('1f2003d5'))
        pins[offset:offset+4]=branch(m['sdkPinStart']+offset,m['sdkPinOriginal'])
    if m['stubStart']+len(trust)>m['sdkPinStart'] or m['sdkPinStart']+len(pins)>m['paddingEnd']:
        raise ValueError('Patch exceeds reviewed executable padding')
    records=[(r['offset'],bytes.fromhex(r['hex'])) for r in m['retainedOriginalRecords']]
    records += [(m['stubStart']+0x100,trust),(m['verifyHook']+0x100,branch(m['verifyHook'],m['stubStart'],True)),
                (m['sdkPinStart']+0x100,pins),(m['sdkPinHook']+0x100,branch(m['sdkPinHook'],m['sdkPinStart'],True))]
    raw=b'IPS32'+b''.join(struct.pack('>IH',offset,len(data))+data for offset,data in records)+b'EEOF'
    if hashlib.sha256(raw).hexdigest()!=m['sha256']:
        raise ValueError('Rebuilt IPS does not match reviewed SHA256')
    path=root.parents[1]/'romfs/sd/atmosphere/exefs_patches/jd22-nextendo'/(m['buildId']+'.ips')
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(raw)
    print(folder+': reproduced reviewed IPS SHA256')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang',default='clang')
    parser.add_argument('--objcopy',default='llvm-objcopy')
    args=parser.parse_args()
    root=Path(__file__).resolve().parent
    for folder in ('base-v0','update-v131072'):
        build(root,folder,args.clang,args.objcopy)
