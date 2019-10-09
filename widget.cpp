#include "widget.h"
#include "ui_widget.h"
#include <QMessageBox>
#include <QKeyEvent>

uint16_t crc16(uint8_t *buffer, uint16_t buffer_length)
{
    uint8_t crc_hi = 0xFF; /* high CRC byte initialized */
    uint8_t crc_lo = 0xFF; /* low CRC byte initialized */
    unsigned int i; /* will index into CRC lookup */

    /* pass through message buffer */
    while (buffer_length--) {
        i = crc_hi ^ *buffer++; /* calculate the CRC  */
        crc_hi = crc_lo ^ table_crc_hi[i];
        crc_lo = table_crc_lo[i];
    }

    return (uint16_t)(crc_hi << 8 | crc_lo);
}

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    mytcpclient(new MyTCPClient), /* 初始化的顺序跟定义的顺序一致 */
    ymodemFileTransmit(new YmodemFileTransmit),
    ymodemFileReceive(new YmodemFileReceive),
    writeTimer(new QTimer),
    connTimer(new QTimer),
    connPDlg(new MyProgressDlg(this)),
    fileDlg(new QFileDialog(this))
{
    ui->setupUi(this);
    this->setWindowTitle(tr("agv充电站程序升级工具"));
    this->setFixedSize( this->width (),this->height ());

    ui->transmitBrowse->setDisabled(true);

    // 连接中的进度对话框
    connPDlg->setWindowTitle("连接设备");
    connPDlg->setMinimum(0);
    connPDlg->setMaximum( CONN_TIMEOUT/PROGRESS_PERIOD );
    connPDlg->setValue(0);
    connPDlg->setLabelText("connecting...");
    connPDlg->setWindowFlag( Qt::FramelessWindowHint );
    connPDlg->setModal(true);
    QPushButton *btn = nullptr;
    connPDlg->setCancelButton(btn);
    connPDlg->reset(); // 必须添加，否则new完后，会自动弹出

    transmitButtonStatus = false;
    receiveButtonStatus  = false;
    devOnlineStatus = false;

    flash_step = FlashStep_ToConnDev;

    loadSettings();

    connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientButtonClicked()));
    connect(ymodemFileTransmit, SIGNAL(tcpConnectResult(bool)), this, SLOT(transmitTcpclientResult(bool)) );

    connect(ymodemFileTransmit, SIGNAL(transmitProgress(int)), this, SLOT(transmitProgress(int)));
    connect(ymodemFileTransmit, SIGNAL(transmitStatus(YmodemFileTransmit::Status)), this, SLOT(transmitStatus(YmodemFileTransmit::Status)));

    connect(writeTimer, SIGNAL(timeout()), this, SLOT(writeTimeOut()));
    connect(connTimer, SIGNAL(timeout()), this, SLOT(connTimeOut()));
    connect(connPDlg->progressTimer, SIGNAL(timeout()), this, SLOT(connPDlgTimeout()));
}

Widget::~Widget()
{
    delete ui;
    delete mytcpclient;
    delete ymodemFileTransmit;
    delete ymodemFileReceive;
    delete writeTimer;
    delete connTimer;
    delete connPDlg;
}

void Widget::loadSettings()
{
    settingsFileDir = QApplication::applicationDirPath() + "/config.ini";
    QSettings settings(settingsFileDir, QSettings::IniFormat);
    ui->lineEdit_TcpClientTargetIP->setText(settings.value("TCP_CLIENT_TARGET_IP", "10.10.100.254").toString());
//    ui->lineEdit_TcpClientTargetIP->setText(settings.value("TCP_CLIENT_TARGET_IP", "192.168.50.250").toString());
    ui->lineEdit_TcpClientTargetPort->setText(settings.value("TCP_CLIENT_TARGET_PORT", 8899).toString());
}

