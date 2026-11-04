import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('analyzer',Path(__file__).with_name('analizar-mesa.py'))
analyzer=importlib.util.module_from_spec(spec);spec.loader.exec_module(analyzer)

class CaptureTests(unittest.TestCase):
    def test_events_duration_and_boots(self):
        result=analyzer.summarize('''DIAG_PRINT|evaluation_local_us=100
MODEL_EVENT|kind=DISARM|disarm_id=1|cause=source gap|operator_required=1
MODEL_EVENT|kind=RECOVER|disarm_id=1|missing_interval_us=60000|operator_required=1
MODEL_HEALTH|disarms=1|event_drops=0|no_dynamic_total_us=60000
DIAG_PRINT|evaluation_local_us=10
MODEL_HEALTH|disarms=0|event_drops=0
''')
        a,b=result['cabin_boots'];self.assertEqual(a['disarm_counter'],1)
        self.assertEqual(a['disarms'][0]['cause'],'source gap')
        self.assertEqual(a['recoveries'][0]['missing_interval_us'],'60000')
        self.assertEqual(b['disarm_counter'],0)
    def test_incomplete_capture_not_pass(self):
        boot=analyzer.summarize('MODEL_HEALTH|disarms=2|event_drops=1')['cabin_boots'][0]
        self.assertIn('event_queue_overflow',boot['warnings'])
        self.assertIn('capture_missing_disarm_events_or_started_late',boot['warnings'])

if __name__=='__main__': unittest.main()
