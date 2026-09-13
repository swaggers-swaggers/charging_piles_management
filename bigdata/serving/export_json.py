"""Publish a coherent seven-file generation via one atomic current pointer."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'jobs'))
from common import *
import jsonschema
import tempfile
import shutil

def atomic_json(file,value):
 file.parent.mkdir(parents=True,exist_ok=True)
 with tempfile.NamedTemporaryFile('w',encoding='utf-8',dir=file.parent,delete=False) as f:
  json.dump(value,f,ensure_ascii=False,allow_nan=False,default=str); f.flush(); os.fsync(f.fileno()); tmp=f.name
 os.replace(tmp,file)

def publish(directory,payloads,schema,run_id):
 # No target is touched until ALL schemas validate. current.json points to one immutable generation.
 for name,value in payloads.items():
  jsonschema.validate(value,schema)
  contract=REPO/'bigdata/serving/schemas'/(name+'.schema.json')
  if contract.exists(): jsonschema.validate(value['data'],json.loads(contract.read_text()))
 forecasts=payloads['load-forecast']['data']
 forecast_schema=json.loads((REPO/'bigdata/serving/schemas/load-forecast.schema.json').read_text())
 jsonschema.validate(forecasts,forecast_schema)
 ids=[v['station_id'] for v in forecasts]
 if len(ids)!=len(set(ids)): raise ValueError('Duplicate station forecast')
 for station in forecasts:
  if [p['horizon'] for p in station['points']]!=list(range(1,25)): raise ValueError('Incomplete horizon')
  origin=dt.datetime.fromisoformat(station['origin'])
  for p in station['points']:
   if dt.datetime.fromisoformat(p['time'])!=origin+dt.timedelta(hours=p['horizon']): raise ValueError('Misaligned forecast timestamp')
   if not p['lower_kwh']<=p['load_kwh']<=p['upper_kwh']: raise ValueError('Invalid forecast bounds')
 if set(ids)!={v['station_id'] for v in payloads['stations']['data']}: raise ValueError('Missing forecast station')
 generation=run_id+'_'+uuid.uuid4().hex[:8]; folder=directory/'runs'/generation
 try:
  for name,value in payloads.items(): atomic_json(folder/(name+'.json'),value)
  atomic_json(directory/'current.json',{'schemaVersion':'1.0','runId':run_id,'generation':generation,'generatedAt':now(),'files':{k:'runs/'+generation+'/'+k+'.json' for k in payloads}})
 except Exception:
  shutil.rmtree(folder,ignore_errors=True); raise
 # Fixed filenames for external consumers; dashboard uses pointer for cross-file consistency.
 for name,value in payloads.items(): atomic_json(directory/(name+'.json'),value)
 return generation

def export(s,c,a):
 operation=read_json(s,c,'ads/operation/report.json'); forecast=read_json(s,c,'ads/forecast/report.json'); user=read_json(s,c,'ads/user/report.json'); quality=read_json(s,c,'ads/quality/report.json')
 logs=[]
 for row in s.read.option('wholetext',True).text(path(c,'audit/'+c['run_id']+'/*.json')).collect():
  value=json.loads(row.value)
  if isinstance(value,dict) and 'stage' in value: logs.append(value)
 health=dict(storage=c['root'],master=s.sparkContext.master,stages=sorted(logs,key=lambda x:x['started_at']),layers={layer:path(c,layer) for layer in ['ods','dwd','dws','features','models','ads']},quality_rule_count=len(quality),warning_rows=sum(q['failed_count'] for q in quality if q['severity']=='WARN'),error_rule_hits=sum(q['failed_count'] for q in quality if q['severity'] in ['ERROR','BLOCKER']),status='SUCCESS' if all(v['status']=='SUCCESS' for v in logs) else 'DEGRADED',conservation=read_json(s,c,'audit/'+c['run_id']+'/energy_conservation.json'),source_manifest=read_json(s,c,'audit/'+c['run_id']+'/source_manifest.json'),model_count=len(forecast),baseline_fallbacks=sum(v['needs_optimization'] for v in forecast),test_degraded_count=sum(v['test_degraded'] for v in forecast))
 if exists(s,c,'ads/quality/distribution.json'): health['distribution']=read_json(s,c,'ads/quality/distribution.json')
 raw={'overview':operation,'stations':[{'station_id':r.station_id,'name':r.station_name,'district':r.district,'device_count':r.device_count} for r in read(s,c,'dwd/station').collect()],'station-ranking':operation['station_health_rank'],'load-forecast':forecast,'user-demand':user,'data-quality':quality,'pipeline-health':health}
 payloads={name:dict(schemaVersion='1.0',dataNature='模拟数据',dataTime=c['forecast_origin'].replace(' ','T')+'+08:00',generatedAt=now(),runId=c['run_id'],data=value) for name,value in raw.items()}
 schema=json.loads((REPO/'bigdata/serving/schemas/envelope.schema.json').read_text())
 generation=publish(REPO/('.bigdata/sample-dashboard/data' if c.get('sample') else 'code/web/data'),payloads,schema,c['run_id'])
 write_json(s,c,'ads/dashboard/current.json',{'generation':generation,'run_id':c['run_id'],'generated_at':now()})
 return len(payloads)
if __name__=='__main__': run('export',export)
