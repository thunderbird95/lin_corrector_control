#include <QMap>

QMap<int32_t, QByteArray> hexFileToMap(QString hexFile)
{
    QMap<int32_t, QByteArray> data;
    bool isSegmentAddressChoosen = false;
    int32_t segmentAddress = 0;
    int32_t lineAddress = 0;
    QStringList hexStrings = hexFile.split("\n");
    for (int i = 0; i < hexStrings.length(); i++)
    {
        if (hexStrings[i].isEmpty())
            continue;
        if (hexStrings[i][0] != ":")
            continue;
        if ((hexStrings[i].length() % 2) != 1)
        {
            data.clear();
            data.insert(-1, QString("Error: symbols num in str %1 not correct").arg(i).toLocal8Bit());
            return data;
        }
        QByteArray currentBytes;
        bool conversionSuccess = false;
        for (int j = 0; j < ((hexStrings[i].length() - 1) / 2); j++)
        {
            QString currentByteText = hexStrings[i].mid(1+j*2,2);
            currentBytes.push_back(currentByteText.toInt(&conversionSuccess,16));
            if (!conversionSuccess)
            {
                data.clear();
                data.insert(-1, QString("Error: symbol %1  in str %2 not correct").arg(j).arg(i).toLocal8Bit());
                return data;
            }
        }
        if (currentBytes.length() < 5)
        {
            data.clear();
            data.insert(-1, QString("Error: str %1 is too short (%2 bytes, minimum - 5)").arg(i).arg(currentBytes.length()).toLocal8Bit());
            return data;
        }
        uint8_t checksum = 0;
        foreach (char byte, currentBytes)
            checksum += ((uint8_t)byte);
        uint8_t dataLength = (uint8_t)(currentBytes[0]);
        if ((dataLength + 5) != currentBytes.length())
        {
            data.clear();
            data.insert(-1, QString("Error: str %1 has not correct length").arg(i).toLocal8Bit());
            return data;
        }
        int32_t currentAddress = ((uint16_t)((uint8_t)(currentBytes[1]))) * 256 + ((uint16_t)((uint8_t)(currentBytes[2])));
        switch (currentBytes[3])
        {
            case 0:
                // ADDRESS DIV 2 BECAUSE ONE PIC WORD HAS 2 BYTES
                if (isSegmentAddressChoosen)
                    data.insert((currentAddress + segmentAddress) / 2, currentBytes.mid(4, dataLength));
                else
                    data.insert((currentAddress + lineAddress) / 2, currentBytes.mid(4, dataLength));
                break;
            case 1:
                return data;
            case 2:
                segmentAddress = ((int32_t)((uint8_t)(currentBytes[4]))) * 16*256 + ((int32_t)((uint8_t)(currentBytes[5]))) * 16;
                isSegmentAddressChoosen = true;
                break;
            case 4:
                lineAddress = ((int32_t)((uint8_t)(currentBytes[4]))) * 256*256*256 + ((int32_t)((uint8_t)(currentBytes[5]))) * 256*256;
                isSegmentAddressChoosen = false;
                break;
            default:
                data.clear();
                data.insert(-1, QString("Error: str %1 has not correct type").arg(i).toLocal8Bit());
                return data;
        }
    }
    data.clear();
    data.insert(-1, QString("Error: end of writing (type 1) not found").toLocal8Bit());
    return data;
}

QMap<int32_t, QByteArray> resizeMap(QMap<int32_t, QByteArray> map)
{
    QMap<int32_t, QByteArray> resizedMap;
    foreach (int32_t address, map.keys())
    {
        int32_t segment = address / 16;
        int32_t newAddress = segment * 16;
        if (!resizedMap.contains(newAddress))
            resizedMap.insert(newAddress, QByteArray(32, 0xFF));
    }
    foreach (int32_t address, map.keys())
    {
        for (int i = 0; i < (map.value(address).length() / 2); i++)
        {
            int32_t currentAddress = address + i;
            int32_t currentSegment = currentAddress / 16;
            int32_t segmentAddress = currentSegment * 16;
            int32_t addressInSegment = currentAddress - segmentAddress;
            resizedMap[segmentAddress][addressInSegment * 2] = map.value(address)[i * 2];
            resizedMap[segmentAddress][addressInSegment * 2 + 1] = map.value(address)[i * 2 + 1];
        }
    }
    return resizedMap;
}

QString displayHexMap(QMap<int32_t, QByteArray> map)
{
    QString textData;
    foreach(int32_t address, map.keys())
    {
        if (address < 0x8000)
        {
            textData = textData + QString::number(address, 16) + ":";
            foreach (char byte, map.value(address))
                textData = textData + " " + QString::number((uint32_t)(byte) & 0xFF, 16) + " ";
            textData += "\n";
        }
        else
        {
            for (int i = 0; i < (map.value(address).length() / 2); i++)
            {
                int32_t currentAddress = address + i;
                int32_t word = ((int32_t)((uint8_t)((map.value(address))[2*i]))) + ((int32_t)((uint8_t)((map.value(address))[2*i+1]))) * 256;
                switch (currentAddress)
                {
                    case 0x8000:
                        textData = textData + "USER ID 0 (8000): " + QString::number(word, 16) + "\n";
                        break;
                    case 0x8001:
                        textData = textData + "USER ID 1 (8001): " + QString::number(word, 16) + "\n";
                        break;
                    case 0x8002:
                        textData = textData + "USER ID 2 (8002): " + QString::number(word, 16) + "\n";
                        break;
                    case 0x8003:
                        textData = textData + "USER ID 3 (8003): " + QString::number(word, 16) + "\n";
                        break;
                    case 0x8006:
                        textData = textData + "DEVICE ID (8006): " + QString::number(word, 16) + "\n";
                        break;
                    case 0x8007:
                        textData = textData + "CONFIGURATION WORD 1 (8007): " + QString::number(word, 16) + "\n";
                        break;
                    case 0x8008:
                        textData = textData + "CONFIGURATION WORD 2 (8008): " + QString::number(word, 16) + "\n";
                        break;
                }
            }
        }
    }
    return textData;
}
