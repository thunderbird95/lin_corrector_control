#include "corrector_control.h"
#include "ui_corrector_control.h"

#include <QDateTime>
#include <QByteArray>
#include <QMessageBox>
#include <QFileDialog>
#include  <qmath.h>

#include "hex_converter.h"

#define LIN_ERRORS_NUM  5
QString linErrorHeaders[LIN_ERRORS_NUM] = {"PROGRAM ERROR", "CORRECTOR ERROR", "LIN ERROR", "CORRECTOR ERROR", "LIN CONNECTION "};
QString linErrorDescriptions[LIN_ERRORS_NUM] = {"Please contact to developer", "Corrector not sent current values frame", "Lin tranciever loop broken", "Corrector not sent settings", "Lin checksum not correct!"};


correctorControl::correctorControl(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::correctorControl)
{
    ui->setupUi(this);
    connect(ui->connect, &QPushButton::clicked, this, &correctorControl::connectToCom);
    connect(ui->com_reflesh, &QPushButton::clicked, this, &correctorControl::refleshComList);
    //connect(ui->com_list, &QComboBox::currentIndexChanged, this, &correctorControl::listIndexChanged);
    connect(ui->com_list, SIGNAL(currentIndexChanged(int)), this, SLOT(listIndexChanged(int)));
    connect(ui->readFromFlash, SIGNAL(clicked()), this, SLOT(readFromFlash()));
    connect(ui->writeToFlash, SIGNAL(clicked()), this, SLOT(writeToFlash()));

    connect(ui->clear_log, &QPushButton::clicked, ui->log, &QPlainTextEdit::clear);
    connect(ui->openFile, &QPushButton::clicked, this, &correctorControl::openFile);

    connect(ui->correctorsPositionMult, SIGNAL(valueChanged(int)), this, SLOT(changeCorrectorsMult(int)), Qt::QueuedConnection);
    connect(ui->correctorsNum, SIGNAL(valueChanged(int)), this, SLOT(changeCorrectorsNum(int)), Qt::QueuedConnection);
    connect(ui->positionsNum, SIGNAL(valueChanged(int)), this, SLOT(changeCorrectorsPositionsNum(int)), Qt::QueuedConnection);
    //connect(ui->extPositionControl, SIGNAL(toggled(bool)), this, SLOT(changeExtPositionMode(bool)), Qt::QueuedConnection);
    connect(ui->corrector1startPosition, SIGNAL(valueChanged(int)), this, SLOT(readSettingsFromInterface()), Qt::QueuedConnection);
    connect(ui->corrector2startPosition, SIGNAL(valueChanged(int)), this, SLOT(readSettingsFromInterface()), Qt::QueuedConnection);
    connect(ui->corrector1address, SIGNAL(valueChanged(int)), this, SLOT(readSettingsFromInterface()), Qt::QueuedConnection);
    connect(ui->corrector2address, SIGNAL(valueChanged(int)), this, SLOT(readSettingsFromInterface()), Qt::QueuedConnection);
    connect(ui->correctorPositionsTable, SIGNAL(cellChanged(int,int)), this, SLOT(readSettingsFromInterface()), Qt::QueuedConnection);

    connect(ui->readSettings, SIGNAL(pressed()), this, SLOT(readSettingsFromController()));
    connect(ui->writeSettings, SIGNAL(pressed()), this, SLOT(writeSettingsToController()));
    connect(ui->readSettingsFromEeprom, SIGNAL(pressed()), this, SLOT(readSettingsFromEeprom()));
    connect(ui->writeSettingsToEeprom, SIGNAL(pressed()), this, SLOT(writeSettingsToEeprom()));
    connect(ui->clearErrors, SIGNAL(pressed()), this, SLOT(clearErrors()));

    connect(ui->corrector1currentPosition, SIGNAL(valueChanged(int)), this, SLOT(refleshCurrentCorrectorValues()));
    connect(ui->corrector2currentPosition, SIGNAL(valueChanged(int)), this, SLOT(refleshCurrentCorrectorValues()));

    connect(ui->corrector1currentPosition, SIGNAL(sliderReleased()), this, SLOT(readExtValuesFromInterface()));
    connect(ui->corrector2currentPosition, SIGNAL(sliderReleased()), this, SLOT(readExtValuesFromInterface()));
    connect(ui->extPositionControl, SIGNAL(toggled(bool)), this, SLOT(readExtValuesFromInterface()));
    connect(ui->correctorsPositionMult, SIGNAL(valueChanged(int)), this, SLOT(readExtValuesFromInterface()));

    m_tmr.setInterval(10);
    m_tmr.setSingleShot(false);
    //tmr.setTimerType();
    //QObject::connect(&tmr, SIGNAL(timeout()), this, SLOT (readComData()));
    QObject::connect(&m_com, SIGNAL(readyRead()), this, SLOT (readComData()));

    refleshComList();

    ui->progress->setVisible(false);
    ui->labelCurrentProgress->setVisible(false);

    QWidget* widgets_unlocked[] = { ui->writeToFlash, ui->readFromFlash  };
    for (int i = 0; i < sizeof(widgets_unlocked)/sizeof(QWidget*); i++)
        widgets_unlocked[i]->setEnabled(false);

    loadDefaultSettings();
    displaySettings();

    readExtValuesFromInterface();
    ui->tabCurrentControl->setEnabled(false);
}

correctorControl::~correctorControl()
{
    delete ui;
}

void correctorControl::refleshComList()
{
    m_com_list = QSerialPortInfo::availablePorts();
    for (int i = ui->com_list->count(); i >=0; i--)
        ui->com_list->removeItem(i);
    for (int i = 0; i < m_com_list.length(); i++)
        ui->com_list->addItem(m_com_list.at(i).portName());
}

