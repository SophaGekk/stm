#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QInputDialog>
#include <QDateTime>
#include <QDialog>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QScrollBar>
#include <QThread>
#include <QBrush>
#include <QColor>
#include <QTimer>
#include <QRegularExpression>
#include <QSettings>
#include <QProcess>
#include <QDirIterator>
#include <QDir>
#include <QStorageInfo>

// ==================== ДИАЛОГ НАСТРОЙКИ DS18B20 ====================
class DS18B20ConfigDialog : public QDialog
{
public:
    DS18B20ConfigDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("Настройка DS18B20");
        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QFormLayout *formLayout = new QFormLayout();

        thSpin = new QDoubleSpinBox();
        thSpin->setRange(-55, 125);
        thSpin->setDecimals(1);
        thSpin->setSuffix(" °C");
        thSpin->setValue(30.0);
        formLayout->addRow("Порог TH (макс):", thSpin);

        tlSpin = new QDoubleSpinBox();
        tlSpin->setRange(-55, 125);
        tlSpin->setDecimals(1);
        tlSpin->setSuffix(" °C");
        tlSpin->setValue(-10.0);
        formLayout->addRow("Порог TL (мин):", tlSpin);

        resCombo = new QComboBox();
        resCombo->addItem("9 бит (0.5°C)", 9);
        resCombo->addItem("10 бит (0.25°C)", 10);
        resCombo->addItem("11 бит (0.125°C)", 11);
        resCombo->addItem("12 бит (0.0625°C)", 12);
        resCombo->setCurrentIndex(3);
        formLayout->addRow("Разрешение:", resCombo);

        mainLayout->addLayout(formLayout);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(buttonBox);
    }

    void setTH(double value) { thSpin->setValue(value); }
    void setTL(double value) { tlSpin->setValue(value); }
    void setResolution(int value) {
        for(int i = 0; i < resCombo->count(); i++) {
            if(resCombo->itemData(i).toInt() == value) {
                resCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    double getTH() const { return thSpin->value(); }
    double getTL() const { return tlSpin->value(); }
    int getResolution() const { return resCombo->currentData().toInt(); }

private:
    QDoubleSpinBox *thSpin;
    QDoubleSpinBox *tlSpin;
    QComboBox *resCombo;
};

// ==================== ДИАЛОГ НАСТРОЙКИ LM75A ====================
class LM75AConfigDialog : public QDialog
{
public:
    LM75AConfigDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("Настройка LM75A");
        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QFormLayout *formLayout = new QFormLayout();

        thSpin = new QDoubleSpinBox();
        thSpin->setRange(-55, 125);
        thSpin->setDecimals(1);
        thSpin->setSuffix(" °C");
        thSpin->setValue(30.0);
        formLayout->addRow("Порог TH:", thSpin);

        tlSpin = new QDoubleSpinBox();
        tlSpin->setRange(-55, 125);
        tlSpin->setDecimals(1);
        tlSpin->setSuffix(" °C");
        tlSpin->setValue(-10.0);
        formLayout->addRow("Порог TL:", tlSpin);

        mainLayout->addLayout(formLayout);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(buttonBox);
    }

    double getTH() const { return thSpin->value(); }
    double getTL() const { return tlSpin->value(); }
    void setTH(double value) { thSpin->setValue(value); }
    void setTL(double value) { tlSpin->setValue(value); }

private:
    QDoubleSpinBox *thSpin;
    QDoubleSpinBox *tlSpin;
};

// ==================== ДИАЛОГ ВЫБОРА ДАТЧИКА DS18B20 ====================
class SelectDS18B20Dialog : public QDialog
{
public:
    SelectDS18B20Dialog(int sensorCount, QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("Выбор датчика DS18B20");
        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        QLabel *label = new QLabel("Выберите датчик для настройки:");
        mainLayout->addWidget(label);

        sensorCombo = new QComboBox();
        for(int i = 0; i < sensorCount; i++) {
            sensorCombo->addItem(QString("DS18B20 #%1").arg(i));
        }
        sensorCombo->addItem("Применить ко всем датчикам");
        sensorCombo->setCurrentIndex(0);
        mainLayout->addWidget(sensorCombo);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(buttonBox);
    }

    int getSelectedIndex() const {
        int idx = sensorCombo->currentIndex();
        int sensorCount = sensorCombo->count() - 1;
        if(idx == sensorCount) return -1;
        return idx;
    }

private:
    QComboBox *sensorCombo;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QIcon icon(":/app_icon.ico");
    setWindowIcon(icon);

    QTimer::singleShot(500, this, &MainWindow::checkSTM32Driver);
    lm75aParams.isActive = false;
    lm75aParams.th = 30.0;
    lm75aParams.tl = -10.0;
    serial = new QSerialPort(this);
    updateTimer = new QTimer(this);
    pauseTimer = new QTimer(this);

    // Инициализация флагов
    waitingForResponse = false;
    consecutiveErrors = 0;
    autoUpdatePaused = false;
    dsConfigDialogOpen = false;
    lm75aConfigDialogOpen = false;
    dsConfigDialog_warning = false;
    lm75aConfigDialog_warning = false;
    logg = false;
    repeat = false;
    loggingEnabled = false;
    currentLogFileName = "";

    sensorModel = new QStandardItemModel(this);
    sensorModel->setHorizontalHeaderLabels({"Датчик", "ROM код", "Температура", "TH", "TL", "Разрядность", "Статус"});
    ui->tableView->setModel(sensorModel);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    refreshPortList();

    connect(ui->checkDriverButton, &QPushButton::clicked, this, &MainWindow::on_checkDriverButton_clicked);
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readSerialData);
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::on_connectButton_func);
    connect(ui->configureDSButton, &QPushButton::clicked, this, &MainWindow::DialogOptDB);
    connect(ui->configureLM75AButton, &QPushButton::clicked, this, &MainWindow::DialogOptLM);
    connect(ui->rgbSlider, &QSlider::valueChanged, this, &MainWindow::on_rgbSlider_valueChanged);
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::on_refreshButton_clicked);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::on_autoUpdate);
    connect(pauseTimer, &QTimer::timeout, this, &MainWindow::resumeAutoUpdate);
    connect(ui->startLoggingButton, &QPushButton::clicked, this, &MainWindow::DialogLogg);
    connect(ui->openLogsFolderButton, &QPushButton::clicked, this, &MainWindow::on_openLogsFolderButton_clicked);

    connect(ui->rgbSlider, &QSlider::valueChanged, this, [this](int val){
        ui->rgbValueLabel->setText(QString::number(val) + "%");
    });
    ui->rgbValueLabel->setText("0%");

    ui->statusbar->showMessage("Не подключено");

    receivedBuffer.clear();
}

