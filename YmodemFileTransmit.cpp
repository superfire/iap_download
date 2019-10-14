#include "YmodemFileTransmit.h"
#include <QFileInfo>

#define READ_TIME_OUT   (10)
#define WRITE_TIME_OUT  (100)

YmodemFileTransmit::YmodemFileTransmit(QObject *parent) :
    QObject(parent),
    file(new QFile),
    readTimer(new QTimer),
    writeTimer(new QTimer),
    mytcpclient(new MyTCPClient)
{
    setTimeDivide(499);
    setTimeMax(5);
    setErrorMax(999);

    connect(mytcpclient, SIGNAL(myClientConnected(const QString, quint16)), this, SLOT(tcpConnectSuccess()));
    connect(mytcpclient, SIGNAL(connectionFailed()), this, SLOT(tcpConnectFailed()));

    connect(readTimer, SIGNAL(timeout()), this, SLOT(readTimeOut()));
    connect(writeTimer, SIGNAL(timeout()), this, SLOT(writeTimeOut()));

    connect(mytcpclient, SIGNAL(newMessage(QString, QByteArray)), this, SLOT(onTcpClientAppendMessage(QString, QByteArray)));

}

YmodemFileTransmit::~YmodemFileTransmit()
{
    delete file;
    delete readTimer;
    delete writeTimer;
//    delete serialPort;
    delete mytcpclient;
}

void YmodemFileTransmit::setFileName(const QString &name)
{
    file->setFileName(name);
}

void YmodemFileTransmit::setPortName(const QString &name)
{
//    serialPort->setPortName(name);
}

void YmodemFileTransmit::setPortBaudRate(qint32 baudrate)
{
//    serialPort->setBaudRate(baudrate);
}

void YmodemFileTransmit::setTcpClientSetting(QHostAddress addr, quint16 port)
{
    tcpClientTargetAddr = addr;
    tcpClientTargetPort = port;
}

bool YmodemFileTransmit::startTransmit()
{
    qDebug() << "再次打开tcp连接， 地址:" << tcpClientTargetAddr << "端口:" << tcpClientTargetPort;
    mytcpclient->connectTo(tcpClientTargetAddr, tcpClientTargetPort);
    mytcpclient->open(QTcpSocket::ReadWrite);
    return true;
}

void YmodemFileTransmit::tcpConnectSuccess()
{
    progress = 0;
    status   = StatusEstablish;

    readTimer->start(READ_TIME_OUT);

    emit tcpConnectResult(true);
}

void YmodemFileTransmit::tcpConnectFailed()
{
    qDebug("connect failed");
    emit tcpConnectResult(false);
}

void YmodemFileTransmit::stopTransmit()
{
    file->close();
    abort();
    status = StatusAbort;
    writeTimer->start(WRITE_TIME_OUT);
}

int YmodemFileTransmit::getTransmitProgress()
{
    return progress;
}

Ymodem::Status YmodemFileTransmit::getTransmitStatus()
{
    return status;
}

void YmodemFileTransmit::readTimeOut()
{
    qDebug() << "read timeout, status:" << status;
    readTimer->stop();

    transmit();

    if((status == StatusEstablish) || (status == StatusTransmit))
    {
        readTimer->start(READ_TIME_OUT);
    }
}

void YmodemFileTransmit::writeTimeOut()
{
    qDebug("write timeout");
    writeTimer->stop();
//    serialPort->close();
    mytcpclient->closeClient();
    mytcpclient->close();
    transmitStatus(status);
}

Ymodem::Code YmodemFileTransmit::callback(Status status, uint8_t *buff, uint32_t *len)
{
    switch(status)
    {
        case StatusEstablish:
        {
            if(file->open(QFile::ReadOnly) == true)
            {
                QFileInfo fileInfo(*file);

                fileSize  = fileInfo.size();
                fileCount = 0;

                strcpy((char *)buff, fileInfo.fileName().toLocal8Bit().data());
                strcpy((char *)buff + fileInfo.fileName().toLocal8Bit().size() + 1, QByteArray::number(fileInfo.size()).data());

                *len = YMODEM_PACKET_SIZE;

                YmodemFileTransmit::status = StatusEstablish;

                transmitStatus(StatusEstablish);

                return CodeAck;
            }
            else
            {
                YmodemFileTransmit::status = StatusError;

                writeTimer->start(WRITE_TIME_OUT);

                return CodeCan;
            }
        }

        case StatusTransmit:
        {
            if(fileSize != fileCount)
            {
                if((fileSize - fileCount) > YMODEM_PACKET_SIZE)
                {
                    fileCount += file->read((char *)buff, YMODEM_PACKET_1K_SIZE);

                    *len = YMODEM_PACKET_1K_SIZE;
                }
                else
                {
                    fileCount += file->read((char *)buff, YMODEM_PACKET_SIZE);

                    *len = YMODEM_PACKET_SIZE;
                }

                progress = (int)(fileCount * 100 / fileSize);

                YmodemFileTransmit::status = StatusTransmit;

                transmitProgress(progress);
                transmitStatus(StatusTransmit);

                return CodeAck;
            }
            else
            {
                YmodemFileTransmit::status = StatusTransmit;

                transmitStatus(StatusTransmit);

                return CodeEot;
            }
        }

        case StatusFinish:
        {
            file->close();
            mytcpclient->closeClient();

            YmodemFileTransmit::status = StatusFinish;

            writeTimer->start(WRITE_TIME_OUT);

            return CodeAck;
        }

        case StatusAbort:
        {
            file->close();
            mytcpclient->closeClient();

            YmodemFileTransmit::status = StatusAbort;

            writeTimer->start(WRITE_TIME_OUT);

            return CodeCan;
        }

        case StatusTimeout:
        {
            YmodemFileTransmit::status = StatusTimeout;
            mytcpclient->closeClient();

            writeTimer->start(WRITE_TIME_OUT);

            return CodeCan;
        }

        default:
        {
            file->close();
            mytcpclient->closeClient();

            YmodemFileTransmit::status = StatusError;

            writeTimer->start(WRITE_TIME_OUT);

            return CodeCan;
        }
    }
}

uint32_t YmodemFileTransmit::read(uint8_t *buff, uint32_t len)
{
    uint32_t tmp_len = rx_len;
    // copy data to buff
    memmove(buff, rx_cache, tmp_len);
    // clear cache
    memset(rx_cache, 0, len);
    rx_len = 0;
    return tmp_len;
}

uint32_t YmodemFileTransmit::write(uint8_t *buff, uint32_t len)
{
    QByteArray data;
    qDebug() << "in write, wirte data:";
    for(quint32 i=0; i<len; i++) {
        data.append(buff[i]);
        qDebug("%02X ", buff[i]);
    }
    mytcpclient->sendMessage(data);
}

void YmodemFileTransmit::onTcpClientAppendMessage(const QString &from, const QByteArray &message)
{
    if (from.isEmpty() || message.isEmpty())
    {
        return;
    }

    // read all rx data
    qDebug() << "rx from: " << from;
    QByteArray::const_iterator it = message.begin();
    for( quint32 idx=0 ; it != message.end(); it++, idx++ )
    {
        rx_cache[idx] = (quint8)(*it);
        qDebug("%02x ", rx_cache[idx]);
    }
    rx_len = message.length();

    qDebug() << "rec_len:" << message.length();
}
