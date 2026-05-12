#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>
#include <QStandardItemModel>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QFileDialog>
#include <QSaveFile>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <QMessageBox>
#include <QProcess>
#include <QFileInfo>
#include <QStorageInfo>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_connectButton_clicked();
    void on_connectButton_func();
    void readSerialData();
    void sendCommand(const QString &cmd);
    void on_configureDSButton_clicked();
    void on_configureLM75AButton_clicked();
    void on_rgbSlider_valueChanged(int value);
    void on_refreshButton_clicked();
    void on_autoUpdate();
    void resumeAutoUpdate();
    void on_rgbTestButton_clicked();
    void on_rgbToggleButton_clicked();
    void updateRgbButtonState(bool isOn);
    void on_startLoggingButton_clicked();
    void on_openLogsFolderButton_clicked();

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;
    QStandardItemModel *sensorModel;
    QString receivedBuffer;
    QTimer *updateTimer;
    QTimer *pauseTimer;

    void appendLog(const QString &msg);
    void refreshPortList();

    // Флаги и переменные
    bool waitingForResponse;
    int consecutiveErrors;
    bool autoUpdatePaused;

    void pauseAutoUpdate(int seconds = 3);

    struct DS18B20Params {
        int index;
        QString romCode;
        float th;
        float tl;
        int resolution;
        QString resolutionStr;
        bool isGenuine;
        bool isConnected;
        bool isActive;
    };
    QList<DS18B20Params> ds18b20Params;
    float lm75aTH = 30.0;
    float lm75aTL = -10.0;

    struct LM75AParams {
        bool isActive;
        float th;
        float tl;
    };
    LM75AParams lm75aParams;

    // Флаги для предотвращения повторного открытия диалогов
    bool dsConfigDialogOpen;
    bool dsConfigDialog_warning;
    bool lm75aConfigDialogOpen;
    bool lm75aConfigDialog_warning;
    bool repeat;
    bool logg;

    // Парсинг данных из текстового протокола
    void parseDs18b20Line(const QString &line);
    void parseLm75aLine(const QString &line);
    void parseGenuineStatus(const QString &line);
    void handleSensorDisconnected(const QString &romCode);
    void handleSensorConnected(const QString &romCode);
    void updateOrAddDS18B20Sensor(int id, float temp, const QString &romCode);
    void updateLM75ASensor(float temp, bool connected, float th = 0, float tl = 0);
    void updateRowColor(int row, float temp, float th, float tl);
    void handleSensorCountUpdate(int newCount);
    void updateButtonsState();

    void DialogOptDB();
    void DialogOptLM();
    void DialogLogg();

    QFile logFile;
    QString currentLogFileName;
    qint64 maxLogSize = 3 * 1024 * 1024; // 3 МБ
    bool loggingEnabled;

    void initLogFile();
    void writeToLog(const QString &message);
    void archiveLogFile();
    void closeLogFile();

    void checkSTM32Driver();
    void installDriver();
    bool isSTM32Connected();
    void on_checkDriverButton_clicked();
};

#endif
