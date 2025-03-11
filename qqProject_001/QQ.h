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
    void minimizeWindow();  // �ۺ�������С������
    void closeWindow();     // �ۺ������رմ���
    void login();           // �ۺ�����ʵ�ֵ�¼����
    void fadeOutAndSwitch(const QString& username);  // �˳���������ת
    void clearErrorMessage(); // ���������Ϣ
    void onComboBoxClicked(int index); // ����������
    void onUserBoxSelectionChanged(int index); // Ͷ�䵽userLineEdit����

private:
    Ui::QQClass *ui;
    QPoint lastPos; //��¼���λ��
    QLabel* errorLabel;  // ������ʾ�ı�
    mainWindow* mw;   //����������


    //ʵ�ֱ����˺ŵĹ���
    void saveAccount(const QString& username);
    void loadAccounts();
    void loadAvatar(const QString& username);
    void updateAvatar();

    QComboBox* userBox;
    QLabel* avatarLabel;


    QSqlDatabase db;  // ���ݿ�����

    



    //ʵ������϶�����
    void mouseMoveEvent(QMouseEvent* event);
    void mousePressEvent(QMouseEvent* event);

};
