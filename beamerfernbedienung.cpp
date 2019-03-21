#include "beamerfernbedienung.h"

BeamerFernbedienung::BeamerFernbedienung(QWidget *parent)
    :
        QWidget(parent),
        _beamerConnection(make_unique<QTcpSocket>(this)),
        _ui(new Ui::BeamerFernbedienung()),
        _beamerAddress("192.168.0.38"),
        //_beamerAddress("127.0.0.1"),
        _beamerPort(7000),
        _commands(
            {
                {"avMute", "pic.mute"},
                {"inputSelector", "input"},
                {"powerSwitch", "power"},
                {"lensSelector", "lens.load"}
            }
        ),
        _lensSelectorSlotNames(
            {{
                "Mem 1 - Leinwand oben",
                "Mem 2",
                "Mem 3 - Godi unten",
                "Mem 4 - Godi unten",
                "Mem 5 - Godi oben",
                "Mem 6 - Leinwand unten",
                "Mem 7",
                "Mem 8",
                "Mem 9",
                "Mem 10 - Standard -130 DBI",
            }}
        )
    {
        _ui->setupUi(this);
        setWindowTitle("Beamerfernbedienung");
        establishConnection();
        // Setting up all inputs
        _ui->inputSelector->addItems(
            {
                "HDMI 1",
                "HDMI 2",
                "DVI-D",
                "VGA",
                "Component",
                "HDBaseT"
            }
        );
        //_ui->lensSelector->setEditable(false);
        loadSettings();
        // Setting lens Names
        for(const auto& name : _lensSelectorSlotNames) {
            _ui->lensSelector->addItem(name);
        }
        updateVar();
        updateGui();
    }

void BeamerFernbedienung::updateVar(){
    // Checking current status
    _power = sendCommandGet(_commands["powerSwitch"]).toInt();
    _muted = sendCommandGet(_commands["avMute"]).toInt();
    _inputSelector= sendCommandGet(_commands["inputSelector"]).toInt(); ;
    _lensSelector = sendCommandGet(_commands["lensSelector"]).toInt();;
}

void BeamerFernbedienung::loadSettings() {
    _settings = make_unique<QSettings>();
    _settings->setPath(QSettings::IniFormat, QSettings::UserScope, "settings.ini");

    qDebug() << "read settings: " << _settings->fileName();
    // Read in the Slot names
    int size = _settings->beginReadArray("lensSelectorSlotNames");
    if(_lensSelectorSlotNames.size() < size)
        _lensSelectorSlotNames.resize(size);
    for(int i = 0; i < size; i++) {
        _settings->setArrayIndex(i);
        _lensSelectorSlotNames[i] = _settings->value("Slot").toString();
    }
    _settings->endArray();

    _settings->beginGroup("MainWindow");
    resize(_settings->value("size", QSize(400, 400)).toSize());
    move(_settings->value("pos", QPoint(200, 200)).toPoint());
    _settings->endGroup();



    _settings->beginGroup("Connection");
    //_beamerAddress(_settings->value("IP").toString());
    _beamerPort = (quint16)_settings->value("port").toUInt();
    _settings->endGroup();

}

void BeamerFernbedienung::saveSettings() {
    qint8 counter = 0;
    qDebug() << "write settings: " << _settings->fileName();
    _settings->beginWriteArray("lensSelectorSlotNames");
    for(const auto& name : _lensSelectorSlotNames) {
        _settings->setArrayIndex(counter++);
        if(_settings->value("Slot").toString() != name && name != "")
            _settings->setValue("Slot", name);
    }
    _settings->endArray();


    _settings->beginGroup("MainWindow");
    _settings->setValue("size", size());
    _settings->setValue("pos", pos());
    _settings->endGroup();


    _settings->beginGroup("Connection");
    _settings->setValue("IP", _beamerAddress.toString());
    _settings->setValue("port", _beamerPort);
    _settings->endGroup();

}

void BeamerFernbedienung::establishConnection() {
    _beamerConnection->connectToHost(_beamerAddress, _beamerPort);
    if(_beamerConnection->waitForConnected()) {
        _connected =true;
        QMessageBox::information(
                this,
                tr("Beamerfernbedienung"),
                "Successfully connected to " + full_addr() + "!");
    }
    else {
        QString error = _beamerConnection->errorString();
        _connected = false;
        QMessageBox::critical(
                this,
                tr("Beamerfernbedienung"),
                "Error while connecting to " + full_addr() + "!\n"
                + error);
    }
    if(_connected) {
        _ui->reconnectButton->setEnabled(false);
    }
}

//QString BeamerFernbedienung::readAnswer(){
//    QByteArray buffer;
//    buffer.append(_beamerConnection->readAll());
//    const QString answer = buffer;
//    #ifdef QT_DEBUG
//    qInfo() << "DEBUG: answer  <- " << answer;
//    #endif
//    // const QStringRef executionStatus(&answer,0,3);
//    // if(executionStatus.contains("ACK", Qt::CaseInsensitive))
//    //     qInfo() << "Yay";
//    // else if (executionStatus.contains("NAK", Qt::CaseInsensitive)) {
//    //     qInfo() << "Nayy";
//    // }
//    // QString number = answer.back();
//    // int result = number.toInt();
//    // qInfo() << result << number;
//    return answer;
//}

