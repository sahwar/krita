/*
 *  copyright (c) 2002 patrick julien <freak@codepimps.org>
 *
 *  this program is free software; you can redistribute it and/or modify
 *  it under the terms of the gnu general public license as published by
 *  the free software foundation; either version 2 of the license, or
 *  (at your option) any later version.
 *
 *  this program is distributed in the hope that it will be useful,
 *  but without any warranty; without even the implied warranty of
 *  merchantability or fitness for a particular purpose.  see the
 *  gnu general public license for more details.
 *
 *  you should have received a copy of the gnu general public license
 *  along with this program; if not, write to the free software
 *  foundation, inc., 675 mass ave, cambridge, ma 02139, usa.
 */
#ifndef KIS_PAINT_DEVICE_IMPL_H_
#define KIS_PAINT_DEVICE_IMPL_H_

#include <qcolor.h>
#include <qobject.h>
#include <qpixmap.h>
#include <qrect.h>
#include <qvaluelist.h>
#include <qstring.h>

#include "kdebug.h"
#include "kis_global.h"
#include "kis_types.h"
#include "kis_image.h"
#include "kis_datamanager.h"
#include "kis_colorspace.h"
#include "kis_canvas_controller.h"
#include "kis_color.h"
#include "kis_paint_device.h"

#include <koffice_export.h>

class DCOPObject;

class QImage;
class QSize;
class QPoint;
class KoStore;
class KisImage;
class QWMatrix;
class KisRectIteratorPixel;
class KisVLineIteratorPixel;
class KisHLineIteratorPixel;
class KNamedCommand;
class KisRotateVisitor;
class KisScaleVisitor;
class KisFilterStrategy;


/**
 * Class modelled on QPaintDevice.
 */