void correctorControl::connectToCom()
{
    QWidget* widgets_locked[] = { ui->com_list, ui->com_reflesh  };
    QWidget* widgets_unlocked[] = { ui->writeToFlash, ui->readFromFlash, ui->tabCurrentControl };
    if (ui->connect->text() == "Connect")   {
        m_com.setPortName(m_com_list.at(ui->com_list->currentIndex()).portName());
        if (m_com.open(QSerialPort::ReadWrite))   {
            m_com.setParity(QSerialPort::NoParity);
            m_com.setDataBits(QSerialPort::Data8);
            m_com.setStopBits(QSerialPort::OneStop);
            m_com.setFlowControl(QSerialPort::NoFlowControl);
            m_com.setBaudRate(19200);
            ui->connect->setText("Disconnect");
            toLog ("COM " + m_com.portName() + " OPENED OK");
            m_comDataPack.clear();
            //tmr.start();
        }
        else
            toLog ("COM " + m_com.portName() + " OPEN ERROR");
    }
    else    {
        m_com.close();
        toLog ("COM " + m_com.portName() + " CLOSED");
        ui->connect->setText("Connect");
        //tmr.stop();
    }
    for (int i = 0; i < sizeof(widgets_locked)/sizeof(QWidget*); i++)
        widgets_locked[i]->setEnabled(!m_com.isOpen());
    for (int i = 0; i < sizeof(widgets_unlocked)/sizeof(QWidget*); i++)
        widgets_unlocked[i]->setEnabled(m_com.isOpen());
}

void correctorControl::listIndexChanged(int index)
{
    if (ui->com_list->currentIndex() >= 0)
        ui->com_list->setToolTip(m_com_list.at(index).description());
    else
        ui->com_list->setToolTip ("NO AVALUABLE COM!");
}

void correctorControl::toLog(const QString &text)
{
    //log = QTime::currentTime().toString("HH:mm:ss") + "\t" + str + "\n" + log;
    //ui->log->setText("LOG\n\n" + log);
    ui->log->appendPlainText(QTime::currentTime().toString("HH:mm:ss") + "\t" + text);
}

void correctorControl::readFromFlash()
{
    uint16_t startAddress = ui->flashStartAddress->value();
    uint16_t endAddress = ui->flashEndAddress->value();
    uint16_t wordsNumber = endAddress + 1 - startAddress;
    uint16_t commandsNumber = wordsNumber / 16;
    ui->progress->setVisible(true);
    ui->centralWidget->setEnabled(false);
    //ui->flashData->clear();
    m_flashData.clear();
    for (uint16_t i = 0; i < commandsNumber; i++)
    {
        ui->progress->setText(QString::number(startAddress) + "/" + QString::number(endAddress) + " (" + QString::number(startAddress * 100 / endAddress) + "%)");
        //tmr.stop();
        m_lastReceivedData.clear();
        m_collectComData = true;
        QByteArray readFrame(6, 0);
        readFrame[0] = 0xE2;
        readFrame[1] = 4;
        readFrame[3] = 0x10;
        readFrame[4] = startAddress & 0xFF;
        readFrame[5] = (startAddress >> 8) & 0xFF;
        readFrame[2] = linChecksum(readFrame);
        m_com.write(readFrame);
        //tmr.start();

        QList<QByteArray> receivedPackets;
        for (int i = 0; i < 100; i++)
        {
            QEventLoop waitLoop;
            QTimer::singleShot(10, &waitLoop, &QEventLoop::quit);
            connect(this, SIGNAL(someLinDataReceived()), &waitLoop, SLOT(quit()));
            waitLoop.exec();

            receivedPackets = linPackets(m_lastReceivedData);
            if (receivedPackets.size() >= 2)
                break;
        }

        m_collectComData = false;

        readFrame.push_back((char)0); // LAST 0 - BYTE WITH CORRECT CHECKSUM FLAG
        if (receivedPackets.size() <= 1)
        {
            if (!receivedPackets.contains(readFrame))
                QMessageBox::warning(this, "LIN ERROR", "Lin can't receive transmitted bytes");
            else
                QMessageBox::warning(this, "CONTROLLER ERROR", "No correct ack from controller, LIN works normally");
            ui->centralWidget->setEnabled(true);
            return;
        }

        else
        {
            if (((uint8_t)(receivedPackets[1][receivedPackets[1].size()-1])) != 0x00)
            {
                QMessageBox::warning(this, "CHECKSUM ERROR", "Lin received frame with uncorrect checksum");
                ui->centralWidget->setEnabled(true);
                return;
            }
            if ((((uint8_t)(receivedPackets[1][3])) != 0x12) || (receivedPackets[1].size() != 39))
            {
                QMessageBox::warning(this, "CONTROLLER ERROR", "Controller sent frame with uncorrect struct");
                ui->centralWidget->setEnabled(true);
                return;
            }
//            QString textData = QString::number(startAddress,16) + ":";
//            for (int i = 0; i < 32; i++)
//                textData = textData + " " + QString::number((uint32_t)(receivedPackets[1][i+5]) & 0xFF, 16) + " ";
//            ui->flashData->appendPlainText(textData);
            receivedPackets[1].resize(receivedPackets[1].size() - 1);
            m_flashData.insert(startAddress, receivedPackets[1].right(32));
        }
        startAddress += 16;
    }
    displayFlashData();
    ui->progress->setVisible(false);
    ui->centralWidget->setEnabled(true);
}



