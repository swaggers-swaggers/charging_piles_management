"""Three actual stations, all dates, referenced sessions/BMS: no synthetic fake metrics."""
import csv,io,zipfile
from pathlib import Path
repo=Path(__file__).resolve().parents[2]
output=repo/'.bigdata/sample.zip';output.parent.mkdir(exist_ok=True)
ids={1001,1014,1035}; session_ids=set()
with zipfile.ZipFile(repo/'05.北京模拟充电数据集.zip') as source,zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as target:
 for table in ['stations','weather','sessions','snapshots','bms']:
  filename='beijing_'+table+'.csv'; member=next(x for x in source.namelist() if x.endswith('/'+filename))
  with source.open(member) as raw,target.open('sample/'+filename,'w') as dest:
   reader=csv.DictReader(io.TextIOWrapper(raw,encoding='utf-8-sig')); stream=io.TextIOWrapper(dest,encoding='utf-8',newline='');writer=csv.DictWriter(stream,fieldnames=reader.fieldnames);writer.writeheader();n=0
   for row in reader:
    keep=table=='weather' or (table=='bms' and int(row['esd']) in session_ids) or (table not in ['weather','bms'] and int(row['stationId']) in ids)
    if keep:
     writer.writerow(row);n+=1
     if table=='sessions':session_ids.add(int(row['sessionId']))
   stream.flush();stream.detach();print(table,n)
print(output)
