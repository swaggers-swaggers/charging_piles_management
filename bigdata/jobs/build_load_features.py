from common import *
from pyspark.sql import Window
LAGS=[1,2,3,6,12,24,48,168]
EXOG=['utilization','fault_rate','queue_count','orders','users','temp_high','temp_low','precipitation','holiday','condition']
FEATURES=['station_id','facility_type','device_count','hour','weekday','month','hour_sin','hour_cos','week_sin','week_cos']+[f'lag_{i}' for i in LAGS]+[f'{op}_{i}' for i in [6,24,168] for op in ['mean','std','max','trend']]+['observed_'+k for k in EXOG]
# Historical settlement labels become available at the END of the preceding hour.
# All features here have timestamps strictly less than event_hour; no future telemetry/weather.

def features(d):
 w=Window.partitionBy('station_id').orderBy('event_hour')
 d=d.withColumn('hour',F.hour('event_hour')).withColumn('weekday',F.pmod(F.dayofweek('event_hour')+5,F.lit(7))).withColumn('month',F.month('event_hour'))
 for col,period,prefix in [('hour',24,'hour'),('weekday',7,'week')]:
  d=d.withColumn(prefix+'_sin',F.sin(F.col(col)*2*3.141592653589793/period)).withColumn(prefix+'_cos',F.cos(F.col(col)*2*3.141592653589793/period))
 for col in EXOG:
  d=d.withColumn('observed_'+col,F.coalesce(F.lag(col).over(w),F.lit(0.)))
 for lag in LAGS: d=d.withColumn(f'lag_{lag}',F.lag('load_kwh',lag).over(w))
 for size in [6,24,168]:
  win=w.rowsBetween(-size,-1)
  d=d.withColumn(f'mean_{size}',F.avg('load_kwh').over(win)).withColumn(f'std_{size}',F.stddev_pop('load_kwh').over(win)).withColumn(f'max_{size}',F.max('load_kwh').over(win)).withColumn(f'trend_{size}',(F.col('lag_1')-F.lag('load_kwh',size).over(w))/F.lit(size-1))
 d=d.withColumn('baseline_hour',F.col('lag_1')).withColumn('baseline_day',F.col('lag_24')).withColumn('baseline_week',F.col('lag_168'))
 weeks=[F.lag('load_kwh',168*i).over(w) for i in range(1,9)]
 d=d.withColumn('baseline_8week',sum(F.coalesce(x,F.lit(0.)) for x in weeks)/sum(F.when(x.isNotNull(),1).otherwise(0) for x in weeks))
 return d.withColumn('feature_max_time',F.col('event_hour')-F.expr('INTERVAL 1 HOUR'))

def build(s,c,a):
 d=features(read(s,c,'dws/station_hourly_load')).filter(F.col('lag_168').isNotNull() & (F.col('missing_snapshots')==0))
 d=d.withColumn('split',F.when(F.col('event_hour')<c['train_end'],'train').when(F.col('event_hour')<c['validation_end'],'validation').when(F.col('event_hour')<c['test_end'],'test').otherwise('demo'))
 write(d,c,'features/load/version='+c['run_id'])
 write_json(s,c,'features/load/current.json',{'version':c['run_id'],'features':FEATURES,'available_at':'Previous closed hour only; recursive forecast freezes observed weather and operations at the origin (persistence scenario), never reads future observations','split':{k:c[k] for k in ['train_end','validation_end','test_end']}})
 return d.count()
if __name__=='__main__': run('features',build)
