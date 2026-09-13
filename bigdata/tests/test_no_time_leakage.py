import datetime as dt
import numpy as np
import pytest
from pyspark.sql import functions as F
from build_load_features import features,FEATURES,EXOG
from modeling import feature_vector,forecast,backtest

def test_future_perturbation_cannot_change_past_features(spark):
 start=dt.datetime(2026,1,1)
 rows=[(1001,start+dt.timedelta(hours=i),float(i%33),1,12) for i in range(400)]
 d=spark.createDataFrame(rows,'station_id int,event_hour timestamp,load_kwh double,facility_type int,device_count int')
 for col in EXOG: d=d.withColumn(col,F.lit(0.))
 cutoff=start+dt.timedelta(hours=300)
 first=features(d).filter(F.col('event_hour')==cutoff).first()
 changed=features(d.withColumn('load_kwh',F.when(F.col('event_hour')>=cutoff,999999.).otherwise(F.col('load_kwh')))).filter(F.col('event_hour')==cutoff).first()
 assert [first[k] for k in FEATURES]==[changed[k] for k in FEATURES]
 local=feature_vector([r[2] for r in rows[:300]],cutoff,dict(station_id=1001,facility_type=1,device_count=12))
 assert local==pytest.approx([first[k] for k in FEATURES],abs=1e-9)
 assert first.feature_max_time<first.event_hour
 future_weather=d.withColumn('temp_high',F.when(F.col('event_hour')>=cutoff,999.).otherwise(F.col('temp_high')))
 check=features(future_weather).filter(F.col('event_hour')==cutoff).first()
 assert check.observed_temp_high==first.observed_temp_high

def test_recursive_baseline_does_not_read_future():
 hist=list(range(200)); times=[dt.datetime(2026,1,2)+dt.timedelta(hours=i) for i in range(24)]
 assert forecast(hist,times,{},'hour')==[199.]*24
 assert forecast(hist,times,{},'day')==list(range(176,200))

def test_incomplete_end_window_excluded():
 times=[dt.datetime(2026,1,1)+dt.timedelta(hours=i) for i in range(200)]
 m,_,_=backtest(times,list(range(200)),[0]*200,{},'2026-01-09','2026-01-10','hour')
 assert m['n']==0
