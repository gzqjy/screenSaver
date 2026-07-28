#include "ImageManager.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>

static const QStringList IMAGE_FILTERS = {
    "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif"
};

ImageManager::ImageManager()
    : m_currentIndex(0)
{
}

bool ImageManager::loadFromPath(const QString &path)
{
    m_imagePaths.clear();
    m_currentIndex = 0;
    m_currentPixmap = QPixmap();

    if (path.isEmpty())
        return false;

    QFileInfo info(path);
    if (!info.exists()) {
        qWarning() << "ImageManager: path does not exist:" << path;
        return false;
    }

    if (info.isFile()) {
        // 单张图片
        m_imagePaths.append(path);
        qDebug() << "ImageManager: loaded single image:" << path;
    } else if (info.isDir()) {
        // 目录 - 扫描所有图片文件
        QDir dir(path);
        QFileInfoList entries = dir.entryInfoList(IMAGE_FILTERS, QDir::Files, QDir::Name);
        for (const QFileInfo &entry : entries) {
            m_imagePaths.append(entry.absoluteFilePath());
        }
        qDebug() << "ImageManager: loaded" << m_imagePaths.size() << "images from directory:" << path;
    }

    if (!m_imagePaths.isEmpty()) {
        loadCurrentPixmap();
        return true;
    }

    return false;
}

QPixmap ImageManager::currentImage() const
{
    return m_currentPixmap;
}

QPixmap ImageManager::scaledImage(const QSize &targetSize) const
{
    if (m_currentPixmap.isNull())
        return QPixmap();

    return m_currentPixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void ImageManager::nextImage()
{
    if (m_imagePaths.isEmpty())
        return;

    m_currentIndex = (m_currentIndex + 1) % m_imagePaths.size();
    loadCurrentPixmap();
}

bool ImageManager::hasImages() const
{
    return !m_imagePaths.isEmpty();
}

int ImageManager::imageCount() const
{
    return m_imagePaths.size();
}

void ImageManager::loadCurrentPixmap()
{
    if (m_currentIndex >= 0 && m_currentIndex < m_imagePaths.size()) {
        m_currentPixmap = QPixmap(m_imagePaths[m_currentIndex]);
        if (m_currentPixmap.isNull()) {
            qWarning() << "ImageManager: failed to load image:" << m_imagePaths[m_currentIndex];
        }
    }
}
