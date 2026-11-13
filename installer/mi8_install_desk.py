"""Mi8 Install Desk: local-only recovery/boot workflow. Python 3.11+, stdlib."""
import argparse,hashlib,json,os,re,subprocess,time,urllib.request,urllib.parse,zipfile,shutil,threading,queue
from pathlib import Path
ROOT=Path(__file__).resolve().parent
def sha(p):
 h=hashlib.sha256()
 with open(p,'rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def checked_image(p,digest,capacity=None):
 p=Path(p)
 if not re.fullmatch('[0-9a-fA-F]{64}',digest or '') or sha(p)!=digest.lower():raise ValueError('镜像 SHA256 不匹配或未填写可信摘要')
 if not p.read_bytes()[:8]==b'ANDROID!':raise ValueError('只接受 Android boot/recovery 镜像；EFI/FFS 不能刷入')
 if capacity and not 0<p.stat().st_size<=capacity:raise ValueError('镜像超出实际分区容量')
 return p
def download(url,digest,dest):
 if urllib.parse.urlparse(url).scheme!='https' or not re.fullmatch('[0-9a-fA-F]{64}',digest or ''):raise ValueError('下载必须提供 HTTPS 与独立可信 SHA256')
 dest=Path(dest);dest.parent.mkdir(parents=True,exist_ok=True)
 if dest.exists():
  if sha(dest)==digest.lower():return dest
  raise ValueError('已有缓存摘要不匹配；保留现场，请更换输出文件名')
 part=dest.with_suffix(dest.suffix+'.part')
 with urllib.request.urlopen(url,timeout=60) as r,part.open('wb') as f:
  if urllib.parse.urlparse(r.url).scheme!='https':raise ValueError('拒绝非 HTTPS 重定向')
  shutil.copyfileobj(r,f,1024*1024)
 if sha(part)!=digest.lower():raise ValueError('下载校验失败，保留 .part；禁止使用')
 part.replace(dest);return dest
class Engine:
 def __init__(self,work,tools,serial='',log=print):
  self.work=Path(work).resolve();self.work.mkdir(parents=True,exist_ok=True);self.tools=Path(tools);self.serial=serial;self.log=log;self.statefile=self.work/'state.json'
  self.state=json.loads(self.statefile.read_text()) if self.statefile.exists() else {'schema':1,'events':[]}
 def save(self):
  tmp=self.statefile.with_suffix('.tmp');tmp.write_text(json.dumps(self.state,ensure_ascii=False,indent=2),encoding='utf8');tmp.replace(self.statefile)
 def run(self,tool,*args,timeout=45):
  exe=self.tools/(tool+('.exe' if os.name=='nt' else ''))
  cmd=[str(exe)]+(['-s',self.serial] if self.serial else [])+list(args)
  r=subprocess.run(cmd,capture_output=True,text=True,timeout=timeout,encoding='utf8',errors='replace')
  out=r.stdout+'\n'+r.stderr
  self.state['events'].append({'time':time.time(),'tool':tool,'args':list(args),'exit':r.returncode,'output':out});self.save()
  if r.returncode:raise RuntimeError(out[-1600:])
  return out.strip()
 def one(self,mode):
  out=self.run(mode,'devices');rows=[]
  for l in out.splitlines():
   a=l.split()
   if len(a)>=2 and a[1] in ['device','recovery','fastboot','unauthorized','offline']:rows.append(a)
  if self.serial:rows=[a for a in rows if a[0]==self.serial]
  if len(rows)!=1:raise RuntimeError('必须仅连接一台目标设备（或指定序列号）')
  if rows[0][1] in ['unauthorized','offline']:raise RuntimeError('设备未授权或离线：解锁手机并允许 USB 调试')
  serial=rows[0][0]
  if self.state.get('serial') and self.state['serial']!=serial:raise RuntimeError('目标设备已改变；请选择新的工作目录')
  self.serial=serial;self.state['serial']=serial;self.save()
 def identify(self,mode):
  self.one(mode)
  if mode=='adb':
   product=self.run('adb','shell','getprop ro.product.device').strip()
   if product!='dipper':raise RuntimeError('仅支持小米8 dipper，拒绝 '+product)
  else:
   if not re.search(r'product:\s*dipper\b',self.run('fastboot','getvar','product')):raise RuntimeError('FASTBOOT 机型不是 dipper')
   info=self.run('fastboot','oem','device-info')
   if not re.search(r'Device unlocked:\s*true',info,re.I):raise RuntimeError('BL 未解锁或无法核实；软件不执行自动解锁')
  self.log('设备身份通过：dipper；私有日志保存在工作目录')
 def recovery(self):
  self.identify('adb')
  if self.run('adb','shell','getprop ro.twrp.boot').strip()!='1':raise RuntimeError('请先进入 TWRP；Android root 不代替 recovery 核验')
  if self.run('adb','shell','id','-u').strip()!='0':raise RuntimeError('Recovery shell 不是 root')
  # This workflow deliberately excludes A/B and non-stock layouts.
  slot=self.run('adb','shell','getprop ro.boot.slot_suffix').strip()
  if slot:raise RuntimeError('发现 A/B 槽位；不适用 Mi8 单 boot 流程')
 def size(self,part):
  if part not in ['boot','recovery','darwin']:raise ValueError('不允许操作的分区')
  return int(self.run('adb','shell','blockdev','--getsize64','/dev/block/by-name/'+part))
 def remote_sha(self,path):
  out=self.run('adb','shell','sha256sum',path)
  m=re.match(r'([0-9a-f]{64})\s',out)
  if not m:raise RuntimeError('无法获得设备端 SHA256')
  return m[1]
 def backup(self):
  if self.state.get("pending_write"):raise RuntimeError("写入未结束，禁止用新备份覆盖回退记录")
  self.recovery();free=shutil.disk_usage(self.work).free
  if free<1024**3:raise RuntimeError('工作磁盘至少需要 1 GiB 空闲；系统镜像另外需要更多空间')
  folder=self.work/('backup-'+time.strftime('%Y%m%d-%H%M%S'));folder.mkdir()
  records={}
  for part in ['boot','recovery']:
   n=self.size(part);remote='/tmp/mi8-backup-'+part+'.img'
   self.run('adb','shell','dd','if=/dev/block/by-name/'+part,'of='+remote,'bs=1048576',timeout=180)
   digest=self.remote_sha(remote);local=folder/(part+'.img');self.run('adb','pull',remote,str(local),timeout=180)
   if local.stat().st_size!=n or sha(local)!=digest:raise RuntimeError('备份不完整或摘要不一致')
   records[part]={'file':str(local),'bytes':n,'sha256':digest}
  self.state['backup']=records;self.save();self.log('boot/recovery 完整备份校验通过。此备份不包含用户数据或 GPT。')
 def temp_twrp(self,image,digest):
  self.identify('fastboot');p=checked_image(image,digest)
  self.run('fastboot','boot',str(p.resolve()),timeout=90);self.log('已发送 TWRP 临时启动；手机出现后保持连接。尚未刷 recovery。')
 def flash(self,image,digest,part,confirmation):
  if confirmation!='FLASH '+part:raise ValueError('请输入 FLASH '+part+' 以确认本次具体写入')
  self.recovery();records=self.state.get('backup',{});b=records.get(part)
  if not b or sha(b['file'])!=b['sha256'] or Path(b['file']).stat().st_size!=b['bytes']:raise RuntimeError('缺少有效完整回退备份')
  capacity=self.size(part)
  if capacity!=b['bytes']:raise RuntimeError('分区容量已变化，停止写入')
  p=checked_image(image,digest,capacity);remote='/tmp/mi8-candidate.img'
  self.run('adb','push',str(p.resolve()),remote,timeout=180)
  if self.remote_sha(remote)!=digest.lower():raise RuntimeError('设备端候选校验不一致')
  self.state['pending_write']={'part':part,'digest':digest,'backup':b};self.save()
  try:
   self.run('adb','shell','dd','if='+remote,'of=/dev/block/by-name/'+part,'bs=1048576','conv=fsync',timeout=180)
   self.run('adb','shell',f'head -c {p.stat().st_size} /dev/block/by-name/{part} > /tmp/mi8-readback.img',timeout=180)
   if self.remote_sha('/tmp/mi8-readback.img')!=digest.lower():raise RuntimeError('候选全长回读不一致')
  except subprocess.TimeoutExpired:
   self.log("操作超时，设备端写入可能仍在进行；禁止立即恢复或重启。保持连接，人工核实后再恢复。")
   raise
  except Exception:
   self.log('写入未通过。仅在 recovery 仍可通讯时尝试恢复已校验备份；失败时保留 pending_write，勿盲目重启。')
   self.restore(part);raise
  self.state['pending_write']=None;self.save();self.log('写入与候选全长回读通过；请执行实际系统验收。')
 def restore(self,part):
  self.recovery();b=self.state.get('backup',{}).get(part)
  if not b or sha(b['file'])!=b['sha256'] or self.size(part)!=b['bytes']:raise RuntimeError('恢复文件/目标校验失败')
  self.run('adb','push',b['file'],'/tmp/mi8-restore.img',timeout=180)
  if self.remote_sha('/tmp/mi8-restore.img')!=b['sha256']:raise RuntimeError('恢复传输校验失败')
  self.run('adb','shell','dd','if=/tmp/mi8-restore.img','of=/dev/block/by-name/'+part,'bs=1048576','conv=fsync',timeout=180)
  if self.remote_sha('/dev/block/by-name/'+part)!=b['sha256']:raise RuntimeError('恢复回读失败；保持 recovery 并求助')
  self.state['pending_write']=None;self.save();self.log('原分区完整恢复并核验通过')
def gui():
 import tkinter as tk
 from tkinter import ttk,filedialog,messagebox
 w=tk.Tk();w.title('Hercules Mi8 Install Desk — 开发预览');w.geometry('900x720');q=queue.Queue();busy=False
 fields={}
 def field(label,value=''):
  row=ttk.Frame(w);row.pack(fill='x',padx=12,pady=3);ttk.Label(row,text=label,width=19).pack(side='left');v=tk.StringVar(value=value);ttk.Entry(row,textvariable=v).pack(side='left',fill='x',expand=True);fields[label]=v
 for l,v in [('工作目录',str(ROOT.parent/'local/install-desk')),('platform-tools目录',''),('镜像文件',''),('可信 SHA256',''),('HTTPS 下载地址',''),('写入确认','')]:field(l,v)
 ttk.Label(w,text='先准备 → FASTBOOT 核验 → 临时 TWRP → 备份 → 仅在完整候选验证后写入。\n首次 iOS 分区/系统构建尚需工程步骤；本软件不伪装为已完成的一键整包安装。',wraplength=850).pack(pady=8)
 log=tk.Text(w,height=17,wrap='word');log.pack(fill='both',expand=True,padx=12,pady=8)
 def action(fn):
  nonlocal busy
  if busy:return
  busy=True
  vals={k:v.get() for k,v in fields.items()}
  def worker():
   try:
    e=Engine(vals['工作目录'],vals['platform-tools目录'],log=lambda s:q.put(s));fn(e,vals)
   except Exception as ex:q.put('停止：'+str(ex))
   finally:q.put(None)
  threading.Thread(target=worker,daemon=True).start()
 def poll():
  nonlocal busy
  while not q.empty():
   x=q.get()
   if x is None:busy=False
   else:log.insert('end',str(x)+'\n');log.see('end')
  w.after(100,poll)
 buttons=ttk.Frame(w);buttons.pack(fill='x',padx=12)
 actions=[('检查电脑',lambda e,v:e.log(json.dumps({'free_GiB':round(shutil.disk_usage(e.work).free/1024**3,1),'tools_present':all((e.tools/(n+'.exe')).exists() for n in ['adb','fastboot']),'pending_write':bool(e.state.get('pending_write'))},ensure_ascii=False))),('ADB核验',lambda e,v:e.identify('adb')),('FASTBOOT核验',lambda e,v:e.identify('fastboot')),('临时启动TWRP',lambda e,v:e.temp_twrp(v['镜像文件'],v['可信 SHA256'])),('备份boot/recovery',lambda e,v:e.backup()),('刷入boot',lambda e,v:e.flash(v['镜像文件'],v['可信 SHA256'],'boot',v['写入确认'])),('刷入recovery',lambda e,v:e.flash(v['镜像文件'],v['可信 SHA256'],'recovery',v['写入确认'])),('恢复boot',lambda e,v:e.restore('boot')),('校验下载',lambda e,v:e.log(str(download(v['HTTPS 下载地址'],v['可信 SHA256'],Path(v['工作目录'])/'downloads'/Path(v['镜像文件']).name))))]
 for i,(text,fn) in enumerate(actions):ttk.Button(buttons,text=text,command=lambda f=fn:action(f)).grid(row=i//4,column=i%4,padx=3,pady=4)
 def close():
  if busy:messagebox.showwarning('任务运行中','请等待当前操作结束，写入期间不要关闭。');return
  w.destroy()
 w.protocol('WM_DELETE_WINDOW',close);poll();w.mainloop()
if __name__=='__main__':gui()
