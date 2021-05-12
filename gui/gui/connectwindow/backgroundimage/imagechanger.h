#ifndef IMAGECHANGER_H
#define IMAGECHANGER_H

#include <QObject>
#include <QVariantAnimation>
#include "graphicresources/independentpixmap.h"

namespace ConnectWindow {

// Provides smooth image change and gif animation
class ImageChanger : public QObject
{
    Q_OBJECT
public:
    explicit ImageChanger(QObject *parent);
    virtual ~ImageChanger();

    QPixmap *currentPixmap();
    void setImage(QSharedPointer<IndependentPixmap> pixmap, bool bShowPrevChangeAnimation);
    void setMovie(QSharedPointer<QMovie> movie);       // for gif

signals:
    void updated();

private slots:
    void onOpacityChanged(const QVariant &value);
    void onOpacityFinished();

private:
    static constexpr int WIDTH = 332;

    QPixmap *pixmap_;
    QVariantAnimation opacityAnimation_;
    qreal opacityCurImage_;
    qreal opacityPrevImage_;

    struct ImageInfo
    {
        bool isMovie;
        QSharedPointer<IndependentPixmap> pixmap;
        QSharedPointer<QMovie> movie;

        ImageInfo() : isMovie(false) {}
        bool isValid() const
        {
            if (!isMovie)
            {
                return pixmap != nullptr;
            }
            else
            {
                return movie != nullptr;
            }
        }

        void clear()
        {
            pixmap.reset();
            movie.reset();
            isMovie = false;
        }
    };

    ImageInfo curImage_;
    ImageInfo prevImage_;

    void updatePixmap();
};

} //namespace ConnectWindow

#endif // IMAGECHANGER_H
