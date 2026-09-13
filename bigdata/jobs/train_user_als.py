from common import *
from modeling import metrics
from pyspark.ml.recommendation import ALS
import numpy as np
import math

def energy_fallback(user,slot,user_means,slot_means,global_mean):
 return user_means.get(user,slot_means.get(slot,global_mean))

def ranking_metrics(predictions,held):
 vals=[]
 for row in held:
  order=predictions[row['user_id']]; rank=order.index(row['slot'])+1
  vals.append((rank==1,rank<=3,1/rank,1/math.log2(rank+1) if rank<=3 else 0))
 mean=np.mean(vals,axis=0)
 return dict(n=len(vals),hit_rate_1=float(mean[0]),hit_rate_3=float(mean[1]),mrr=float(mean[2]),ndcg_3=float(mean[3]))

def train(s,c,a):
 v=read_json(s,c,'features/user_demand/current.json')['version']; data=read(s,c,'features/user_demand/version='+v)
 matrix=read(s,c,'dws/user_slot_profile').persist()
 models={}
 for kind,rating,implicit in [('time','time_rating',True),('energy','energy_rating',False)]:
  model=ALS(userCol='user_id',itemCol='slot',ratingCol=rating,implicitPrefs=implicit,rank=c['als_rank'],maxIter=c['als_iterations'],regParam=.1,alpha=20.,seed=20260912,nonnegative=True,coldStartStrategy='nan',numUserBlocks=4,numItemBlocks=4).fit(matrix)
  model.write().overwrite().save(path(c,'models/user_'+kind+'/version='+c['run_id']+'/model')); models[kind]=model
 rows=[r.asDict() for r in matrix.collect()]; users=sorted(r.user_id for r in data.select('user_id').distinct().collect())
 user_means={r.user_id:float(r.avg) for r in data.filter(F.col('split')=='train').groupBy('user_id').agg(F.avg('kwh_total').alias('avg')).collect()}
 slot_means={r.slot:float(r.avg) for r in data.filter(F.col('split')=='train').groupBy('slot').agg(F.avg('kwh_total').alias('avg')).collect()}
 global_mean=float(data.filter(F.col('split')=='train').agg(F.avg('kwh_total')).first()[0])
 bounds=data.filter(F.col('split')=='train').approxQuantile('kwh_total',[.01,.99],.001)
 counts={u:np.zeros(168) for u in users}; popular=np.zeros(168)
 for r in rows: counts[r['user_id']][r['slot']]=r['visits']; popular[r['slot']]+=r['visits']
 candidates=s.createDataFrame([(u,k) for u in users for k in range(168)],'user_id int, slot int')
 predictions={}
 for kind,model in models.items():
  predictions[kind]={(r.user_id,r.slot):float(r.prediction) for r in model.transform(candidates).select('user_id','slot','prediction').collect()}
 def order(scores): return sorted(range(168),key=lambda i:(-float(scores[i]),i))
 ranks={'als':{},'user_history':{},'global_popular':{}}
 for u in users:
  scores=[predictions['time'].get((u,k),float('nan')) for k in range(168)]
  scores=[x if math.isfinite(x) else popular[k] for k,x in enumerate(scores)]
  ranks['als'][u]=order(scores); ranks['user_history'][u]=order(counts[u]); ranks['global_popular'][u]=order(popular)
 evaluation={}
 for split in ['validation','test']:
  held=[r.asDict() for r in data.filter(F.col('split')==split).collect()]
  time_metrics={name:ranking_metrics(rank,held) for name,rank in ranks.items()}
  actual=[r['kwh_total'] for r in held]; energy_metrics={}
  for kind in ['als','user_mean','slot_mean']:
   pp=[]
   for r in held:
    fallback=energy_fallback(r['user_id'],r['slot'],user_means,slot_means,global_mean)
    value=predictions['energy'].get((r['user_id'],r['slot']),float('nan')) if kind=='als' else (fallback if kind=='user_mean' else slot_means.get(r['slot'],global_mean))
    pp.append(float(np.clip(value if math.isfinite(value) else fallback,*bounds)))
   energy_metrics[kind]=metrics(actual,pp)
  evaluation[split]={'time':time_metrics,'energy':energy_metrics}
 time_winner=max(['global_popular','user_history','als'],key=lambda k:evaluation['validation']['time'][k]['hit_rate_3'])
 energy_winner=min(['als','user_mean','slot_mean'],key=lambda k:evaluation['validation']['energy'][k]['mae'])
 origin=dt.datetime.fromisoformat(c['forecast_origin']); next_start=origin+dt.timedelta(hours=1)
 distribution=np.zeros(168,dtype=int); energies=[]; daily_counts={}; cold=sum(u not in user_means for u in users)
 for u in users:
  slot=ranks[time_winner][u][0]; distribution[slot]+=1
  times=[next_start+dt.timedelta(hours=h) for h in range(168)]
  next_time=next(t for t in times if t.weekday()*24+t.hour==slot)
  daily_counts[str(next_time.date())]=daily_counts.get(str(next_time.date()),0)+1
  fallback=energy_fallback(u,slot,user_means,slot_means,global_mean)
  value=predictions['energy'].get((u,slot),float('nan')) if energy_winner=='als' else (fallback if energy_winner=='user_mean' else slot_means.get(slot,global_mean))
  if not math.isfinite(value): cold+=1; value=fallback
  energies.append(float(np.clip(value,*bounds)))
 manifest=dict(version=c['run_id'],feature_version=v,application_id=s.sparkContext.applicationId,time_model=time_winner,energy_model=energy_winner,evaluation=evaluation,user_count=len(users),coverage=1.,cold_start_count=cold,energy_bounds=bounds,global_energy_mean=global_mean,fallback='用户均值 → 同时段均值 → 全局均值；新用户只展示群体分布',origin=origin.isoformat()+'+08:00',generated_at=now())
 write_json(s,c,'models/user/production.json',manifest)
 # User-level factors/mappings remain private in HDFS. ADS contains only aggregates.
 heatmap=[[k%24,k//24,int(popular[k])] for k in range(168)]
 hist,edges=np.histogram(energies,bins=10)
 report=dict(**manifest,heatmap=heatmap,next_slot_distribution=[int(x) for x in distribution],next_day_distribution=daily_counts,energy_distribution=[{'from':float(edges[i]),'to':float(edges[i+1]),'count':int(n)} for i,n in enumerate(hist)],mean_next_kwh=float(np.mean(energies)),cold_start_example={'personalized':False,'next_kwh':energy_fallback(-1,-1,user_means,slot_means,global_mean),'basis':'全局训练均值'})
 write_json(s,c,'ads/user/report.json',report); matrix.unpersist(); return len(users)
if __name__=='__main__': run('train_user',train)
