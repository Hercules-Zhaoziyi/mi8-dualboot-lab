#!/usr/bin/env python3
"""Package ONLY approved UI EFI/FFS/config components, never a phone image."""
from pathlib import Path
import argparse,subprocess,hashlib,json,struct,uuid,shutil
UI='6d77b2bb-69eb-42ab-becf-4f40c89568c3';ANDROID='801a189c-e4a4-4195-85c2-64c5450a15ee';CONFIG='4b0364cf-1c5b-47aa-9073-d7b5039ce49b'
def check_pe(file):
 b=file.read_bytes();assert b[:2]==b'MZ';off=struct.unpack_from('<I',b,60)[0];assert b[off:off+4]==b'PE\0\0' and struct.unpack_from('<H',b,off+4)[0]==0xaa64
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--workspace',type=Path,required=True);p.add_argument('--out',type=Path,default=Path('dist/ui'));p.add_argument('--port',type=Path,default=Path('local/edk2-sdm845'));a=p.parse_args();repo=Path(__file__).resolve().parents[1];a.out.mkdir(parents=True,exist_ok=True);tools=a.port/'Common/edk2/BaseTools/Source/C/bin';build=a.workspace/'Build/HerculesBootUI/RELEASE_CLANG38/AARCH64'
 for stem,source,guid in [('HerculesBootMenu','SimpleInitMain.efi',UI),('AndroidHandoff','Reboot2PayloadApp.efi',ANDROID)]:
  target=a.out/(stem+'.efi');subprocess.run([str(tools/'GenFw'),'-z','-o',str(target),str(build/source)],check=True);check_pe(target);target.with_suffix(".txt").unlink(missing_ok=True)
  sec=a.out/(stem+'.pe32');subprocess.run([str(tools/'GenSec'),'-s','EFI_SECTION_PE32','-o',str(sec),str(target)],check=True);ffs=a.out/(stem+'.ffs');subprocess.run([str(tools/'GenFfs'),'-t','EFI_FV_FILETYPE_APPLICATION','-g',guid,'-s','-i',str(sec),'-o',str(ffs)],check=True);assert ffs.read_bytes()[:16]==uuid.UUID(guid).bytes_le;sec.unlink()
 cfg=repo/'source/edk2-integration/simpleinit.static.uefi.cfg';shutil.copyfile(cfg,a.out/cfg.name);raw=a.out/'config.raw';ui=a.out/'config.ui'
 subprocess.run([str(tools/'GenSec'),'-s','EFI_SECTION_RAW','-o',str(raw),str(cfg)],check=True);subprocess.run([str(tools/'GenSec'),'-s','EFI_SECTION_USER_INTERFACE','-n',cfg.name,'-o',str(ui)],check=True);subprocess.run([str(tools/'GenFfs'),'-t','EFI_FV_FILETYPE_FREEFORM','-g',CONFIG,'-s','-i',str(raw),'-i',str(ui),'-o',str(a.out/'HerculesBootConfig.ffs')],check=True);raw.unlink();ui.unlink()
 report={f.name:{'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in a.out.iterdir() if f.is_file() and f.suffix in ['.efi','.ffs','.cfg']};(a.out/'MANIFEST.json').write_text(json.dumps({'format':'UI components only','architecture':'AARCH64','font_mode':'external-local-RAW-FFS','apple_os_payloads':False,'files':report},indent=2));(a.out/'SHA256SUMS').write_text(''.join(v['sha256']+'  '+k+'\n' for k,v in sorted(report.items())));print(json.dumps(report,indent=2))
if __name__=='__main__':main()
