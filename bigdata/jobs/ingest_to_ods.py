"""ZIP verification and immutable ingestion, using Hadoop's filesystem API."""
import csv, hashlib, io, zipfile
from common import *

def ingest(s,c,a):
 archive=REPO/c.get('source_zip','05.北京模拟充电数据集.zip'); manifest=[]
 with zipfile.ZipFile(archive) as z:
  bad=z.testzip()
  if bad: raise ValueError('Corrupt archive member: '+bad)
  for table,filename in FILENAMES.items():
   names=[n for n in z.namelist() if n.split('/')[-1]==filename]
   if len(names)!=1: raise ValueError('Missing/ambiguous member '+filename)
   target=path(c,'ods/beijing/'+table+'/ingest_date='+c['run_id'][:8]+'/'+filename)
   p=s._jvm.org.apache.hadoop.fs.Path(target); fs=p.getFileSystem(s._jsc.hadoopConfiguration())
   # Identical ingestion date is reusable; different bytes are rejected, never overwritten.
   digest=hashlib.sha256(); lines=0
   with z.open(names[0]) as f:
    head=f.readline(); columns=next(csv.reader([head.decode('utf-8-sig').strip()]))
    if columns!=[v[0] for v in SPECS[table]]: raise ValueError('BLOCKER schema '+table)
   local=REPO/'.bigdata/source'/filename; local.parent.mkdir(parents=True,exist_ok=True)
   with z.open(names[0]) as src,local.open('wb') as out:
    for block in iter(lambda:src.read(1024*1024),b''):
     digest.update(block); lines+=block.count(b'\n'); out.write(block)
   if fs.exists(p):
    old=s.read.text(target).count()-1
    check=path(c,'ods/manifests/'+c['run_id'][:8]+'/'+table+'.json')
    prev=read_json(s,c,'ods/manifests/'+c['run_id'][:8]+'/'+table+'.json')
    if prev['sha256']!=digest.hexdigest() or old!=lines-1: raise ValueError('Immutable ODS conflict '+table)
   else:
    fs.mkdirs(p.getParent()); fs.copyFromLocalFile(False,False,s._jvm.org.apache.hadoop.fs.Path(local.as_uri()),p)
   if not fs.exists(p): raise RuntimeError('ODS missing '+target)
   count=s.read.option('header',True).csv(target).count()
   if count!=lines-1: raise ValueError('ODS count mismatch')
   item=dict(table=table,rows=count,sha256=digest.hexdigest(),source=filename,path=target,hdfs_checksum=str(fs.getFileChecksum(p)),columns=columns)
   write_json(s,c,'ods/manifests/'+c['run_id'][:8]+'/'+table+'.json',item); manifest.append(item)
 write_json(s,c,'audit/'+c['run_id']+'/source_manifest.json',manifest)
 return sum(x['rows'] for x in manifest)
if __name__=='__main__': run('ingest',ingest)
