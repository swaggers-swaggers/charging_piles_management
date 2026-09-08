#ifndef SERVERDATALOCK_H
#define SERVERDATALOCK_H

// 服务端进程内统一数据锁。读操作可并行，写操作及事务独占。
// 支持同一线程中的嵌套调用，例如写事务内继续调用多个 DAO。
class ServerDataLock
{
public:
    enum Mode { Read, Write };
    explicit ServerDataLock(Mode mode);
    ~ServerDataLock();

    ServerDataLock(const ServerDataLock &) = delete;
    ServerDataLock &operator=(const ServerDataLock &) = delete;

private:
    Mode m_mode;
    bool m_acquired = false;
    bool m_upgraded = false;
};

#define SERVER_READ_LOCK ServerDataLock serverDataReadLock(ServerDataLock::Read)
#define SERVER_WRITE_LOCK ServerDataLock serverDataWriteLock(ServerDataLock::Write)

#endif
