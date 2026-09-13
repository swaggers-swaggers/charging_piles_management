import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'serving'))
from export_json import *
if __name__=='__main__': run('export',export)
