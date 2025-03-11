#include "mainWindow.h"
#include "chat.h"
#include "GroupChat.h"
#include "FindFriendGroup.h"

#include<QListWidget>
#include<QHboxLayout>
#include<QButtonGroup>
#include<QTreeWidget>
#include<QMessageBox>

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QTimer>

mainWindow::mainWindow(const QString& username,QWidget *parent)
	: QWidget(parent),currentUsername(username),
	ui(new Ui::mainWindowClass)
{
	ui->setupUi(this);

    // 连接数据库
    db = QSqlDatabase::addDatabase("QMYSQL", "MainWindowConnection");
    db.setHostName("192.168.217.131");
    db.setPort(3306);
    db.setDatabaseName("chat_app");
    db.setUserName("owl");
    db.setPassword("123456");

    //设置默认无边框
    this->setWindowFlags(Qt::FramelessWindowHint);

    // 初始化消息列表，加载有聊天记录的好友
    updateMessageList();
    // 定时刷新消息列表，每 3 秒刷新一次
    QTimer* messageUpdateTimer = new QTimer(this);
    connect(messageUpdateTimer, &QTimer::timeout, this, &mainWindow::updateMessageList);
    messageUpdateTimer->start(3000);  // 3 秒刷新一次

    setupFriendList();
    setupConnections();
    setupGroupList();
    setupFriendsConnections();

    loadUserInfo(username);

    // 在mainWindow构造函数中添加定时器，定期刷新好友列表
    QTimer* friendListUpdateTimer = new QTimer(this);
    connect(friendListUpdateTimer, &QTimer::timeout, this, &mainWindow::setupFriendList);
    friendListUpdateTimer->start(5000);  // 每5秒刷新一次好友列表

    //绑定按钮到槽函数
    connect(ui->close_1Button, &QPushButton::clicked, this, &mainWindow::closeWindow);
    connect(ui->min_1Button, &QPushButton::clicked, this, &mainWindow::miniWindow);
    connect(ui->friendListTree, &QTreeWidget::itemDoubleClicked, this, &mainWindow::openChatWindow);
    connect(ui->messageListWidget, &QListWidget::itemDoubleClicked, this, &mainWindow::message_openChatWindow);
    connect(ui->groupListTree, &QTreeWidget::itemDoubleClicked, this, &mainWindow::openGroupChatWindow);
    connect(ui->friendAddButton, &QPushButton::clicked, this, &mainWindow::openFindFriendGroupWindow);

}


