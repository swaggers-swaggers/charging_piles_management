from modeling import *
def predict(s,c,a):
 output=[]; origin=dt.datetime.fromisoformat(c['forecast_origin']); future=[origin+dt.timedelta(hours=i) for i in range(1,25)]
 stations=read(s,c,'dwd/station').orderBy('station_id').collect()
 # Effective per-pile power comes from positive, historical telemetry only.
 power=read(s,c,'dwd/snapshot').filter((F.col('in_use')>0)&(F.col('charging_power_kw')>0)&(F.col('record_time')<=F.lit(origin))).groupBy('station_id').agg(F.percentile_approx(F.col('charging_power_kw')/F.col('in_use'),.5).alias('power')).collect()
 powers={r.station_id:float(r.power) for r in power}
 for row in stations:
  station=row.asDict(); sid=station['station_id']; prod=read_json(s,c,'models/load/station_id='+str(sid)+'/production.json')
  kind=prod['winner']; model=None
  if kind in ('global','station'):
   if prod.get('features')!=FEATURES: raise ValueError('Model feature contract mismatch; keeping previous dashboard release')
   scope='global' if kind=='global' else 'station_id='+str(sid); version=prod['global_version'] if kind=='global' else prod['version']
   model=read_json(s,c,'models/load/'+scope+'/version='+version+'/portable.json')
  times,loads,missing,exogenous=series(s,c,sid); history=[v for t,v in zip(times,loads) if t<=origin]
  station['exogenous']=exogenous[len(history)-1]
  if len(history)<168: raise ValueError('Insufficient history '+str(sid))
  preds=forecast(history[-1344:],future,station,kind,model); p=powers.get(sid)
  points=[]
  for h,(time,value) in enumerate(zip(future,preds)):
   busy=math.ceil(value/p) if p else None; capacity=station['device_count']*p if p else None
   lo,hi=prod['interval_residual_p10_p90'][h]
   points.append(dict(horizon=h+1,time=time.isoformat()+'+08:00',load_kwh=value,lower_kwh=min(value,max(0.,value+lo)),upper_kwh=max(value,value+hi),idle=max(0,station['device_count']-busy) if busy is not None else None,queue=max(0,busy-station['device_count']) if busy is not None else None,capacity_kwh=capacity,alert='peak' if capacity and (value>capacity*.85 or busy>station['device_count']) else 'normal'))
  m=prod['test'][kind]; base=prod['test'][prod['best_baseline']]
  output.append(dict(station_id=sid,name=station['station_name'],district=station['district'],device_count=station['device_count'],model=kind,model_version=prod['version'],origin=origin.isoformat()+'+08:00',train_start=prod['train_start'],train_end=prod['train_end'],metrics=m,baseline_metrics=base,baseline=prod['best_baseline'],improvement=1-m['mae']/base['mae'] if base['mae'] else None,needs_optimization=prod['needs_optimization'],test_degraded=prod['test_degraded'],feature_importance=prod['feature_importance'],rolling_backtests=prod['rolling_backtests'],interval_method='验证集逐步长残差 P10/P90 经验区间，非概率保证',exogenous='历史负荷 + 时间/站点；天气及运营量使用起点前观测并在递归中保持不变（持续性情景），未使用未来实测值',history=[dict(time=t.isoformat()+'+08:00',load_kwh=v) for t,v in zip(times,loads) if origin-dt.timedelta(hours=23)<=t<=origin],points=points))
 if len(output)!=len(stations) or any(len(x['points'])!=24 for x in output): raise ValueError('Incomplete forecast')
 write_json(s,c,'ads/forecast/report.json',output)
 flat=[dict(station_id=x['station_id'],model=x['model'],model_version=x['model_version'],**p) for x in output for p in x['points']]
 write(s.createDataFrame(flat),c,'ads/load_forecast')
 return len(flat)
if __name__=='__main__': run('predict',predict)
