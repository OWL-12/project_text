#include "QQ.h"
#include "ChatServer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    ChatServer server;
    server.startServer(8888); // ���� 8888 �˿�
    QApplication a(argc, argv);
    QQ w;
    w.show();
    return a.exec();
}