MainWindow::~MainWindow()
{
    if(serial->isOpen()) serial->close();
    if(loggingEnabled) {
        closeLogFile();
    }
    delete ui;
}

void MainWindow::on_checkDriverButton_clicked()
{
    if (isSTM32Connected()) {
        QMessageBox::information(this, "Драйвер",
                                 "STM32 устройство обнаружено!\nДрайвер работает корректно.");
        appendLog("✅ STM32 устройство обнаружено");
    } else {
        QMessageBox::warning(this, "Драйвер",
                             "STM32 устройство не обнаружено.\n"
                             "Возможно, драйвер не установлен или плата не подключена.");
        checkSTM32Driver();
    }
}
void MainWindow::refreshPortList()
{
    ui->portCombo->clear();
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        ui->portCombo->addItem(info.portName());
    }
    ui->baudCombo->setCurrentText("9600");
}

void MainWindow::pauseAutoUpdate(int seconds)
{
    if(updateTimer->isActive()) {
        updateTimer->stop();
        autoUpdatePaused = true;
        appendLog(QString("Автообновление приостановлено на %1 сек").arg(seconds));
        pauseTimer->start(seconds * 1000);
    }
}

void MainWindow::on_connectButton_func(){
    on_connectButton_clicked();
    repeat = false;
}

void MainWindow::on_connectButton_clicked()
{
    if(repeat) {
        return;
    }
    repeat = true;
    if(serial->isOpen()) {
        serial->close();
        updateTimer->stop();
        pauseTimer->stop();
        autoUpdatePaused = false;
        ui->connectButton->setText("Подключиться");
        ui->statusbar->showMessage("Отключено");
        appendLog("Отключено от " + serial->portName());
        return;
    }

    if(ui->portCombo->currentText().isEmpty()) {
        QMessageBox::warning(this, "Предупреждение", "Выберите COM-порт");
        return;
    }

    serial->setPortName(ui->portCombo->currentText());
    serial->setBaudRate(ui->baudCombo->currentText().toInt());
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if(serial->open(QIODevice::ReadWrite)) {
        ui->connectButton->setText("Отключиться");
        ui->statusbar->showMessage("Подключено к " + ui->portCombo->currentText() +
                                   " (" + ui->baudCombo->currentText() + " бод)");
        appendLog("Подключено к " + ui->portCombo->currentText());

        receivedBuffer.clear();
        waitingForResponse = false;
        consecutiveErrors = 0;
        autoUpdatePaused = false;
        sensorModel->removeRows(0, sensorModel->rowCount());
        ds18b20Params.clear();
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть порт " + ui->portCombo->currentText());
        ui->statusbar->showMessage("Ошибка подключения");
    }
}

void MainWindow::sendCommand(const QString &cmd)
{
    if(!serial || !serial->isOpen()) {
        appendLog("TX: " + cmd + " - порт не открыт!");
        if(loggingEnabled) {
            writeToLog(QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + " TX: " + cmd + " - порт не открыт!");
        }
        return;
    }

    QByteArray data = cmd.toUtf8() + "\r\n";
    serial->write(data);
    serial->flush();
    appendLog("TX: " + cmd);

    if(loggingEnabled) {
        writeToLog(QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + " TX: " + cmd);
    }
}

void MainWindow::readSerialData()
{
    if(!serial) return;

    QByteArray data = serial->readAll();
    if(data.isEmpty()) return;

    QString received = QString::fromUtf8(data);
    receivedBuffer += received;

    if(receivedBuffer.contains('\n')) {
        QStringList lines = receivedBuffer.split('\n');
        receivedBuffer = lines.last();

        for(int i = 0; i < lines.size() - 1; ++i) {
            QString line = lines[i].trimmed();
            if(line.isEmpty()) continue;

            if(loggingEnabled) {
                writeToLog(QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + " RX: " + line);
            }

            appendLog("RX: " + line);

            // Обработка отключения датчика
            if(line.contains("Sensor disconnected:")) {
                QString romCode = line.mid(line.indexOf(":") + 2).trimmed();
                handleSensorDisconnected(romCode);
            }
            // Обработка подключения датчика
            else if(line.contains("Sensor connected:")) {
                QString romCode = line.mid(line.indexOf(":") + 2).trimmed();
                handleSensorConnected(romCode);
            }
            // Обработка DS18B20 данных
            else if(line.contains("DS18B20 Sensor") && line.contains("):")) {
                parseDs18b20Line(line);
            }
            // Обработка LM75A данных
            else if(line.contains("LM75A Sensor")) {
                parseLm75aLine(line);
            }
            // Информация о подлинности датчика
            else if(line.contains("DS18B20 #") && (line.contains("is GENUINE") || line.contains("is FAKE"))) {
                parseGenuineStatus(line);
            }
            // Количество датчиков
            else if(line.contains("Sensor count:")) {
                int count = line.mid(line.indexOf(":") + 1).trimmed().toInt();
                handleSensorCountUpdate(count);
            }
            // Подтверждение настройки датчика
            else if(line.contains("DS18B20 #") && line.contains("configured:")) {
                appendLog("✅ " + line);
                // Можно обновить статус в таблице, если нужно
                QTimer::singleShot(100, this, [this]() {
                    sendCommand("temp");  // Запрашиваем обновленные данные
                });
            }
            // Ошибка настройки
            else if(line.contains("ERROR: Invalid set_ds command")) {
                appendLog("❌ " + line);
                QMessageBox::warning(this, "Ошибка",
                                     "Не удалось настроить датчик DS18B20.\n"
                                     "Проверьте правильность параметров.");
            }
            else if(line.contains("LM75A sensor connected")) {
                appendLog("🟢 " + line);
                QMessageBox::information(this, "LM75A", "Датчик LM75A подключен!");
                // Запрашиваем обновленные данные
                QTimer::singleShot(500, this, [this]() {
                    sendCommand("temp");
                });
            }
            else if(line.contains("LM75A sensor disconnected")) {
                appendLog("🔴 " + line);
                QMessageBox::warning(this, "LM75A", "Датчик LM75A отключен!");
                updateLM75ASensor(0, false);
            }

            // Обработка RGB подключения/отключения
            else if(line.contains("RGB module connected")) {
                appendLog("🟢 " + line);
                ui->statusbar->showMessage("RGB модуль подключен");
                QMessageBox::information(this, "RGB", "RGB модуль подключен!");
                ui->rgbSlider->setEnabled(true);
                ui->rgbToggleButton->setEnabled(true);
                ui->rgbToggleButton->setText("Выключить RGB");
                ui->rgbTestButton->setEnabled(true);
            }
            else if(line.contains("RGB module disconnected")) {
                appendLog("🔴 " + line);
                ui->statusbar->showMessage("RGB модуль отключен");
                QMessageBox::warning(this, "RGB", "RGB модуль отключен!");
                ui->rgbSlider->setEnabled(false);
                ui->rgbTestButton->setEnabled(false);
                ui->rgbToggleButton->setText("Включить RGB");
                ui->rgbToggleButton->setEnabled(false);
            }
            else if(line.contains("RGB module initialized successfully")) {
                appendLog("✅ " + line);
                ui->statusbar->showMessage("RGB модуль готов");
                ui->rgbSlider->setEnabled(true);
                ui->rgbToggleButton->setEnabled(true);
                ui->rgbTestButton->setEnabled(true);
            }
            else if(line.contains("RGB module not found")) {
                appendLog("⚠️ " + line);
                ui->statusbar->showMessage("RGB модуль не обнаружен");
            }

        }
    }

    waitingForResponse = false;
}

