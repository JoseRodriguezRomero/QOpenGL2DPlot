#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFrame>
#include <QVBoxLayout>
#include <QOpenGL2DPlot.h>
#include <QPushButton>
#include <QComboBox>

#include <QtSerialPort/QSerialPortInfo>

#include <serialreaderthread.h>

class MainWindow : public QMainWindow
{
    Q_OBJECT
private:
    QOpenGL2DPlot *gl_scene;
    QTimer gl_timer;

    QComboBox *serial_names;
    QPushButton *start;
    QPushButton *refresh;

    SerialReaderThread read_thread;

    int current;
    QVector<QPointF> values;

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void Start();
    void Stop();

public slots:
    void RefreshSerialList();
    void StartPressed();

    void SetValue(int value);
};

#endif // MAINWINDOW_H
