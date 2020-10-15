#ifndef HEX_CONVERTER_H
#define HEX_CONVERTER_H

#include <QMap>

QMap<int32_t,QByteArray> hexFileToMap(QString hexFile);
QString displayHexMap(QMap<int32_t, QByteArray> map);
QMap<int32_t, QByteArray> resizeMap(QMap<int32_t, QByteArray> map);

#endif // HEX_CONVERTER_H
