#pragma once

#include <QWidget>
#include "ui_FindFriendGroup.h"
#include <QPoint>
#include <QMouseEvent>
#include <QtSql>


class FindFriendGroup : public QWidget
{
	Q_OBJECT

public:
	FindFriendGroup(QWidget *parent = nullptr);
	~FindFriendGroup();


	void setUsername(const QString& username); // 获取用户名
	void setCurrentUser(QString username);                   // 获取用户id

private slots:
	void minimizeWindow();  // 窗口最小化
	void closeWindow();     // 关闭窗口
	
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;

	void onSearchButtonClicked();

private:
	Ui::FindFriendGroupClass* ui;
	QSqlDatabase db;            // 数据库连接
	QPoint lastPos;             // 用于窗口拖动
	QString username;           // 当前用户名
	int currentUserId;          // 存储当前用户 ID


	void connectDatabase();     //  连接数据库
	void setupConnections();    // 页面切换功能

	void onAddFriendClicked(int targetUserId); // 添加好友
	void acceptFriendRequest(int senderId);    // 处理接受好友申请的功能
	void loadFriendRequests();                 // 加载好友申请
	void rejectFriendRequest(int senderId);    // 拒绝好友申请

	void loadGroupRequests();  // 加载群聊申请列表
	void handleGroupRequest(int requestId, int groupId, int userId, bool accept);  // 处理群聊申请
};
