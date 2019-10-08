#ifndef YMODEMFILETRANSMIT_H
#define YMODEMFILETRANSMIT_H

#include <QFile>
#include <QTimer>
#include <QObject>
#include <QSerialPort>
#include "Ymodem.h"
#include "mytcpclient.h"

class YmodemFileTransmit : public QObject, public Ymodem
{
    Q_OBJECT

public:
    explicit YmodemFileTransmit(QObject *parent = 0);
    ~YmodemFileTransmit();

    void setFileName(const QString &name);

    void setPortName(const QString &name);
    void setPortBaudRate(qint32 baudrate);
    void setTcpClientSetting(QHostAddress addr, quint16 port);

    bool startTransmit();
    void stopTransmit();

    int getTransmitProgress();
    Status getTransmitStatus();

signals:
    void transmitProgress(int progress);
    void transmitStatus(YmodemFileTransmit::Status status);
    void tcpConnectResult(bool result);

private slots:
    void readTimeOut();
    void writeTimeOut();
    void tcpConnectSuccess();
    void tcpConnectFailed();

    void onTcpClientAppendMessage(const QString &from, const QByteArray &message);

private:
    Code callback(Status status, uint8_t *buff, uint32_t *len);

    uint32_t read(uint8_t *buff, uint32_t len);
    uint32_t write(uint8_t *buff, uint32_t len);

    QFile       *file;
    QTimer      *readTimer;
    QTimer      *writeTimer;
    QSerialPort *serialPort;

    MyTCPClient *mytcpclient = nullptr;
//    QString settingsFileDir;
    QHostAddress tcpClientTargetAddr;
    quint16 tcpClientTargetPort;

    int      progress;
    Status   status;
    uint64_t fileSize;
    uint64_t fileCount;

    uint8_t rx_cache[2048];
    uint32_t rx_len;
};

#endif // YMODEMFILETRANSMIT_H