void MainWindow::handleSensorDisconnected(const QString &romCode)
{
    // Ищем датчик по ROM коду и помечаем как OFFLINE
    for(int row = 0; row < sensorModel->rowCount(); row++) {
        QString sensorRom = sensorModel->data(sensorModel->index(row,1)).toString();
        if(sensorRom == romCode) {
            // Сохраняем текущие данные датчика перед пометкой как OFFLINE
            QString sensorName = sensorModel->data(sensorModel->index(row,0)).toString();

            // Отмечаем как оффлайн
            sensorModel->setData(sensorModel->index(row,6), "OFFLINE");

            // Запоминаем, что этот датчик отключен
            QBrush grayBrush(QColor(200, 200, 200));
            for (int col = 0; col < sensorModel->columnCount(); col++) {
                sensorModel->setData(sensorModel->index(row, col), grayBrush, Qt::BackgroundRole);
            }

            // Отмечаем в параметрах, что датчик отключен
            for(auto &p : ds18b20Params) {
                if(p.romCode == romCode) {
                    p.isConnected = false;
                    p.isActive = false;
                    break;
                }
            }

            // Показываем уведомление (НЕ меняем статус после закрытия!)
            QMessageBox::warning(this, "Датчик отключен",
                                 QString("Датчик %1 (%2) отключен!\nОн будет отображаться серым до переподключения.")
                                     .arg(sensorName).arg(romCode));
            updateButtonsState();
            break;
        }
    }
}

void MainWindow::handleSensorConnected(const QString &romCode)
{
    // Восстанавливаем статус подключенного датчика
    for(int row = 0; row < sensorModel->rowCount(); row++) {
        QString sensorRom = sensorModel->data(sensorModel->index(row,1)).toString();
        if(sensorRom == romCode) {
            // Сбрасываем статус OFFLINE
            sensorModel->setData(sensorModel->index(row,6), "OK");

            // Отмечаем в параметрах, что датчик подключен
            for(auto &p : ds18b20Params) {
                if(p.romCode == romCode) {
                    p.isConnected = true;
                    p.isActive = true;
                    break;
                }
            }

            // Сброс цвета будет при следующем обновлении температуры
            // Пока просто сбрасываем фон на белый
            QBrush whiteBrush(QColor(255, 255, 255));
            for (int col = 0; col < sensorModel->columnCount(); col++) {
                sensorModel->setData(sensorModel->index(row, col), whiteBrush, Qt::BackgroundRole);
            }

            // Показываем уведомление
            QString sensorName = sensorModel->data(sensorModel->index(row,0)).toString();
            QMessageBox::information(this, "Датчик подключен",
                                     QString("Датчик %1 (%2) подключен!").arg(sensorName).arg(romCode));
            updateButtonsState();
            break;
        }
    }
}

void MainWindow::handleSensorCountUpdate(int newCount)
{
    // Получаем список активных ROM кодов из текущих датчиков в модели
    QStringList activeRomCodes;
    for(int row = 0; row < sensorModel->rowCount(); row++) {
        QString status = sensorModel->data(sensorModel->index(row,6)).toString();
        QString romCode = sensorModel->data(sensorModel->index(row,1)).toString();
        // Если датчик не OFFLINE, считаем его активным
        if(status != "OFFLINE" && !romCode.isEmpty() && romCode != "I2C") {
            activeRomCodes.append(romCode);
        }
    }

    // Если количество активных датчиков не совпадает с newCount,
    // это значит, что какие-то датчики пропали, но сообщение об отключении
    // уже должно было прийти. Ничего не делаем, ждем сообщений disconnected.
}

void MainWindow::parseDs18b20Line(const QString &line)
{
    // Парсим: "DS18B20 Sensor 0 (2839B23C000000F3): 27.25 C"
    QRegularExpression rx("DS18B20 Sensor (\\d+) \\(([0-9A-F]{16})\\): ([0-9.-]+) C");
    QRegularExpressionMatch match = rx.match(line);

    if(match.hasMatch()) {
        int id = match.captured(1).toInt();
        QString romCode = match.captured(2);
        float temp = match.captured(3).toFloat();

        appendLog(QString("Получены данные DS18B20: #%1, температура %2°C").arg(id).arg(temp));

        // Проверяем, не отключен ли этот датчик
        bool isConnected = true;
        for(const auto &p : ds18b20Params) {
            if(p.romCode == romCode && !p.isConnected) {
                isConnected = false;
                break;
            }
        }

        if(isConnected) {
            updateOrAddDS18B20Sensor(id, temp, romCode);
        } else {
            // Датчик отключен, не обновляем его данные, но можно обновить ROM код если его нет
            bool found = false;
            for(int row = 0; row < sensorModel->rowCount(); row++) {
                if(sensorModel->data(sensorModel->index(row,1)).toString() == romCode) {
                    found = true;
                    break;
                }
            }
            if(!found) {
                updateOrAddDS18B20Sensor(id, temp, romCode);
                // Помечаем как OFFLINE сразу
                handleSensorDisconnected(romCode);
            }
        }
    }
}

void MainWindow::parseLm75aLine(const QString &line)
{
    // Парсим: "LM75A Sensor Address 0x48: 27.25 C"
    QRegularExpression rx("LM75A Sensor Address (0x[0-9A-F]{2}): ([0-9.-]+) C");
    QRegularExpressionMatch match = rx.match(line);

    if(match.hasMatch()) {
        QString address = match.captured(1);
        float temp = match.captured(2).toFloat();

        updateLM75ASensor(temp, true);
    } else if(line.contains("LM75A Sensor Not found")) {
        updateLM75ASensor(0, false);
    }
}

