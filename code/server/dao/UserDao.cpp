#include "UserDao.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static UserInfo readUser(const QSqlQuery &q)
{
    UserInfo u;
    u.id = q.value(0).toInt();
    u.phone = q.value(1).toString();
    u.nickname = q.value(2).toString();
    u.avatar = q.value(3).toString();
    u.balance = q.value(4).toDouble();
    u.status = q.value(5).toInt();
    u.registerTime = q.value(6).toString();
    return u;
}

QList<UserInfo> UserDao::list(const QString &search, const QString &connName)
{
    QList<UserInfo> users;
    QSqlQuery query(QSqlDatabase::database(connName));
    QString sql = "SELECT id, phone, nickname, avatar, balance, status, register_time FROM user";
    if (!search.isEmpty()) {
        sql += " WHERE phone LIKE :s OR nickname LIKE :s2";
    }
    sql += " ORDER BY id";

    query.prepare(sql);
    if (!search.isEmpty()) {
        const QString pattern = "%" + search + "%";
        query.bindValue(":s", pattern);
        query.bindValue(":s2", pattern);
    }
    if (!query.exec())
        return users;
    while (query.next())
        users.append(readUser(query));
    return users;
}

bool UserDao::getById(int userId, UserInfo *out, QString *errMsg, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare("SELECT id, phone, nickname, avatar, balance, status, register_time "
                  "FROM user WHERE id = :id");
    query.bindValue(":id", userId);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "查询用户失败: " + query.lastError().text();
        return false;
    }
    if (!query.next()) {
        if (errMsg)
            *errMsg = "用户不存在";
        return false;
    }
    if (out)
        *out = readUser(query);
    return true;
}

bool UserDao::setStatus(int userId, int status, QString *errMsg, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare("UPDATE user SET status = :s WHERE id = :id");
    query.bindValue(":s", status);
    query.bindValue(":id", userId);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "更新用户状态失败: " + query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool UserDao::updateProfile(int userId, const QString &nickname, const QString &avatar,
                            QString *errMsg, const QString &connName)
{
    QStringList sets;
    if (!nickname.isEmpty())
        sets << "nickname = :n";
    if (!avatar.isEmpty())
        sets << "avatar = :a";
    if (sets.isEmpty())
        return true;   // 没有需要修改的字段

    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare("UPDATE user SET " + sets.join(", ") + " WHERE id = :id");
    if (!nickname.isEmpty())
        query.bindValue(":n", nickname);
    if (!avatar.isEmpty())
        query.bindValue(":a", avatar);
    query.bindValue(":id", userId);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "更新用户资料失败: " + query.lastError().text();
        return false;
    }
    return true;
}

bool UserDao::recharge(int userId, double amount, double *newBalance,
                       QString *errMsg, const QString &connName)
{
    QSqlDatabase db = QSqlDatabase::database(connName);
    QSqlQuery query(db);
    query.prepare("UPDATE user SET balance = balance + :a WHERE id = :id");
    query.bindValue(":a", amount);
    query.bindValue(":id", userId);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "充值失败: " + query.lastError().text();
        return false;
    }
    if (query.numRowsAffected() <= 0) {
        if (errMsg)
            *errMsg = "用户不存在";
        return false;
    }
    if (newBalance) {
        QSqlQuery q2(db);
        q2.prepare("SELECT balance FROM user WHERE id = :id");
        q2.bindValue(":id", userId);
        if (q2.exec() && q2.next())
            *newBalance = q2.value(0).toDouble();
    }
    return true;
}

bool UserDao::adjustBalance(int userId, double delta, QString *errMsg, const QString &connName)
{
    QSqlQuery query(QSqlDatabase::database(connName));
    query.prepare("UPDATE user SET balance = balance + :d WHERE id = :id");
    query.bindValue(":d", delta);
    query.bindValue(":id", userId);
    if (!query.exec()) {
        if (errMsg)
            *errMsg = "更新余额失败: " + query.lastError().text();
        return false;
    }
    return true;
}
