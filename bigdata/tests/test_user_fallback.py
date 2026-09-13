from train_user_als import energy_fallback,ranking_metrics

def test_unknown_user_and_slot_still_return_energy():
 assert energy_fallback(1,2,{1:10},{2:20},30)==10
 assert energy_fallback(-1,2,{1:10},{2:20},30)==20
 assert energy_fallback(-1,-1,{1:10},{2:20},30)==30

def test_ranking_metrics_include_all_candidates():
 m=ranking_metrics({1:list(range(168)),2:list(range(168))},[{'user_id':1,'slot':0},{'user_id':2,'slot':3}])
 assert m['hit_rate_1']==.5 and m['hit_rate_3']==.5 and m['mrr']==.625