void MainWindow::parseGenuineStatus(const QString &line)
{
    // Парсим: "DS18B20 #0 is GENUINE" или "DS18B20 #0 is FAKE"
    QRegularExpression rx("DS18B20 #(\\d+) is (GENUINE|FAKE)");
    QRegularExpressionMatch match = rx.match(line);

    if(match.hasMatch()) {
        int id = match.captured(1).toInt();
        bool isGenuine = (match.captured(2) == "GENUINE");

        appendLog(QString("Обработка подлинности: DS18B20 #%1 = %2").arg(id).arg(isGenuine ? "Оригинальный" : "ПОДДЕЛКА!"));

        // Обновляем статус в таблице
        for(int row = 0; row < sensorModel->rowCount(); row++) {
            QString sensorName = sensorModel->data(sensorModel->index(row,0)).toString();

            // Проверяем по имени датчика
            if(sensorName == QString("DS18B20 #%1").arg(id)) {
                // Принудительно обновляем статус, даже если был "Проверяется..."
                appendLog(QString("  → Установлен статус для строки %1: %2").arg(row).arg(isGenuine ? "Оригинальный" : "ПОДДЕЛКА!"));
                break;
            }
        }

        // Обновляем параметры
        for(auto &p : ds18b20Params) {
            if(p.index == id) {
                p.isGenuine = isGenuine;
                appendLog(QString("  → Обновлены параметры для датчика #%1").arg(id));
                break;
            }
        }
    }
}

void MainWindow::updateOrAddDS18B20Sensor(int id, float temp, const QString &romCode)
{
    int row = -1;
    for(int r=0; r<sensorModel->rowCount(); r++) {
        if(sensorModel->data(sensorModel->index(r,1)).toString() == romCode) {
            row = r;
            break;
        }
    }

    // Определяем точность и разрядность
    int precision = 2;
    QString resolutionStr = "12 бит (0.0625°C)";
    float th = 30.0f;
    float tl = -10.0f;
    bool isGenuine = false;
    bool isConnected = true;
    bool hasGenuineInfo = false;

    for(const auto &p : ds18b20Params) {
        if(p.romCode == romCode) {
            precision = (p.resolution == 9) ? 1 : (p.resolution == 10) ? 2 : (p.resolution == 11) ? 3 : 4;
            resolutionStr = p.resolutionStr;
            th = p.th;
            tl = p.tl;
            isGenuine = p.isGenuine;
            isConnected = p.isConnected;
            hasGenuineInfo = true;  // Уже есть информация о подлинности
            break;
        }
    }

    if(row == -1) {
        row = sensorModel->rowCount();
        sensorModel->insertRow(row);
        sensorModel->setData(sensorModel->index(row,0), QString("DS18B20 #%1").arg(id));
        sensorModel->setData(sensorModel->index(row,1), romCode);

        DS18B20Params params;
        params.index = id;
        params.romCode = romCode;
        params.th = 30.0;
        params.tl = -10.0;
        params.resolution = 12;
        params.resolutionStr = "12 бит (0.0625°C)";
        params.isGenuine = false;
        params.isConnected = true;
        params.isActive = true;
        ds18b20Params.append(params);
        hasGenuineInfo = false;  // Новый датчик, информации о подлинности еще нет
    }

    // Форматируем температуру с нужной точностью
    QString tempStr = QString::number(temp, 'f', precision) + " °C";
    sensorModel->setData(sensorModel->index(row,2), tempStr);
    sensorModel->setData(sensorModel->index(row,3), QString::number(th, 'f', 1) + " °C");
    sensorModel->setData(sensorModel->index(row,4), QString::number(tl, 'f', 1) + " °C");
    sensorModel->setData(sensorModel->index(row,5), resolutionStr);

    // Статус: если отключен - OFFLINE
    if(!isConnected) {
        sensorModel->setData(sensorModel->index(row,6), "OFFLINE");
        QBrush grayBrush(QColor(200, 200, 200));
        for (int col = 0; col < sensorModel->columnCount(); col++) {
            sensorModel->setData(sensorModel->index(row, col), grayBrush, Qt::BackgroundRole);
        }
    } else {
        // Если уже есть информация о подлинности, отображаем её
        if(hasGenuineInfo && isGenuine) {
            sensorModel->setData(sensorModel->index(row,6), "Оригинальный");
        }
        else if(hasGenuineInfo && !isGenuine) {
            sensorModel->setData(sensorModel->index(row,6), "ПОДДЕЛКА!");
        }
        else {
            // Если информации о подлинности еще нет, ставим "Проверяется..."
            QString currentStatus = sensorModel->data(sensorModel->index(row,6)).toString();
            if(currentStatus != "Оригинальный" && currentStatus != "ПОДДЕЛКА!" && currentStatus != "OFFLINE") {
                sensorModel->setData(sensorModel->index(row,6), "Проверяется...");
            }
        }
        updateRowColor(row, temp, th, tl);
    }
}

void MainWindow::updateLM75ASensor(float temp, bool connected, float th, float tl)
{
    int row = -1;
    for(int r=0; r<sensorModel->rowCount(); r++) {
        if(sensorModel->data(sensorModel->index(r,0)).toString() == "LM75A") {
            row = r;
            break;
        }
    }

    if(row == -1) {
        row = sensorModel->rowCount();
        sensorModel->insertRow(row);
        sensorModel->setData(sensorModel->index(row,0), "LM75A");
        sensorModel->setData(sensorModel->index(row,1), "---");
    }

    if(connected) {
        if(th != 0) lm75aParams.th = th;
        if(tl != 0) lm75aParams.tl = tl;
        lm75aTH = lm75aParams.th;
        lm75aTL = lm75aParams.tl;
        lm75aParams.isActive = true;

        sensorModel->setData(sensorModel->index(row,2), QString::number(temp, 'f', 2) + " °C");
        sensorModel->setData(sensorModel->index(row,3), QString::number(lm75aParams.th, 'f', 1) + " °C");
        sensorModel->setData(sensorModel->index(row,4), QString::number(lm75aParams.tl, 'f', 1) + " °C");
        sensorModel->setData(sensorModel->index(row,5), "11 бит (0.125°C)");
        sensorModel->setData(sensorModel->index(row,6), "OK");
        updateRowColor(row, temp, lm75aParams.th, lm75aParams.tl);
    } else {
        lm75aParams.isActive = false;
        sensorModel->setData(sensorModel->index(row,2), "---");
        sensorModel->setData(sensorModel->index(row,3), "---");
        sensorModel->setData(sensorModel->index(row,4), "---");
        sensorModel->setData(sensorModel->index(row,5), "---");
        sensorModel->setData(sensorModel->index(row,6), "OFFLINE");
        QBrush grayBrush(QColor(200, 200, 200));
        for (int col = 0; col < sensorModel->columnCount(); col++) {
            sensorModel->setData(sensorModel->index(row, col), grayBrush, Qt::BackgroundRole);
        }
    }
    updateButtonsState();
}

