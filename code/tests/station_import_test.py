"""Verify additive and idempotent import against a copy of the shipped database."""
from pathlib import Path
import sqlite3
import tempfile
root = Path(__file__).resolve().parents[2]
sql = (root/'code/resources/data/beijing_real_stations.sql').read_text()
with tempfile.TemporaryDirectory() as temp:
    db = sqlite3.connect(Path(temp)/'test.db')
    with sqlite3.connect(f'file:{root}/database/test.db?mode=ro',uri=True) as source:
        source.backup(db)
    # Repository DB may already include the pack: remove only imported rows in this disposable copy.
    names=[r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table'")]
    if 'app_data_migration' in names:
        ids=[r[0] for r in db.execute("SELECT id FROM station WHERE name LIKE '% · Tesla 超级充电站'")]
        for id in ids:
            db.execute('DELETE FROM price_rule WHERE station_id=?',(id,))
            db.execute('DELETE FROM pile WHERE station_id=?',(id,))
            db.execute('DELETE FROM station WHERE id=?',(id,))
        db.execute("DELETE FROM app_data_migration WHERE version='beijing_real_stations_20260908_v1'")
    db.commit()
    snapshot={t:db.execute('SELECT * FROM '+t).fetchall() for t in ('user','charge_order','charge_reservation','station','pile')}
    db.executescript(sql)
    assert db.execute('SELECT COUNT(*) FROM station').fetchone()[0]==len(snapshot['station'])+12
    assert db.execute('SELECT COUNT(*) FROM pile').fetchone()[0]==len(snapshot['pile'])+59
    for t,rows in snapshot.items():
        current=db.execute('SELECT * FROM '+t+' ORDER BY id').fetchall()
        assert current[:len(rows)]==rows, t
    counts=[db.execute('SELECT COUNT(*) FROM '+t).fetchone() for t in ('station','pile','price_rule')]
    db.executescript(sql)
    assert counts==[db.execute('SELECT COUNT(*) FROM '+t).fetchone() for t in ('station','pile','price_rule')]
    db.execute("UPDATE station SET price=2.8 WHERE name LIKE '北京清华科技园 ·%'");db.commit()
    db.executescript(sql)
    assert db.execute("SELECT price FROM station WHERE name LIKE '北京清华科技园 ·%'").fetchone()[0]==2.8
    assert db.execute('PRAGMA integrity_check').fetchone()[0]=='ok'
    print('PASS: 12 real stations / 59 piles, preserved records, repeat import and preserved admin edits')
