#ifndef PILEDAO_H
#define PILEDAO_H

#include "types.h"

#include <QString>

// 充电桩表数据访问
class PileDao
{
public:
    // 全部电桩(联表带所属电站名)
    static QList<PileInfo> listAll(const QString &connName = QString());

    // 某电站的全部电桩
    static QList<PileInfo> listByStation(int stationId, const QString &connName = QString());

    static bool getById(int pileId, PileInfo *out, QString *errMsg = nullptr,
                        const QString &connName = QString());

    static bool setStatus(int pileId, int status, QString *errMsg = nullptr,
                          const QString &connName = QString());

    // 远程重启(模拟): 在用中的桩拒绝重启; 闲置/故障的桩重启后恢复闲置
    static bool restart(int pileId, QString *errMsg = nullptr, const QString &connName = QString());

    // 累计充电次数 +1, 累计时长 +durationMinutes
    static bool addUsage(int pileId, int durationMinutes, QString *errMsg = nullptr,
                         const QString &connName = QString());

    // 状态数量统计
    static bool statusCounts(int *idle, int *inUse, int *fault, QString *errMsg = nullptr,
                             const QString &connName = QString());
};

#endif // PILEDAO_H