void MainWindow::updateButtonsState()
{
    // Проверяем наличие АКТИВНЫХ DS18B20 (не отключенных)
    bool hasActiveDS18B20 = false;
    for(const auto &p : ds18b20Params) {
        if(p.isActive) {
            hasActiveDS18B20 = true;
            break;
        }
    }
    ui->configureDSButton->setEnabled(hasActiveDS18B20);

    // Проверяем наличие АКТИВНОГО LM75A
    bool hasActiveLM75A = lm75aParams.isActive;
    ui->configureLM75AButton->setEnabled(hasActiveLM75A);

    // Обновляем статус в строке состояния
    if(!hasActiveDS18B20 && !hasActiveLM75A) {
        ui->statusbar->showMessage("⚠️ Нет активных датчиков! Проверьте подключение.");
    } else if(!hasActiveDS18B20) {
        ui->statusbar->showMessage("⚠️ Нет активных датчиков DS18B20. LM75A готов.");
    } else if(!hasActiveLM75A) {
        ui->statusbar->showMessage("⚠️ Датчик LM75A не активен. DS18B20 готовы.");
    } else {
        ui->statusbar->showMessage("✅ Все датчики активны");
    }
}

void MainWindow::resumeAutoUpdate()
{
    pauseTimer->stop();
    if(serial && serial->isOpen() && autoUpdatePaused) {
        updateTimer->start(8000);
        autoUpdatePaused = false;
        appendLog("Автообновление возобновлено");
    }
}

void MainWindow::on_autoUpdate()
{
    if(!serial || !serial->isOpen()) {
        return;
    }
    if(autoUpdatePaused) {
        return;
    }
    // Запрашиваем данные командой temp
    //sendCommand("temp");
}

void MainWindow::updateRowColor(int row, float temp, float th, float tl)
{
    QColor bgColor;
    if (temp > th) {
        bgColor = QColor(255, 200, 200);
    } else if (temp < tl) {
        bgColor = QColor(200, 200, 255);
    } else {
        bgColor = QColor(255, 255, 255);
    }
    QBrush brush(bgColor);
    for (int col = 0; col < sensorModel->columnCount(); col++) {
        sensorModel->setData(sensorModel->index(row, col), brush, Qt::BackgroundRole);
    }
}

void MainWindow::appendLog(const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    ui->logTextEdit->append(timestamp + " " + msg);
    QScrollBar *bar = ui->logTextEdit->verticalScrollBar();
    bar->setValue(bar->maximum());
}


void MainWindow::on_configureDSButton_clicked()
{
    if(dsConfigDialogOpen || dsConfigDialog_warning) {
        return;
    }
    dsConfigDialog_warning = true;

    if(ds18b20Params.isEmpty()) {
        QMessageBox::warning(this, "Предупреждение", "Нет доступных датчиков DS18B20!");
        return;
    }

    // Приостанавливаем автообновление
    pauseAutoUpdate(10);
    dsConfigDialogOpen = true;

    SelectDS18B20Dialog selectDlg(ds18b20Params.size(), this);

    int result = selectDlg.exec();
    qDebug() << "Dialog result:" << result;

    if(result != QDialog::Accepted) {
        return;
    }

    int selectedIndex = selectDlg.getSelectedIndex();
    bool applyToAll = (selectedIndex == -1);

    qDebug() << "Selected index:" << selectedIndex;
    qDebug() << "Apply to all:" << applyToAll;

    DS18B20ConfigDialog dlg(this);

    if(!applyToAll && selectedIndex >= 0 && selectedIndex < ds18b20Params.size()) {
        qDebug() << "Loading current settings for sensor" << selectedIndex;
        dlg.setTH(ds18b20Params[selectedIndex].th);
        dlg.setTL(ds18b20Params[selectedIndex].tl);
        dlg.setResolution(ds18b20Params[selectedIndex].resolution);
    }

    if(dlg.exec() == QDialog::Accepted) {
        double tl = dlg.getTL();
        double th = dlg.getTH();
        int resolutionValue = dlg.getResolution();
        QString resolutionStr;
        switch(resolutionValue) {
        case 9: resolutionStr = "9 бит (0.5°C)"; break;
        case 10: resolutionStr = "10 бит (0.25°C)"; break;
        case 11: resolutionStr = "11 бит (0.125°C)"; break;
        default: resolutionStr = "12 бит (0.0625°C)"; break;
        }

        qDebug() << "New settings: TH=" << th << "TL=" << tl << "Res=" << resolutionValue;

        if(applyToAll) {
            for(int i = 0; i < ds18b20Params.size(); i++) {
                QString cmd = QString("set_ds %1,%2,%3,%4")
                .arg(ds18b20Params[i].index)
                    .arg(th, 0, 'f', 1)
                    .arg(tl, 0, 'f', 1)
                    .arg(resolutionValue);

                qDebug() << "Sending command:" << cmd;
                sendCommand(cmd);
                QThread::msleep(200);

                ds18b20Params[i].th = th;
                ds18b20Params[i].tl = tl;
                ds18b20Params[i].resolution = resolutionValue;
                ds18b20Params[i].resolutionStr = resolutionStr;
            }
            appendLog(QString("✅ Все DS18B20 настроены: TH=%1°C, TL=%2°C, %3")
                          .arg(th).arg(tl).arg(resolutionStr));
        }
        else if(selectedIndex >= 0 && selectedIndex < ds18b20Params.size()) {
            QString cmd = QString("set_ds %1,%2,%3,%4")
            .arg(ds18b20Params[selectedIndex].index)
                .arg(th, 0, 'f', 1)
                .arg(tl, 0, 'f', 1)
                .arg(resolutionValue);

            qDebug() << "Sending command:" << cmd;
            sendCommand(cmd);

            ds18b20Params[selectedIndex].th = th;
            ds18b20Params[selectedIndex].tl = tl;
            ds18b20Params[selectedIndex].resolution = resolutionValue;
            ds18b20Params[selectedIndex].resolutionStr = resolutionStr;

            appendLog(QString("✅ DS18B20 #%1 настроен: TH=%2°C, TL=%3°C, %4")
                          .arg(ds18b20Params[selectedIndex].index).arg(th).arg(tl).arg(resolutionStr));
        }

        // Обновляем таблицу
        for(int row = 0; row < sensorModel->rowCount(); row++) {
            QString sensorName = sensorModel->data(sensorModel->index(row,0)).toString();
            if(sensorName.startsWith("DS18B20 #")) {
                int idx = sensorName.split("#")[1].toInt();
                for(const auto& p : ds18b20Params) {
                    if(p.index == idx) {
                        sensorModel->setData(sensorModel->index(row,3), QString::number(p.th, 'f', 1) + " °C");
                        sensorModel->setData(sensorModel->index(row,4), QString::number(p.tl, 'f', 1) + " °C");
                        sensorModel->setData(sensorModel->index(row,5), p.resolutionStr);
                        break;
                    }
                }
            }
        }

        QMessageBox::information(this, "Информация", "Настройки отправлены на устройство.");
    }

}

