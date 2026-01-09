#include "ChatWindow.h"
#include <QMessageBox>
#include <QScrollBar>
#include <QDebug>

ChatWindow::ChatWindow(QWidget *parent) : QMainWindow(parent) {
    isGroupChat = false;
    currentChatTarget = "";


    setupUI();

    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, this, &ChatWindow::onConnect);
    connect(socket, &QTcpSocket::readyRead, this, &ChatWindow::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &ChatWindow::onDisconnect);

    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &ChatWindow::sendRefreshRequests);

    // socket->connectToHost("127.0.0.1", 9000);
    startDiscovery();
}

ChatWindow::~ChatWindow() {
    if(socket->isOpen()) socket->close();
}

void ChatWindow::startDiscovery() {
    currentUserLabel->setText("Searching for server...");
    statusBar()->showMessage("Looking for server in local network...", 0);

    udpSocket = new QUdpSocket(this);
    udpSocket->bind(QHostAddress::Any, 0);
    connect(udpSocket, &QUdpSocket::readyRead, this, &ChatWindow::onDiscoveryResponse);

    QByteArray data = "WHO_IS_SERVER";
    udpSocket->writeDatagram(data, QHostAddress::Broadcast, 9001);
}

void ChatWindow::onDiscoveryResponse() {
    while (udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();

        if (datagram.data().contains("SERVER_HERE")) {
            QHostAddress serverIp = datagram.senderAddress();
            statusBar()->showMessage("Found server at: " + serverIp.toString(), 2000);

            socket->connectToHost(serverIp, 9000);

            udpSocket->close();
            udpSocket->deleteLater();
            udpSocket = nullptr;
            return;
        }
    }
}

