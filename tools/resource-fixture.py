"""Local test assets derived from the tester's own JK installation. Never distribute."""
import pathlib,struct,sys,zlib
root,out=map(pathlib.Path,sys.argv[1:3])
found={}
for gob in sorted((root/'Resource').glob('*')):
    if gob.suffix.lower()!='.gob': continue
    with gob.open('rb') as f:
        h=f.read(12)
        if h[:4]!=b'GOB ': continue
        f.seek(struct.unpack_from('<I',h,8)[0]);count=struct.unpack('<I',f.read(4))[0]
        entries=[f.read(136) for _ in range(count)]
        for e in entries:
            at,size=struct.unpack_from('<II',e);name=e[8:].split(b'\0')[0].decode('ascii').lower().replace('\\','/')
            for target in ('mat/dflt.mat','3do/dflt.3do'):
                if name==target:f.seek(at);found[target]=f.read(size)
assert len(found)==2,'Missing default test resources'
files={'mat/zz_net_custom.mat':found['mat/dflt.mat'],'3do/zz_net_model.3do':found['3do/dflt.3do'],
       'cog/zz_net_script.cog':b'# GeoSith resource fixture\nsymbols\nmessage startup\nend\ncode\nstartup:\nreturn;\nend\n'}
# Exercise both folder and GOB discovery in one host-only fixture.
material=files.pop('mat/zz_net_custom.mat')
if '--png' in sys.argv:
    def chunk(kind,data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    material=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',3,1,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(bytes([0,255,0,0,0,255,0,255,1,0,255,0,255])))+chunk(b'IEND',b'')
    p=out/'FolderMod/mat/zz_net_custom.png'
else:
    p=out/'FolderMod/mat/zz_net_custom.mat'
p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(material)
data=bytearray(b'GOB '+struct.pack('<II',20,0));entries=[]
for name,b in files.items():
    entries.append(struct.pack('<II128s',len(data),len(b),name.replace('/','\\').encode()));data.extend(b)
struct.pack_into('<I',data,8,len(data));data.extend(struct.pack('<I',len(entries)));data.extend(b''.join(entries))
(out/'Models.gob').write_bytes(data)