void MainWindow::DialogOptDB(){
    on_configureDSButton_clicked();
    dsConfigDialogOpen = false;
    dsConfigDialog_warning = false;
}

void MainWindow::DialogOptLM(){
    on_configureLM75AButton_clicked();
    lm75aConfigDialogOpen = false;
    lm75aConfigDialog_warning = false;
}

void MainWindow::on_configureLM75AButton_clicked()
{
    if(lm75aConfigDialogOpen || lm75aConfigDialog_warning) {
        return;
    }

    bool hasLM75A = false;
    for(int row = 0; row < sensorModel->rowCount(); row++) {
        if(sensorModel->data(sensorModel->index(row,0)).toString() == "LM75A") {
            hasLM75A = true;
            break;
        }
    }
    lm75aConfigDialog_warning = true;
    if(!hasLM75A) {
        QMessageBox::warning(this, "Предупреждение", "Датчик LM75A не обнаружен!");
        return;
    }

    pauseAutoUpdate(10);
    lm75aConfigDialogOpen = true;
    LM75AConfigDialog dlg(this);
    dlg.setTH(lm75aTH);
    dlg.setTL(lm75aTL);

    if(dlg.exec() == QDialog::Accepted) {
        double th = dlg.getTH();
        double tl = dlg.getTL();

        // Отправляем команду на устройство
        QString cmd = QString("set_lm75a %1,%2")
                          .arg(th, 0, 'f', 1)
                          .arg(tl, 0, 'f', 1);

        sendCommand(cmd);

        lm75aTH = th;
        lm75aTL = tl;

        for(int row = 0; row < sensorModel->rowCount(); row++) {
            if(sensorModel->data(sensorModel->index(row,0)).toString() == "LM75A") {
                sensorModel->setData(sensorModel->index(row,3), QString::number(lm75aTH, 'f', 1) + " °C");
                sensorModel->setData(sensorModel->index(row,4), QString::number(lm75aTL, 'f', 1) + " °C");
                break;
            }
        }

        appendLog(QString("✅ LM75A настроен: TH=%1°C, TL=%2°C (команда отправлена)").arg(th).arg(tl));

        QMessageBox::information(this, "Информация",
                                 "Настройки отправлены на устройство.\n"
                                 "Датчик LM75A сконфигурирован.");
    }
}

void MainWindow::on_rgbTestButton_clicked()
{
    if(!serial || !serial->isOpen()) {
        QMessageBox::warning(this, "Предупреждение", "Не подключено к устройству!");
        return;
    }

    sendCommand("rgb_test");
    appendLog("Запущен тест RGB модуля");
}

void MainWindow::on_rgbToggleButton_clicked()
{
    if(!serial || !serial->isOpen()) {
        QMessageBox::warning(this, "Предупреждение", "Не подключено к устройству!");
        return;
    }

    bool isOn = (ui->rgbToggleButton->text() == "Выключить RGB");

    if(isOn) {
        sendCommand("rgb_off");
        ui->rgbSlider->setValue(0);
        ui->rgbToggleButton->setText("Включить RGB");
        appendLog("RGB выключен");
        ui->rgbSlider->setEnabled(false);
        ui->rgbTestButton->setEnabled(false);
    } else {
        // Включаем на последнюю установленную яркость
        int value = ui->rgbSlider->value();
        int rgbValue = (value * 255) / 100;
        QString cmd = QString("rgb_set %1").arg(rgbValue);
        sendCommand(cmd);
        ui->rgbToggleButton->setText("Выключить RGB");
        ui->rgbTestButton->setEnabled(true);
        ui->rgbSlider->setEnabled(true);
        appendLog(QString("RGB включен с яркостью %1%").arg(value));
    }
}

void MainWindow::updateRgbButtonState(bool isOn)
{
    if(isOn) {
        ui->rgbToggleButton->setText("Выключить RGB");
    } else {
        ui->rgbToggleButton->setText("Включить RGB");
    }
}

void MainWindow::on_rgbSlider_valueChanged(int value)
{
    if(!serial || !serial->isOpen()) {
        appendLog(QString("⚠️ Порт не открыт! Яркость %1% не отправлена").arg(value));
        return;
    }

    // Преобразуем проценты (0-100) в значение 0-255
    int rgbValue = (value * 255) / 100;

    QString cmd = QString("rgb_set %1").arg(rgbValue);
    sendCommand(cmd);
}


void MainWindow::on_refreshButton_clicked()
{
    if(serial && serial->isOpen()) {
        sendCommand("temp");
    }
}

void MainWindow::initLogFile()
{
    if (!loggingEnabled) return;

    // Закрываем предыдущий файл если открыт
    if (logFile.isOpen()) {
        logFile.close();
    }

    // Создаем директорию для логов если её нет
    QDir dir("logs");
    if (!dir.exists()) {
        dir.mkdir(".");
    }

    // Генерируем имя файла с датой и временем
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    currentLogFileName = QString("logs/temperature_log_%1.txt").arg(timestamp);

    logFile.setFileName(currentLogFileName);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        writeToLog("ЛОГИРОВАНИЕ ЗАПУЩЕНО");
        writeToLog(QString("Файл лога: %1").arg(currentLogFileName));
        writeToLog(QString("Максимальный размер: %1 МБ").arg(maxLogSize / (1024 * 1024)));
        writeToLog("                                ");
    } else {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать файл лога!");
        loggingEnabled = false;
    }
}

void MainWindow::writeToLog(const QString &message)
{
    if (!loggingEnabled) return;

    // Проверяем размер файла
    if (logFile.isOpen() && logFile.size() >= maxLogSize) {
        archiveLogFile();
    }

    // Переоткрываем файл если он был закрыт
    if (!logFile.isOpen()) {
        logFile.setFileName(currentLogFileName);
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qDebug() << "Не удалось открыть файл лога для записи";
            return;
        }
    }

    QTextStream stream(&logFile);
    stream << message << "\n";
    stream.flush();
}

