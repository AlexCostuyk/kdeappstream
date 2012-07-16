#include "JSONBuilder.h"

#include "paint/PaintDevice.h"
#include "paint/PaintEngine.h"
#include "websocket/QWsSocket.h"

#include <QString>
#include <QWidget>
#include <QTimer>
#include <QDebug>
#include <QByteArray>
#include <QBuffer>
#include <QLocalSocket>
#include <QCoreApplication>

using namespace KAppStream;

JSONBuilder * JSONBuilder::m_instance = 0;

JSONBuilder::JSONBuilder(QObject * parent) :
    QObject(parent),
    _sem(1)
{
    buffer.reserve(10000);
}

JSONBuilder * JSONBuilder::instance(QObject * parent)
{
    if (!m_instance)
        m_instance = new JSONBuilder(parent);
    return m_instance;
}

void JSONBuilder::beginRender(QWidget * widget)
{
    if (!widget)
        return;

    QSize s = widget->size();
    QPoint p = widget->pos();
    if (!widget->parentWidget())
        p = QPoint(0,0);
    else if (widget->windowType() & Qt::Window)
        p = widget->parentWidget()->mapFromGlobal(p);

    context << widget;
    buffer.append("{\"command\":\"draw\"")
          .append(",\"widget\":{")
          .append("\"id\":").append(QString::number((long long)widget).toAscii())
          .append(",\"name\":\"").append(widget->metaObject()->className()).append("\"")
          .append(",\"x\":").append(QString::number( p.x() ).toAscii())
          .append(",\"y\":").append(QString::number( p.y() ).toAscii())
          .append(",\"w\":").append(QString::number( s.width() ).toAscii())
          .append(",\"h\":").append(QString::number( s.height() ).toAscii())
          .append("},\"render\":[");
}

void JSONBuilder::endRender()
{
    if (!context.length())
        return;
    context.removeFirst();
    if (buffer[buffer.length()-1] == ',')
        buffer.remove(buffer.length()-1, 1);
    buffer.append("]},");
}

void JSONBuilder::addChild(QWidget * child, QWidget * parent)
{
    _sem.acquire();
    buffer.append("{\"command\":\"addChild\"")
          .append(",\"id\":").append(QString::number((long long)parent).toAscii())
          .append(",\"child\":").append(QString::number((long long)child).toAscii())
          .append("},");
    _sem.release();
}

void JSONBuilder::removeChild(QWidget * child, QWidget * parent)
{
    _sem.acquire();
    buffer.append("{\"command\":\"removeChild\"")
          .append(",\"id\":").append(QString::number((long long)parent).toAscii())
          .append(",\"child\":").append(QString::number((long long)child).toAscii())
          .append("},");
    _sem.release();
}

void JSONBuilder::finish()
{
    emit readyRead();
}

void JSONBuilder::flush(QWsSocket * device)
{
    _sem.acquire();
    if (device)
    {
        if (buffer.length())
        {
            if (buffer[buffer.length()-1] == ',')
                buffer.remove(buffer.length()-1, 1);
            device->write(QString('[' + buffer + ']'));
            device->waitForBytesWritten(1000);
        }
    }
    buffer.clear();
    _sem.release();
}

void JSONBuilder::ellipse(const QRect & r)
{
    _sem.acquire();
    this->saveStatePriv();
    buffer.append("{\"t\":\"ellipse\"")
          .append(",\"x\":").append(QString::number(r.x()))
          .append(",\"y\":").append(QString::number(r.y()))
          .append(",\"w\":").append(QString::number(r.width()))
          .append(",\"h\":").append(QString::number(r.height()))
          .append("},");
    _sem.release();
}

void JSONBuilder::ellipse(const QRectF & r)
{
    _sem.acquire();
    this->saveStatePriv();
    buffer.append("{\"t\":\"ellipse\"")
          .append(",\"x\":").append(QString::number(r.x()))
          .append(",\"y\":").append(QString::number(r.y()))
          .append(",\"w\":").append(QString::number(r.width()))
          .append(",\"h\":").append(QString::number(r.height()))
          .append("},");
    _sem.release();
}

void JSONBuilder::image(const QImage & i)
{
    _sem.acquire();
    this->saveStatePriv();
    QByteArray byteArray;
    QBuffer buf(&byteArray);
    i.save(&buf, "PNG");
    buffer.append("{\"t\":\"image\"")
          .append(",\"data:image/png;base64,")
          .append(byteArray.toBase64())
          .append("\"},");
    _sem.release();
}

