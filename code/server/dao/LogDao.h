#ifndef LOGDAO_H
#define LOGDAO_H

#include <QString>
#include <QStringList>
#include <QList>

struct OperationLog {
    int id = 0;
    QString time;
    QString user;
    QString action;
    QString detail;
    QString undoType;
    int targetId = 0;
    QString beforeValue;
    QString afterValue;
    bool reverted = false;
};

// 操作日志表(op_log)数据访问: 记录管理端关键操作, 供审计
class LogDao
{
public:
    static bool record(const QString &opUser, const QString &action, const QString &detail,
                       QString *errMsg = nullptr, const QString &connName = QString());
    static bool recordReversible(const QString &opUser, const QString &action,
                                 const QString &detail, const QString &undoType,
                                 int targetId, const QString &beforeValue,
                                 const QString &afterValue, QString *errMsg = nullptr,
                                 const QString &connName = QString());

    // 最近 limit 条日志: [时间, 操作人, 动作, 详情]
    static QList<QStringList> list(int limit = 200, const QString &connName = QString());
    static QList<OperationLog> search(const QString &keyword = QString(), int limit = 500,
                                      const QString &connName = QString());
    static bool undo(int logId, const QString &opUser, QString *errMsg = nullptr,
                     const QString &connName = QString());
};

#endif // LOGDAO_H
