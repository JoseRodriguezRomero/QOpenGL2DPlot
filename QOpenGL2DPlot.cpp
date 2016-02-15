#include "QOpenGL2DPlot.h"

#define MAX_FRAME_MARGIN            5
#define MARGIN_REL_SIZE             0.05

#define TITLE_REL_SIZE              0.15
#define LABEL_REL_SIZE              0.05

#define BOTTOM                      0
#define TOP                         1
#define LEFT                        2
#define RIGHT                       3

#define VERTICAL                    0
#define HORIZONTAL                  1

#define TOP_LEFT_X                  0
#define TOP_LEFT_Y                  1
#define BOTTOM_LEFT_X               2
#define BOTTOM_LEFT_Y               3
#define BOTTOM_RIGHT_X              4
#define BOTTOM_RIGHT_Y              5
#define TOP_RIGHT_X                 6
#define TOP_RIGHT_Y                 7

#define DEFAULT_FRAME_COLOR         QColor(0,0,0,255)
#define DEFAULT_PLOT_COLOR          QColor(0,0,0,255)
#define DEFAULT_GRID_COLOR          QColor(200,200,200,255)
#define DEFAULT_SEC_GRID_COLOR      QColor(230,230,230,255)
#define DEFAULT_PLOT_VISIBLE        true

#define DEFAULT_TOP_RANGE           10
#define DEFAULT_BOT_RANGE           0

#define DEFAULT_LOG_TOP_RANGE       10
#define DEFAULT_LOG_BOT_RANGE       1E-1

#define DEFAULT_TICK_STEP           2
#define DEFAULT_SEC_TICK_COUNT      4

#define DEFAULT_TITLE               "Plot Title"
#define DEFAULT_BOT_LABEL           "Bottom Label"
#define DEFAULT_TOP_LABEL           "Top Label"
#define DEFAULT_LEFT_LABEL          "Left Label"
#define DEFAULT_RIGHT_LABEL         "Right Label"

#ifdef QT_DEBUG
#define NO_ERRORS                   0x00
#define PLOT_INDEX_ERROR            0x01
#define INDEX_ERROR                 0x02
#define LOG_RANGE_ERROR             0x04
#define RANGE_ERROR                 0x08
#define BUFFER_SIZE_ERROR           0x10
#endif

static const char vertexShaderSource[] =
        "attribute highp vec2 pos;\n"
        "uniform lowp vec4 col;\n"
        "uniform highp mat4 matrix;\n"
        "varying lowp vec4 FragColor;\n"
        "void main() {\n"
        "   gl_Position = matrix*vec4(pos,0.0,1.0);\n"
        "   FragColor = col;\n"
        "}\n";

static const char vertexFragmentSource[] =
        "varying lowp vec4 FragColor;\n"
        "void main() {\n"
        "   gl_FragColor = FragColor;\n"
        "}\n";

#ifdef QT_DEBUG
typedef int Error;

void ErrorHandle(Error error)
{
    if (NO_ERRORS)
    {
        return;
    }

    QString error_string("");

    if (error & PLOT_INDEX_ERROR)
    {
        error_string.append("QOpenGL2DPlot: Plot index out of "
                            "range.\n");
    }

    if (error & INDEX_ERROR)
    {
        error_string.append("QOpenGL2DPlot: Index out of "
                            "range.\n");
    }

    if (error & LOG_RANGE_ERROR)
    {
        error_string.append("QOpenGL2DPlot: Logarithmic "
                            "ranges must be superior to 0.\n");
    }

    if (error & RANGE_ERROR)
    {
        error_string.append("QOpenGL2DPlot: Top range must be "
                            "superior to bottom range.\n");
    }

    if (error & BUFFER_SIZE_ERROR)
    {
        error_string.append("QOpenGL2DPlot: Buffer size "
                            "exceeded\n");
    }

    try {
        if (error_string.length())
        {
            throw error;
        }
    } catch (Error e)
    {
        qFatal(error_string.toLatin1());
        exit(e);
    }
}

void CheckPlotIndex(int plot_index,
                    const QVector<QVector<QPointF>> &data,
                    Error &error)
{
    if (plot_index >= data.count() &&
            plot_index)
    {
        error |= PLOT_INDEX_ERROR;
    }
}

void CheckIndex(int index, const QVector<QPointF> &data,
                Error &error)
{
    if (index >= data.count())
    {
        error |= INDEX_ERROR;
    }
}

void CheckRange(double top, double bottom, Error &error)
{
    if (bottom > top)
    {
        error |= RANGE_ERROR;
    }
}

void CheckLogRange(double range, Error &error)
{
    if (range <= 0)
    {
        error |= LOG_RANGE_ERROR;
    }
}
#endif // QT_DEBUG

struct PlotDataStruct {
    QPainter painter;
    QRect viewport;

    QOpenGLContext *context;
    QOpenGLFunctions *functions;
    QOpenGLPaintDevice *device;

    GLint pos;
    GLint col;
    GLint mat;

    QOpenGLShaderProgram m_program;
    QOpenGLBuffer frame_pos_buffer;
    QOpenGLBuffer frame_index_buffer;
    QOpenGLVertexArrayObject frame_vao;
    QColor frame_color;

    QMatrix4x4 matrix;
    QMatrix4x4 data_matrix;
    QMatrix4x4 grid_matrix;

    QRect plot_pane;

    bool title_visible;
    QRect title_rect;
    QString title;

    bool labels_visible[4];
    QRect labels_rect[4];
    QString labels[4];

    bool tick_labels_visible[4];
    QRect tick_labels_rect[4];
    QVector<QString> tick_labels[4];
    QVector<QRect> tick_labels_rects[4];

    double bottom_range[4];
    double top_range[4];

    double log_bottom_range[4];
    double log_top_range[4];

    bool logplot[2];

    QVector<QVector<QPointF>> data;
    QVector<QOpenGLBuffer> data_pos_buffer;
    QVector<QOpenGLBuffer> data_index_buffer;
    QVector<QOpenGLVertexArrayObject*> data_vao;
    QVector<bool> data_visible;
    QVector<QColor> data_color;

    double tick_step[4];
    uint ticks_count[4];
    uint sec_ticks_count[4];
    uint total_sec_ticks_count[4];

    bool ticks_visible[4];
    QOpenGLBuffer tick_buffer[4];
    bool sec_ticks_visible[4];
    QOpenGLBuffer sec_tick_buffer[4];

    bool grid_visible[4];
    QOpenGLBuffer grid_buffer[4];
    QOpenGLVertexArrayObject grid_vao[4];
    QColor grid_color[4];
    bool sec_grid_visible[4];
    QOpenGLBuffer sec_grid_buffer[4];
    QOpenGLVertexArrayObject sec_grid_vao[4];
    QColor sec_grid_color[4];

    bool frame_visible;

    double x_scale;
    double y_scale;
    double x_offset;
    double y_offset;
};

