#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "QOpenGL2DPlot.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
private:
    QOpenGL2DPlot *gl_scene;

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

};

#endif // MAINWINDOW_H
