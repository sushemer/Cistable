"""Investigate the historical rearm assertion without changing active firmware.
Current executable is captured first. The old cc_arm behavior is reconstructed
in build/fallo-integration only, clearly distinguished from the original binary.
"""
from pathlib import Path
import argparse
import ctypes
import hashlib
import json
import os
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--output',default='pruebas/modelo-a/build/fallo-integration')
arguments=parser.parse_args()
OUT=(ROOT/arguments.output).resolve()
if not OUT.is_relative_to(ROOT/'pruebas/modelo-a/build'):
    raise SystemExit('Evidence directory must stay inside pruebas/modelo-a/build')
if OUT.exists():
    raise SystemExit('Evidence directory already exists; choose a new --output to preserve it')
OUT.mkdir(parents=True)
if os.name=='nt':
    # Process-local, inherited by children. Avoid OS fault dialog blocking capture.
    ctypes.windll.kernel32.SetErrorMode(0x8003)

def capture(label,command,timeout=30):
    stdout=OUT/(label+'.stdout.txt');stderr=OUT/(label+'.stderr.txt')
    with stdout.open('wb') as out,stderr.open('wb') as err:
        process=subprocess.Popen(command,cwd=ROOT,stdout=out,stderr=err)
        timed_out=False
        try: code=process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out=True;process.kill();code=process.wait()
    result={'command':command,'cwd':str(ROOT),'returncode':code,
            'returncode_hex':f'0x{code & 0xffffffff:08X}', 'timed_out':timed_out,
            'stdout':str(stdout),'stderr':str(stderr)}
    (OUT/(label+'.json')).write_text(json.dumps(result,indent=2),encoding='utf8')
    print(label,json.dumps(result))
    print(stderr.read_text(errors='replace')[-1500:])
    return result

current=ROOT/'pruebas/modelo-a/build/integration.exe'
saved=OUT/'actual-previo'
saved.mkdir(exist_ok=True)
shutil.copy2(current,saved/'integration.exe')
(saved/'sha256.txt').write_text(hashlib.sha256(current.read_bytes()).hexdigest())
capture('actual-antes-de-cambios',[str(current)])

control=(ROOT/'firmware/componentes/control_cabina/control_cabina.c').read_text()
reset='''    s->alpha.initialized=false;
    s->last_accel_us=s->last_gyro_us=s->last_orientation_us=0;
    s->last_accel_sequence=s->last_gyro_sequence=0;
    return true; /* require two new observations for alpha after explicit rearm */'''
assert control.count(reset)==1,'Reconstruction no longer matches source; refusing mutation'
legacy=OUT/'control_cabina_sin_reset_de_rearme.c'
legacy.write_text(control.replace(reset,'    return true; /* historical rearm: retains previous sequence/alpha */'))
gcc=shutil.which('gcc')
repro=OUT/'reconstruccion-fallo-rearme'
repro.mkdir(exist_ok=True)
executable=repro/'integration.exe'
sources=['firmware/componentes/modelo_a/modelo_a.c','firmware/componentes/modelo_a/dinamica.c',
         'firmware/componentes/telemetria/telemetria.c',str(legacy),
         'firmware/integracion/cabina/main/demo.c','firmware/integracion/cabina/main/presentacion.c',
         'pruebas/modelo-a/integration_tests.c']
command=[gcc,'-std=c11','-g3','-O0','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-pedantic',
         '-Ifirmware/componentes/modelo_a/include','-Ifirmware/componentes/telemetria/include',
         '-Ifirmware/componentes/control_cabina/include','-Ifirmware/integracion/cabina/main',
         *sources,'-lm','-o',str(executable)]
compiled=capture('compilacion-reconstruccion',command)
if compiled['returncode']!=0: raise SystemExit('Reconstruction compilation failed')
reproduced=capture('reconstruccion-fallida',[str(executable)],timeout=10)
if reproduced['returncode']==0 or reproduced['timed_out']:
    raise SystemExit('Reproduction not established: inspect evidence; not a PASS')
gdb=Path(gcc).with_name('gdb.exe')
script=OUT/'trace.gdb'
script.write_text('''set pagination off
set confirm off
break cc_arm if s->stream.sent_us == 40000
break abort
run
print s->armed
print s->last_accel_sequence
print s->last_accel_us
finish
print engine.armed
print engine.last_accel_sequence
print engine.alpha.initialized
continue
bt full
quit
''')
if gdb.exists():
    capture('traza-reconstruccion',[str(gdb),'-q','-batch','-x',str(script),str(executable)],timeout=45)
