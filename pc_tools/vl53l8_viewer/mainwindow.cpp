#include "mainwindow.h"
#include "ranginggrid.h"
#include "serialreader.h"
#include "objectdetector.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QStatusBar>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("VL53L8A1 Viewer");
    setupUi();
    refreshPorts();
}

MainWindow::~MainWindow()
{
    disconnect();
}

void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    /* ---- Serial port bar ---- */
    QGroupBox *serialBox = new QGroupBox("Serial Port");
    m_portCombo  = new QComboBox;
    m_baudCombo  = new QComboBox;
    for (int b : {9600, 115200, 460800, 921600})
        m_baudCombo->addItem(QString::number(b), b);
    m_baudCombo->setCurrentText("115200");
    m_refreshBtn = new QPushButton("Refresh");
    m_connectBtn = new QPushButton("Connect");
    m_connectBtn->setCheckable(true);

    QHBoxLayout *serialRow = new QHBoxLayout(serialBox);
    serialRow->addWidget(new QLabel("Port:"));
    serialRow->addWidget(m_portCombo, 1);
    serialRow->addWidget(new QLabel("Baud:"));
    serialRow->addWidget(m_baudCombo);
    serialRow->addWidget(m_refreshBtn);
    serialRow->addWidget(m_connectBtn);

    /* ---- Heatmap grid ---- */
    m_grid     = new RangingGrid;
    m_fpsLabel = new QLabel("FPS: --");
    m_fpsLabel->setAlignment(Qt::AlignRight);

    QVBoxLayout *gridCol = new QVBoxLayout;
    gridCol->addWidget(m_grid, 1);
    gridCol->addWidget(m_fpsLabel);

    /* ---- Threshold slider ---- */
    QGroupBox *threshBox = new QGroupBox("Object Detection Threshold");
    m_threshSlider = new QSlider(Qt::Horizontal);
    m_threshSlider->setRange(100, 4000);
    m_threshSlider->setValue(static_cast<int>(m_threshold));
    m_threshSlider->setTickPosition(QSlider::TicksBelow);
    m_threshSlider->setTickInterval(500);
    m_threshLabel  = new QLabel(QString("%1 mm").arg(m_threshold));
    m_threshLabel->setMinimumWidth(70);

    QHBoxLayout *threshRow = new QHBoxLayout(threshBox);
    threshRow->addWidget(new QLabel("100 mm"));
    threshRow->addWidget(m_threshSlider, 1);
    threshRow->addWidget(new QLabel("4000 mm"));
    threshRow->addWidget(m_threshLabel);

    /* ---- Object table ---- */
    QGroupBox *blobBox = new QGroupBox("Detected Objects");
    m_blobTable = new QTableWidget(0, 5);
    m_blobTable->setHorizontalHeaderLabels(
        {"ID", "Shape", "Dist (mm)", "Centroid (r,c)", "Zones"});
    m_blobTable->horizontalHeader()->setStretchLastSection(true);
    m_blobTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_blobTable->setEditTriggers(QTableWidget::NoEditTriggers);
    m_blobTable->setSelectionBehavior(QTableWidget::SelectRows);
    m_blobTable->setAlternatingRowColors(true);

    QVBoxLayout *blobBoxLay = new QVBoxLayout(blobBox);
    blobBoxLay->addWidget(m_blobTable);

    QVBoxLayout *rightCol = new QVBoxLayout;
    rightCol->addWidget(threshBox);
    rightCol->addWidget(blobBox, 1);

    /* ---- Main layout ---- */
    QHBoxLayout *midRow = new QHBoxLayout;
    midRow->addLayout(gridCol, 1);
    midRow->addLayout(rightCol, 1);

    QVBoxLayout *root = new QVBoxLayout(central);
    root->addWidget(serialBox);
    root->addLayout(midRow, 1);

    statusBar()->showMessage("Disconnected");

    /* ---- Signal wiring ---- */
    connect(m_connectBtn,  &QPushButton::toggled,
            this, &MainWindow::onConnectToggle);
    connect(m_refreshBtn,  &QPushButton::clicked,
            this, &MainWindow::refreshPorts);
    connect(m_threshSlider, &QSlider::valueChanged,
            this, &MainWindow::onThresholdChanged);
}

