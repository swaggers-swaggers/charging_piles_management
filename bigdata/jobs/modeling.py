"""MLlib tree serialization and recursive backtests using identical causal features."""
import math
import numpy as np
from common import *
from build_load_features import FEATURES,LAGS,EXOG
from pyspark.ml.feature import VectorAssembler
from pyspark.ml.regression import GBTRegressor

def metrics(actual,predicted):
 a=np.asarray(actual,dtype=float); p=np.asarray(predicted,dtype=float)
 if a.size==0: return dict(n=0,mae=None,rmse=None,wmape=None,smape=None,r2=None)
 e=p-a; denom=np.abs(a).sum(); variance=((a-a.mean())**2).sum()
 return dict(n=int(a.size),mae=float(np.abs(e).mean()),rmse=float(np.sqrt((e*e).mean())),wmape=float(np.abs(e).sum()/denom) if denom else None,smape=float(np.mean(2*np.abs(e)/np.maximum(np.abs(a)+np.abs(p),1e-9))),r2=float(1-(e*e).sum()/variance) if variance else None)

def node_dict(node):
 if node.getClass().getSimpleName()=='LeafNode': return {'value':float(node.prediction())}
 split=node.split()
 return dict(feature=int(split.featureIndex()),threshold=float(split.threshold()),left=node_dict(node.leftChild()),right=node_dict(node.rightChild()))

def serialize(model): return dict(trees=[node_dict(t._java_obj.rootNode()) for t in model.trees],weights=list(model.treeWeights))

def tree_predict(node,x):
 if 'value' in node: return node['value']
 return tree_predict(node['left'] if x[node['feature']]<=node['threshold'] else node['right'],x)

def score(model,x): return max(0.,sum(w*tree_predict(t,x) for w,t in zip(model['weights'],model['trees'])))

def feature_vector(history,time,station):
 v=[station['station_id'],station['facility_type'],station['device_count'],time.hour,time.weekday(),time.month,math.sin(time.hour*2*math.pi/24),math.cos(time.hour*2*math.pi/24),math.sin(time.weekday()*2*math.pi/7),math.cos(time.weekday()*2*math.pi/7)]
 v += [history[-i] for i in LAGS]
 for size in [6,24,168]:
  a=np.asarray(history[-size:]); v += [float(a.mean()),float(a.std()),float(a.max()),float((a[-1]-a[0])/(size-1))]
 v += list(station.get('exogenous',[0.]*len(EXOG)))
 return v

def forecast(history,times,station,kind,model=None):
 h=list(history); result=[]
 for time in times:
  if kind=='hour': value=h[-1]
  elif kind=='day': value=h[-24]
  elif kind=='week': value=h[-168]
  elif kind=='8week': value=float(np.mean([h[-i*168] for i in range(1,9) if len(h)>=i*168]))
  else: value=score(model,feature_vector(h,time,station))
  result.append(max(0.,float(value))); h.append(result[-1])
 return result

def series(s,c,station_id):
 rows=read(s,c,'dws/station_hourly_load').filter(F.col('station_id')==station_id).orderBy('event_hour').select('event_hour','load_kwh','missing_snapshots',*EXOG).collect()
 # Naive timestamps consistently interpreted in Asia/Shanghai, set by launcher TZ.
 return [r.event_hour for r in rows],[float(r.load_kwh) for r in rows],[int(r.missing_snapshots) for r in rows],[[float(r[k] or 0.) for k in EXOG] for r in rows]

def backtest(times,loads,missing,station,start,end,kind,model=None,exogenous=None):
 actual=[]; predicted=[]; residual=[[] for _ in range(24)]; by_day=[]
 for idx,time in enumerate(times):
  if idx<168 or time.hour!=0 or not(start<=str(time)<end) or idx+24>len(times): continue
  if str(times[idx+23])>=end or any(missing[idx:idx+24]): continue
  origin_station={**station,'exogenous':exogenous[idx-1] if exogenous is not None else [0.]*len(EXOG)}
  p=forecast(loads[max(0,idx-1344):idx],times[idx:idx+24],origin_station,kind,model)
  a=loads[idx:idx+24]; actual+=a; predicted+=p
  for k,(aa,pp) in enumerate(zip(a,p)): residual[k].append(aa-pp)
  by_day.append({'date':str(time.date()),'actual':a,'predicted':p})
 return metrics(actual,predicted),residual,by_day

def fit(s,c,data,scope):
 train=data.filter(F.col('split')=='train')
 assembler=VectorAssembler(inputCols=FEATURES,outputCol='features',handleInvalid='error')
 prepared=assembler.transform(train).select('features',F.col('load_kwh').cast('double').alias('label')).persist()
 estimator=GBTRegressor(maxIter=c['gbt_iterations'],maxDepth=c['gbt_depth'],seed=20260912,stepSize=.1,lossType='squared',maxBins=64)
 model=estimator.fit(prepared); prepared.unpersist()
 base='models/load/'+scope+'/version='+c['run_id']
 model.write().overwrite().save(path(c,base+'/model'))
 plain=serialize(model); write_json(s,c,base+'/portable.json',plain)
 # Verify portable recursive evaluator against Spark, not merely against itself.
 probe=assembler.transform(train.limit(30)); check=model.transform(probe).select('features','prediction').collect()
 if any(abs(score(plain,list(r.features))-max(0.,r.prediction))>1e-7 for r in check): raise ValueError('Portable tree prediction mismatch')
 manifest=dict(version=c['run_id'],feature_version=read_json(s,c,'features/load/current.json')['version'],features=FEATURES,parameters={p.name:v for p,v in estimator.extractParamMap().items()},train_start=str(train.agg(F.min('event_hour')).first()[0]),train_end=str(train.agg(F.max('event_hour')).first()[0]),trained_at=now(),application_id=s.sparkContext.applicationId,code_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=REPO,text=True).strip(),feature_importance=dict(zip(FEATURES,[float(v) for v in model.featureImportances])),published=False)
 manifest['code_sha256']=hashlib.sha256(b''.join(p.read_bytes() for p in sorted((REPO/'bigdata/jobs').glob('*.py')))).hexdigest()
 manifest['working_tree_dirty']=bool(subprocess.check_output(['git','status','--porcelain'],cwd=REPO,text=True).strip())
 write_json(s,c,base+'/manifest.json',manifest)
 return plain,manifest

def feature_data(s,c):
 version=read_json(s,c,'features/load/current.json')['version']
 return read(s,c,'features/load/version='+version)