void SetFrameSize(PlotDataStruct *plot_data,
                  const QRect &viewport)
{    
    plot_data->matrix.setToIdentity();
    plot_data->matrix.ortho(viewport);

    plot_data->viewport = viewport;

    GLfloat pos[8];
    QFont font;
    QFontMetrics metrics(font);

    int w_margin = floor(viewport.width()*MARGIN_REL_SIZE);
    if (w_margin > MAX_FRAME_MARGIN)
    {
        w_margin = MAX_FRAME_MARGIN;
    }

    int h_margin = floor(viewport.height()*MARGIN_REL_SIZE);
    if (h_margin > MAX_FRAME_MARGIN)
    {
        h_margin = MAX_FRAME_MARGIN;
    }

    pos[BOTTOM_LEFT_X]  = 0 + w_margin;
    pos[BOTTOM_LEFT_Y]  = viewport.height() - h_margin;

    pos[TOP_LEFT_X]     = 0 + w_margin;
    pos[TOP_LEFT_Y]     = 0 + h_margin;

    pos[TOP_RIGHT_X]    = viewport.width()  - w_margin;
    pos[TOP_RIGHT_Y]    = 0 + h_margin;

    pos[BOTTOM_RIGHT_X] = viewport.width()  - w_margin;
    pos[BOTTOM_RIGHT_Y] = viewport.height() - h_margin;

    int w_label = floor(viewport.width()*LABEL_REL_SIZE);
    int max_label_height = 1.5*metrics.height();

    if (w_label > 4*max_label_height)
    {
        w_label = 4*max_label_height;
    }

    int h_label = floor(viewport.height()*LABEL_REL_SIZE);
    if (h_label > max_label_height)
    {
        h_label = max_label_height;
    }

    int h_label_comp = floor(1.5*h_label);
    int h_label_s_comp = h_label_comp-h_label;

    if (plot_data->labels_visible[BOTTOM])
    {
        pos[BOTTOM_LEFT_Y]  -= h_label;
        pos[BOTTOM_RIGHT_Y] -= h_label;
    }
    if (plot_data->labels_visible[TOP])
    {
        pos[TOP_LEFT_Y]  += h_label;
        pos[TOP_RIGHT_Y] += h_label;
    }
    if (plot_data->labels_visible[LEFT])
    {
        pos[BOTTOM_LEFT_X]  += h_label;
        pos[TOP_LEFT_X]     += h_label;
    }
    if (plot_data->labels_visible[RIGHT])
    {
        pos[BOTTOM_RIGHT_X] -= h_label;
        pos[TOP_RIGHT_X]    -= h_label;
    }

    if (plot_data->tick_labels_visible[BOTTOM])
    {
        pos[BOTTOM_LEFT_Y]  -= h_label_comp;
        pos[BOTTOM_RIGHT_Y] -= h_label_comp;
    }
    if (plot_data->tick_labels_visible[TOP])
    {
        pos[TOP_LEFT_Y]     += h_label_comp;
        pos[TOP_RIGHT_Y]    += h_label_comp;
    }
    if (plot_data->tick_labels_visible[LEFT])
    {
        pos[BOTTOM_LEFT_X]  += w_label+h_label_s_comp;
        pos[TOP_LEFT_X]     += w_label+h_label_s_comp;
    }
    if (plot_data->tick_labels_visible[RIGHT])
    {
        pos[BOTTOM_RIGHT_X] -= w_label+h_label_s_comp;
        pos[TOP_RIGHT_X]    -= w_label+h_label_s_comp;
    }

    int title_w = floor(viewport.width()-2*w_margin);
    int title_h = 0;
    int max_title_size = 3*metrics.height();

    if (plot_data->title_visible)
    {
        title_h = viewport.height()*TITLE_REL_SIZE;
    }

    if (title_h > max_title_size)
    {
        title_h = max_title_size;
    }

    pos[TOP_LEFT_Y]  += title_h;
    pos[TOP_RIGHT_Y] += title_h;

    QPoint top_left(pos[TOP_LEFT_X],
                    pos[TOP_LEFT_Y]);
    QPoint bottom_right(pos[BOTTOM_RIGHT_X],
                        pos[BOTTOM_RIGHT_Y]);

    plot_data->plot_pane = QRect(top_left,bottom_right);

    QOpenGLVertexArrayObject::Binder vao_binder(&(plot_data->frame_vao));
    {
        plot_data->m_program.enableAttributeArray(plot_data->pos);

        plot_data->frame_pos_buffer.bind();
        plot_data->frame_pos_buffer.allocate(pos,8*sizeof(GLfloat));
        plot_data->m_program.setAttributeBuffer(plot_data->pos,GL_FLOAT,0,2);
        plot_data->frame_pos_buffer.release();

        plot_data->frame_index_buffer.bind();
    }
    vao_binder.release();

    plot_data->title_rect.setHeight(title_h-2*h_margin);
    plot_data->title_rect.setWidth(title_w);
    plot_data->title_rect.setX(w_margin);
    plot_data->title_rect.setY(h_margin);

    //LEFT

    plot_data->labels_rect[LEFT].setX(w_margin);
    plot_data->labels_rect[LEFT].setY(plot_data->plot_pane.y());

    plot_data->labels_rect[LEFT].setHeight(h_label);
    plot_data->labels_rect[LEFT].setWidth(plot_data->plot_pane.height()-1);

    plot_data->tick_labels_rect[LEFT].setX(plot_data->plot_pane.topLeft().x()
                                -w_label-h_label_s_comp);
    plot_data->tick_labels_rect[LEFT].setY(plot_data->plot_pane.y());

    plot_data->tick_labels_rect[LEFT].setWidth(w_label);
    plot_data->tick_labels_rect[LEFT].setHeight(plot_data->plot_pane.height()-1);

    //RIGHT

    plot_data->labels_rect[RIGHT].setX(viewport.width()-h_label-w_margin);
    plot_data->labels_rect[RIGHT].setY(plot_data->plot_pane.y());

    plot_data->labels_rect[RIGHT].setWidth(plot_data->plot_pane.height()-1);
    plot_data->labels_rect[RIGHT].setHeight(h_label);

    plot_data->tick_labels_rect[RIGHT].setX(plot_data->plot_pane.topRight().x()+
                                            h_label_s_comp);
    plot_data->tick_labels_rect[RIGHT].setY(plot_data->plot_pane.y());

    plot_data->tick_labels_rect[RIGHT].setWidth(w_label);
    plot_data->tick_labels_rect[RIGHT].setHeight(plot_data->plot_pane.height()-1);

    //BOTTOM

    plot_data->labels_rect[BOTTOM].setX(plot_data->plot_pane.x());
    plot_data->labels_rect[BOTTOM].setY(viewport.height()-h_label-h_margin);

    plot_data->labels_rect[BOTTOM].setWidth(plot_data->plot_pane.width()-1);
    plot_data->labels_rect[BOTTOM].setHeight(h_label);

    plot_data->tick_labels_rect[BOTTOM].setX(plot_data->plot_pane.x());
    plot_data->tick_labels_rect[BOTTOM].setY(plot_data->plot_pane.bottomLeft().y()
                                             +(h_label_s_comp));

    plot_data->tick_labels_rect[BOTTOM].setWidth(plot_data->plot_pane.width()-1);
    plot_data->tick_labels_rect[BOTTOM].setHeight(h_label);

    //TOP

    plot_data->labels_rect[TOP].setX(plot_data->plot_pane.x());
    plot_data->labels_rect[TOP].setY(title_h+h_margin);

    plot_data->labels_rect[TOP].setHeight(h_label);
    plot_data->labels_rect[TOP].setWidth(plot_data->plot_pane.width()-1);

    plot_data->tick_labels_rect[TOP].setX(plot_data->plot_pane.x());
    plot_data->tick_labels_rect[TOP].setY(plot_data->plot_pane.y()-h_label_comp);

    plot_data->tick_labels_rect[TOP].setWidth(plot_data->plot_pane.width()-1);
    plot_data->tick_labels_rect[TOP].setHeight(h_label);
}

double LinReg(const QPointF &p1, const QPointF &p2, double eval)
{
    double m = (p2.y()-p1.y())/(p2.x()-p1.x());
    double b = p1.y()-m*p1.x();

    return (eval*m+b);
}

void TruncLogScale(PlotDataStruct *plot_data, GLfloat *pos,
                   int plot_index, int index)
{
    double x_bot = log10(plot_data->log_bottom_range[BOTTOM])-1.0;
    double y_bot = log10(plot_data->log_bottom_range[LEFT])-1.0;

    QPointF *data = plot_data->data[plot_index].data();
    bool *logplot = plot_data->logplot;

    int count = plot_data->data[plot_index].count();

    if (index < count-1)
    {
        if (data[index+1].x() < plot_data->log_bottom_range[BOTTOM] &&
                data[index+1].y() < plot_data->log_bottom_range[LEFT])
        {
            if (logplot[HORIZONTAL])
            {
                //TODO
            }
            else
            {

            }

            pos[index*2]   = x_bot;
            pos[index*2+1] = y_bot;
        }
        else if (data[index+1].x() < plot_data->log_bottom_range[BOTTOM])
        {
            pos[index*2]   = x_bot;
            pos[index*2+1] = data[index].y();
        }
        else
        {
            pos[index*2]   = data[index].x();
            pos[index*2+1] = y_bot;
        }
    }
    else
    {
        if (data[index-1].x() < plot_data->log_bottom_range[BOTTOM] &&
                data[index-1].y() < plot_data->log_bottom_range[LEFT])
        {
            pos[index*2]   = x_bot;
            pos[index*2+1] = y_bot;
        }
        else if (data[index-1].x() < plot_data->log_bottom_range[BOTTOM])
        {
            pos[index*2]   = x_bot;
            pos[index*2+1] = data[index].y();
        }
        else
        {
            pos[index*2]   = data[index].x();
            pos[index*2+1] = y_bot;
        }
    }
}

