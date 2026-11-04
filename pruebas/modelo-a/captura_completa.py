"""Binary application capture. record opens only the explicitly selected serial
port; decode works offline. No flashing or automatic model authorization.
"""
import argparse
import csv
import datetime
import hashlib
import json
import math
from pathlib import Path
import struct
import threading
import time
import zlib

MAGIC=b'CSTCAP01'
KINDS={1:'RX',2:'PROCESS',3:'TX',4:'TX_DONE',5:'STATS',6:'BOOT',7:'COMMAND',8:'CONFIG'}
DECISIONS={0:'rejected_or_waiting',1:'initial_anchor',2:'integrated',3:'duplicate_packet',
           4:'invalid_wire',5:'query_state_change',6:'explicit_arm'}

def frames(data):
    records=[];text=bytearray();pos=0;errors=0;tail=0
    while pos<len(data):
        start=data.find(MAGIC,pos)
        if start<0: text.extend(data[pos:]);break
        text.extend(data[pos:start])
        if len(data)-start<28: tail=len(data)-start;break
        kind,flags,n,ident,local=struct.unpack_from('<BBHIQ',data,start+8)
        if n>272 or kind not in KINDS: errors+=1;pos=start+1;continue
        end=start+28+n
        if end>len(data): tail=len(data)-start;break
        expected=struct.unpack_from('<I',data,end-4)[0]
        if zlib.crc32(data[start:end-4])!=expected: errors+=1;pos=start+1;continue
        records.append({'kind':kind,'flags':flags,'id':ident,'local_us':local,'payload':data[start+24:end-4]})
        pos=end
    return records,bytes(text),errors,tail

def cst2(wire):
    if len(wire)!=124 or wire[:4]!=b'CST2' or wire[4]!=2: return None
    names=['sent_us','accel_us','gyro_us','orientation_us','water_us']
    out=dict(zip(names,struct.unpack_from('<5Q',wire,16)))
    out.update(session=struct.unpack_from('<I',wire,8)[0],tx_seq=struct.unpack_from('<I',wire,12)[0])
    out.update(zip(['accel_seq','gyro_seq','orientation_seq'],struct.unpack_from('<3I',wire,56)))
    out.update(zip(['ax_m_s2','ay_m_s2','az_m_s2','wx_rad_s','wy_rad_s','wz_rad_s','qw','qx','qy','qz','water_value'],struct.unpack_from('<11f',wire,68)))
    out.update(water_register=struct.unpack_from('<H',wire,112)[0],water_unit_enum=wire[114],presence_flags=wire[115],
               accel_quality=wire[116],gyro_quality=wire[117],orientation_quality=wire[118],water_seq=struct.unpack_from('<I',wire,120)[0])
    flags=out['presence_flags']
    valid=bool(out['session']) and flags<=15 and out['water_unit_enum']<=2 and wire[5]==0 and wire[119]==0 and struct.unpack_from('<H',wire,6)[0]==124
    for k,name in enumerate(['accel','gyro','orientation','water']):
        if flags&(1<<k): valid=valid and 0<out[name+'_us']<=out['sent_us']
    if flags&1: valid=valid and all(math.isfinite(out[n]) for n in ('ax_m_s2','ay_m_s2','az_m_s2'))
    if flags&2: valid=valid and all(math.isfinite(out[n]) for n in ('wx_rad_s','wy_rad_s','wz_rad_s'))
    if flags&4:
        norm=sum(out[n]**2 for n in ('qw','qx','qy','qz'));valid=valid and math.isfinite(norm) and abs(norm-1)<=1e-4
    if flags&8: valid=valid and math.isfinite(out['water_value']) and out['water_value']>=0
    out['wire_syntax_valid']=bool(valid) # not freshness, calibration or physical validation
    return out

def process(p):
    if len(p)<120 or p[118]>96 or len(p)!=120+p[118]: raise ValueError('PROCESS length')
    names=['rx_id','model_session','model_tx_seq','rx_local_us','previous_source_us','current_source_us','gyro_source_us','expires_local_us','disarms']
    values=struct.unpack_from('<QII6Q',p)
    out=dict(zip(names,values))
    out.update(zip(['theta_rad','theta_rate_rad_s','ltr_signed','alpha_x_rad_s2','alpha_y_rad_s2','alpha_z_rad_s2'],struct.unpack_from('<6d',p,64)))
    out.update(status=p[112],output_status=p[113],mode=p[114],armed=p[115],decision=p[116],conditions=p[117],
               cause=p[120:].decode('utf-8',errors='replace'),cause_truncated=bool(p[119]&1),cause_is_disarm=bool(p[119]&2))
    out['disposition']=DECISIONS.get(p[116],'unknown')
    out['dt_source_us']=out['current_source_us']-out['previous_source_us'] if p[116]==2 else None
    return out

