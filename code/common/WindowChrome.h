#ifndef WINDOWCHROME_H
#define WINDOWCHROME_H
#include <QMainWindow>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>
#include <QSizeGrip>

namespace UiMotion {
// Paint the shadow directly: no graphics effect on a WebEngine ancestor.
class WindowBackdrop : public QWidget {
public:
    explicit WindowBackdrop(QWidget *w) : QWidget(w) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        w->installEventFilter(this); setGeometry(w->rect()); lower(); show();
    }
protected:
    bool eventFilter(QObject *, QEvent *e) override {
        if (e->type()==QEvent::Resize) setGeometry(parentWidget()->rect());
        return false;
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setPen(Qt::NoPen);
        for (int i=12;i>0;--i) {
            p.setBrush(QColor(24,55,43,2+(12-i)/3));
            p.drawRoundedRect(QRectF(rect()).adjusted(i,i,i*-1,i*-1),20,20);
        }
        p.setBrush(QColor("#F3F7F6"));
        p.drawRoundedRect(QRectF(rect()).adjusted(12,12,-12,-12),16,16);
    }
};
class TitleBar : public QWidget {
public:
    explicit TitleBar(QWidget *w) : QWidget(w), host(w) {
        setObjectName("customTitleBar"); setFixedHeight(42);
        auto *row=new QHBoxLayout(this); row->setContentsMargins(14,0,6,0);
        auto *title=new QLabel(w->windowTitle(),this); title->setAttribute(Qt::WA_TransparentForMouseEvents);
        row->addWidget(title,1);
        connect(w,&QWidget::windowTitleChanged,title,&QLabel::setText);
        auto add=[&](const QString &text,const QString &name) {
            auto *b=new QPushButton(text,this); b->setObjectName(name); b->setFixedSize(32,28);
            b->setFocusPolicy(Qt::NoFocus); b->setAutoDefault(false);
            b->setStyleSheet("QPushButton{padding:0;min-height:0;border:none;background:transparent;} QPushButton:hover{background:#DDEFE3;}");
            row->addWidget(b); return b;
        };
        auto *min=add(QStringLiteral("−"),"windowMinimize"); min->setToolTip("最小化");
        connect(min,&QPushButton::clicked,w,&QWidget::showMinimized);
        if (qobject_cast<QMainWindow*>(w)) {
            auto *max=add(QStringLiteral("□"),"windowMaximize"); max->setToolTip("最大化 / 还原");
            connect(max,&QPushButton::clicked,this,[this]{toggle();});
        }
        auto *close=add(QStringLiteral("×"),"windowClose"); close->setToolTip("关闭");
        connect(close,&QPushButton::clicked,w,&QWidget::close);
    }
protected:
    void mousePressEvent(QMouseEvent *e) override {
        if(e->button()==Qt::LeftButton) {
            offset=e->globalPos()-host->frameGeometry().topLeft();
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
            if(host->windowHandle() && host->windowHandle()->startSystemMove()) return;
#endif
            dragging=true;
        }
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if(dragging && (e->buttons() & Qt::LeftButton) && !host->isMaximized()) host->move(e->globalPos()-offset);
    }
    void mouseReleaseEvent(QMouseEvent *) override {dragging=false;}
    void mouseDoubleClickEvent(QMouseEvent *e) override {
        if(e->button()==Qt::LeftButton && qobject_cast<QMainWindow*>(host)) toggle();
    }
private:
    void toggle(){host->isMaximized()?host->showNormal():host->showMaximized();}
    QWidget *host; QPoint offset; bool dragging=false;
};
inline void installChrome(QWidget *w) {
    // WebEngine 窗口使用系统不透明边框，避免透明顶层与 GPU 合成产生黑边。
    if (w->property("nativeWindowChrome").toBool()) return;
    if (!w->isWindow() || w->property("chromeInstalled").toBool()) return;
    if (!qobject_cast<QMainWindow*>(w) && !qobject_cast<QDialog*>(w)) return;
    w->setProperty("chromeInstalled",true);
    w->setWindowFlag(Qt::FramelessWindowHint);
    w->setAttribute(Qt::WA_TranslucentBackground);
    w->setContentsMargins(12,12,12,12);
    new WindowBackdrop(w);
    auto *title=new TitleBar(w);
    if(auto *main=qobject_cast<QMainWindow*>(w)) main->setMenuWidget(title);
    else {
        auto *body=new QWidget(w);
        if(w->layout()) body->setLayout(w->layout());
        auto *layout=new QVBoxLayout(w); layout->setContentsMargins(0,0,0,6); layout->setSpacing(0);
        layout->addWidget(title); layout->addWidget(body,1);
        layout->addWidget(new QSizeGrip(w),0,Qt::AlignRight);
    }
}
}
#endif