void correctorControl::readComData()
{
    qDebug("Some lin data received");
    QByteArray receivedData = m_com.readAll();

    if (m_collectComData)
        m_lastReceivedData.append(receivedData);

    if (!receivedData.isEmpty())
    {
#if 1
        QString textData;
        for (int i = 0; i < receivedData.size(); i++)
            textData = textData + " " + QString::number((uint32_t)(receivedData[i]) & 0xFF, 16) + " ";
        toLog(textData);
#endif
        //qDebug("1");
        m_comDataPack.append(receivedData);
        for (int i = 0; i < (m_comDataPack.size() - CURRENT_DATA_SIZE + 1); i++)
        {
//            if (m_comDataPack.at(i) == ((int8_t)0xE2))
//                qDebug("1");
//            if (m_comDataPack.at(i+1) == 0x05)
//                qDebug("2");
            if ((m_comDataPack.at(i) != ((int8_t)0xE2)) || (m_comDataPack.at(i+1) != VALUES_FRAME_CODE))
                continue;
            int8_t sum = 0;
            for (int j = 1; j < (CURRENT_DATA_SIZE - 1); j++)
                sum += m_comDataPack[i + j];
            if (sum != m_comDataPack[i + CURRENT_DATA_SIZE - 1])
            {
                toLog("Received current values with uncorrect checksum");
                m_comDataPack = m_comDataPack.mid(i + CURRENT_DATA_SIZE);
                continue;
            }
            displayCurrentValues(m_comDataPack.mid(i + 2, CURRENT_DATA_SIZE - 3));
            m_comDataPack = m_comDataPack.mid(i + CURRENT_DATA_SIZE);
            qDebug("Received lin values");
            m_currentValuesReceived = true;
            emit currentValuesReceived();
        }
    }

    emit someLinDataReceived();
}

uint8_t correctorControl::linChecksum(QByteArray frame)
{
    if (frame.length() < 3)
        return 0;
    uint8_t sum = 0;
    for (int i = 3; i < frame.size(); i++)
        sum = sum + ((uint8_t)(frame[i]));
    return sum;
}

QList<QByteArray> correctorControl::linPackets(QByteArray receivedData)
{
    QList<QByteArray> packets;
    QByteArray currentPacket;
    for (int i = 0; i < receivedData.size(); i++)
    {
        if (currentPacket.isEmpty())
        {
            if (((uint8_t)(receivedData[i])) == 0xE2)
               currentPacket.append(receivedData[i]);
        }
        else if (currentPacket.size() == 1)
        {
            currentPacket.append(receivedData[i]);
            if (((uint8_t)(currentPacket[1])) < 2)
            {
                currentPacket.clear();
                currentPacket.append(0xFF);
                packets.append(currentPacket);
                currentPacket.clear();
            }
        }
        else
        {
            if (currentPacket.size() < (((uint8_t)(currentPacket[1])) + 1))
                currentPacket.append(receivedData[i]);
            else
            {
                currentPacket.append(receivedData[i]);
                currentPacket.push_back((char)0);
                uint8_t checksum = linChecksum(currentPacket);
                if (checksum != ((uint8_t)(currentPacket[2])))
                    currentPacket[currentPacket.size()-1] = 0xFF;
                packets.append(currentPacket);
                currentPacket.clear();
            }
        }
    }
    return packets;
}

void correctorControl::displayFlashData()
{
//    QString textData;
//    foreach(int32_t address, flashData.keys())
//    {
//        textData = textData + QString::number(address, 16) + ":";
//        foreach (char byte, flashData.value(address))
//            textData = textData + " " + QString::number((uint32_t)(byte) & 0xFF, 16) + " ";
//        textData += "\n";
    QString text = displayHexMap(m_flashData);
    ui->flashData->setPlainText(text);
//    }
}

void correctorControl::resizeEvent(QResizeEvent *event)
{
    int x1 = event->oldSize().width();
    int y1 = event->oldSize().height();
    int x2 = event->size().width();
    int y2 = event->size().height();
    if ((x1 <= 0) || (y1 <= 0))
        return;
}

void correctorControl::openFile()
{
    QString hexFileName = QFileDialog::getOpenFileName();
    QFile hexFile(hexFileName);
    if (!hexFile.open(QFile::ReadOnly))
    {
        QMessageBox::warning(this, "Open file error", QString("File %1 cant open").arg(hexFileName));
        return;
    }
    QString hexFileData = QString(hexFile.readAll());
    QMap<int32_t,QByteArray> dataFromFile = hexFileToMap(hexFileData);
    if (dataFromFile.isEmpty())
    {
        QMessageBox::warning(this, "File error", QString("File %1 is empty").arg(hexFileName));
        return;
    }
    if (dataFromFile.keys().contains(-1))
    {
        QMessageBox::warning(this, "File error", dataFromFile.value(-1));
        return;
    }
    m_flashData = resizeMap(dataFromFile);
//    flashData.clear();
//    foreach(int32_t address, dataFromFile.keys())
//        flashData.insert(address / 2, dataFromFile.value(address));
    displayFlashData();
}

void correctorControl::writeToFlash()
{
    ui->progress->setVisible(true);
    ui->centralWidget->setEnabled(false);
    //ui->flashData->clear();
    //flashData.clear();
    int counter = 0;
    int keysNum = m_flashData.keys().length();
    foreach (int32_t address, m_flashData.keys())
    {
        ui->progress->setText(QString::number(counter) + "/" + QString::number(keysNum) + " (" + QString::number(counter * 100 / keysNum) + "%)");
        counter++;
        m_lastReceivedData.clear();
        m_collectComData = true;

        QByteArray writeFrame(6, 0);
        writeFrame[0] = 0xE2;
        writeFrame[1] = 36;
        writeFrame[3] = 0x20;
        writeFrame[4] = address & 0xFF;
        writeFrame[5] = (address >> 8) & 0xFF;
        writeFrame.append(m_flashData.value(address));
        writeFrame[2] = linChecksum(writeFrame);
        m_com.write(writeFrame);

        QList<QByteArray> receivedPackets;
        for (int i = 0; i < 100; i++)
        {
            QEventLoop waitLoop;
            QTimer::singleShot(10, &waitLoop, &QEventLoop::quit);
            connect(this, SIGNAL(someLinDataReceived()), &waitLoop, SLOT(quit()));
            waitLoop.exec();

            receivedPackets = linPackets(m_lastReceivedData);
            if (receivedPackets.size() >= 2)
                break;
        }

        m_collectComData = false;

        writeFrame.push_back((char)0); // LAST 0 - BYTE WITH CORRECT CHECKSUM FLAG
        if (receivedPackets.size() <= 1)
        {
            if (!receivedPackets.contains(writeFrame))
                QMessageBox::warning(this, "LIN ERROR", "Lin can't receive transmitted bytes");
            else
                QMessageBox::warning(this, "CONTROLLER ERROR", "No correct ack from controller, LIN works normally");
            ui->centralWidget->setEnabled(true);
            return;
        }

        else
        {
            if (((uint8_t)(receivedPackets[1][receivedPackets[1].size()-1])) != 0x00)
            {
                QMessageBox::warning(this, "CHECKSUM ERROR", "Lin received frame with uncorrect checksum");
                ui->centralWidget->setEnabled(true);
                return;
            }
            if ((((uint8_t)(receivedPackets[1][3])) != 0x22) || (receivedPackets[1].size() != 5))
            {
                QMessageBox::warning(this, "CONTROLLER ERROR", "Controller sent frame with uncorrect struct");
                ui->centralWidget->setEnabled(true);
                return;
            }
            receivedPackets[1].resize(receivedPackets[1].size() - 1);
            // PACKET GOOD
        }
    }

    ui->progress->setVisible(false);
    ui->centralWidget->setEnabled(true);
}