def sample_stats(rows,kind):
    # Separate source sessions. A packet repeated at radio/application level is
    # not another sensor observation. Never subtract clocks of different nodes.
    sessions={}
    for row in rows:
        if row['record_kind']!=kind or (kind=='TX' and row.get('api_result')!=0): continue
        s=sessions.setdefault(str(row['session']),{'packets':0,'first_local':row['event_local_us'],
            'last_local':row['event_local_us'],'reports':{},'max_partial_age_us':0,'max_skew_us':0})
        s['packets']+=1;s['last_local']=row['event_local_us']
        if not row.get('wire_syntax_valid',False):
            s['invalid_wire']=s.get('invalid_wire',0)+1;continue
        active=[]
        for k,name in enumerate(['accel','gyro','orientation','water']):
            if not row['presence_flags']&(1<<k): continue
            stamp=row[name+'_us'];seq=row[name+'_seq'];active.append(stamp) if k<3 else None
            age=row['sent_us']-stamp
            if kind=='RX': age+=row.get('processing_residence_us',0)
            s['max_partial_age_us']=max(s['max_partial_age_us'],age)
            r=s['reports'].setdefault(name,{'observed_new':0,'repeats':0,'gaps':0,'reverse':0,
                'first_source_us':stamp,'last_source_us':stamp,'last_seq':None})
            if r['last_seq'] is None: r['observed_new']+=1
            else:
                delta=(seq-r['last_seq'])&0xffffffff
                if delta==0: r['repeats']+=1
                elif delta>=0x80000000 or stamp<=r['last_source_us']: r['reverse']+=1
                else: r['observed_new']+=1;r['gaps']+=delta-1
            r['last_seq']=seq;r['last_source_us']=stamp
        if len(active)==3: s['max_skew_us']=max(s['max_skew_us'],max(active)-min(active))
    for s in sessions.values():
        duration=(s['last_local']-s['first_local'])/1e6
        s['packet_rate_hz']=(s['packets']-1)/duration if duration>0 else None
        for r in s['reports'].values():
            seconds=(r['last_source_us']-r['first_source_us'])/1e6
            r['observed_rate_hz']=(r['observed_new']-1)/seconds if seconds>0 else None
            r['driver_counter_rate_hz']=(r['observed_new']-1+r['gaps'])/seconds if seconds>0 else None
    return sessions

