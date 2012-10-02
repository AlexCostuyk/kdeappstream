#include "HttpServer.h"
#include <QTcpSocket>
#include <QFile>
#include <QStringList>
#include <QRegExp>
#include <kstandarddirs.h>
#include <QUuid>
#include <QProcessEnvironment>
#include <QLocalSocket>

#include "ApplicationWrapperThread.h"

HttpServer::HttpServer(uint _port, QObject * parent) :
    QTcpServer(parent),
    port(_port),
    disabled(false),
    init_server(this)
{
    QLocalServer::removeServer("kappstream_init");
    listen(QHostAddress::Any, port);
    if (!init_server.listen("kappstream_init"))
    {
        qDebug() << "Cannot start istening on local server";
        qDebug() << init_server.errorString();
        qDebug() << init_server.fullServerName();
        qDebug() << init_server.serverName();
        exit(-1);
    }
}

void HttpServer::incomingConnection(int socket)
{
    QTcpSocket * s = new QTcpSocket(this);
    connect(s, SIGNAL(readyRead()), this, SLOT(readClient()));
    connect(s, SIGNAL(disconnected()), this, SLOT(discardClient()));
    s->setSocketDescriptor(socket);
}

void HttpServer::readClient()
{
    QTcpSocket * socket = (QTcpSocket*)sender();
    if (socket->canReadLine())
    {
        QTextStream os(socket);
        os.setAutoDetectUnicode(true);

        QStringList tokens = QString(socket->readLine()).split(QRegExp("[ \r\n][ \r\n]*"));
        if (tokens[0] == "GET")
        {
            QStringList resources = tokens[1].split(QRegExp("\\?"));
            if (resources.count() == 2)
            {
                if (resources[0] == "/canvas.html")
                {
                    QStringList args = resources[1].split(QRegExp("&(amp){0}"));
                    foreach (QString arg, args)
                    {
                        QStringList kv = arg.split("=");
                        if (kv.count() == 2 && kv[0] == "app")
                        {
                            this->sendCanvas(os, kv[1]);
                            goto close_socket;
                        }
                    }
                    this->sendStatus(os, 400);
                    goto close_socket;
                }
                else
                {
                    this->sendIndex(os);
                }
            }
            else
            {
                this->sendIndex(os);
            }
        }
        else
        {
            this->sendStatus(os, 400);
        }
    }
close_socket:
    socket->close();
    if (socket->state() == QTcpSocket::UnconnectedState)
        delete socket;
}

void HttpServer::discardClient()
{
    QTcpSocket* socket = (QTcpSocket*)sender();
    socket->deleteLater();
}

void HttpServer::sendStatus(QTextStream & os, int status)
{
    switch (status)
    {
        case 200:
            os << "HTTP/1.0 200 Ok\r\n"
                  "Content-Type: text/html; charset=\"utf-8\"\r\n"
                  "\r\n";
            break;
        case 400:
        default:
            os << "HTTP/1.0 400 Bad Request\r\n"
                  "Content-Type: text/html; charset=\"utf-8\"\r\n"
                  "\r\n";
    }
}

void HttpServer::sendIndex(QTextStream & os)
{
    this->sendStatus(os, 200);

    KStandardDirs sd;
    QStringList sl = sd.findDirs("data", "kdeappstream/data/");
    if (!sl.count())
    {
        qDebug() << "File not found.";
        os << "<html><head></head><body>Server internal error.</body></head>";
        return;
    }

    foreach (QString dir, sl)
    {
        QFile file(dir + "/index.html");
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QRegExp sessionIDTag("\\[PUT_SESSION_ID_HERE\\]");
        QTextStream in(&file);
        QString data = in.readAll();
        data.replace(sessionIDTag, QUuid::createUuid().toString());
        os << data;
        return;
    }
    qDebug() << "Can't read index";
}

void HttpServer::sendCanvas(QTextStream & os, QString applicationName)
{
    this->sendStatus(os, 200);

    KStandardDirs sd;
    QStringList sl = sd.findDirs("data", "kdeappstream/data/");
    if (!sl.count())
    {
        qDebug() << "File not found.";
        os << "<html><head></head><body>Server internal error.</body></head>";
        return;
    }

    foreach (QString dir, sl)
    {
        QFile file(dir + "/canvas.html");
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        ApplicationWrapperThread * wraper = new ApplicationWrapperThread(applicationName, this);
        wraper->start();

        if (init_server.waitForNewConnection(10000))
        {
            QLocalSocket * socket = init_server.nextPendingConnection();
            if (socket)
            {
                if (socket->waitForReadyRead())
                {
                    char buf[11];
                    memset(buf, 0, 11);
                    socket->read(buf, 10);

                    qDebug() << buf;

                    QRegExp sessionIDTag("\\[PUT_SESSION_ID_HERE\\]");
                    QRegExp portNumberTag("\\[PUT_PORT_NUMBER_HERE\\]");
                    QTextStream in(&file);
                    QString data = in.readAll();
                    data.replace(sessionIDTag, QUuid::createUuid().toString());
                    data.replace(portNumberTag, buf);
                    os << data;
                    return;
                }
                else
                    qDebug() << "Read not ready";
            }
            else
                qDebug() << "Empty socket";
            qDebug() << socket->errorString();
        }
        else
            qDebug() << "Waiting for connection failed";

        qDebug() << "Cannot get app server port number";
        qDebug() << init_server.errorString();
        os << "Server internal error";
        return;
    }
    qDebug() << "Can't read canvas";
}
