#ifndef PILEDAO_H
#define PILEDAO_H

#include "types.h"

#include <QList>
#include <QString>

// 充电桩 DAO
class PileDao
{
public:
    // 注意: list 系列保持旧签名(connName 紧随业务参数), Predictor/管理页面按此调用
    static QList<PileInfo> listAll(const QString &connName = QString());
    static QList<PileInfo> listByStation(int stationId,
                                         const QString &connName = QString());
    static PileInfo getById(int id, QString *errMsg = nullptr,
                            const QString &connName = QString());
    static bool setStatus(int id, int status, QString *errMsg = nullptr,
                          const QString &connName = QString());
    static bool restart(int id, QString *errMsg = nullptr,
                        const QString &connName = QString());
    static bool addUsage(int id, int addedMinutes, QString *errMsg = nullptr,
                         const QString &connName = QString());
    // 各状态桩数量(电桩状态页)
    static bool statusCounts(int *idle, int *inUse, int *fault,
                             QString *errMsg = nullptr,
                             const QString &connName = QString());

    // v2: 原子抢占空闲桩。仅当 status=0 时置为 1(在用),
    // 影响行数==1 才算抢到, 从数据库层面杜绝两个用户同时抢到同一桩
    static bool acquire(int pileId, QString *errMsg = nullptr,
                        const QString &connName = QString());
    // v2: 释放桩为空闲并累计使用时长
    static bool release(int pileId, int usedMinutes, QString *errMsg = nullptr,
                        const QString &connName = QString());
};

#endif // PILEDAO_H