void SetDataPointsPosition(PlotDataStruct *plot_data, int plot_index)
{
    QPointF *data = plot_data->data[plot_index].data();
    bool *logplot = plot_data->logplot;

    int count = plot_data->data[plot_index].count();
    GLfloat *pos = new GLfloat[count*2];

    for (int i = 0; i < count; i++)
    {
        if (logplot[HORIZONTAL])
        {
            if (data[i].x() > 0)
            {
                pos[i*2] = log10(data[i].x());
            }
            else
            {
                TruncLogScale(plot_data,pos,
                              plot_index,i);
                continue;
            }
        }
        else
        {
            pos[i*2] = data[i].x();
        }

        if (logplot[VERTICAL])
        {
            if (data[i].y() > 0)
            {
                pos[i*2+1] = log10(data[i].y());
            }
            else
            {
                TruncLogScale(plot_data,pos,
                              plot_index,i);
            }
        }
        else
        {
            pos[i*2+1] = data[i].y();
        }
    }

    GLuint *index;
    int index_count;

    QOpenGLBuffer *pos_buffer = plot_data->data_pos_buffer.data();
    QOpenGLBuffer *index_buffer = plot_data->data_index_buffer.data();

    if (!count)
    {
        index = nullptr;
        index_count = 0;
    }
    else if (count < 2)
    {
        index = new GLuint[1];
        index[1] = 0;
        index_count = 1*sizeof(GLuint);
    }
    else
    {
        index = new GLuint[(count-1)*2];

        for (int i = 1; i < count-1; i++)
        {
            index[i*2-1] = i;
            index[i*2]   = i;
        }

        index[0] = 0;
        index[(count-1)*2-1] = count-1;
        index_count = (count-1)*2*sizeof(GLuint);
    }

    QOpenGLVertexArrayObject::Binder vao_binder(
                plot_data->data_vao[plot_index]);
    {
        plot_data->m_program.enableAttributeArray(plot_data->pos);

        pos_buffer[plot_index].bind();
        pos_buffer[plot_index].allocate(pos,2*count*sizeof(GLfloat));
        plot_data->m_program.setAttributeBuffer(plot_data->pos,
                                                GL_FLOAT,0,2);
        pos_buffer[plot_index].release();


        index_buffer[plot_index].bind();
        index_buffer[plot_index].allocate(index,index_count);
    }
    vao_binder.release();

    delete[] pos;
    delete[] index;
}

void SetLinGridPosition(PlotDataStruct *plot_data, int side)
{
    double bot = plot_data->bottom_range[side];
    double top = plot_data->top_range[side];

    double step = plot_data->tick_step[side];
    double rem = fabs(remainder(bot,step));

    double unit_step = step/(top-bot);
    double unit_rem = rem/(top-bot);

    int count = ceil((top-bot)/step);
    plot_data->ticks_count[side] = count;

    GLfloat pos[count*4];

    if (side == LEFT || side == RIGHT)
    {
        for (int i = 0; i < count; i++)
        {
            pos[i*4]   = 0;
            pos[i*4+1] = i*unit_step+unit_rem;
            pos[i*4+2] = 1;
            pos[i*4+3] = pos[i*4+1];
        }
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            pos[i*4]   = i*unit_step+unit_rem;
            pos[i*4+1] = 0;
            pos[i*4+2] = pos[i*4];
            pos[i*4+3] = 1;
        }
    }

    QOpenGLShaderProgram *m_program = &(plot_data->m_program);
    QOpenGLBuffer *grid_buffer      = &(plot_data->grid_buffer[side]);
    QOpenGLBuffer *sec_grid_buffer  = &(plot_data->sec_grid_buffer[side]);

    QOpenGLVertexArrayObject::Binder vao_binder(
                &(plot_data->grid_vao[side]));
    {
        m_program->enableAttributeArray(plot_data->pos);
        grid_buffer->bind();
        grid_buffer->allocate(pos,4*count*sizeof(GLfloat));
        m_program->setAttributeBuffer(plot_data->pos,GL_FLOAT,0,2);
        plot_data->grid_buffer[side].release();
    }
    vao_binder.release();

    int sec_count = plot_data->sec_ticks_count[side];
    double unit_sec_step = unit_step/(sec_count+1.0);

    uint total_sec_count = (count+2)*sec_count*4;
    plot_data->total_sec_ticks_count[side] = total_sec_count/2.0;
    GLfloat sec_pos[total_sec_count];

    double sec_step_val;

    if (side == LEFT || side == RIGHT)
    {
        for (int i = 0; i <= count+2; i++)
        {
            for (int j = 0; j <= sec_count; j++)
            {
                sec_step_val = unit_step*(i-1)+
                        (j+1)*unit_sec_step+unit_rem;

                sec_pos[(i*sec_count+j)*4]   = 0;
                sec_pos[(i*sec_count+j)*4+1] = sec_step_val;
                sec_pos[(i*sec_count+j)*4+2] = 1;
                sec_pos[(i*sec_count+j)*4+3] = sec_step_val;
            }
        }
    }
    else
    {
        for (int i = 0; i <= count+2; i++)
        {
            for (int j = 0; j <= sec_count; j++)
            {
                sec_step_val = unit_step*(i-1)+
                        (j+1)*unit_sec_step+unit_rem;

                sec_pos[(i*sec_count+j)*4]   = sec_step_val;
                sec_pos[(i*sec_count+j)*4+1] = 0;
                sec_pos[(i*sec_count+j)*4+2] = sec_step_val;
                sec_pos[(i*sec_count+j)*4+3] = 1;
            }
        }
    }

    QOpenGLVertexArrayObject::Binder sec_vao_binder(
                &(plot_data->sec_grid_vao[side]));
    {
        m_program->enableAttributeArray(plot_data->pos);
        sec_grid_buffer->bind();
        sec_grid_buffer->allocate(sec_pos,
                                  total_sec_count*sizeof(GLfloat));
        m_program->setAttributeBuffer(plot_data->pos,
                                                GL_FLOAT,0,2);
        sec_grid_buffer->release();
    }
    sec_vao_binder.release();
}

void SetLogGridPosition(PlotDataStruct *plot_data, int side)
{
    double bot = log10(plot_data->log_bottom_range[side]);
    double top = log10(plot_data->log_top_range[side]);

    double rem = fabs(remainder(bot,1));

    double unit_step = 1.0/(top-bot);
    double unit_rem = rem/(top-bot);

    int count = ceil(top-bot);
    plot_data->ticks_count[side] = count;

    GLfloat pos[4*count];
    double step_val;

    if (side == LEFT || side == RIGHT)
    {
        for (int i = 0; i < count; i++)
        {
            step_val = i*unit_step+unit_rem;

            pos[i*4]   = 0;
            pos[i*4+1] = step_val;
            pos[i*4+2] = 1;
            pos[i*4+3] = step_val;
        }
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            step_val = i*unit_step+unit_rem;

            pos[i*4]   = step_val;
            pos[i*4+1] = 0;
            pos[i*4+2] = step_val;
            pos[i*4+3] = 1;
        }
    }

    QOpenGLShaderProgram *m_program = &(plot_data->m_program);
    QOpenGLBuffer *grid_buffer      = &(plot_data->grid_buffer[side]);
    QOpenGLBuffer *sec_grid_buffer  = &(plot_data->sec_grid_buffer[side]);

    QOpenGLVertexArrayObject::Binder vao_binder(
                &(plot_data->grid_vao[side]));
    {
        m_program->enableAttributeArray(plot_data->pos);
        grid_buffer->bind();
        grid_buffer->allocate(pos,4*count*sizeof(GLfloat));
        m_program->setAttributeBuffer(plot_data->pos,
                                      GL_FLOAT,0,2);
        grid_buffer->release();
    }
    vao_binder.release();

    double log_sec_step[8];

    for (int i = 0; i < 8; i++)
    {
        log_sec_step[i] = unit_step*log10(i+2.0);
    }

    plot_data->total_sec_ticks_count[side] = 2*8*(count+2);

    GLfloat sec_pos[4*8*(count+2)];
    double sec_step_val;

    if (side == LEFT || side == RIGHT)
    {
        for (int i = 0; i < count+2; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                sec_step_val = (i-1)*unit_step+unit_rem+
                        log_sec_step[j];

                sec_pos[(i*8+j)*4]   = 0;
                sec_pos[(i*8+j)*4+1] = sec_step_val;
                sec_pos[(i*8+j)*4+2] = 1;
                sec_pos[(i*8+j)*4+3] = sec_step_val;
            }
        }
    }
    else
    {
        for (int i = 0; i < count+2; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                sec_step_val = (i-1)*unit_step+unit_rem+
                        log_sec_step[j];

                sec_pos[(i*8+j)*4]   = sec_step_val;
                sec_pos[(i*8+j)*4+1] = 0;
                sec_pos[(i*8+j)*4+2] = sec_step_val;
                sec_pos[(i*8+j)*4+3] = 1;
            }
        }
    }

    QOpenGLVertexArrayObject::Binder sec_vao_binder(
                &(plot_data->sec_grid_vao[side]));
    {
        m_program->enableAttributeArray(plot_data->pos);
        sec_grid_buffer->bind();
        sec_grid_buffer->allocate(sec_pos,4*8*(count+2)*
                                  sizeof(GLfloat));
        m_program->setAttributeBuffer(plot_data->pos,
                                      GL_FLOAT,0,2);
        sec_grid_buffer->release();
    }
    sec_vao_binder.release();
}