void ChatWindow::setupUI() {
    QWidget *centralWidget = new QWidget;
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 1. TOP BAR
    QHBoxLayout *topLayout = new QHBoxLayout;
    currentUserLabel = new QLabel("Not logged in");
    currentUserLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #00e5ff;"); // Cyan deschis

    authContainer = new QWidget;
    QHBoxLayout *authLayout = new QHBoxLayout(authContainer);
    authLayout->setContentsMargins(0,0,0,0);

    usernameInput = new QLineEdit(); usernameInput->setPlaceholderText("User");
    passwordInput = new QLineEdit(); passwordInput->setPlaceholderText("Pass");
    passwordInput->setEchoMode(QLineEdit::Password);
    loginBtn = new QPushButton("Login");
    registerBtn = new QPushButton("Register");

    logoutBtn = new QPushButton("Logout");
    logoutBtn->setVisible(false); // Ascuns inițial
    logoutBtn->setStyleSheet("background-color: #d32f2f; color: white;"); // Roșu

    authLayout->addWidget(usernameInput);
    authLayout->addWidget(passwordInput);
    authLayout->addWidget(loginBtn);
    authLayout->addWidget(registerBtn);

    topLayout->addWidget(currentUserLabel);
    topLayout->addStretch();
    topLayout->addWidget(authContainer);
    topLayout->addWidget(logoutBtn);
    topLayout->addWidget(authContainer);

    // 2. SIDEBARS
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal);
    QSplitter *leftSplitter = new QSplitter(Qt::Vertical);
    leftSplitter->setMinimumWidth(320);

    // --- Friends Sidebar (Sus) ---
    QGroupBox *friendsBox = new QGroupBox("Friends Management");
    QVBoxLayout *friendsLayout = new QVBoxLayout(friendsBox);

    QHBoxLayout *addFriendLayout = new QHBoxLayout();
    addFriendInput = new QLineEdit(); addFriendInput->setPlaceholderText("Username...");
    friendTypeSelector = new QComboBox();
    friendTypeSelector->addItem("Normal");
    friendTypeSelector->addItem("Close");
    addFriendBtn = new QPushButton("Add");

    addFriendLayout->addWidget(addFriendInput);
    addFriendLayout->addWidget(friendTypeSelector);
    addFriendLayout->addWidget(addFriendBtn);

    friendsList = new QListWidget();
    QLabel *reqLabel = new QLabel("Pending Requests (Click to Accept):");
    reqLabel->setStyleSheet("color: #ff6b6b; font-weight: bold; margin-top: 5px;"); // Roșu deschis
    friendRequestsList = new QListWidget();
    friendRequestsList->setMaximumHeight(80);

    friendsLayout->addLayout(addFriendLayout);
    friendsLayout->addWidget(new QLabel("My Friends:"));
    friendsLayout->addWidget(friendsList);
    friendsLayout->addWidget(reqLabel);
    friendsLayout->addWidget(friendRequestsList);

    // --- Groups & Chats Sidebar (Jos) ---
    QGroupBox *chatsBox = new QGroupBox("Conversations & Groups");
    QVBoxLayout *chatsLayout = new QVBoxLayout(chatsBox);

    QHBoxLayout *createGroupLayout = new QHBoxLayout();
    createGroupInput = new QLineEdit();
    createGroupInput->setPlaceholderText("New Group Name...");
    createGroupBtn = new QPushButton("Create Group");

    createGroupLayout->addWidget(createGroupInput);
    createGroupLayout->addWidget(createGroupBtn);

    QHBoxLayout *selectorLayout = new QHBoxLayout;
    selectorLayout->addWidget(new QLabel("Show:"));
    chatSelector = new QComboBox();
    chatSelector->addItem("Private Messages");
    chatSelector->addItem("Groups");
    selectorLayout->addWidget(chatSelector);

    chatList = new QListWidget();

    chatsLayout->addLayout(createGroupLayout);
    chatsLayout->addLayout(selectorLayout);
    chatsLayout->addWidget(chatList);

    leftSplitter->addWidget(friendsBox);
    leftSplitter->addWidget(chatsBox);

    // 3. MAIN CONTENT
    mainStack = new QStackedWidget();

    // Page 0: Feed
    QWidget *feedPage = new QWidget();
    QVBoxLayout *feedLayout = new QVBoxLayout(feedPage);
    QGroupBox *feedBox = new QGroupBox("News Feed");
    QVBoxLayout *contentLayout = new QVBoxLayout(feedBox);

    postsList = new QListWidget();
    postsList->setAlternatingRowColors(true);
    postsList->setWordWrap(true);

    QHBoxLayout *newPostLayout = new QHBoxLayout;
    newPostInput = new QLineEdit(); newPostInput->setPlaceholderText("Write something...");
    visibilityInput = new QComboBox();
    visibilityInput->addItem("Public", 0);
    visibilityInput->addItem("Friends", 1);
    visibilityInput->addItem("Close Friends", 2);
    QPushButton *postBtn = new QPushButton("Post");

    newPostLayout->addWidget(newPostInput);
    newPostLayout->addWidget(visibilityInput);
    newPostLayout->addWidget(postBtn);

    contentLayout->addWidget(postsList);
    contentLayout->addLayout(newPostLayout);
    feedLayout->addWidget(feedBox);

    // Page 1: Chat
    chatPage = new QWidget();
    QVBoxLayout *chatPageLayout = new QVBoxLayout(chatPage);

    QHBoxLayout *chatHeader = new QHBoxLayout();
    currentChatLabel = new QLabel("Select a chat");
    currentChatLabel->setStyleSheet("font-weight: bold; font-size: 16px; color: #ffeb3b;"); // Galben

    addMemberBtn = new QPushButton("Add Member");
    addMemberBtn->setVisible(false);
    addMemberBtn->setStyleSheet("background-color: #9c27b0; color: white;"); // Mov

    QPushButton *backBtn = new QPushButton("Back to Feed");

    chatHeader->addWidget(currentChatLabel);
    chatHeader->addWidget(addMemberBtn);
    chatHeader->addStretch();
    chatHeader->addWidget(backBtn);

    messagesList = new QListWidget();
    messagesList->setAlternatingRowColors(true);

    QHBoxLayout *inputChat = new QHBoxLayout();
    chatInput = new QLineEdit(); chatInput->setPlaceholderText("Message...");
    QPushButton *sendMsgBtn = new QPushButton("Send");
    inputChat->addWidget(chatInput);
    inputChat->addWidget(sendMsgBtn);

    chatPageLayout->addLayout(chatHeader);
    chatPageLayout->addWidget(messagesList);
    chatPageLayout->addLayout(inputChat);

    mainStack->addWidget(feedPage);
    mainStack->addWidget(chatPage);

    mainSplitter->addWidget(leftSplitter);
    mainSplitter->addWidget(mainStack);
    mainSplitter->setStretchFactor(1, 1);

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(mainSplitter);

    // --- CONECTĂRI ---
    connect(loginBtn, &QPushButton::clicked, this, &ChatWindow::onLoginClicked);
    connect(registerBtn, &QPushButton::clicked, this, &ChatWindow::onRegisterClicked);
    connect(logoutBtn, &QPushButton::clicked, this, &ChatWindow::onLogoutClicked);
    connect(postBtn, &QPushButton::clicked, this, &ChatWindow::onSendPostClicked);
    connect(friendsList, &QListWidget::itemClicked, this, &ChatWindow::onFriendClicked);

    connect(chatSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChatWindow::onChatTypeChanged);
    connect(chatList, &QListWidget::itemClicked, this, &ChatWindow::onChatListClicked);

    connect(backBtn, &QPushButton::clicked, [this](){ mainStack->setCurrentIndex(0); });
    connect(sendMsgBtn, &QPushButton::clicked, this, &ChatWindow::onSendMessageClicked);
    connect(chatInput, &QLineEdit::returnPressed, this, &ChatWindow::onSendMessageClicked);

    connect(addFriendBtn, &QPushButton::clicked, this, &ChatWindow::onAddFriendClicked);
    connect(friendRequestsList, &QListWidget::itemClicked, this, &ChatWindow::onAcceptRequestClicked);
    connect(createGroupBtn, &QPushButton::clicked, this, &ChatWindow::onCreateGroupClicked);

    connect(addMemberBtn, &QPushButton::clicked, this, &ChatWindow::onAddMemberClicked);

    this->setStyleSheet(
        "QMainWindow { background-color: #2b2b2b; color: #ffffff; }" // Fundal gri închis, text alb
        "QWidget { color: #f0f0f0; }"
        "QGroupBox { font-weight: bold; border: 1px solid #555; border-radius: 5px; margin-top: 10px; background-color: #333; color: #fff; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #4fc3f7; }" // Titlu albastru deschis

        "QListWidget { background-color: #1e1e1e; border: 1px solid #555; border-radius: 3px; color: #ffffff; alternate-background-color: #252525; }"
        "QListWidget::item:selected { background-color: #3d5afe; }"

        "QLineEdit { background-color: #424242; color: #ffffff; border: 1px solid #666; padding: 4px; border-radius: 3px; selection-background-color: #007bff; }"

        "QComboBox { background-color: #424242; color: white; border: 1px solid #666; padding: 3px; }"
        "QComboBox QAbstractItemView { background-color: #424242; color: white; selection-background-color: #3d5afe; }"

        "QPushButton { background-color: #0d47a1; color: white; border-radius: 3px; padding: 5px 10px; border: 1px solid #000; }"
        "QPushButton:hover { background-color: #1565c0; }"
        "QPushButton:pressed { background-color: #0d47a1; }"

        "QStatusBar { color: #ffffff; }"
        "QLabel { color: #e0e0e0; }"
    );

    resize(1100, 750);
}

