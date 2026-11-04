"""Compile the actual S3 startup GPIO/controller guard on host; no hardware I/O.
Requires generated sdkconfigs, not only defaults. Old classic results stay in
build/cableado-peralta. Run with --profile normal during an initial build.
"""
from pathlib import Path
import argparse,json,re,subprocess,shutil
root=Path(__file__).resolve().parents[2];out=root/'pruebas/modelo-a/build/s3-final';out.mkdir(exist_ok=True)
parser=argparse.ArgumentParser();parser.add_argument('--profile',choices=['normal','virtual100','virtual50'],action='append');args=parser.parse_args()
source=(root/'firmware/integracion/cabina/main/app_main.c').read_text(encoding='utf-8-sig')
function=source[source.index('static bool wiring_valid(void) {'):source.index('static estado_sistema_t display_state')]
header=(root/'firmware/integracion/cabina/main/cableado_cabina.h').read_text()
host_macros='\n'.join(line for line in header.splitlines() if line.startswith('#define CI_'))
compiler=shutil.which('gcc');assert compiler
results=[]
expected={'CI_LCD_SPI_HOST':'2','CI_IMU_SPI_HOST':'3','CI_LCD_SCLK':'12','CI_LCD_MOSI':'11','CI_LCD_DC':'9','CI_LCD_RST':'8','CI_LCD_CS':'10','CI_IMU_MOSI':'6','CI_IMU_MISO':'7','CI_IMU_SCLK':'5','CI_IMU_CS':'4','CI_IMU_INT':'15','CI_IMU_RST':'16','CI_GPS_RX':'18','CI_GPS_TX':'17','CI_BUTTON_GPIO':'21','CI_ENABLE_CABIN_IMU':'y','CI_ENABLE_GPS':'y'}
for profile in args.profile or ['normal','virtual100','virtual50']:
 folder=root/'firmware/integracion/cabina';sdk=folder/'sdkconfig' if profile=='normal' else folder/'build/s3'/profile/'sdkconfig'
 settings=dict(re.findall(r'^(CONFIG_\w+)=(.*)$',sdk.read_text(),re.M));assert settings['CONFIG_IDF_TARGET']=='"esp32s3"'
 for key,value in expected.items():assert settings.get('CONFIG_'+key)==value,(profile,key,'run IDF reconfigure/build first')
 assert settings['CONFIG_SPIRAM_MODE_OCT']=='y' and settings['CONFIG_ESPTOOLPY_FLASHMODE_QIO']=='y' and settings['CONFIG_ESPTOOLPY_FLASHMODE']=='"dio"'
 assert settings.get('CONFIG_CI_VIRTUAL_MOUNT_CONFIRMED')!='y'
 base={k:('1' if v=='y' else v) for k,v in settings.items() if (k.startswith(('CONFIG_CI_','CONFIG_ESP_CONSOLE_UART_')) or k in ('CONFIG_IDF_TARGET_ESP32S3','CONFIG_SPIRAM')) and (v=='y' or re.fullmatch(r'-?\d+',v))}
 cases=[('valid_S3',{},True),('button_enabled',{'CONFIG_CI_ENABLE_BUTTON':'1'},True),('different_hosts_swapped',{'CONFIG_CI_LCD_SPI_HOST':'3','CONFIG_CI_IMU_SPI_HOST':'2'},True),('same_SPI_controller',{'CONFIG_CI_LCD_SPI_HOST':'2','CONFIG_CI_IMU_SPI_HOST':'2'},False),('SPI1_reserved_memory',{'CONFIG_CI_LCD_SPI_HOST':'1'},False),('nonexistent_SPI4',{'CONFIG_CI_IMU_SPI_HOST':'4'},False),('button_conflicts_MOSI',{'CONFIG_CI_BUTTON_GPIO':'6'},False),('LCD_conflicts_INT',{'CONFIG_CI_LCD_CS':'15'},False),('LCD_conflicts_MISO',{'CONFIG_CI_LCD_MOSI':'7'},False),('LCD_conflicts_SCK',{'CONFIG_CI_LCD_SCLK':'5'},False),('LCD_conflicts_RST',{'CONFIG_CI_LCD_RST':'16'},False),('wrong_console_bus',{'CONFIG_ESP_CONSOLE_UART_NUM':'2'},False),('custom_console_conflicts',{'CONFIG_ESP_CONSOLE_UART_TX_GPIO':'11','CONFIG_ESP_CONSOLE_UART_RX_GPIO':'44'},False),('wrong_target',{'CONFIG_IDF_TARGET_ESP32S3':None},False),('native_USB_console',{'CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG':'1','CONFIG_ESP_CONSOLE_UART_NUM':None,'CONFIG_ESP_CONSOLE_UART_TX_GPIO':None,'CONFIG_ESP_CONSOLE_UART_RX_GPIO':None},True),('disabled_driver_still_wired',{'CONFIG_CI_ENABLE_CABIN_IMU':None,'CONFIG_CI_ENABLE_GPS':None,'CONFIG_CI_LCD_CS':'15'},False)]
 for pin in [-1,0,3,19,20,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,43,44,45,46,48,49]:cases.append(('reserved_'+str(pin).replace('-','minus'),{'CONFIG_CI_LCD_CS':str(pin)},False))
 code=['#include <stdbool.h>\n#include <stdint.h>\n#include <stdio.h>\nenum {SPI2_HOST=1,SPI3_HOST=2};\n#define GPIO_IS_VALID_OUTPUT_GPIO(g) ((g)>=0 && (g)<=48 && (g)!=46 && !((g)>=22 && (g)<=25))\n'+host_macros+'\n']
 allkeys=set(base)|{k for _,changes,_ in cases for k in changes}
 for index,(_,changes,_) in enumerate(cases):
  values=dict(base);values.update(changes);code+=['#undef '+k+'\n' for k in sorted(allkeys)];code+=['#define '+k+' '+v+'\n' for k,v in sorted(values.items()) if v is not None];code.append(function.replace('wiring_valid','case_'+str(index),1))
 code.append('int main(void) {int errors=0;\n')
 for index,(name,_,ok) in enumerate(cases):code.append('if(case_'+str(index)+'()!='+('true' if ok else 'false')+') {puts("FAIL '+name+'");errors++;}\n')
 code.append('return errors?1:0;}\n');c=out/(profile+'-wiring.c');c.write_text(''.join(code));exe=out/(profile+'-wiring.exe')
 for label,cmd in [('compile',[compiler,'-std=c11','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)]),('run',[str(exe)])]:
  with (out/(profile+'-guard-'+label+'.stdout.txt')).open('wb') as stdout,(out/(profile+'-guard-'+label+'.stderr.txt')).open('wb') as stderr:r=subprocess.run(cmd,stdout=stdout,stderr=stderr,timeout=30)
  results.append(dict(profile=profile,step=label,returncode=r.returncode,command=cmd,cases=len(cases)));(out/('gpio-guard-results-'+profile+'.json')).write_text(json.dumps(results[-2:] if label=='run' else results[-1:],indent=2));assert r.returncode==0,(profile,label)
 print('PASS ESP32-S3',profile,len(cases),'GPIO/controller guard cases; host only')