void SetGridPosition(PlotDataStruct *plot_data, int side)
{
    bool logplot;

    if (side == LEFT || side == RIGHT)
    {
        logplot = plot_data->logplot[VERTICAL];
    }
    else
    {
        logplot = plot_data->logplot[HORIZONTAL];
    }

    if (logplot)
    {
        SetLogGridPosition(plot_data,side);
    }
    else
    {
        SetLinGridPosition(plot_data,side);
    }
}

void SetGridPosition(PlotDataStruct *plot_data)
{
    for (int i = 0; i < 4; i++)
    {
        SetGridPosition(plot_data,i);
    }
}

void SetTickLabelsPositions(PlotDataStruct *plot_data, int side)
{
    int count;
    bool logplot;
    float delta;
    float rem, rel_rem;
    float screen_rem;

    float rel_step;
    float tick_h,tick_w,tick_step;

    float base_x = plot_data->tick_labels_rect[side].x();
    float base_y = plot_data->tick_labels_rect[side].y();
    float base_h = plot_data->tick_labels_rect[side].height();
    float base_w = plot_data->tick_labels_rect[side].width();

    if (side == LEFT || side == RIGHT)
    {
        logplot = plot_data->logplot[VERTICAL];
    }
    else
    {
        logplot = plot_data->logplot[HORIZONTAL];
    }

    if (logplot)
    {
        delta = log10((plot_data->log_top_range[side])/(
                                 plot_data->log_bottom_range[side]));
        count = floor(delta)+1;

        rem = remainder(log10(plot_data->log_bottom_range[side]),1.0);
        rel_rem = rem/delta;

        rel_step = 1.0/delta;
    }
    else
    {
        delta = plot_data->top_range[side]-plot_data->bottom_range[side];

        count = floor(delta/plot_data->tick_step[side])+1;

        rem = remainder(plot_data->bottom_range[side],plot_data->tick_step[side]);
        rel_rem = rem/(plot_data->top_range[side]-
                                  plot_data->bottom_range[side]);

        rel_step = plot_data->tick_step[side]/delta;
    }

    if (side == LEFT || side == RIGHT)
    {
        plot_data->tick_labels_rects[side].resize(count);
        screen_rem = rel_rem*base_h;
        tick_step = rel_step*base_h;
        tick_h = rel_step*base_h;

        if (plot_data->tick_labels_visible[TOP] ||
                plot_data->tick_labels_visible[BOTTOM])
        {
            int top_h = plot_data->tick_labels_rect[TOP].height();
            int bot_h = plot_data->tick_labels_rect[BOTTOM].height();

            int max_h = std::max<int>(top_h,bot_h);

            if (tick_h > max_h)
            {
                tick_h = plot_data->tick_labels_rect[TOP].height();
            }
        }

        QRect *rects = plot_data->tick_labels_rects[side].data();
        float step = base_y+base_h-screen_rem-0.5*tick_h;

        for (int i = 0; i < count; i++)
        {
            rects[i].setX(base_x);
            rects[i].setY(round(step));

            rects[i].setWidth(plot_data->tick_labels_rect[side].width());
            rects[i].setHeight(tick_h);

            step -= tick_step;
        }
    }
    else
    {
        plot_data->tick_labels_rects[side].resize(count);
        screen_rem = rel_rem*base_w;
        tick_step = rel_step*base_w;
        tick_w = rel_step*base_w;

        QRect *rects = plot_data->tick_labels_rects[side].data();

        float step = base_x+screen_rem-0.5*tick_w;

        for (int i = 0; i < count; i++)
        {
            rects[i].setX(round(step));
            rects[i].setY(base_y);

            rects[i].setHeight(base_h);
            rects[i].setWidth(tick_w);

            step += tick_step;
        }
    }
}

void SetTickLabelsPositions(PlotDataStruct *plot_data)
{
    for (int i = 0; i < 4; i++)
    {
        SetTickLabelsPositions(plot_data,i);
    }
}

QOpenGL2DPlot::QOpenGL2DPlot(QWidget *parent):
    QOpenGLWidget(parent)
{
    plot_data = new PlotDataStruct;

    plot_data->title          = DEFAULT_TITLE;
    plot_data->labels[BOTTOM] = DEFAULT_BOT_LABEL;
    plot_data->labels[TOP]    = DEFAULT_TOP_LABEL;
    plot_data->labels[LEFT]   = DEFAULT_LEFT_LABEL;
    plot_data->labels[RIGHT]  = DEFAULT_RIGHT_LABEL;

    plot_data->logplot[HORIZONTAL] = false;
    plot_data->logplot[VERTICAL]   = false;

    plot_data->title_visible = true;

    for (int i = 0; i < 4; i++)
    {
        bool t = (i%2 == 0);

        plot_data->labels_visible[i]      = t;
        plot_data->tick_labels_visible[i] = t;
        plot_data->ticks_visible[i]       = t;
        plot_data->sec_ticks_visible[i]   = t;
        plot_data->grid_visible[i]        = t;
        plot_data->sec_grid_visible[i]    = t;
    }

    plot_data->frame_color = DEFAULT_FRAME_COLOR;

    plot_data->frame_visible = true;
    plot_data->frame_pos_buffer = QOpenGLBuffer(
                QOpenGLBuffer::VertexBuffer);
    plot_data->frame_index_buffer = QOpenGLBuffer(
                QOpenGLBuffer::IndexBuffer);
    plot_data->m_program.setParent(this);

    for (int i = 0; i < 4; i++)
    {
        plot_data->grid_buffer[i] = QOpenGLBuffer(
                    QOpenGLBuffer::VertexBuffer);
        plot_data->sec_grid_buffer[i] = QOpenGLBuffer(
                    QOpenGLBuffer::VertexBuffer);

        plot_data->top_range[i]         = DEFAULT_TOP_RANGE;
        plot_data->bottom_range[i]      = DEFAULT_BOT_RANGE;

        plot_data->tick_step[i]         = DEFAULT_TICK_STEP;
        plot_data->sec_ticks_count[i]   = DEFAULT_SEC_TICK_COUNT;
        plot_data->grid_color[i]        = DEFAULT_GRID_COLOR;
        plot_data->sec_grid_color[i]    = DEFAULT_SEC_GRID_COLOR;

        plot_data->log_top_range[i]     = DEFAULT_LOG_TOP_RANGE;
        plot_data->log_bottom_range[i]  = DEFAULT_LOG_BOT_RANGE;
    }

    QSurfaceFormat newFormat;
    newFormat.setProfile(QSurfaceFormat::CoreProfile);
    newFormat.setSamples(16);
    newFormat.setSwapBehavior(QSurfaceFormat::TripleBuffer);
    setFormat(newFormat);
}

