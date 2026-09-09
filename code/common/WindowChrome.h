#ifndef WINDOWCHROME_H
#define WINDOWCHROME_H
#include <QMainWindow>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPushButton>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QWindow>
#include <QSizeGrip>
#include <QShortcut>
#include <QTimer>

namespace UiMotion {
// Paint the shadow directly: no graphics effect on a WebEngine ancestor.
class WindowBackdrop : public QWidget {
public:
    explicit WindowBackdrop(QWidget *w) : QWidget(w) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        w->installEventFilter(this); setGeometry(w->rect()); updateWindowMask(); lower(); show();
    }
protected:
    bool eventFilter(QObject *, QEvent *e) override {
        if (e->type()==QEvent::Resize || e->type()==QEvent::WindowStateChange) {
            setGeometry(parentWidget()->rect());
            updateWindowMask();
        }
        return false;
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen);
        QLinearGradient background(rect().topLeft(),rect().bottomRight());
        background.setColorAt(0.00,QColor("#8EA487"));
        background.setColorAt(0.17,QColor("#FFFFFF"));
        background.setColorAt(0.43,QColor("#DDD5B9"));
        background.setColorAt(0.68,QColor("#FFFFFF"));
        background.setColorAt(1.00,QColor("#5E8A58"));
        p.setBrush(background);
        QWidget *host = parentWidget();
        if (host && (host->isMaximized() || host->isFullScreen()))
            p.drawRect(rect());
        else
            p.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),14,14);
    }
private:
    void updateWindowMask() {
        QWidget *host = parentWidget();
        if (!host) return;
        if (host->isMaximized() || host->isFullScreen()) {
            host->clearMask();
            return;
        }
        QPainterPath path;
        path.addRoundedRect(QRectF(host->rect()), 14, 14);
        host->setMask(QRegion(path.toFillPolygon().toPolygon()));
    }
};
class TitleBar : public QWidget {
public:
    explicit TitleBar(QWidget *w) : QWidget(w), host(w) {
        setObjectName("customTitleBar"); setFixedHeight(34);
        auto *row=new QHBoxLayout(this); row->setContentsMargins(14,0,6,0);
        auto *title=new QLabel(w->windowTitle(),this); title->setAttribute(Qt::WA_TransparentForMouseEvents);
        row->addWidget(title,1);
        connect(w,&QWidget::windowTitleChanged,title,&QLabel::setText);
        auto add=[&](const QString &text,const QString &name) {
            auto *b=new QPushButton(text,this); b->setObjectName(name); b->setFixedSize(28,24);
            b->setFocusPolicy(Qt::NoFocus); b->setAutoDefault(false);
            b->setStyleSheet("QPushButton{padding:0;min-height:0;border:none;background:transparent;} QPushButton:hover{background:#DDEFE3;}");
            row->addWidget(b); return b;
        };
        auto *min=add(QStringLiteral("−"),"windowMinimize"); min->setToolTip("最小化");
        connect(min,&QPushButton::clicked,w,&QWidget::showMinimized);
        if (qobject_cast<QMainWindow*>(w)) {
            m_maxButton=add(QStringLiteral("□"),"windowMaximize"); m_maxButton->setToolTip("最大化 / 还原");
            connect(m_maxButton,&QPushButton::clicked,this,[this]{toggle();});
        }
        auto *close=add(QStringLiteral("×"),"windowClose"); close->setToolTip("关闭");
        connect(close,&QPushButton::clicked,w,&QWidget::close);
        host->installEventFilter(this);
        updateWindowState();
    }
protected:
    bool eventFilter(QObject *object, QEvent *event) override {
        if (object==host && (event->type()==QEvent::WindowStateChange
                            || event->type()==QEvent::Show))
            QTimer::singleShot(0,this,[this]{updateWindowState();});
        return QWidget::eventFilter(object,event);
    }
    void mousePressEvent(QMouseEvent *e) override {
        if(e->button()==Qt::LeftButton) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            offset=e->globalPosition().toPoint()-host->frameGeometry().topLeft();
#else
            offset=e->globalPos()-host->frameGeometry().topLeft();
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
            if(host->windowHandle() && host->windowHandle()->startSystemMove()) return;
#endif
            dragging=true;
        }
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if(dragging && (e->buttons() & Qt::LeftButton) && !host->isMaximized() && !host->isFullScreen()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            host->move(e->globalPosition().toPoint()-offset);
#else
            host->move(e->globalPos()-offset);
#endif
        }
    }
    void mouseReleaseEvent(QMouseEvent *) override {dragging=false;}
    void mouseDoubleClickEvent(QMouseEvent *e) override {
        if(e->button()==Qt::LeftButton && qobject_cast<QMainWindow*>(host)) toggle();
    }
private:
    void toggle(){
        if(host->isFullScreen()) {
            host->property("windowWasMaximizedBeforeFullScreen").toBool()
                ? host->showMaximized() : host->showNormal();
        } else {
            host->isMaximized()?host->showNormal():host->showMaximized();
        }
    }
    void updateWindowState() {
        const bool fullScreen=host->isFullScreen();
        setVisible(!fullScreen);
        if(m_maxButton) m_maxButton->setText(host->isMaximized()?QStringLiteral("❐"):QStringLiteral("□"));
    }
    QWidget *host; QPushButton *m_maxButton=nullptr; QPoint offset; bool dragging=false;
};
inline void installChrome(QWidget *w) {
    // WebEngine 窗口使用系统不透明边框，避免透明顶层与 GPU 合成产生黑边。
    if (w->property("nativeWindowChrome").toBool()) return;
    if (!w->isWindow() || w->property("chromeInstalled").toBool()) return;
    if (!qobject_cast<QMainWindow*>(w) && !qobject_cast<QDialog*>(w)) return;
    w->setProperty("chromeInstalled",true);
    w->setWindowFlag(Qt::FramelessWindowHint);
    // 透明区域只保留在四个圆角，不再预留外围阴影带。
    w->setAttribute(Qt::WA_TranslucentBackground);
    w->setContentsMargins(0,0,0,0);
    new WindowBackdrop(w);
    auto *title=new TitleBar(w);
    if(auto *main=qobject_cast<QMainWindow*>(w)) main->setMenuWidget(title);
    else {
        auto *body=new QWidget(w);
        if(w->layout()) body->setLayout(w->layout());
        auto *layout=new QVBoxLayout(w); layout->setContentsMargins(0,0,0,0); layout->setSpacing(0);
        layout->addWidget(title); layout->addWidget(body,1);
        layout->addWidget(new QSizeGrip(w),0,Qt::AlignRight);
    }
    auto *fullScreenShortcut=new QShortcut(QKeySequence(Qt::Key_F11),w);
    fullScreenShortcut->setObjectName("windowFullScreenShortcut");
    QObject::connect(fullScreenShortcut,&QShortcut::activated,w,[w]{
        if(w->isFullScreen()) {
            w->property("windowWasMaximizedBeforeFullScreen").toBool()
                ? w->showMaximized() : w->showNormal();
        } else {
            w->setProperty("windowWasMaximizedBeforeFullScreen",w->isMaximized());
            w->showFullScreen();
        }
    });
    auto *escapeShortcut=new QShortcut(QKeySequence(Qt::Key_Escape),w);
    escapeShortcut->setObjectName("windowFullScreenEscapeShortcut");
    QObject::connect(escapeShortcut,&QShortcut::activated,w,[w]{
        if(w->isFullScreen()) {
            w->property("windowWasMaximizedBeforeFullScreen").toBool()
                ? w->showMaximized() : w->showNormal();
        }
    });
}
}
#endif
