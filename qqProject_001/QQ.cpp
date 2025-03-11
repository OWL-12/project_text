#include "QQ.h"
#include "ui_QQ.h"


#include <QGraphicsDropShadowEffect>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QtSql/QSqlDataBase>
#include <QtSql/QSqlQuery>
#include <QtSql/QsqlError>

#include <QComboBox>
#include <QLabel>
#include <QPixmap>
#include <QSettings>

QQ::QQ(QWidget *parent)
    : QWidget(parent),
    ui(new Ui::QQClass)
{

    //设置数据库连接
    db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("192.168.217.131");
    db.setPort(3306);
    db.setDatabaseName("chat_app");
    db.setUserName("owl");
    db.setPassword("123456");

    qDebug() << "Available drivers:" << QSqlDatabase::drivers();


    ui->setupUi(this);

    ui->avatarlabel->setParent(this);

    //设置默认无边框
    this->setWindowFlags(Qt::FramelessWindowHint);

    

    //头像阴影效果
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(15);
    shadow->setXOffset(0);
    shadow->setYOffset(5);
    shadow->setColor(QColor(0, 0, 0, 100));  // 透明黑色阴影
    ui->avatarlabel->setGraphicsEffect(shadow);

    // 让头像稍微上浮
    ui->avatarlabel->move(ui->avatarlabel->x(), ui->avatarlabel->y() - 3);

    //登录按钮设置
    ui->loginButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #4A90E2;"  // 浅蓝色背景
        "   color: white;"
        "   border-radius: 40px;"  // 圆角，按钮变为椭圆形状
        "   font-size: 16px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #005A9E;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #004A8E;"
        "}"
        "QPushButton:disabled {"
        "   background-color: #B0C4DE;"
        "   color: #FFFFFF;"
        "}"
    );

    ui->userLineEdit->setStyleSheet(
        "QLineEdit {"
        "   border: none;"  // 移除默认边框
        "   border-bottom: 1px solid #CCCCCC;"  // 默认底部灰色线
        "   background: transparent;"  // 背景透明
        "   font-size: 16px;"
        "}"
        "QLineEdit:hover {"
        "   border-bottom: 1px solid #0078D7;"  // 悬停时变蓝
        "}"
        "QLineEdit:focus {"
        "   border-bottom: 2px solid #005A9E;"  // 输入时深蓝色
        "}"
    );

    ui->pdLineEdit->setStyleSheet(
        "QLineEdit {"
        "   border: none;"  // 移除默认边框
        "   border-bottom: 1px solid #CCCCCC;"  // 默认底部灰色线
        "   background: transparent;"  // 背景透明
        "   font-size: 16px;"
        "}"
        "QLineEdit:hover {"
        "   border-bottom: 1px solid #0078D7;"  // 悬停时变蓝
        "}"
        "QLineEdit:focus {"
        "   border-bottom: 2px solid #005A9E;"  // 输入时深蓝色
        "}"
    );


    // 监听用户选择账号事件
    connect(userBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &QQ::updateAvatar);

    // 在构造函数中添加连接信号和槽
    connect(ui->userBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &QQ::onUserBoxSelectionChanged);

    // 手动打开下拉框的方法
    connect(ui->userBox, QOverload<int>::of(&QComboBox::activated), this, &QQ::onComboBoxClicked);


    // 绑定按钮点击信号到槽函数
    connect(ui->minButton, &QPushButton::clicked, this, &QQ::minimizeWindow);
    connect(ui->closeButton, &QPushButton::clicked, this, &QQ::closeWindow);

    //绑定登录按钮
    connect(ui->loginButton, &QPushButton::clicked, this, &QQ::login);

    //绑定清楚错误信息信号
    connect(ui->pdLineEdit, &QLineEdit::textEdited, this,&QQ::clearErrorMessage);

    loadAccounts();
    
}