class KRITACORE_EXPORT KisPaintDeviceImpl
    : public QObject
    , public KisPaintDevice
    , public KShared
{

        Q_OBJECT

public:
    KisPaintDeviceImpl(KisColorSpace * colorSpace, const QString& name);

    KisPaintDeviceImpl(KisImage *img,  KisColorSpace * colorSpace, const QString& name);

    KisPaintDeviceImpl(const KisPaintDeviceImpl& rhs);
    virtual ~KisPaintDeviceImpl();
    virtual DCOPObject *dcopObject();

public:

    virtual bool write(KoStore *store);
    virtual bool read(KoStore *store);

public:

    virtual void move(Q_INT32 x, Q_INT32 y);
    virtual void move(const QPoint& pt);
    virtual KNamedCommand * moveCommand(Q_INT32 x, Q_INT32 y);

    virtual const bool visible() const;
    virtual void setVisible(bool v);
    KNamedCommand *setVisibleCommand(bool visible);

    bool contains(Q_INT32 x, Q_INT32 y) const;
    bool contains(const QPoint& pt) const;

    /**
     * Retrieve the bounds of the paint device. The size is not exact,
     * but may be larger if the underlying datamanager works that way.
     * For instance, the tiled datamanager keeps the extent to the nearest
     * multiple of 64.
     */
    void extent(Q_INT32 &x, Q_INT32 &y, Q_INT32 &w, Q_INT32 &h) const;
    virtual QRect extent() const;

    /**
     * XXX: This should be a temporay hack, awaiting a proper fix.
     *
     * Indicates whether the extent really represents the extent. For example,
     * the KisBackground checkerboard pattern is generated by filling the
     * default tile but it will return an empty extent.
     */
    bool extentIsValid() const;
    void setExtentIsValid(bool isValid);

    /**
     * Get the exact bounds of this paint device. This may be very slow,
     * especially on larger paint devices because it does a linear scanline search.
     */
    void exactBounds(Q_INT32 &x, Q_INT32 &y, Q_INT32 &w, Q_INT32 &h);
    virtual QRect exactBounds();

    void crop(Q_INT32 x, Q_INT32 y, Q_INT32 w, Q_INT32 h) { m_datamanager -> setExtent(x - m_x, y - m_y, w, h); };
    void crop(QRect r) { r.moveBy(-m_x, -m_y); m_datamanager -> setExtent(r); };

    /**
     * Complete erase the current paint device. Its size will become 0.
     */
    virtual void clear() { m_datamanager -> clear(); };

    /**
     * Read the bytes representing the rectangle described by x, y, w, h into
     * data. If data is not big enough, Krita will gladly overwrite the rest
     * of your precious memory.
     *
     * Since this is a copy, you need to make sure you have enough memory.
     *
     * Reading from areas not previously initialized will read the default
     * pixel value into data.
     */
    virtual void readBytes(Q_UINT8 * data, Q_INT32 x, Q_INT32 y, Q_INT32 w, Q_INT32 h);

    /**
     * Copy the bytes in data into the rect specified by x, y, w, h. If there
     * data is too small or uninitialized, Krita will happily read parts of
     * memory you never wanted to be read.
     *
     * If the data is written to areas of the paint device not previously initialized,
     * the paint device will grow.
     */
    virtual void writeBytes(const Q_UINT8 * data, Q_INT32 x, Q_INT32 y, Q_INT32 w, Q_INT32 h);

    /**
     * Get the number of contiguous columns starting at x, valid for all values
     * of y between minY and maxY.
     */
    Q_INT32 numContiguousColumns(Q_INT32 x, Q_INT32 minY, Q_INT32 maxY);

    /**
     * Get the number of contiguous rows starting at y, valid for all values
     * of x between minX and maxX.
     */
    Q_INT32 numContiguousRows(Q_INT32 y, Q_INT32 minX, Q_INT32 maxX);

    /**
     * Get the row stride at pixel (x, y). This is the number of bytes to add to a
     * pointer to pixel (x, y) to access (x, y + 1).
     */
    Q_INT32 rowStride(Q_INT32 x, Q_INT32 y);

    /**
     * Get a read-only pointer to pixel (x, y).
     */
    const Q_UINT8* pixel(Q_INT32 x, Q_INT32 y);

    /**
     * Get a read-write pointer to pixel (x, y).
     */
    Q_UINT8* writablePixel(Q_INT32 x, Q_INT32 y);

    /**
     *   Converts the paint device to a different colorspace
     */
    virtual void convertTo(KisColorSpace * dstColorSpace, Q_INT32 renderingIntent = INTENT_PERCEPTUAL);

    /**
     * Fill this paint device with the data from img;
     */
    virtual void convertFromQImage(const QImage& img);

    /**
     * Create an RGBA QImage from a rectangle in the paint device.
     *
     * @param x Left coordinate of the rectangle
     * @param y Top coordinate of the rectangle
     * @param w Width of the rectangle in pixels
     * @param h Height of the rectangle in pixels
     * @param dstProfile RGB profile to use in conversion. May be 0, in which
     * case it's up to the colour strategy to choose a profile (most
     * like sRGB).
     * @param exposure The exposure setting used to render a preview of a high dynamic range image.
     */
    virtual QImage convertToQImage(KisProfile *  dstProfile, Q_INT32 x, Q_INT32 y, Q_INT32 w, Q_INT32 h, float exposure = 0.0f);

    /**
     * Create an RGBA QImage from a rectangle in the paint device. The rectangle is defined by the parent image's bounds.
     *
     * @param dstProfile RGB profile to use in conversion. May be 0, in which
     * case it's up to the colour strategy to choose a profile (most
     * like sRGB).
     * @param exposure The exposure setting used to render a preview of a high dynamic range image.
     */
    virtual QImage convertToQImage(KisProfile *  dstProfile, float exposure = 0.0f);

    virtual QString name() const;
    virtual void setName(const QString& name);

    /**
     * Fill c and opacity with the values found at x and y.
     *
     * The color values will be transformed from the profile of
     * this paint device to the display profile.
     *
     * @return true if the operation was succesful.
     */

    bool pixel(Q_INT32 x, Q_INT32 y, QColor *c, Q_UINT8 *opacity);

    bool pixel(Q_INT32 x, Q_INT32 y, KisColor * kc);

    /**
     * Return the KisColor of the pixel at x,y.
     */
    KisColor colorAt(Q_INT32 x, Q_INT32 y);

    /**
     * Set the specified pixel to the specified color. Note that this
     * bypasses KisPainter. the PaintDevice is here used as an equivalent
     * to QImage, not QPixmap. This means that this is not undoable; also,
     * there is no compositing with an existing value at this location.
     *
     * The color values will be transformed from the display profile to
     * the paint device profile.
     *
     * Note that this will use 8-bit values and may cause a significant
     * degradation when used on 16-bit or hdr quality images.
     *
     * @return true if the operation was succesful
     *
     */
    bool setPixel(Q_INT32 x, Q_INT32 y, const QColor& c, Q_UINT8 opacity);

    bool setPixel(Q_INT32 x, Q_INT32 y, const KisColor& kc);

    bool hasAlpha() const;

    KisColorSpace * colorSpace() const;

    KisDataManagerSP dataManager() const { return m_datamanager; }

    /**
     * Replace the pixel data, color strategy, and profile.
     */
    void setData(KisDataManagerSP data, KisColorSpace * colorSpace);

    KisCompositeOp compositeOp() { return m_compositeOp; }
    void setCompositeOp(const KisCompositeOp& compositeOp) { m_compositeOp = compositeOp; }

    KNamedCommand *setCompositeOpCommand(const KisCompositeOp& compositeOp);

    /**
     * The X offset of the paint device
     */
    Q_INT32 getX();

    /**
     * The Y offset of the paint device
     */
    Q_INT32 getY();

    /**
     * Return the X offset of the paint device
     */
    void setX(Q_INT32 x);

    /**
     * Return the Y offset of the paint device
     */
    void setY(Q_INT32 y);


    /**
     * Return the number of bytes a pixel takes.
     */
    virtual Q_INT32 pixelSize() const;

    /**
     * Return the number of channels a pixel takes
     */
    virtual Q_INT32 nChannels() const;

    KisImage *image();
        const KisImage *image() const;
        void setImage(KisImage *image);

    KisUndoAdapter *undoAdapter() const;

    void scale(double sx, double sy, KisProgressDisplayInterface *m_progress, KisFilterStrategy *filterStrategy);
        void rotate(double angle, bool rotateAboutImageCentre, KisProgressDisplayInterface *m_progress);
        void shear(double angleX, double angleY, KisProgressDisplayInterface *m_progress);

    /**
     * Mirror the device along the X axis
     */
    void mirrorX();
    /**
     * Mirror the device along the Y axis
     */
    void mirrorY();

    KisMementoSP getMemento() { return m_datamanager -> getMemento(); };
    void rollback(KisMementoSP memento) { m_datamanager -> rollback(memento); };
    void rollforward(KisMementoSP memento) { m_datamanager -> rollforward(memento); };
    /**
     * This function return an iterator which points to the first pixel of an rectangle
     */
    KisRectIteratorPixel createRectIterator(Q_INT32 left, Q_INT32 top, Q_INT32 w, Q_INT32 h, bool writable);

    /**
     * This function return an iterator which points to the first pixel of a horizontal line
     */
    KisHLineIteratorPixel createHLineIterator(Q_INT32 x, Q_INT32 y, Q_INT32 w, bool writable);

    /**
     * This function return an iterator which points to the first pixel of a vertical line
     */
    KisVLineIteratorPixel createVLineIterator(Q_INT32 x, Q_INT32 y, Q_INT32 h, bool writable);

    /** make owning image emit a selectionChanged */
    void emitSelectionChanged();
    void emitSelectionChanged(const QRect& r);

    /** Get the current selection or create one if this paintdevice hasn't got a selection yet. */
    KisSelectionSP selection();

    /** Set the specified selection as the active selection for this paintdevice */
    //void setSelection(KisSelectionSP selection);

    /** Adds the specified selection to the currently active selection for this paintdevice */
    void addSelection(KisSelectionSP selection);

    /** Subtracts the specified selection from the currently active selection for this paindevice */
    void subtractSelection(KisSelectionSP selection);

    /** Whether there is a valid selection for this paintdevice. */
    bool hasSelection();

    /** Deselect the selection for this paintdevice. */
    void deselect();

    /** Reinstates the old selection */
    void reselect();
        
    /** Clear the selected pixels from the paint device */
    void clearSelection();

    /**
     * Apply a mask to the image data, i.e. multiply each pixel's opacity by its
     * selectedness in the mask.
     */
    void applySelectionMask(KisSelectionSP mask);

signals:

        void visibilityChanged(KisPaintDeviceImplSP device);
        void positionChanged(KisPaintDeviceImplSP device);
        void ioProgress(Q_INT8 percentage);
    void profileChanged(KisProfile *  profile);

private:
    KisPaintDeviceImpl& operator=(const KisPaintDeviceImpl&);

protected:
    KisDataManagerSP m_datamanager;

private:
    // This is not a shared pointer by design. A layer does not own its containing image,
    // the image owns its layers. This allows the image and its layers to be destroyed
    // when the last reference to the image is removed. If the layers kept references,
    // a cycle would be created, and removing the last external reference to the image would not
    // destroy the objects.
    KisImage *m_owner;

    bool m_extentIsValid;

    Q_INT32 m_x;
    Q_INT32 m_y;
    bool m_visible;
    QString m_name;
    // Operation used to composite this layer with the layers _under_ this layer
    KisCompositeOp m_compositeOp;
    KisColorSpace * m_colorSpace;
    // Cached for quick access
    Q_INT32 m_pixelSize;
    Q_INT32 m_nChannels;

    void accept(KisScaleVisitor &);
    void accept(KisRotateVisitor &);

    // Whether the selection is active
    bool m_hasSelection;
    bool m_selectionDeselected;
    
    // Contains the actual selection. For now, there can be only
    // one selection per layer. XXX: is this a limitation?
    KisSelectionSP m_selection;
    
    DCOPObject * m_dcop;

};

