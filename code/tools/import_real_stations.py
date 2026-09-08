"""Import the bundled verified station pack without replacing existing data."""
import argparse
from datetime import datetime
from pathlib import Path
import sqlite3

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('database', type=Path)
args = parser.parse_args()
path = args.database.resolve()
if not path.is_file():
    parser.error('数据库不存在，请先启动服务端初始化')
sql = (Path(__file__).resolve().parents[1] / 'resources/data/beijing_real_stations.sql').read_text()
with sqlite3.connect(path, timeout=10) as db:
    for table in ('station','pile','price_rule'):
        db.execute('SELECT 1 FROM '+table+' LIMIT 1').fetchall()
    backup = path.with_name(path.name+'.before-stations-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.bak')
    with sqlite3.connect(backup) as copy:
        db.backup(copy)
    before = db.execute('SELECT COUNT(*) FROM station').fetchone()[0]
    try:
        db.executescript(sql)
    except Exception:
        db.rollback()
        raise
    after = db.execute('SELECT COUNT(*) FROM station').fetchone()[0]
    print(f'站点 {before} → {after}；备份：{backup}')
