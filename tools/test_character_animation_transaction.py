"""Cheap multi-process publication/recovery tests; no Blender or native build."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from transaction_character_animation import locked_output,staging,publish


def worker(root,mode,hold=0):
    root=Path(root)
    with locked_output(root,timeout=5):
        (root/'ready').write_text('locked')
        if mode=='read':
            print(json.dumps([(root/n).read_text() for n in ['a','b','manifest']]),flush=True);return
        before=(root/'input').read_text()
        if hold:time.sleep(hold)
        if mode=='hold':return
        if (root/'manifest').read_text()=='new':return
        with staging(root) as stage:
            for name in ['a','b','manifest']:(stage/name).write_text('new')
            if before!=(root/'input').read_text():raise ValueError('Input changed during generation')
            if mode=='fail':raise ValueError('Fake validation failure')
            count=root/'attempts';count.write_text(str(int(count.read_text())+1))
            def hook(index):
                if mode=='crash' and index==0:os._exit(77)
                if mode=='error' and index==0:raise RuntimeError('Fake publication failure')
            publish(root,stage,['a','b','manifest'],after_replace=hook)


class Transactions(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        for n in ['a','b','manifest']:(self.root/n).write_text('old')
        (self.root/'input').write_text('source');(self.root/'attempts').write_text('0')
        self.jobs=[]
    def tearDown(self):
        for p in self.jobs:
            if p.poll() is None:p.kill();p.wait()
        self.temp.cleanup()
    def start(self,mode,hold=0):
        p=subprocess.Popen([sys.executable,__file__,'worker',str(self.root),mode,str(hold)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.jobs.append(p);return p
    def ready(self):
        end=time.monotonic()+5
        while not (self.root/'ready').exists():
            if time.monotonic()>end:self.fail('Worker did not acquire lock')
            time.sleep(.01)
    def finish(self,p,code=0):
        out,err=p.communicate(timeout=10);self.assertEqual(p.returncode,code,err);return out
    def read(self):return json.loads(self.finish(self.start('read')))
    def test_freshness_ignores_unconsumed_infantry_inputs(self):
        import hashlib
        from unittest.mock import patch
        import cook_character_animation as cook
        cache=self.root/'cache';(cache/'exports').mkdir(parents=True);(cache/'reports').mkdir()
        (cache/'exports/body.glb').write_bytes(b'fixture')
        (self.root/'body.glb').write_bytes(b'source');(self.root/'shared.py').write_text('shared')
        digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
        config={'characters':[{'id':'body'}]}
        report={'config':config['characters'][0],'source':'body.glb','source_sha256':digest(self.root/'body.glb'),
                'authoring_inputs':{'shared.py':digest(self.root/'shared.py')}}
        (cache/'reports/body.json').write_text(json.dumps(report))
        (cache/'reports/validation.json').write_text(json.dumps({'characters':[{'id':'body','sha256':digest(cache/'exports/body.glb')}]}))
        with patch.object(cook,'ROOT',self.root),patch.object(cook,'EXPORTS',cache/'exports'):
            self.assertTrue(cook.fresh_exports(config))
            (self.root/'infantry.py').write_text('new isolated work')
            self.assertTrue(cook.fresh_exports(config))
            (self.root/'shared.py').write_text('changed consumed input')
            self.assertFalse(cook.fresh_exports(config))

    def test_two_producers_and_two_readers(self):
        first=self.start('produce',.25);self.ready()
        second=self.start('produce');readers=[self.start('read') for _ in range(2)]
        self.finish(first);self.finish(second)
        for p in readers:self.assertEqual(json.loads(self.finish(p)),['new']*3)
        self.assertEqual((self.root/'attempts').read_text(),'1')
    def test_crash_mid_publish_recovers_old_set_and_retry(self):
        self.finish(self.start('crash'),77)
        self.assertTrue((self.root/'.animation-transaction.json').exists())
        self.assertEqual(self.read(),['old']*3)
        self.assertFalse((self.root/'.animation-transaction.json').exists())
        self.finish(self.start('produce'));self.assertEqual(self.read(),['new']*3)
    def test_exception_mid_publish_rolls_back(self):
        self.finish(self.start('error'),1);self.assertEqual(self.read(),['old']*3)
    def test_failed_validation_preserves_old_set(self):
        self.finish(self.start('fail'),1);self.assertEqual(self.read(),['old']*3)
    def test_killed_lock_owner_releases_without_stale_lock(self):
        p=self.start('hold',30);self.ready();p.kill();p.communicate(timeout=5)
        self.assertEqual(self.read(),['old']*3)
        self.finish(self.start('produce'));self.assertEqual(self.read(),['new']*3)
    def test_changed_input_aborts_before_publication(self):
        p=self.start('produce',.25);self.ready();(self.root/'input').write_text('changed')
        self.finish(p,1);self.assertEqual(self.read(),['old']*3)
    def test_lock_timeout_is_actionable(self):
        p=self.start('hold',30);self.ready()
        with self.assertRaisesRegex(TimeoutError,'Animation producer lock timed out'):
            with locked_output(self.root,timeout=.05):self.fail('Concurrent owner admitted')
        p.kill();p.communicate(timeout=5)
    def test_new_file_is_removed_on_rollback(self):
        with locked_output(self.root),staging(self.root) as stage:
            (stage/'new-only').write_text('new');(stage/'a').write_text('new')
            def fail(index):raise RuntimeError('stop')
            with self.assertRaises(RuntimeError):publish(self.root,stage,['new-only','a'],fail)
        self.assertFalse((self.root/'new-only').exists());self.assertEqual(self.read(),['old']*3)
    def test_lock_and_staging_never_enter_packaged_output(self):
        with locked_output(self.root),staging(self.root) as stage:
            self.assertNotIn(self.root,stage.parents)
            self.assertEqual(sorted(p.name for p in self.root.iterdir()),['a','attempts','b','input','manifest'])


if __name__=='__main__':
    if len(sys.argv)>1 and sys.argv[1]=='worker':worker(sys.argv[2],sys.argv[3],float(sys.argv[4]))
    else:unittest.main()