//连接数据库实现登录功能
void QQ::login()
{
    QString username = ui->userLineEdit->text();
    QString password = ui->pdLineEdit->text();

    if (!db.open()) {
        QMessageBox::critical(this, "错误", "无法连接到数据库！");
        return;
    }

    // 查询数据库中的用户信息
    QSqlQuery query;
    query.prepare("SELECT password FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (query.exec() && query.next()) 
    {
        QString storedPassword = query.value(0).toString();

        //检查密码是否匹配
        if (password == storedPassword)
        {
            fadeOutAndSwitch(username);
        }
        else {
            ui->pdLineEdit->clear();  // 清空密码输入框
            ui->pdLineEdit->setPlaceholderText("账号或密码错误");
            ui->pdLineEdit->setStyleSheet(
                "QLineEdit {"
                "   border: none;"
                "   border-bottom: 1px solid #CCCCCC;"
                "   background: transparent;"
                "   font-size: 16px;"
                "   color: red;"
                "}"
            );
        }
    }
    else {
        ui->pdLineEdit->clear();
        ui->pdLineEdit->setPlaceholderText("账号或密码错误");
        ui->pdLineEdit->setStyleSheet(
            "QLineEdit {"
            "   border: none;"
            "   border-bottom: 1px solid #CCCCCC;"
            "   background: transparent;"
            "   font-size: 16px;"
            "   color: red;"
            "}"
        );
    }

    db.close();
        
}

//登录动画实现
void QQ::fadeOutAndSwitch(const QString& username)
{
    // 透明度效果
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(this);
    this->setGraphicsEffect(effect);
    QPropertyAnimation* fadeOut = new QPropertyAnimation(effect, "opacity");
    fadeOut->setDuration(500);  // 500ms 动画时间
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);

    // 窗口缩小动画
    QPropertyAnimation* scaleDown = new QPropertyAnimation(this, "geometry");
    scaleDown->setDuration(500);
    scaleDown->setStartValue(this->geometry());
    scaleDown->setEndValue(QRect(this->x() + width() / 4, this->y() + height() / 4, width() / 2, height() / 2));

    // 组合动画
    QParallelAnimationGroup* group = new QParallelAnimationGroup(this);
    group->addAnimation(fadeOut);
    group->addAnimation(scaleDown);

    // 连接动画结束信号，关闭当前窗口并打开新窗口
    connect(group, &QParallelAnimationGroup::finished, [=]() {
        mw = new mainWindow(username);  // 创建新窗口
        mw->show();
        this->close();  // 关闭当前窗口
    });

    group->start();

}

//实现自动保存登录账号的功能，并且匹配账号对应的头像
void QQ::saveAccount(const QString& username)
{
    QSettings settings("MyCompany", "ChatApp");
    QStringList accounts = settings.value("accounts").toStringList();
    if (!accounts.contains(username)) {
        accounts.append(username);
        settings.setValue("accounts", accounts);
    }
}

void QQ::loadAccounts()
{
    QSqlQuery query(db);
    if (query.exec("SELECT username FROM users")) {
        userBox->clear();
        while (query.next()) {
            QString username = query.value(0).toString();
            userBox->addItem(username);
        }
    }
    else {
        qDebug() << "查询失败:" << query.lastError().text();
    }
}

void QQ::loadAvatar(const QString& username) {
    QSqlQuery query(db);
    query.prepare("SELECT avatar FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (query.exec() && query.next()) {
        QString avatarPath = query.value(0).toString();
        if (!avatarPath.isEmpty()) {
            QPixmap avatar(avatarPath);
            avatarLabel->setPixmap(avatar.scaled(50, 50));
        }
        else {
            avatarLabel->clear();
        }
    }
    else {
        qDebug() << "无法获取头像:" << query.lastError().text();
    }
}

void QQ::updateAvatar() {
    QString selectedUser = userBox->currentText();
    loadAvatar(selectedUser);
}


void QQ::onComboBoxClicked(int index) {
    ui->userBox->showPopup();  // 点击后展开下拉框
}

// 槽函数：当选择某项时将选中的内容投射到 userLineEdit 上
void QQ::onUserBoxSelectionChanged(int index) {
    // 获取选中的文本
    QString selectedUsername = ui->userBox->currentText();

    // 将选中的文本设置到 userLineEdit 上
    ui->userLineEdit->setText(selectedUsername);
}

//清楚错误信息
void QQ::clearErrorMessage()
{
    ui->pdLineEdit->setPlaceholderText("请输入密码");  // 恢复原始提示
    ui->pdLineEdit->setStyleSheet("color: black;");  // 恢复正常颜色
}

void QQ::minimizeWindow()
{
    this->showMinimized();  // 最小化窗口
}

void QQ::closeWindow()
{
    this->close();  // 关闭窗口
}

//窗口可以被鼠标拖动
void QQ::mousePressEvent(QMouseEvent *event)
{
    lastPos = event->globalPosition().toPoint() - this->pos();
}

void QQ::mouseMoveEvent(QMouseEvent* event)
{
    this->move(event->globalPosition().toPoint() - lastPos);
}

QQ::~QQ()
{
    delete ui;
    db.close();
}