void MainWindow::refreshPorts()
{
    m_portCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        m_portCombo->addItem(info.portName());
    if (m_portCombo->count() == 0)
        m_portCombo->addItem("(none)");
}

void MainWindow::onConnectToggle(bool checked)
{
    if (checked) {
        const QString port = m_portCombo->currentText();
        if (port.isEmpty() || port == "(none)") {
            m_connectBtn->setChecked(false);
            QMessageBox::warning(this, "No port", "Please select a serial port.");
            return;
        }
        const int baud = m_baudCombo->currentData().toInt();

        m_reader = new SerialReader(port, baud, this);
        connect(m_reader, &SerialReader::frameReady,
                this, &MainWindow::onFrameReady,
                Qt::QueuedConnection);
        connect(m_reader, &SerialReader::errorOccurred,
                this, &MainWindow::onSerialError,
                Qt::QueuedConnection);
        m_reader->start();

        m_connectBtn->setText("Disconnect");
        m_portCombo->setEnabled(false);
        m_baudCombo->setEnabled(false);
        m_frameCount   = 0;
        m_fpsTimestamp = QDateTime::currentMSecsSinceEpoch();
        statusBar()->showMessage(QString("Connected — %1 @ %2 baud").arg(port).arg(baud));
    } else {
        disconnect();
        m_connectBtn->setText("Connect");
        m_portCombo->setEnabled(true);
        m_baudCombo->setEnabled(true);
        m_fpsLabel->setText("FPS: --");
        statusBar()->showMessage("Disconnected");
    }
}

void MainWindow::disconnect()
{
    if (m_reader) {
        m_reader->stop();
        m_reader->wait(2000);
        delete m_reader;
        m_reader = nullptr;
    }
}

void MainWindow::onFrameReady(QVector<uint16_t> zones)
{
    /* Update FPS counter every second. */
    m_frameCount++;
    const qint64 now     = QDateTime::currentMSecsSinceEpoch();
    const qint64 elapsed = now - m_fpsTimestamp;
    if (elapsed >= 1000) {
        m_fpsLabel->setText(
            QString("FPS: %1").arg(m_frameCount * 1000.0 / elapsed, 0, 'f', 1));
        m_frameCount   = 0;
        m_fpsTimestamp = now;
    }

    m_grid->setFrame(zones);

    const QVector<Blob> blobs = ObjectDetector::detect(zones.constData(), m_threshold);
    m_grid->setBlobs(blobs);
    updateBlobTable(blobs);
}

void MainWindow::onSerialError(const QString &msg)
{
    if (m_connectBtn->isChecked()) {
        m_connectBtn->setChecked(false);   /* triggers onConnectToggle(false) */
    }
    statusBar()->showMessage("Serial error: " + msg);
}

void MainWindow::onThresholdChanged(int value)
{
    m_threshold = static_cast<uint16_t>(value);
    m_threshLabel->setText(QString("%1 mm").arg(value));
}

void MainWindow::updateBlobTable(const QVector<Blob> &blobs)
{
    m_blobTable->setRowCount(blobs.size());
    for (int i = 0; i < blobs.size(); i++) {
        const Blob &b = blobs[i];
        m_blobTable->setItem(i, 0, new QTableWidgetItem(QString::number(b.id)));
        m_blobTable->setItem(i, 1, new QTableWidgetItem(b.shape));
        m_blobTable->setItem(i, 2, new QTableWidgetItem(
            QString::number(static_cast<int>(b.avg_distance_mm))));
        m_blobTable->setItem(i, 3, new QTableWidgetItem(
            QString("(%1, %2)")
                .arg(b.centroid_row, 0, 'f', 1)
                .arg(b.centroid_col, 0, 'f', 1)));
        m_blobTable->setItem(i, 4, new QTableWidgetItem(
            QString::number(b.zone_count)));
    }
}
