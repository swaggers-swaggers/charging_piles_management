#include "LogDao.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool LogDao::record(const QString &opUser, const QString &action, const QString &detail,
                    QString *errMsg, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
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

QList<QStringList> LogDao::list(int limit, const QString &connName)
{
    QList<QStringList> rows;
    QSqlQuery query(QSqlDatabase::database(connName));
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