QString BeamerFernbedienung::sendCommandSet(const QString& cmd, const QString& value) {
    QString pre = "*";
    QString suf = " = " + value + "\r";
    QByteArray data = pre.toUtf8() + cmd.toUtf8() + suf.toUtf8();
    if(_beamerConnection->state() == QAbstractSocket::ConnectedState) {
        // _beamerConnection->write(IntToArray(data.size()));
         _beamerConnection->write(data);
         _beamerConnection->waitForBytesWritten(1000);

         qInfo() << data;
         QByteArray buffer;
         buffer.clear();

         if (_beamerConnection->waitForReadyRead(2000))
         {
             buffer =_beamerConnection->readLine(128);
             qInfo() << buffer;
             lastResponse = buffer;
         }
    }
    #ifdef QT_DEBUG
    qInfo() << "DEBUG: command -> " << cmd;
    #endif

    // for first testig only
    //lastResponse = "ACK *" + cmd +" = " + value + "\r";

    if ((lastResponse.contains("NACK") | (!lastResponse.contains(cmd))))
    {
        qInfo() << "DEBUG: response NACK ->" << lastResponse;
        //Exception??
        return "";
    }


    QRegExp rx("(\\ |\\r)"); //RegEx for ' ' or ',' or '.' or ':' or '\r'
    QStringList query = lastResponse.split(rx);

    foreach(QString s, query)
    {
         qInfo() << "DEBUG: response ACK -> " << s;
    }
    return query[3];
}


QString BeamerFernbedienung::sendCommandGet(const QString& cmd) {
    QString pre = "*";
    QString suf = " ?\r";
    QByteArray data = pre.toUtf8() + cmd.toUtf8() + suf.toUtf8();
    if(_beamerConnection->state() == QAbstractSocket::ConnectedState) {
       // _beamerConnection->write(IntToArray(data.size()));
        _beamerConnection->write(data);
        _beamerConnection->waitForBytesWritten(1000);

        qInfo() << data;
        QByteArray buffer;
        buffer.clear();

        if (_beamerConnection->waitForReadyRead(2000))
        {
            buffer =_beamerConnection->readLine(128);
            qInfo() << buffer;
            lastResponse = buffer;
        }
    }
    #ifdef QT_DEBUG
    qInfo() << "DEBUG: command -> " << cmd;
    #endif

    // for first testig only
    //lastResponse = "ACK *power = 0\r";

    if ((lastResponse.contains("NACK") | (!lastResponse.contains(cmd))))
    {
        qInfo() << "DEBUG: response NACK ->" << lastResponse;
        //Exception??
        return "";
    }


    QRegExp rx("(\\ |\\r)"); //RegEx for ' ' or ',' or '.' or ':' or '\r'
    QStringList query = lastResponse.split(rx);

    foreach(QString s, query)
    {
         qInfo() << "DEBUG: response ACK -> " << s;
    }
    return query[3];
}

void BeamerFernbedienung::updateGui(){
    if(_power) {
        _ui->powerSwitch->setText("Ausschalten");
    }
    else {
        _ui->powerSwitch->setText("Anschalten");
    }

    if (_muted) {
        _ui->avMute->setText("Unmute");
    }else {
        _ui->avMute->setText("Mute");
    }

    _ui->inputSelector->setCurrentIndex(_inputSelector);

    _ui->lensSelector->setCurrentIndex(_lensSelector);

}

void BeamerFernbedienung::on_avMute_clicked() {
    _muted = sendCommandSet(_commands["avMute"], QString::number(!_muted)).toInt();
    updateGui();
}

void BeamerFernbedienung::on_powerSwitch_clicked() {
    _power = sendCommandSet(_commands["powerSwitch"], QString::number(!_power)).toInt();
    updateGui();

}

void BeamerFernbedienung::on_reconnectButton_clicked() {
    saveSettings();
    establishConnection();

    updateVar();
    updateGui();
}

void BeamerFernbedienung::on_inputSelector_activated(int input) {
    _inputSelector = sendCommandSet(_commands["inputSelector"], QString::number(input)).toInt();
}

QString BeamerFernbedienung::full_addr() const {
    return _beamerAddress.toString() + ":" + QString::number(_beamerPort);
}

void BeamerFernbedienung::on_lensSelector_currentTextChanged(const QString& arg1) {
    auto index = _ui->lensSelector->currentIndex();
    _lensSelectorSlotNames[index] = arg1;
}

void BeamerFernbedienung::on_lensSelector_activated(int index) {
   _lensSelector = sendCommandSet(_commands["lensSelector"],QString::number(index)).toInt();

}

BeamerFernbedienung::~BeamerFernbedienung(){
    saveSettings();
    delete _ui;
}
