#include "ChatServer.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QDateTime>
#include <QNetworkInterface>
#include <QTimer>
#include <QThread>
#include <stdexcept>

ChatServer::ChatServer(QObject* parent) : QTcpServer(parent)
{
    connect(this, &QTcpServer::newConnection, this, &ChatServer::handleNewConnection);
    
    // 在构造函数中连接数据库
    connectDatabase();
}

// 连接数据库
void ChatServer::connectDatabase()
{
    // 检查可用的数据库驱动
    QStringList drivers = QSqlDatabase::drivers();
    qDebug() << "可用的数据库驱动：" << drivers;
    
    if (!drivers.contains("QMYSQL")) {
        qDebug() << "错误：MySQL驱动不可用！请确保Qt MySQL插件已正确安装。";
        return;
    }

    // 如果已经有一个连接，先关闭它
    if (db.isOpen()) {
        qDebug() << "关闭现有数据库连接";
        db.close();
    }

    // 移除现有连接
    QString connectionName = "ChatServerConnection";
    if (QSqlDatabase::contains(connectionName)) {
        qDebug() << "移除现有连接：" << connectionName;
        QSqlDatabase::removeDatabase(connectionName);
    }

    // 创建新的命名连接，而不是使用默认连接
    db = QSqlDatabase::addDatabase("QMYSQL", connectionName);
    if (!db.isValid()) {
        qDebug() << "创建数据库连接失败：无效的数据库连接对象";
        return;
    }

    db.setHostName("192.168.217.131");
    db.setPort(3306);
    db.setDatabaseName("chat_app");
    db.setUserName("owl");
    db.setPassword("123456");
    db.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=5");

    // 尝试连接数据库，最多尝试3次
    int maxRetries = 3;
    for (int i = 0; i < maxRetries; i++) {
        if (db.open()) {
            qDebug() << "数据库连接成功，连接名称：" << db.connectionName();
            
            // 验证连接是否真的有效
            QSqlQuery testQuery(db);
            if (testQuery.exec("SELECT 1")) {
                qDebug() << "数据库连接测试成功";
                return;
            } else {
                qDebug() << "数据库连接测试失败：" << testQuery.lastError().text();
                db.close();
            }
        }
        else {
            qDebug() << "数据库连接失败 (尝试 " << i+1 << "/" << maxRetries << "):" << db.lastError().text();
            QThread::sleep(1); // 等待1秒后重试
        }
    }

    qDebug() << "所有数据库连接尝试均失败";
}

void ChatServer::startServer(quint16 port)
{
    if (listen(QHostAddress::Any, port)) {
        qDebug() << "服务器已启动，监听端口：" << port;

        // 获取本机的 IP 地址
        QList<QHostAddress> ipList = QNetworkInterface::allAddresses();
        for (const QHostAddress& address : ipList) {
            if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.toString().startsWith("127.")) {
                qDebug() << "服务器本机 IP 地址：" << address.toString();
            }
        }
    }
    else {
        qDebug() << "服务器启动失败：" << errorString();
    }
}

void ChatServer::handleNewConnection()
{
    qDebug() << "检查是否有新连接：" << hasPendingConnections();  // 先打印看看
    QTcpSocket* clientSocket = nextPendingConnection();
    if (clientSocket) {
        qDebug() << "新客户端连接：" << clientSocket->peerAddress().toString() << ":" << clientSocket->peerPort();
        
        // 设置socket选项
        clientSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1); // 禁用Nagle算法
        clientSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1); // 启用KeepAlive
        
        // 设置读写缓冲区大小
        clientSocket->setReadBufferSize(1024 * 1024); // 1MB读缓冲区
        
        // 连接信号
        connect(clientSocket, &QTcpSocket::readyRead, this, &ChatServer::readMessage);
        connect(clientSocket, &QTcpSocket::disconnected, this, &ChatServer::clientDisconnected);
        connect(clientSocket, &QTcpSocket::errorOccurred, this, [this, clientSocket](QAbstractSocket::SocketError error) {
            qDebug() << "客户端Socket错误：" << clientSocket->errorString() << "，错误代码：" << error;
            qDebug() << "客户端地址：" << clientSocket->peerAddress().toString() << ":" << clientSocket->peerPort();
        });
        
        // 发送欢迎消息
        QJsonObject welcome;
        welcome["type"] = "welcome";
        welcome["message"] = "欢迎连接到聊天服务器";
        welcome["timestamp"] = QDateTime::currentMSecsSinceEpoch();
        QByteArray welcomeData = QJsonDocument(welcome).toJson();
        
        qint64 bytesSent = clientSocket->write(welcomeData);
        bool flushSuccess = clientSocket->flush();
        qDebug() << "欢迎消息发送：" << bytesSent << "字节，flush结果：" << (flushSuccess ? "成功" : "失败");
        
        // 确保客户端仍然连接
        if (clientSocket->state() != QAbstractSocket::ConnectedState) {
            qDebug() << "警告：发送欢迎消息后客户端已断开连接";
        }
    }
    else {
        qDebug() << "新客户端连接失败";
    }
}

