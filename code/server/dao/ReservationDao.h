#ifndef RESERVATIONDAO_H
#define RESERVATIONDAO_H

#include "types.h"

#include <QList>
#include <QString>

// 排队 + 预约 DAO(charge_reservation, type: 0=现场排队 1=时段预约)
class ReservationDao
{
public:
    // ---- 现场排队 ----
    // 创建排队; 返回记录 id; -1=DB错误; -2=已在排队(ErrQueueExists); -3=排队已满(ErrQueueFull)
    static int enqueue(int userId, int pileId, int stationId,
                       QString *errMsg = nullptr, const QString &connName = QString());
    // 用户取消自己的排队/预约
    static bool cancelByUser(int reservationId, int userId,
                             QString *errMsg = nullptr, const QString &connName = QString());
    // 管理端取消
    static bool cancelByAdmin(int reservationId,
                              QString *errMsg = nullptr, const QString &connName = QString());
    static int queuePosition(int reservationId,
                             QString *errMsg = nullptr, const QString &connName = QString());
    static int pendingCount(int pileId,
                            QString *errMsg = nullptr, const QString &connName = QString());
    // 该桩最早的有效排队(status=0, type=0)
    static ReservationInfo nextPending(int pileId,
                                       QString *errMsg = nullptr,
                                       const QString &connName = QString());
    // 置为已分配待确认(status=1), 记录分配时间与过期时间(now+confirmSec 真实秒)
    static bool markAssigned(int reservationId, int confirmSec,
                             QString *errMsg = nullptr, const QString &connName = QString());
    // 用户确认后转充电, 排队记录置为已履约
    static bool markFulfilled(int reservationId,
                              QString *errMsg = nullptr, const QString &connName = QString());
    static bool setStatus(int reservationId, int status,
                          QString *errMsg = nullptr, const QString &connName = QString());

    // ---- 时段预约 ----
    // 创建预约; 返回 id; -1=DB错误; -2=时段冲突(ErrSlotConflict);
    // -3=时段已过(ErrSlotPast); -4=已有有效预约(ErrQueueExists)
    static int appointCreate(int userId, int pileId, int stationId,
                             const QString &date, const QString &start, const QString &end,
                             QString *errMsg = nullptr, const QString &connName = QString());
    // 某日某桩已被占用的预约时段(type=1, status in 0/1)
    static QList<ReservationInfo> bookedSlots(int pileId, const QString &date,
                                              QString *errMsg = nullptr,
                                              const QString &connName = QString());
    // 用户到桩开始充电: 将其该桩今日有效预约置为已履约, 返回命中的记录(可能为空)
    static ReservationInfo fulfillTodayAppoint(int userId, int pileId,
                                               QString *errMsg = nullptr,
                                               const QString &connName = QString());

    // ---- 查询 ----
    static QList<ReservationInfo> myList(int userId,
                                         QString *errMsg = nullptr,
                                         const QString &connName = QString());
    // 管理端: statusFilter=-1 不限; typeFilter=-1 不限
    static QList<ReservationInfo> listAll(int statusFilter, int typeFilter,
                                          QString *errMsg = nullptr,
                                          const QString &connName = QString());
    static ReservationInfo getById(int id, QString *errMsg = nullptr,
                                   const QString &connName = QString());

    // ---- 引擎周期扫描 ----
    // 已分配但超过确认时限的排队记录(引擎置过期后继续分配下一位)
    static QList<ReservationInfo> listAssignedExpired(QString *errMsg = nullptr,
                                                      const QString &connName = QString());
    // 到达"开始前10分钟"提醒窗口且未提醒的预约
    static QList<ReservationInfo> listAppointRemindDue(QString *errMsg = nullptr,
                                                       const QString &connName = QString());
    // 超过结束宽限期仍未履约的预约
    static QList<ReservationInfo> listAppointExpired(QString *errMsg = nullptr,
                                                     const QString &connName = QString());
    static bool markRemindSent(int reservationId,
                               QString *errMsg = nullptr, const QString &connName = QString());
};

#endif // RESERVATIONDAO_H
