from quality import rules
from pyspark.sql import functions as F

def rule(id,name): return next(x[2] for x in rules(name) if x[0]==id)

def test_capacity_conflict_and_correction(spark):
 d=spark.createDataFrame([(12,12,0,1),(12,6,5,1)],'_capacity int,in_use int,idle int,fault int')
 assert d.filter(rule('capacity_conflict','snapshot')).count()==1
 fixed=d.withColumn('clean_fault',F.least('fault',F.greatest(F.col('_capacity')-F.col('in_use'),F.lit(0)))).withColumn('clean_idle',F.col('_capacity')-F.col('in_use')-F.col('clean_fault')).collect()
 assert fixed[0].fault==1 and fixed[0].clean_fault==0
 assert all(x.in_use+x.clean_idle+x.clean_fault==x._capacity for x in fixed)

def test_negative_charging_current_is_information(spark):
 r=next(x for x in rules('bms') if x[0]=='negative_current')
 assert r[1]=='INFO' and r[3]=='keep'
 d=spark.createDataFrame([(-64.,)],'charge_current_a double')
 assert d.filter(r[2]).count()==1

def test_invalid_duration_rejected(spark):
 d=spark.createDataFrame([('2026-01-02','2026-01-01',10.,1.,1.)], 'created string,ended string,kwh_total double,charging_fees double,charge_time_hrs double')
 assert d.filter(rule('session_domain','session')).count()==1
