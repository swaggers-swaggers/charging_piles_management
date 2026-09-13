import yaml
from common import REPO,SPECS,KEYS

def test_schema_dictionary_matches_executable_contract():
 data=yaml.safe_load((REPO/'bigdata/conf/schema_contracts.yaml').read_text())
 for name,spec in SPECS.items():
  assert data['tables'][name]['primary_key']==KEYS[name]
  assert [(v['source'],v['name'],v['type']) for v in data['tables'][name]['fields']]==spec
