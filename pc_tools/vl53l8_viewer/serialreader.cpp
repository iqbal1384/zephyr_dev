#include "serialreader.h"
#include <QSerialPort>

SerialReader::SerialReader(const QString &port, int baud, QObject *parent)
    : QThread(parent), m_port(port), m_baud(baud)
{}

void SerialReader::stop()
{
    m_running = false;
}

bool SerialReader::parseLine(const QByteArray &line, uint16_t out[64])
{
    if (!line.startsWith("FRAME:"))
        return false;

    const QByteArray payload = line.mid(6).trimmed();
    const QList<QByteArray> parts = payload.split(',');
    if (parts.size() != 64)
        return false;

    for (int i = 0; i < 64; i++) {
        bool ok;
        out[i] = parts[i].trimmed().toUShort(&ok);
        if (!ok)
            return false;
    }
    return true;
}

void SerialReader::run()
{
    QSerialPort serial;
    serial.setPortName(m_port);
    serial.setBaudRate(m_baud);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!serial.open(QIODevice::ReadOnly)) {
        emit errorOccurred(serial.errorString());
        return;
    }

    QByteArray buf;
    while (m_running) {
        if (serial.waitForReadyRead(100)) {
            buf += serial.readAll();

            int nl;
            while ((nl = buf.indexOf('\n')) >= 0) {
                QByteArray line = buf.left(nl);
                buf.remove(0, nl + 1);

                uint16_t zones[64];
                if (parseLine(line, zones)) {
                    QVector<uint16_t> v(zones, zones + 64);
                    emit frameReady(v);
                }
            }
        }
    }
    serial.close();
}
