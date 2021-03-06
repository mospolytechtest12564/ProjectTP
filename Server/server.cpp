#include "Server.h"
const int port = 2323;


Server::Server() {
    if (this->listen(QHostAddress::Any, port)) {
        qDebug() << QString("Start server {port: %1}").arg(port);
    } else {
        qDebug() << "Error";
    }
    nextBlockSize = 0;
}

QString Server::get_passwd(QString login) {
    // connect 2 database
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("passwd.db");
    if (!db.open()) {
        qDebug() << "DBError";
        qDebug() << db.lastError().text();
    } else {
        //qDebug() << "Connect";
        QSqlQuery query = QSqlQuery(db);
        if (query.exec(QString("SELECT passw FROM passwdords WHERE login='%1';").arg(login))) {
            while (query.next())
                //qDebug() << query.value("passw").toString();
                return query.value("passw").toString();
        }
    }
    return "(╯°□°）╯︵ ┻━┻";
}

void Server::incomingConnection(qintptr socketDescriptor) {
    socket = new QTcpSocket;
    socket->setSocketDescriptor(socketDescriptor);
    connect(socket, &QTcpSocket::readyRead, this, &Server::slotReadyRead);
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);

    Sockets.push_back(socket);
    qDebug() << "Connect client: " << socketDescriptor;
    // token
    // --socketDescriptor
    QString local_token = get_token();
    tokens.insert(socketDescriptor, local_token);
    SendToClient(QString(">> Token: %1").arg(local_token));

}

QString Server::get_token() {
    const int len = 6;
    static const char alphanum[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
    QString tmp_s;
    tmp_s.reserve(len);
    for (int i = 0; i < len; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return tmp_s;
}

QString Server::md5(QString str) {
    QByteArray str_arr = str.toUtf8();
    return QString(QCryptographicHash::hash((str_arr),QCryptographicHash::Md5).toHex());
}

bool Server::is_not_login(QString login) {
    // connect 2 database

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("passwd.db");

    /*db.setHostName("127.0.0.1");
    db.setDatabaseName("parser");
    db.setUserName("root");
    db.setPassword("binarybun");*/
    // try open
    if (!db.open()) {
        qDebug() << "DBError";
        qDebug() << db.lastError().text();
    } else {
        //qDebug() << "Connect";
        QSqlQuery query = QSqlQuery(db);
        if (query.exec(QString("SELECT passw FROM passwdords WHERE login='%1';").arg(login))) {
            while (query.next())
                //qDebug() << query.value("passw").toString();
                return false;
        }
    }
    return true;
}

void Server::slotReadyRead() {
    socket = (QTcpSocket*)sender();  // get client socket
    QString token = tokens[socket->socketDescriptor()];  // get token
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_5_6);
    if (in.status() == QDataStream::Ok) {
        qDebug() << "read..";
        while (true) {
            if (nextBlockSize == 0) {
                if (socket->bytesAvailable() < 2) {
                    break;
                }
                in >> nextBlockSize;
            }
            if (socket->bytesAvailable() < nextBlockSize) {
                 break;
            }

            QString str;
            in >> str;
            nextBlockSize = 0;
            //SendToClient(QString(">> Token: %1\n\tMessage: %2").arg(token, str));
            // read message
            //qDebug() << str;
            if (str.left(3) == "cnu") {
                QString passwd = str.mid(3, 32);
                QString login = str.right(str.length()-3-32);
                qDebug() << passwd << ' ' << login;
                if (is_not_login(login)) {
                    db = QSqlDatabase::addDatabase("QSQLITE");
                    db.setDatabaseName("passwd.db");

                    /*db.setHostName("127.0.0.1");
                    db.setDatabaseName("parser");
                    db.setUserName("root");
                    db.setPassword("binarybun");*/
                    // try open
                    if (!db.open()) {
                        qDebug() << "DBError";
                        qDebug() << db.lastError().text();
                    } else {
                        //qDebug() << "Connect";
                        QSqlQuery query = QSqlQuery(db);
                        query.exec(QString("insert into passwdords values ('%1', '%2')").arg(login, passwd));
                        SendToClient("cnuTrue");
                    }
                } else {
                    SendToClient("cnuFalse");
                }

            } else if (str.left(3) == "tlg") {
                str = str.right(str.length()-3);
                QString passwd = get_passwd(str.split('|')[0]);
                if (md5(token + passwd) == str.split('|')[1]) {
                    SendToClient("tlgTrue");
                } else {
                    SendToClient("tlgFalse");
                }
            } else {
                SendToClient(QString("Message: %1").arg(str));
            }
            break;
        }

    } else {
        qDebug() << "DataStream error";
    }
}

void Server::SendToClient(QString str) {
    Data.clear();
    QDataStream out(&Data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_6);
    out << quint16(0) << str;  // size pac
    out.device()->seek(0);
    out << quint16(Data.size() - sizeof (quint16));
    socket->write(Data);  // from last client
}