QOpenGL2DPlot::~QOpenGL2DPlot()
{
    plot_data->m_program.bind();
    plot_data->frame_pos_buffer.destroy();
    plot_data->frame_index_buffer.destroy();

    for (int i = 0; i < plot_data->data.count(); i++)
    {
        plot_data->data_pos_buffer[i].destroy();
        plot_data->data_index_buffer[i].destroy();
    }

    for (int i = 0; i < 4; i++)
    {
        plot_data->grid_buffer[i].destroy();
        plot_data->sec_grid_buffer[i].destroy();
    }

    plot_data->m_program.removeAllShaders();
    plot_data->m_program.release();

    delete plot_data->device;

    delete plot_data;
}

void DrawArrays(QOpenGLVertexArrayObject *vao,
                const QColor &color,
                GLsizei len, GLenum mode,
                PlotDataStruct *plot_data)
{
    QOpenGLVertexArrayObject::Binder vao_binder(vao);
    {
        plot_data->m_program.setUniformValue(plot_data->col,
                                             color);

        plot_data->functions->glDrawArrays(mode,0,len);
    }
    vao_binder.release();
}

void DrawElements(QOpenGLVertexArrayObject *vao,
                  const QColor &color,
                  GLsizei len, GLenum mode,
                  PlotDataStruct *plot_data)
{
    QOpenGLVertexArrayObject::Binder vao_binder(vao);
    {
        plot_data->m_program.setUniformValue(plot_data->col,
                                             color);

        plot_data->functions->glDrawElements(
                    mode,len,GL_UNSIGNED_INT,nullptr);
    }
    vao_binder.release();
}

void BufferAllocateSize(QOpenGLBuffer *pos,
                        QOpenGLBuffer *index,
                        int count)
{
    pos->create();
    pos->bind();
    pos->allocate(2*count*sizeof(GLfloat));
    pos->release();

    index->create();
    index->bind();
    index->allocate((2*count-2)*sizeof(GLuint));
    index->release();
}

void SetScales(PlotDataStruct *plot_data)
{
    if (plot_data->logplot[HORIZONTAL])
    {
        plot_data->x_scale =  2.0 / log10(plot_data->log_top_range[BOTTOM] /
                             plot_data->log_bottom_range[BOTTOM]);

        plot_data->x_offset = -log10(plot_data->log_bottom_range[BOTTOM]) *
                plot_data->x_scale;
    }
    else
    {
        plot_data->x_scale =  2.0 / (plot_data->top_range[BOTTOM] -
                          plot_data->bottom_range[BOTTOM]);

        plot_data->x_offset = -plot_data->bottom_range[BOTTOM] *
                plot_data->x_scale;
    }

    if (plot_data->logplot[VERTICAL])
    {
        plot_data->y_scale =  -2.0 / log10(plot_data->log_top_range[LEFT]/
                             plot_data->log_bottom_range[LEFT]);

        plot_data->y_offset = -log10(plot_data->log_bottom_range[LEFT]) *
                plot_data->y_scale;
    }
    else
    {
        plot_data->y_scale = -2.0 / (plot_data->top_range[LEFT] -
                          plot_data->bottom_range[LEFT]);

        plot_data->y_offset = -plot_data->bottom_range[LEFT] *
                plot_data->y_scale;
    }
}

void SetLogLabel(PlotDataStruct *plot_data, int side)
{
    int base  = ceil(log10(plot_data->log_bottom_range[side]));
    int delta = floor(log10((plot_data->log_top_range[side])/
                     (plot_data->log_bottom_range[side])));
    double num;

    for (int i = 0; i <= delta; i++)
    {
        num = pow(10,base+i);

        (plot_data->tick_labels[side]).append(QString::number(num));
    }
}

void SetLinLabel(PlotDataStruct *plot_data, int side)
{
    int base = ceil((plot_data->bottom_range[side])/
                    (plot_data->tick_step[side]));
    int delta = floor((plot_data->top_range[side]-plot_data->bottom_range[side])/
                      (plot_data->tick_step[side]));

    double num = base*(plot_data->tick_step[side]);

    for (int i = 0; i <= delta; i++)
    {
        (plot_data->tick_labels[side]).append(QString::number(num));

        num += plot_data->tick_step[side];
    }
}

void SetLabels(PlotDataStruct *plot_data, int side)
{
    plot_data->tick_labels[side].clear();

    if (side == LEFT || side == RIGHT)
    {
        if (plot_data->logplot[VERTICAL])
        {
            SetLogLabel(plot_data,side);
        }
        else
        {
            SetLinLabel(plot_data,side);
        }
    }
    else
    {
        if (plot_data->logplot[HORIZONTAL])
        {
            SetLogLabel(plot_data,side);
        }
        else
        {
            SetLinLabel(plot_data,side);
        }
    }
}

void SetLabels(PlotDataStruct *plot_data)
{
    for (int i = 0; i < 4; i++)
    {
        SetLabels(plot_data,i);
    }
}

void InitializeFrameData(PlotDataStruct *plot_data, const QRect &rect)
{
    plot_data->frame_vao.create();
    plot_data->frame_vao.bind();
    BufferAllocateSize(&(plot_data->frame_pos_buffer),
                       &(plot_data->frame_index_buffer),4);
    plot_data->frame_vao.release();

    GLuint frame_index[4] = {
        0,1,2,3
    };

    plot_data->frame_index_buffer.bind();
    plot_data->frame_index_buffer.write(0,frame_index,4*sizeof(GLuint));
    plot_data->frame_index_buffer.release();
    SetFrameSize(plot_data,rect);

    QOpenGLVertexArrayObject::Binder vao_binder(
                &(plot_data->frame_vao));
    {
        plot_data->m_program.enableAttributeArray(plot_data->pos);

        plot_data->frame_pos_buffer.bind();
        plot_data->m_program.setAttributeBuffer(plot_data->pos,GL_FLOAT,0,2);

        plot_data->frame_index_buffer.bind();
    }
    vao_binder.release();
}

void QOpenGL2DPlot::initializeGL()
{
    initializeOpenGLFunctions();

    plot_data->context = context();
    plot_data->functions = context()->functions();
    this->glClearColor(1,1,1,0);

    plot_data->m_program.addShaderFromSourceCode(
                QOpenGLShader::Vertex, vertexShaderSource);
    plot_data->m_program.addShaderFromSourceCode(
                QOpenGLShader::Fragment, vertexFragmentSource);
    plot_data->m_program.create();
    plot_data->m_program.link();
    plot_data->m_program.bindAttributeLocation("pos",0);
    plot_data->m_program.bind();

    plot_data->pos = plot_data->m_program.attributeLocation("pos");
    plot_data->col = plot_data->m_program.uniformLocation("col");
    plot_data->mat = plot_data->m_program.uniformLocation("matrix");

    InitializeFrameData(plot_data, rect());

    for (int i = 0; i < plot_data->data.count(); i++)
    {
        plot_data->data_vao[i]->create();

        BufferAllocateSize(&(plot_data->data_pos_buffer[i]),
                           &(plot_data->data_index_buffer[i]),
                           plot_data->data[i].count());
        SetDataPointsPosition(plot_data,i);
    }

    for (int i = 0; i < 4; i++)
    {
        plot_data->grid_vao[i].create();
        plot_data->grid_buffer[i].create();

        plot_data->sec_grid_vao[i].create();
        plot_data->sec_grid_buffer[i].create();
    }

    SetGridPosition(plot_data);

    plot_data->m_program.release();

    SetTickLabelsPositions(plot_data);
    SetScales(plot_data);
    SetLabels(plot_data);

    plot_data->device = new QOpenGLPaintDevice();
}

void SetFontRelativeSize(QPainter *painter, const QString &text,
                         const QRect &rect, float font_factor = 1.0)
{
    QFont font;
    QFontMetrics metrics(font);

    int size_h = rect.height();
    int size_w = floor(rect.width()/(text.length()));
    int size   = std::min<int>(size_h,size_w);

    float font_size = font_factor*metrics.height();

    if (size < font_size)
    {
        float rel_size = size/font_size;
        size = round(font.pointSize()*rel_size*font_factor);

        if (size <= 0)
        {
            size = 1;
        }

        font.setPointSize(size);
    }
    else
    {
        font.setPointSize(font.pointSize()*font_factor);
    }

    painter->setFont(font);
}

