#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    gl_scene = new QOpenGL2DPlot(this);
    this->setCentralWidget(gl_scene);

    gl_scene->setTitle("My Plot");
    gl_scene->setLabel(QOpenGL2DPlot::Left,"My Left Label");
    gl_scene->setLabel(QOpenGL2DPlot::Bottom,"My Bottom Label");

    gl_scene->setRange(QOpenGL2DPlot::Bottom,100,0);
    gl_scene->setRange(QOpenGL2DPlot::Left,1,0);

    gl_scene->setTickStep(QOpenGL2DPlot::Bottom,10);
    gl_scene->setTickStep(QOpenGL2DPlot::Left,.1);

    QVector<QPointF> values;
    QPointF aux;

    for (int i = 0; i <= 100; i++)
    {
        aux.setX(40.0*cos(1.0*i*2*M_PI/100.0)+50.0);
        aux.setY( 0.4*sin(2.0*i*2*M_PI/100.0)+ 0.5);

        values.append(aux);
    }

    gl_scene->addPlot();
    gl_scene->addPoints(0,values);
    gl_scene->setPlotColor(0,Qt::green);


    values.clear();

    for (int i = 0; i <= 100; i++)
    {
        aux.setX(20.0*cos(1.0*i*2*M_PI/100.0)+50.0);
        aux.setY( 0.2*sin(1.0*i*2*M_PI/100.0)+ 0.5);

        values.append(aux);
    }

    gl_scene->addPlot();
    gl_scene->addPoints(1,values);
    gl_scene->setPlotColor(1,QColor(Qt::red));

    std::cout << gl_scene->PlotSize(0) << std::endl;
    std::cout << gl_scene->PlotSize(1) << std::endl;
}

MainWindow::~MainWindow()
{    
}
