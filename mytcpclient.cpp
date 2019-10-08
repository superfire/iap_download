/*
 *   mytcpclient.cpp: TCP client
 * 
 *   Copyright (C) 2017  Zhengyu Peng, https://zpeng.me
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mytcpclient.h"

MyTCPClient::MyTCPClient(QObject *parent) : QTcpSocket(parent)
{
    tcpSocket = new QTcpSocket();
}

void MyTCPClient::connectTo(QHostAddress addr, quint16 port)
{
    tcpSocket->connectToHost(addr, port);
    connect(tcpSocket, SIGNAL(connected()), this, SLOT(onConnected()));
    connect(tcpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onStateChanged(QAbstractSocket::SocketState)));
}

void MyTCPClient::onConnected()
{
    disconnect(tcpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onStateChanged(QAbstractSocket::SocketState)));
    connect(tcpSocket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(messageReady()));
    emit myClientConnected(tcpSocket->peerAddress().toString(), tcpSocket->peerPort());
}

void MyTCPClient::onStateChanged(QAbstractSocket::SocketState state)
{
    disconnect(tcpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onStateChanged(QAbstractSocket::SocketState)));
    switch (state)
    {
    case QAbstractSocket::UnconnectedState:
        emit connectionFailed();
        qDebug("unconnected");
        //qDebug()<<"connecting timeout";
        break;
    case QAbstractSocket::HostLookupState:
        qDebug()<<"HostLookupState";
        break;
    case QAbstractSocket::ConnectingState:
        qDebug()<<"ConnectingState";
        break;
    case QAbstractSocket::ConnectedState:
        qDebug()<<"ConnectedState";
        break;
    case QAbstractSocket::BoundState:
        qDebug()<<"BoundState";
        break;
    case QAbstractSocket::ListeningState:
        qDebug()<<"ListeningState";
        break;
    case QAbstractSocket::ClosingState:
        qDebug()<<"ClosingState";
        break;
    }
}

void MyTCPClient::onDisconnected()
{
    disconnect(tcpSocket, SIGNAL(disconnected()));
    disconnect(tcpSocket, SIGNAL(readyRead()));
    tcpSocket->close();
    emit myClientDisconnected();
}

void MyTCPClient::closeClient()
{
    tcpSocket->close();
    qDebug() << "close tcpclient";
}

void MyTCPClient::abortConnection()
{
    tcpSocket->abort();
}

void MyTCPClient::messageReady()
{
    array = tcpSocket->readAll();
    emit newMessage(tcpSocket->peerAddress().toString(), array);

#if 0
    QByteArray::const_iterator it = array.begin();

    qDebug() << "..............\n";

    for(; it != array.end(); it++ )
    {
        qDebug("%02x",(uint8_t) *it);
    }
#endif
}

QByteArray StrToHex(QString string)
{
    QByteArray data;
    QString tmp_str;
    int len = 0, idx = 0, idx_2 = 0;
    bool ok;

    len = string.length();
//    qDebug()<<"len:"<<len<<endl;

    for(idx = 0; idx<len; )
    {
        tmp_str = string.at(idx++);
        tmp_str += string.at(idx++);
        data[idx_2++] = tmp_str.toUInt(&ok, 16);
    }
//    qDebug()<<data<<endl;

    return data;
}

void MyTCPClient::sendMessage(QString string)
{
    QByteArray Data;

    Data = StrToHex(string);
//    Data.append(StrToHexStr(string));
    if (tcpSocket->state() == QTcpSocket::ConnectedState)
    {
        tcpSocket->write(Data);
        tcpSocket->flush();
    }
}

void MyTCPClient::sendMessage(QByteArray Data)
{
    if (tcpSocket->state() == QTcpSocket::ConnectedState)
    {
        tcpSocket->write(Data);
        tcpSocket->flush();
    }
}

void MyTCPClient::disconnectCurrentConnection()
{
    tcpSocket->disconnectFromHost();
}
