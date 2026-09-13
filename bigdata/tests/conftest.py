import os,sys
from pathlib import Path
import pytest
os.environ['TZ']='Asia/Shanghai'
os.environ['SPARK_LOCAL_IP']='127.0.0.1'
import time
time.tzset()
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'jobs'))
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'serving'))
@pytest.fixture(scope='session')
def spark():
 from pyspark.sql import SparkSession
 s=SparkSession.builder.master('local[2]').appName('charging-contract-tests').config('spark.sql.session.timeZone','Asia/Shanghai').config('spark.sql.shuffle.partitions','2').config('spark.sql.ansi.enabled','false').getOrCreate()
 s.sparkContext.setLogLevel('ERROR');yield s;s.stop()