void ChatServer::readMessage()
{
    QTcpSocket* senderSocket = qobject_cast<QTcpSocket*>(sender());
    if (!senderSocket) {
        qDebug() << "读取消息失败，无法获取发送者套接字";
        return;
    }

    qDebug() << "接收到客户端消息，客户端地址：" << senderSocket->peerAddress().toString() << ":" << senderSocket->peerPort();
    qDebug() << "客户端状态：" << (senderSocket->state() == QAbstractSocket::ConnectedState ? "已连接" : "未连接");
    qDebug() << "可读数据大小：" << senderSocket->bytesAvailable() << "字节";

    if (senderSocket->bytesAvailable() <= 0) {
        qDebug() << "没有数据可读取";
        return;
    }

    QByteArray data = senderSocket->readAll();
    qDebug() << "接收到的数据：" << data;
    qDebug() << "数据长度：" << data.size() << "字节";

    if (data.isEmpty()) {
        qDebug() << "接收到的数据为空";
        return;
    }

    // 尝试解析JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON解析错误：" << parseError.errorString() << "，位置：" << parseError.offset;
        qDebug() << "错误附近的数据：" << data.mid(qMax(0, parseError.offset - 10), 20);
        return;
    }

    if (!doc.isObject()) {
        qDebug() << "解析的JSON不是一个对象";
        return;
    }

    QJsonObject jsonMsg = doc.object();
    QString type = jsonMsg["type"].toString();
    qDebug() << "消息类型：" << type;

    try {
        if (type == "chat") {
            processChat(senderSocket, jsonMsg);
        }
        else if (type == "friend_request") {
            processFriendRequest(senderSocket, jsonMsg);
        }
        else if (type == "search") {  // 判断类型处理搜索请求
            qDebug() << "处理搜索请求";
            processSearchRequest(senderSocket, jsonMsg);
        }
        else {
            qDebug() << "未知的消息类型：" << type;
            // 发送错误响应
            QJsonObject response;
            response["type"] = "error";
            response["message"] = "未知的消息类型";
            QByteArray responseData = QJsonDocument(response).toJson();
            senderSocket->write(responseData);
            senderSocket->flush();
        }
    }
    catch (const std::exception& e) {
        qDebug() << "处理消息时发生异常：" << e.what();
        // 发送错误响应
        QJsonObject response;
        response["type"] = "error";
        response["message"] = QString("服务器错误: %1").arg(e.what());
        QByteArray responseData = QJsonDocument(response).toJson();
        senderSocket->write(responseData);
        senderSocket->flush();
    }
    catch (...) {
        qDebug() << "处理消息时发生未知异常";
        // 发送错误响应
        QJsonObject response;
        response["type"] = "error";
        response["message"] = "服务器发生未知错误";
        QByteArray responseData = QJsonDocument(response).toJson();
        senderSocket->write(responseData);
        senderSocket->flush();
    }
}

