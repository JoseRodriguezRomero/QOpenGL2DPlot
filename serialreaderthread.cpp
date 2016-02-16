#include "serialreaderthread.h"

SerialReaderThread::SerialReaderThread()
{
    timer.setInterval(10);

    this->moveToThread(&thread);
    thread.start();

    connect(&timer,SIGNAL(timeout()),this,SLOT(Read()));
    connect(this,SIGNAL(Started()),&timer,SLOT(start()));
    connect(this,SIGNAL(Stopped()),&timer,SLOT(stop()));
}

SerialReaderThread::~SerialReaderThread()
{
    serial.close();
}

QThread* SerialReaderThread::my_thread()
{
    return &thread;
}

void SerialReaderThread::Read()
{
    QByteArray buffer;

    if (serial.isOpen())
    {
        serial.waitForReadyRead(3000);
        buffer = serial.readAll();
        serial.flush();
    }

    read_data.append(buffer);

    while (read_data.length() > 0 && read_data[0] != '{')
    {
        read_data.remove(0,1);
    }

    QByteArray aux;
    int len = read_data.length();
    int rem = 0;

    for (int i = 0; i < len; i++)
    {
        aux.append(read_data[i]);

        if (read_data[i] == '}')
        {
            rem = i+1;

            if (aux[aux.length()-1] == '}' &&
                    aux[0] == '{')
            {
                aux.remove(0,1);
                aux.chop(1);

                emit (ValueRead(aux.toInt()));
            }

            aux.clear();
        }
    }

    read_data.remove(0,rem);

    if (read_data.length() > 5000)
    {
        read_data.clear();
    }
}

void SerialReaderThread::SetSerialName(const QString &name)
{
    serial.close();

    while (serial.isOpen())
    {
    }

    serial.setPortName(name);
    serial.setBaudRate(QSerialPort::Baud9600);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);
    serial.open(QIODevice::ReadOnly);
}

void SerialReaderThread::Start()
{
    emit Started();
    serial.open(QIODevice::ReadOnly);
    serial.flush();
}

void SerialReaderThread::Stop()
{
    emit Stopped();
    serial.close();
}