void SetFontRelativeSize(QPainter *painter, const QString &text,
                         const QRectF &rect, float font_factor = 1.0)
{
    SetFontRelativeSize(painter,text,rect.toRect(),font_factor);
}

void DrawTitle(PlotDataStruct *plot_data)
{
    if (!(plot_data->title_visible))
    {
        return;
    }

    SetFontRelativeSize(&(plot_data->painter),plot_data->title,
                        plot_data->title_rect,2.0);

    plot_data->painter.drawText(plot_data->title_rect,
                                plot_data->title,
                                QTextOption(Qt::AlignCenter));
}

void DrawLabels(PlotDataStruct *plot_data)
{
    int x_c;
    int y_c;

    QPainter *painter = &(plot_data->painter);

    if (plot_data->labels_visible[LEFT])
    {
        x_c = floor(plot_data->labels_rect[LEFT].topLeft().x()+
                    plot_data->viewport.width()/2.0);
        y_c = floor(plot_data->labels_rect[LEFT].topLeft().y()+
                    plot_data->viewport.height()/2.0);

        painter->translate(x_c,y_c);
        painter->rotate(-90);
        painter->translate(-plot_data->labels_rect[LEFT].topLeft().x()+
                          floor(plot_data->viewport.height()/2.0)-
                          plot_data->labels_rect[LEFT].width(),
                          -plot_data->labels_rect[LEFT].topLeft().y()-
                          floor(plot_data->viewport.width()/2.0));

        SetFontRelativeSize(painter,plot_data->labels[LEFT],
                            plot_data->labels_rect[LEFT]);

        painter->drawText(plot_data->labels_rect[LEFT],
                         plot_data->labels[LEFT],
                         QTextOption(Qt::AlignCenter));

        painter->resetTransform();
    }

    if (plot_data->labels_visible[RIGHT])
    {
        x_c = floor(plot_data->labels_rect[RIGHT].topLeft().x()+
                    plot_data->viewport.width()/2.0);
        y_c = floor(plot_data->labels_rect[RIGHT].topLeft().y()+
                    plot_data->viewport.height()/2.0);

        painter->translate(x_c,y_c);
        painter->rotate(-90);
        painter->translate(-plot_data->labels_rect[RIGHT].topRight().x()+
                          floor(plot_data->viewport.height()/2.0)-1,
                          -plot_data->labels_rect[RIGHT].topRight().y()-
                          floor(plot_data->viewport.width()/2.0));

        SetFontRelativeSize(painter,plot_data->labels[RIGHT],
                            plot_data->labels_rect[RIGHT]);

        painter->drawText(plot_data->labels_rect[RIGHT],
                          plot_data->labels[RIGHT],
                         QTextOption(Qt::AlignCenter));

        painter->resetTransform();
    }

    if (plot_data->labels_visible[BOTTOM])
    {
        painter->drawText(plot_data->labels_rect[BOTTOM],
                          plot_data->labels[BOTTOM],
                         QTextOption(Qt::AlignCenter));
    }

    if (plot_data->labels_visible[TOP])
    {
        painter->drawText(plot_data->labels_rect[TOP],
                          plot_data->labels[TOP],
                         QTextOption(Qt::AlignCenter));
    }

    //TICK LABELS

    int count;

    QTextOption text_opt[4];
    text_opt[BOTTOM] = QTextOption(Qt::AlignHCenter|Qt::AlignTop);
    text_opt[TOP]    = QTextOption(Qt::AlignHCenter|Qt::AlignBottom);
    text_opt[LEFT]   = QTextOption(Qt::AlignVCenter|Qt::AlignRight);
    text_opt[RIGHT]  = QTextOption(Qt::AlignVCenter|Qt::AlignLeft);

    for (int i = 0; i < 4; i++)
    {
        if (plot_data->tick_labels_visible[i])
        {
            QRect *tick_rects = plot_data->tick_labels_rects[i].data();
            count = plot_data->tick_labels_rects[i].count();
            QString *tick_labels = plot_data->tick_labels[i].data();

            for (int j = 0; j < count; j++)
            {
                SetFontRelativeSize(painter,tick_labels[j],
                                    tick_rects[i]);

                painter->drawText(tick_rects[j],tick_labels[j],
                                  text_opt[i]);
            }
        }
    }
}

void DrawFrame(PlotDataStruct *plot_data)
{
    plot_data->functions->glDisable(GL_MULTISAMPLE);

    if (plot_data->frame_visible)
    {
        plot_data->m_program.setUniformValue(plot_data->mat,
                                             plot_data->matrix);

        if (plot_data->frame_vao.isCreated())
        {
            DrawElements(&plot_data->frame_vao,
                     plot_data->frame_color,
                     4,GL_LINE_LOOP,plot_data);
        }
    }
}

void DrawGrid(PlotDataStruct *plot_data)
{
    int count;
    plot_data->m_program.setUniformValue(plot_data->mat,
                                         plot_data->grid_matrix);

    for (int i = 0; i < 4; i++)
    {
        if (plot_data->sec_grid_visible[i])
        {
            count = plot_data->total_sec_ticks_count[i];

            plot_data->m_program.setUniformValue(plot_data->col,
                                                 plot_data->sec_grid_color[i]);

            DrawArrays(&(plot_data->sec_grid_vao[i]),
                       plot_data->sec_grid_color[i],
                       count,GL_LINES,plot_data);
        }
    }

    for (int i = 0; i < 4; i++)
    {
        if (plot_data->grid_visible[i])
        {
            count = 2*plot_data->ticks_count[i];

            plot_data->m_program.setUniformValue(plot_data->col,
                                                 plot_data->grid_color[i]);

            DrawArrays(&(plot_data->grid_vao[i]),
                       plot_data->grid_color[i],
                       count,GL_LINES,plot_data);
        }
    }
}

void DrawData(PlotDataStruct *plot_data)
{
    plot_data->functions->glEnable(GL_MULTISAMPLE);
    plot_data->m_program.setUniformValue("matrix",
                                         plot_data->data_matrix);

    for (int i = 0; i < plot_data->data.size(); i++)
    {
        if (plot_data->data_visible[i])
        {
            DrawElements(plot_data->data_vao[i],
                         plot_data->data_color[i],
                         2*(plot_data->data[i].count()-1),
                         GL_LINES,plot_data);
        }
    }
    plot_data->functions->glDisable(GL_MULTISAMPLE);
}

void QOpenGL2DPlot::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    plot_data->painter.begin(this);

    DrawTitle(plot_data);       //SLOOOOOOW AS FUCK!!!!
    DrawLabels(plot_data);      //SLOOOOOOW AS FUCK!!!!

    plot_data->painter.beginNativePainting();
    plot_data->m_program.bind();

    glEnable(GL_SCISSOR_TEST);
    {
        glScissor(plot_data->plot_pane.x(),
                  rect().height()-
                  plot_data->plot_pane.bottomLeft().y(),
                  plot_data->plot_pane.width()-2,
                  plot_data->plot_pane.height()-2);

        DrawGrid(plot_data);
        DrawData(plot_data);
    }
    glDisable(GL_SCISSOR_TEST);

    DrawFrame(plot_data);

    plot_data->m_program.release();
    plot_data->painter.endNativePainting();
    plot_data->painter.end();

    glFinish();
    context()->swapBuffers(context()->surface());
    doneCurrent();
}

void SetProjectionMatrices(PlotDataStruct *plot_data,
                           const QRect &rect)
{
    plot_data->data_matrix.setToIdentity();
    plot_data->data_matrix.ortho(rect);
    plot_data->data_matrix.viewport(plot_data->plot_pane);

    plot_data->grid_matrix = plot_data->data_matrix;

    plot_data->data_matrix.translate(-1+plot_data->x_offset,
                                      1+plot_data->y_offset);
    plot_data->data_matrix.scale(plot_data->x_scale,
                                 plot_data->y_scale);

    plot_data->grid_matrix.translate(-1,1);
    plot_data->grid_matrix.scale(2,-2);
}

