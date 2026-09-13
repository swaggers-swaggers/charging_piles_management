from common import *
def build(s,c,a):
 for name,location in [('hourly','dws/station_hourly_load'),('daily','dws/station_daily_operation'),('stations','dwd/station'),('sessions','dwd/session'),('snapshots','dwd/snapshot')]: read(s,c,location).createOrReplaceTempView(name)
 s.sql("CREATE OR REPLACE TEMP VIEW data_day AS SELECT max(dt) dt FROM snapshots")
 for statement in (REPO/'bigdata/sql/build_ads_operation.sql').read_text().split(';'):
  if statement.strip(): s.sql(statement)
 tables=['operation_overview','station_health_rank','district_map','operation_trend','load_factors','tariff_contribution','realtime_alert','user_segments','load_discrepancy','device_status']
 result={}
 for name in tables:
  d=s.table(name).withColumn('run_id',F.lit(c['run_id'])).withColumn('generated_at',F.current_timestamp()).withColumn('data_nature',F.lit('模拟数据')).withColumn('data_time',F.lit(c['forecast_origin']))
  write(d,c,'ads/'+name); result[name]=[r.asDict(recursive=True) for r in d.collect()]
 write_json(s,c,'ads/operation/report.json',result)
 return {k:len(v) for k,v in result.items()}
if __name__=='__main__': run('ads',build)