void MainWindow::archiveLogFile()
{
    if (!logFile.isOpen()) return;
    logFile.close();
    QString oldFileName = currentLogFileName;
    QString baseName = oldFileName;
    baseName.replace(".txt", "");
    QString archiveName = baseName + "_" + QDateTime::currentDateTime().toString("HH-mm-ss") + ".zip";

    // Используем QProcess для скрытого выполнения
    QProcess process;
    QStringList args;

#ifdef Q_OS_WIN
    // Для Windows используем PowerShell в скрытом режиме
    QString psCommand = QString("Compress-Archive -Path \"%1\" -DestinationPath \"%2\" -Force")
                            .arg(QDir::toNativeSeparators(oldFileName))
                            .arg(QDir::toNativeSeparators(archiveName));

    args << "-NoProfile" << "-ExecutionPolicy" << "Bypass" << "-Command" << psCommand;
    process.start("powershell.exe", args);
#else
    // Для Linux используем zip
    args << "-j" << archiveName << oldFileName;
    process.start("zip", args);
#endif

    // Ждем завершения (максимум 5 секунд)
    bool finished = process.waitForFinished(5000);

    if (finished && process.exitCode() == 0) {
        // Проверяем, создался ли архив
        if (QFile::exists(archiveName)) {
            // Удаляем оригинальный файл
            QFile::remove(oldFileName);
            writeToLog(QString("Файл лога заархивирован: %1").arg(archiveName));

            // Создаем новый файл лога
            initLogFile();
        } else {
            writeToLog("ОШИБКА: Архив не был создан!");
            // Продолжаем писать в старый файл
            logFile.setFileName(oldFileName);
            logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        }
    } else {
        QString error = process.readAllStandardError();
        writeToLog(QString("ОШИБКА: Не удалось заархивировать файл лога! %1").arg(error));
        // Продолжаем писать в старый файл
        logFile.setFileName(oldFileName);
        logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
}

void MainWindow::closeLogFile()
{
    if (logFile.isOpen()) {
        writeToLog("ЛОГИРОВАНИЕ ОСТАНОВЛЕНО");
        logFile.close();
    }
}

void MainWindow::DialogLogg(){
    on_startLoggingButton_clicked();
    logg = false;
}

void MainWindow::on_startLoggingButton_clicked()
{
    if(logg){
            return;
    }
    logg = true;
    if (!loggingEnabled) {
        loggingEnabled = true;
        initLogFile();
        ui->startLoggingButton->setText("Остановить запись в лог");
        ui->statusbar->showMessage("Запись в лог запущена");
        appendLog("✅ Запись в лог запущена");
    } else {
        loggingEnabled = false;
        closeLogFile();
        ui->startLoggingButton->setText("Начать запись в лог");
        ui->statusbar->showMessage("Запись в лог остановлена");
        appendLog("⏹️ Запись в лог остановлена");
    }

}

void MainWindow::on_openLogsFolderButton_clicked()
{
    QDir dir("logs");
    if (!dir.exists()) {
        dir.mkdir(".");
    }

    QString logsPath = dir.absolutePath();

#ifdef Q_OS_WIN
    QProcess::startDetached("explorer", QStringList() << QDir::toNativeSeparators(logsPath));
#elif defined(Q_OS_MAC)
    QProcess::startDetached("open", QStringList() << logsPath);
#else
    QProcess::startDetached("xdg-open", QStringList() << logsPath);
#endif

    appendLog(QString("Открыта папка с логами: %1").arg(logsPath));
}

void MainWindow::checkSTM32Driver()
{
    bool stm32Found = false;
    bool hasAnyPorts = QSerialPortInfo::availablePorts().count() > 0;
    bool hasSTMDriver = false;
    bool hasCorrectName = false;
    QString detectedDevice;
    QString actualName;

    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        QString desc = info.description();
        QString manuf = info.manufacturer();
        quint16 vid = info.vendorIdentifier();
        quint16 pid = info.productIdentifier();
        actualName = desc;

        qDebug() << "Port:" << info.portName()
                 << "VID:" << QString::number(vid, 16)
                 << "PID:" << QString::number(pid, 16)
                 << "Desc:" << desc
                 << "Manuf:" << manuf;

        // Проверка на ST-LINK по VID/PID
        if (vid == 0x0483) {
            hasSTMDriver = true;
            stm32Found = true;
            detectedDevice = QString("STM32 (VID=0x%1, PID=0x%2)").arg(vid, 4, 16, QChar('0')).arg(pid, 4, 16, QChar('0'));

            // Проверяем корректность отображения
            if (desc.contains("STMicroelectronics", Qt::CaseInsensitive) ||
                desc.contains("STLink", Qt::CaseInsensitive) ||
                desc.contains("ST-LINK", Qt::CaseInsensitive)) {
                hasCorrectName = true;
            }
            break;
        }
        // Проверка на обычный USB-UART адаптер (CH340, CP2102, FTDI)
        else if (vid != 0) {
            hasSTMDriver = false;
            stm32Found = false;
            detectedDevice = QString("USB-UART адаптер (VID=0x%1, PID=0x%2)").arg(vid, 4, 16, QChar('0')).arg(pid, 4, 16, QChar('0'));
        }
        // Неизвестное устройство
        else if (!desc.isEmpty()) {
            detectedDevice = desc;
        }
    }

    // Если ST-LINK найден, но отображается некорректно
    if (hasSTMDriver && !hasCorrectName && hasAnyPorts) {
        QSettings settings(" ", "TemperatureMonitor");
        QString lastAskKey = "driverAsk_" + QDateTime::currentDateTime().toString("yyyy-MM-dd");
        bool askedToday = settings.value(lastAskKey, false).toBool();

        if (askedToday) {
            QMessageBox msgBox;
            msgBox.setWindowTitle("Внимание! Драйвер отображается некорректно");
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("Обнаружен ST-LINK, но драйвер отображается неправильно!");
            msgBox.setInformativeText(
                QString("Текущее отображение: %1\n\n"
                        "Правильное отображение: STMicroelectronics STLink Virtual COM Port\n\n"
                        "Рекомендуется переустановить драйвер для корректной работы.\n\n"
                        "Нажмите 'Переустановить драйвер' для автоматической установки.")
                    .arg(actualName.isEmpty() ? "Устройство с последовательным интерфейсом USB" : actualName));

            QPushButton *installBtn = msgBox.addButton("Переустановить драйвер", QMessageBox::ActionRole);
            QPushButton *skipBtn = msgBox.addButton("Пропустить", QMessageBox::RejectRole);
            QPushButton *manualBtn = msgBox.addButton("Инструкция", QMessageBox::ActionRole);

            msgBox.setDefaultButton(installBtn);
            msgBox.exec();

            if (msgBox.clickedButton() == installBtn) {
                installDriver();
            } else if (msgBox.clickedButton() == manualBtn) {
                // Показываем инструкцию
                QMessageBox::information(this, "Ручная установка драйвера",
                                         "📋 ИНСТРУКЦИЯ ПО УСТАНОВКЕ ДРАЙВЕРА ST-LINK\n\n"
                                         "1️⃣ Откройте Диспетчер устройств (Win+X → Диспетчер устройств)\n\n"
                                         "2️⃣ Найдите 'Устройство с последовательным интерфейсом USB' в разделе 'Порты (COM и LPT)'\n\n"
                                         "3️⃣ Правой кнопкой → 'Обновить драйвер'\n\n"
                                         "4️⃣ 'Выполнить поиск драйверов на этом компьютере'\n\n"
                                         "5️⃣ 'Выбрать драйвер из списка уже установленных'\n\n"
                                         "6️⃣ Нажмите 'Установить с диска'\n\n"
                                         "7️⃣ Укажите путь к распакованной папке с драйвером\n\n"
                                         "8️⃣ Выберите 'STMicroelectronics Virtual COM Port' и нажмите 'Далее'\n\n");
            }

            settings.setValue(lastAskKey, true);
        }
    }
    // Если устройство найдено, но это не ST-LINK
    else if (hasAnyPorts && !hasSTMDriver && !stm32Found) {
        QSettings settings("YourCompany", "TemperatureMonitor");
        bool driverWarningShown = settings.value("driverWarningShown", false).toBool();

        if (!driverWarningShown) {
            QMessageBox msgBox;
            msgBox.setWindowTitle("Внимание! Неверный драйвер");
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("Обнаружено устройство, но это НЕ ST-LINK!");
            msgBox.setInformativeText(
                QString("Обнаружено: %1\n\n"
                        "Это не драйвер ST-LINK, а стандартный USB-UART драйвер.\n\n"
                        "Для работы с платой STM32 необходимо установить официальный драйвер ST-LINK.\n\n"
                        "Нажмите 'Установить драйвер' для автоматической установки.")
                    .arg(detectedDevice));

            QPushButton *installBtn = msgBox.addButton("Установить драйвер", QMessageBox::ActionRole);
            QPushButton *skipBtn = msgBox.addButton("Пропустить", QMessageBox::RejectRole);

            msgBox.exec();

            if (msgBox.clickedButton() == installBtn) {
                installDriver();
            }

            settings.setValue("driverWarningShown", true);
        }
    }
    // Если ST-LINK найден и отображается корректно
    else if (hasSTMDriver && hasCorrectName) {
        appendLog("✅ Обнаружена плата STM32 с драйвером ST-LINK: " + detectedDevice);
        ui->statusbar->showMessage("✅ ST-LINK драйвер установлен корректно");
    }
    // Если ST-LINK найден, но отображается не идеально (но приемлемо)
    else if (hasSTMDriver) {
        appendLog("✅ Обнаружена плата STM32 с драйвером ST-LINK: " + detectedDevice);
        appendLog("⚠️ Отображение драйвера: " + actualName);
    }
    // Если нет портов
    else if (!hasAnyPorts) {
        appendLog("ℹ️ Нет подключенных COM-портов. Подключите плату STM32.");
    }
}

