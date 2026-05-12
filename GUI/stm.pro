QT       += core gui serialport widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

RESOURCES += resources.qrc

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

win32 {
    CONFIG(release, debug|release) {
        # Создаём папку install_data
        QMAKE_POST_LINK += cmd /c "if not exist \"$$OUT_PWD/install_data\" mkdir \"$$OUT_PWD/install_data\""
        # Копируем setup.exe (если есть в папке проекта)
        QMAKE_POST_LINK += & cmd /c "if exist \"$$PWD/install_data/setup.exe\" copy \"$$PWD/install_data/setup.exe\" \"$$OUT_PWD/install_data/\""
        # Копируем dpinst_amd64.exe (альтернативный вариант)
        QMAKE_POST_LINK += & cmd /c "if exist \"$$PWD/install_data/dpinst_amd64.exe\" copy \"$$PWD/install_data/dpinst_amd64.exe\" \"$$OUT_PWD/install_data/\""
    }
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc
