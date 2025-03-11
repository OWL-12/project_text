#include "FindFriendGroup.h"

#include <QButtonGroup>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QtSql/QSqlQuery>
#include <QTimer>
#include <QMessageBox>



FindFriendGroup::FindFriendGroup(QWidget* parent)
    : QWidget(parent),
    ui(new Ui::FindFriendGroupClass)
{
    ui->setupUi(this);

    // 设置默认无边框
    this->setWindowFlags(Qt::FramelessWindowHint);

    // 先确保数据库连接
    connectDatabase();

    setupConnections();

    // 搜索功能按钮绑定
    this->connect(ui->requestSendButton, &QPushButton::clicked, this, &FindFriendGroup::onSearchButtonClicked);

    // 绑定按钮点击信号到槽函数
    connect(ui->miniButton, &QPushButton::clicked, this, &FindFriendGroup::minimizeWindow);
    connect(ui->closeButton, &QPushButton::clicked, this, &FindFriendGroup::closeWindow);
    
    // 初始加载好友请求和群聊申请
    loadFriendRequests();
    loadGroupRequests();
}

// 页面切换功能
void FindFriendGroup::setupConnections()
{
    // 确保按钮可以被选中
    ui->searchButton->setCheckable(true);
    ui->friendRequest->setCheckable(true);
    ui->groupRequestsButton->setCheckable(true);

    // 连接按钮的 clicked 信号到对应的槽函数
    connect(ui->searchButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(0);
        });

    connect(ui->friendRequest, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(2);
        loadFriendRequests();
        });

    connect(ui->groupRequestsButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(1);
        loadGroupRequests();  // 添加这一行，确保加载群聊申请
        qDebug() << "切换到群聊申请页面，加载群聊申请";
        });

    // 互斥按钮组，保证只有一个按钮处于选中状态
    QButtonGroup* buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(ui->searchButton);
    buttonGroup->addButton(ui->friendRequest);
    buttonGroup->addButton(ui->groupRequestsButton);
    buttonGroup->setExclusive(true); // 确保按钮互斥

    // 设置默认界面为"搜索"页面
    ui->stackedWidget->setCurrentIndex(0);
    ui->searchButton->setChecked(true);

    // 设置样式，显示当前选中的按钮
    QString buttonStyle = R"(
        QPushButton {
    border: none;
    background-color: #4A4A8D;
    color: white;
    font-size: 16px;
}

QPushButton:checked {
    background-color: #5C5C9E; /* 选中时颜色加深 */
    border-bottom: 4px solid white; /* 也可以用纯白色底边 */
}

QPushButton:hover {
    background-color: #3C3C6D;
}
    )";
    ui->searchButton->setStyleSheet(buttonStyle);
    ui->friendRequest->setStyleSheet(buttonStyle);
    ui->groupRequestsButton->setStyleSheet(buttonStyle);
}

