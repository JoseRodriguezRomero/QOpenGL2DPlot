#-------------------------------------------------
#
# Project created by QtCreator 2015-07-23T17:05:46
#
#-------------------------------------------------

QT       += core gui
QT       += opengl
QT       += svg
QT       += serialport

CONFIG += c++14

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = GLProj1
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
    QOpenGL2DPlot.cpp

HEADERS  += mainwindow.h \
    QOpenGL2DPlot.h

