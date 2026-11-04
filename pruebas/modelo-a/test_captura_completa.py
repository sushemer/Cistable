import importlib.util
from pathlib import Path
import struct
import unittest
import zlib
spec=importlib.util.spec_from_file_location('capture',Path(__file__).with_name('captura_completa.py'))
capture=importlib.util.module_from_spec(spec);spec.loader.exec_module(capture)
def frame(kind,ident,time,payload):
    data=b'CSTCAP01'+struct.pack('<BBHIQ',kind,0,len(payload),ident,time)+payload
    return data+struct.pack('<I',zlib.crc32(data))
def wire(t,seq):
    data=bytearray(124);data[:4]=b'CST2';data[4]=2;struct.pack_into('<HII',data,6,124,99,seq)
    struct.pack_into('<5Q',data,16,t,t,t-500,t-1000,t)
    struct.pack_into('<3I',data,56,seq,seq,seq);struct.pack_into('<11f',data,68,1,2,3,4,5,6,1,0,0,0,17)
    data[115]=15;struct.pack_into('<I',data,120,seq);return bytes(data)
def proc(rx,seq,time,previous,current,decision=2,disarms=0):
    p=bytearray(120);struct.pack_into('<QII6Q',p,0,rx,99,seq,time,previous,current,current-500,time+50000,disarms)
    p[112]=7;p[115]=1;p[116]=decision
    return bytes(p)
class CaptureTests(unittest.TestCase):
    def test_configuration_metadata_round_trip(self):
        text=b'CONFIG|profile=PIPA_VIRTUAL_ENTRADAS_REALES|lever_enabled=0|r_m=0,0,0'
        rows,_,errors,tail=capture.frames(frame(6,1,0,b'boot')+frame(8,2,1,text))
        self.assertEqual((errors,tail),(0,0))
        self.assertEqual(capture.analyze(rows)[0]['configuration_records'],[text.decode()])
    def test_c_encoder_fixture(self):
        rows,_,errors,tail=capture.frames(Path(__file__).with_name('build').joinpath('capture-codec.raw').read_bytes())
        self.assertEqual((len(rows),errors,tail),(1,0,0));self.assertIn(b'fixture',rows[0]['payload'])
    def test_binary_mixed_text_crc_and_truncation(self):
        a=frame(6,1,0,b'boot');b=frame(1,2,1,b'\n\r\x00CSTCAP01');bad=bytearray(b);bad[-1]^=1
        rows,text,errors,tail=capture.frames(b'RESUMEN\n'+a+bad+b+b[:18])
        self.assertEqual([r['id'] for r in rows],[1,2]);self.assertGreater(errors,0);self.assertEqual(tail,18)
        self.assertIn(b'RESUMEN',text)
    def test_100_and_50_hz_actual_dt_async_reports(self):
        for period in (10000,20000):
            data=frame(6,1,0,b'physical_fixture')
            for k in range(1,101):
                t=period*k;raw=struct.pack('<Q',k)+b'\x01'*6+struct.pack('<bB',-50,1)+wire(t,k)
                data+=frame(1,k*2,t+900000,raw)
                data+=frame(2,k*2+1,t+900100,proc(k,k,t+900000,t-period,t,1 if k==1 else 2))
            rows,_,errors,tail=capture.frames(data);summary,measurements,processes=capture.analyze(rows)
            self.assertEqual((errors,tail,summary['capture_id_gaps']),(0,0,0))
            self.assertEqual(summary['committed_steps'],99)
            self.assertEqual(summary['dt_source_us_max'],period)
            self.assertAlmostEqual(summary['received']['99']['packet_rate_hz'],1e6/period)
            self.assertEqual(summary['received']['99']['max_skew_us'],1000)
            self.assertEqual(summary['admitted_skew_us_max'],1000)
            self.assertEqual(summary['admitted_gyro_accel_separation_us_max'],500)
            self.assertEqual(measurements[-1]['processing_residence_us'],100)
            self.assertEqual(processes[-1]['rx_id'],100)
    def test_lost_capture_and_control_overwrite_are_distinct(self):
        data=frame(6,1,0,b'boot')
        for k in (1,2): data+=frame(1,k+1,k*10000,struct.pack('<Q',k)+b'\0'*8+wire(k*10000,k))
        data+=frame(2,5,21000,proc(2,2,20000,0,20000,1)) # capture ID4 deliberately missing
        summary,m,_=capture.analyze(capture.frames(data)[0])
        self.assertEqual(summary['capture_id_gaps'],1);self.assertIn('capture_loss_not_complete',summary['warnings'])
        self.assertEqual(m[0]['disposition'],'unknown_due_to_capture_loss')
        self.assertEqual(m[1]['disposition'],'initial_anchor')
        complete=data.replace(frame(2,5,21000,proc(2,2,20000,0,20000,1)),frame(2,4,21000,proc(2,2,20000,0,20000,1)))
        summary,m,_=capture.analyze(capture.frames(complete)[0])
        self.assertEqual(summary['capture_id_gaps'],0)
        self.assertEqual(m[0]['disposition'],'not_processed_latest_slot')
    def test_invalid_wire_is_saved_but_not_a_valid_sensor_rate(self):
        bad=bytearray(wire(10000,1));struct.pack_into('<f',bad,92,2)
        payload=struct.pack('<Q',1)+b'\0'*8+bytes(bad)
        summary,measurements,_=capture.analyze(capture.frames(frame(6,1,0,b'boot')+frame(1,2,10000,payload))[0])
        self.assertFalse(measurements[0]['wire_syntax_valid'])
        self.assertEqual(summary['received']['99']['invalid_wire'],1)
        self.assertEqual(summary['received']['99']['reports'],{})
if __name__=='__main__': unittest.main()
