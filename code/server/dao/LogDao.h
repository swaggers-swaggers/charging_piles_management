#ifndef LOGDAO_H
#define LOGDAO_H

#include <QString>
#include <QStringList>

// 操作日志表(op_log)数据访问: 记录管理端关键操作, 供审计
class LogDao
{
public:
    static bool record(const QString &opUser, const QString &action, const QString &detail,
                       QString *errMsg = nullptr, const QString &connName = QString());

    // 最近 limit 条日志: [时间, 操作人, 动作, 详情]
    static QList<QStringList> list(int limit = 200, const QString &connName = QString());
};

#endif // LOGDAO_H