// 显示个人信息
void mainWindow::loadUserInfo(const QString& username)
{
    if (!db.open()) {
        QMessageBox::critical(this, "错误", "无法连接到数据库！");
        return;
    }

    //查询用户信息
    QSqlQuery query(db);
    query.prepare("SELECT username, avatar, signature FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (query.exec() && query.next()) {
        QString name = query.value("username").toString();
        QString avatarPath = query.value("avatar").toString();
        QString signature = query.value("signature").toString();

        if (avatarPath.startsWith(": /")) {
            avatarPath.replace(": /", ":/");
        }

        // 更新 UI
        ui->nameLabel->setText(name);
        ui->signatureLabel->setText(signature);

        // 头像显示处理
        QPixmap avatarPixmap(avatarPath);
        if (!avatarPixmap.isNull()) {
            // 将头像缩放为QLabel大小，并保持比例
            ui->mainAvatarlabel->setPixmap(avatarPixmap.scaled(ui->mainAvatarlabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            ui->mainAvatarlabel->setScaledContents(true);  // 确保图像缩放填充QLabel
        }
        else {
            qDebug() << "头像加载失败！";
        }
    }
    else {
        qDebug() << "查询失败：" << query.lastError().text();
    }
}


// 好友栏页面自动填充
void mainWindow::setupFriendList()
{
    qDebug() << "开始刷新好友列表...";
    QTreeWidget* treeWidget = ui->friendListTree;
    treeWidget->setColumnCount(1);
    treeWidget->setHeaderHidden(true); // 隐藏表头

    // 清空之前的好友列表
    treeWidget->clear();

    // ====== 创建分组 ======
    QTreeWidgetItem* newFriendGroup = new QTreeWidgetItem(treeWidget, QStringList("新朋友"));
    QTreeWidgetItem* deviceGroup = new QTreeWidgetItem(treeWidget, QStringList("我的设备 1/1"));
    QTreeWidgetItem* friendGroup = new QTreeWidgetItem(treeWidget, QStringList("我的好友"));

    treeWidget->addTopLevelItem(newFriendGroup);
    treeWidget->addTopLevelItem(deviceGroup);
    treeWidget->addTopLevelItem(friendGroup);

    // ====== 数据库查询好友列表 ======
    if (!db.open()) {
        qDebug() << "数据库连接失败，无法刷新好友列表";
        return;
    }
    qDebug() << "数据库连接成功，开始查询好友";

    QSqlQuery query(db);
    query.prepare(
        "SELECT DISTINCT u.username, u.avatar, u.signature "
        "FROM friends f "
        "JOIN users u ON (CASE "
        "    WHEN f.user_id = (SELECT id FROM users WHERE username = :username) THEN f.friend_id "
        "    WHEN f.friend_id = (SELECT id FROM users WHERE username = :username) THEN f.user_id "
        "END) = u.id "
        "WHERE (f.user_id = (SELECT id FROM users WHERE username = :username) "
        "   OR f.friend_id = (SELECT id FROM users WHERE username = :username)) "
        "AND f.status = 'accepted' "
        "AND u.username != :username"  // 排除自己
    );
    query.bindValue(":username", this->currentUsername);
    qDebug() << "当前用户名:" << this->currentUsername;

    if (query.exec()) {
        qDebug() << "好友查询执行成功";
        int friendCount = 0;
        while (query.next()) {
            QString friendName = query.value("username").toString();
            QString avatarPath = query.value("avatar").toString().trimmed();
            avatarPath.replace(": ", ":");  // 修正 ": " 变成 ":"
            QString signature = query.value("signature").toString();

            if (avatarPath.isEmpty()) {
                avatarPath = ":/QQ/image/头像 女孩.png"; // 默认头像
            }
            if (signature.isEmpty()) {
                signature = "这个人很懒，什么都没写";
            }
            addFriendItem(friendGroup, avatarPath, friendName, signature);
            friendCount++;
        }
        qDebug() << "成功加载" << friendCount << "个好友";
    }
    else {
        qDebug() << "查询好友列表失败:" << query.lastError().text();
    }

    db.close();
    qDebug() << "数据库连接已关闭";
    treeWidget->expandAll(); // 默认展开所有分组
}

void mainWindow::addFriendItem(QTreeWidgetItem* parent, const QString& avatar,
    const QString& name, const QString& status)
{
    QWidget* widget = new QWidget();
    QLabel* avatarLabel = new QLabel(widget);
    QLabel* nameLabel = new QLabel(name, widget);
    QLabel* statusLabel = new QLabel(status, widget);

    avatarLabel->setPixmap(QPixmap(avatar).scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    avatarLabel->setFixedSize(40, 40);
    nameLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    statusLabel->setStyleSheet("font-size: 12px; color: gray;");

    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(statusLabel);
    textLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->addWidget(avatarLabel);
    layout->addLayout(textLayout);
    layout->setContentsMargins(5, 5, 5, 5);

    widget->setLayout(layout);

    QTreeWidgetItem* item = new QTreeWidgetItem(parent);
    item->setData(0, Qt::UserRole, name);  // **绑定用户名，避免 findChild 失败**
    parent->addChild(item);
    parent->treeWidget()->setItemWidget(item, 0, widget);
}

//群聊栏构建
void mainWindow::setupGroupList()
{
    QTreeWidget* treeWidget = ui->groupListTree;
    treeWidget->setColumnCount(1);
    treeWidget->setHeaderHidden(true); // 隐藏表头

    if (!db.isOpen()) {
        if (!db.open()) {
            qDebug() << "Database connection failed!";
            return;
        }
    }

    // 修改数据库查询，只查询用户加入的群聊
    QSqlQuery query(db);
    query.prepare(
        "SELECT g.group_id, g.group_name, g.avatar "
        "FROM `groups` g "
        "JOIN group_members gm ON g.group_id = gm.group_id "
        "WHERE gm.user_id = (SELECT id FROM users WHERE username = :username)"
    );
    query.bindValue(":username", currentUsername);

    if (!query.exec()) {
        qDebug() << "查询群聊列表失败：" << query.lastError().text();
        return;
    }

    // 创建群聊分类
    QTreeWidgetItem* groupRoot = new QTreeWidgetItem(treeWidget, QStringList("群聊"));
    treeWidget->addTopLevelItem(groupRoot);

    // 遍历查询结果，添加群聊
    while (query.next()) {
        int groupId = query.value("group_id").toInt();
        QString groupName = query.value("group_name").toString();
        QString avatarPath = query.value("avatar").toString();
        qDebug() << "查询到用户所在群聊：" << groupId << groupName << avatarPath;

        addGroupItem(groupRoot, avatarPath, groupName, groupId);
    }

    treeWidget->expandAll(); // 默认展开所有分组
}

void mainWindow::addGroupItem(QTreeWidgetItem* parent, const QString& avatar,
    const QString& name, int groupId)
{
    QWidget* widget = new QWidget();
    QLabel* avatarLabel = new QLabel(widget);
    QLabel* nameLabel = new QLabel(name, widget);

    avatarLabel->setPixmap(QPixmap(avatar).scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    avatarLabel->setFixedSize(40, 40);
    nameLabel->setStyleSheet("font-size: 14px; font-weight: bold;");

    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->addWidget(avatarLabel);
    layout->addWidget(nameLabel);
    layout->setContentsMargins(5, 5, 5, 5);
    widget->setLayout(layout);

    widget->setLayout(layout);

    // **✅ 创建 QTreeWidgetItem 并存储 `groupId`**
    QTreeWidgetItem* item = new QTreeWidgetItem(parent);
    item->setData(0, Qt::UserRole, groupId);  // **存储群聊 ID**
    item->setText(0, name);  // **设置群聊名称**

    parent->addChild(item);
    parent->treeWidget()->setItemWidget(item, 0, widget);
}

// 消息页面处理
void mainWindow::updateMessageList() {
    // 清空现有列表
    ui->messageListWidget->clear();

    // 确保数据库连接是打开的
    if (!db.isOpen()) {
        if (!db.open()) {
            qDebug() << "数据库连接失败：" << db.lastError().text();
            return;
        }
    }

    // 使用类成员数据库连接
    QSqlQuery query(db);
    query.prepare(
        "SELECT DISTINCT "
        "    CASE "
        "        WHEN sender = :username THEN receiver "
        "        WHEN receiver = :username THEN sender "
        "    END AS friend "
        "FROM messages "
        "WHERE sender = :username OR receiver = :username"
    );
    query.bindValue(":username", currentUsername);
    if (!query.exec()) {
        qDebug() << "SQL 执行失败: " << query.lastError().text();
        return;
    }

    while (query.next()) {
        QString friendName = query.value(0).toString();
        if (friendName == currentUsername) continue;  // 过滤掉自己

        QString avatarPath;
        QString signature;
        QString lastMessage;

        // 获取好友的头像和签名
        QSqlQuery userQuery(db);
        userQuery.prepare("SELECT avatar, signature FROM users WHERE username = :username");
        userQuery.bindValue(":username", friendName);
        if (userQuery.exec() && userQuery.next()) {
            avatarPath = userQuery.value(0).toString();
            signature = userQuery.value(1).toString();
        }

        // 获取好友的最后一条消息
        QSqlQuery messageQuery(db);
        messageQuery.prepare(
            "SELECT content FROM messages "
            "WHERE (sender = :friendName AND receiver = :username) OR (sender = :username AND receiver = :friendName) "
            "ORDER BY timestamp DESC LIMIT 1"
        );
        messageQuery.bindValue(":username", currentUsername);
        messageQuery.bindValue(":friendName", friendName);
        if (messageQuery.exec() && messageQuery.next()) {
            lastMessage = messageQuery.value(0).toString();
        }

        // 处理头像路径
        if (avatarPath.startsWith("/QQ/image/")) {
            avatarPath = ":" + avatarPath;
        }

        // 添加到 UI
        addMessageWidget(friendName, avatarPath, signature, lastMessage);
    }

    // 2. 加载群聊消息记录
    QSqlQuery groupQuery(db);
    groupQuery.prepare(
        "SELECT DISTINCT g.group_id, g.group_name, g.avatar, gm.content "
        "FROM group_members gm_user "
        "JOIN `groups` g ON gm_user.group_id = g.group_id "
        "LEFT JOIN group_messages gm ON g.group_id = gm.group_id "
        "WHERE gm_user.user_id = (SELECT id FROM users WHERE username = :username) "
        "ORDER BY gm.timestamp DESC"
    );
    groupQuery.bindValue(":username", currentUsername);
    if (!groupQuery.exec()) {
        qDebug() << "加载群聊信息失败：" << groupQuery.lastError().text();
        return;
    }

    while (groupQuery.next()) {
        int groupId = groupQuery.value("group_id").toInt();
        QString groupName = groupQuery.value("group_name").toString();
        QString groupAvatarPath = groupQuery.value("avatar").toString();
        QString lastGroupMessage = groupQuery.value("content").toString();

        // 处理群头像路径
        if (groupAvatarPath.startsWith("/QQ/image/")) {
            groupAvatarPath = ":" + groupAvatarPath;
        }
        if (groupAvatarPath.isEmpty()) {
            groupAvatarPath = ":/QQ/image/group_avatar.png"; // 默认群聊头像
        }

        // 添加到 UI
        addMessageWidget(groupName, groupAvatarPath, "群聊", lastGroupMessage);
    }

}



// 将好友信息和最后一条消息添加到消息列表
void mainWindow::addMessageWidget(const QString& friendName, const QString& avatarPath, const QString& signature, const QString& lastMessage) {
    QListWidgetItem* item = new QListWidgetItem(ui->messageListWidget);
    item->setSizeHint(QSize(350, 60));

    // 确保设置了 text 内容
    item->setText(friendName);  // 设置文本为好友名称

    // 存储 friendName，确保后续能正确获取
    item->setData(Qt::UserRole, friendName);

    QWidget* widget = new QWidget();
    QLabel* avatarLabel = new QLabel(widget);
    // Lambda 表达式：将数据库路径转换为 qrc 资源路径
    auto getAvatarPath = [](const QString& dbPath) -> QString {
        QString path = dbPath.trimmed(); // 去除首尾空格

        // 处理之前错误的 ": /" 前缀
        if (path.startsWith(": /")) {
            path.replace(": /", ":/");
        }

        // 如果没有 ":" 前缀，添加它
        if (path.startsWith("/QQ/image/")) {
            path = ":" + path;
        }

        return path;
        };


    // 解析头像路径
    QString resolvedAvatarPath = getAvatarPath(avatarPath);

    if (QFile::exists(resolvedAvatarPath)) {
        avatarLabel->setPixmap(QPixmap(resolvedAvatarPath).scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    else {
        avatarLabel->setPixmap(QPixmap(":/default_avatar.png")); // 使用默认头像
    }


    avatarLabel->setFixedSize(40, 40);

    QLabel* nameLabel = new QLabel(friendName, widget);
    nameLabel->setStyleSheet("font-size: 16px; font-weight: bold;");

    // 这里用最后一条消息代替个性签名
    QLabel* lastMessageLabel = new QLabel(lastMessage, widget);
    lastMessageLabel->setStyleSheet("font-size: 14px; color: gray;");
    lastMessageLabel->setMaximumWidth(250); // 设置最大宽度


    // 使用水平布局
    QHBoxLayout* mainLayout = new QHBoxLayout(widget);
    mainLayout->setContentsMargins(10, 5, 10, 5); // 设置边距
    mainLayout->setSpacing(10); // 设置控件之间的间距

    // 头像部分
    QVBoxLayout* avatarLayout = new QVBoxLayout();
    avatarLayout->addWidget(avatarLabel);
    avatarLayout->setAlignment(Qt::AlignTop); // 头像靠上对齐
    mainLayout->addLayout(avatarLayout);

    // 文字信息部分
    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(lastMessageLabel);  // 用最后一条消息代替个性签名
    textLayout->setSpacing(5); // 设置文字控件之间的间距
    mainLayout->addLayout(textLayout);

    widget->setLayout(mainLayout);

    // 设置样式表
    widget->setStyleSheet(
        "QWidget { background: transparent; }"
        "QWidget:hover { background: #F0F0F0; border-radius: 5px; }"
        "QWidget:selected { background: #D0D0D0; border-radius: 5px; }"
    );

    ui->messageListWidget->setItemWidget(item, widget);
}

//根据按钮切换页面
void mainWindow::setupConnections()
{
    // 确保按钮是 Checkable 的
    ui->messageButton->setCheckable(true);
    ui->friendListButton->setCheckable(true);
    ui->spaceButton->setCheckable(true);

    // 连接按钮的 clicked 信号到对应的槽函数
    connect(ui->messageButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(0);
        ui->messageButton->setChecked(true); // 选中当前按钮
        });

    connect(ui->friendListButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(1);
        ui->friendListButton->setChecked(true);
        ui->FriendStackedWidget->setCurrentIndex(0);
        ui->friendButton->setChecked(true);
        });

    connect(ui->spaceButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(2);
        ui->spaceButton->setChecked(true);
        });

    // 互斥按钮组，保证只有一个按钮处于选中状态
    QButtonGroup* buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(ui->messageButton);
    buttonGroup->addButton(ui->friendListButton);
    buttonGroup->addButton(ui->spaceButton);
    buttonGroup->setExclusive(true); // 互斥选择

    // ===== 设置默认界面为 Message =====
    ui->stackedWidget->setCurrentIndex(0); // 默认显示消息页面
    ui->messageButton->setChecked(true);   // 选中消息按钮

}

//实现好友页面的好友与群聊的切换
void mainWindow::setupFriendsConnections()
{
    // 确保按钮是 Checkable 的
    ui->friendButton->setCheckable(true);
    ui->groupButton->setCheckable(true);

    // 连接按钮点击事件到 lambda 进行切换
    connect(ui->friendButton, &QPushButton::clicked, this, [this]() {
        ui->FriendStackedWidget->setCurrentIndex(0); // 切换到好友界面
        ui->friendButton->setChecked(true);
        });

    connect(ui->groupButton, &QPushButton::clicked, this, [this]() {
        ui->FriendStackedWidget->setCurrentIndex(1); // 切换到群聊界面
        ui->groupButton->setChecked(true);
        });

    // 互斥按钮组，保证只有一个按钮被选中
    QButtonGroup* buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(ui->friendButton);
    buttonGroup->addButton(ui->groupButton);
    buttonGroup->setExclusive(true); // 互斥选择
}

//实现鼠标拖动界面
void mainWindow::mousePressEvent(QMouseEvent* event)
{
    lastPos = event->globalPosition().toPoint() - this->pos();
}

void mainWindow::mouseMoveEvent(QMouseEvent* event)
{
    this->move(event->globalPosition().toPoint() - lastPos);
}

// 打开聊天界面
void mainWindow::openChatWindow(QTreeWidgetItem* item, int column) {
    // 直接从 QTreeWidgetItem 获取用户名
    QString chatPartner = item->data(0, Qt::UserRole).toString();

    if (chatPartner.isEmpty()) {
        return;
    }

    // 如果已经有该用户的聊天窗口，则激活
    if (chatWindows.contains(chatPartner)) {
        chatWindows[chatPartner]->activateWindow();
        return;
    }

    // 否则新建一个聊天窗口
    chat* c = new chat();
    c->setWindowTitle(chatPartner);
    c->setUsername(this->currentUsername); // 将当前窗口的 username 传递给聊天窗口
    c->setCurrentChatPartner(chatPartner); // 设置当前聊天对象
    qDebug() << "当前用户：" << this->currentUsername;
    c->show();

    // 监听窗口关闭信号
    connect(c, &chat::chatClosed, this, &mainWindow::onChatClosed);

    // 记录窗口，方便管理
    chatWindows[chatPartner] = c;
}


// 通过消息打开聊天窗口
void mainWindow::message_openChatWindow(QListWidgetItem* item) {
    if (!item) {
        return;
    }

    QString name = item->text();  // 获取点击的名字

    if (name.isEmpty()) {
        return;
    }

    // 获取好友 ID 或群聊 ID
    int id = item->data(Qt::UserRole).toInt(); // 可能是好友 ID 或群聊 ID

    // 检查是否为群聊
    QSqlQuery query(db);
    query.prepare("SELECT group_id FROM `groups` WHERE group_name = :name");
    query.bindValue(":name", name);
    if (query.exec() && query.next()) {
        int groupId = query.value(0).toInt();

        // **创建一个 QTreeWidgetItem 来兼容 openGroupChatWindow**
        QTreeWidgetItem* groupItem = new QTreeWidgetItem();
        groupItem->setData(0, Qt::UserRole, groupId);
        groupItem->setText(0, name);

        openGroupChatWindow(groupItem);  // 调用打开群聊窗口
        return;
    }

    // 否则是私聊
    QTreeWidgetItem* friendItem = new QTreeWidgetItem();
    friendItem->setData(0, Qt::UserRole, name);
    friendItem->setText(0, name);

    openChatWindow(friendItem, 0);  // 调用打开私聊窗口
}



//打开群聊窗口
void mainWindow::openGroupChatWindow(QTreeWidgetItem* item)
{
    // 获取群聊 ID 和群聊名称
    int groupId = item->data(0, Qt::UserRole).toInt();
    QString groupName = item->text(0); // 获取群聊名称

    if (groupId == 0 || groupName.isEmpty()) {
        qDebug() << "无效的群聊信息";
        return;
    }

    // 如果群聊窗口已打开，则激活
    if (groupChatWindows.contains(groupId)) {
        groupChatWindows[groupId]->activateWindow();
        return;
    }

    // 创建新的群聊窗口
    GroupChat* gc = new GroupChat();
    gc->setWindowTitle(groupName);
    gc->setGroupId(groupId);  // 传递群 ID
    gc->setGroupName(groupName);
    gc->setUsername(this->currentUsername); // 传递当前用户名
    gc->show();

    // 通过 setGroupChat 方法设置群聊并加载历史记录
    gc->setGroupChat(groupId, groupName);

    // 监听窗口关闭信号
    connect(gc, &GroupChat::groupChatClosed, this, &mainWindow::onGroupChatClosed);

    // 记录窗口
    groupChatWindows[groupId] = gc;
}

// 打开添加好友或加入群聊窗口
void mainWindow::openFindFriendGroupWindow()
{
    FindFriendGroup* ffg = new FindFriendGroup();
    ffg->setUsername(this->currentUsername);
    ffg->setAttribute(Qt::WA_DeleteOnClose);
    ffg->show();
}

//删除群聊窗口
void mainWindow::onGroupChatClosed(int groupId)
{
    if (groupChatWindows.contains(groupId)) {
        groupChatWindows.remove(groupId);
        qDebug() << "群聊窗口关闭：" << groupId;
    }
}

void mainWindow::onChatClosed(const QString& chatPartner) {
    if (chatWindows.contains(chatPartner)) {
        chatWindows.remove(chatPartner);
    }
}

//最小化页面和关闭页面
void mainWindow::closeWindow()
{
    this->close();
}

void mainWindow::miniWindow()
{
    this->showMinimized();
}

void mainWindow::closeEvent(QCloseEvent* event)
{
    QApplication::quit();
}

mainWindow::~mainWindow()
{
    if (db.isOpen()) {
        db.close();
    }
    QSqlDatabase::removeDatabase("MainWindowConnection");
}