// 搜索功能的实现
void FindFriendGroup::onSearchButtonClicked()
{
    QString searchText = ui->searchLineEdit->text().trimmed();
    if (searchText.isEmpty()) return;

    ui->searchResultList->clear();  // 清空之前的搜索结果

    // 确保数据库已连接
    if (!db.isOpen()) {
        connectDatabase();
    }

    if (!db.isOpen()) {
        QMessageBox::warning(this, "错误", "无法连接到数据库");
        return;
    }

    qDebug() << "发送好友请求前的currentUserId：" << this->currentUserId;

    // 先搜索用户
    QSqlQuery userQuery(db);
    userQuery.prepare("SELECT username, avatar, id FROM users WHERE username LIKE BINARY :name");
    userQuery.bindValue(":name", "%" + searchText + "%");

    if (userQuery.exec()) {
        while (userQuery.next()) {
            QString username = userQuery.value("username").toString();
            QString avatarPath = userQuery.value("avatar").toString();
            int userId = userQuery.value("id").toInt();

            if (avatarPath.startsWith(": ")) {
                avatarPath.remove(1, 1);  // 删除索引 1 处的空格
            }

            QWidget* itemWidget = new QWidget();
            QHBoxLayout* layout = new QHBoxLayout(itemWidget);
            QLabel* avatarLabel = new QLabel();
            QLabel* usernameLabel = new QLabel(username);
            QPushButton* addButton = new QPushButton("添加");

            // 判断是否是当前用户
            if (userId == this->currentUserId) {
                usernameLabel->setText(username + " (这是你)");
                addButton->setEnabled(false);
                addButton->setStyleSheet("QPushButton { background-color: #cccccc; }");
            }
            else {
                // 判断是否是已添加的好友
                QSqlQuery checkFriendQuery(db);
                checkFriendQuery.prepare("SELECT * FROM friends WHERE ((user_id = :user_id AND friend_id = :friend_id) OR (user_id = :friend_id AND friend_id = :user_id)) AND status = 'accepted'");
                checkFriendQuery.bindValue(":user_id", this->currentUserId);
                checkFriendQuery.bindValue(":friend_id", userId);

                if (checkFriendQuery.exec() && checkFriendQuery.next()) {
                    // 已是好友
                    addButton->setText("已添加");
                    addButton->setEnabled(false);
                    addButton->setStyleSheet("QPushButton { background-color: #cccccc; }");
                    usernameLabel->setText(username + " (已是好友)");
                }
                else {
                    // 检查是否有待处理的好友请求
                    QSqlQuery checkPendingQuery(db);
                    checkPendingQuery.prepare("SELECT status FROM friend_requests WHERE sender_id = :sender_id AND receiver_id = :receiver_id AND status = 'pending'");
                    checkPendingQuery.bindValue(":sender_id", this->currentUserId);
                    checkPendingQuery.bindValue(":receiver_id", userId);

                    if (checkPendingQuery.exec() && checkPendingQuery.next()) {
                        addButton->setText("等待确认");
                        addButton->setEnabled(false);
                        addButton->setStyleSheet("QPushButton { background-color: #cccccc; }");
                        usernameLabel->setText(username + " (请求待确认)");
                    }
                }
            }

            // 设置用户头像
            QPixmap pixmap(avatarPath);
            if (pixmap.isNull()) {
                pixmap.load(":/QQ/image/头像 女孩.png");  // 使用默认头像
            }
            avatarLabel->setPixmap(pixmap.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            avatarLabel->setFixedSize(40, 40);

            layout->addWidget(avatarLabel);
            layout->addWidget(usernameLabel);
            layout->addWidget(addButton);
            layout->setStretch(1, 1);
            layout->setSpacing(10);
            itemWidget->setLayout(layout);

            QListWidgetItem* item = new QListWidgetItem();
            item->setSizeHint(itemWidget->sizeHint());
            ui->searchResultList->addItem(item);
            ui->searchResultList->setItemWidget(item, itemWidget);

            // 连接添加按钮的槽函数
            connect(addButton, &QPushButton::clicked, this, [this, userId]() {
                onAddFriendClicked(userId);
                });
        }
    }
    else {
        qDebug() << "查询用户失败:" << userQuery.lastError().text();
    }

    // 然后搜索群聊
    QSqlQuery groupQuery(db);
    groupQuery.prepare("SELECT group_name, avatar, group_id FROM `groups` WHERE group_name LIKE BINARY :name");
    groupQuery.bindValue(":name", "%" + searchText + "%");

    if (groupQuery.exec()) {
        while (groupQuery.next()) {
            QString groupName = groupQuery.value("group_name").toString();
            QString avatarPath = groupQuery.value("avatar").toString();
            int groupId = groupQuery.value("group_id").toInt();

            if (avatarPath.startsWith(": ")) {
                avatarPath.remove(1, 1);
            }

            QWidget* itemWidget = new QWidget();
            QHBoxLayout* layout = new QHBoxLayout(itemWidget);
            QLabel* avatarLabel = new QLabel();
            QLabel* groupNameLabel = new QLabel(groupName);
            QPushButton* joinButton = new QPushButton("加入");

            // 判断当前用户是否已加入该群聊
            QSqlQuery checkGroupQuery(db);
            checkGroupQuery.prepare("SELECT * FROM group_members WHERE group_id = :group_id AND user_id = :user_id");
            checkGroupQuery.bindValue(":group_id", groupId);
            checkGroupQuery.bindValue(":user_id", this->currentUserId);

            bool isMember = false;
            if (checkGroupQuery.exec() && checkGroupQuery.next()) {
                isMember = true;
            }

            // 检查是否有待处理的加群申请
            QSqlQuery checkRequestQuery(db);
            checkRequestQuery.prepare("SELECT status FROM group_join_requests "
                "WHERE group_id = :group_id AND user_id = :user_id "
                "AND status = 'pending'");
            checkRequestQuery.bindValue(":group_id", groupId);
            checkRequestQuery.bindValue(":user_id", this->currentUserId);

            bool hasPendingRequest = false;
            if (checkRequestQuery.exec() && checkRequestQuery.next()) {
                hasPendingRequest = true;
            }

            // 设置群聊头像
            QPixmap pixmap(avatarPath);
            if (pixmap.isNull()) {
                pixmap.load(":/QQ/image/群聊.png");  // 使用默认群聊头像
            }
            avatarLabel->setPixmap(pixmap.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            avatarLabel->setFixedSize(40, 40);

            // 根据状态设置按钮和标签
            if (isMember) {
                joinButton->setText("已加入");
                joinButton->setEnabled(false);
                joinButton->setStyleSheet("QPushButton { background-color: #cccccc; }");
                groupNameLabel->setText(groupName + " (已加入)");
            }
            else if (hasPendingRequest) {
                joinButton->setText("等待审核");
                joinButton->setEnabled(false);
                joinButton->setStyleSheet("QPushButton { background-color: #cccccc; }");
                groupNameLabel->setText(groupName + " (申请审核中)");
            }

            layout->addWidget(avatarLabel);
            layout->addWidget(groupNameLabel);
            layout->addWidget(joinButton);
            layout->setStretch(1, 1);
            layout->setSpacing(10);
            itemWidget->setLayout(layout);

            QListWidgetItem* item = new QListWidgetItem();
            item->setSizeHint(itemWidget->sizeHint());
            ui->searchResultList->addItem(item);
            ui->searchResultList->setItemWidget(item, itemWidget);

            // 连接加入按钮的槽函数
            connect(joinButton, &QPushButton::clicked, this, [this, groupId, groupName, joinButton, groupNameLabel]() {
                // 检查是否已经是群成员
                QSqlQuery checkMemberQuery(db);
                checkMemberQuery.prepare("SELECT * FROM group_members WHERE group_id = :group_id AND user_id = :user_id");
                checkMemberQuery.bindValue(":group_id", groupId);
                checkMemberQuery.bindValue(":user_id", this->currentUserId);

                if (checkMemberQuery.exec() && checkMemberQuery.next()) {
                    QMessageBox::information(this, "提示", "您已经是该群成员");
                    return;
                }

                // 获取验证信息
                bool ok;
                QString message = QInputDialog::getText(this,
                    "加入群聊",
                    "请输入验证信息:",
                    QLineEdit::Normal,
                    "你好，我想加入群聊", &ok);

                if (!ok || message.trimmed().isEmpty()) {
                    return;
                }

                // 发送加群请求
                QSqlQuery joinQuery(db);
                joinQuery.prepare("INSERT INTO group_join_requests (group_id, user_id, message) "
                    "VALUES (:group_id, :user_id, :message)");
                joinQuery.bindValue(":group_id", groupId);
                joinQuery.bindValue(":user_id", this->currentUserId);
                joinQuery.bindValue(":message", message);

                if (joinQuery.exec()) {
                    QMessageBox::information(this, "提示", "群聊申请已发送，请等待群主审核");
                    // 更新UI显示
                    joinButton->setEnabled(false);
                    joinButton->setText("等待审核");
                    joinButton->setStyleSheet("QPushButton { background-color: #cccccc; }");
                    groupNameLabel->setText(groupName + " (申请审核中)");
                }
                else {
                    QMessageBox::warning(this, "错误", "申请发送失败：" + joinQuery.lastError().text());
                }
                });
        }
    }
    else {
        qDebug() << "查询群聊失败:" << groupQuery.lastError().text();
    }

    // 如果没有搜索结果，显示提示信息
    if (ui->searchResultList->count() == 0) {
        QListWidgetItem* emptyItem = new QListWidgetItem("未找到相关用户或群聊");
        emptyItem->setTextAlignment(Qt::AlignCenter);
        ui->searchResultList->addItem(emptyItem);
    }
}
// 添加好友
void FindFriendGroup::onAddFriendClicked(int targetUserId)
{
    if (targetUserId == this->currentUserId) return;  // 不能加自己

    // 确保数据库已连接
    if (!db.isOpen()) {
        connectDatabase();
    }

    bool ok;
    QString message = QInputDialog::getText(this, "发送好友申请", "请输入验证信息:", QLineEdit::Normal, "你好，我想加你为好友", &ok);
    if (!ok || message.trimmed().isEmpty()) {
        qDebug() << "用户取消了申请或未输入留言";
        return;
    }

    QSqlQuery checkRequestQuery(db);
    checkRequestQuery.prepare("SELECT status FROM friend_requests WHERE sender_id = :sender_id AND receiver_id = :receiver_id");
    checkRequestQuery.bindValue(":sender_id", this->currentUserId);
    checkRequestQuery.bindValue(":receiver_id", targetUserId);
    checkRequestQuery.exec();

    if (checkRequestQuery.next()) {
        QString status = checkRequestQuery.value("status").toString();

        if (status == "pending") {
            qDebug() << "好友申请已发送，等待对方确认";
            return;
        }
        else if (status == "rejected") {
            // 先删除旧记录
            QSqlQuery deleteQuery(db);
            deleteQuery.prepare("DELETE FROM friend_requests WHERE sender_id = :sender_id AND receiver_id = :receiver_id");
            deleteQuery.bindValue(":sender_id", this->currentUserId);
            deleteQuery.bindValue(":receiver_id", targetUserId);
            deleteQuery.exec();
        }
    }

    // 在绑定前打印值和类型
    qDebug() << "绑定前 currentUserId 值:" << this->currentUserId
        << "类型:" << QVariant(this->currentUserId).typeName();

    // 使用命名参数绑定
    QSqlQuery insertQuery(db);
    insertQuery.prepare("INSERT INTO friend_requests (sender_id, receiver_id, message) VALUES (:sender_id, :receiver_id, :message)");
    insertQuery.bindValue(":sender_id", this->currentUserId);
    insertQuery.bindValue(":receiver_id", targetUserId);
    insertQuery.bindValue(":message", message);

    // 检查绑定后的参数实际值（通过反射或间接方式）
    QVariant boundValue = insertQuery.boundValue(":sender_id");
    qDebug() << "绑定后 sender_id 值:" << boundValue
        << "类型:" << boundValue.typeName();

    if (insertQuery.exec()) {
        qDebug() << "好友申请发送成功";
    }
    else {
        qDebug() << "发送好友申请失败：" << insertQuery.lastError().text();
    }
}


void FindFriendGroup::loadFriendRequests()
{
    ui->friendRequestList->clear();  // 清空之前的请求

    QSqlQuery query;
    query.prepare("SELECT fr.sender_id, u.username, u.avatar, fr.message "
        "FROM friend_requests fr "
        "JOIN users u ON fr.sender_id = u.id "
        "WHERE fr.receiver_id = :receiver_id AND fr.status = 'pending'");
    query.bindValue(":receiver_id", this->currentUserId);

    if (!query.exec()) {
        qDebug() << "查询好友申请失败：" << query.lastError().text();
        return;
    }

    while (query.next()) {
        int senderId = query.value("sender_id").toInt();
        QString username = query.value("username").toString();
        QString avatarPath = query.value("avatar").toString();
        QString message = query.value("message").toString();

        // 调试输出
        qDebug() << "加载好友请求 - 用户:" << username << "头像路径:" << avatarPath;

        // 处理头像路径格式
        if (avatarPath.startsWith(": ")) {
            avatarPath = ":" + avatarPath.mid(2);  // 将": /QQ" 转换为 ":/QQ"
        }

        QWidget* itemWidget = new QWidget();
        QHBoxLayout* layout = new QHBoxLayout(itemWidget);
        QLabel* avatarLabel = new QLabel();
        QLabel* nameLabel = new QLabel(username + "： " + message);
        QPushButton* acceptButton = new QPushButton("接受");
        QPushButton* rejectButton = new QPushButton("拒绝");

        // 设置头像
        QPixmap pixmap(avatarPath);
        if (pixmap.isNull()) {
            qDebug() << "头像加载失败，路径:" << avatarPath;
            // 使用默认头像
            pixmap.load(":/images/default_avatar.png");
        }
        avatarLabel->setPixmap(pixmap.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        // 绑定按钮事件
        connect(acceptButton, &QPushButton::clicked, this, [this, senderId]() { acceptFriendRequest(senderId); });
        connect(rejectButton, &QPushButton::clicked, this, [this, senderId]() { rejectFriendRequest(senderId); });

        layout->addWidget(avatarLabel);
        layout->addWidget(nameLabel);
        layout->addWidget(acceptButton);
        layout->addWidget(rejectButton);
        layout->setStretch(1, 1);
        itemWidget->setLayout(layout);

        QListWidgetItem* item = new QListWidgetItem();
        item->setSizeHint(itemWidget->sizeHint());
        ui->friendRequestList->addItem(item);  // 将请求添加到 friendRequestList 中
        ui->friendRequestList->setItemWidget(item, itemWidget);
    }
}



// 处理接受好友申请的功能
void FindFriendGroup::acceptFriendRequest(int senderId)
{
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE friend_requests SET status = 'accepted' WHERE sender_id = :sender_id AND receiver_id = :receiver_id");
    updateQuery.bindValue(":sender_id", senderId);
    updateQuery.bindValue(":receiver_id", this->currentUserId);

    if (!updateQuery.exec()) {
        qDebug() << "接受好友请求失败：" << updateQuery.lastError().text();
        return;
    }

    // 在 friends 表中插入好友关系
    QSqlQuery insertQuery(db);
    insertQuery.prepare("INSERT INTO friends (user_id, friend_id, status) VALUES (:user_id, :friend_id, 'accepted')");
    insertQuery.bindValue(":user_id", this->currentUserId);
    insertQuery.bindValue(":friend_id", senderId);

    if (insertQuery.exec()) {
        qDebug() << "好友添加成功";
    }
    else {
        qDebug() << "好友添加失败：" << insertQuery.lastError().text();
    }

    loadFriendRequests(); // 重新加载好友请求
}

// 处理拒绝好友申请的功能
void FindFriendGroup::rejectFriendRequest(int senderId)
{
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE friend_requests SET status = 'rejected' WHERE sender_id = :sender_id AND receiver_id = :receiver_id");
    updateQuery.bindValue(":sender_id", senderId);
    updateQuery.bindValue(":receiver_id", this->currentUserId);

    if (updateQuery.exec()) {
        qDebug() << "已拒绝好友申请";
    }
    else {
        qDebug() << "拒绝好友申请失败：" << updateQuery.lastError().text();
    }

    loadFriendRequests(); // 重新加载好友请求列表
}

// 加载群聊申请列表
void FindFriendGroup::loadGroupRequests()
{
    // 清空之前的请求列表
    ui->groupRequestList->clear();
    
    qDebug() << "===== 开始加载群聊申请 =====";
    qDebug() << "当前用户ID:" << this->currentUserId;
    
    // 确保数据库已连接
    if (!db.isOpen()) {
        connectDatabase();
    }

    if (!db.isOpen()) {
        QMessageBox::warning(this, "错误", "无法连接到数据库");
        qDebug() << "加载群聊申请失败: 数据库未连接";
        return;
    }

    // 查询当前用户管理的群聊
    QSqlQuery ownerQuery(db);
    ownerQuery.prepare("SELECT group_id FROM `groups` WHERE owner_id = :user_id");
    ownerQuery.bindValue(":user_id", this->currentUserId);
    
    qDebug() << "执行查询: SELECT group_id FROM `groups` WHERE owner_id =" << this->currentUserId;

    QList<int> managedGroups;
    if (ownerQuery.exec()) {
        while (ownerQuery.next()) {
            int groupId = ownerQuery.value("group_id").toInt();
            managedGroups.append(groupId);
            qDebug() << "找到管理的群聊:" << groupId;
        }
    }
    else {
        qDebug() << "查询管理的群聊失败:" << ownerQuery.lastError().text();
    }

    qDebug() << "管理的群聊数量:" << managedGroups.size();

    if (managedGroups.isEmpty()) {
        QListWidgetItem* emptyItem = new QListWidgetItem("您不是任何群的群主，无法查看群聊申请");
        emptyItem->setTextAlignment(Qt::AlignCenter);
        ui->groupRequestList->addItem(emptyItem);
        qDebug() << "用户不是任何群的群主，无法查看群聊申请";
        return;  // 如果用户不是任何群的群主，直接返回
    }

    // 构建群组ID字符串
    QString groupIds;
    groupIds = QString::number(managedGroups[0]);
    for (int i = 1; i < managedGroups.size(); ++i) {
        groupIds += "," + QString::number(managedGroups[i]);
    }

    qDebug() << "查询的群组IDs:" << groupIds;

    // 查询这些群的加入请求
    QString queryStr = "SELECT r.request_id, r.group_id, r.user_id, r.message, "
        "g.group_name, u.username, u.avatar "
        "FROM group_join_requests r "
        "JOIN `groups` g ON r.group_id = g.group_id "
        "JOIN users u ON r.user_id = u.id "
        "WHERE r.group_id IN (" + groupIds + ") AND r.status = 'pending'";
    
    qDebug() << "执行查询:" << queryStr;
    
    QSqlQuery requestQuery(db);
    requestQuery.prepare(queryStr);

    int requestCount = 0;
    if (requestQuery.exec()) {
        while (requestQuery.next()) {
            requestCount++;
            int requestId = requestQuery.value("request_id").toInt();
            int groupId = requestQuery.value("group_id").toInt();
            int userId = requestQuery.value("user_id").toInt();
            QString message = requestQuery.value("message").toString();
            QString groupName = requestQuery.value("group_name").toString();
            QString username = requestQuery.value("username").toString();
            QString avatarPath = requestQuery.value("avatar").toString();

            qDebug() << "找到申请:" << requestId << "用户:" << username << "群组:" << groupName;

            // 创建请求项的UI - 使用QGridLayout而不是QHBoxLayout
            QWidget* itemWidget = new QWidget();
            QGridLayout* layout = new QGridLayout(itemWidget);
            layout->setSpacing(10);
            layout->setContentsMargins(10, 5, 10, 5);

            // 处理头像
            QLabel* avatarLabel = new QLabel();
            if (avatarPath.startsWith(": ")) {
                avatarPath = ":" + avatarPath.mid(2);
            }
            QPixmap pixmap(avatarPath);
            if (pixmap.isNull()) {
                // 如果头像加载失败，使用默认头像
                pixmap.load(":/QQ/image/头像 女孩.png");
            }
            avatarLabel->setPixmap(pixmap.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            avatarLabel->setFixedSize(40, 40);

            // 创建信息标签
            QLabel* infoLabel = new QLabel(QString("%1 申请加入群聊 %2\n%3").arg(username, groupName, message));
            infoLabel->setWordWrap(true);  // 允许文本换行

            // 创建按钮
            QPushButton* acceptButton = new QPushButton("接受");
            QPushButton* rejectButton = new QPushButton("拒绝");

            // 设置按钮样式
            acceptButton->setStyleSheet("background-color: #4CAF50; color: white; padding: 5px 10px; border: none; border-radius: 3px;");
            rejectButton->setStyleSheet("background-color: #f44336; color: white; padding: 5px 10px; border: none; border-radius: 3px;");

            // 设置按钮大小
            acceptButton->setFixedSize(60, 30);
            rejectButton->setFixedSize(60, 30);

            // 添加组件到网格布局
            layout->addWidget(avatarLabel, 0, 0, 2, 1);  // 头像占据左侧两行
            layout->addWidget(infoLabel, 0, 1, 1, 2);    // 信息标签占据中间区域
            layout->addWidget(acceptButton, 1, 1);       // 接受按钮在下方左侧
            layout->addWidget(rejectButton, 1, 2);       // 拒绝按钮在下方右侧
            layout->setColumnStretch(1, 1);              // 让信息标签区域可以伸展

            // 连接按钮信号
            connect(acceptButton, &QPushButton::clicked, this, [this, requestId, groupId, userId]() {
                qDebug() << "接受按钮被点击 - 请求ID:" << requestId << "群组ID:" << groupId << "用户ID:" << userId;
                handleGroupRequest(requestId, groupId, userId, true);
            });
            
            connect(rejectButton, &QPushButton::clicked, this, [this, requestId, groupId, userId]() {
                qDebug() << "拒绝按钮被点击 - 请求ID:" << requestId << "群组ID:" << groupId << "用户ID:" << userId;
                handleGroupRequest(requestId, groupId, userId, false);
            });

            // 设置项目部件
            QListWidgetItem* item = new QListWidgetItem();
            item->setSizeHint(QSize(itemWidget->sizeHint().width(), 80));  // 设置更大的高度
            ui->groupRequestList->addItem(item);
            ui->groupRequestList->setItemWidget(item, itemWidget);

            // 设置交替背景色
            if (ui->groupRequestList->count() % 2 == 0) {
                itemWidget->setStyleSheet("background-color: #f5f5f5;");
            }
            else {
                itemWidget->setStyleSheet("background-color: white;");
            }
        }
    }
    else {
        qDebug() << "查询群聊申请失败:" << requestQuery.lastError().text();
    }

    qDebug() << "找到的申请数量:" << requestCount;

    // 如果没有请求，显示提示信息
    if (ui->groupRequestList->count() == 0) {
        QListWidgetItem* emptyItem = new QListWidgetItem("暂无群聊申请");
        emptyItem->setTextAlignment(Qt::AlignCenter);
        ui->groupRequestList->addItem(emptyItem);
        qDebug() << "没有找到任何群聊申请";
    }
    
    qDebug() << "===== 群聊申请加载完成 =====";
}
// 处理群聊申请
void FindFriendGroup::handleGroupRequest(int requestId, int groupId, int userId, bool accept)
{
    qDebug() << "\n===== 开始处理群聊申请 =====";
    qDebug() << "请求ID:" << requestId;
    qDebug() << "群组ID:" << groupId;
    qDebug() << "用户ID:" << userId;
    qDebug() << "是否接受:" << accept;
    
    // 确保数据库已连接
    if (!db.isOpen()) {
        qDebug() << "数据库未连接，尝试重新连接...";
        connectDatabase();
    }

    if (!db.isOpen()) {
        qDebug() << "数据库连接失败！";
        QMessageBox::warning(this, "错误", "无法连接到数据库");
        return;
    }
    qDebug() << "数据库连接状态：已连接";
    
    // 开始事务
    if (!db.transaction()) {
        qDebug() << "开始事务失败:" << db.lastError().text();
        return;
    }
    qDebug() << "事务开始成功";

    bool success = true;
    QString status = accept ? "accepted" : "rejected";

    // 更新请求状态
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE group_join_requests SET status = :status WHERE request_id = :request_id");
    updateQuery.bindValue(":status", status);
    updateQuery.bindValue(":request_id", requestId);

    qDebug() << "执行更新请求状态SQL:";
    qDebug() << "SQL:" << updateQuery.lastQuery();
    qDebug() << "参数 - status:" << status << "request_id:" << requestId;

    if (!updateQuery.exec()) {
        success = false;
        qDebug() << "更新请求状态失败:" << updateQuery.lastError().text();
    } else {
        qDebug() << "更新请求状态成功，受影响行数:" << updateQuery.numRowsAffected();
    }

    if (accept) {
        // 检查用户是否已经是群成员
        QSqlQuery checkMemberQuery(db);
        checkMemberQuery.prepare("SELECT * FROM group_members WHERE group_id = :group_id AND user_id = :user_id");
        checkMemberQuery.bindValue(":group_id", groupId);
        checkMemberQuery.bindValue(":user_id", userId);
        
        qDebug() << "检查用户是否已是群成员:";
        qDebug() << "SQL:" << checkMemberQuery.lastQuery();
        qDebug() << "参数 - group_id:" << groupId << "user_id:" << userId;
        
        if (checkMemberQuery.exec()) {
            if (checkMemberQuery.next()) {
                qDebug() << "用户已经是群成员";
                success = false;
            } else {
                // 如果接受，将用户添加到群成员中
                QSqlQuery addMemberQuery(db);
                addMemberQuery.prepare("INSERT INTO group_members (group_id, user_id) VALUES (:group_id, :user_id)");
                addMemberQuery.bindValue(":group_id", groupId);
                addMemberQuery.bindValue(":user_id", userId);

                qDebug() << "执行添加群成员SQL:";
                qDebug() << "SQL:" << addMemberQuery.lastQuery();
                qDebug() << "参数 - group_id:" << groupId << "user_id:" << userId;

                if (!addMemberQuery.exec()) {
                    success = false;
                    qDebug() << "添加群成员失败:" << addMemberQuery.lastError().text();
                } else {
                    qDebug() << "添加群成员成功，受影响行数:" << addMemberQuery.numRowsAffected();
                }
            }
        } else {
            qDebug() << "检查群成员状态失败:" << checkMemberQuery.lastError().text();
            success = false;
        }

        // 发送系统消息
        QSqlQuery userQuery(db);
        userQuery.prepare("SELECT username FROM users WHERE id = :user_id");
        userQuery.bindValue(":user_id", userId);

        if (userQuery.exec() && userQuery.next()) {
            QString newMemberName = userQuery.value("username").toString();
            QSqlQuery systemMsgQuery(db);
            systemMsgQuery.prepare("INSERT INTO group_messages (group_id, sender, content, timestamp) VALUES (:group_id, :sender, :content, NOW())");
            systemMsgQuery.bindValue(":group_id", groupId);
            systemMsgQuery.bindValue(":sender", "系统消息");  // 使用"系统消息"作为发送者
            QString welcomeMsg = QString("欢迎新成员 %1 加入群聊！").arg(newMemberName);
            systemMsgQuery.bindValue(":content", welcomeMsg);

            qDebug() << "执行发送系统消息SQL:";
            qDebug() << "SQL:" << systemMsgQuery.lastQuery();
            qDebug() << "参数 - group_id:" << groupId;
            qDebug() << "参数 - sender: 系统消息";
            qDebug() << "参数 - content:" << welcomeMsg;

            if (!systemMsgQuery.exec()) {
                success = false;
                qDebug() << "发送系统消息失败:" << systemMsgQuery.lastError().text();
            } else {
                qDebug() << "发送系统消息成功，受影响行数:" << systemMsgQuery.numRowsAffected();
            }
        } else {
            qDebug() << "查询用户名失败:" << userQuery.lastError().text();
            success = false;
        }
    }

    if (success) {
        if (db.commit()) {
            qDebug() << "事务提交成功";
            QMessageBox::information(this, "提示", accept ? "已接受加群申请" : "已拒绝加群申请");
        } else {
            qDebug() << "事务提交失败:" << db.lastError().text();
            success = false;
        }
    }

    if (!success) {
        if (db.rollback()) {
            qDebug() << "事务回滚成功";
        } else {
            qDebug() << "事务回滚失败:" << db.lastError().text();
        }
        QMessageBox::warning(this, "错误", "处理加群申请失败");
    }

    // 刷新请求列表
    qDebug() << "重新加载群聊申请列表";
    loadGroupRequests();
    
    qDebug() << "===== 群聊申请处理完成 =====\n";
}


FindFriendGroup::~FindFriendGroup()
{
    if (db.isOpen()) {
        db.close();
    }
    QSqlDatabase::removeDatabase("FindFriendGroupConnection");
    delete ui;
}

// 获取用户id
void FindFriendGroup::setCurrentUser(QString username) {
    if (username.isEmpty()) {
        qDebug() << "设置用户失败：用户名为空";
        return;
    }

    this->username = username;

    // 确保数据库已连接
    if (!db.isOpen()) {
        connectDatabase();
    }

    QSqlQuery query(db);
    query.prepare("SELECT id FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (query.exec()) {
        if (query.next()) {
            this->currentUserId = query.value(0).toInt();
            qDebug() << "成功设置当前用户ID:" << this->currentUserId;
        }
        else {
            this->currentUserId = -1;
            qDebug() << "未找到用户:" << username;
        }
    }
    else {
        this->currentUserId = -1;
        qDebug() << "查询用户ID失败:" << query.lastError().text();
    }
}



// **连接数据库**
void FindFriendGroup::connectDatabase()
{
    db = QSqlDatabase::addDatabase("QMYSQL", "FindFriendGroupConnection");
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

void FindFriendGroup::minimizeWindow()
{
    this->showMinimized();  // 最小化窗口
}

void FindFriendGroup::closeWindow()
{
    if (db.isOpen()) {
        db.close();
    }
    QSqlDatabase::removeDatabase("FindFriendGroupConnection");
    this->close();
}

// 窗口可以被鼠标拖动
void FindFriendGroup::mousePressEvent(QMouseEvent* event)
{
    lastPos = event->globalPosition().toPoint() - this->pos();
}

void FindFriendGroup::mouseMoveEvent(QMouseEvent* event)
{
    this->move(event->globalPosition().toPoint() - lastPos);
}

// 获取用户名
void FindFriendGroup::setUsername(const QString& username) {
    this->username = username;
    qDebug() << "设置用户名:" << this->username;

    // 立即更新currentUserId
    setCurrentUser(username);
}