void JSONBuilder::line(const QLine & l)
{
    _sem.acquire();
    this->saveStatePriv();
    buffer.append("{\"t\":\"line\"")
          .append(",\"xs\":").append(QString::number(l.x1()))
          .append(",\"ys\":").append(QString::number(l.y1()))
          .append(",\"xe\":").append(QString::number(l.x2()))
          .append(",\"ye\":").append(QString::number(l.y2()))
          .append("},");
    _sem.release();
}

void JSONBuilder::line(const QLineF & l)
{
    _sem.acquire();
    this->saveStatePriv();
    buffer.append("{\"t\":\"line\"")
          .append(",\"xs\":").append(QString::number(l.x1()))
          .append(",\"ys\":").append(QString::number(l.y1()))
          .append(",\"xe\":").append(QString::number(l.x2()))
          .append(",\"ye\":").append(QString::number(l.y2()))
          .append("},");
    _sem.release();
}

void JSONBuilder::pixmap(const QRectF & r, const QPixmap & pm, const QRectF & sr)
{
    _sem.acquire();
    this->saveStatePriv();
    QByteArray byteArray;
    QBuffer buf(&byteArray);
    pm.copy(sr.toRect()).save(&buf, "PNG");
    buffer.append("{\"t\":\"pixmap\"")
          .append(",\"x\":").append(QString::number(r.x()))
          .append(",\"y\":").append(QString::number(r.y()))
          .append(",\"w\":").append(QString::number(r.width()))
          .append(",\"h\":").append(QString::number(r.height()))
          .append(",\"data\":\"data:image/png;base64,").append(byteArray.toBase64())
          .append("\"},");
    _sem.release();
}

void JSONBuilder::pixmap(const QPixmap & pm)
{
    _sem.acquire();
    this->saveStatePriv();
    QByteArray byteArray;
    QBuffer buf(&byteArray);
    pm.save(&buf, "PNG");
    buffer.append("{\"t\":\"pixmap\"")
          .append(",\"data\":\"data:image/png;base64,")
          .append(byteArray.toBase64())
          .append("\"},");
    _sem.release();
}

void JSONBuilder::rect(const QRect & r)
{
    _sem.acquire();
    this->saveStatePriv();
    buffer.append("{\"t\":\"rect\"")
          .append(",\"x\":").append(QString::number(r.x()))
          .append(",\"y\":").append(QString::number(r.y()))
          .append(",\"w\":").append(QString::number(r.width()))
          .append(",\"h\":").append(QString::number(r.height()))
          .append("},");
    _sem.release();
}

void JSONBuilder::rect(const QRectF & r)
{
    _sem.acquire();
    this->saveStatePriv();
    buffer.append("{\"t\":\"rect\"")
          .append(",\"x\":").append(QString::number(r.x()))
          .append(",\"y\":").append(QString::number(r.y()))
          .append(",\"w\":").append(QString::number(r.width()))
          .append(",\"h\":").append(QString::number(r.height()))
          .append("},");
    _sem.release();
}

void JSONBuilder::resize(QWidget * w, const QSize & oldSize, const QSize & newSize)
{
    if (w->windowType() & Qt::Window)
    {
        buffer.append("{\"t\":\"resize\"")
              .append(",\"id\":").append(QString::number((long long)w).toAscii())
              .append(",\"old\":")
                .append("{\"w\":").append(QString::number(oldSize.width()).toAscii())
                .append(",\"h\":").append(QString::number(oldSize.height()).toAscii())
              .append("},\"new\":")
                .append("{\"w\":").append(QString::number(oldSize.width()).toAscii())
                .append(",\"h\":").append(QString::number(oldSize.height()).toAscii())
              .append("}},");
    }
}

