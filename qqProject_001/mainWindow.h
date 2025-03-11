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
	void closeWindow(); // 关闭页面
	void miniWindow();  // 最小化页面
	void closeEvent(QCloseEvent* event);
	void openChatWindow(QTreeWidgetItem* item,int colum); // 打开聊天页面
	void message_openChatWindow(QListWidgetItem* item); // 打开聊天页面
	void openGroupChatWindow(QTreeWidgetItem* item);   // 打开群聊界面
	void openFindFriendGroupWindow();  // 打开添加好友窗口

	void onChatClosed(const QString& chatPartner); // 删除窗口指针
	void onGroupChatClosed(int groupId);


private:
	Ui::mainWindowClass *ui;

	QTreeWidget* treeWidget;
	QPoint lastPos;   //记录鼠标位置

	QSqlDatabase db; // 数据库

	QString currentUsername; // 接受登录界面传递过来的用户名

	QMap<QString, chat*> chatWindows; // 存储已打开的聊天窗口

	QMap<int, GroupChat*> groupChatWindows; // 存储已打开的群聊聊天窗口

	void loadUserInfo(const QString& username);  // 登录个人信息


	// 实现好友栏填充
	void setupFriendList();   
	void addFriendItem(QTreeWidgetItem* parent, const QString& avatar,
		const QString& name, const QString& status);
	//实现群组栏填充
	void setupGroupList();
	void addGroupItem(QTreeWidgetItem* parent, const QString& avatar,
		const QString& name, int groupId);



	//void setupMessageList();  //实现信息页面自动填充
	void setupConnections();  //实现根据按钮切换页面的功能
	void setupFriendsConnections();  //实现根据按钮切换页面的功能

	void addMessageWidget(const QString& friendName, const QString& avatarPath, const QString& signature, const QString& lastMessage);
	void updateMessageList();

	//实现鼠标拖动界面
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
};
