QT += core gui widgets multimedia multimediawidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

msvc {
    QMAKE_CFLAGS += /utf-8
    QMAKE_CXXFLAGS += /utf-8
}

win32-g++ {
    QMAKE_CFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}

SOURCES += \
    main.cpp \
    widget.cpp

HEADERS += \
    widget.h \
    xdmaDLL_public.h

FORMS += \
    widget.ui

INCLUDEPATH += $$PWD/driver
win32 {
    LIBS += -L$$PWD/driver -lXDMA_MoreB
}

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
