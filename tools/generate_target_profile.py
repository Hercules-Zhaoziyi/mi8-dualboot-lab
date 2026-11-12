#!/usr/bin/env python3
"""Generate local menu integration inputs, never platform firmware or a flasher."""
from pathlib import Path
import argparse,json,re,uuid
p=argparse.ArgumentParser(description=__doc__);p.add_argument('profile',type=Path);p.add_argument('--out',type=Path,required=True);a=p.parse_args();repo=Path(__file__).resolve().parents[1];out=a.out.resolve()
if not out.is_relative_to((repo/'local').resolve()) or out.exists():raise SystemExit('Use a new output directory under local/')
d=json.loads(a.profile.read_text());entries=[d['primary'],d['secondary']]
for e in entries:
 if not re.fullmatch('[a-z][a-z0-9_-]{0,31}',e['id']):raise SystemExit('Invalid target id')
 if not e.get('efi_guid') or uuid.UUID(e['efi_guid']).int==0:raise SystemExit('A real target EFI GUID is required')
 for k in ['label','version']:
  if not isinstance(e[k],str) or len(e[k])>48 or any(ord(c)<32 for c in e[k]):raise SystemExit('Invalid display label')
if entries[0]['id']==entries[1]['id']:raise SystemExit('Target IDs must differ')
quote=lambda s:json.dumps(s,ensure_ascii=False)
header='/* Local generated target configuration. */\n'
for i,e in enumerate(entries):
 for key,field in [('ID','id'),('LABEL','label'),('VERSION','version')]:header+=f'#define HERCULES_TARGET_{i}_{key} {quote(e[field])}\n'
cfg='boot {\n hercules_skin = true\n timeout = 10\n default = '+quote(entries[0]['id'])+'\n configs {\n'
for e in entries:cfg+=f'  {e["id"]} {{\n mode = "efi"\n desc = {quote(e["label"])}\n show = true\n enabled = true\n extra {{ efi_fv_guid = {quote(str(uuid.UUID(e["efi_guid"])))} }}\n }}\n'
cfg+=' }\n}\n';out.mkdir(parents=True);(out/'hercules_target.h').write_text(header);(out/'simpleinit.static.uefi.cfg').write_text(cfg);(out/'README.txt').write_text('Integrate the header beside bootmenu_hercules.inc and the config as the host FV config resource. Supply your own platform DSC, target adapters, fonts and validated boot chain. No image was built or device changed.\n');print('Local integration inputs generated; hardware/iOS support is not implied.')
