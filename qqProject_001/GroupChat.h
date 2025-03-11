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

	void connectToServer(const QString& host, quint16 port); // ���ӷ�����

	// �����ٴδ򿪴���
	void closeEvent(QCloseEvent* event);

	void setUsername(const QString& username); // ��ȡ�û���

	void setGroupId(int id);
	void setGroupName(const QString& name);

	bool eventFilter(QObject* obj, QEvent* event) override; // ���� Enter

	void setGroupChat(int groupId, const QString& groupName);


private slots:
	void reconnectToServer(); // ��������
	void receiveMessage(); // �����������Ϣ
	void minimizeWindow();  // �ۺ�������С������
	void closeWindow();     // �ۺ������رմ���

signals:
	void groupChatClosed(int groupId);

private:
	Ui::GroupChatClass *ui;

	QSqlDatabase group_db;  // ���ݿ�����
	QTcpSocket* groupSocket;  // ��������ͨ��
	QString username;     // ��ǰ�û���

	int groupId;           // Ⱥ�� ID
	QString groupName;     // Ⱥ������
	QPoint lastPos; //��¼���λ��

	void connectDatabase(); // �������ݿ�

	// �������ui����
	void addMessageToChatList(const QString& sender, const QString& message, bool isSelf);
	QString getUserAvatar(const QString& username);

	// �洢�����¼�����ݿ�
	void saveGroupMessageToDB(int groupId, const QString& sender, const QString& message);

	// ����Ⱥ�ĳ�Ա
	void loadGroupMembers(int groupId);

	// ������ʷ������Ϣ
	void loadGroupChatHistory(int groupId);
	void startMessagePolling();
	void checkForNewGroupMessages();

	void sendMessage();

	void scrollToLastMessage();

	//ʵ������϶�����
	void mouseMoveEvent(QMouseEvent* event);
	void mousePressEvent(QMouseEvent* event);
};
