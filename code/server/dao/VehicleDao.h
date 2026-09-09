#ifndef VEHICLEDAO_H
#define VEHICLEDAO_H

#include "types.h"

#include <QList>
#include <QString>

class VehicleDao
{
public:
    static QList<VehicleInfo> listByUser(int userId, QString *errMsg = nullptr,
                                         const QString &connName = QString());
    static bool save(int userId, VehicleInfo *vehicle, QString *errMsg = nullptr,
                     const QString &connName = QString());
    static bool remove(int userId, int vehicleId, QString *errMsg = nullptr,
                       const QString &connName = QString());
    static bool setDefault(int userId, int vehicleId, QString *errMsg = nullptr,
                           const QString &connName = QString());
};

#endif