void correctorControl::changeCorrectorsMult(int mult)
{
    ui->corrector1startPosition->setSingleStep(mult);
    ui->corrector2startPosition->setSingleStep(mult);
    ui->corrector1startPosition->setMaximum(mult * 255);
    ui->corrector2startPosition->setMaximum(mult * 255);
    readSettingsFromInterface();
}

void correctorControl::changeCorrectorsNum(int num)
{
    if (num > (ui->correctorPositionsTable->columnCount() - 1))
    {
        ui->correctorPositionsTable->setColumnCount(num + 1);
        for (int i = 0; i < ui->correctorPositionsTable->rowCount(); i++)
            ui->correctorPositionsTable->setItem(i, num, new QTableWidgetItem(ui->correctorPositionsTable->item(i, 1)->text()));
    }
    else
        ui->correctorPositionsTable->setColumnCount(num + 1);
    readSettingsFromInterface();
}

void correctorControl::changeCorrectorsPositionsNum(int num)
{
    if (num > (ui->correctorPositionsTable->rowCount()))
    {
        ui->correctorPositionsTable->setRowCount(num);
        ui->correctorPositionsTable->setItem(num - 2, 0, new QTableWidgetItem(ui->correctorPositionsTable->item(num - 3, 0)->text()));
        for (int i = 1; i < ui->correctorPositionsTable->columnCount(); i++)
            ui->correctorPositionsTable->setItem(num - 1, i, new QTableWidgetItem(ui->correctorPositionsTable->item(num - 2, i)->text()));
    }
    else
        ui->correctorPositionsTable->setRowCount(num);
    readSettingsFromInterface();
}

void correctorControl::changeExtPositionMode(bool isExtPositionChecked)
{
    ui->corrector1currentPosition->setEnabled(isExtPositionChecked);
    ui->corrector2currentPosition->setEnabled(isExtPositionChecked);
}

void correctorControl::displaySettings()
{
    disconnect(ui->correctorsPositionMult, SIGNAL(valueChanged(int)), this, SLOT(changeCorrectorsMult(int)));
    disconnect(ui->correctorsNum, SIGNAL(valueChanged(int)), this, SLOT(changeCorrectorsNum(int)));
    disconnect(ui->positionsNum, SIGNAL(valueChanged(int)), this, SLOT(changeCorrectorsPositionsNum(int)));
    disconnect(ui->extPositionControl, SIGNAL(toggled(bool)), this, SLOT(changeExtPositionMode(bool)));
    disconnect(ui->corrector1startPosition, SIGNAL(valueChanged(int)), this, SLOT(readSettingsFromInterface()));
    disconnect(ui->corrector2startPosition, SIGNAL(valueChanged(int)), this, SLOT(readSettingsFromInterface()));
    disconnect(ui->correctorPositionsTable, SIGNAL(cellChanged(int,int)), this, SLOT(readSettingsFromInterface()));

    ui->correctorsNum->setValue(m_settings[0]);
    ui->positionsNum->setValue(m_settings[1]);
    ui->correctorsPositionMult->setValue((static_cast<uint8_t>(m_settings.at(2))));
    ui->corrector1address->setValue((static_cast<uint8_t>(m_settings.at(3))));
    ui->corrector2address->setValue((static_cast<uint8_t>(m_settings.at(4))));
    ui->corrector1startPosition->setValue((static_cast<int8_t>(m_settings.at(5))) * ui->correctorsPositionMult->value());
    ui->corrector2startPosition->setValue((static_cast<int8_t>(m_settings.at(6))) * ui->correctorsPositionMult->value());

    ui->correctorPositionsTable->setRowCount(ui->positionsNum->value());
    ui->correctorPositionsTable->setColumnCount(1 + ui->correctorsNum->value());

    for (int i = 0; i < ui->correctorPositionsTable->rowCount(); i++)
    {
        if (i != (ui->correctorPositionsTable->rowCount() - 1))
            ui->correctorPositionsTable->setItem(i, 0, new QTableWidgetItem(QString::number(static_cast<uint8_t>(m_settings[i + 7]))));
        for (int j = 1; j < ui->correctorPositionsTable->columnCount(); j++)
            ui->correctorPositionsTable->setItem(i, j,
                new QTableWidgetItem(QString::number((static_cast<int8_t>(m_settings[i + 6 + j * 16])) * ui->correctorsPositionMult->value())));
    }

    connect(ui->correctorsPositionMult, SIGNAL(valueChanged(int)), this, SLOT(changeCorrectorsMult(int)), Qt::QueuedConnection);
    connect(ui->correctorsNum, SIGNAL(valueChanged(int)), this, SLOT(changeCorrectorsNum(int)), Qt::QueuedConnection);
    connect(ui->positionsNum, SIGNAL(valueChanged(int)), this, SLOT(changeCorrectorsPositionsNum(int)), Qt::QueuedConnection);
    connect(ui->extPositionControl, SIGNAL(toggled(bool)), this, SLOT(changeExtPositionMode(bool)), Qt::QueuedConnection);
    connect(ui->corrector1startPosition, SIGNAL(valueChanged(int)), this, SLOT(readSettingsFromInterface()), Qt::QueuedConnection);
    connect(ui->corrector2startPosition, SIGNAL(valueChanged(int)), this, SLOT(readSettingsFromInterface()), Qt::QueuedConnection);
    connect(ui->correctorPositionsTable, SIGNAL(cellChanged(int,int)), this, SLOT(readSettingsFromInterface()), Qt::QueuedConnection);
}