def analyze(records):
    measurements=[];processes=[];boots=[];configuration=[];capture_ids=[];warnings=[];max_dropped=0;last_time=0;radio={};encoder_rejections=0
    for r in records:
        capture_ids.append(r['id']);last_time=max(last_time,r['local_us']);p=r['payload'];kind=r['kind']
        if kind==6: boots.append(p.decode('utf-8',errors='replace'))
        if kind==8: configuration.append(p.decode('utf-8',errors='replace'))
        if kind==5 and len(p)>=40:
            max_dropped=max(max_dropped,struct.unpack_from('<Q',p,16)[0])
            if len(p)==112:
                radio=dict(zip(['rx_callbacks','rx_delivered','rx_skipped','rx_rejected_input','tx_submitted',
                    'tx_completed','tx_failed','tx_busy','sending'],struct.unpack_from('<9Q',p,40)))
        if kind==2: processes.append(dict(process(p),event_local_us=r['local_us'],capture_id=r['id']))
        if kind in (1,3,4):
            prefix=16 if kind==1 else 4
            if len(p)<prefix: warnings.append('short_raw_record');continue
            f=cst2(p[prefix:])
            if kind==3 and len(p)==4: encoder_rejections+=1
            if f is None: continue # exact wire still exists in .raw and records.csv
            f.update(record_kind=KINDS[kind],event_local_us=r['local_us'],capture_id=r['id'],wire_hex=p[prefix:].hex())
            if kind==1: f.update(rx_id=struct.unpack_from('<Q',p)[0],rssi_dbm=struct.unpack_from('<b',p,14)[0],rssi_valid=p[15])
            else: f['api_result']=struct.unpack_from('<i',p)[0]
            measurements.append(f)
    proc_by_rx={p['rx_id']:p for p in processes if p['rx_id']}
    max_processed=max(proc_by_rx,default=0)
    ids=set(capture_ids);gaps=max(ids)-min(ids)+1-len(ids) if ids else 0
    duplicate_ids=len(ids)!=len(capture_ids)
    for row in measurements:
        if row['record_kind']!='RX': continue
        p=proc_by_rx.get(row['rx_id'])
        row['disposition']=p['disposition'] if p else ('unknown_due_to_capture_loss' if gaps or max_dropped or duplicate_ids else
            'not_processed_latest_slot' if row['rx_id']<max_processed else 'unprocessed_tail')
        if p:
            row.update(processing_residence_us=p['event_local_us']-row['event_local_us'],model_cause=p['cause'],
                decision=p['decision'],dt_source_us=p['dt_source_us'],disarms=p['disarms'])
            times=[row[name+'_us'] for name in ('accel','gyro','orientation')]
            row['pair_skew_us']=max(times)-min(times)
            row['gyro_accel_separation_us']=abs(row['gyro_us']-row['accel_us'])
            row['partial_age_us_at_process']=row['sent_us']-min(times)+row['processing_residence_us']
            p['pair_skew_us']=row['pair_skew_us'];p['gyro_accel_separation_us']=row['gyro_accel_separation_us']
            p['origin_session']=row['session'];p['origin_tx_seq']=row['tx_seq']
    used=[p for p in processes if p['decision']==2];reasons={};disarms=[];missing=[]
    last_disarms=0;was_available=False;missing_start=min(r['local_us'] for r in records) if processes else None
    for p in processes:
        reasons[p['cause']]=reasons.get(p['cause'],0)+1
        if p['disarms']>last_disarms:
            disarms.append({'local_us':p['event_local_us'],'count':p['disarms'],'cause':p['cause'],'operator_required':True})
        last_disarms=p['disarms']
        available=p['armed'] and p['status'] in (1,4,7) and p['current_source_us'] and p['expires_local_us']>=p['event_local_us']
        if available and not was_available:
            missing.append({'start_local_us':missing_start,'end_local_us':p['event_local_us'],
                'duration_us':p['event_local_us']-missing_start,'operator_required':p['disarms']>0});missing_start=None
        if not available and was_available: missing_start=p['event_local_us']
        was_available=bool(available)
    if missing_start is not None:
        missing.append({'start_local_us':missing_start,'end_local_us':None,'duration_us':last_time-missing_start,'operator_required':last_disarms>0})
    if not boots: warnings.append('capture_started_without_BOOT_metadata')
    if not measurements: warnings.append('no_CST2_measurements_observed')
    if len(boots)>1: warnings.append('multiple_boots_split_capture_before_rate_analysis')
    if gaps or max_dropped: warnings.append('capture_loss_not_complete')
    if duplicate_ids: warnings.append('duplicate_capture_ids_possible_missing_boot')
    if radio.get('rx_rejected_input'): warnings.append('radio_callback_rejected_unsupported_input_not_saved_in_full')
    dt=[p['dt_source_us'] for p in used if p['dt_source_us'] is not None]
    duration=(used[-1]['event_local_us']-used[0]['event_local_us'])/1e6 if len(used)>1 else 0
    summary={'scope':'APPLICATION_INPUTS_AND_DECISIONS_NOT_EVERY_INTERNAL_MEMS_SAMPLE','boots':boots,'configuration_records':configuration,
        'capture_records':len(records),'capture_id_gaps':gaps,'capture_dropped_max':max_dropped,'warnings':warnings,
        'last_radio_counters':radio,
        'source_encoder_rejections':encoder_rejections,
        'received':sample_stats(measurements,'RX'),'submitted':sample_stats(measurements,'TX'),
        'committed_steps':len(used),'committed_rate_hz':(len(used)-1)/duration if duration>0 else None,
        'dt_source_us_min':min(dt) if dt else None,'dt_source_us_max':max(dt) if dt else None,
        'admitted_skew_us_max':max((p.get('pair_skew_us',0) for p in used),default=None),
        'admitted_gyro_accel_separation_us_max':max((p.get('gyro_accel_separation_us',0) for p in used),default=None),
        'disarm_events':disarms,'missing_intervals':missing,'model_reason_counts':reasons,
        'historical_water_real':False,'historical_gps_real':False,'mounting_check_pending':True}
    return summary,measurements,processes

def write_csv(path,rows):
    keys=list(dict.fromkeys(k for row in rows for k in row))
    with path.open('w',newline='',encoding='utf-8') as file:
        writer=csv.DictWriter(file,fieldnames=keys);writer.writeheader();writer.writerows(rows)