void JSONBuilder::state(const QPaintEngineState & s)
{
    _sem.acquire();
    QPaintEngine::DirtyFlags f = s.state();
    if (f & QPaintEngine::DirtyBackground)
        cur_state.backgroundBrush = s.backgroundBrush();
    if (f & QPaintEngine::DirtyBackgroundMode)
        cur_state.backgroundMode = s.backgroundMode();
    if (f & QPaintEngine::DirtyBrush)
        cur_state.brush = s.brush();
    if (f & QPaintEngine::DirtyBrushOrigin)
        cur_state.brushOrigin = s.brushOrigin();
    if (f & QPaintEngine::DirtyClipPath || f & QPaintEngine::DirtyClipRegion)
        cur_state.clipOperation = s.clipOperation();
    if (f & QPaintEngine::DirtyClipPath)
        cur_state.clipPath = s.clipPath();
    if (f & QPaintEngine::DirtyClipRegion)
        cur_state.clipRegion = s.clipRegion();
    if (f & QPaintEngine::DirtyCompositionMode)
        cur_state.compositionMode = s.compositionMode();
    if (f & QPaintEngine::DirtyFont)
        cur_state.font = s.font();
    if (f & QPaintEngine::DirtyClipEnabled)
        cur_state.isClipEnabled = s.isClipEnabled();
    if (f & QPaintEngine::DirtyOpacity)
        cur_state.opacity = s.opacity();
    if (f & QPaintEngine::DirtyPen)
        cur_state.pen = s.pen();
    if (f & QPaintEngine::DirtyHints)
        cur_state.renderHints = s.renderHints();
    if (f & QPaintEngine::DirtyTransform)
        cur_state.transform = s.transform();
    cur_state.state |= f;
    _sem.release();
}

void JSONBuilder::saveState()
{
    _sem.acquire();
    saveStatePriv();
    _sem.release();
}

void JSONBuilder::saveStatePriv()
{
    QPaintEngine::DirtyFlags f = cur_state.state;
    if (f & QPaintEngine::AllDirty)
    {
        buffer.append("{\"t\":\"state\"")
              .append(",\"data\":{");

        if (f & QPaintEngine::DirtyPen)
        {
            pen(cur_state.pen);
            buffer.append(",");
        }

        if (f & QPaintEngine::DirtyBrush)
        {
            brush(cur_state.brush);
            buffer.append(",");
        }

        if (f & QPaintEngine::DirtyFont)
        {
            font(cur_state.font);
            buffer.append(",");
        }

        if (f & QPaintEngine::DirtyBrushOrigin)
        {
            buffer.append("\"origin\":{\"x\":").append(QString::number(cur_state.brushOrigin.x()))
                  .append(",\"y\":").append(QString::number(cur_state.brushOrigin.y()))
                  .append("},");
        }

        if (f & QPaintEngine::DirtyTransform)
        {
            transform(cur_state.transform);
            buffer.append(",");
        }

//        if (f & QPaintEngine::DirtyCompositionMode)
//        {
//            buffer.append("\"mode\":").append(QString::number(cur_state.compositionMode));
//            buffer.append(",");
//        }

        if (f & (QPaintEngine::DirtyBackground | QPaintEngine::DirtyBackgroundMode))
        {
            buffer.append("\"bg\":{");
//            if (f & QPaintEngine::DirtyBackground)
//            {
//                brush(cur_state.backgroundBrush);
//                buffer.append(",");
//            }
//            if (f & QPaintEngine::DirtyBackgroundMode)
//                buffer.append("\"mode\":").append(QString::number(cur_state.backgroundMode));
//            if (buffer[buffer.length()-1] == ',')
//            {
//                buffer[buffer.length()-1] = '}';
//                buffer.append(',');
//            }
            buffer.append("},");
        }

        if (f & QPaintEngine::DirtyHints)
        {
            buffer.append("\"hints\":{");
            buffer += "\"aliasing\":";                   buffer += QString::number(cur_state.renderHints & QPainter::Antialiasing);
            buffer += ",\"textAliasing\":";              buffer += QString::number(cur_state.renderHints & QPainter::TextAntialiasing);
            buffer += ",\"smoothPixmapTransform\":";     buffer += QString::number(cur_state.renderHints & QPainter::SmoothPixmapTransform);
            buffer += ",\"highQualityAntialiasing\":";   buffer += QString::number(cur_state.renderHints & QPainter::HighQualityAntialiasing);
            buffer += ",\"nonCosmeticDefaultPen\":";     buffer += QString::number(cur_state.renderHints & QPainter::NonCosmeticDefaultPen);
            buffer.append("},");
        }

        if (f & QPaintEngine::DirtyOpacity)
        {
            buffer.append("\"opacity\":")
                  .append(QString::number(cur_state.opacity))
                  .append(',');
        }


//        if (f & QPaintEngine::DirtyClipRegion);
//        if (f & QPaintEngine::DirtyClipPath);
//        if (f & QPaintEngine::DirtyClipEnabled);


        if (buffer[buffer.length()-1] == ',')
            buffer.remove(buffer.length()-1, 1);
        buffer.append("}},");

        cur_state.state = 0;
    }
}

