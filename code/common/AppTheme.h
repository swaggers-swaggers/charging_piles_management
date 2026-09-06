#ifndef APPTHEME_H
#define APPTHEME_H
#include <QApplication>
#include <QFile>
#include <QPalette>

// 显式设置全部色组，避免桌面深色主题渗透到 viewport、Tab 和空表区域。
namespace AppTheme {
inline void apply(QApplication &app)
{
    QPalette palette;
    for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        palette.setColor(group, QPalette::Window, QColor("#F3F7F6"));
        palette.setColor(group, QPalette::Base, QColor("#FFFFFF"));
        palette.setColor(group, QPalette::AlternateBase, QColor("#F1F7F3"));
        palette.setColor(group, QPalette::Button, QColor("#FFFFFF"));
        palette.setColor(group, QPalette::WindowText, QColor("#243E33"));
        palette.setColor(group, QPalette::Text, QColor("#243E33"));
        palette.setColor(group, QPalette::ButtonText, QColor("#243E33"));
        palette.setColor(group, QPalette::Highlight, QColor("#DDEFE3"));
        palette.setColor(group, QPalette::HighlightedText, QColor("#174F35"));
        palette.setColor(group, QPalette::ToolTipBase, QColor("#FFFFFF"));
        palette.setColor(group, QPalette::ToolTipText, QColor("#243E33"));
        palette.setColor(group, QPalette::Light, QColor("#FFFFFF"));
        palette.setColor(group, QPalette::Mid, QColor("#D2E1D8"));
        palette.setColor(group, QPalette::Dark, QColor("#A0B8A9"));
        palette.setColor(group, QPalette::Shadow, QColor("#D2E1D8"));
        palette.setColor(group, QPalette::PlaceholderText, QColor("#73877C"));
    }
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#87998E"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#87998E"));
    app.setPalette(palette);
    QString stylesheet;
    for (const auto *path : {":/qss/global.qss", ":/qss/client.qss"}) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) stylesheet += QString::fromUtf8(file.readAll()) + '\n';
    }
    app.setStyleSheet(stylesheet);
}
}
#endif
