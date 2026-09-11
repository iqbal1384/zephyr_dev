#pragma once
#include <QThread>
#include <QVector>
#include <QString>
#include <cstdint>

/* Runs in a dedicated thread; emits frameReady() for every valid FRAME line. */
class SerialReader : public QThread
{
    Q_OBJECT
public:
    explicit SerialReader(const QString &port, int baud, QObject *parent = nullptr);
    void stop();

signals:
    void frameReady(QVector<uint16_t> zones);
    void errorOccurred(const QString &msg);

protected:
    void run() override;

private:
    QString m_port;
    int     m_baud;
    volatile bool m_running = true;

    static bool parseLine(const QByteArray &line, uint16_t out[64]);
};
