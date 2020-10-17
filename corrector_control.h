#ifndef CORRECTOR_CONTROL_H
#define CORRECTOR_CONTROL_H

#include <QMainWindow>

#include <QtSerialPort/qserialport.h>
#include <QtSerialPort/qserialportinfo.h>

#include <QTimer>

#include <QMap>

#define CURRENT_DATA_SIZE   (16 + 3)
#define SETTINGS_DATA_SIZE  (54 + 3)
#define ACK_FRAME_SIZE      (1 + 3)

#define ACK_FRAME_CODE      0x25
#define VALUES_FRAME_CODE   0x35
#define SETTINGS_FRAME_CODE 0x15

namespace Ui {
class correctorControl;
}

class correctorControl : public QMainWindow
{
    Q_OBJECT

public:
    explicit correctorControl(QWidget *parent = 0);
    ~correctorControl();

signals:

    void someLinDataReceived();

    void currentValuesReceived();

private slots:

    void refleshComList();

    void connectToCom();

    void listIndexChanged(int index);

    void toLog(const QString& text);

    void readFromFlash();

    void readComData();

    void openFile();

    void writeToFlash();

    void changeCorrectorsMult(int mult);

    void changeCorrectorsNum(int num);

    void changeCorrectorsPositionsNum(int num);

    void changeExtPositionMode(bool isExtPositionChecked);

    void readSettingsFromInterface();

    void readSettingsFromController();

    void writeSettingsToController();

    void readSettingsFromEeprom();

    void writeSettingsToEeprom();

    void refleshCurrentCorrectorValues();

    void readExtValuesFromInterface();

    void clearErrors();

    void tmrTimeout();

protected:

    virtual void resizeEvent(QResizeEvent *);

private:
    Ui::correctorControl *ui;

    QList<QSerialPortInfo> m_com_list;

    QSerialPort m_com;

    QTimer m_tmr;

    QByteArray m_comDataPack;

    QByteArray m_lastReceivedData;

    bool m_collectComData;

    bool m_currentValuesReceived;

    QMap<int32_t, QByteArray> m_flashData;

    uint8_t linChecksum(QByteArray frame);

    QList<QByteArray> linPackets(QByteArray receivedData);

    void displayFlashData();

    QByteArray m_settings;

    void displaySettings();

    QString checkSettings(QByteArray settings);

    void loadDefaultSettings();

    int calcDiv(int num, int div);

    void displayCurrentValues(QByteArray packet);

    QByteArray sendFrameAndWaitAck(QByteArray frameToSend, int receivedFrameCode, int receivedFrameSize, QString waitState = QString("Waiting ack"));
};

#endif // CORRECTOR_CONTROL_H