bool Widget::setupConnection(quint8 type)
{
    bool isSuccess = false;

    switch (type)
    {
    case TCPCLIENT:
        isSuccess = true;
        tcpClientTargetAddr.setAddress(ui->lineEdit_TcpClientTargetIP->text());
        tcpClientTargetPort = ui->lineEdit_TcpClientTargetPort->text().toInt();
        if (mytcpclient == nullptr)
        {
            mytcpclient = new MyTCPClient;
        }
        mytcpclient->connectTo(tcpClientTargetAddr, tcpClientTargetPort);
        break;
    }
    return isSuccess;
}

void Widget::onTcpClientButtonClicked()
{
    disconnect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientButtonClicked()));

    if (setupConnection(TCPCLIENT))
    {
        ui->lineEdit_TcpClientTargetIP->setDisabled(true);
        ui->lineEdit_TcpClientTargetPort->setDisabled(true);
        ui->button_TcpClient->setText("断开连接");

        connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientStopButtonClicked()));
        connect(mytcpclient, SIGNAL(myClientConnected(QString, quint16)), this, SLOT(onTcpClientNewConnection(QString, quint16)));
        connect(mytcpclient, SIGNAL(connectionFailed()), this, SLOT(onTcpClientTimeOut()));
    }

    // 启动连接超时定时器
    qDebug("start conn timer");
    connTimer->start( CONN_TIMEOUT );

    // 弹出连接中的对话框
    connPDlg->move(this->x() + this->width()/2 - connPDlg->width()/2, this->y() + this->height()/2 - connPDlg->height()/2);
    connPDlg->show();
    connPDlg->progressTimer->start(PROGRESS_PERIOD);

//    saveSettings();
}

void Widget::onTcpClientNewConnection(const QString &from, quint16 port)
{
    disconnect(mytcpclient, SIGNAL(myClientConnected(QString, quint16)), this, SLOT(onTcpClientNewConnection(QString, quint16)));
    disconnect(mytcpclient, SIGNAL(connectionFailed()), this, SLOT(onTcpClientTimeOut()));
    connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientStopButtonClicked()));
    connect(mytcpclient, SIGNAL(myClientDisconnected()), this, SLOT(onTcpClientDisconnected()));

    ui->button_TcpClient->setDisabled(false);
    ui->button_TcpClient->setText("断开连接");
    connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientDisconnectButtonClicked()));

    connect(mytcpclient, SIGNAL(newMessage(QString, QByteArray)), this, SLOT(onTcpClientAppendMessage(QString, QByteArray)));

    // 发送设备查询指令
    qDebug("new connection established, Send query dev instructions");

    // 重新启动连接超时计时器
    qDebug("restart conn timer");
    if( connTimer->isActive() ) {
        connTimer->stop();
        connTimer->start( CONN_TIMEOUT );
    }else{
        connTimer->start( CONN_TIMEOUT );
    }

    // 启动超时定时器
    qDebug("start write timer, 500ms period");
    writeTimer->start(WR_PERIOD);
}

void Widget::onTcpClientTimeOut()
{
    disconnect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientStopButtonClicked()));
    disconnect(mytcpclient, SIGNAL(myClientConnected(QString, quint16)), this, SLOT(onTcpClientNewConnection(QString, quint16)));
    disconnect(mytcpclient, SIGNAL(connectionFailed()), this, SLOT(onTcpClientTimeOut()));

    ui->button_TcpClient->setText("连接设备");
    ui->lineEdit_TcpClientTargetIP->setDisabled(false);
    ui->lineEdit_TcpClientTargetPort->setDisabled(false);
    ui->transmitBrowse->setDisabled(true);

    mytcpclient->closeClient();
    connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientButtonClicked()));

    QMessageBox::warning(this, "设备连接失败", "请检查设备地址是否正确!", u8"断开连接");

    if(writeTimer->isActive()) {
        writeTimer->stop();
    }

    if( connTimer->isActive() ) {
        connTimer->stop();
    }
}

