#ifndef WEBCHARTWIDGET_H
#define WEBCHARTWIDGET_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QStackedLayout>
#include <QUrl>
#include <QWidget>

#ifdef CHARGING_HAS_WEBENGINE
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>
#endif

// 双端共用的 ECharts 容器。网页未就绪时只显示灰色骨架，
// 就绪后通过 JavaScript 更新 option，避免数据刷新时重复闪屏。
class WebChartWidget : public QWidget
{
public:
    explicit WebChartWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName("webChartWidget");
        setMinimumSize(220, 180);
        auto *stack = new QStackedLayout(this);
        stack->setContentsMargins(0, 0, 0, 0);
        m_placeholder = new QLabel(this);
        m_placeholder->setObjectName("chartLoadingPlaceholder");
        m_placeholder->setAlignment(Qt::AlignCenter);
        m_placeholder->setText(QStringLiteral("图表加载中…"));
        setProperty("chartReady", false);
        stack->addWidget(m_placeholder);

#ifdef CHARGING_HAS_WEBENGINE
        m_view = new QWebEngineView(this);
        m_view->setObjectName("chartWebView");
        m_view->setContextMenuPolicy(Qt::NoContextMenu);
        m_view->settings()->setAttribute(QWebEngineSettings::ShowScrollBars, false);
        m_view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
        stack->addWidget(m_view);
        stack->setCurrentWidget(m_placeholder);
        connect(m_view, &QWebEngineView::loadFinished, this, [this, stack](bool ok) {
            if (!ok) {
                m_placeholder->setText(QStringLiteral("图表网页加载失败"));
                return;
            }
            QPointer<WebChartWidget> guarded(this);
            m_view->page()->runJavaScript(QStringLiteral("typeof chart !== 'undefined'"),
                [guarded, stack](const QVariant &value) {
                if (!guarded) return;
                if (!value.toBool()) {
                    guarded->m_placeholder->setText(QStringLiteral("图表脚本加载失败"));
                    return;
                }
                guarded->m_ready = true;
                guarded->setProperty("chartReady", true);
                stack->setCurrentWidget(guarded->m_view);
                guarded->applyPendingOption();
            });
        });
        const QString html = QStringLiteral(R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<style>
*{box-sizing:border-box}html,body,#chart{width:100%;height:100%;margin:0;overflow:hidden;background:#fff}
body{font-family:"PingFang SC","Microsoft YaHei",sans-serif}
</style>
<script src="qrc:/charts/echarts.min.js"></script></head>
<body><div id="chart"></div><script>
const chart=echarts.init(document.getElementById('chart'),null,{renderer:'canvas'});
window.addEventListener('resize',()=>chart.resize());
new ResizeObserver(()=>chart.resize()).observe(document.getElementById('chart'));
</script></body></html>)HTML");
        m_view->setHtml(html, QUrl(QStringLiteral("qrc:/charts/")));
#else
        m_placeholder->setText(QStringLiteral("未安装 Qt WebEngine，无法显示网页图表"));
#endif
    }

    void setOption(const QJsonObject &option)
    {
        m_pendingOption = QString::fromUtf8(QJsonDocument(option).toJson(QJsonDocument::Compact));
        applyPendingOption();
    }

private:
    void applyPendingOption()
    {
#ifdef CHARGING_HAS_WEBENGINE
        if (!m_ready || !m_view || m_pendingOption.isEmpty()) return;
        m_view->page()->runJavaScript(
            QStringLiteral("chart.setOption(%1,true);chart.resize();").arg(m_pendingOption));
#endif
    }

    QLabel *m_placeholder = nullptr;
    QString m_pendingOption;
    bool m_ready = false;
#ifdef CHARGING_HAS_WEBENGINE
    QWebEngineView *m_view = nullptr;
#endif
};

#endif // WEBCHARTWIDGET_H
