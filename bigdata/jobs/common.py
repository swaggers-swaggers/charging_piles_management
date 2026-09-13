"""Shared Spark runtime, auditable writes, and exact source contracts."""
import argparse
import hashlib
import datetime as dt
import json
import os
import subprocess
import uuid
from pathlib import Path
import yaml
from pyspark.sql import SparkSession, functions as F, types as T

REPO = Path(__file__).resolve().parents[2]
SPECS = {
 'station': [('stationId','station_id','int'),('locationId','district','string'),('facilityType','facility_type','int'),('station_name','station_name','string'),('address','address','string'),('device_count','device_count','int'),('open_time','open_time','string'),('update_time','update_time','date')],
 'weather': [('date','dt','date'),('weekday','weekday','int'),('holiday','holiday','int'),('temp_high','temp_high','double'),('temp_low','temp_low','double'),('condition','condition','int'),('precipitation','precipitation','double')],
 'session': [('sessionId','session_id','long'),('kwhTotal','kwh_total','decimal(18,6)'),('charging_fees','charging_fees','decimal(18,6)'),('created','created','timestamp'),('ended','ended','timestamp'),('startTime','start_hour','int'),('endTime','end_hour','int'),('chargeTimeHrs','charge_time_hrs','double'),('weekday','weekday_text','string'),('platform','platform','string'),('userId','user_id','int'),('stationId','station_id','int'),('locationId','district','string'),('managerVehicle','manager_vehicle','int'),('facilityType','facility_type','int')] + [(x,x.lower(),'int') for x in ['Mon','Tues','Wed','Thurs','Fri','Sat','Sun']],
 'snapshot': [(x,y,z) for x,y,z in [('stationId','station_id','int'),('record_time','record_time','timestamp'),('in_use','in_use','int'),('idle','idle','int'),('fault','fault','int'),('charging_power_kw','charging_power_kw','double'),('queue_count','queue_count','int'),('cum_kwh_day','cum_kwh_day','double')]],
 'bms': [('esd','session_id','long'),('record_time','record_time','timestamp'),('soc','soc','double'),('pack_voltage (V)','pack_voltage_v','double'),('charge_current (A)','charge_current_a','double'),('max_cell_voltage (V)','max_cell_voltage_v','double'),('min_cell_voltage (V)','min_cell_voltage_v','double'),('max_temperature (℃)','max_temperature_c','double'),('min_temperature (℃)','min_temperature_c','double'),('available_energy (kw)','available_energy_kw','double'),('available_capacity (Ah)','available_capacity_ah','double')]
}
FILENAMES = {x: 'beijing_'+y+'.csv' for x,y in [('station','stations'),('weather','weather'),('session','sessions'),('snapshot','snapshots'),('bms','bms')]}
KEYS = {'station':['station_id'],'weather':['dt'],'session':['session_id'],'snapshot':['station_id','record_time'],'bms':['session_id','record_time']}

def now(): return dt.datetime.now(dt.timezone(dt.timedelta(hours=8))).isoformat()

def options():
 p=argparse.ArgumentParser()
 p.add_argument('--config',default=str(REPO/'bigdata/conf/dev.yaml'))
 p.add_argument('--run-id',default=os.environ.get('RUN_ID') or dt.datetime.now().strftime('%Y%m%d_%H%M%S_')+uuid.uuid4().hex[:6])
 p.add_argument('--stations',default='')
 p.add_argument('--stage',default='all')
 a=p.parse_args(); c=yaml.safe_load(Path(a.config).read_text()); c['run_id']=a.run_id
 if os.environ.get('BIGDATA_ROOT'): c['root']=os.environ['BIGDATA_ROOT']
 if '://' not in c['root']: c['root']=(REPO/c['root']).resolve().as_uri()
 return a,c

def spark_for(c,name):
 b=(SparkSession.builder.appName('charging-'+name+'-'+c['run_id'])
   .config('spark.sql.session.timeZone','Asia/Shanghai')
   .config('spark.sql.shuffle.partitions',str(c['shuffle_partitions']))
   .config('spark.sql.adaptive.enabled','true')
   .config('spark.sql.parquet.compression.codec','snappy')
   .config('spark.sql.ansi.enabled','false'))
 # spark-submit owns master when supplied by the launcher.
 if not os.environ.get('SPARK_SUBMIT_MASTER'): b=b.master(c['master'])
 
 if c.get('hive_metastore_uri'): b=b.config('hive.metastore.uris',c['hive_metastore_uri']).enableHiveSupport()
 elif c.get('enable_hive',False) or name=='catalog': b=b.config('spark.sql.warehouse.dir',path(c,'warehouse')).enableHiveSupport()
 s=b.getOrCreate(); s.sparkContext.setLogLevel('WARN'); return s

def path(c,p): return c['root'].rstrip('/')+'/'+p

def read(s,c,p): return s.read.parquet(path(c,p))

def write(d,c,p,partition=None):
 w=(d.repartition(8,F.col(partition)) if partition else d.repartition(8)).write.mode('overwrite').option('compression','snappy')
 if partition: w=w.partitionBy(partition)
 w.parquet(path(c,p))

def write_json(s,c,p,value):
 target=s._jvm.org.apache.hadoop.fs.Path(path(c,p)); fs=target.getFileSystem(s._jsc.hadoopConfiguration())
 stream=fs.create(target,True)
 try: stream.write(bytearray(json.dumps(value,ensure_ascii=False,allow_nan=False,default=str).encode()))
 finally: stream.close()

def read_json(s,c,p):
 return json.loads('\n'.join(r.value for r in s.read.text(path(c,p)).collect()))

def exists(s,c,p):
 target=s._jvm.org.apache.hadoop.fs.Path(path(c,p))
 return target.getFileSystem(s._jsc.hadoopConfiguration()).exists(target)

def source(s,c,name):
 raw=s.read.option('header',True).option('mode','FAILFAST').csv(path(c,'ods/beijing/'+name+'/ingest_date='+c['run_id'][:8]+'/'+FILENAMES[name]))
 required=[v[0] for v in SPECS[name]]
 if raw.columns!=required: raise ValueError(f'BLOCKER schema {name}: {raw.columns} != {required}')
 # Keep exact raw CSV values for quarantine and replay; never silently cast away evidence.
 raw=raw.withColumn('raw_values',F.to_json(F.struct(*[F.col('`'+v+'`') for v in raw.columns])))
 return raw.select(*[F.col('`'+a+'`').cast(t).alias(b) for a,b,t in SPECS[name]],'raw_values',F.input_file_name().alias('source_file'))

def audit(s,c,stage,status,started,error=None,rows=None):
 record=dict(run_id=c['run_id'],stage=stage,status=status,started_at=started,ended_at=now(),application_id=s.sparkContext.applicationId,master=s.sparkContext.master,input_root=c['root'],output_rows=rows,error=error)
 write_json(s,c,'audit/'+c['run_id']+'/attempts/'+stage+'_'+uuid.uuid4().hex+'.json',record)
 write_json(s,c,'audit/'+c['run_id']+'/'+stage+'.json',record)

def run(stage,fn):
 a,c=options(); s=spark_for(c,stage); started=now()
 if a.stations: stage += '_'+a.stations.replace(',','_')
 try:
  result=fn(s,c,a); audit(s,c,stage,'SUCCESS',started,rows=result)
 except Exception as e:
  audit(s,c,stage,'FAILED',started,error=str(e)[:3000]); raise
 finally: s.stop()
