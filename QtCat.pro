# -------------------------------------------------
# Project created by QtCreator 2010-03-24T16:38:47
# -------------------------------------------------
QT += network
QT += script
TARGET = QtCat
TEMPLATE = app
SOURCES += main.cpp \
    mainwindow.cpp
HEADERS += mainwindow.h
FORMS += mainwindow.ui
LIBS += -L/usr/local/lib \
    -L/usr/lib/libssl.so
OTHER_FILES +=
unix|win32:LIBS += -lqextserialport
RESOURCES += resources.qrc
CONFIG += release