void Widget::onTcpClientStopButtonClicked()
{
    qDebug("stop tcp client");
    if(flash_step == FlashStep_Flashing )
    {
        // 发送3，跳转到app区域
        connect(mytcpclient, SIGNAL(newMessage(QString, QByteArray)), this, SLOT(onTcpClientAppendMessage(QString, QByteArray)));
        qDebug() << "send 3, jump to app";
        QString text = "0x33";
        mytcpclient->sendMessage(text);

        if(connTimer->isActive())
            connTimer->stop();

        flash_step = FlashStep_ToConnDev;
    }

    disconnect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientStopButtonClicked()));

    disconnect(mytcpclient, SIGNAL(myClientConnected(QString, quint16)), this, SLOT(onTcpClientNewConnection(QString, quint16)));
    disconnect(mytcpclient, SIGNAL(connectionFailed()), this, SLOT(onTcpClientTimeOut()));
    ui->button_TcpClient->setText("连接设备");
    mytcpclient->abortConnection();

    ui->lineEdit_TcpClientTargetIP->setDisabled(false);
    ui->lineEdit_TcpClientTargetPort->setDisabled(false);
    ui->transmitButton->setDisabled(true);
    ui->transmitPath->setText("");
    ui->transmitBrowse->setDisabled(true);

    connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientButtonClicked()));
}

void Widget::onTcpClientDisconnectButtonClicked()
{
    mytcpclient->disconnectCurrentConnection();
}

void Widget::onTcpClientDisconnected()
{
    disconnect(mytcpclient, SIGNAL(myClientDisconnected()), this, SLOT(onTcpClientDisconnected()));
    disconnect(mytcpclient, SIGNAL(newMessage(QString, QByteArray)), this, SLOT(onTcpClientAppendMessage(QString, QByteArray)));
    disconnect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientDisconnectButtonClicked()));

    ui->button_TcpClient->setText("连接设备");
    ui->button_TcpClient->setDisabled(false);
    ui->lineEdit_TcpClientTargetIP->setDisabled(false);
    ui->lineEdit_TcpClientTargetPort->setDisabled(false);

    ui->transmitBrowse->setDisabled(true);

    mytcpclient->closeClient();
    mytcpclient->close();

    connect(ui->button_TcpClient, SIGNAL(clicked()), this, SLOT(onTcpClientButtonClicked()));

    // 停止writeTimer
    if(writeTimer->isActive())
        writeTimer->stop();

    if( connTimer->isActive() ) {
        connTimer->stop();
    }

    connPDlg->reset();
    connPDlg->hide();
    connPDlg->progressTimer->stop();
    connPDlg->progressCnt = 0;
}

