"""Reproducible, read-only git exports into the new integration directory.
Never overwrites a destination. No checkout/merge/submodule mutation.
"""
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEST = ROOT / 'firmware/integracion'
manifest = []

def export(ref, prefix, destination, allowed=None):
    commit = subprocess.check_output(['git', 'rev-parse', ref], cwd=ROOT).decode().strip()
    paths = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', ref, prefix], cwd=ROOT).decode().splitlines()
    for path in paths:
        relative = path[len(prefix):].lstrip('/')
        if allowed and not allowed(relative):
            continue
        target = DEST / destination / relative
        if target.exists():
            raise RuntimeError(f'Destination exists: {target}')
        data = subprocess.check_output(['git', 'show', f'{commit}:{path}'], cwd=ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        manifest.append({'ref': ref, 'commit': commit, 'source': path,
                         'destination': str(target.relative_to(ROOT)).replace('\\', '/')})

if __name__ == '__main__':
    export('origin/TACOS', 'firmware/esp32-master/PRUEBA MASTER/main/modules', 'cabina/main/modules')
    export('origin/TACOS', 'firmware/Apoyo/field_espnow', 'components/field_espnow')
    export('origin/TACOS', 'firmware/Apoyo/imu_bno085', 'components/imu_bno085')
    export('origin/Pruebas-Gael', 'firmware/esp32-trasero/PRUEBA SLAVE/components/esp32_BNO08x',
           'components/esp32_BNO08x', lambda p: p.startswith(('source/', 'include/', 'SH2/', 'etl/')) or p in
           ('CMakeLists.txt', 'Kconfig.projbuild', 'LICENSE', 'README.md'))
    (DEST / 'procedencia.json').write_text(json.dumps(manifest, indent=2), encoding='utf8')
