from modeling import *
def predict(s,c,a):
 output=[]; origin=dt.datetime.fromisoformat(c['forecast_origin']); future=[origin+dt.timedelta(hours=i) for i in range(1,25)]
 stations=read(s,c,'dwd/station').orderBy('station_id').collect()
 direct=read_json(s,c,'models/load/direct/production.json')
 residual_path='models/load/residual_cnn/production.json'
 residual=read_json(s,c,residual_path) if exists(s,c,residual_path) else None
 if residual and residual.get('feature_version')!=direct.get('feature_version'):
  raise ValueError('Residual CNN feature version mismatch; keeping previous dashboard release')
 if residual and residual.get('origin')!=origin.isoformat()+'+08:00':
  raise ValueError('Residual CNN origin mismatch; keeping previous dashboard release')
 direct_models={h:read_json(s,c,f'models/load/direct_horizon={h}/version='+direct['version']+'/portable.json') for h in [1,6,24]}
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
  direct_horizons={}
  for horizon in [1,6,24]:
   meta=direct['horizons'][str(horizon)]['stations'][str(sid)]
   shared=max(0.,score(direct_models[horizon],feature_vector(history[-1344:],future[0],station))*station['device_count'])
   baseline=sum(forecast(history[-1344:],future[:horizon],station,'8week'))
   winner=meta['winner']; predicted=shared if winner=='shared_gbt' else baseline
   chosen=meta['test'][winner]; base_metrics=meta['test']['8week']
   candidate_predictions={'shared_gbt':shared,'8week':baseline}
   selection='站点验证集在共享 GBT 与 8 周基线中选优'
   if residual and horizon in (6,24):
    cnn_meta=residual['station_selection'][str(horizon)]['stations'][str(sid)]
    winner=cnn_meta['winner']; predicted=cnn_meta['prediction_kwh']
    chosen=cnn_meta['test'][winner]; base_metrics=cnn_meta['test']['8week']
    candidate_predictions=cnn_meta['candidate_predictions_kwh']
    selection='相同窗口逐站验证：残差 CNN / 共享 GBT / 8 周基线三选一'
   direct_horizons[str(horizon)]=dict(model=winner,prediction_kwh=predicted,shared_gbt_prediction_kwh=shared,
      baseline_prediction_kwh=baseline,metrics=chosen,baseline_metrics=base_metrics,
      residual_cnn_prediction_kwh=candidate_predictions.get('residual_cnn'),
      candidate_predictions_kwh=candidate_predictions,selection=selection,
      candidate_version=residual['run_id'] if residual and horizon in (6,24) else direct['version'],
      improvement=1-chosen['mae']/base_metrics['mae'] if base_metrics['mae'] else None)
  points=[]
  for h,(time,value) in enumerate(zip(future,preds)):
   busy=math.ceil(value/p) if p else None; capacity=station['device_count']*p if p else None
   lo,hi=prod['interval_residual_p10_p90'][h]
   points.append(dict(horizon=h+1,time=time.isoformat()+'+08:00',load_kwh=value,lower_kwh=min(value,max(0.,value+lo)),upper_kwh=max(value,value+hi),idle=max(0,station['device_count']-busy) if busy is not None else None,queue=max(0,busy-station['device_count']) if busy is not None else None,capacity_kwh=capacity,alert='peak' if capacity and (value>capacity*.85 or busy>station['device_count']) else 'normal'))
  m=prod['test'][kind]; base=prod['test'][prod['best_baseline']]
  output.append(dict(station_id=sid,name=station['station_name'],district=station['district'],device_count=station['device_count'],model=kind,model_version=prod['version'],origin=origin.isoformat()+'+08:00',train_start=prod['train_start'],train_end=prod['train_end'],metrics=m,baseline_metrics=base,baseline=prod['best_baseline'],improvement=1-m['mae']/base['mae'] if base['mae'] else None,needs_optimization=prod['needs_optimization'],test_degraded=prod['test_degraded'],feature_importance=prod['feature_importance'],rolling_backtests=prod['rolling_backtests'],recent_backtest=prod.get('recent_backtest',[]),direct_horizons=direct_horizons,interval_method='验证集逐步长残差 P10/P90 经验区间，非概率保证',exogenous='小时曲线使用历史负荷、时间与站点信息；H1 在共享 GBT 与 8 周基线中选优，H6/H24 加入 CNN 残差模型并按相同窗口逐站验证。天气及运营量仅使用起点前观测，未使用未来实测值',history=[dict(time=t.isoformat()+'+08:00',load_kwh=v) for t,v in zip(times,loads) if origin-dt.timedelta(hours=23)<=t<=origin],points=points))
 if len(output)!=len(stations) or any(len(x['points'])!=24 for x in output): raise ValueError('Incomplete forecast')
 write_json(s,c,'ads/forecast/report.json',output)
 flat=[dict(station_id=x['station_id'],model=x['model'],model_version=x['model_version'],**p) for x in output for p in x['points']]
 write(s.createDataFrame(flat),c,'ads/load_forecast')
 return len(flat)
if __name__=='__main__': run('predict',predict)
