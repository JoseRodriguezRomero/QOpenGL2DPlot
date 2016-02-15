#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QFrame *main_frame = new QFrame(this);
    main_frame->setLayout(new QHBoxLayout(main_frame));
    main_frame->setFrameStyle(QFrame::StyledPanel);
    main_frame->layout()->setContentsMargins(0,0,0,0);
    this->setCentralWidget(main_frame);
    main_frame->setFrameStyle(QFrame::NoFrame);

    gl_scene = new QOpenGL2DPlot(this);
    gl_scene->setMinimumWidth(180*3);
    gl_scene->setMinimumHeight(180*2.5);
    main_frame->layout()->addWidget(gl_scene);

    gl_scene->setRange(QOpenGL2DPlot::Left,1024,0);
    gl_scene->setRange(QOpenGL2DPlot::Bottom,1000,0);
    gl_scene->addPlots(1);

    gl_scene->setTickStep(QOpenGL2DPlot::Left,100);
    gl_scene->setTickStep(QOpenGL2DPlot::Bottom,100);

    gl_scene->hideTitle();

    gl_scene->hideTickLabel(QOpenGL2DPlot::Left);
    gl_scene->hideTickLabel(QOpenGL2DPlot::Bottom);

    gl_scene->hideLabel(QOpenGL2DPlot::Left);
    gl_scene->hideLabel(QOpenGL2DPlot::Bottom);

    QFrame *F1 = new QFrame(this);
    F1->setLayout(new QVBoxLayout(F1));
    F1->setFixedWidth(180);
    main_frame->layout()->addWidget(F1);

    serial_names = new QComboBox(this);
    F1->layout()->addWidget(serial_names);

    QFrame *F11 = new QFrame(this);
    F11->setLayout(new QHBoxLayout(F11));
    F11->layout()->setContentsMargins(0,0,0,0);
    F1->layout()->addWidget(F11);

    refresh = new QPushButton(this);
    refresh->setText("Refresh");
    F11->layout()->addWidget(refresh);

    start = new QPushButton(this);
    start->setText("Start");
    F11->layout()->addWidget(start);    

    F1->layout()->addItem(new QSpacerItem(0,0,QSizePolicy::MinimumExpanding,
                                          QSizePolicy::MinimumExpanding));

    QVector<QPointF> vals;

    for (int i = 0; i < 1000; i++)
    {
        vals.append(QPointF(999-i,512));
    }

    gl_scene->addPoints(0,vals);

    gl_timer.setInterval(15);
    gl_timer.start();

    current = 0;

    connect(refresh,SIGNAL(clicked(bool)),this,SLOT(RefreshSerialList()));
    connect(start,SIGNAL(clicked(bool)),this,SLOT(StartPressed()));
    connect(&gl_timer,SIGNAL(timeout()),gl_scene,SLOT(update()));
    connect(&read_thread,SIGNAL(ValueRead(int)),this,SLOT(SetValue(int)));
}

MainWindow::~MainWindow()
{    
    read_thread.Stop();

    while (read_thread.my_thread()->isRunning())
    {
        read_thread.my_thread()->quit();
        read_thread.my_thread()->wait();
    }
}

void MainWindow::RefreshSerialList()
{
    QList<QSerialPortInfo> list = QSerialPortInfo::availablePorts();

    QStringList port_names;

    foreach (QSerialPortInfo port_info, list)
    {
        port_names.append(port_info.portName());
    }

    serial_names->clear();
    serial_names->addItems(port_names);
}

void MainWindow::StartPressed()
{
    if (start->text() == "Start")
    {
        Start();
    }
    else
    {
        Stop();
    }
}

void MainWindow::Start()
{
    start->setText("Stop");

    read_thread.SetSerialName(serial_names->currentText());
    read_thread.Start();
}

void MainWindow::Stop()
{
    start->setText("Start");
    read_thread.Stop();
}

void MainWindow::SetValue(int value)
{
    double x = static_cast<double>(current);
    double y = static_cast<double>(value);

    gl_scene->setPoint(0,QPointF(x,y),current);
    current++;

    if (current >= 1000)
    {
        current = 0;
    }
}
