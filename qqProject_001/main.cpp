#include "QQ.h"
#include "ChatServer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    ChatServer server;
    server.startServer(8888); // ¼àÌý 8888 ¶Ë¿Ú
    QApplication a(argc, argv);
    QQ w;
    w.show();
    return a.exec();
}
