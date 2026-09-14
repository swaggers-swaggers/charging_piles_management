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
 direct=read_json(s,c,'models/load/direct/production.json') if exists(s,c,'models/load/direct/production.json') else None
 direct_comparison={h:{'validation':v['validation'],'test':v['test']} for h,v in direct['horizons'].items()} if direct else {}
 residual_path='models/load/residual_cnn/production.json'
 residual=read_json(s,c,residual_path) if exists(s,c,residual_path) else None
 residual_selection={h:{'winner_counts':v['winner_counts'],'selected_test':v['selected_test']}
                     for h,v in residual.get('station_selection',{}).items()} if residual else {}
 write_json(s,c,'ads/model_metrics/report.json',dict(run_id=c['run_id'],comparison=comparison,direct_horizons=direct_comparison,residual_cnn=residual_selection,station_count=len(entries),test_start=c['validation_end'],test_end_exclusive=c['test_end'],selection='validation chooses hourly and direct winners independently; H6/H24 include residual CNN on common windows; test never used for selection'))
 return comparison
if __name__=='__main__': run('evaluate',evaluate)
