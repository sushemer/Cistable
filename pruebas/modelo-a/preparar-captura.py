"""Prepare current ESP32-S3/N16R8 configurations in build/s3/, preserving old
ESP32 and earlier rear evidence. Defaults: UART0 console (43/44). Use --console
usb only after identifying native USB19/20 on the actual development board.
"""
import argparse,re
from pathlib import Path
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rate',type=int,choices=[50,100],default=100)
parser.add_argument('--virtual',action='store_true')
parser.add_argument('--normal',action='store_true',help='base real mode; fresh S3 build, root sdkconfig')
parser.add_argument('--console',choices=['uart','usb'],default='uart')
parser.add_argument('--refresh-generated',action='store_true',help='backup then replace only selected S3 config')
args=parser.parse_args()
# Peripheral defaults are owned by Kconfig, not copied from the obsolete map.
hardware={'CONFIG_CI_'+k for k in ['LCD_SPI_HOST','IMU_SPI_HOST','LCD_SCLK','LCD_MOSI','LCD_DC','LCD_RST','LCD_CS','IMU_MOSI','IMU_MISO','IMU_SCLK','IMU_CS','IMU_INT','IMU_RST','GPS_RX','GPS_TX','BUTTON_GPIO']}
def read_settings(p):
 return dict(re.findall(r'^(CONFIG_\w+)=(.*)$',p.read_text(encoding='utf-8-sig'),re.M))
def write_settings(p,values):
 p.parent.mkdir(parents=True,exist_ok=True)
 text='\n'.join(f'{k}={v}' if v is not None else f'# {k} is not set' for k,v in values.items())+'\n'
 p.write_text(text,encoding='utf-8')
for project in ['cabina','trasero']:
 folder=root/'firmware/integracion'/project
 name='normal' if args.normal else (f'virtual{args.rate}' if args.virtual and project=='cabina' else f'capture{args.rate}')
 if args.console=='usb':name+='-usb'
 build=folder/'build/s3'/name
 # Keep the normal root sdkconfig effective and backed up; never reuse old build root.
 target=folder/'sdkconfig' if args.normal and args.console=='uart' else build/'sdkconfig'
 if target.exists() and read_settings(target).get('CONFIG_IDF_TARGET')=='"esp32s3"' and (not args.normal or 'CONFIG_SPIRAM_MODE_OCT=y' in target.read_text()) and not args.refresh_generated:
  if 'CONFIG_SPIRAM_MODE_OCT=y' not in target.read_text(): raise SystemExit(f'Existing S3 config is not N16R8 OPI: {target}')
  print('Preserving existing S3 config:',target)
 else:
  # First migration: recover only application parameters from matching old profile.
  old=folder/'sdkconfig' if args.normal else folder/'build'/name.replace('-usb','')/'sdkconfig'
  if target.exists() and read_settings(target).get('CONFIG_IDF_TARGET')=='"esp32s3"':old=target
  if not old.exists():old=folder/'sdkconfig'
  settings=read_settings(old) if old.exists() else read_settings(folder/'sdkconfig.defaults')
  values={k:v for k,v in settings.items() if k.startswith('CONFIG_CI_') and k not in hardware}
  values.update(read_settings(folder/'sdkconfig.defaults'))
  values.update(CONFIG_IDF_TARGET='"esp32s3"',CONFIG_ESPTOOLPY_FLASHSIZE_16MB='y',CONFIG_ESPTOOLPY_FLASHMODE_QIO='y',CONFIG_ESPTOOLPY_FLASHFREQ_80M='y',CONFIG_SPIRAM='y',CONFIG_SPIRAM_MODE_OCT='y',CONFIG_SPIRAM_SPEED_80M='y',CONFIG_SPIRAM_BOOT_INIT='y',CONFIG_SPIRAM_MEMTEST='y',CONFIG_SPIRAM_USE_CAPS_ALLOC='y',CONFIG_SPIRAM_IGNORE_NOTFOUND=None)
  values.update(CONFIG_CI_FULL_CAPTURE=None if args.normal else 'y',CONFIG_CI_BENCH_50HZ='y' if args.rate==50 and not args.normal else None)
  # Explicitly select one output, preventing duplicate/mixed console transports.
  values.update(CONFIG_ESP_CONSOLE_SECONDARY_NONE='y',CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=None)
  if args.console=='usb':
   values.update(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG='y',CONFIG_ESP_CONSOLE_UART_DEFAULT=None,CONFIG_ESP_CONSOLE_UART_CUSTOM=None)
  else:
   values.update(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=None,CONFIG_ESP_CONSOLE_UART_DEFAULT='y' if args.normal else None,CONFIG_ESP_CONSOLE_UART_CUSTOM=None if args.normal else 'y',CONFIG_ESP_CONSOLE_UART_CUSTOM_NUM_0=None if args.normal else 'y',CONFIG_ESP_CONSOLE_UART_NUM='0',CONFIG_ESP_CONSOLE_UART_BAUDRATE='115200' if args.normal else '921600')
   if not args.normal:values.update(CONFIG_ESP_CONSOLE_UART_TX_GPIO='43',CONFIG_ESP_CONSOLE_UART_RX_GPIO='44')
  if project=='cabina':
   values.update(CONFIG_CI_MODE_REAL=None if args.virtual and not args.normal else 'y',CONFIG_CI_PIPA_VIRTUAL='y' if args.virtual and not args.normal else None,CONFIG_CI_DEMO=None,CONFIG_CI_ENABLE_CABIN_IMU='y',CONFIG_CI_ENABLE_GPS='y',CONFIG_CI_ENABLE_BUTTON=None,CONFIG_CI_VIRTUAL_MOUNT_CONFIRMED=None,CONFIG_CI_REAL_MOUNT_CONFIRMED=None,CONFIG_CI_VIRTUAL_LEVER=None,CONFIG_CI_VIRTUAL_FILL_PERCENT='50')
  else:values['CONFIG_CI_REAR_WATER_ENABLED']='y' if args.normal else None
  if target.exists():
   backup=root/'pruebas/modelo-a/build/s3-final/config-backups'/project/(name+'.sdkconfig')
   backup.parent.mkdir(parents=True,exist_ok=True)
   if not backup.exists():backup.write_bytes(target.read_bytes())
  write_settings(target,values)
  print('Prepared S3 config:',target)
 build.mkdir(parents=True,exist_ok=True)
 print(f'{folder}\n  idf.py -B build/s3/{name} -D SDKCONFIG={target.as_posix()} build')
