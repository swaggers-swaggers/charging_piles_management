#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QMutex>
#include <QString>

class QFile;

// 服务端系统运行日志: 只写文件 + 控制台, 不接入管理端 GUI。
// 通过 qInstallMessageHandler 全局接管 qDebug/qInfo/qWarning/qCritical,
// 现有代码中带 [模块] 前缀的日志自动全部落盘, 无需逐处修改。
//
// 输出:
//   - 文件: <可执行文件目录>/logs/server-YYYYMMDD.log, 按天滚动;
//     单文件超 10MB 自动续号(server-YYYYMMDD-1.log ...);
//   - 控制台: 同步打印到 stderr, 后端终端实时可见。
// 级别过滤: 环境变量 CHARGING_LOG_LEVEL = DEBUG / INFO / WARN / ERROR, 默认 INFO。
// 清理: 启动时自动删除 7 天前的旧日志文件。
class LogManager
{
public:
    static void init();      // main 中 QApplication 创建后第一件事调用
    static void shutdown();  // 退出前调用(刷新并关闭文件)

private:
    static void handle(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);
    static QString logDir();
    static QString dayString();
    static void rollIfNeeded();   // 日期变化或超 10MB 时滚动文件(须持锁调用)
    static void cleanupOld();

    static QMutex s_mutex;
    static QFile *s_file;    // 当前日志文件(持锁访问)
    static QString s_day;    // 当前文件日期 yyyyMMdd
    static int s_volume;     // 当前滚动卷号
    static int s_minLevel;   // 0=DEBUG 1=INFO 2=WARN 3=ERROR
};

#endif // LOGMANAGER_H
