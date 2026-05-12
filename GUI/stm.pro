QT       += core gui serialport widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Windows специфичные настройки
win32 {
    # Для релизной сборки копируем файлы
    CONFIG(release, debug|release) {
        # Создаем команду копирования (одной строкой без &)
        QMAKE_POST_LINK += cmd /c "if not exist \"$$OUT_PWD/install_data\" mkdir \"$$OUT_PWD/install_data\" && if exist \"$$PWD/install_data/dpinst_amd64.exe\" copy \"$$PWD/install_data/dpinst_amd64.exe\" \"$$OUT_PWD/install_data/\""
    }
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
