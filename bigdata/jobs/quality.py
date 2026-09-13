"""Declarative row rules shared by profiling and DWD quarantine."""
from common import *
from pyspark.sql import Window

def attach(s,c,name,d):
 if name in ('session','snapshot'):
  st=source(s,c,'station').select('station_id',F.col('device_count').alias('_capacity'),F.col('facility_type').alias('_type'),F.col('district').alias('_district'))
  d=d.join(F.broadcast(st),'station_id','left')
 if name=='session':
  w=source(s,c,'weather').select(F.col('dt').alias('_weather_dt'))
  d=d.join(F.broadcast(w),F.to_date('created')==F.col('_weather_dt'),'left')
 if name=='bms':
  sessions=read(s,c,'dwd/session').select('session_id','created','ended') if exists(s,c,'dwd/session') else source(s,c,'session').select('session_id','created','ended')
  d=d.join(sessions,'session_id','left')
  d=d.withColumn('_prev_soc',F.lag('soc').over(Window.partitionBy('session_id').orderBy('record_time')))
 return d.withColumn('_duplicates',F.count('*').over(Window.partitionBy(*KEYS[name])))

def rules(name):
 # Every typed column is required in this contract. Cast failures are explicit errors.
 required=F.lit(False)
 for _,col,_ in SPECS[name]: required=required | F.col(col).isNull() | (F.trim(F.col(col).cast('string'))=='')
 r=[('required','ERROR',required,'reject','必填字段缺失或类型转换失败'),('duplicate','ERROR',F.col('_duplicates')>1,'reject','主键重复，全部隔离等待明确修复')]
 def add(id,severity,expr,action,desc): r.append((id,severity,expr,action,desc))
 if name=='station':
  add('station_domain','ERROR',(~F.col('facility_type').isin(1,2,3,4))|(F.col('device_count')<=0),'reject','站点类型或设备数非法')
 if name=='weather':
  add('weather_domain','ERROR',(~F.col('holiday').isin(0,1))|(~F.col('condition').between(0,4))|(F.col('temp_high')<F.col('temp_low'))|(F.col('precipitation')<0),'reject','天气范围非法')
  add('weekday','WARN',F.col('weekday')!=F.pmod(F.dayofweek('dt')+5,F.lit(7)),'fix','星期按日期重新计算')
 if name=='session':
  add('foreign_key','ERROR',F.col('_capacity').isNull()|F.col('_weather_dt').isNull(),'reject','站点或开始日天气关联失败')
  add('session_domain','ERROR',(F.col('ended')<=F.col('created'))|(F.col('kwh_total')<0)|(F.col('charging_fees')<0)|(F.col('charge_time_hrs')<=0),'reject','时间先后或电量费用非法')
  add('dimension_mismatch','ERROR',(F.col('facility_type')!=F.col('_type'))|(F.col('district')!=F.col('_district')),'reject','会话站点类型或行政区不一致')
  add('duration','WARN',F.abs((F.col('ended').cast('long')-F.col('created').cast('long'))/3600-F.col('charge_time_hrs'))>0.02,'fix','时长与起止时间不一致，分摊采用真实秒数')
  dow=F.pmod(F.dayofweek('created')+5,F.lit(7)); invalid=F.lit(False)
  for i,k in enumerate(['mon','tues','wed','thurs','fri','sat','sun']): invalid=invalid|(F.col(k)!=(dow==i).cast('int'))
  add('weekday_onehot','WARN',invalid,'fix','星期独热编码按开始日期修复')
  add('cross_midnight','INFO',F.to_date('created')!=F.to_date('ended'),'keep','跨午夜会话按真实时间交集分摊')
 if name=='snapshot':
  add('foreign_key','ERROR',F.col('_capacity').isNull(),'reject','站点关联失败')
  add('snapshot_domain','ERROR',(F.col('in_use')<0)|(F.col('in_use')>F.col('_capacity'))|(F.col('idle')<0)|(F.col('fault')<0)|(F.col('queue_count')<0)|(F.col('charging_power_kw')<0)|(F.pmod(F.col('record_time').cast('long'),F.lit(900))!=0),'reject','数量范围或15分钟时间对齐非法')
  add('capacity_conflict','WARN',F.col('in_use')+F.col('idle')+F.col('fault')!=F.col('_capacity'),'fix','保留原状态，按容量修正故障与空闲')
  add('zero_cumulative','WARN',F.col('cum_kwh_day')==0,'alert','累计电量为零，不作为训练标签')
 if name=='bms':
  add('foreign_key','ERROR',F.col('created').isNull(),'reject','会话关联失败')
  add('bms_range','ERROR',(~F.col('soc').between(0,100))|(F.col('max_cell_voltage_v')<F.col('min_cell_voltage_v'))|(F.col('max_temperature_c')<F.col('min_temperature_c'))|(F.col('record_time')<F.col('created'))|(F.col('record_time')>F.col('ended')),'reject','SOC、电压、温度或会话时间范围非法')
  add('soc_decrease','WARN',F.col('soc')<F.col('_prev_soc'),'alert','会话内SOC下降')
  add('negative_current','INFO',F.col('charge_current_a')<0,'keep','负电流表示充电方向，另派生绝对值')
 return r

def tagged(s,c,name):
 d=attach(s,c,name,source(s,c,name)); rr=rules(name)
 flags=[F.when(F.coalesce(expr,F.lit(False)),F.lit(id)) for id,_,expr,_,_ in rr]
 errors=[F.when(F.coalesce(expr,F.lit(False)),F.lit(id)) for id,sev,expr,_,_ in rr if sev=='ERROR']
 return d.withColumn('quality_flags',F.array_compact(F.array(*flags))).withColumn('error_rules',F.array_compact(F.array(*errors)))
