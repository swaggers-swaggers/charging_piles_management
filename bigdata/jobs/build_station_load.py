from common import *

def allocate_sessions(d):
 """Half-open windows: an exact endpoint never creates a phantom next bucket."""
 return (d.withColumn('_begin',F.floor(F.col('created').cast('long')/900)*900)
  .withColumn('_last',F.floor((F.col('ended').cast('long')-1)/900)*900)
  .withColumn('bucket_seconds',F.explode(F.sequence('_begin','_last',F.lit(900))))
  .withColumn('overlap_seconds',F.least(F.col('ended').cast('long'),F.col('bucket_seconds')+900)-F.greatest(F.col('created').cast('long'),F.col('bucket_seconds')))
  .withColumn('allocated_kwh',F.col('kwh_total').cast('double')*F.col('overlap_seconds')/(F.col('ended').cast('long')-F.col('created').cast('long')))
  .withColumn('allocated_fees',F.col('charging_fees').cast('double')*F.col('overlap_seconds')/(F.col('ended').cast('long')-F.col('created').cast('long')))
  .withColumn('record_time',F.col('bucket_seconds').cast('timestamp')))

def build(s,c,a):
 sessions=read(s,c,'dwd/session'); alloc=allocate_sessions(sessions).persist()
 errors=alloc.groupBy('session_id').agg(F.sum('allocated_kwh').alias('allocated'),F.first('kwh_total').cast('double').alias('original'))
 err=errors.agg(F.max(F.abs(F.col('allocated')-F.col('original'))/F.greatest(F.abs('original'),F.lit(1e-12))).alias('max_relative_error'),F.sum('allocated').alias('allocated_kwh'),F.sum('original').alias('original_kwh')).first().asDict()
 if (err['max_relative_error'] or 0)>=1e-6: raise ValueError('Energy conservation failed '+str(err))
 write_json(s,c,'audit/'+c['run_id']+'/energy_conservation.json',err)
 station=read(s,c,'dwd/station'); weather=read(s,c,'dwd/weather')
 # Extend through cross-midnight tail so conservation holds outside the 619-day observation grid too.
 b=alloc.agg(F.min('record_time'),F.max('record_time')).first()
 w=weather.agg(F.min('dt'),F.max('dt')).first()
 lo=min(b[0],dt.datetime.combine(w[0],dt.time())); hi=max(b[1],dt.datetime.combine(w[1],dt.time(23,45)))
 grid=station.select('station_id').crossJoin(s.range(1).select(F.explode(F.sequence(F.lit(lo),F.lit(hi),F.expr('INTERVAL 15 MINUTES'))).alias('record_time')))
 energy=alloc.groupBy('station_id','record_time').agg(F.sum('allocated_kwh').alias('load_kwh'),F.sum('allocated_fees').alias('revenue'))
 snap=read(s,c,'dwd/snapshot').select('station_id','record_time','in_use','idle','fault','fault_alarm_raw','queue_count','telemetry_kwh','charging_power_kw')
 quarter=(grid.join(energy,['station_id','record_time'],'left').fillna(0,['load_kwh','revenue'])
  .join(snap,['station_id','record_time'],'left').withColumn('snapshot_missing',F.col('in_use').isNull()).withColumn('dt',F.to_date('record_time')))
 write(quarter,c,'dws/station_15m_load','dt')
 read(s,c,'dws/station_15m_load').createOrReplaceTempView('quarter_load')
 sessions.createOrReplaceTempView('sessions'); station.createOrReplaceTempView('stations'); weather.createOrReplaceTempView('weather')
 for file in ['build_dws.sql']:
  sql=(REPO/'bigdata/sql'/file).read_text()
  for statement in sql.split(';'):
   if statement.strip(): s.sql(statement)
 hourly=s.table('station_hourly').withColumn('dt',F.to_date('event_hour'))
 write(hourly,c,'dws/station_hourly_load','dt')
 daily=s.table('station_hourly').groupBy('station_id',F.to_date('event_hour').alias('dt')).agg(F.sum('load_kwh').alias('load_kwh'),F.sum('revenue').alias('revenue'),F.sum('orders').alias('orders'),F.avg('utilization').alias('utilization'),F.avg('fault_rate').alias('fault_rate'))
 write(daily,c,'dws/station_daily_operation','dt')
 alloc.unpersist(); return {'quarter_rows':quarter.count(),'hourly_rows':hourly.count(),'observed_hourly_rows':hourly.filter(F.to_date('event_hour')<=F.lit(w[1])).count()}
if __name__=='__main__': run('load',build)
