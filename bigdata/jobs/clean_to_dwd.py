from common import *
from quality import tagged

def clean(s,c,a):
 counts={}
 for name in SPECS:
  d=tagged(s,c,name).withColumn('run_id',F.lit(c['run_id'])).withColumn('processed_at',F.current_timestamp()).persist()
  time={'station':'update_time','weather':'dt','session':'created','snapshot':'record_time','bms':'record_time'}[name]
  d=d.withColumn('dt',F.to_date(time))
  invalid=d.filter(F.size('error_rules')>0)
  write(invalid,c,'dwd/quarantine/table_name='+name,'dt')
  if name=='station' and invalid.limit(1).count(): raise ValueError('BLOCKER invalid station dimension; cannot silently drop forecast stations')
  good=d.filter(F.size('error_rules')==0)
  if name=='weather': good=good.withColumn('weekday_raw',F.col('weekday')).withColumn('weekday',F.pmod(F.dayofweek('dt')+5,F.lit(7)))
  if name=='session':
   good=good.withColumn('duration_seconds',F.col('ended').cast('long')-F.col('created').cast('long')).withColumn('average_power_kw',F.col('kwh_total')*3600/F.col('duration_seconds'))
   quart=good.groupBy('station_id').agg(F.percentile_approx('average_power_kw',[.25,.75]).alias('_q'))
   good=good.join(quart,'station_id').withColumn('quality_flags',F.when(F.col('average_power_kw')>F.col('_q')[1]+1.5*(F.col('_q')[1]-F.col('_q')[0]),F.array_union('quality_flags',F.array(F.lit('power_iqr_outlier')))).otherwise(F.col('quality_flags'))).drop('_q')
   for i,k in enumerate(['mon','tues','wed','thurs','fri','sat','sun']): good=good.withColumn(k+'_raw',F.col(k)).withColumn(k,(F.pmod(F.dayofweek('created')+5,F.lit(7))==i).cast('int'))
  if name=='snapshot':
   for k in ['in_use','idle','fault']: good=good.withColumn(k+'_raw',F.col(k))
   good=(good.withColumn('fault_alarm_raw',F.col('fault_raw')).withColumn('fault',F.least(F.col('fault_raw'),F.greatest(F.col('_capacity')-F.col('in_use'),F.lit(0))))
     .withColumn('idle',F.col('_capacity')-F.col('in_use')-F.col('fault')).withColumn('telemetry_kwh',F.col('charging_power_kw')*.25))
  if name=='bms': good=good.withColumn('charge_current_abs_a',F.abs('charge_current_a')).withColumn('available_energy_kwh',F.col('available_energy_kw'))
  good=good.drop(*[x for x in good.columns if x.startswith('_')])
  counts[name]=good.count(); write(good,c,'dwd/'+name, 'dt' if name!='station' else None)
  d.unpersist()
 write_json(s,c,'audit/'+c['run_id']+'/dwd_counts.json',counts)
 return counts
if __name__=='__main__': run('clean',clean)