void Widget::onTcpClientAppendMessage(const QString &from, const QByteArray &message)
{
    if (from.isEmpty() || message.isEmpty())
    {
        return;
    }

    // 处理下位机传过来的信息
    if( (quint8)message.at(0) == 0xA5 )
    {
        qDebug() << "rec board iap signal, now send ack:0xA5";
        QString text = "5A";
        mytcpclient->sendMessage(text);

        if( flash_step == FlashStep_ToConnDev )
        {
            connPDlg->progressCnt = 100;
            connPDlg->setValue( connPDlg->progressCnt );
            connPDlg->progressCnt = 0;
            connPDlg->setValue( connPDlg->progressCnt );
            connPDlg->reset();
            connPDlg->hide();
            connPDlg->progressTimer->stop();


            ui->transmitBrowse->setEnabled(true);
            if(ui->transmitPath->text().isEmpty() != true) {
                qDebug("set enable");
                ui->transmitButton->setEnabled(true);
            }

            // 停止超时计时器和写计时器
            writeTimer->stop();
            connTimer->stop();

            QMessageBox::information(this, "提示", "设备连接成功", u8"确定");

            qDebug("enter next step->start flashing");
            flash_step = FlashStep_Flashing;
            // 停止write计时器
            writeTimer->stop();
            // 重新启动超时计时器，如果连接设备成功后，2min内没有升级操作，那么重新复位设备，然后断开连接
            connTimer->start(OP_TIMEOUT);
        }

        return;
    }

    quint32 frame_len = 0;   // 报文长度，从报文长度高字节之后，CRC校验低字节之前的部分
    qDebug("\r\n\r\n<<------------解析充电站信息---------->>");
    // 把所有数据读取出来
    QByteArray::const_iterator it = message.begin();
    for( quint16 idx=0 ; it != message.end(); it++, idx++ )
    {
        rec_buff[idx] = (quint8)(*it);
    }
    frame_len = rec_buff[4] & 0xff;
    frame_len |= ( ( (rec_buff[5] & 0xff) << 8) & 0xFF00 );
    qDebug("whole frame size:%d, frame_len: %d, (hex:0x%04X)", message.size(), frame_len, frame_len);

    // 校验帧头
    quint8 buff_header_std[] = {0x55, 0xAA, 0xA5, 0x5A};
    for( quint16 idx = 0; idx < 4; idx ++ )
    {
        qDebug("chk header: %02x", rec_buff[idx] );
        if( buff_header_std[idx] != rec_buff[idx] )
        {
            qDebug() << "header error";
            return;
        }
    }

    // 校验crc值
    quint16 crc_read = 0;
    quint16 crc_calc = 0;

    crc_calc = crc16( &rec_buff[4], frame_len+2  );
    crc_read = (rec_buff[frame_len+2+4] & 0xFF) | \
               ( ( ( rec_buff[frame_len+2+4+1] & 0xFF ) << 8) & 0xFF00 );
    if( crc_calc != crc_read )
    {
        qDebug("crc check failed, crc_read:%04X, crc_calc:%04X", crc_read, crc_calc);
        return;
    }
    else
    {
        qDebug("crc check success, crc_read:%04X, crc_calc:%04X", crc_read, crc_calc);
    }

    uint8_t fc = 0;
    uint8_t cc = 0;
    // 判断功能码和命令字
    fc = rec_buff[OFFSET_FC];
    cc = rec_buff[OFFSET_CC];
    if( fc == (uint8_t)FC_QUERY ) {
        if(cc == CC_QUERY_CONN ) {
            qDebug("rec device ack, device conn success");
            onDeviceConnSuccess();
        }
    }else if( fc == (uint8_t)FC_CTL ) {
        if(cc == CC_SET_IAP ) {
            qDebug("rec device ack, device rst success");
            if( flash_step == FlashStep_RstDev ) {
                qDebug("enter next step->start flashing");
                flash_step = FlashStep_Flashing;

                // 停止write计时器
                writeTimer->stop();
                // 重新启动超时计时器，如果连接设备成功后，2min内没有升级操作，那么重新复位设备，然后断开连接
                connTimer->start(OP_TIMEOUT);
            }
        }
    }

    qDebug() << "rec_len:" << message.length();
    qDebug() << "data: " << message;
    qDebug("%02x", message.at(0));
}

void Widget::onDeviceConnSuccess()
{
    connPDlg->progressCnt = 100;
    connPDlg->setValue( connPDlg->progressCnt );
    connPDlg->progressCnt = 0;
    connPDlg->setValue( connPDlg->progressCnt );
    connPDlg->reset();
    connPDlg->hide();
    connPDlg->progressTimer->stop();

    ui->transmitBrowse->setEnabled(true);
    if(ui->transmitPath->text().isEmpty() != true) {
        qDebug("set enable");
        ui->transmitButton->setEnabled(true);
    }

    // 停止超时计时器和写计时器
    writeTimer->stop();
    connTimer->stop();

    QMessageBox::information(this, "提示", "设备连接成功", u8"确定");

    if( flash_step == FlashStep_ToConnDev ) {
        qDebug("enter next step->send rst");
        flash_step = FlashStep_RstDev;

        // 重新启动计时器
        connTimer->start( CONN_TIMEOUT );
        writeTimer->start(WR_PERIOD);
    }
}