// --- LOGICA ---

void ChatWindow::onConnect() {
    currentUserLabel->setText("Connected as Guest");
    statusBar()->showMessage("Connected to server.", 5000);
    socket->write("FEED\n");
}

void ChatWindow::onDisconnect() {
    currentUserLabel->setText("Disconnected");
    refreshTimer->stop();
    authContainer->setVisible(true);
    statusBar()->showMessage("Disconnected from server.", 0);
}

void ChatWindow::sendRefreshRequests() {
    if(socket->state() == QAbstractSocket::ConnectedState) {
        socket->write("FEED\n");
        socket->write("VIEW_FRIENDS\n");
        socket->write("VIEW_REQUESTS\n");
        socket->write("VIEW_GROUPS\n");
    }
}

void ChatWindow::onLoginClicked() {
    QString u = usernameInput->text();
    QString p = passwordInput->text();
    if(u.isEmpty() || p.isEmpty()) return;
    socket->write(("LOGIN " + u + " " + p + "\n").toUtf8());
    currentUsername = u;
}

void ChatWindow::onLogoutClicked() {
    if(socket->isOpen()) {
        socket->write("LOGOUT\n");
    }

    currentUsername = "";
    cachedFriends.clear();
    cachedGroups.clear();
    friendsList->clear();
    friendRequestsList->clear();
    chatList->clear();
    messagesList->clear();
    currentUserLabel->setText("Guest Mode (Not logged in)");
    QMainWindow::setWindowTitle("Logged Out");

    authContainer->setVisible(true);
    logoutBtn->setVisible(false);
    addMemberBtn->setVisible(false);
    refreshTimer->stop();

    socket->write("FEED\n");
}

