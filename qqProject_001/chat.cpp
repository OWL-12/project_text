#include "chat.h"
#include "ui_chat.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtSql>
#include <QListWidgetItem>
#include <QNetworkProxy>
#include <QTimer>
#include <QDateTime>

#include <mainWindow.h>

QTimer* messageTimer;
QDateTime lastMessageTime; // 记录最后获取消息的时间

chat::chat(QWidget* parent)
    : QWidget(parent),
    ui(new Ui::chatClass),
    socket(new QTcpSocket(this)),
    currentChatPartner("")
{
    ui->setupUi(this);

    //设置默认无边框
    this->setWindowFlags(Qt::FramelessWindowHint);

    // 初始化 socket
    connect(socket, &QTcpSocket::readyRead, this, &chat::receiveMessage);
    connect(socket, &QTcpSocket::disconnected, this, &chat::reconnectToServer);

    // 绑定按钮事件
    connect(ui->sendButton, &QPushButton::clicked, this, &chat::sendMessage);


    // 限制输入框最大 500 字符
    connect(ui->chatInputText, &QTextEdit::textChanged, this, [this]() {
        if (ui->chatInputText->toPlainText().length() > 500) {
            ui->chatInputText->setPlainText(ui->chatInputText->toPlainText().left(500));
            QTextCursor cursor = ui->chatInputText->textCursor();
            cursor.movePosition(QTextCursor::End);
            ui->chatInputText->setTextCursor(cursor);
        }
        });

    // 监听 Enter 键
    ui->chatInputText->installEventFilter(this);

    // 窗口加载后滚动到底部
    QTimer::singleShot(100, this, &chat::scrollToLastMessage);

    // 绑定按钮点击信号到槽函数
    connect(ui->miniButton, &QPushButton::clicked, this, &chat::minimizeWindow);
    connect(ui->closeButton, &QPushButton::clicked, this, &chat::closeWindow);

    connectDatabase(); // 连接数据库
}

// 获取用户名
void chat::setUsername(const QString& username)
{
    this->username = username;
    qDebug() << "chat 窗口的 username 设置为：" << this->username;
}

