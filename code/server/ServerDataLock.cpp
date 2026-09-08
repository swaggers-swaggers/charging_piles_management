#include "ServerDataLock.h"

#include <QReadWriteLock>
#include <QtGlobal>

namespace {
QReadWriteLock &globalLock()
{
    static QReadWriteLock lock;
    return lock;
}

thread_local int readDepth = 0;
thread_local int writeDepth = 0;
}

ServerDataLock::ServerDataLock(Mode mode) : m_mode(mode)
{
    if (writeDepth > 0) {
        ++writeDepth;
        return;
    }
    if (mode == Read) {
        if (readDepth++ == 0) {
            globalLock().lockForRead();
            m_acquired = true;
        }
        return;
    }

    // 兼容未来出现的“读取后决定写入”嵌套调用。先释放本线程的共享锁再
    // 取得写锁，避免两个读线程同时升级而互相等待。
    if (readDepth > 0) {
        globalLock().unlock();
        m_upgraded = true;
    }
    globalLock().lockForWrite();
    writeDepth = 1;
    m_acquired = true;
}

ServerDataLock::~ServerDataLock()
{
    if (writeDepth > 0) {
        if (--writeDepth == 0 && m_acquired) {
            globalLock().unlock();
            if (m_upgraded)
                globalLock().lockForRead();
        }
        return;
    }
    if (m_mode == Read && --readDepth == 0 && m_acquired)
        globalLock().unlock();
}
