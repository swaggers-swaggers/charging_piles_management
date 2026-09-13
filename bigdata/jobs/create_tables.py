"""Register Parquet layers in persistent Spark/Hive catalog; no embedded Derby sharing."""
from common import *
def create(s,c,a):
 # enableHiveSupport is chosen before session creation by common.spark_for when configured.
 s.sql('CREATE DATABASE IF NOT EXISTS charging')
 names={'dwd_station':'dwd/station','dwd_weather':'dwd/weather','dwd_session':'dwd/session','dwd_snapshot':'dwd/snapshot','dwd_bms':'dwd/bms','dws_station_15m_load':'dws/station_15m_load','dws_station_hourly_load':'dws/station_hourly_load','dws_station_daily_operation':'dws/station_daily_operation','dws_user_slot_profile':'dws/user_slot_profile','ads_load_forecast':'ads/load_forecast','ads_data_quality_report':'ads/data_quality_report'}
 for name,loc in names.items():
  if not exists(s,c,loc): continue
  d=read(s,c,loc)
  schema=d.schema.toDDL()
  partition=" PARTITIONED BY (dt)" if 'dt' in d.columns and name!='dwd_station' else ''
  s.sql(f"CREATE TABLE IF NOT EXISTS charging.{name} ({schema}) USING PARQUET{partition} LOCATION '{path(c,loc)}'")
  if partition: s.sql(f'MSCK REPAIR TABLE charging.{name}')
  expected=d.count(); actual=s.table('charging.'+name).count()
  if actual!=expected: raise ValueError(f'Catalog row mismatch {name}: {actual} != {expected}')
 return len(names)
if __name__=='__main__': run('catalog',create)
