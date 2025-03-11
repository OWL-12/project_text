#pragma once

#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QMap>
#include <QtSql>

class ChatServer  : public QTcpServer
{
	Q_OBJECT

public:
	explicit ChatServer(QObject *parent = nullptr);
	void startServer(quint16 port);
	~ChatServer();

private slots:
	void handleNewConnection();
	void clientDisconnected();
	void readMessage();

private:
	QMap<QString, QTcpSocket*> onlineUsers;  // 记录在线用户（用户名 -> 套接字）

	QSqlDatabase db;

	void processChat(QTcpSocket* client, const QJsonObject& json);

	void connectDatabase(); // 连接数据库函数

	void saveMessageToDB(const QString& sender, const QString& receiver, const QString& content); // 存储聊天消息

	void processFriendRequest(QTcpSocket* client, const QJsonObject& json); // 处理好友请求
	bool userExists(const QString& userId);
	void saveFriendRequestToDB(const QString& sender, const QString& receiver);
	void processSearchRequest(QTcpSocket* client, const QJsonObject& json);
};

#endif // CHATSERVER_H
