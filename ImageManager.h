#ifndef IMAGEMANAGER_H
#define IMAGEMANAGER_H

#include <QString>
#include <QStringList>
#include <QPixmap>

class ImageManager
{
public:
    ImageManager();

    bool loadFromPath(const QString &path);
    QPixmap currentImage() const;
    QPixmap scaledImage(const QSize &targetSize) const;
    void nextImage();
    bool hasImages() const;
    int imageCount() const;

private:
    void loadCurrentPixmap();

    QStringList m_imagePaths;
    int         m_currentIndex;
    QPixmap     m_currentPixmap;
};

#endif // IMAGEMANAGER_H
