#include "UserManagePage.h"

#include "LogDao.h"
#include "ServerSession.h"
#include "UserDao.h"

#include <QBrush>
#include <QColor>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

UserManagePage::UserManagePage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("用户管理", this);
    title->setObjectName("pageTitle");

    QHBoxLayout *topRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("按手机号/昵称模糊搜索, 留空显示全部");
    m_searchEdit->setFixedWidth(280);
    QPushButton *searchBtn = new QPushButton("搜索", this);
    QPushButton *refreshBtn = new QPushButton("刷新", this);
    m_freezeBtn = new QPushButton("冻结/解冻", this);
    m_freezeBtn->setEnabled(false);
    topRow->addWidget(m_searchEdit);
    topRow->addWidget(searchBtn);
    topRow->addWidget(refreshBtn);
    topRow->addWidget(m_freezeBtn);
    topRow->addStretch();

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(
        { "用户ID", "手机号", "昵称", "钱包余额(元)", "注册时间", "状态" });

    layout->addWidget(title);
    layout->addLayout(topRow);
    layout->addWidget(m_table, 1);

    connect(searchBtn, &QPushButton::clicked, this, &UserManagePage::refresh);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &UserManagePage::refresh);
    connect(refreshBtn, &QPushButton::clicked, this, &UserManagePage::refresh);
    connect(m_freezeBtn, &QPushButton::clicked, this, &UserManagePage::onFreezeClicked);
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &UserManagePage::onSelectionChanged);
    refresh();
}

void UserManagePage::refresh()
{
    const QList<UserInfo> users = UserDao::list(m_searchEdit->text().trimmed());
    m_table->setRowCount(users.size());
    for (int i = 0; i < users.size(); ++i) {
        const UserInfo &u = users[i];
        auto *idItem = new QTableWidgetItem(QString::number(u.id));
        idItem->setData(Qt::UserRole, u.id);
        idItem->setData(Qt::UserRole + 1, u.status);
        m_table->setItem(i, 0, idItem);
        m_table->setItem(i, 1, new QTableWidgetItem(u.phone));
        m_table->setItem(i, 2, new QTableWidgetItem(u.nickname));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(u.balance, 'f', 2)));
        m_table->setItem(i, 4, new QTableWidgetItem(u.registerTime));
        auto *statusItem = new QTableWidgetItem(u.status == UserFrozen ? "冻结" : "正常");
        statusItem->setForeground(
            QBrush(u.status == UserFrozen ? QColor("#E5484D") : QColor("#1D976C")));
        m_table->setItem(i, 5, statusItem);
    }
    m_table->resizeColumnsToContents();
    onSelectionChanged();
}

void UserManagePage::onSelectionChanged()
{
    const QList<QTableWidgetItem *> selected = m_table->selectedItems();
    if (selected.isEmpty()) {
        m_selectedUserId = -1;
        m_freezeBtn->setEnabled(false);
        return;
    }
    const QTableWidgetItem *idItem = m_table->item(selected.first()->row(), 0);
    m_selectedUserId = idItem->data(Qt::UserRole).toInt();
    m_selectedStatus = idItem->data(Qt::UserRole + 1).toInt();
    m_freezeBtn->setText(m_selectedStatus == UserFrozen ? "解冻" : "冻结");
    m_freezeBtn->setEnabled(true);
}

void UserManagePage::onFreezeClicked()
{
    if (m_selectedUserId < 0)
        return;

    const bool toFrozen = (m_selectedStatus != UserFrozen);
    const QString phone = m_table->item(m_table->selectedItems().first()->row(), 1)->text();

    if (QMessageBox::question(this, toFrozen ? "冻结用户" : "解冻用户",
                              QString("确定要%1用户 %2 (%3) 吗?")
                                  .arg(toFrozen ? "冻结" : "解冻", phone))
        != QMessageBox::Yes)
        return;

    QString errMsg;
    if (!UserDao::setStatus(m_selectedUserId,
                            toFrozen ? UserFrozen : UserNormal, &errMsg)) {
        QMessageBox::warning(this, "操作失败", errMsg);
        return;
    }

    LogDao::record(ServerSession::instance().adminName,
                   toFrozen ? "冻结用户" : "解冻用户",
                   QString("用户 %1 (%2)").arg(phone).arg(m_selectedUserId));
    refresh();
}
