#pragma once
#include <QMainWindow>
#include <QVector>
#include <cstdint>

class QComboBox;
class QPushButton;
class QSlider;
class QLabel;
class QTableWidget;
class RangingGrid;
class SerialReader;
struct Blob;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectToggle(bool checked);
    void onFrameReady(QVector<uint16_t> zones);
    void onSerialError(const QString &msg);
    void onThresholdChanged(int value);
    void refreshPorts();

private:
    void setupUi();
    void updateBlobTable(const QVector<Blob> &blobs);
    void disconnect();

    QComboBox    *m_portCombo   = nullptr;
    QComboBox    *m_baudCombo   = nullptr;
    QPushButton  *m_connectBtn  = nullptr;
    QPushButton  *m_refreshBtn  = nullptr;
    RangingGrid  *m_grid        = nullptr;
    QSlider      *m_threshSlider= nullptr;
    QLabel       *m_threshLabel = nullptr;
    QLabel       *m_fpsLabel    = nullptr;
    QTableWidget *m_blobTable   = nullptr;

    SerialReader *m_reader      = nullptr;
    uint16_t      m_threshold   = 1500;
    int           m_frameCount  = 0;
    qint64        m_fpsTimestamp= 0;
};
