"""Distribution and business consistency summaries, derived after DWD cleaning."""
from common import *
def profile(s,c,a):
 sessions=read(s,c,'dwd/session'); snapshots=read(s,c,'dwd/snapshot')
 station=sessions.groupBy('station_id').agg(F.count('*').alias('sessions'),F.percentile_approx('average_power_kw',[.01,.25,.5,.75,.99]).alias('power_quantiles'),F.sum(F.when(F.array_contains('quality_flags','power_iqr_outlier'),1).otherwise(0)).alias('power_outliers'))
 users=sessions.groupBy('user_id').count().agg(F.count('*').alias('users'),F.min('count').alias('min_sessions'),F.max('count').alias('max_sessions'),F.avg('count').alias('mean_sessions'),F.sum(F.when(F.col('count')<3,1).otherwise(0)).alias('fewer_than_3_sessions')).first().asDict()
 totals=sessions.agg(F.sum('kwh_total').cast('double').alias('settlement_kwh'),F.sum('charging_fees').cast('double').alias('revenue')).first().asDict()
 telemetry=float(snapshots.agg(F.sum('telemetry_kwh')).first()[0]);totals.update(telemetry_kwh=telemetry,relative_difference=(telemetry-totals['settlement_kwh'])/totals['settlement_kwh'])
 report=dict(run_id=c['run_id'],users=users,load_accounting=totals,station_distributions=[r.asDict(recursive=True) for r in station.orderBy('station_id').collect()])
 write_json(s,c,'ads/quality/distribution.json',report)
 return len(report['station_distributions'])
if __name__=='__main__':run('distribution',profile)
