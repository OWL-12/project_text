#pragma once

#include <QWidget>
#include "ui_chat.h"
#include <QKeyEvent>
#include <QTcpSocket>
#include <QtSql>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QListWidgetItem>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEvent>
#include <QObject>
#include <QPoint>
#include <QDebug>

class chat : public QWidget
{
    Q_OBJECT

public:
    explicit chat(QWidget* parent = nullptr);
    ~chat();

    void connectToServer(const QString& host, quint16 port); // 连接服务器
    bool eventFilter(QObject* obj, QEvent* event) override; // 处理 Enter
    void setCurrentChatPartner(const QString& chatPartner); // 设置当前聊天伙伴
    void setUsername(const QString& username);
    void closeEvent(QCloseEvent* event);  // 关闭窗口

private slots:
    void receiveMessage(); // 接收消息
    void sendMessage(); // 发送消息
    void reconnectToServer(); // 重新连接服务器
    void minimizeWindow();  // 最小化窗口
    void closeWindow();     // 关闭窗口

signals:
    void chatClosed(const QString& chatPartner);  // 关闭聊天信号
    void messageReceived(QString userName, QString avatarPath, QString lastMessage); // 接收消息到页面
    void messageSent(QString userName, QString message);

private:
    Ui::chatClass* ui;

    QTcpSocket* socket;  // 通信套接字
    QString username;     // 当前用户名
    QString currentChatPartner; // 当前聊天伙伴
    QSqlDatabase db;  // 数据库
    QPoint lastPos; //记录位置

    void connectDatabase();
    void loadChatHistory(const QString& chatPartner);
    void displayMessage(const QString& sender, const QString& message, bool isSelf);
    void saveMessageToDB(const QString& sender, const QString& receiver, const QString& message);
    void addMessageToChatList(const QString& sender, const QString& message);
    void addMessageToChatList(const QString& sender, const QString& message, bool isSelf);
    void checkForNewMessages();
    void startMessagePolling();
    QString getUserAvatar(const QString& username);

    void scrollToLastMessage();

    //实现鼠标移动事件
    void mouseMoveEvent(QMouseEvent* event);
    void mousePressEvent(QMouseEvent* event);
};
