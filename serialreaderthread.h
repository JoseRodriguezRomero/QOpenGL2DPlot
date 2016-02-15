#ifndef SERIALREADERTHREAD_H
#define SERIALREADERTHREAD_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QtSerialPort/QSerialPort>

class SerialReaderThread : public QObject
{
    Q_OBJECT
private:
    QThread thread;
    QSerialPort serial;
    QTimer timer;

    QByteArray read_data;

public:
    SerialReaderThread();
    ~SerialReaderThread();

    QThread* my_thread();

public slots:
    void SetSerialName(const QString &name);
    void Read();

    void Start();
    void Stop();

signals:
    void SerialError();

    void Started();
    void Stopped();

    void ValueRead(int value);
};

#endif // SERIALREADERTHREAD_H