void QOpenGL2DPlot::resizeGL(int w,int h)
{
    QSize size(w,h);

    this->resize(size);
    plot_data->device->setSize(size);

    SetFrameSize(plot_data,rect());
    SetTickLabelsPositions(plot_data);
    SetProjectionMatrices(plot_data,rect());
}

void QOpenGL2DPlot::hideFrame(bool hide)
{
    plot_data->frame_visible = !hide;
}

void QOpenGL2DPlot::showFrame(bool show)
{
    hideFrame(!show);
}

bool QOpenGL2DPlot::isFrameVisible() const
{
    return plot_data->frame_visible;
}

bool QOpenGL2DPlot::isFrameHidden() const
{
    return !(plot_data->frame_visible);
}

void QOpenGL2DPlot::addPlot(int before)
{
    addPlots(1,before);
}

void QOpenGL2DPlot::addPlots(int count, int before)
{
    QVector<QVector<QPointF>> point_vector;
    point_vector.resize(count);

    addPlots(point_vector,before);
}

void QOpenGL2DPlot::addPlots(const QVector<QVector<QPointF>> data,
                             int before)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckPlotIndex(before,plot_data->data,error);
    ErrorHandle(error);
#endif

    int data_count = data.count();
    int it;

    if (plot_data->m_program.isLinked())
    {
        makeCurrent();
        plot_data->m_program.bind();
    }

    for (int i = 0; i < data_count; i++)
    {
        it = before+i;
        plot_data->data.insert(it,data[i]);
        plot_data->data_color.insert(it,DEFAULT_PLOT_COLOR);
        plot_data->data_visible.insert(it,DEFAULT_PLOT_VISIBLE);
        plot_data->data_pos_buffer.insert(
                    it,QOpenGLBuffer(QOpenGLBuffer::VertexBuffer));
        plot_data->data_index_buffer.insert(
                    it,QOpenGLBuffer(QOpenGLBuffer::IndexBuffer));
        plot_data->data_vao.insert(
                    it,new QOpenGLVertexArrayObject(this));

        if (plot_data->m_program.isLinked())
        {

            plot_data->data_vao[it]->create();

            QOpenGLVertexArrayObject::Binder vao_binder(
                        plot_data->data_vao[it]);
            {
                plot_data->data_pos_buffer[it].create();
                plot_data->data_index_buffer[it].create();
            }
            vao_binder.release();
        }
    }

    if (plot_data->m_program.isLinked())
    {
        plot_data->m_program.release();
        doneCurrent();
    }
}

void QOpenGL2DPlot::addPoint(int plot_index, const QPointF &point,
                             int pos)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckPlotIndex(plot_index,plot_data->data,error);
    ErrorHandle(error);
#endif

    plot_data->data[plot_index].insert(pos,point);
}

void QOpenGL2DPlot::addPoints(int plot_index,
                              const QVector<QPointF> &points, int pos)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckPlotIndex(plot_index,plot_data->data,error);
    ErrorHandle(error);
#endif

    int count = points.count();

    for (int i = 0; i < count; i++)
    {
        plot_data->data[plot_index].insert(pos,points[i]);
    }

    if (plot_data->m_program.isLinked())
    {
        SetDataPointsPosition(plot_data,plot_index);
    }
}

void QOpenGL2DPlot::setPoint(int plot_index, const QPointF &point,
                             int index)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckPlotIndex(plot_index,plot_data->data,error);
    CheckIndex(index,plot_data->data[plot_index],error);
    ErrorHandle(error);
#endif

    plot_data->data[plot_index][index] = point;
}

void QOpenGL2DPlot::setPoints(int plot_index,
                              const QVector<QPointF> &points, int index)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckPlotIndex(plot_index,plot_data->data,error);
    CheckIndex(index,plot_data->data[plot_index],error);
    ErrorHandle(error);
#endif

    int count = points.count();
    QPointF dummy;

    while (plot_data->data[plot_index].count() <
           index + count)
    {
        plot_data->data[plot_index].append(dummy);
    }

    for (int i = 0; i < count; i++)
    {
        plot_data->data[plot_index][index+i] = points[i];
    }
}

void SetPositions(PlotDataStruct *plot_data)
{
    if (plot_data->m_program.isLinked())
    {
        plot_data->context->makeCurrent(
                    plot_data->context->surface());
        plot_data->m_program.bind();

        SetGridPosition(plot_data);

        int count = plot_data->data.count();

        for (int i = 0; i < count; i++)
        {
            SetDataPointsPosition(plot_data,i);
        }

        plot_data->m_program.release();
        plot_data->context->doneCurrent();
    }
}

void QOpenGL2DPlot::setTopRange(Axis axis, double range)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckRange(range,plot_data->bottom_range[axis],error);
    ErrorHandle(error);
#endif

    plot_data->top_range[axis] = range;

    SetPositions(plot_data);
    SetScales(plot_data);
}

void QOpenGL2DPlot::setBottomRange(Axis axis, double range)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckRange(plot_data->top_range[axis],range,error);
    ErrorHandle(error);
#endif

    plot_data->bottom_range[axis] = range;

    SetPositions(plot_data);
    SetScales(plot_data);
}

void QOpenGL2DPlot::setRange(Axis axis, double top,
                             double bottom)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckRange(top,bottom,error);
    ErrorHandle(error);
#endif

    plot_data->top_range[axis] = top;
    plot_data->bottom_range[axis] = bottom;

    SetPositions(plot_data);
    SetScales(plot_data);
}

double QOpenGL2DPlot::TopRange(Axis axis) const
{
    return plot_data->top_range[axis];
}

double QOpenGL2DPlot::BottomRange(Axis axis) const
{
    return plot_data->bottom_range[axis];
}

void QOpenGL2DPlot::setLogTopRange(Axis axis, double range)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckLogRange(range,error);
    CheckRange(range,plot_data->bottom_range[axis],error);
    ErrorHandle(error);
#endif

    plot_data->log_top_range[axis] = range;

    SetPositions(plot_data);
    SetScales(plot_data);
}

void QOpenGL2DPlot::setLogBottomRange(Axis axis, double range)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckLogRange(range,error);
    CheckRange(plot_data->top_range[axis],range,error);
    ErrorHandle(error);
#endif

    plot_data->log_bottom_range[axis] = range;

    SetPositions(plot_data);
    SetScales(plot_data);
}

void QOpenGL2DPlot::setLogRange(Axis axis, double top, double bottom)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckLogRange(top,error);
    CheckLogRange(bottom,error);
    CheckRange(top,bottom,error);
    ErrorHandle(error);
#endif

    plot_data->log_top_range[axis]    = top;
    plot_data->log_bottom_range[axis] = bottom;

    SetPositions(plot_data);
    SetScales(plot_data);
}

double QOpenGL2DPlot::LogTopRange(Axis axis) const
{
    return plot_data->log_top_range[axis];
}
double QOpenGL2DPlot::LogBottomRange(Axis axis) const
{
    return plot_data->log_bottom_range[axis];
}

void QOpenGL2DPlot::setPlotColor(int plot_index, const QColor &color)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckPlotIndex(plot_index,plot_data->data,error);
    ErrorHandle(error);
#endif

    plot_data->data_color[plot_index] = color;
}

void QOpenGL2DPlot::showPlot(int plot_index, bool show)
{
#ifdef QT_DEBUG
    Error error = NO_ERRORS;
    CheckPlotIndex(plot_index,plot_data->data,error);
    ErrorHandle(error);
#endif

    plot_data->data_visible[plot_index] = show;
}

void QOpenGL2DPlot::hidePlot(int plot_index, bool hide)
{
    showPlot(plot_index, !hide);
}

void QOpenGL2DPlot::hideLabel(Axis axis, bool hide)
{
    plot_data->labels_visible[axis] = !hide;

    if (plot_data->m_program.isLinked())
    {
        SetFrameSize(plot_data,rect());
    }
}

void QOpenGL2DPlot::showLabel(Axis axis, bool show)
{
    hideLabel(axis,!show);
}

bool QOpenGL2DPlot::isLabelVisible(Axis axis) const
{
    return plot_data->labels_visible[axis];
}

bool QOpenGL2DPlot::isLabelHidden(Axis axis) const
{
    return !(isLabelVisible(axis));
}