void MainWindow::installDriver()
{
    // Поиск установщика драйвера
    QStringList searchPaths;
    searchPaths << QCoreApplication::applicationDirPath() + "/install_data/setup.exe";
    searchPaths << QCoreApplication::applicationDirPath() + "/setup.exe";

    QString installerPath;
    foreach(QString path, searchPaths) {
        if (QFile::exists(path)) {
            installerPath = path;
            break;
        }
    }

    if (installerPath.isEmpty()) {
        QMessageBox::critical(this, "Ошибка",
                              "❌ Установщик драйвера не найден!\n\n"
                              "Проверены папки:\n" + searchPaths.join("\n") +
                                  "\n\nПожалуйста, скачайте драйвер ST-LINK вручную\n");
        return;
    }

    appendLog("📦 Найден установщик драйвера: " + installerPath);

    // Проверяем, запущена ли программа от имени администратора
    bool isAdmin = false;
#ifdef Q_OS_WIN
    QProcess whoami;
    whoami.start("whoami", QStringList() << "/groups");
    whoami.waitForFinished();
    QString output = whoami.readAllStandardOutput();
    if (output.contains("S-1-16-12288")) {  // SID для администратора
        isAdmin = true;
    }
#endif

    if (!isAdmin) {
        QMessageBox::warning(this, "Требуются права администратора",
                             "Для установки драйвера требуются права администратора.\n\n"
                             "Пожалуйста, перезапустите программу от имени администратора,\n"
                             ":\n" + installerPath);

        QProcess::startDetached(installerPath);
        return;
    }

    appendLog("🚀 Запуск установщика драйвера...");

    QMessageBox::information(this, "Установка драйвера",
                             "Сейчас запустится мастер установки драйвера ST-LINK.\n\n"
                             "📌 Инструкция:\n"
                             "1. В появившемся окне нажмите 'Далее'\n"
                             "2. Примите лицензионное соглашение\n"
                             "3. Нажмите 'Установить'\n"
                             "4. Если появится предупреждение Windows, выберите 'Установить'\n"
                             "5. После завершения нажмите 'Готово'\n\n");

    // Запускаем установщик
    QStringList args;
    if (installerPath.contains("dpinst", Qt::CaseInsensitive)) {
        args << "/SE" << "/SW";  // Silent mode with UI
    }

    bool started = QProcess::startDetached(installerPath, args);
    if (!started) {
        QMessageBox::critical(this, "Ошибка", "Не удалось запустить установщик:\n" + installerPath);
        appendLog("❌ Не удалось запустить установщик");
    } else {
        appendLog("✅ Установщик запущен");

        // Запоминаем, что установка была запущена
        QSettings settings("YourCompany", "TemperatureMonitor");
        settings.setValue("driverInstallStarted", QDateTime::currentDateTime().toString());

        // Предлагаем перезапустить программу после установки
        QMessageBox::information(this, "Установка драйвера",
                                 "Установщик запущен. Пожалуйста, следуйте инструкциям.\n\n");
    }
}

bool MainWindow::isSTM32Connected()
{
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        qDebug() << "Port:" << info.portName()
        << "VID:" << QString::number(info.vendorIdentifier(), 16)
        << "PID:" << QString::number(info.productIdentifier(), 16)
        << "Desc:" << info.description()
        << "Manuf:" << info.manufacturer()
        << "SystemLocation:" << info.systemLocation();

        // ST-LINK VID/PID
        if (info.vendorIdentifier() == 0x0483) {  // STMicroelectronics
            // ST-LINK может иметь разные PID
            if (info.productIdentifier() == 0x3744 ||  // ST-LINK/V2
                info.productIdentifier() == 0x3748 ||  // ST-LINK/V2-1
                info.productIdentifier() == 0x374b ||  // ST-LINK/V2.1
                info.productIdentifier() == 0x3752) {  // ST-LINK/V3
                return true;
            }
        }

        // Проверка по описанию
        QString desc = info.description().toLower();
        if (desc.contains("stlink") || desc.contains("stm32")) {
            return true;
        }

        // Проверка по manufacturer
        QString manuf = info.manufacturer().toLower();
        if (manuf.contains("stmicroelectronics")) {
            return true;
        }

        // Проверка по системному пути
        if (info.systemLocation().contains("vid_0483", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}