void Widget::writeTimeOut()
{
    qDebug("write timeout");
    uint8_t query_buff[] = { 0x55,0xAA,0xA5,0x5A,0x07,0x00,0x00,0x00,0x04,0xFF,0xFF,0xFF,0xFF,0x94,0xDC };
    uint8_t rst_buff[] = { 0x55,0xAA,0xA5,0x5A,0x07,0x00,0x00,0x03,0x03,0xFF,0xFF,0xFF,0xFF,0x67,0x69 };
    quint32 len;
    QByteArray data;

    switch(flash_step) {
    case FlashStep_ToConnDev:
        qDebug("send conn package");
        len = sizeof(query_buff)/sizeof(uint8_t);
        qDebug() << "send len:" << len;
        for(quint32 i=0; i<len; i++)
            data.append( query_buff[i] );
        mytcpclient->sendMessage(data);
        break;
    case FlashStep_RstDev:
        qDebug("send rst package");
        len = sizeof(rst_buff)/sizeof(uint8_t);
        for(quint32 i=0; i<len; i++)
            data.append( rst_buff[i] );

        mytcpclient->sendMessage(data);
        break;
    case FlashStep_Flashing:
        qDebug("flashing...");
        break;
    case FlashStep_FlashFinish:
        qDebug("flash finished...");
        break;
    default:
        break;
    }
}

void Widget::connTimeOut()
{
    if( flash_step == FlashStep_ToConnDev) {
        qDebug("conn timerout");

        connect(mytcpclient, SIGNAL(myClientDisconnected()), this, SLOT(onTcpClientDisconnected()));

        connPDlg->hide();
        connPDlg->progressTimer->stop();
        connPDlg->progressCnt = 0;
        connPDlg->setValue( connPDlg->progressCnt );

        onTcpClientStopButtonClicked();

        // 停止连接超时定时器
        connTimer->stop();

        QMessageBox::warning(this, "设备连接失败", "请检查通信连接!", u8"确定");
    }else if( flash_step == FlashStep_Flashing) {
        flash_step = FlashStep_ToConnDev;

        // 发送3，跳转到app区域
        connect(mytcpclient, SIGNAL(newMessage(QString, QByteArray)), this, SLOT(onTcpClientAppendMessage(QString, QByteArray)));
        qDebug() << "send 3, jump to app";
        QString text = "0x33";
        mytcpclient->sendMessage(text);

        mytcpclient->closeClient();

        ui->transmitButton->setDisabled(true);
        ui->transmitPath->setText("");
        ui->transmitBrowse->setDisabled(true);

        QMessageBox::warning(this, "超时", "2分钟内没有执行升级操作，断开设备", u8"确定");

        // 停止连接超时定时器
        connTimer->stop();
    }
}

void Widget::connPDlgTimeout()
{
    connPDlg->setValue( ++connPDlg->progressCnt );
}

void Widget::on_transmitBrowse_clicked()
{
//    ui->transmitPath->setText(QFileDialog::getOpenFileName(this, u8"打开文件", ".", u8"任意文件 (*.*)"));
//    fileDlg->show();
//    fileDlg->setWindowTitle(tr("打开文件"));
//    fileDlg->setNameFilter(tr("任意文件 (*.*)"));
    ui->transmitPath->setText(fileDlg->getOpenFileName(this, u8"打开文件", ".", u8"任意文件 (*.*)"));

    if(ui->transmitPath->text().isEmpty() != true)
    {
        ui->transmitButton->setEnabled(true);
    }
    else
    {
        ui->transmitButton->setDisabled(true);
    }
}

void Widget::on_transmitButton_clicked()
{
    if(transmitButtonStatus == false)
    {
//        mytcpclient->closeClient();
        disconnect(mytcpclient, SIGNAL(newMessage(QString, QByteArray)), this, SLOT(onTcpClientAppendMessage(QString, QByteArray)));
        ymodemFileTransmit->setFileName(ui->transmitPath->text());

        QHostAddress addr;
        quint16 port;
        addr.setAddress(ui->lineEdit_TcpClientTargetIP->text());
        port = ui->lineEdit_TcpClientTargetPort->text().toInt();
        ymodemFileTransmit->setTcpClientSetting( addr, port );

        ymodemFileTransmit->startTransmit(); // 里面重新打开tcp连接，进行数据传输
    }
    else
    {
        ymodemFileTransmit->stopTransmit();
    }
}

