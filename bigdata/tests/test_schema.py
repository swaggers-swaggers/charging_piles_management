import json,zipfile,csv,io
import pytest
from common import SPECS,FILENAMES,REPO
from export_json import publish

def test_archive_headers_match_contract():
 with zipfile.ZipFile(REPO/'05.北京模拟充电数据集.zip') as z:
  for table,name in FILENAMES.items():
   member=next(n for n in z.namelist() if n.endswith('/'+name))
   with z.open(member) as f: assert next(csv.reader([f.readline().decode('utf-8-sig').strip()]))==[v[0] for v in SPECS[table]]

def test_failed_export_keeps_last_success(tmp_path):
 (tmp_path/'current.json').write_text('{"generation":"old"}')
 schema=json.loads((REPO/'bigdata/serving/schemas/envelope.schema.json').read_text())
 with pytest.raises(Exception): publish(tmp_path,{'load-forecast':{'invalid':True}},schema,'broken')
 assert json.loads((tmp_path/'current.json').read_text())['generation']=='old'
