#include "LogDao.h"
#include "ServerDataLock.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QSqlDatabase logDb(const QString &connName)
{
    return connName.isEmpty() ? QSqlDatabase::database()
                              : QSqlDatabase::database(connName);
}

}

bool LogDao::record(const QString &opUser, const QString &action, const QString &detail,
                    QString *errMsg, const QString &connName)
{
    SERVER_WRITE_LOCK;
    QSqlQuery query(logDb(connName));
    query.prepare("INSERT INTO op_log (op_user, action, detail) VALUES (:u, :a, :d)");
    query.bindValue(":u", opUser);
    query.bindValue(":a", action);
    query.bindValue(":d", detail);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "写入日志失败: " + query.lastError().text();
        return false;
    }
    return true;
}

bool LogDao::recordReversible(const QString &opUser, const QString &action,
                              const QString &detail, const QString &undoType,
                              int targetId, const QString &beforeValue,
                              const QString &afterValue, QString *errMsg,
                              const QString &connName)
{
    SERVER_WRITE_LOCK;
    QSqlQuery q(logDb(connName));
    q.prepare("INSERT INTO op_log(op_user,action,detail,undo_type,target_id,before_value,after_value)"
              " VALUES(?,?,?,?,?,?,?)");
    q.addBindValue(opUser); q.addBindValue(action); q.addBindValue(detail);
    q.addBindValue(undoType); q.addBindValue(targetId);
    q.addBindValue(beforeValue); q.addBindValue(afterValue);
    if (!q.exec()) {
        if (errMsg) *errMsg = "写入日志失败: " + q.lastError().text();
        return false;
    }
    return true;
}

QList<QStringList> LogDao::list(int limit, const QString &connName)
{
    SERVER_READ_LOCK;
    QList<QStringList> rows;
    QSqlQuery query(logDb(connName));
    query.prepare("SELECT op_time, op_user, action, detail FROM op_log"
                  " ORDER BY id DESC LIMIT :l");
    query.bindValue(":l", limit);
    if (!query.exec())
        return rows;
    while (query.next()) {
        rows.append(QStringList{ query.value(0).toString(), query.value(1).toString(),
                                 query.value(2).toString(), query.value(3).toString() });
    }
    return rows;
}

QList<OperationLog> LogDao::search(const QString &keyword, int limit, const QString &connName)
{
    SERVER_READ_LOCK;
    QList<OperationLog> rows;
    QSqlQuery q(logDb(connName));
    q.prepare("SELECT id,op_time,op_user,action,detail,undo_type,target_id,before_value,"
              "after_value,reverted FROM op_log WHERE (?='' OR op_user LIKE ? OR action LIKE ?"
              " OR detail LIKE ?) ORDER BY id DESC LIMIT ?");
    const QString like = "%" + keyword + "%";
    q.addBindValue(keyword); q.addBindValue(like); q.addBindValue(like); q.addBindValue(like);
    q.addBindValue(qBound(1, limit, 2000));
    if (!q.exec()) return rows;
    while (q.next()) {
        OperationLog r;
        r.id=q.value(0).toInt(); r.time=q.value(1).toString(); r.user=q.value(2).toString();
        r.action=q.value(3).toString(); r.detail=q.value(4).toString();
        r.undoType=q.value(5).toString(); r.targetId=q.value(6).toInt();
        r.beforeValue=q.value(7).toString(); r.afterValue=q.value(8).toString();
        r.reverted=q.value(9).toInt()!=0; rows.append(r);
    }
    return rows;
}

bool LogDao::undo(int logId, const QString &opUser, QString *errMsg, const QString &connName)
{
    SERVER_WRITE_LOCK;
    QSqlDatabase db=logDb(connName);
    if (!db.transaction()) { if(errMsg)*errMsg=db.lastError().text(); return false; }
    QSqlQuery find(db);
    find.prepare("SELECT action,undo_type,target_id,before_value,after_value,reverted FROM op_log WHERE id=?");
    find.addBindValue(logId);
    if (!find.exec() || !find.next()) { db.rollback(); if(errMsg)*errMsg="日志不存在"; return false; }
    if (find.value(5).toInt()!=0) { db.rollback(); if(errMsg)*errMsg="该操作已经回退"; return false; }
    const QString type=find.value(1).toString();
    if (type!="user_status") { db.rollback(); if(errMsg)*errMsg="该操作不支持回退"; return false; }
    QSqlQuery change(db);
    change.prepare("UPDATE user SET status=? WHERE id=? AND status=?");
    change.addBindValue(find.value(3).toInt()); change.addBindValue(find.value(2).toInt());
    change.addBindValue(find.value(4).toInt());
    if (!change.exec() || change.numRowsAffected()!=1) {
        db.rollback(); if(errMsg)*errMsg="当前数据已发生变化，不能安全回退"; return false;
    }
    QSqlQuery mark(db); mark.prepare("UPDATE op_log SET reverted=1,reverted_at=datetime('now','localtime'),reverted_by=? WHERE id=?");
    mark.addBindValue(opUser); mark.addBindValue(logId);
    if (!mark.exec()) { db.rollback(); if(errMsg)*errMsg=mark.lastError().text(); return false; }
    QSqlQuery audit(db); audit.prepare("INSERT INTO op_log(op_user,action,detail) VALUES(?,?,?)");
    audit.addBindValue(opUser); audit.addBindValue("回退操作");
    audit.addBindValue(QString("已回退日志 #%1：%2").arg(logId).arg(find.value(0).toString()));
    if (!audit.exec() || !db.commit()) { db.rollback(); if(errMsg)*errMsg=db.lastError().text(); return false; }
    return true;
}
