from modeling import *

def train(s,c,a):
 data=feature_data(s,c).persist()
 stations=[r.asDict() for r in read(s,c,'dwd/station').orderBy('station_id').collect()]
 if a.stations: stations=[x for x in stations if x['station_id'] in {int(i) for i in a.stations.split(',')}]
 gv=read_json(s,c,'models/load/global/current.json')['version']; global_model=read_json(s,c,'models/load/global/version='+gv+'/portable.json')
 for station in stations:
  sid=station['station_id']; scope='station_id='+str(sid); failure=None
  try: local,manifest=fit(s,c,data.filter(F.col('station_id')==sid),scope)
  except Exception as e: local=None; manifest={}; failure=str(e)[:2000]
  times,loads,missing,exogenous=series(s,c,sid)
  candidates={key:None for key in ['hour','day','week','8week']}; candidates['global']=global_model
  if local: candidates['station']=local
  validation={}; tests={}; residuals={}; daily={}
  for name,model in candidates.items():
   validation[name],residuals[name],_=backtest(times,loads,missing,station,c['train_end'],c['validation_end'],name,model,exogenous)
   tests[name],_,daily[name]=backtest(times,loads,missing,station,c['validation_end'],c['test_end'],name,model,exogenous)
  if not all(v['n'] for v in validation.values()): raise ValueError('Empty validation window')
  baseline=min(['hour','day','week','8week'],key=lambda x:validation[x]['mae'])
  winner=min(candidates,key=lambda x:validation[x]['mae'])
  # Selection uses validation only; publish test results even when generalization deteriorates.
  interval=[[float(np.quantile(r,.1)),float(np.quantile(r,.9))] for r in residuals[winner]]
  production=dict(features=FEATURES,station_id=sid,version=c['run_id'],global_version=gv,winner=winner,best_baseline=baseline,validation=validation,test=tests,interval_residual_p10_p90=interval,station_training_error=failure,train_start=manifest.get('train_start','2025-01-08'),train_end=manifest.get('train_end',c['train_end']),feature_importance=manifest.get('feature_importance',{}),published_at=now(),needs_optimization=winner not in ('station','global'),test_degraded=tests[winner]['mae']>tests[baseline]['mae'],application_id=s.sparkContext.applicationId)
  # Three contiguous temporal blocks; models are trained before every block.
  records=daily[winner]; production['rolling_backtests']=[]
  for block in np.array_split(np.arange(len(records)),3):
   days=[records[int(i)] for i in block]
   if days: production['rolling_backtests'].append(dict(start=days[0]['date'],end=days[-1]['date'],**metrics([v for d in days for v in d['actual']],[v for d in days for v in d['predicted']])))
  write_json(s,c,'models/load/'+scope+'/version='+c['run_id']+'/metrics.json',production)
  manifest.update(published=winner=='station',selected=winner,published_at=production['published_at'],test_metrics=tests.get('station'),failure=failure)
  write_json(s,c,'models/load/'+scope+'/version='+c['run_id']+'/manifest.json',manifest)
  write_json(s,c,'models/load/'+scope+'/production.json',production)
  print('STATION_COMPLETE',sid,winner,tests[winner],flush=True)
 data.unpersist(); return len(stations)
if __name__=='__main__': run('train_station',train)