// 检测重新打开的tcp连接是否正常
void Widget::transmitTcpclientResult(bool result)
{
    if(result)
    {
        qDebug("connect success");
        transmitButtonStatus = true;

        // 发送1，进行升级操作
        qDebug() << "send 1, update";
        QString text = "0x31";
        mytcpclient->sendMessage(text);

        ui->transmitBrowse->setDisabled(true);
        ui->transmitButton->setText(u8"取消");
        ui->transmitProgress->setValue(0);

        // 连接超时计时器
        qDebug()<<"停止超时计时器";
        connTimer->stop();

        ui->button_TcpClient->setDisabled(true);
    }
    else
    {
        QMessageBox::warning(this, u8"失败", u8"文件发送失败！", u8"关闭");
    }
}

void Widget::transmitProgress(int progress)
{
    ui->transmitProgress->setValue(progress);
}

void Widget::transmitStatus(Ymodem::Status status)
{
    switch(status)
    {
        case YmodemFileTransmit::StatusEstablish:
        {
            break;
        }

        case YmodemFileTransmit::StatusTransmit:
        {
            break;
        }

        case YmodemFileTransmit::StatusFinish:
        {
            transmitButtonStatus = false;

            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"发送");

            QMessageBox::warning(this, u8"成功", u8"程序升级成功！", u8"关闭");

            // 发送3，跳转到app区域
            connect(mytcpclient, SIGNAL(newMessage(QString, QByteArray)), this, SLOT(onTcpClientAppendMessage(QString, QByteArray)));
            qDebug() << "send 3, jump to app";
            QString text = "0x33";
            mytcpclient->sendMessage(text);

            mytcpclient->closeClient();

            ui->button_TcpClient->setEnabled(true);
            ui->transmitButton->setDisabled(true);
            ui->transmitPath->setText("");
            ui->transmitBrowse->setDisabled(true);

            ui->transmitProgress->setValue(0);

            break;
        }

        case YmodemFileTransmit::StatusAbort:
        {
            transmitButtonStatus = false;

            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"发送");

            QMessageBox::warning(this, u8"失败", u8"文件发送失败！", u8"关闭");

            mytcpclient->closeClient();

            ui->button_TcpClient->setEnabled(true);
            ui->transmitButton->setDisabled(true);
            ui->transmitPath->setText("");
            ui->transmitBrowse->setDisabled(true);

            ui->transmitProgress->setValue(0);

            break;
        }

        case YmodemFileTransmit::StatusTimeout:
        {
            transmitButtonStatus = false;

            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"发送");

            QMessageBox::warning(this, u8"失败", u8"文件发送失败！", u8"关闭");

            mytcpclient->closeClient();

            ui->button_TcpClient->setEnabled(true);
            ui->transmitButton->setDisabled(true);
            ui->transmitPath->setText("");
            ui->transmitBrowse->setDisabled(true);

            ui->transmitProgress->setValue(0);

            break;
        }

        default:
        {
            transmitButtonStatus = false;

            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"发送");

            QMessageBox::warning(this, u8"失败", u8"文件发送失败！", u8"关闭");

            mytcpclient->closeClient();

            ui->button_TcpClient->setEnabled(true);
            ui->transmitButton->setDisabled(true);
            ui->transmitPath->setText("");
            ui->transmitBrowse->setDisabled(true);

            ui->transmitProgress->setValue(0);
        }
    }
}

MyProgressDlg::MyProgressDlg(QWidget *parent):
    progressTimer(new QTimer())
{
//    setWindowTitle("连接设备");
//    setMinimum(0);
//    setMaximum(20);
//    setValue(0);
//    setLabelText("connecting...");
//    setWindowFlag(Qt::FramelessWindowHint);
//    setModal(true);
//    QPushButton *btn = nullptr;
//    setCancelButton(btn);
//    reset(); // 必须添加，否则new完后，会自动弹出

}

MyProgressDlg::~MyProgressDlg()
{
}

void MyProgressDlg::keyPressEvent(QKeyEvent *event)
{
    qDebug("widget, esc pressed");
    switch (event->key())
    {
    case Qt::Key_Escape:
        qDebug("widget, esc pressed");
        break;
    default:
        QDialog::keyPressEvent(event);
        break;
    }
}
