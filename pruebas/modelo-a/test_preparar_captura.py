"""Profile generation from versioned inputs only; no SDK or hardware needed."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]


class CleanProfiles(unittest.TestCase):
    def test_fresh_checkout_and_preserved_mount(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            script = root / 'pruebas/modelo-a/preparar-captura.py'
            script.parent.mkdir(parents=True)
            shutil.copy2(ROOT / 'pruebas/modelo-a/preparar-captura.py', script)
            for project in ('cabina', 'trasero'):
                folder = root / 'firmware/integracion' / project
                folder.mkdir(parents=True)
                shutil.copy2(ROOT / 'firmware/integracion' / project / 'sdkconfig.defaults', folder)
                self.assertFalse((folder / 'sdkconfig').exists())
            for rate in (100, 50):
                subprocess.run([sys.executable, str(script), '--virtual', '--rate', str(rate)], check=True,
                               capture_output=True)
                for project, profile in (('cabina', f'virtual{rate}'), ('trasero', f'capture{rate}')):
                    config = root / 'firmware/integracion' / project / 'build/s3' / profile / 'sdkconfig'
                    text = config.read_text()
                    self.assertIn('CONFIG_IDF_TARGET="esp32s3"', text)
                    self.assertIn('CONFIG_SPIRAM_MODE_OCT=y', text)
                    self.assertIn('CONFIG_CI_FULL_CAPTURE=y', text)
                    self.assertIn('CONFIG_ESP_CONSOLE_UART_BAUDRATE=921600', text)
                    self.assertIn('CONFIG_CI_BENCH_50HZ=y' if rate == 50 else '# CONFIG_CI_BENCH_50HZ is not set', text)
                    if project == 'cabina':
                        self.assertIn('CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192', text)
                        self.assertIn('CONFIG_CI_PIPA_VIRTUAL=y', text)
                        self.assertIn('# CONFIG_CI_VIRTUAL_MOUNT_CONFIRMED is not set', text)
                        self.assertIn('# CONFIG_CI_VIRTUAL_LEVER is not set', text)
                    else:
                        self.assertIn('# CONFIG_CI_REAR_WATER_ENABLED is not set', text)
            config = root / 'firmware/integracion/cabina/build/s3/virtual100/sdkconfig'
            custom = config.read_text().replace('# CONFIG_CI_VIRTUAL_MOUNT_CONFIRMED is not set',
                                              'CONFIG_CI_VIRTUAL_MOUNT_CONFIRMED=y')
            config.write_text(custom)
            subprocess.run([sys.executable, str(script), '--virtual', '--rate', '100'], check=True,
                           capture_output=True)
            self.assertEqual(config.read_text(), custom)


if __name__ == '__main__':
    unittest.main()
