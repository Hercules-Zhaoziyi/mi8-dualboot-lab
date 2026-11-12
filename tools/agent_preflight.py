#!/usr/bin/env python3
"""Repository-only preflight; no device commands and no downloads."""
from pathlib import Path
import json,shutil,platform
r=Path(__file__).resolve().parents[1]
required=['AGENTS.md','docs/IOS-REPRODUCTION.md','docs/PORTING.md','UPSTREAM_SOURCES.json','source/simple-init/src/gui/interface/core/bootmenu_hercules.inc']
missing=[v for v in required if not (r/v).is_file()]
print(json.dumps({'host':platform.system(),'architecture':platform.machine(),'missing_repository_inputs':missing,'tools':{v:bool(shutil.which(v)) for v in ['git','python3','clang','make','aarch64-linux-gnu-gcc']},'free_bytes':shutil.disk_usage(r).free,'phone_contacted':False,'os_inputs':'must be supplied locally','ready_to_flash':False},indent=2))
raise SystemExit(bool(missing))
