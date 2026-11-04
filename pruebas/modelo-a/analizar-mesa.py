"""Summarize an existing CABINA console capture; never opens a serial port.
Usage: python pruebas/modelo-a/analizar-mesa.py cabina.txt --output resumen.json
No automatic hardware/model PASS: a temporal probe is only a necessary check.
"""
import argparse
import json
import re
from pathlib import Path

def summarize(text):
    boots=[]
    def new_boot():
        b={'disarms':[], 'recoveries':[], 'last_health':{}, 'last_transport':{},
           'last_probe':{}, 'reports':{}, 'warnings':[]}
        boots.append(b)
        return b
    boot=new_boot();last_evaluation=None
    for line in text.splitlines():
        match=re.search(r'\b(DIAG_PRINT|BENCH|REAR_REPORT|MODEL_EVENT|MODEL_HEALTH|TIMING_PROBE)\|',line)
        if not match: continue
        kind=match.group(1)
        fields=dict(x.split('=',1) for x in line[match.end():].split('|') if '=' in x)
        if kind=='DIAG_PRINT':
            evaluation=int(fields['evaluation_local_us'])
            if last_evaluation is not None and evaluation<last_evaluation: boot=new_boot()
            last_evaluation=evaluation
        if kind=='BENCH': boot['last_transport']=fields
        if kind=='REAR_REPORT': boot['reports'][fields['kind']]=fields
        if kind=='MODEL_HEALTH':
            boot['last_health']=fields
            if int(fields.get('event_drops',0)) and 'event_queue_overflow' not in boot['warnings']:
                boot['warnings'].append('event_queue_overflow')
        if kind=='TIMING_PROBE': boot['last_probe']=fields
        if kind=='MODEL_EVENT': boot['recoveries' if fields['kind']=='RECOVER' else 'disarms'].append(fields)
    for boot in boots:
        health=boot['last_health'];events=boot['disarms']
        boot['disarm_counter']=max([int(health.get('disarms',0))]+[int(e['disarm_id']) for e in events])
        if boot['disarm_counter']>len(events): boot['warnings'].append('capture_missing_disarm_events_or_started_late')
        if not boot['last_transport']: boot['warnings'].append('no_cst2_diagnostic_snapshot')
    return {'scope':'DIAGNOSTICS_NOT_PHYSICAL_VALIDATION','cabin_boots':boots}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture',type=Path);parser.add_argument('--output',type=Path)
    args=parser.parse_args();result=summarize(args.capture.read_text(encoding='utf-8',errors='replace'))
    rendered=json.dumps(result,ensure_ascii=False,indent=2)
    if args.output: args.output.write_text(rendered+'\n',encoding='utf-8')
    print(rendered)