QString correctorControl::checkSettings(QByteArray settings)
{
    QString errors;
    if (settings.size() != (SETTINGS_DATA_SIZE - 3))
        return QString("SETTINGS SIZE NOT CORRECT");
    if ((settings.at(0) > 2) || (settings.at(0) < 0))
        errors += QString("NOT MORE 2 CORRECTORS SUPPORTED (VALUE FROM SETTINGS - %1)\n").arg(static_cast<uint8_t>(settings.at(0)));
    if ((settings.at(1) > 16) || (settings.at(1) < 2))
        errors += QString("POSITIONS NUM CAN BE 2 - 16 (VALUE FROM SETTINGS - %1)\n").arg(static_cast<uint8_t>(settings.at(1)));
    if (settings.at(2) == 0)
        errors += QString("POSITION MULT CANNOT BE ZERO\n");
//    if ((settings.at(3) > 16) || (settings.at(3) < 2))
//        errors += QString("POSITIONS NUM CAN BE 2 - 16 (VALUE FROM SETTINGS - %1)\n").arg(settings.at(1));
    return errors;
}

void correctorControl::readSettingsFromInterface()
{
//    uint8_t correctorsNum;
//    uint8_t positionsNum;
//    uint8_t correctorPositionMult;
//    uint8_t correctorsAdresses[2];
//    uint8_t correctorStartPositions[2];
//    uint8_t adcValues[16-1];
//    uint8_t correctorsValues[2][16];

    m_settings.resize(54);
    m_settings.fill(0);
    m_settings[0] = ui->correctorsNum->value();
    m_settings[1] = ui->positionsNum->value();
    m_settings[2] = ui->correctorsPositionMult->value();
    m_settings[3] = ui->corrector1address->value();
    m_settings[4] = ui->corrector2address->value();
    m_settings[5] = calcDiv(ui->corrector1startPosition->value(), ui->correctorsPositionMult->value());
    m_settings[6] = calcDiv(ui->corrector2startPosition->value(), ui->correctorsPositionMult->value());
    for (int i = 0; i < ui->correctorPositionsTable->rowCount(); i++)
    {
        if (i != (ui->correctorPositionsTable->rowCount() - 1))
            m_settings[i + 7] = ui->correctorPositionsTable->item(i, 0)->text().toInt();
        for (int j = 0; j < (ui->correctorPositionsTable->columnCount() - 1); j++)
            m_settings[i + 22 + j * 16] = calcDiv(ui->correctorPositionsTable->item(i, j + 1)->text().toInt(), ui->correctorsPositionMult->value());
    }

    displaySettings();
}

void correctorControl::loadDefaultSettings()
{
    m_settings.resize(54);
    m_settings.fill(255);
    m_settings[0] = 2;
    m_settings[1] = 2;
    m_settings[2] = 50;
    m_settings[3] = 0xF8;
    m_settings[4] = 0xF0;
    m_settings[5] = 0;
    m_settings[6] = 0;
}

int correctorControl::calcDiv(int num, int div)
{
    int divident = qRound(((qreal)num) / ((qreal)div));
    return divident;
}