def decode(args):
    raw=args.input.read_bytes();records,console,errors,tail=frames(raw)
    groups=[];current=[];seen_boot=False
    for r in records:
        if r['kind']==6 and seen_boot:
            groups.append(current);current=[];seen_boot=False
        current.append(r)
        if r['kind']==6: seen_boot=True
    if current: groups.append(current)
    segments=[];measurements=[];processes=[]
    for index,group in enumerate(groups):
        part,m,p=analyze(group);segments.append(part)
        measurements.extend(dict(row,boot_segment=index) for row in m)
        processes.extend(dict(row,boot_segment=index) for row in p)
    summary=segments[0] if len(segments)==1 else {'segments':segments,'warnings':sorted(set(w for s in segments for w in s['warnings']))}
    if not records: summary['warnings'].append('no_capture_records')
    summary.update(crc_or_header_errors=errors,truncated_tail_bytes=tail,raw_sha256=hashlib.sha256(raw).hexdigest())
    if errors or tail: summary['warnings'].append('corruption_or_incomplete_tail')
    args.output.mkdir(parents=True,exist_ok=False)
    (args.output/'summary.json').write_text(json.dumps(summary,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    (args.output/'console.txt').write_bytes(console)
    (args.output/'configuration.json').write_text(json.dumps([
        dict(boot_segment=i,records=s.get('configuration_records',[])) for i,s in enumerate(segments)],indent=2)+'\n',encoding='utf-8')
    write_csv(args.output/'measurements.csv',measurements);write_csv(args.output/'process.csv',processes)
    write_csv(args.output/'records.csv',[dict(kind=KINDS[r['kind']],id=r['id'],event_local_us=r['local_us'],payload_hex=r['payload'].hex()) for r in records])
    print(json.dumps({'records':len(records),'warnings':summary['warnings'],'output':str(args.output)},indent=2))

def record(args):
    import serial # available in the ESP-IDF Python environment; not needed offline
    if args.transport=='uart' and args.baud<921600: raise SystemExit('Full UART capture requires >=921600 baud in this profile')
    if args.seconds<=0: raise SystemExit('seconds must be positive')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    metadata={'started_pc_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'port':args.port,'baud':args.baud,'transport':args.transport,
        'baud_is_link_rate':args.transport=='uart','pc_time_is_not_acquisition':True,'configs':{}}
    for source in args.config:
        data=source.read_bytes();target=Path(str(args.output)+'.'+source.parent.name+'.'+source.name)
        with target.open('xb') as file: file.write(data)
        metadata['configs'][str(source)]={'sha256':hashlib.sha256(data).hexdigest(),'copy':str(target)}
    stop=threading.Event();link=serial.Serial(port=None,baudrate=args.baud,timeout=0.1)
    link.dtr=False;link.rts=False;link.port=args.port
    started=time.monotonic();count=0;digest=hashlib.sha256()
    with args.output.open('xb') as file:
        link.open()
        metadata['rx_buffer_requested_bytes']=262144
        try:
            if hasattr(link,'set_buffer_size'):
                link.set_buffer_size(rx_size=262144,tx_size=4096)
                metadata['rx_buffer_request_succeeded']=True
            else: metadata['rx_buffer_request_succeeded']=False
        except (OSError,ValueError) as error:
            metadata['rx_buffer_request_succeeded']=False;metadata['rx_buffer_error']=str(error)
        if args.interactive:
            def commands():
                while not stop.is_set():
                    try: command=input().strip()
                    except EOFError: return
                    if stop.is_set(): return
                    if command=='q': stop.set();return
                    if command in ('f','s','a','u'):
                        try: link.write(command.encode('ascii'))
                        except (serial.SerialException,OSError) as error:
                            metadata['command_error']=str(error);stop.set();return
            threading.Thread(target=commands,daemon=True).start()
        try:
            while not stop.is_set() and time.monotonic()-started<args.seconds:
                data=link.read(4096)
                if data: file.write(data);count+=len(data);digest.update(data)
        except KeyboardInterrupt: metadata['stopped_by_operator']=True
        except (serial.SerialException,OSError) as error: metadata['record_error']=str(error)
        finally: stop.set();link.close()
    metadata.update(bytes=count,seconds=time.monotonic()-started,sha256=digest.hexdigest())
    with Path(str(args.output)+'.json').open('x',encoding='utf-8') as file: json.dump(metadata,file,indent=2)
    print(json.dumps(metadata,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    p=sub.add_parser('record');p.add_argument('--port',required=True);p.add_argument('--baud',type=int,default=921600)
    p.add_argument('--transport',choices=['uart','usb'],default='uart',help='usb: virtual baud is not native USB throughput')
    p.add_argument('--seconds',type=float,default=180);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--config',type=Path,action='append',default=[]);p.add_argument('--interactive',action='store_true')
    p=sub.add_parser('decode');p.add_argument('input',type=Path);p.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();record(args) if args.command=='record' else decode(args)
