#ifndef USERDAO_H
#define USERDAO_H

#include "types.h"

#include <QList>
#include <QString>

// 用户表数据访问
// connName: 数据库连接名, 空串表示默认连接(主线程); 网络线程传入自己的连接名
class UserDao
{
public:
    // 用户列表, search 非空时按手机号/昵称模糊过滤
    static QList<UserInfo> list(const QString &search = QString(),
                                const QString &connName = QString());

    static bool getById(int userId, UserInfo *out, QString *errMsg = nullptr,
                        const QString &connName = QString());

    // 冻结(1) / 解冻(0)
    static bool setStatus(int userId, int status, QString *errMsg = nullptr,
                          const QString &connName = QString());

    // 修改资料: nickname/avatar 传空串表示不修改该项
    static bool updateProfile(int userId, const QString &nickname, const QString &avatar,
                              QString *errMsg = nullptr, const QString &connName = QString());

    // 充值(余额累加)
    static bool recharge(int userId, double amount, double *newBalance = nullptr,
                         QString *errMsg = nullptr, const QString &connName = QString());

    // 余额增减(delta 可为负), 用于结算扣款
    static bool adjustBalance(int userId, double delta, QString *errMsg = nullptr,
                              const QString &connName = QString());
};

#endif // USERDAO_H
