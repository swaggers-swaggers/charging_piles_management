from common import *
from pyspark.sql import Window

def build(s,c,a):
 d=read(s,c,'dwd/session').select('session_id','user_id','created',F.col('kwh_total').cast('double').alias('kwh_total'))
 w=Window.partitionBy('user_id').orderBy(F.col('created').desc(),F.col('session_id').desc())
 d=d.withColumn('holdout_rank',F.row_number().over(w)).withColumn('slot',(F.pmod(F.dayofweek('created')+5,F.lit(7))*24+F.hour('created')).cast('int'))
 d=d.withColumn('split',F.when(F.col('holdout_rank')==1,'test').when(F.col('holdout_rank')==2,'validation').otherwise('train'))
 write(d,c,'features/user_demand/version='+c['run_id'])
 train=d.filter(F.col('split')=='train')
 matrix=train.groupBy('user_id','slot').agg(F.count('*').alias('visits'),F.avg('kwh_total').alias('energy_rating'),F.max('created').alias('last_seen')).withColumn('time_rating',F.log1p('visits'))
 write(matrix,c,'dws/user_slot_profile')
 write(d.select('user_id').distinct().withColumn('als_index',F.col('user_id')),c,'models/user_mapping/version='+c['run_id'])
 write_json(s,c,'features/user_demand/current.json',{'version':c['run_id'],'mapping':'source integer user_id is stable; never exposed in ADS','split':'last event test; penultimate validation; all earlier train'})
 return matrix.count()
if __name__=='__main__': run('user_matrix',build)
