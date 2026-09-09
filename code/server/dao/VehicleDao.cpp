#include "VehicleDao.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QSqlDatabase connection(const QString &name)
{
    return name.isEmpty() ? QSqlDatabase::database() : QSqlDatabase::database(name);
}

VehicleInfo readVehicle(const QSqlQuery &q)
{
    VehicleInfo v;
    v.id = q.value(0).toInt();
    v.userId = q.value(1).toInt();
    v.plateNumber = q.value(2).toString();
    v.brandModel = q.value(3).toString();
    v.energyType = q.value(4).toString();
    v.batteryCapacity = q.value(5).toDouble();
    v.isDefault = q.value(6).toInt() != 0;
    v.createTime = q.value(7).toString();
    return v;
}

bool rollback(QSqlDatabase &db, QString *errMsg, const QString &message)
{
    db.rollback();
    if (errMsg) *errMsg = message;
    return false;
}
}

QList<VehicleInfo> VehicleDao::listByUser(int userId, QString *errMsg,
                                           const QString &connName)
{
    QList<VehicleInfo> vehicles;
    QSqlQuery q(connection(connName));
    q.prepare("SELECT id,user_id,plate_number,brand_model,energy_type,battery_capacity,"
              "is_default,create_time FROM user_vehicle WHERE user_id=? "
              "ORDER BY is_default DESC,id DESC");
    q.addBindValue(userId);
    if (!q.exec()) {
        if (errMsg) *errMsg = "查询车辆失败: " + q.lastError().text();
        return vehicles;
    }
    while (q.next()) vehicles.append(readVehicle(q));
    return vehicles;
}

bool VehicleDao::save(int userId, VehicleInfo *v, QString *errMsg,
                      const QString &connName)
{
    if (!v || userId <= 0) {
        if (errMsg) *errMsg = "车辆参数错误";
        return false;
    }
    QSqlDatabase db = connection(connName);
    if (!db.transaction()) {
        if (errMsg) *errMsg = "无法开始车辆保存事务";
        return false;
    }
    QSqlQuery duplicate(db);
    duplicate.prepare("SELECT 1 FROM user_vehicle WHERE user_id=? AND plate_number=? AND id<>?");
    duplicate.addBindValue(userId);
    duplicate.addBindValue(v->plateNumber);
    duplicate.addBindValue(v->id);
    if (!duplicate.exec() || duplicate.next())
        return rollback(db, errMsg, duplicate.lastError().isValid()
            ? "检查车牌失败: " + duplicate.lastError().text() : "该车牌已经添加");
    if (v->id > 0) {
        QSqlQuery q(db);
        q.prepare("UPDATE user_vehicle SET plate_number=?,brand_model=?,energy_type=?,"
                  "battery_capacity=? WHERE id=? AND user_id=?");
        q.addBindValue(v->plateNumber); q.addBindValue(v->brandModel);
        q.addBindValue(v->energyType); q.addBindValue(v->batteryCapacity);
        q.addBindValue(v->id); q.addBindValue(userId);
        if (!q.exec() || q.numRowsAffected() != 1)
            return rollback(db, errMsg, q.lastError().nativeErrorCode() == "2067"
                ? "该车牌已经添加" : "车辆不存在或保存失败: " + q.lastError().text());
    } else {
        QSqlQuery count(db);
        count.prepare("SELECT COUNT(*) FROM user_vehicle WHERE user_id=?");
        count.addBindValue(userId);
        if (!count.exec() || !count.next())
            return rollback(db, errMsg, "读取车辆数量失败");
        v->isDefault = count.value(0).toInt() == 0;
        QSqlQuery q(db);
        q.prepare("INSERT INTO user_vehicle(user_id,plate_number,brand_model,energy_type,"
                  "battery_capacity,is_default) VALUES(?,?,?,?,?,?)");
        q.addBindValue(userId); q.addBindValue(v->plateNumber);
        q.addBindValue(v->brandModel); q.addBindValue(v->energyType);
        q.addBindValue(v->batteryCapacity); q.addBindValue(v->isDefault ? 1 : 0);
        if (!q.exec())
            return rollback(db, errMsg, q.lastError().nativeErrorCode() == "2067"
                ? "该车牌已经添加" : "添加车辆失败: " + q.lastError().text());
        v->id = q.lastInsertId().toInt();
    }
    if (!db.commit()) return rollback(db, errMsg, "保存车辆失败: " + db.lastError().text());
    v->userId = userId;
    return true;
}

bool VehicleDao::remove(int userId, int vehicleId, QString *errMsg,
                        const QString &connName)
{
    QSqlDatabase db = connection(connName);
    if (!db.transaction()) { if (errMsg) *errMsg = "无法开始删除事务"; return false; }
    QSqlQuery find(db);
    find.prepare("SELECT is_default FROM user_vehicle WHERE id=? AND user_id=?");
    find.addBindValue(vehicleId); find.addBindValue(userId);
    if (!find.exec() || !find.next()) return rollback(db, errMsg, "车辆不存在");
    const bool wasDefault = find.value(0).toBool();
    QSqlQuery del(db);
    del.prepare("DELETE FROM user_vehicle WHERE id=? AND user_id=?");
    del.addBindValue(vehicleId); del.addBindValue(userId);
    if (!del.exec() || del.numRowsAffected() != 1)
        return rollback(db, errMsg, "删除车辆失败: " + del.lastError().text());
    if (wasDefault) {
        QSqlQuery next(db);
        next.prepare("UPDATE user_vehicle SET is_default=1 WHERE id=(SELECT id FROM "
                     "user_vehicle WHERE user_id=? ORDER BY id DESC LIMIT 1)");
        next.addBindValue(userId);
        if (!next.exec()) return rollback(db, errMsg, "更新默认车辆失败");
    }
    if (!db.commit()) return rollback(db, errMsg, "删除车辆失败");
    return true;
}

bool VehicleDao::setDefault(int userId, int vehicleId, QString *errMsg,
                            const QString &connName)
{
    QSqlDatabase db = connection(connName);
    if (!db.transaction()) { if (errMsg) *errMsg = "无法开始设置事务"; return false; }
    QSqlQuery exists(db);
    exists.prepare("SELECT 1 FROM user_vehicle WHERE id=? AND user_id=?");
    exists.addBindValue(vehicleId); exists.addBindValue(userId);
    if (!exists.exec() || !exists.next()) return rollback(db, errMsg, "车辆不存在");
    QSqlQuery clear(db);
    clear.prepare("UPDATE user_vehicle SET is_default=0 WHERE user_id=?");
    clear.addBindValue(userId);
    if (!clear.exec()) return rollback(db, errMsg, "更新默认车辆失败");
    QSqlQuery set(db);
    set.prepare("UPDATE user_vehicle SET is_default=1 WHERE id=? AND user_id=?");
    set.addBindValue(vehicleId); set.addBindValue(userId);
    if (!set.exec() || set.numRowsAffected() != 1)
        return rollback(db, errMsg, "更新默认车辆失败");
    if (!db.commit()) return rollback(db, errMsg, "更新默认车辆失败");
    return true;
}
