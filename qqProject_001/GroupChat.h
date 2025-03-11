#pragma once

#include <QWidget>
#include "ui_GroupChat.h"
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


class GroupChat : public QWidget
{
	Q_OBJECT

public:
	GroupChat(QWidget *parent = nullptr);
	~GroupChat();

	void connectToServer(const QString& host, quint16 port); // 连接服务器

	// 可以再次打开窗口
	void closeEvent(QCloseEvent* event);

	void setUsername(const QString& username); // 获取用户名

	void setGroupId(int id);
	void setGroupName(const QString& name);

	bool eventFilter(QObject* obj, QEvent* event) override; // 监听 Enter

	void setGroupChat(int groupId, const QString& groupName);


private slots:
	void reconnectToServer(); // 断线重连
	void receiveMessage(); // 处理服务器消息
	void minimizeWindow();  // 槽函数：最小化窗口
	void closeWindow();     // 槽函数：关闭窗口

signals:
	void groupChatClosed(int groupId);

private:
	Ui::GroupChatClass *ui;

	QSqlDatabase group_db;  // 数据库连接
	QTcpSocket* groupSocket;  // 负责网络通信
	QString username;     // 当前用户名

	int groupId;           // 群聊 ID
	QString groupName;     // 群聊名称
	QPoint lastPos; //记录鼠标位置

	void connectDatabase(); // 连接数据库

	// 聊天界面ui加载
	void addMessageToChatList(const QString& sender, const QString& message, bool isSelf);
	QString getUserAvatar(const QString& username);

	// 存储聊天记录到数据库
	void saveGroupMessageToDB(int groupId, const QString& sender, const QString& message);

	// 加载群聊成员
	void loadGroupMembers(int groupId);

	// 加载历史聊天消息
	void loadGroupChatHistory(int groupId);
	void startMessagePolling();
	void checkForNewGroupMessages();

	void sendMessage();

	void scrollToLastMessage();

	//实现鼠标拖动窗口
	void mouseMoveEvent(QMouseEvent* event);
	void mousePressEvent(QMouseEvent* event);
};
