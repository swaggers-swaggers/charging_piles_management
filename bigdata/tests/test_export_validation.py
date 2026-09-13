import datetime as dt,json
import pytest
from export_json import publish
from common import REPO

def test_mixed_station_or_bad_horizon_rejected_before_pointer(tmp_path):
 # Use actual sample ADS when available; this exercises business invariants beyond JSON syntax.
 folder=REPO/'.bigdata/sample-dashboard/data'
 if not (folder/'load-forecast.json').exists():pytest.skip('sample artifact not generated')
 names=['overview','stations','station-ranking','load-forecast','user-demand','data-quality','pipeline-health']
 p={k:json.loads((folder/(k+'.json')).read_text()) for k in names}
 p['load-forecast']['data'][0]['points'][1]['time']=p['load-forecast']['data'][0]['points'][0]['time']
 schema=json.loads((REPO/'bigdata/serving/schemas/envelope.schema.json').read_text())
 with pytest.raises(ValueError,match='Misaligned'):publish(tmp_path,p,schema,'bad')
 assert not (tmp_path/'current.json').exists()