inline Q_INT32 KisPaintDeviceImpl::pixelSize() const
{
    Q_ASSERT(m_pixelSize > 0);
    return m_pixelSize;
}

inline Q_INT32 KisPaintDeviceImpl::nChannels() const
{
    Q_ASSERT(m_nChannels > 0);
    return m_nChannels;
;
}

inline KisColorSpace * KisPaintDeviceImpl::colorSpace() const
{
    Q_ASSERT(m_colorSpace != 0);
        return m_colorSpace;
}


inline Q_INT32 KisPaintDeviceImpl::getX()
{
    return m_x;
}

inline Q_INT32 KisPaintDeviceImpl::getY()
{
    return m_y;
}

inline const bool KisPaintDeviceImpl::visible() const
{
        return m_visible;
}

inline void KisPaintDeviceImpl::setVisible(bool v)
{
        if (m_visible != v) {
                m_visible = v;
                emit visibilityChanged(this);
        }
}


inline KisImage *KisPaintDeviceImpl::image()
{
        return m_owner;
}

inline const KisImage *KisPaintDeviceImpl::image() const
{
        return m_owner;
}

inline void KisPaintDeviceImpl::setImage(KisImage *image)
{
        m_owner = image;
}

inline bool KisPaintDeviceImpl::hasAlpha() const
{
        return colorSpace() -> hasAlpha();
}

inline void KisPaintDeviceImpl::readBytes(Q_UINT8 * data, Q_INT32 x, Q_INT32 y, Q_INT32 w, Q_INT32 h)
{
    m_datamanager -> readBytes(data, x - m_x, y - m_y, w, h);
}

inline void KisPaintDeviceImpl::writeBytes(const Q_UINT8 * data, Q_INT32 x, Q_INT32 y, Q_INT32 w, Q_INT32 h)
{
    m_datamanager -> writeBytes( data, x - m_x, y - m_y, w, h);
}


#endif // KIS_PAINT_DEVICE_IMPL_H_