void correctorControl::displayCurrentValues(QByteArray packet)
{
//    uint8_t temperature;
//    uint8_t adcPositionValue;
//    uint8_t positionIndex;
//    uint16_t rdCorrectorValues[2];
//    uint16_t wrCorrectorValues[2];
//    //uint16_t extCorrectorValues[2];
//    currentFlags_t flags;
//    internalErrorFlags_t internalErrors;
//    motorErrorFlags_t motorErrors[2];
//    uint8_t counter;
//    uint8_t displayed_error;

    uint8_t temperature = static_cast<uint8_t>(packet.at(0));
    uint8_t adcValue = static_cast<uint8_t>(packet.at(1));
    uint8_t positionIndex = static_cast<uint8_t>(packet.at(2));
//    int readCorrector1value = (static_cast<uint8_t>(packet.at(4))) * 256 + (static_cast<uint8_t>(packet.at(3))) * 1;
//    int readCorrector2value = (static_cast<uint8_t>(packet.at(6))) * 256 + (static_cast<uint8_t>(packet.at(5))) * 1;
//    int writingCorrector1value = (static_cast<uint8_t>(packet.at(8))) * 256 + (static_cast<uint8_t>(packet.at(7))) * 1;
//    int writingCorrector2value = (static_cast<uint8_t>(packet.at(10))) * 256 + (static_cast<uint8_t>(packet.at(9))) * 1;
    int16_t readCorrector1value = (packet.at(4) << 8) | (packet.at(3) & 0xFF);
    int16_t readCorrector2value = (packet.at(6) << 8) | (packet.at(5) & 0xFF);
    int16_t writingCorrector1value = (packet.at(8) << 8) | (packet.at(7) & 0xFF);
    int16_t writingCorrector2value = (packet.at(10) << 8) | (packet.at(9) & 0xFF);
    uint8_t currentFlags = static_cast<uint8_t>(packet.at(11));
    uint8_t internalErrors = static_cast<uint8_t>(packet.at(12));
    uint8_t motorErrors[2];
    motorErrors[0] = static_cast<uint8_t>(packet.at(13));
    motorErrors[1] = static_cast<uint8_t>(packet.at(14));
    //uint8_t counter = static_cast<uint8_t>(packet.at(15));
    uint8_t displayedError = static_cast<uint8_t>(packet.at(15));

    QString report = QTime::currentTime().toString("hh:mm:ss.zzz") + "\n";
    report += ("Temperature: " + QString::number((int)temperature) + "\n");
    report += ("Adc value: " + QString::number((int)adcValue) + "\n");
    report += ("Position index: " + QString::number((int)positionIndex) + "\n");
    report += ("Real corrector 1 value: " + QString::number((int)readCorrector1value) + "\n");
    report += ("Real corrector 2 value: " + QString::number((int)readCorrector2value) + "\n");
    report += ("Written corrector 1 value: " + QString::number((int)writingCorrector1value) + "\n");
    report += ("Written corrector 2 value: " + QString::number((int)writingCorrector2value) + "\n");

    report += ("Flags (" + QString::number((int)currentFlags) + "): ");
    if (currentFlags & 0x01)
        report += "C1 INIT OK ";
    else
        report += "C1 NOT INITED ";
    if (currentFlags & 0x02)
        report += "C2 INIT OK ";
    else
        report += "C2 NOT INITED ";
    if (currentFlags & 0x08)
        report += "EXT VALUES ";
    report += "\n";

    report += ("Internal errors (" + QString::number((int)internalErrors) + "): ");
    if (internalErrors == 0)
        report += "NONE";
    if ((internalErrors & 0x01) && ((internalErrors & 0x02) == 0))
        report += "SETTINGS ERROR ";
    if (internalErrors & 0x02)
        report += "SETTINGS EMPTY ";
    if (internalErrors & 0x04)
        report += "ADC VALUE UNCORRECT ";
    if (internalErrors & 0x08)
        report += "LIN ERROR ";
    report += "\n";

    for (int i = 0; i < 2; i++)
    {
        report += ("Motor " + QString::number(i) + " errors (" + QString::number((int)motorErrors[i]) + "): ");
        if (motorErrors[i] == 0)
            report += "NONE";
        if (motorErrors[i] & 0x01)
            report += "LIN_TXRX_INIT ";
        if (motorErrors[i] & 0x02)
            report += "NO_ACK_INIT ";
        if (motorErrors[i] & 0x04)
            report += "CHECKSUM_ERROR ";
        if (motorErrors[i] & 0x08)
            report += "LIN_TXRX_PROCESSING ";
        if (motorErrors[i] & 0x10)
            report += "NO_ACK_PROCESSING ";
        if (motorErrors[i] & 0x20)
            report += "BAD_CONNECTION_PROCESSING ";
        if (motorErrors[i] & 0x40)
            report += "LIN_TXRX_SET ";
//        if (motorErrors[i] & 0x80)
//            report += "LIN_TXRX_PROCESSING ";
        report += "\n";
    }

    //report += ("Counter: " + QString::number((int)counter) + "\n");
    report += ("Displayed error: " + QString::number((int)displayedError));

    ui->currentInformation->setPlainText(report);
}

void correctorControl::readSettingsFromController()
{
    ui->labelCurrentProgress->setVisible(true);
    ui->centralWidget->setEnabled(false);

    QByteArray readSettingsFrame(11, 0);
    readSettingsFrame[0] = 0xE2;
    readSettingsFrame[1] = 0x12;

    QByteArray ackFrame = sendFrameAndWaitAck(readSettingsFrame, SETTINGS_FRAME_CODE, SETTINGS_DATA_SIZE);

    if (ackFrame.size() != SETTINGS_DATA_SIZE)
        ackFrame = QByteArray(1, 0);
    if (ackFrame.size() == 1)
    {
        if ((ackFrame.at(0) >= LIN_ERRORS_NUM) || (ackFrame.at(0) < 0))
            ackFrame[0] = 0;
        ui->labelCurrentProgress->setVisible(false);
        ui->centralWidget->setEnabled(true);
        QMessageBox::warning(this, linErrorHeaders[ackFrame.at(0)], linErrorDescriptions[ackFrame.at(0)]);
        return;
    }

    QByteArray settingsValues = ackFrame.mid(2, SETTINGS_DATA_SIZE - 3);

    QString errorsInSettings = checkSettings(settingsValues);
    if (!errorsInSettings.isEmpty())
    {
        ui->labelCurrentProgress->setVisible(false);
        ui->centralWidget->setEnabled(true);
        QMessageBox::warning(this, "RECEIVED SETTINGS ERROR", errorsInSettings);
        return;
    }

    m_settings = settingsValues;
    displaySettings();
    QMessageBox::information(this, "SETTINGS RECEIVED", "Received settings OK!");

    ui->labelCurrentProgress->setVisible(false);
    ui->centralWidget->setEnabled(true);
}

void correctorControl::writeSettingsToController()
{
    if (m_settings.size() != (SETTINGS_DATA_SIZE - 3))
        return;

    int framesNum = (m_settings.size() + 7) / 8;
    ui->labelCurrentProgress->setVisible(true);
    ui->centralWidget->setEnabled(false);

    for (int dataCounter = 0; dataCounter < framesNum; dataCounter++)
    {
        QByteArray writeSettingsFrame(2, 0);
        writeSettingsFrame[0] = 0xE2;
        writeSettingsFrame[1] = dataCounter;
        writeSettingsFrame.append(m_settings.mid(dataCounter * 8, 8));
        writeSettingsFrame.append(QByteArray(11 - writeSettingsFrame.size(), 0));

        QByteArray ackFrame = sendFrameAndWaitAck(writeSettingsFrame, ACK_FRAME_CODE, ACK_FRAME_SIZE);

        if (ackFrame.size() != ACK_FRAME_SIZE)
            ackFrame = QByteArray(1, 0);
        if (ackFrame.size() == 1)
        {
            if ((ackFrame.at(0) >= LIN_ERRORS_NUM) || (ackFrame.at(0) < 0))
                ackFrame[0] = 0;
            ui->labelCurrentProgress->setVisible(false);
            ui->centralWidget->setEnabled(true);
            QMessageBox::warning(this, linErrorHeaders[ackFrame.at(0)], linErrorDescriptions[ackFrame.at(0)]);
            return;
        }

        if (ackFrame.at(2) != 0)
        {
            ui->labelCurrentProgress->setVisible(false);
            ui->centralWidget->setEnabled(true);
            QMessageBox::warning(this, "TRANSMISSION SETTINGS ERROR", QString("Controller sent error code %1").arg(static_cast<uint8_t>(ackFrame.at(2))));
            return;
        }
    }

    QMessageBox::information(this, "SETTINGS SENDED", "Sending settings OK!");

    ui->labelCurrentProgress->setVisible(false);
    ui->centralWidget->setEnabled(true);
}

