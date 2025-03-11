#include "GroupChat.h"
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

QTimer* groupMessageTimer;
QDateTime groupLastMessageTime; // 记录最后获取消息的时间

GroupChat::GroupChat(QWidget *parent)
	: QWidget(parent),
	ui(new Ui::GroupChatClass),
	groupSocket(new QTcpSocket(this)),
    groupId(0), groupName("")
{
	ui->setupUi(this);

	// 初始化 socket
	connect(groupSocket, &QTcpSocket::readyRead, this, &GroupChat::receiveMessage);
	connect(groupSocket, &QTcpSocket::disconnected, this, &GroupChat::reconnectToServer);

    // 绑定按钮事件
    connect(ui->sendButton, &QPushButton::clicked, this, &GroupChat::sendMessage);

    // 限制输入框最大 500 字符
    connect(ui->chatInputext, &QTextEdit::textChanged, this, [this]() {
        if (ui->chatInputext->toPlainText().length() > 500) {
            ui->chatInputext->setPlainText(ui->chatInputext->toPlainText().left(500));
            QTextCursor cursor = ui->chatInputext->textCursor();
            cursor.movePosition(QTextCursor::End);
            ui->chatInputext->setTextCursor(cursor);
        }
        });

    // 监听 Enter 键
    ui->chatInputext->installEventFilter(this);

    // 绑定按钮点击信号到槽函数
    connect(ui->miniButton, &QPushButton::clicked, this, &GroupChat::minimizeWindow);
    connect(ui->closeButton, &QPushButton::clicked, this, &GroupChat::closeWindow);

	//设置默认无边框
	this->setWindowFlags(Qt::FramelessWindowHint);

    loadGroupMembers(groupId);

    // 窗口加载后滚动到底部
    QTimer::singleShot(100, this, &GroupChat::scrollToLastMessage);

    connectDatabase();
}

