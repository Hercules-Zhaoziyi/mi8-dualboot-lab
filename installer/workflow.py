"""Explicit, resumable Mi8 workflow. No arbitrary shell commands in plans."""
import argparse,json,hashlib,urllib.request,xml.etree.ElementTree as ET,zipfile,shutil,os
from pathlib import Path
from mi8_install_desk import Engine,download,sha,checked_image
GOOGLE='https://dl.google.com/android/repository/'
def platform_tools(out):
 out=Path(out);out.mkdir(parents=True,exist_ok=True)
 with urllib.request.urlopen(GOOGLE+'repository2-1.xml',timeout=60) as r:data=r.read()
 root=ET.fromstring(data);package=next(x for x in root.iter() if x.tag.endswith('remotePackage') and x.get('path')=='platform-tools')
 archive=next(x for x in package.iter('archive') if x.findtext('host-os')=='windows');complete=archive.find('complete');name=complete.findtext('url');expected=complete.findtext('checksum');kind=complete.find('checksum').get('type','sha1')
 if '/' in name or chr(92) in name or not name.endswith('.zip') or kind not in ['sha1','sha256']:raise ValueError('Unexpected official tools metadata')
 target=out/name
 with urllib.request.urlopen(GOOGLE+name,timeout=60) as r,target.with_suffix('.part').open('wb') as f:shutil.copyfileobj(r,f,1024*1024)
 part=target.with_suffix('.part');h=hashlib.new(kind)
 with part.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 if h.hexdigest()!=expected.lower():raise ValueError('Official tools checksum mismatch')
 part.replace(target)
 with zipfile.ZipFile(target) as z:
  for member in z.infolist():
   dest=(out/member.filename).resolve()
   if not dest.is_relative_to(out.resolve()) or member.external_attr>>16&0o170000==0o120000:raise ValueError('Unsafe archive member')
  z.extractall(out)
 receipt={'source':GOOGLE+name,'official_metadata_checksum_type':kind,'official_metadata_checksum':expected,'sha256':sha(target),'metadata_sha256':hashlib.sha256(data).hexdigest()}
 receipt['executables']={n:sha(out/'platform-tools'/n) for n in ['adb.exe','fastboot.exe']}
 (out/'tools-receipt.json').write_text(json.dumps(receipt,indent=2));return out/'platform-tools'
def validate(plan):
 if plan.get('schema')!=1 or plan.get('device')!='dipper':raise ValueError('Only schema 1 and dipper supported')
 if plan.get('mode') not in ['prepare','existing-install']:raise ValueError('First-install partitioning has no validated adapter yet; use documented engineering flow')
 if any(k not in ['schema','device','mode','resources','twrp','candidate'] for k in plan):raise ValueError('Unknown plan keys')
 for name,entry in plan.get('resources',{}).items():
  if not name or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-_' for c in name):raise ValueError('Invalid resource name')
  if set(entry)-{'url','sha256','filename'}:raise ValueError('Unknown resource fields')
  if Path(entry['filename']).name!=entry['filename'] or any(x in entry['filename'] for x in ['/', chr(92), ':']):raise ValueError('Unsafe resource filename')
 if plan.get('mode')=='existing-install':
  for field in ['twrp','candidate']:
   if plan.get(field) not in plan.get('resources',{}):raise ValueError('Missing '+field+' resource')
 return plan
def load(path):return validate(json.loads(Path(path).read_text(encoding='utf-8-sig')))
def execute(planfile,work,apply=False):
 plan=load(planfile);work=Path(work).resolve();work.mkdir(parents=True,exist_ok=True);digest=hashlib.sha256(json.dumps(plan,sort_keys=True).encode()).hexdigest();statefile=work/'workflow.json'
 state=json.loads(statefile.read_text()) if statefile.exists() else {'plan_sha256':digest,'completed':[]}
 if state['plan_sha256']!=digest:raise ValueError('Plan changed; select a new work directory')
 if state.get('write_started'):raise RuntimeError('Previous write incomplete: inspect Engine pending_write and restore original boot; never auto-repeat')
 def save():
  tmp=statefile.with_suffix('.tmp');tmp.write_text(json.dumps(state,indent=2));tmp.replace(statefile)
 def gate(text):
  if not apply:raise RuntimeError('Device stages require --apply; preparation is complete')
  input(text+'\n完成后按回车继续；Ctrl+C 可停止：')
 tools=platform_tools(work/'tools') if not (work/'tools/platform-tools/adb.exe').exists() else work/'tools/platform-tools'
 receipt=json.loads((work/'tools/tools-receipt.json').read_text())
 for name,digest in receipt['executables'].items():
  if name not in ['adb.exe','fastboot.exe'] or sha(tools/name)!=digest:raise ValueError('Cached tools changed; stop and inspect cache')
 resources={}
 for name,e in plan.get('resources',{}).items():resources[name]=download(e['url'],e['sha256'],work/'downloads'/e['filename'])
 state['prepared']=True;save();print('Tools and pinned inputs ready. No device write performed.')
 if plan['mode']=='prepare' or not apply:return
 e=Engine(work/'device',tools)
 # Every launch rechecks real hardware and backup; completed markers never skip checks.
 gate('连接已解锁的小米8，手动进入 FASTBOOT。首次 BL 解锁/账号锁处理不由软件执行。')
 e.identify('fastboot');e.temp_twrp(resources[plan['twrp']],plan['resources'][plan['twrp']]['sha256'])
 gate('等待 TWRP 并保持连接；不要清除/格式化。')
 e.recovery()
 if e.size('darwin')<=0:raise RuntimeError('Missing existing iOS layout; first-install adapter required')
 e.backup();state['completed'].append('boot-recovery-backup');save()
 e.run('adb','reboot','bootloader')
 gate('待 FASTBOOT 出现。接下来临时启动候选；菜单出现立即按音量键取消10秒默认安卓，再选iOS。')
 e.identify('fastboot');candidate=resources[plan['candidate']];checksum=plan['resources'][plan['candidate']]['sha256'];checked_image(candidate,checksum)
 e.run('fastboot','boot',str(candidate),timeout=120)
 answer=input('确认两个系统、声音、照片/应用恢复均通过后输入 RAM TEST PASSED；否则停止并保留原正式boot：')
 if answer!='RAM TEST PASSED':raise RuntimeError('RAM test not accepted; original boot unchanged')
 gate('让iOS完成周期存档，进入 TWRP 并重连。备份重要iOS数据后再继续；本工具目前只备份boot/recovery。')
 e.recovery();e.identify('adb');e.size('darwin')
 # State binding and complete rollback are checked again by Engine.flash.
 token=input('即将仅写boot。必须已独立备份用户数据与GPT；输入 FLASH boot 确认：')
 if token!='FLASH boot':raise RuntimeError('No write authorized')
 state['write_started']=True;save();e.flash(candidate,checksum,'boot',token);state['write_started']=False;state['completed'].append('boot-write-readback');save()
 print('Boot write and readback passed. No automatic reboot; verify on phone. Other partitions were not written.')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--plan',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--apply',action='store_true');a=p.parse_args();execute(a.plan,a.work,a.apply)