void ChatServer::processChat(QTcpSocket* client, const QJsonObject& json)
{
    QString sender = json["sender"].toString();
    QString receiver = json["receiver"].toString();
    QString message = json["message"].toString();

    QJsonObject response;
    response["type"] = "chat";
    response["sender"] = sender;
    response["message"] = message;
    response["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QJsonDocument responseDoc(response);
    QByteArray responseData = responseDoc.toJson();

    if (onlineUsers.contains(receiver)) {
        // 目标用户在线，发送消息
        onlineUsers[receiver]->write(responseData);
    }
    else {
        // 目标用户不在线，返回错误信息
        QJsonObject errorResponse;
        errorResponse["type"] = "error";
        errorResponse["message"] = "用户不在线";
        client->write(QJsonDocument(errorResponse).toJson());
    }
}

//存储聊天信息
void ChatServer::saveMessageToDB(const QString& sender, const QString& receiver, const QString& content) {
    if (!db.isOpen()) {
        qDebug() << "Database is not open!";
        return;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO messages (sender, receiver, content, timestamp) VALUES (?, ?, ?, NOW())");
    query.addBindValue(sender);
    query.addBindValue(receiver);
    query.addBindValue(content);

    if (!query.exec()) {
        qDebug() << "Failed to insert message:" << query.lastError().text();
    }
}

void ChatServer::clientDisconnected()
{
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (clientSocket) {
        QString clientAddress = clientSocket->peerAddress().toString();
        quint16 clientPort = clientSocket->peerPort();
        
        // 查找是否是已登录用户
        QString username = onlineUsers.key(clientSocket);
        if (!username.isEmpty()) {
            qDebug() << "用户" << username << "断开连接，地址：" << clientAddress << ":" << clientPort;
            onlineUsers.remove(username);  // 删除用户
        } else {
            qDebug() << "未登录客户端断开连接，地址：" << clientAddress << ":" << clientPort;
        }
        
        // 记录断开原因
        qDebug() << "断开原因：" << clientSocket->errorString();
        
        clientSocket->deleteLater();
    } else {
        qDebug() << "客户端断开连接，但无法获取客户端信息";
    }
}

// 处理好友请求
void ChatServer::processFriendRequest(QTcpSocket* client, const QJsonObject& json)
{
    QString sender = json["sender"].toString();
    QString receiver = json["receiver"].toString();

    // 验证用户是否存在
    if (userExists(receiver)) {
        // 发送好友请求成功的响应
        QJsonObject response;
        response["type"] = "friend_request_response";
        response["sender"] = sender;
        response["receiver"] = receiver;
        response["status"] = "request_sent";
        client->write(QJsonDocument(response).toJson());

        // 将请求保存到数据库中
        saveFriendRequestToDB(sender, receiver);
    }
    else {
        // 用户不存在
        QJsonObject response;
        response["type"] = "friend_request_response";
        response["status"] = "user_not_found";
        client->write(QJsonDocument(response).toJson());
    }
}

// 处理搜索请求
void ChatServer::processSearchRequest(QTcpSocket* client, const QJsonObject& json)
{
    QString keyword = json["keyword"].toString().trimmed();
    qDebug() << "收到搜索请求，关键字：" << keyword;

    if (keyword.isEmpty()) {
        qDebug() << "搜索关键字为空";
        return;
    }

    QJsonArray results;
    QSqlQuery query;
    query.prepare("SELECT id as user_id, username, avatar FROM users "
        "WHERE username LIKE :keyword OR id LIKE :keyword");
    query.bindValue(":keyword", "%" + keyword + "%");

    if (query.exec()) {
        while (query.next()) {
            QJsonObject user;
            user["user_id"] = query.value("user_id").toString();
            user["username"] = query.value("username").toString();
            user["avatar"] = query.value("avatar").toString();
            results.append(user);
        }
        qDebug() << "数据库查询到" << results.size() << "个用户";
    }
    else {
        qDebug() << "查询执行失败：" << query.lastError().text();
    }

    QJsonObject response;
    response["type"] = "search_response";
    response["results"] = results;

    QByteArray responseData = QJsonDocument(response).toJson();
    qDebug() << "即将发送的响应数据：" << responseData;

    client->write(responseData);
    client->flush();
    qDebug() << "响应数据已发送";
}

// 检查用户是否存在
bool ChatServer::userExists(const QString& userId) {
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM users WHERE user_id = ?");
    query.addBindValue(userId);

    if (query.exec()) {
        query.next();
        return query.value(0).toInt() > 0;
    }
    return false;
}

// 保存好友请求到数据库
void ChatServer::saveFriendRequestToDB(const QString& sender, const QString& receiver) {
    QSqlQuery query;
    query.prepare("INSERT INTO friend_requests (sender, receiver, status) VALUES (?, ?, 'pending')");
    query.addBindValue(sender);
    query.addBindValue(receiver);

    if (!query.exec()) {
        qDebug() << "Failed to save friend request:" << query.lastError().text();
    }
}

ChatServer::~ChatServer()
{
    // 关闭所有客户端连接
    for (QTcpSocket* socket : onlineUsers.values()) {
        if (socket) {
            socket->close();
            socket->deleteLater();
        }
    }
    onlineUsers.clear();

    // 关闭数据库连接
    if (db.isOpen()) {
        qDebug() << "关闭数据库连接";
        db.close();
    }

    // 如果有命名连接，移除它
    QString connectionName = db.connectionName();
    if (!connectionName.isEmpty() && QSqlDatabase::contains(connectionName)) {
        qDebug() << "移除数据库连接：" << connectionName;
        QSqlDatabase::removeDatabase(connectionName);
    }
}
