from pathlib import Path
import importlib.util,tempfile,hashlib,unittest,json,subprocess
S=Path(__file__).resolve().parents[1]
f=S/"installer/mi8_install_desk.py"
spec=importlib.util.spec_from_file_location("desk",f);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Test(unittest.TestCase):
 def test_image_checks(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'a';p.write_bytes(b'ANDROID!'+bytes(4088));h=m.sha(p);self.assertEqual(m.checked_image(p,h),p)
   with self.assertRaises(ValueError):m.checked_image(p,'0'*64)
   with self.assertRaises(ValueError):m.checked_image(p,h,100)
   p.write_bytes(b'MZ'+bytes(4094))
   with self.assertRaises(ValueError):m.checked_image(p,m.sha(p))
 def test_download_policy(self):
  with self.assertRaises(ValueError):m.download('http://example.com/a','0'*64,'unused')
  with self.assertRaises(ValueError):m.download('https://example.com/a','','unused')
 def test_states_and_identity(self):
  with tempfile.TemporaryDirectory() as d:
   e=m.Engine(d,d,log=lambda s:None);e.run=lambda tool,*a,**kw:'abc\tunauthorized' if a==('devices',) else ''
   with self.assertRaises(RuntimeError):e.identify('adb')
   e.run=lambda tool,*a,**kw:'abc\tdevice' if a==('devices',) else 'other-device'
   with self.assertRaises(RuntimeError):e.identify('adb')
   e.state['pending_write']={'part':'boot'}
   with self.assertRaises(RuntimeError):e.backup()
   with self.assertRaises(ValueError):e.flash('none','0'*64,'boot','')
 def test_timeout_does_not_restore(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'boot.img';p.write_bytes(b'ANDROID!'+bytes(4088));e=m.Engine(d,d,log=lambda s:None);e.state['backup']={'boot':{'file':str(p),'sha256':m.sha(p),'bytes':4096}};e.recovery=lambda:None;e.size=lambda x:4096;e.remote_sha=lambda x:m.sha(p);calls=[]
   def run(tool,*a,**kw):
    if 'dd' in a:raise subprocess.TimeoutExpired('dd',180)
    return ''
   e.run=run;e.restore=lambda p:calls.append(p)
   with self.assertRaises(subprocess.TimeoutExpired):e.flash(p,m.sha(p),'boot','FLASH boot')
   self.assertFalse(calls);self.assertTrue(e.state['pending_write'])

if __name__=="__main__":unittest.main()
