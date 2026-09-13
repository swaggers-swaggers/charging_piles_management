import datetime as dt
import pytest
from build_station_load import allocate_sessions
from pyspark.sql import functions as F

def test_midnight_and_exact_boundary(spark):
 d=spark.createDataFrame([(1,dt.datetime(2026,1,1,23,50),dt.datetime(2026,1,2,0,20),30.,60.),(2,dt.datetime(2026,1,2,0,0),dt.datetime(2026,1,2,0,15),10.,20.)], 'session_id int, created timestamp, ended timestamp, kwh_total double, charging_fees double')
 rows=allocate_sessions(d).orderBy('session_id','record_time').collect()
 assert [r.allocated_kwh for r in rows]==pytest.approx([10,15,5,10])
 assert [r.allocated_fees for r in rows]==pytest.approx([20,30,10,20])
 assert len({r.record_time.date() for r in rows})==2
 assert sum(r.allocated_kwh for r in rows)==pytest.approx(40.)
