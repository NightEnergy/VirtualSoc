#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QComboBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QGroupBox>
#include <QTimer>
#include <QStatusBar>
#include <QInputDialog>

class ChatWindow : public QMainWindow {
    Q_OBJECT

public:
    ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow();


private slots:
    void onConnect();
    void onReadyRead();
    void onDisconnect();
    void sendRefreshRequests();

    void onLoginClicked();
    void onLogoutClicked();
    void onRegisterClicked();
    void onSendPostClicked();
    void onSendMessageClicked();
    void onFriendClicked(QListWidgetItem* item);
    void onChatTypeChanged(int index);
    void onChatListClicked(QListWidgetItem* item);

    void onAddFriendClicked();
    void onAcceptRequestClicked(QListWidgetItem* item);

    void onCreateGroupClicked();

    void onAddMemberClicked();

private:
    void setupUI();
    void processServerMessage(QString message);

    // --- UI Elements ---
    QLabel *currentUserLabel;
    QLineEdit *usernameInput;
    QLineEdit *passwordInput;
    QPushButton *loginBtn;
    QPushButton *logoutBtn;
    QPushButton *registerBtn;
    QWidget *authContainer;

    QListWidget *friendsList;
    QListWidget *friendRequestsList;
    QLineEdit *addFriendInput;
    QComboBox *friendTypeSelector;
    QPushButton *addFriendBtn;

    QComboBox *chatSelector;
    QListWidget *chatList;
    QLineEdit *createGroupInput;
    QPushButton *createGroupBtn;

    QStackedWidget *mainStack;
    QListWidget *postsList;
    QLineEdit *newPostInput;
    QComboBox *visibilityInput;

    QWidget *chatPage;
    QListWidget *messagesList;
    QLineEdit *chatInput;
    QLabel *currentChatLabel;

    QPushButton *addMemberBtn;

    // --- Network & Data ---
    QTcpSocket *socket;
    QString currentUsername;
    QTimer *refreshTimer;

    QStringList cachedFriends;
    QStringList cachedGroups;

    QString currentChatTarget;
    bool isGroupChat;

    enum ParseState {
        STATE_NONE,
        STATE_FEED,
        STATE_FRIENDS,
        STATE_REQUESTS,
        STATE_GROUPS
    };
};

#endif // CHATWINDOW_H