// 监听Enter
bool GroupChat::eventFilter(QObject* obj, QEvent* event) {
    if (obj == ui->chatInputext && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            sendMessage();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

// 连接数据库
void GroupChat::connectDatabase()
{
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        group_db = QSqlDatabase::database("qt_sql_default_connection");
    }
    else {
        group_db = QSqlDatabase::addDatabase("QMYSQL");
        group_db.setHostName("192.168.217.131");
        group_db.setPort(3306);
        group_db.setDatabaseName("chat_app");
        group_db.setUserName("owl");
        group_db.setPassword("123456");
    }

    if (!group_db.isOpen() && !group_db.open()) {
        qDebug() << "数据库连接失败：" << group_db.lastError().text();
    }
    else {
        qDebug() << "数据库连接成功";
    }
}


// **连接服务器**
void GroupChat::connectToServer(const QString& host, quint16 port) {
    if (groupSocket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "已经连接到服务器";
        return;
    }

    // 禁用代理
    groupSocket->setProxy(QNetworkProxy::NoProxy);

    groupSocket->connectToHost(host, port);

    if (!groupSocket->waitForConnected(1000)) { // 5秒超时
        qDebug() << "连接服务器失败：" << groupSocket->errorString();
    }
    else {
        qDebug() << "成功连接服务器：" << groupSocket->peerAddress().toString() << ":" << groupSocket->peerPort();
    }
}

void GroupChat::setGroupChat(int groupId, const QString& groupName) {
    this->groupId = groupId;  // 设置群聊 ID
    this->groupName = groupName;  // 设置群聊名称
    qDebug() << "设置当前群聊：" << groupName << " (ID:" << groupId << ")";

    ui->chatListWidget->clear(); // 清空聊天列表
    loadGroupChatHistory(groupId); // 加载群聊历史记录

    // 连接到服务器，传递服务器地址、端口和用户名
    QString serverAddress = "192.168.253.1";  // 服务器地址
    quint16 serverPort = 8888;  // 服务器端口
    qDebug() << "准备连接服务器";
    connectToServer(serverAddress, serverPort);  // 调用 connectToServer 连接服务器

    startMessagePolling();
}


// 断线重连
void GroupChat::reconnectToServer()
{
    qDebug() << "与服务器断开连接，尝试重连...";

    if (!groupSocket->peerAddress().isNull() && groupSocket->peerPort() != 0) {
        groupSocket->abort();
        groupSocket->connectToHost(groupSocket->peerAddress(), groupSocket->peerPort());

        if (!groupSocket->waitForConnected(3000)) {
            qDebug() << "重连失败：" << groupSocket->errorString();
        }
        else {
            qDebug() << "重连成功：" << groupSocket->peerAddress().toString();
        }
    }
    else {
        qDebug() << "无法重连，之前的连接信息丢失！";
    }
}

// 发送消息
void GroupChat::sendMessage() {
    QString message = ui->chatInputext->toPlainText();
    if (message.isEmpty() || groupId == 0) {  // 群聊 ID 不能为 0
        qDebug() << "消息为空或群聊 ID 无效";
        return;
    }

    QJsonObject json;
    json["type"] = "group_chat";   // 群聊类型
    json["sender"] = username;     // 发送者用户名
    json["groupId"] = groupId;     // 发送的群聊 ID
    json["message"] = message;     // 发送的消息内容

    QByteArray data = QJsonDocument(json).toJson();
    if (groupSocket->state() == QAbstractSocket::ConnectedState) {
        groupSocket->write(data);
        groupSocket->flush();
        saveGroupMessageToDB(groupId, username, message);  // 存入群聊数据库

        // 创建消息显示项
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        bool isSelf = true;  // 发送者就是当前用户

        // 获取最后一条消息的 QListWidgetItem
        int lastItemIndex = ui->chatListWidget->count() - 1;
        QListWidgetItem* lastItem = ui->chatListWidget->item(lastItemIndex);

        // 滚动到发送的消息位置
        ui->chatListWidget->scrollToItem(lastItem);
    }
    else {
        qDebug() << "未连接到服务器，无法发送群聊消息";
    }

    ui->chatInputext->clear();
}


// 接收消息
void GroupChat::receiveMessage()
{
    QByteArray data = groupSocket->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;

    QJsonObject json = doc.object();
    if (json["type"].toString() == "group_chat") {  // 确保是群聊消息
        int groupId = json["group_id"].toInt();  // 解析群聊 ID
        QString sender = json["sender"].toString();
        QString message = json["message"].toString();

        addMessageToChatList(sender, message, false); // false 表示接收的消息
        saveGroupMessageToDB(groupId, sender, message); // 这里传入 groupId
    }
}


// 存储聊天记录到数据库
void GroupChat::saveGroupMessageToDB(int groupId, const QString& sender, const QString& message) {
    if (!group_db.isOpen()) {
        qDebug() << "数据库未连接，无法存储群聊消息";
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO group_messages (group_id, sender, content, timestamp) VALUES (?, ?, ?, ?)");
    query.addBindValue(groupId);
    query.addBindValue(sender);
    query.addBindValue(message);
    query.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")); // 统一时间格式

    if (!query.exec()) {
        qDebug() << "群聊消息存储失败：" << query.lastError().text();
    }
    else {
        qDebug() << "群聊消息存储成功：" << groupId << " : " << sender << " -> " << message;
    }
}

// 加载历史聊天消息
void GroupChat::loadGroupChatHistory(int groupId) {
    qDebug() << "加载群聊聊天记录，群聊 ID：" << groupId;
    ui->chatListWidget->clear(); // 清空消息列表

    if (!group_db.isOpen()) {
        qDebug() << "数据库连接失败";
        return;
    }

    QSqlQuery query;
    query.prepare("SELECT sender, content, timestamp FROM group_messages WHERE group_id = ? ORDER BY timestamp ASC");
    query.addBindValue(groupId);

    qDebug() << "执行的 SQL 查询：" << query.lastQuery();
    qDebug() << "绑定的参数：" << groupId;

    if (!query.exec()) {
        qDebug() << "查询群聊历史记录失败：" << query.lastError().text();
        return;
    }

    int count = 0;
    while (query.next()) {
        QString sender = query.value(0).toString();
        QString message = query.value(1).toString();
        QDateTime messageTime = query.value(2).toDateTime();
        bool isSelf = (sender == this->username);  // 判断消息发送者是否是自己
        addMessageToChatList(sender, message, isSelf);
        groupLastMessageTime = messageTime; // 更新最近的消息时间
        count++;
    }

    qDebug() << "加载的群聊历史记录条数：" << count;
}



// 聊天界面ui显示
void GroupChat::addMessageToChatList(const QString& sender, const QString& message, bool isSelf)
{
    // 创建 QListWidgetItem
    QListWidgetItem* item = new QListWidgetItem();

    // 创建消息显示控件
    QWidget* messageWidget = new QWidget(ui->chatListWidget);
    QHBoxLayout* mainLayout = new QHBoxLayout(messageWidget);  // 水平布局（左侧头像 + 右侧内容）
    mainLayout->setSpacing(5);
    mainLayout->setAlignment(Qt::AlignLeft);

    // **1️⃣ 头像 QLabel**
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

    // **2️⃣ 昵称 QLabel**
    QLabel* senderLabel = new QLabel(sender, messageWidget);
    senderLabel->setStyleSheet("font-size: 10px; color: gray;");
    senderLabel->setAlignment(Qt::AlignCenter);

    // **3️⃣ 气泡 QLabel**
    QLabel* messageLabel = new QLabel(messageWidget);
    messageLabel->setText(message);
    messageLabel->setWordWrap(true);
    messageLabel->setMargin(8);
    messageLabel->setMaximumWidth(250);
    messageLabel->adjustSize();
    messageLabel->setStyleSheet(QString(
        "background-color: %1; border-radius: 10px; padding: 5px;"
    ).arg(isSelf ? "lightblue" : "lightgray"));

    // **🔹 左侧（头像 + 昵称）布局**
    QVBoxLayout* avatarLayout = new QVBoxLayout();
    avatarLayout->addWidget(senderLabel, 0, Qt::AlignHCenter);  // 先放昵称
    avatarLayout->addWidget(avatarLabel, 0, Qt::AlignHCenter);  // 头像居中对齐
    avatarLayout->setSpacing(2);  // 控制昵称和头像的间距
    avatarLayout->setAlignment(Qt::AlignTop);

    // **🔹 右侧（消息气泡）布局**
    QVBoxLayout* messageLayout = new QVBoxLayout();
    messageLayout->addWidget(messageLabel);
    messageLayout->setAlignment(Qt::AlignTop);

    // **🔹 组装最终布局**
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

    messageWidget->setLayout(mainLayout);

    // **计算 item 高度**
    int messageHeight = messageLabel->sizeHint().height() + senderLabel->sizeHint().height() + 10;
    item->setSizeHint(QSize(messageLabel->width() + 60, messageHeight));

    // **添加到 QListWidget**
    ui->chatListWidget->addItem(item);
    ui->chatListWidget->setItemWidget(item, messageWidget);
}


QString GroupChat::getUserAvatar(const QString& username) {
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

// 加载群成员
void GroupChat::loadGroupMembers(int groupId) {
    ui->memberList->clear(); // 清空列表

    QSqlQuery query;
    query.prepare(R"(
    SELECT u.id, u.username, u.avatar, g.owner_id
    FROM group_members gm
    JOIN users u ON gm.user_id = u.id
    JOIN `groups` g ON gm.group_id = g.group_id
    WHERE gm.group_id = :groupId
)");

    query.bindValue(":groupId", groupId);
    if (!query.exec()) {
        qDebug() << "加载群成员失败：" << query.lastError().text();
        return;
    }

    int ownerId = -1;
    if (query.next()) {
        ownerId = query.value("owner_id").toInt();
        qDebug() << "群主 ID：" << ownerId;
    }

    // 重新遍历所有成员（去掉 query.seek(-1); ）
    query.first(); // 回到第一条记录
    do {
        int userId = query.value("id").toInt();
        QString username = query.value("username").toString();
        QString avatarPath = query.value("avatar").toString().trimmed();

        // 处理之前错误的 ": /" 前缀
        if (avatarPath.startsWith(": /")) {
            avatarPath.replace(": /", ":/");  // 修正 ": /" 为 ":/"
        }

        // 如果路径是 "/QQ/image/..." 开头并且没有 ":"，则添加 ":"
        if (avatarPath.startsWith("/QQ/image/")) {
            avatarPath = ":" + avatarPath;
        }

        // 处理头像路径，确保是完整路径
        QString fullAvatarPath = avatarPath;
        qDebug() << "加载成员：" << username << "，头像路径：" << fullAvatarPath;

        // 判断是否是群主，群主前加【群主】
        if (userId == ownerId) {
            username = "【群主】" + username;
        }

        QListWidgetItem* item = new QListWidgetItem();
        QWidget* memberWidget = new QWidget(ui->memberList);
        QHBoxLayout* layout = new QHBoxLayout(memberWidget);
        layout->setSpacing(10);

        QLabel* avatarLabel = new QLabel(memberWidget);
        avatarLabel->setFixedSize(40, 40);
        QPixmap avatarPixmap(fullAvatarPath);
        if (!avatarPixmap.isNull()) {
            avatarLabel->setPixmap(avatarPixmap.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        else {
            avatarLabel->setText("❌"); // 头像加载失败，显示默认占位符
        }

        QLabel* nameLabel = new QLabel(username, memberWidget);
        nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        layout->addWidget(avatarLabel);
        layout->addWidget(nameLabel);
        layout->setAlignment(Qt::AlignLeft);

        memberWidget->setLayout(layout);
        item->setSizeHint(memberWidget->sizeHint());

        // 如果是群主，插入到列表第一项
        if (userId == ownerId) {
            ui->memberList->insertItem(0, item); // 群主在列表第一个
        }
        else {
            ui->memberList->addItem(item); // 其他成员在后面
        }

        ui->memberList->setItemWidget(item, memberWidget);
    } while (query.next()); // 继续遍历数据库

    qDebug() << "群成员加载完成！";
}



void GroupChat::closeEvent(QCloseEvent* event)
{
    emit groupChatClosed(this->groupId);
    QWidget::closeEvent(event);
}

void GroupChat::startMessagePolling()
{
    if (groupMessageTimer) {
        groupMessageTimer->stop();
        delete groupMessageTimer;
    }

    groupMessageTimer = new QTimer(this);
    connect(groupMessageTimer, &QTimer::timeout, this, &GroupChat::checkForNewGroupMessages);
    groupMessageTimer->start(500); // 每 0.5 秒检查一次
}

void GroupChat::checkForNewGroupMessages() {
    if (!group_db.isOpen()) {
        return;
    }

    QSqlQuery query;
    query.prepare("SELECT sender, content, timestamp FROM group_messages "
        "WHERE group_id = ? AND timestamp > ? ORDER BY timestamp ASC");
    query.addBindValue(this->groupId);
    query.addBindValue(groupLastMessageTime.toString("yyyy-MM-dd HH:mm:ss"));

    if (!query.exec()) {
        qDebug() << "查询新群聊消息失败：" << query.lastError().text();
        return;
    }

    bool hasNewMessage = false;
    while (query.next()) {
        QString sender = query.value(0).toString();
        QString message = query.value(1).toString();
        QDateTime messageTime = query.value(2).toDateTime();

        if (messageTime > groupLastMessageTime) {
            groupLastMessageTime = messageTime;
        }

        bool isSelf = (sender == this->username);
        addMessageToChatList(sender, message, isSelf);
        hasNewMessage = true;
    }

    if (hasNewMessage) {
        qDebug() << "新的群聊消息已更新";
    }
}

// 打开页面时定位到最后一条消息
void GroupChat::scrollToLastMessage() {
    if (ui->chatListWidget->count() > 0) {
        ui->chatListWidget->scrollToBottom();
    }
}


// 获取用户名
void GroupChat::setUsername(const QString& username)
{
    this->username = username;
    qDebug() << "chat 窗口的 username 设置为：" << this->username;
}

void GroupChat::minimizeWindow()
{
    this->showMinimized();  // 最小化窗口
}

void GroupChat::closeWindow()
{
    this->close();  // 关闭窗口
}

//窗口可以被鼠标拖动
void GroupChat::mousePressEvent(QMouseEvent* event)
{
    lastPos = event->globalPosition().toPoint() - this->pos();
}

void GroupChat::mouseMoveEvent(QMouseEvent* event)
{
    this->move(event->globalPosition().toPoint() - lastPos);
}

void GroupChat::setGroupId(int id) {
    groupId = id;
    loadGroupMembers(groupId);  // 确保在设置 ID 后加载群成员
}

void GroupChat::setGroupName(const QString& name) {
    groupName = name;
    // 设置字体颜色为白色
    QPalette palette = ui->groupChatLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    ui->groupChatLabel->setPalette(palette);

    // 设置字体大小
    QFont font = ui->groupChatLabel->font();
    font.setPointSize(16);  // 设置字体大小为16，可以根据需要调整
    ui->groupChatLabel->setFont(font);

    // 设置群聊名称
    ui->groupChatLabel->setText(groupName);

    qDebug() << "群聊名称是：" << groupName;
}


GroupChat::~GroupChat()
{}
