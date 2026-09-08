#ifndef VEHICLEDAO_H
#define VEHICLEDAO_H

#include "types.h"

#include <QList>
#include <QString>

// 用户车辆表数据访问
// connName: 数据库连接名, 空串表示默认连接(主线程); 网络线程传入自己的连接名
class VehicleDao
{
public:
    // 某用户的全部车辆, 默认车辆排前, 其余按创建时间倒序
    static QList<VehicleInfo> listByUser(int userId, QString *errMsg = nullptr,
                                         const QString &connName = QString());

    // 新增(v.id==0)或更新(v.id>0)车辆; isDefault==1 时先把该用户其他车辆默认标记清 0.
    // 返回保存后的车辆 id(>0 成功, <=0 失败并写 errMsg)
    static int save(int userId, const VehicleInfo &v, QString *errMsg = nullptr,
                    const QString &connName = QString());

    // 单条车辆(不存在返回 id==0 的空对象)
    static VehicleInfo getById(int vehicleId, const QString &connName = QString());

    // 删除车辆(校验归属)
    static bool remove(int vehicleId, int userId, QString *errMsg = nullptr,
                       const QString &connName = QString());
};

#endif // VEHICLEDAO_H