////////////// HELPER FUNCTIONS BELOW - NOT COLON SEPARATED /////////////////

void JSONBuilder::color(const QColor & c)
{
    buffer.append("\"color\":\"")
          .append(c.name())
          .append("\"");
}

void JSONBuilder::font(const QFont & f)
{
    buffer.append("\"font\":{")
          .append("}");
}

void JSONBuilder::pen(const QPen & p)
{
    buffer.append("\"pen\":{");

    // If brush defined (instead color)
    QBrush b = p.brush();
    if (b.style() != Qt::NoBrush && b.style() != Qt::SolidPattern)
        brush(p.brush());
    else
        color(p.color());

    // If Penstyle different than SolidLine
    if (!p.isSolid())
    {
        buffer.append(",\"style\":").append(QString::number(p.style()));
        buffer.append(",\"cap\":").append(QString::number(p.capStyle()));
        buffer.append(",\"pattern\":[");
        foreach (qreal d, p.dashPattern())
            buffer.append(QString::number(d)).append(',');
        if (buffer[buffer.length()-1] == ',')
            buffer[buffer.length()-1] = ']';
        else
            buffer.append("]");
        buffer.append(",\"offset\":").append(QString::number(p.dashOffset()));
    }

    buffer.append(",\"join\":").append(QString::number(p.joinStyle()));
    buffer.append(",\"miter\":").append(QString::number(p.miterLimit()));
    buffer.append(",\"width\":").append(QString::number(p.width()));
    buffer.append('}');
}

void JSONBuilder::brush(const QBrush & b)
{

    buffer.append("\"brush\":{")
          .append("\"t\":").append(QString::number(b.style()))
          .append(",");

    switch (b.style())
    {
        case Qt::NoBrush:
            break;
        case Qt::SolidPattern:
            color(b.color());
            break;
        case Qt::LinearGradientPattern:
        case Qt::RadialGradientPattern:
        case Qt::ConicalGradientPattern:
            if (b.gradient())
                gradient(*(b.gradient()));
            break;
        case Qt::TexturePattern:
            {
                QImage img = b.textureImage();
                if (!img.isNull())
                {
                    QByteArray byteArray;
                    QBuffer buf(&byteArray);
                    img.save(&buf, "PNG");
                    buffer.append("\"image\":\"data:image/png;base64,").append(byteArray.toBase64()).append("\"");
                }
                else
                {
                    QPixmap pix = b.texture();
                    if (!pix.isNull())
                    {
                        QByteArray byteArray;
                        QBuffer buf(&byteArray);
                        pix.save(&buf, "PNG");
                        buffer.append("\"pixmap\":{ \"data\":\"data:image/png;base64,").append(byteArray.toBase64()).append("\"}");
                    }
                }
            }
            break;
        default:
            color(b.color());
            break;
    }

    if (b.style() != Qt::NoBrush && !b.transform().isIdentity())
        transform(b.transform());

    if (buffer[buffer.length()-1] == ',')
        buffer.remove(buffer.length()-1, 1);

    buffer.append('}');
}

void JSONBuilder::gradient(const QGradient & g)
{
    buffer.append("\"gradient\":{\"type\":").append(QString::number(g.type()));
    buffer.append(",\"stops\":[");
    foreach (QGradientStop stop, g.stops())
        buffer.append("[").append(QString::number(stop.first)).append(",").append(stop.second.name()).append("],");
    if (buffer[buffer.length()-1] == ',')
        buffer.remove(buffer.length()-1, 1);
    buffer.append("]}");
}

void JSONBuilder::transform(const QTransform & t)
{
    buffer.append("\"transform\":[[")
          .append(QString::number(t.m11())).append(",")
          .append(QString::number(t.m12())).append(",")
          .append(QString::number(t.m13())).append("],[")
          .append(QString::number(t.m21())).append(",")
          .append(QString::number(t.m22())).append(",")
          .append(QString::number(t.m23())).append("],[")
          .append(QString::number(t.m31())).append(",")
          .append(QString::number(t.m32())).append(",")
            .append(QString::number(t.m33())).append("]]");
}
