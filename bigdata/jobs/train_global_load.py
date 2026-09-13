from modeling import *
def train(s,c,a):
 fit(s,c,feature_data(s,c),'global')
 write_json(s,c,'models/load/global/current.json',{'version':c['run_id']})
 return 1
if __name__=='__main__': run('train_global',train)
