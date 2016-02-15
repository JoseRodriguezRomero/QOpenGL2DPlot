#ifndef QOPENGL2DPLOT_H
#define QOPENGL2DPLOT_H

#ifdef ANDROID
#include <GLES/gl.h>
#else
#include <GL/gl.h>
#endif

#include <QOpenGLWidget>
#include <QSurface>
#include <QPainter>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLPaintDevice>
#include <QPointF>

#include <QFile>
#include <QtSvg/QSvgGenerator>

#include <math.h>

typedef struct PlotDataStruct PlotDataStruct;

class QOpenGL2DPlot : public QOpenGLWidget, protected QOpenGLFunctions
{
public:
    enum Axis {
        Bottom  = 0,
        Top     = 1,
        Left    = 2,
        Right   = 3
    };

    enum Direction {
        Vertical   = 0,
        Horizontal = 1
    };

private:
    PlotDataStruct *plot_data;

public:
    QOpenGL2DPlot(QWidget *parent = 0);
    ~QOpenGL2DPlot();

    void hideFrame(bool hide = true);
    void showFrame(bool show = true);

    bool isFrameVisible() const;
    bool isFrameHidden()  const;

    void hideLabel(Axis axis, bool hide = true);
    void showLabel(Axis axis, bool show = true);

    bool isLabelVisible(Axis axis) const;
    bool isLabelHidden(Axis axis) const;

    void hideTickLabel(Axis axis, bool hide = true);
    void showTickLabel(Axis axis, bool show = true);

    bool isTickLabelVisible(Axis axis) const;
    bool isTickLabelHidden(Axis axis) const;

    void setLogScale(Direction Direction, bool logscale = true);
    void setLinearScale(Direction Direction, bool linscale = true);

    bool isLogScale(Direction Direction) const;
    bool isLinearScale(Direction Direction) const;

    void addPlot(int before = 0);
    void addPlots(int count, int before = 0);
    void addPlots(const QVector<QVector<QPointF>> data, int before = 0);

    void addPoint(int plot_index, const QPointF &point, int pos = 0);
    void addPoints(int plot_index, const QVector<QPointF> &points, int pos = 0);

    void setPoint(int plot_index, const QPointF &point, int index);
    void setPoints(int plot_index, const QVector<QPointF> &points, int index);
    void setPoints(int plot_index, const QPointF point, int from, int to);

    void clearPoints(int plot_index, int from, int to);                                     //TODO
    void clearPoints(int plot_index);                                                       //TODO

    void setTopRange(Axis axis, double range);
    void setBottomRange(Axis axis, double range);
    void setRange(Axis axis, double top, double bottom);

    double TopRange(Axis axis) const;
    double BottomRange(Axis axis) const;

    void setLogTopRange(Axis axis, double range);
    void setLogBottomRange(Axis axis, double range);
    void setLogRange(Axis axis, double top, double bottom);

    double LogTopRange(Axis axis) const;
    double LogBottomRange(Axis axis) const;

    void showTitle(bool show = true);
    void hideTitle(bool hide = true);

    bool isTitleVisible() const;

    void setTickStep(Axis axis, double step);
    void setTicks(Axis axis, uint amount);
    void setSecTicks(Axis axis, uint amount);

    double TickStep(Axis axis) const;
    uint Ticks(Axis axis) const;
    uint SecTicks(Axis axis) const;

    void showGridLines(Axis axis, bool show = true);
    void hideGridLines(Axis axis, bool hide = true);

    void showTicks(Axis axis, bool show = true);
    void hideTicks(Axis axis, bool hide = true);

    void showSecGridLines(Axis axis, bool show = true);
    void hideSecGridLines(Axis axis, bool hide = true);

    void showSecTicks(Axis axis, bool show = true);
    void hideSecTicks(Axis axis, bool hide = true);

    void setGridColor(Axis axis, const QColor &color);
    void setGridColor(const QColor &color);
    QColor GridColor(Axis axis) const;

    void setSecGridColor(Axis axis, const QColor &color);
    void setSecGridColor(const QColor &color);
    QColor SecGridColor(Axis axis) const;

    bool areGridLinesVisible(Axis axis) const;
    bool areGridLinesHidden(Axis axis) const;

    void setPlotColor(int plot_index, const QColor &color);

    void showPlot(int plot_index, bool show = true);
    void hidePlot(int plot_index, bool hide = true);

    void RefreshPlot(Axis axis);

    void SaveSVG(const QString &fileName, const QString &description = QString(""));

protected:
    void initializeGL();
    void paintGL();
    void resizeGL(int w, int h);
};

#endif // QOPENGL2DPLOT_H