void correctorControl::readSettingsFromEeprom()
{
    ui->labelCurrentProgress->setVisible(true);
    ui->centralWidget->setEnabled(false);

    QByteArray readFromEepromFrame(11, 0);
    readFromEepromFrame[0] = 0xE2;
    readFromEepromFrame[1] = 0x11;

    QByteArray ackFrame = sendFrameAndWaitAck(readFromEepromFrame, ACK_FRAME_CODE, ACK_FRAME_SIZE);

    if (ackFrame.size() != ACK_FRAME_SIZE)
        ackFrame = QByteArray(1, 0);
    if (ackFrame.size() == 1)
    {
        if ((ackFrame.at(0) >= LIN_ERRORS_NUM) || (ackFrame.at(0) < 0))
            ackFrame[0] = 0;
        ui->labelCurrentProgress->setVisible(false);
        ui->centralWidget->setEnabled(true);
        QMessageBox::warning(this, linErrorHeaders[ackFrame.at(0)], linErrorDescriptions[ackFrame.at(0)]);
        return;
    }

    if (ackFrame.at(2) != 0)
    {
        ui->labelCurrentProgress->setVisible(false);
        ui->centralWidget->setEnabled(true);
        QMessageBox::warning(this, "READ FROM EEPROM ERROR", QString("Controller sent error code %1").arg(static_cast<uint8_t>(ackFrame.at(2))));
        return;
    }

    ui->labelCurrentProgress->setVisible(false);
    ui->centralWidget->setEnabled(true);
}

void correctorControl::writeSettingsToEeprom()
{
    ui->labelCurrentProgress->setVisible(true);
    ui->centralWidget->setEnabled(false);

    QByteArray writeToEepromFrame(11, 0);
    writeToEepromFrame[0] = 0xE2;
    writeToEepromFrame[1] = 0x10;

    QByteArray ackFrame = sendFrameAndWaitAck(writeToEepromFrame, ACK_FRAME_CODE, ACK_FRAME_SIZE);

    if (ackFrame.size() != ACK_FRAME_SIZE)
        ackFrame = QByteArray(1, 0);
    if (ackFrame.size() == 1)
    {
        if ((ackFrame.at(0) >= LIN_ERRORS_NUM) || (ackFrame.at(0) < 0))
            ackFrame[0] = 0;
        ui->labelCurrentProgress->setVisible(false);
        ui->centralWidget->setEnabled(true);
        QMessageBox::warning(this, linErrorHeaders[ackFrame.at(0)], linErrorDescriptions[ackFrame.at(0)]);
        return;
    }

    if (ackFrame.at(2) != 0)
    {
        ui->labelCurrentProgress->setVisible(false);
        ui->centralWidget->setEnabled(true);
        QMessageBox::warning(this, "WRITE TO EEPROM ERROR", QString("Controller sent error code %1").arg(static_cast<uint8_t>(ackFrame.at(2))));
        return;
    }

    ui->labelCurrentProgress->setVisible(false);
    ui->centralWidget->setEnabled(true);
}

void correctorControl::clearErrors()
{
    ui->labelCurrentProgress->setVisible(true);
    ui->centralWidget->setEnabled(false);

    QByteArray clearErrorsFrame(11, 0);
    clearErrorsFrame[0] = 0xE2;
    clearErrorsFrame[1] = 0x18;

    QByteArray ackFrame = sendFrameAndWaitAck(clearErrorsFrame, ACK_FRAME_CODE, ACK_FRAME_SIZE);

    if (ackFrame.size() != ACK_FRAME_SIZE)
        ackFrame = QByteArray(1, 0);
    if (ackFrame.size() == 1)
    {
        if ((ackFrame.at(0) >= LIN_ERRORS_NUM) || (ackFrame.at(0) < 0))
            ackFrame[0] = 0;
        ui->labelCurrentProgress->setVisible(false);
        ui->centralWidget->setEnabled(true);
        QMessageBox::warning(this, linErrorHeaders[ackFrame.at(0)], linErrorDescriptions[ackFrame.at(0)]);
        return;
    }

    if (ackFrame.at(2) != 0)
    {
        ui->labelCurrentProgress->setVisible(false);
        ui->centralWidget->setEnabled(true);
        QMessageBox::warning(this, "CLEAR ERRORS COMMAND ERROR", QString("Controller sent error code %1").arg(static_cast<uint8_t>(ackFrame.at(2))));
        return;
    }

    ui->labelCurrentProgress->setVisible(false);
    ui->centralWidget->setEnabled(true);
}

void correctorControl::refleshCurrentCorrectorValues()
{
    if (!ui->extPositionControl->isChecked())
    {
        ui->corrector1currentPosition->setEnabled(false);
        ui->corrector2currentPosition->setEnabled(false);
    }

    int16_t correctorValues[2];
    correctorValues[0] = ui->correctorsPositionMult->value() * ui->corrector1currentPosition->value();
    correctorValues[1] = ui->correctorsPositionMult->value() * ui->corrector2currentPosition->value();

    ui->corrector1positionLabel->setText("Corrector 1: " + QString::number(correctorValues[0]));
    ui->corrector2positionLabel->setText("Corrector 2: " + QString::number(correctorValues[1]));
}