void Redraw(QOpenGLWidget *parent, PlotDataStruct *plot_data)
{
    if (plot_data->m_program.isLinked())
    {
        parent->makeCurrent();
        {
            plot_data->m_program.bind();
            SetFrameSize(plot_data,parent->rect());
            SetScales(plot_data);
            SetProjectionMatrices(plot_data,parent->rect());
            SetPositions(plot_data);
            SetTickLabelsPositions(plot_data);
            SetLabels(plot_data);
            plot_data->m_program.release();
        }
        parent->doneCurrent();
        parent->repaint();
    }
}

void QOpenGL2DPlot::repaint()
{
    if (plot_data->m_program.isLinked())
    {
        int len = plot_data->data.length();

        for (int i = 0; i < len; i++)
        {
            SetDataPointsPosition(plot_data,i);
        }
        Redraw(this,plot_data);
    }
}


void QOpenGL2DPlot::setLogScale(Direction Direction,
                                bool logscale)
{
    plot_data->logplot[Direction] = logscale;
    Redraw(this,plot_data);
}

void QOpenGL2DPlot::setLinearScale(Direction Direction,
                                   bool linscale)
{
    setLogScale(Direction,!linscale);
}

bool QOpenGL2DPlot::isLogScale(Direction Direction) const
{
    return plot_data->logplot[Direction];
}

bool QOpenGL2DPlot::isLinearScale(Direction Direction) const
{
    return !isLogScale(Direction);
}

void QOpenGL2DPlot::hideTickLabel(Axis axis, bool hide)
{
    showTickLabel(axis,!hide);
}

void QOpenGL2DPlot::showTitle(bool show)
{
    plot_data->title_visible = show;
    Redraw(this,plot_data);
}

void QOpenGL2DPlot::hideTitle(bool hide)
{
    showTitle(!hide);
}

bool QOpenGL2DPlot::isTitleVisible() const
{
    return plot_data->title_visible;
}

void QOpenGL2DPlot::showTickLabel(Axis axis, bool show)
{
    plot_data->tick_labels_visible[axis] = show;
    Redraw(this,plot_data);
}

bool QOpenGL2DPlot::isTickLabelVisible(Axis axis) const
{
    return plot_data->tick_labels_visible[axis];
}
bool QOpenGL2DPlot::isTickLabelHidden(Axis axis) const
{
    return !isTickLabelVisible(axis);
}

void QOpenGL2DPlot::setTickStep(Axis axis, double step)
{
    plot_data->tick_step[axis] = step;

    uint count = floor((plot_data->top_range-
                  plot_data->bottom_range)/step);
    plot_data->ticks_count[axis] = count;

    Redraw(this,plot_data);
}

void QOpenGL2DPlot::setTicks(Axis axis, uint amount)
{
    double step = plot_data->top_range[axis]-
            plot_data->bottom_range[axis];
    step /= amount;

    setTickStep(axis,step);
}

void QOpenGL2DPlot::setSecTicks(Axis axis, uint amount)
{
    plot_data->sec_ticks_count[axis] = amount;

    Redraw(this,plot_data);
}

double QOpenGL2DPlot::TickStep(Axis axis) const
{
    return plot_data->tick_step[axis];
}

uint QOpenGL2DPlot::Ticks(Axis axis) const
{
    double range = TopRange(axis)-BottomRange(axis);

    return floor(plot_data->tick_step[axis]/range);
}

uint QOpenGL2DPlot::SecTicks(Axis axis) const
{
    return plot_data->sec_ticks_count[axis];
}

void QOpenGL2DPlot::showGridLines(Axis axis, bool show)
{
    plot_data->grid_visible[axis] = show;
}

void QOpenGL2DPlot::hideGridLines(Axis axis, bool hide)
{
    showGridLines(axis,!hide);
}

void QOpenGL2DPlot::showTicks(Axis axis, bool show)
{
    plot_data->ticks_visible[axis] = show;
}

void QOpenGL2DPlot::hideTicks(Axis axis, bool hide)
{
    showTicks(axis,!hide);
}

void QOpenGL2DPlot::showSecGridLines(Axis axis, bool show)
{
    plot_data->sec_grid_visible[axis] = show;
}

void QOpenGL2DPlot::hideSecGridLines(Axis axis, bool hide)
{
    showSecGridLines(axis,!hide);
}

void QOpenGL2DPlot::showSecTicks(Axis axis, bool show)
{
    plot_data->sec_ticks_visible[axis] = show;
}

void QOpenGL2DPlot::hideSecTicks(Axis axis, bool hide)
{
    showSecTicks(axis,!hide);
}

bool QOpenGL2DPlot::areGridLinesVisible(Axis axis) const
{
    return plot_data->grid_visible[axis];
}

bool QOpenGL2DPlot::areGridLinesHidden(Axis axis) const
{
    return !areGridLinesVisible(axis);
}

void QOpenGL2DPlot::setGridColor(Axis axis,
                                 const QColor &color)
{
    plot_data->grid_color[axis] = color;
}

void QOpenGL2DPlot::setGridColor(const QColor &color)
{
    for (int i = 0; i < 4; i++)
    {
        setGridColor(static_cast<Axis>(i),color);
    }
}

QColor QOpenGL2DPlot::GridColor(Axis axis) const
{
    return plot_data->grid_color[axis];
}

void QOpenGL2DPlot::setSecGridColor(Axis axis,
                                    const QColor &color)
{
    plot_data->sec_grid_color[axis] = color;
}

void QOpenGL2DPlot::setSecGridColor(const QColor &color)
{
    for (int i = 0; i < 4; i++)
    {
        setSecGridColor(static_cast<Axis>(i),color);
    }
}

QColor QOpenGL2DPlot::SecGridColor(Axis axis) const
{
    return plot_data->sec_grid_color[axis];
}

void DrawDataPainter(PlotDataStruct *plot_data)
{
    QPainter *painter = &(plot_data->painter);

    painter->resetTransform();
    {
        QMatrix4x4 mat;
        mat.setToIdentity();

        int count1 = plot_data->data.count();
        int count2;

        double top_x = plot_data->top_range[BOTTOM];
        double top_y = plot_data->top_range[LEFT];

        double bot_x = plot_data->bottom_range[BOTTOM];
        double bot_y = plot_data->bottom_range[LEFT];

        double delta_x = top_x-bot_x;
        double delta_y = top_y-bot_y;

        double x_offset = (bot_x/delta_x)*plot_data->plot_pane.width();
        double y_offset = (bot_y/delta_y)*plot_data->plot_pane.height();
        y_offset += plot_data->plot_pane.height();

        QRect viewport = plot_data->plot_pane;
        viewport.translate(-x_offset,y_offset);

        painter->setViewport(viewport);

        double x_scale = viewport.width()/delta_x;
        double y_scale = viewport.height()/delta_y;
        mat.scale(x_scale,-y_scale);

        for (int i = 0; i < count1; i++)
        {
            painter->setPen(plot_data->data_color.at(i));

            count2 = plot_data->data[i].count();
            count2--;
            QPointF *pts = plot_data->data[i].data();
            QPointF pt1,pt2;

            for (int j = 0; j < count2; j++)
            {
                pt1 = pts[j]*mat;
                pt2 = pts[j+1]*mat;

                painter->drawLine(pt1,pt2);
            }
        }
    }
    painter->resetTransform();
}

void DrawFramePainter(PlotDataStruct *plot_data)
{
    QPainter *painter = &(plot_data->painter);

    painter->resetTransform();
    {
        painter->setPen(plot_data->frame_color);
        painter->drawRect(plot_data->plot_pane);
    }
    painter->resetTransform();
}

void DrawGridPainter(PlotDataStruct *plot_data)
{
}

void QOpenGL2DPlot::SaveSVG(const QString &fileName,
                            const QString &description)
{
    QSvgGenerator generator;
    generator.setFileName(fileName);
    generator.setDescription(description);
    generator.setSize(this->size());
    generator.setViewBox(this->rect());

    QPainter *painter = &(plot_data->painter);
    painter->begin(&generator);
    {
        DrawDataPainter(plot_data);
        DrawFramePainter(plot_data);

        DrawTitle(plot_data);
        DrawLabels(plot_data);
    }
    painter->end();
}
