#pragma once

#include <QtWidgets/QWidget>
#include "ui_QQ.h"
#include <qpoint.h>
#include <QMouseEvent>
#include <mainWindow.h>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

class QQ : public QWidget
{
    Q_OBJECT

public:
    QQ(QWidget *parent = nullptr);
    ~QQ();

public slots:
    void minimizeWindow();  // 槽函数：最小化窗口
    void closeWindow();     // 槽函数：关闭窗口
    void login();           // 槽函数：实现登录功能
    void fadeOutAndSwitch(const QString& username);  // 退出动画并跳转
    void clearErrorMessage(); // 清楚错误信息
    void onComboBoxClicked(int index); // 打开下拉窗口
    void onUserBoxSelectionChanged(int index); // 投射到userLineEdit上面

private:
    Ui::QQClass *ui;
    QPoint lastPos; //记录鼠标位置
    QLabel* errorLabel;  // 错误提示文本
    mainWindow* mw;   //创建主界面


    //实现保存账号的功能
    void saveAccount(const QString& username);
    void loadAccounts();
    void loadAvatar(const QString& username);
    void updateAvatar();

    QComboBox* userBox;
    QLabel* avatarLabel;


    QSqlDatabase db;  // 数据库连接

    



    //实现鼠标拖动窗口
    void mouseMoveEvent(QMouseEvent* event);
    void mousePressEvent(QMouseEvent* event);

};
