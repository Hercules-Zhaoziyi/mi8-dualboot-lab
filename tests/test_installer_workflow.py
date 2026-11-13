from pathlib import Path
import sys,tempfile,json,unittest,io,zipfile,hashlib
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'installer'))
import workflow as w
class WorkflowTest(unittest.TestCase):
 def test_reject_first_install(self):
  with self.assertRaises(ValueError):w.validate({'schema':1,'device':'dipper','mode':'first-install'})
 def test_reject_other_device(self):
  with self.assertRaises(ValueError):w.validate({'schema':1,'device':'other','mode':'prepare'})
 def test_reject_escape(self):
  with self.assertRaises(ValueError):w.validate({'schema':1,'device':'dipper','mode':'prepare','resources':{'x':{'filename':'../escape','url':'https://example.com','sha256':'0'*64}}})
 def test_reject_missing_targets(self):
  with self.assertRaises(ValueError):w.validate({'schema':1,'device':'dipper','mode':'existing-install','resources':{}})
 def test_pinned_zip_download(self):
  b=io.BytesIO()
  with zipfile.ZipFile(b,'w') as z:
   z.writestr('platform-tools/adb.exe',b'fixture-adb');z.writestr('platform-tools/fastboot.exe',b'fixture-fastboot')
  payload=b.getvalue();digest=hashlib.sha1(payload).hexdigest();xml=f'<root><remotePackage path="platform-tools"><archives><archive><host-os>windows</host-os><complete><url>tools.zip</url><checksum type="sha1">{digest}</checksum></complete></archive></archives></remotePackage></root>'.encode()
  with tempfile.TemporaryDirectory() as d,patch.object(w.urllib.request,'urlopen',side_effect=[io.BytesIO(xml),io.BytesIO(payload)]):
   out=w.platform_tools(d);self.assertEqual((out/'adb.exe').read_bytes(),b'fixture-adb');r=json.loads((Path(d)/'tools-receipt.json').read_text());self.assertEqual(r['executables']['adb.exe'],hashlib.sha256(b'fixture-adb').hexdigest())
 def test_plan_change_never_touches_device(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'plan.json';p.write_text(json.dumps({'schema':1,'device':'dipper','mode':'prepare'}));work=Path(d)/'work';work.mkdir();(work/'workflow.json').write_text(json.dumps({'plan_sha256':'changed'}))
   with patch.object(w,'platform_tools',side_effect=AssertionError('must not download')):
    with self.assertRaises(ValueError):w.execute(p,work)
if __name__=='__main__':unittest.main()
