#pragma once

#include <QWidget>
#include "ui_mainWindow.h"
#include <QPoint>
#include <QMouseEvent>
#include <QListWidget>
#include <QtSql/QSqlDatabase>
#include "chat.h"
#include "GroupChat.h"

class mainWindow : public QWidget
{
	Q_OBJECT

public:
	mainWindow(const QString& username,QWidget *parent = nullptr);
	~mainWindow();


public slots:
	void closeWindow(); // �ر�ҳ��
	void miniWindow();  // ��С��ҳ��
	void closeEvent(QCloseEvent* event);
	void openChatWindow(QTreeWidgetItem* item,int colum); // ������ҳ��
	void message_openChatWindow(QListWidgetItem* item); // ������ҳ��
	void openGroupChatWindow(QTreeWidgetItem* item);   // ��Ⱥ�Ľ���
	void openFindFriendGroupWindow();  // ����Ӻ��Ѵ���

	void onChatClosed(const QString& chatPartner); // ɾ������ָ��
	void onGroupChatClosed(int groupId);


private:
	Ui::mainWindowClass *ui;

	QTreeWidget* treeWidget;
	QPoint lastPos;   //��¼���λ��

	QSqlDatabase db; // ���ݿ�

	QString currentUsername; // ���ܵ�¼���洫�ݹ������û���

	QMap<QString, chat*> chatWindows; // �洢�Ѵ򿪵����촰��

	QMap<int, GroupChat*> groupChatWindows; // �洢�Ѵ򿪵�Ⱥ�����촰��

	void loadUserInfo(const QString& username);  // ��¼������Ϣ


	// ʵ�ֺ��������
	void setupFriendList();   
	void addFriendItem(QTreeWidgetItem* parent, const QString& avatar,
		const QString& name, const QString& status);
	//ʵ��Ⱥ�������
	void setupGroupList();
	void addGroupItem(QTreeWidgetItem* parent, const QString& avatar,
		const QString& name, int groupId);



	//void setupMessageList();  //ʵ����Ϣҳ���Զ����
	void setupConnections();  //ʵ�ָ��ݰ�ť�л�ҳ��Ĺ���
	void setupFriendsConnections();  //ʵ�ָ��ݰ�ť�л�ҳ��Ĺ���

	void addMessageWidget(const QString& friendName, const QString& avatarPath, const QString& signature, const QString& lastMessage);
	void updateMessageList();

	//ʵ������϶�����
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
};
