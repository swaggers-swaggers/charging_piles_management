#ifndef STATIONDAO_H
#define STATIONDAO_H

#include "types.h"

#include <QString>

// 充电站表数据访问
class StationDao
{
public:
    // 全部充电站, 附带 totalPiles / idlePiles 统计(子查询)
    static QList<StationInfo> list(const QString &connName = QString());

    // 新增电站并按数量生成电桩(快慢交替, 编号顺延), inOut 传入名称/地址/经纬度/电价, 返回时填充 id
    static bool add(StationInfo *inOut, int pileCount,
                    QString *errMsg = nullptr, const QString &connName = QString());
};

#endif // STATIONDAO_H