void ChatWindow::onRegisterClicked() {
    QString u = usernameInput->text();
    QString p = passwordInput->text();
    if(u.isEmpty() || p.isEmpty()) return;
    socket->write(("REGISTER " + u + " " + p + " 0\n").toUtf8());
}

void ChatWindow::onAddFriendClicked() {
    QString targetUser = addFriendInput->text().trimmed();
    if (targetUser.isEmpty()) return;
    QString type = (friendTypeSelector->currentIndex() == 1) ? "close" : "normal";
    socket->write(("ADD_FRIEND " + targetUser + " " + type + "\n").toUtf8());
    addFriendInput->clear();
}

void ChatWindow::onCreateGroupClicked() {
    QString gName = createGroupInput->text().trimmed();
    if (gName.isEmpty()) {
        statusBar()->showMessage("Please enter a group name.", 3000);
        return;
    }
    socket->write(("CREATE_GROUP " + gName + "\n").toUtf8());
    createGroupInput->clear();
    statusBar()->showMessage("Request to create group sent...", 2000);
}

void ChatWindow::onAddMemberClicked() {
    if (!isGroupChat) return;

    bool ok;
    QString newMember = QInputDialog::getText(this, "Add Member",
                                         "Enter username to add to this group:", QLineEdit::Normal,
                                         "", &ok);
    if (ok && !newMember.isEmpty()) {
        // ADD_TO_GROUP <groupID> <username>
        socket->write(("ADD_TO_GROUP " + currentChatTarget + " " + newMember.trimmed() + "\n").toUtf8());
        statusBar()->showMessage("Added " + newMember + " to group.", 3000);
    }
}

void ChatWindow::onAcceptRequestClicked(QListWidgetItem* item) {
    if (!item) return;
    QString reqText = item->text();
    QString username = reqText.split(" ")[0];
    socket->write(("ACCEPT_REQUEST " + username + "\n").toUtf8());
    statusBar()->showMessage("Accepting request from " + username + "...", 2000);
}

void ChatWindow::onSendPostClicked() {
    QString txt = newPostInput->text();
    if(txt.isEmpty()) return;
    QString vis = "public";
    if(visibilityInput->currentIndex() == 1) vis = "friends";
    if(visibilityInput->currentIndex() == 2) vis = "close";

    socket->write(("POST " + vis + " " + txt + "\n").toUtf8());
    newPostInput->clear();
    socket->write("FEED\n");
}

void ChatWindow::onFriendClicked(QListWidgetItem* item) {
    if(!item) return;
    mainStack->setCurrentIndex(0);
    QString target = item->text();
    if(target.contains(" (")) target = target.split(" (")[0];
    socket->write(("VIEW_POSTS " + target + "\n").toUtf8());
    statusBar()->showMessage("Viewing posts for " + target, 3000);
}

void ChatWindow::onChatTypeChanged(int index) {
    chatList->clear();
    if (index == 0) chatList->addItems(cachedFriends);
    else chatList->addItems(cachedGroups);
}

void ChatWindow::onChatListClicked(QListWidgetItem* item) {
    if(!item) return;
    QString text = item->text();

    mainStack->setCurrentIndex(1);
    messagesList->clear();

    if (chatSelector->currentIndex() == 0) {
        // Private
        isGroupChat = false;
        addMemberBtn->setVisible(false);
        if(text.contains(" (")) text = text.split(" (")[0];
        currentChatTarget = text;
        currentChatLabel->setText("Private Chat with: " + currentChatTarget);
    }
    else {
        // Group
        isGroupChat = true;
        addMemberBtn->setVisible(true);
        QStringList parts = text.split(":");
        if (parts.size() >= 2) {
            currentChatTarget = parts[0].trimmed();
            QString groupName = parts[1].trimmed();
            currentChatLabel->setText("Group Chat: " + groupName);
        } else {
            currentChatTarget = text;
        }
    }
}