void correctorControl::readExtValuesFromInterface()
{
    if (!ui->extPositionControl->isChecked())
    {
        ui->corrector1currentPosition->setEnabled(false);
        ui->corrector2currentPosition->setEnabled(false);
    }

    int16_t correctorValues[2];
    correctorValues[0] = ui->correctorsPositionMult->value() * ui->corrector1currentPosition->value();
    correctorValues[1] = ui->correctorsPositionMult->value() * ui->corrector2currentPosition->value();

    ui->corrector1positionLabel->setText("Corrector 1: " + QString::number(correctorValues[0]));
    ui->corrector2positionLabel->setText("Corrector 2: " + QString::number(correctorValues[1]));

    if (!m_com.isOpen())
        return;

    QByteArray sendCurrentValuesFrame(11, 0);
    sendCurrentValuesFrame[0] = 0xE2;
    sendCurrentValuesFrame[1] = 0x17;
    sendCurrentValuesFrame[2] = (correctorValues[0] >> 0) & 0xFF;
    sendCurrentValuesFrame[3] = (correctorValues[0] >> 8) & 0xFF;
    sendCurrentValuesFrame[4] = (correctorValues[1] >> 0) & 0xFF;
    sendCurrentValuesFrame[5] = (correctorValues[1] >> 8) & 0xFF;
    sendCurrentValuesFrame[6] = (ui->extPositionControl->isChecked() ? 1 : 0);

    ui->labelCurrentProgress->setVisible(true);
    ui->centralWidget->setEnabled(false);

    QByteArray ackFrame = sendFrameAndWaitAck(sendCurrentValuesFrame, ACK_FRAME_CODE, ACK_FRAME_SIZE);

    if (ackFrame.size() != ACK_FRAME_SIZE)
        ackFrame = QByteArray(1, 0);
    if (ackFrame.size() == 1)
    {
        if ((ackFrame.at(0) >= LIN_ERRORS_NUM) || (ackFrame.at(0) < 0))
            ackFrame[0] = 0;
        ui->labelCurrentProgress->setVisible(false);
        ui->centralWidget->setEnabled(true);
        QMessageBox::warning(this, linErrorHeaders[ackFrame.at(0)], linErrorDescriptions[ackFrame.at(0)]);
        return;
    }

    if (ackFrame.at(2) != 0)
    {
        ui->labelCurrentProgress->setVisible(false);
        ui->centralWidget->setEnabled(true);
        QMessageBox::warning(this, "SEND EXT POSITIONS COMMAND ERROR", QString("Controller sent error code %1").arg(static_cast<uint8_t>(ackFrame.at(2))));
        return;
    }

    ui->labelCurrentProgress->setVisible(false);
    ui->centralWidget->setEnabled(true);
}

QByteArray correctorControl::sendFrameAndWaitAck(QByteArray frameToSend, int receivedFrameCode, int receivedFrameSize, QString waitState)
{
    if (frameToSend.size() != 11)
        return QByteArray(1, 0);

    QEventLoop waitLoop;
    QTimer tmr;
    tmr.setSingleShot(false);
    connect(&tmr, SIGNAL(timeout()), &waitLoop, SLOT(quit()));
    connect(&tmr, SIGNAL(timeout()), this, SLOT(tmrTimeout()));

    ui->labelCurrentProgress->setText("Waiting curValues frame...");
    m_currentValuesReceived = false;
    connect(this, SIGNAL(currentValuesReceived()), &waitLoop, SLOT(quit()), Qt::QueuedConnection);
    tmr.setInterval(2000);
    tmr.start();
    qDebug("Wait lin values");
    waitLoop.exec();
    tmr.stop();
    qDebug("Timeout or lin values event emitted");
    if (!m_currentValuesReceived)
        return QByteArray(1, 1);

    disconnect(this, SIGNAL(currentValuesReceived()), &waitLoop, SLOT(quit()));

    ui->labelCurrentProgress->setText(waitState);
    frameToSend[10] = 0;
    for (int i = 1; i < 10; i++)
        frameToSend[10] = frameToSend[10] + frameToSend[i];

    m_lastReceivedData.clear();
    m_collectComData = true;
    connect(this, SIGNAL(someLinDataReceived()), &waitLoop, SLOT(quit()), Qt::QueuedConnection);
    QByteArray ackFrameHeader(2, 0xE2);
    ackFrameHeader[1] = receivedFrameCode;
    m_com.write(frameToSend);
    tmr.setInterval(200);
    tmr.start();
    int ackFrameAddress = 0;
    for (int i = 0; i < 20; i++)
    {
        waitLoop.exec();
        if (!m_lastReceivedData.contains(frameToSend))
            continue;
        ackFrameAddress = m_lastReceivedData.indexOf(ackFrameHeader);
        if ((ackFrameAddress >= frameToSend.size()) && ((ackFrameAddress + receivedFrameSize) <= m_lastReceivedData.size()))
            break;
    }
    tmr.stop();
    disconnect(this, SIGNAL(someLinDataReceived()), &waitLoop, SLOT(quit()));
    m_collectComData = false;

    if (!m_lastReceivedData.contains(frameToSend))
        return QByteArray(1, 2);
    if ((ackFrameAddress < frameToSend.size()) || ((ackFrameAddress + receivedFrameSize) > m_lastReceivedData.size()))
        return QByteArray(1, 3);
    QByteArray ackFrame = m_lastReceivedData.mid(ackFrameAddress, receivedFrameSize);
    int8_t sum = 0;
    for (int j = 1; j < (receivedFrameSize - 1); j++)
        sum += ackFrame[j];
    if (sum != ackFrame[receivedFrameSize - 1])
        return QByteArray(1, 4);
    return ackFrame;
}

void correctorControl::tmrTimeout()
{
    qDebug("Tmr timeout event occured!");
}
