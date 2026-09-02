#ifndef DATAEXPORTER_H
#define DATAEXPORTER_H

#include <QObject>

// 大屏数据导出: 主线程定时(10秒)把经营数据写入 web/data.json,
// 供 ECharts 大屏页面轮询展示
class DataExporter : public QObject
{
    Q_OBJECT

public:
    explicit DataExporter(QObject *parent = nullptr);

    // 导出目录: 环境变量 CHARGING_WEB_DIR > 工作目录 web/ > 可执行文件目录 web/
    static QString exportDir();

public slots:
    void exportNow();

private:
    QString resolveExportPath() const;
};

#endif // DATAEXPORTER_H