void ChatWindow::onSendMessageClicked() {
    QString txt = chatInput->text();
    if(txt.isEmpty() || currentChatTarget.isEmpty()) return;

    if (isGroupChat) {
        socket->write(("GROUP_MSG " + currentChatTarget + " " + txt + "\n").toUtf8());
        messagesList->addItem("Me (Group): " + txt);
    } else {
        socket->write(("MSG " + currentChatTarget + " " + txt + "\n").toUtf8());
        messagesList->addItem("Me: " + txt);
    }
    messagesList->scrollToBottom();
    chatInput->clear();
}

void ChatWindow::onReadyRead() {
    QByteArray data = socket->readAll();
    QString msg = QString::fromUtf8(data);
    processServerMessage(msg);
}

void ChatWindow::processServerMessage(QString msg) {
    QStringList lines = msg.split('\n');
    ParseState currentState = STATE_NONE;

    for (QString line : lines) {
        QString cleanLine = line.trimmed();
        if (cleanLine.isEmpty()) continue;

        if (cleanLine.startsWith("[")) {
            if (cleanLine.contains("[Private") || cleanLine.contains("[Group")) {
                if (mainStack->currentIndex() == 1) {
                    messagesList->addItem(cleanLine);
                    messagesList->scrollToBottom();
                } else {
                    statusBar()->showMessage("New message: " + cleanLine, 5000);
                }
                continue;
            }
        }

        if (cleanLine == "--- News Feed ---") { currentState = STATE_FEED; postsList->clear(); continue; }
        else if (cleanLine == "--- Friends List ---") { currentState = STATE_FRIENDS; cachedFriends.clear(); friendsList->clear(); continue; }
        else if (cleanLine == "--- Friend Requests ---") { currentState = STATE_REQUESTS; friendRequestsList->clear(); continue; }
        else if (cleanLine == "--- Groups List ---") { currentState = STATE_GROUPS; cachedGroups.clear(); continue; }

        switch (currentState) {
            case STATE_FEED: {
                if (cleanLine.startsWith("---")) break;
                QListWidgetItem* item = new QListWidgetItem();
                if(cleanLine.contains("[Public]")) item->setForeground(QColor("#00e676")); // Verde deschis
                else if(cleanLine.contains("[Friends]")) item->setForeground(QColor("#40c4ff")); // Albastru deschis
                else if(cleanLine.contains("[Close]")) item->setForeground(QColor("#ff4081")); // Roz/Rosu deschis
                else item->setForeground(Qt::white);

                item->setText(cleanLine);
                postsList->addItem(item);
                break;
            }
            case STATE_FRIENDS: {
                if (cleanLine.startsWith("---")) break;
                cachedFriends.append(cleanLine);
                friendsList->addItem(cleanLine);
                break;
            }
            case STATE_REQUESTS: {
                if (cleanLine.startsWith("---")) break;
                friendRequestsList->addItem(cleanLine);
                break;
            }
            case STATE_GROUPS: {
                if (cleanLine.startsWith("---")) break;
                cachedGroups.append(cleanLine);
                break;
            }
            case STATE_NONE: {
                if (cleanLine.contains("200 OK: Welcome")) {
                    currentUserLabel->setText("User: " + currentUsername);
                    authContainer->setVisible(false);
                    logoutBtn->setVisible(true);
                    refreshTimer->start(3000);
                    QMainWindow::setWindowTitle(currentUsername);

                    sendRefreshRequests();
                    statusBar()->showMessage("Login successful!", 5000);
                }
                else if (cleanLine.contains("200") || cleanLine.contains("201")) {
                     statusBar()->showMessage(cleanLine, 4000);
                }
                else if (cleanLine.startsWith("400") || cleanLine.startsWith("403") || cleanLine.startsWith("404")) {
                     statusBar()->showMessage("Error: " + cleanLine, 5000);
                }
                break;
            }
        }
    }

    if (chatSelector->currentIndex() == 0 && !cachedFriends.isEmpty()) {
        chatList->clear();
        chatList->addItems(cachedFriends);
    } else if (chatSelector->currentIndex() == 1 && !cachedGroups.isEmpty()) {
        chatList->clear();
        chatList->addItems(cachedGroups);
    }
}