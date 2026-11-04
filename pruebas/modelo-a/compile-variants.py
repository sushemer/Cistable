"""Compile conditional app paths with the actual ESP-IDF build flags.
Does not replace objects used by the normal firmware or claim hardware execution.
Run after building firmware/integracion/cabina with ESP-IDF.
"""
from pathlib import Path
import json,re,subprocess,argparse
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument('--build',type=Path,default=root/'firmware/integracion/cabina/build/s3/normal');args=parser.parse_args()
build=args.build
entries=json.loads((build/'compile_commands.json').read_text())
variants=[('configuracion_modelo.c','-DCONFIG_CI_DEMO=1','demo_configuration'),
          ('app_main.c','-DCONFIG_CI_DEMO=1','demo'),
          ('app_main.c','-DCONFIG_CI_DEMO=1 -DCONFIG_CI_DEMO_EXPIRY=1','demo_expiry'),
          ('app_main.c','-DCONFIG_CI_ENABLE_CABIN_IMU=1 -DCONFIG_CI_ENABLE_GPS=1 -DCONFIG_CI_ENABLE_BUTTON=1','peripherals'),
          ('seleccion.c','-DCONFIG_CI_ENABLE_BUTTON=1','button'),
          ('app_main.c','-DCONFIG_CI_BENCH_50HZ=1','profile50_control'),
          ('modules/imu/imu_module.cpp','-DCONFIG_CI_BENCH_50HZ=1','profile50_cabin_imu')]
for filename,defines,name in variants:
    entry=next(x for x in entries if x['file'].replace('\\','/').endswith('/main/'+filename))
    command=re.sub(r' -o [^ ]+ ',f' {defines} -o {name}_compile_check.obj ',entry['command'],count=1)
    # Compile database is produced locally by CMake; invoke compiler directly, no shell.
    subprocess.run(command,cwd=entry['directory'],check=True)
    print('PASS ESP32-S3 compile variant:',name)
