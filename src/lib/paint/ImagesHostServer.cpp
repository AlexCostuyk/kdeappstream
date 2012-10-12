#include "ImagesHostServer.h"

#include <QString>
#include <QStringList>
#include <QRegExp>
#include <QImage>
#include <QImageWriter>
#include <QDateTime>
#include <QDirIterator>
#include <QResource>
#include <QImage>
#include <QPixmap>
#include <QApplication>

ImagesHostServer * ImagesHostServer::m_instance = 0;

ImagesHostServer::ImagesHostServer(QObject * parent) :
    QTcpServer(parent),
    m_sem(1)
{
    qDebug() << "Listening for images fetching:" << this->listen(QHostAddress::Any);

    QDirIterator it(":", QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QResource r(it.next());
        if (r.data())
        {
//            QPixmap p(r.fileName());
//            if (!p.isNull())
//            {
//                qDebug() << "Pixmap:" << p.cacheKey() << r.fileName();
//                continue;
//            }
            QImage i(r.fileName());
            if (!i.isNull())
            {
                qDebug() << "Image:" << i.cacheKey() << r.fileName();
                continue;
            }
        }
    }
}

ImagesHostServer * ImagesHostServer::instance(QObject * parent)
{
    if (!m_instance)
        m_instance = new ImagesHostServer(parent);
    return m_instance;
}

void ImagesHostServer::incomingConnection(int socket)
{
    //qDebug() << "IncomingConnection!" << socket;
    QTcpSocket * s = new QTcpSocket(this);
    connect(s, SIGNAL(readyRead()), this, SLOT(readClient()));
    connect(s, SIGNAL(disconnected()), this, SLOT(discardClient()));
    s->setSocketDescriptor(socket);
}

void ImagesHostServer::readClient()
{
    //qDebug() << "readClient!";
    QTcpSocket * socket = (QTcpSocket*)sender();
    if (socket->canReadLine())
    {
        QStringList tokens = QString(socket->readLine()).split(QRegExp("[ \r\n][ \r\n]*"));
        if (tokens[0] == "GET")
        {
            QStringList resources = tokens[1].split(QRegExp("\\?"));
            //qDebug() << resources;
            if (resources.count() == 1)
            {
                QStringList args = resources[0].split(QRegExp("(\\\\|/|_|\\.)"), QString::SkipEmptyParts);
                if (args.count() == 1)
                {
                    this->sendStatus(socket, 200);
                    this->sendImage(socket, args[0].toAscii());
                    goto close_socket;
                }
            }
        }
    }
    this->sendStatus(socket, 400);
close_socket:
    socket->close();
    if (socket->state() == QTcpSocket::UnconnectedState)
        delete socket;
}

void ImagesHostServer::discardClient()
{
    //qDebug() << "discardClient!";
    QTcpSocket* socket = (QTcpSocket*)sender();
    socket->deleteLater();
}

void ImagesHostServer::sendStatus(QIODevice * device, int status)
{
    switch (status)
    {
        case 200:
            device->write("HTTP/1.0 200 Ok\r\n"
                          "Content-Type: image/png; charset=\"utf-8\"\r\n"
                          "\r\n");
            break;
        case 400:
        default:
            device->write("HTTP/1.0 400 Bad Request\r\n"
                          "Content-Type: text/html; charset=\"utf-8\"\r\n"
                          "\r\n");
    }
}

void ImagesHostServer::sendImage(QIODevice * device, QByteArray id)
{
    //qDebug() << "Reading image of given ID";

    m_sem.acquire();

    if (m_data.contains(id))
    {
        QImage image = m_data[id];
        m_data.remove(id);
        m_sem.release();

        //qDebug() << image.size();

        QImageWriter writer(device, "png");
        writer.write(image);
    }
    else
    {
        //qDebug() << "Cannot find image of given ID";
        m_sem.release();
    }
}

QByteArray ImagesHostServer::hostImage(const QImage & image)
{
    m_sem.acquire();

    QByteArray key = fastmd5(image).toBase64();
    if (m_cached.contains(key))
    {
        m_sem.release();
        return key;
    }

    m_cached.insert(key);

    QImage img = image.copy();
    m_data.insert(key, img);

    while (m_data.count() > 1000)
        m_data.remove(m_data.begin().key());

    m_sem.release();

    return key;
}
