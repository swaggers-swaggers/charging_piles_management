from modeling import *
def evaluate(s,c,a):
 entries=[]
 for row in read(s,c,'dwd/station').select('station_id').collect():
  entries.append(read_json(s,c,'models/load/station_id='+str(row.station_id)+'/production.json'))
 comparison={}
 for name in ['hour','day','week','8week','global','station','selected']:
  mm=[e['test'].get(e['winner'] if name=='selected' else name) for e in entries]; mm=[m for m in mm if m]
  n=sum(m['n'] for m in mm)
  comparison[name]={'stations':len(mm),'n':n,'mae':sum(m['mae']*m['n'] for m in mm)/n,'rmse':math.sqrt(sum(m['rmse']**2*m['n'] for m in mm)/n)}
 write_json(s,c,'ads/model_metrics/report.json',dict(run_id=c['run_id'],comparison=comparison,station_count=len(entries),test_start=c['validation_end'],test_end_exclusive=c['test_end'],selection='validation daily recursive 24-hour MAE; test never used for selection'))
 return comparison
if __name__=='__main__': run('evaluate',evaluate)