// 监听Enter
bool chat::eventFilter(QObject* obj, QEvent* event) {
    if (obj == ui->chatInputText && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            sendMessage();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

// **连接数据库**
void chat::connectDatabase()
{
    db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("192.168.217.131");
    db.setPort(3306);
    db.setDatabaseName("chat_app");
    db.setUserName("owl");
    db.setPassword("123456");

    if (!db.open()) {
        qDebug() << "数据库连接失败：" << db.lastError().text();
    }
    else {
        qDebug() << "数据库连接成功";
    }
}

// **连接服务器**
void chat::connectToServer(const QString& host, quint16 port) {
    if (socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "已经连接到服务器";
        return;
    }

    // 禁用代理
    socket->setProxy(QNetworkProxy::NoProxy);

    socket->connectToHost(host, port);

    if (!socket->waitForConnected(1000)) { // 5秒超时
        qDebug() << "连接服务器失败：" << socket->errorString();
    }
    else {
        qDebug() << "成功连接服务器：" << socket->peerAddress().toString() << ":" << socket->peerPort();
    }
}

// 断线重连
void chat::reconnectToServer()
{
    qDebug() << "与服务器断开连接，尝试重连...";

    if (!socket->peerAddress().isNull() && socket->peerPort() != 0) {
        socket->abort();
        socket->connectToHost(socket->peerAddress(), socket->peerPort());

        if (!socket->waitForConnected(3000)) {
            qDebug() << "重连失败：" << socket->errorString();
        }
        else {
            qDebug() << "重连成功：" << socket->peerAddress().toString();
        }
    }
    else {
        qDebug() << "无法重连，之前的连接信息丢失！";
    }
}


// 发送消息
void chat::sendMessage() {
    QString message = ui->chatInputText->toPlainText();
    if (message.isEmpty() || currentChatPartner.isEmpty()) {
        qDebug() << "消息为空或未选择聊天对象";
        return;
    }

    QJsonObject json;
    json["type"] = "chat";
    json["sender"] = username;
    json["receiver"] = currentChatPartner;
    json["message"] = message;

    QByteArray data = QJsonDocument(json).toJson();
    if (socket->state() == QAbstractSocket::ConnectedState) {
        socket->write(data);
        socket->flush();
        //addMessageToChatList(username, message, true);
        saveMessageToDB(username, currentChatPartner, message);
    }
    else {
        qDebug() << "未连接到服务器，无法发送消息";
    }

    ui->chatInputText->clear();
}

// 接收消息
void chat::receiveMessage()
{
    QByteArray data = socket->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;

    QJsonObject json = doc.object();
    if (json["type"].toString() == "chat") {
        QString sender = json["sender"].toString();
        QString message = json["message"].toString();
        addMessageToChatList(sender, message, false); // false表示消息是接收的
        saveMessageToDB(sender, username, message);


    }
}

// 聊天界面ui显示
void chat::addMessageToChatList(const QString& sender, const QString& message, bool isSelf)
{
    // 创建 QListWidgetItem
    QListWidgetItem* item = new QListWidgetItem();

    // 创建消息显示控件
    QWidget* messageWidget = new QWidget(ui->chatListWidget);
    QHBoxLayout* mainLayout = new QHBoxLayout(messageWidget);  // 水平布局（头像 + 气泡）
    mainLayout->setSpacing(5);
    mainLayout->setAlignment(Qt::AlignLeft);  // 确保左对齐

    // **头像 QLabel**
    QLabel* avatarLabel = new QLabel(messageWidget);
    avatarLabel->setFixedSize(40, 40);
    QString avatarPath = getUserAvatar(sender);
    QPixmap avatarPixmap(avatarPath);
    if (!avatarPixmap.isNull()) {
        avatarLabel->setPixmap(avatarPixmap.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    else {
        avatarLabel->setText("❌");
    }
    avatarLabel->setScaledContents(true);

    // **✅ 头像垂直布局，确保头像不会垂直居中**
    QVBoxLayout* avatarLayout = new QVBoxLayout();
    avatarLayout->addWidget(avatarLabel, 0, Qt::AlignTop);  // **强制顶部对齐**
    avatarLayout->addStretch();  // **确保头像不会随内容高度变动**

    // **气泡 QLabel**
    QLabel* messageLabel = new QLabel(messageWidget);
    messageLabel->setText(message);
    messageLabel->setWordWrap(true);
    messageLabel->setMargin(8);
    messageLabel->setMaximumWidth(250);
    messageLabel->adjustSize();

    // **✅ 气泡垂直布局**
    QVBoxLayout* messageLayout = new QVBoxLayout();
    messageLayout->addWidget(messageLabel);
    messageLayout->setAlignment(Qt::AlignTop); // **确保气泡顶部对齐**

    // **气泡样式**
    messageLabel->setStyleSheet(QString(
        "background-color: %1; border-radius: 10px; padding: 5px;"
    ).arg(isSelf ? "lightblue" : "lightgray"));

    // **组装左右布局**
    if (isSelf) {
        mainLayout->addStretch();
        mainLayout->addLayout(messageLayout);
        mainLayout->addLayout(avatarLayout);
    }
    else {
        mainLayout->addLayout(avatarLayout);
        mainLayout->addLayout(messageLayout);
        mainLayout->addStretch();
    }

    // **确保整体对齐**
    mainLayout->setAlignment(Qt::AlignTop);
    messageWidget->setLayout(mainLayout);

    // **计算 item 高度**
    int messageHeight = messageLabel->sizeHint().height() + 15;
    item->setSizeHint(QSize(messageLabel->width() + 60, messageHeight));

    // **添加到 QListWidget**
    ui->chatListWidget->addItem(item);
    ui->chatListWidget->setItemWidget(item, messageWidget);

    // **添加一个空白占位项**
    QListWidgetItem* emptyItem = new QListWidgetItem();
    emptyItem->setSizeHint(QSize(1, 10));  // 设置小的高度，避免占太大空间
    ui->chatListWidget->addItem(emptyItem);
}

QString chat::getUserAvatar(const QString& username) {
    QSqlQuery query;
    query.prepare("SELECT avatar FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (query.exec() && query.next()) {
        QString avatarPath = query.value("avatar").toString().trimmed();  // 去除首尾空格

        // 统一路径格式，确保 Qt 资源路径正确
        if (avatarPath.startsWith(": /")) {
            avatarPath.replace(": /", ":/");
        }

        qDebug() << "Avatar path for" << username << ":" << avatarPath;

        if (!avatarPath.isEmpty()) {
            return avatarPath;
        }
    }
    return ":/default_avatar.png";  // 默认头像
}

// 存储消息到数据库
void chat::saveMessageToDB(const QString& sender, const QString& receiver, const QString& message)
{
    if (!db.isOpen()) {
        qDebug() << "数据库未连接，无法存储消息";
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO messages (sender, receiver, content, timestamp) VALUES (?, ?, ?, NOW())");
    query.addBindValue(sender);
    query.addBindValue(receiver);
    query.addBindValue(message);

    if (!query.exec()) {
        qDebug() << "消息存储失败：" << query.lastError().text();
    }
    else {
        qDebug() << "消息存储成功：" << sender << " -> " << receiver << " : " << message;
    }
}

// 加载历史聊天记录
void chat::loadChatHistory(const QString& chatPartner) {
    qDebug() << "加载聊天记录：" << chatPartner;
    currentChatPartner = chatPartner;
    ui->chatListWidget->clear();

    if (!db.isOpen()) {
        qDebug() << "数据库连接失败";
        return;
    }

    QSqlQuery query;
    query.prepare("SELECT * FROM messages WHERE (sender = ? AND receiver = ?) OR (sender = ? AND receiver = ?) ORDER BY timestamp ASC");
    query.addBindValue(this->username);
    query.addBindValue(chatPartner);
    query.addBindValue(chatPartner);
    query.addBindValue(this->username);

    qDebug() << "执行的 SQL 查询：" << query.lastQuery();
    qDebug() << "绑定的参数：" << this->username << chatPartner << chatPartner << this->username;


    qDebug() << "执行的 SQL 查询：" << query.lastQuery();

    if (!query.exec()) {
        qDebug() << "查询历史记录失败：" << query.lastError().text();
        return;
    }

    int count = 0;
    while (query.next()) {
        QString sender = query.value(1).toString();
        QString message = query.value(3).toString();
        QDateTime messageTime = query.value(4).toDateTime();
        bool isSelf = (sender == this->username);  // 判断消息发送者是否是自己
        addMessageToChatList(sender, message, isSelf);
        lastMessageTime = messageTime; // 更新最近的消息时间
        count++;
    }

    qDebug() << "加载的历史记录条数：" << count;
}

//显示消息
void chat::displayMessage(const QString& sender, const QString& message, bool isSelf) {
    QString formattedMessage = sender + ": " + message;
    QListWidgetItem* item = new QListWidgetItem(formattedMessage);

    if (isSelf) {
        item->setTextAlignment(Qt::AlignRight);
    }
    else {
        item->setTextAlignment(Qt::AlignLeft);
    }

    ui->chatListWidget->addItem(item);
}

void chat::setCurrentChatPartner(const QString& chatPartner) {
    this->currentChatPartner = chatPartner;
    qDebug() << "设置当前聊天对象：" << chatPartner;
    ui->chatListWidget->clear(); // 清空聊天列表
    loadChatHistory(chatPartner); // 加载聊天记录

    // 连接到服务器，传递服务器地址、端口和用户名
    QString serverAddress = "192.168.253.1";  // 服务器地址
    quint16 serverPort = 8888;  // 服务器端口
    connectToServer(serverAddress, serverPort);  // 调用connectToServer连接到服务器

    startMessagePolling();
}

void chat::closeEvent(QCloseEvent* event) {
    qDebug() << "聊天窗口关闭：" << currentChatPartner;
    emit chatClosed(currentChatPartner);  // 发送信号通知 mainWindow
    event->accept();
}

void chat::startMessagePolling()
{
    if (messageTimer) {
        messageTimer->stop();
        delete messageTimer;
    }

    messageTimer = new QTimer(this);
    connect(messageTimer, &QTimer::timeout, this, &chat::checkForNewMessages);
    messageTimer->start(500); // 每 0.5 秒检查一次
}

void chat::checkForNewMessages()
{

    if (!db.isOpen()) {
        qDebug() << "数据库未连接，无法查询新消息";
        return;
    }

    QSqlQuery query;
    query.prepare("SELECT sender, content, timestamp FROM messages "
        "WHERE ((sender = ? AND receiver = ?) OR (sender = ? AND receiver = ?)) "
        "AND timestamp > ? ORDER BY timestamp ASC");
    query.addBindValue(this->username);
    query.addBindValue(currentChatPartner);
    query.addBindValue(currentChatPartner);
    query.addBindValue(this->username);
    query.addBindValue(lastMessageTime.toString("yyyy-MM-dd HH:mm:ss"));

    if (!query.exec()) {
        qDebug() << "查询新消息失败：" << query.lastError().text();
        return;
    }

    bool hasNewMessage = false;
    while (query.next()) {
        QString sender = query.value(0).toString();
        QString message = query.value(1).toString();
        QDateTime messageTime = query.value(2).toDateTime();

        if (messageTime > lastMessageTime) {
            lastMessageTime = messageTime;
        }

        bool isSelf = (sender == this->username);
        addMessageToChatList(sender, message, isSelf);
        hasNewMessage = true;
    }
}

// 打开页面时定位到最后一条消息
void chat::scrollToLastMessage() {
    if (ui->chatListWidget->count() > 0) {
        ui->chatListWidget->scrollToBottom();
    }
}



void chat::minimizeWindow()
{
    this->showMinimized();  // 最小化窗口
}

void chat::closeWindow()
{
    this->close();  // 关闭窗口
}

//窗口可以被鼠标拖动
void chat::mousePressEvent(QMouseEvent* event)
{
    lastPos = event->globalPosition().toPoint() - this->pos();
}

void chat::mouseMoveEvent(QMouseEvent* event)
{
    this->move(event->globalPosition().toPoint() - lastPos);
}



// **析构**
chat::~chat()
{
    delete ui;
    db.close();
}
