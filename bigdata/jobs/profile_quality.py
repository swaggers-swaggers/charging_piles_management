from common import *
from quality import tagged,rules

def profile(s,c,a):
 report=[]
 for name in SPECS:
  d=tagged(s,c,name).persist(); rr=rules(name)
  counts=d.agg(F.count('*').alias('total'),*[F.sum(F.when(F.array_contains('quality_flags',id),1).otherwise(0)).alias(id) for id,*_ in rr]).first().asDict()
  for id,severity,_,action,description in rr:
   n=int(counts[id] or 0); total=counts['total']
   keys=[str(tuple(row)) for row in d.filter(F.array_contains('quality_flags',id)).select(*KEYS[name]).limit(20).collect()] if n else []
   report.append(dict(run_id=c['run_id'],table_name=name,column_name='*',rule_id=id,rule_description=description,severity=severity,total_count=total,failed_count=n,failed_rate=n/total if total else 0,sample_keys=keys,action=action,detected_at=now()))
  d.unpersist()
 # Full weather calendar and station x 15m grid detect entire missing days, not only gaps between extant records.
 w=source(s,c,'weather'); bounds=w.agg(F.min('dt'),F.max('dt')).first()
 calendar=s.range(1).select(F.explode(F.sequence(F.lit(bounds[0]),F.lit(bounds[1]))).alias('dt'))
 missing=calendar.join(w,'dt','left_anti').count()
 for name,n,total,desc in [('weather',missing,calendar.count(),'完整日历缺日')]:
  report.append(dict(run_id=c['run_id'],table_name=name,column_name='dt',rule_id='missing_grid',rule_description=desc,severity='ERROR',total_count=total,failed_count=n,failed_rate=n/max(1,total),sample_keys=[],action='alert',detected_at=now()))
 snapshots=source(s,c,'snapshot')
 grid=source(s,c,'station').select('station_id').crossJoin(calendar).select('station_id',F.explode(F.sequence(F.col('dt').cast('timestamp'),F.col('dt').cast('timestamp')+F.expr('INTERVAL 23 HOURS 45 MINUTES'),F.expr('INTERVAL 15 MINUTES'))).alias('record_time'))
 n=grid.join(snapshots.select('station_id','record_time'),['station_id','record_time'],'left_anti').count(); total=grid.count()
 report.append(dict(run_id=c['run_id'],table_name='snapshot',column_name='record_time',rule_id='missing_grid',rule_description='完整站点15分钟网格缺口',severity='ERROR',total_count=total,failed_count=n,failed_rate=n/max(1,total),sample_keys=[],action='alert',detected_at=now()))
 write_json(s,c,'ads/quality/report.json',report)
 write(s.createDataFrame(report),c,'ads/data_quality_report')
 return len(report)
if __name__=='__main__': run('quality',profile)
