#include "VehicleDao.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
VehicleInfo readVehicle(const QSqlQuery &q)
{
    VehicleInfo v;
    v.id = q.value(0).toInt();
    v.userId = q.value(1).toInt();
    v.plateNo = q.value(2).toString();
    v.brand = q.value(3).toString();
    v.model = q.value(4).toString();
    v.batteryCapacity = q.value(5).toDouble();
    v.isDefault = q.value(6).toInt();
    v.createTime = q.value(7).toString();
    return v;
}
} // namespace

QList<VehicleInfo> VehicleDao::listByUser(int userId, QString *errMsg, const QString &connName)
{
    QList<VehicleInfo> list;
    QSqlQuery q(QSqlDatabase::database(connName));
    q.prepare("SELECT id, user_id, plate_no, brand, model, battery_capacity, is_default, create_time "
              "FROM user_vehicle WHERE user_id=:uid ORDER BY is_default DESC, id DESC");
    q.bindValue(":uid", userId);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = "查询车辆失败: " + q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(readVehicle(q));
    return list;
}

int VehicleDao::save(int userId, const VehicleInfo &v, QString *errMsg, const QString &connName)
{
    QSqlDatabase db = QSqlDatabase::database(connName);
    if (v.plateNo.trimmed().isEmpty()) {
        if (errMsg)
            *errMsg = "车牌号不能为空";
        return -1;
    }

    if (v.isDefault == 1) {
        QSqlQuery clr(db);
        clr.prepare("UPDATE user_vehicle SET is_default=0 WHERE user_id=:uid");
        clr.bindValue(":uid", userId);
        if (!clr.exec()) {
            if (errMsg)
                *errMsg = "更新默认车辆失败: " + clr.lastError().text();
            return -1;
        }
    }

    QSqlQuery q(db);
    if (v.id == 0) {
        q.prepare("INSERT INTO user_vehicle(user_id, plate_no, brand, model, battery_capacity, is_default) "
                  "VALUES(:uid, :plate, :brand, :model, :cap, :def)");
    } else {
        q.prepare("UPDATE user_vehicle SET plate_no=:plate, brand=:brand, model=:model, "
                  "battery_capacity=:cap, is_default=:def WHERE id=:id AND user_id=:uid");
        q.bindValue(":id", v.id);
    }
    q.bindValue(":uid", userId);
    q.bindValue(":plate", v.plateNo.trimmed());
    q.bindValue(":brand", v.brand.trimmed());
    q.bindValue(":model", v.model.trimmed());
    q.bindValue(":cap", v.batteryCapacity);
    q.bindValue(":def", v.isDefault ? 1 : 0);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = "保存车辆失败: " + q.lastError().text();
        return -1;
    }
    return v.id == 0 ? q.lastInsertId().toInt() : v.id;
}

VehicleInfo VehicleDao::getById(int vehicleId, const QString &connName)
{
    VehicleInfo v;
    QSqlQuery q(QSqlDatabase::database(connName));
    q.prepare("SELECT id, user_id, plate_no, brand, model, battery_capacity, is_default, create_time "
              "FROM user_vehicle WHERE id=:id");
    q.bindValue(":id", vehicleId);
    if (q.exec() && q.next())
        v = readVehicle(q);
    return v;
}

bool VehicleDao::remove(int vehicleId, int userId, QString *errMsg, const QString &connName)
{
    QSqlQuery q(QSqlDatabase::database(connName));
    q.prepare("DELETE FROM user_vehicle WHERE id=:id AND user_id=:uid");
    q.bindValue(":id", vehicleId);
    q.bindValue(":uid", userId);
    if (!q.exec()) {
        if (errMsg)
            *errMsg = "删除车辆失败: " + q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() <= 0) {
        if (errMsg)
            *errMsg = "车辆不存在";
        return false;
    }
    return true;
}
