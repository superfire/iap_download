#-------------------------------------------------
#
# Project created by QtCreator 2019-09-24T10:10:52
#
#-------------------------------------------------

QT       += core gui
QT       += network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = IAP_Download
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

SOURCES += \
        main.cpp \
        widget.cpp \
    mytcpclient.cpp \
    Ymodem.cpp \
    YmodemFileTransmit.cpp

HEADERS += \
        widget.h \
    mytcpclient.h \
    Ymodem.h \
    YmodemFileTransmit.h

FORMS += \
        widget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# 图标位置
RC_ICONS += res/logo.ico
# 版本号
VERSION = 1.0.0.2
# 语言
# 0x0004 - 简体中文，参考链接：https://msdn.microsoft.com/en-us/library/dd318693%28vs.85%29.aspx
RC_LANG = 0x0004
# 公司名
QMAKE_TARGET_COMPANY = 安徽博微智能电气有限公司
# 产品名称
QMAKE_TARGET_PRODUCT = 固件升级工具(海康定制版专用)
# 详细描述
QMAKE_TARGET_DESCRIPTION = 固件升级工具
# 版权
QMAKE_TARGET_COPYRIGHT = Copyright(C) 2019 CETC ECRIEEPOWER (ANHUI) CO.,LTD.
