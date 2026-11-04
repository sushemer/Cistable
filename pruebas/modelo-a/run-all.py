"""Host suite. Run from any directory with Python + GCC, no PowerShell policy change."""
from pathlib import Path
import ctypes
import hashlib
import json
import os
import shutil
import subprocess
root=Path(__file__).resolve().parents[2]
build=root/'pruebas/modelo-a/build'
build.mkdir(exist_ok=True)
logs=build/'logs'
logs.mkdir(exist_ok=True)
if os.name=='nt':
    # Only this runner/children; no registry/global setting. Assertions still
    # abort with a nonzero result instead of waiting on an OS error dialog.
    ctypes.windll.kernel32.SetErrorMode(0x8003)
results=[]
summary=logs/'results.json'
summary.write_text(json.dumps({'status':'RUNNING','steps':results},indent=2))
def run_logged(label,command):
    stdout=logs/(label+'.stdout.txt');stderr=logs/(label+'.stderr.txt')
    with stdout.open('wb') as out,stderr.open('wb') as err:
        child=subprocess.Popen(command,cwd=root,stdout=out,stderr=err)
        timed_out=False
        try: code=child.wait(timeout=30)
        except subprocess.TimeoutExpired:
            timed_out=True;child.kill();code=child.wait()
    result={'step':label,'command':command,'cwd':str(root),'returncode':code,
            'returncode_hex':f'0x{code & 0xffffffff:08X}','timed_out':timed_out,
            'stdout':str(stdout),'stderr':str(stderr)}
    if label.startswith('run-'):
        result['executable_sha256']=hashlib.sha256(Path(command[0]).read_bytes()).hexdigest()
    results.append(result)
    failed=code!=0 or timed_out
    summary.write_text(json.dumps({'status':'FAIL' if failed else 'RUNNING','steps':results},indent=2))
    print(stdout.read_text(errors='replace'),end='')
    if stderr.stat().st_size: print(stderr.read_text(errors='replace'),end='')
    if failed: raise SystemExit(f'FAIL {label}: exit={code}; evidence={summary}')
compiler=shutil.which('gcc')
if not compiler: raise SystemExit('FAIL: GCC not found')
common=[compiler,'-std=c11','-g3','-O0','-fno-omit-frame-pointer','-UNDEBUG','-Wall','-Wextra','-Werror','-pedantic',
        '-Ifirmware/componentes/modelo_a/include']
math=['firmware/componentes/modelo_a/modelo_a.c','firmware/componentes/modelo_a/dinamica.c']
run_logged('compile-geometry',common+math+['pruebas/modelo-a/host_tests.c','-lm','-o',str(build/'geometry.exe')])
run_logged('run-geometry',[str(build/'geometry.exe')])
integration=['-Ifirmware/componentes/telemetria/include','-Ifirmware/componentes/control_cabina/include',
             '-Ifirmware/integracion/cabina/main','firmware/componentes/telemetria/telemetria.c',
             'firmware/componentes/control_cabina/control_cabina.c','firmware/integracion/cabina/main/demo.c',
             'firmware/integracion/cabina/main/presentacion.c','pruebas/modelo-a/integration_tests.c']
run_logged('compile-integration',common+math+integration+['-lm','-o',str(build/'integration.exe')])
run_logged('run-integration',[str(build/'integration.exe')])
continuity=[source for source in integration if source!='pruebas/modelo-a/integration_tests.c']
run_logged('compile-continuity',common+math+continuity+['pruebas/modelo-a/continuity_tests.c','-lm','-o',str(build/'continuity.exe')])
run_logged('run-continuity',[str(build/'continuity.exe')])
run_logged('compile-virtual',common+math+continuity+['firmware/integracion/cabina/main/pipa_virtual.c','pruebas/modelo-a/virtual_tests.c','-lm','-o',str(build/'virtual.exe')])
run_logged('run-virtual',[str(build/'virtual.exe')])
run_logged('compile-bench',common+math+continuity+['firmware/integracion/cabina/main/diagnostico.c',
    'pruebas/modelo-a/bench_tests.c','-lm','-o',str(build/'bench.exe')])
run_logged('run-bench',[str(build/'bench.exe')])
import sys
run_logged('test-capture-analyzer',[sys.executable,str(root/'pruebas/modelo-a/test_analizar_mesa.py')])
run_logged('compile-scenarios',common+math+['pruebas/modelo-a/parameter_scenarios.c','-lm','-o',str(build/'scenarios.exe')])
run_logged('run-scenarios',[str(build/'scenarios.exe')])
run_logged('compile-capture',[compiler,'-std=c11','-Wall','-Wextra','-Werror','-pedantic',
    '-Ifirmware/integracion/components/captura/include','firmware/integracion/components/captura/captura.c',
    'pruebas/modelo-a/capture_codec_tests.c','-o',str(build/'capture-codec.exe')])
run_logged('run-capture',[str(build/'capture-codec.exe')])
run_logged('test-full-capture',[sys.executable,str(root/'pruebas/modelo-a/test_captura_completa.py')])
summary.write_text(json.dumps({'status':'PASS','steps':results},indent=2))
print(f'HOST SUITES PASS: geometry/integration/continuity/bench, scenarios, capture codecs/analyzers exited 0; evidence={summary}')
