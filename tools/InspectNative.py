import json, struct, sys
from pathlib import Path
p=Path(sys.argv[1]); data=p.read_bytes()
pe=struct.unpack_from('<I',data,0x3c)[0]
assert data[pe:pe+4]==b'PE\0\0'
machine,sections=struct.unpack_from('<HH',data,pe+4)
opt=pe+24; opt_size=struct.unpack_from('<H',data,pe+20)[0]
assert struct.unpack_from('<H',data,opt)[0]==0x20b, 'Not a 64-bit PE'
dirs=opt+112
subsystem=struct.unpack_from('<H',data,opt+68)[0]
assert subsystem==2, 'Not a Windows GUI executable'
clr_rva,clr_size=struct.unpack_from('<II',data,dirs+14*8)
assert clr_rva==0 and clr_size==0, 'A CLR/.NET header is still present'
def offset(rva):
    for i in range(sections):
        s=opt+opt_size+i*40
        vsize,va,rsize,raw=struct.unpack_from('<IIII',data,s+8)
        if va<=rva<va+max(vsize,rsize): return raw+rva-va
    raise ValueError(f'RVA {rva:x} is not in a section')
rva,size=struct.unpack_from('<II',data,dirs+8)
imports=[]
if rva:
    pos=offset(rva)
    while any(data[pos:pos+20]):
        name=offset(struct.unpack_from('<I',data,pos+12)[0])
        imports.append(data[name:data.index(0,name)].decode('ascii')); pos+=20
assert not any(any(x in n.lower() for x in ['mscoree','libstdc++','libgcc','libc++','vcruntime']) for n in imports), imports
print(json.dumps({'path':str(p),'bytes':len(data),'machine':hex(machine),'nativePE':True,'subsystem':subsystem,'clrHeader':False,'imports':imports},indent=2))
