#ifndef MODELLOADER_H
#define MODELLOADER_H

#include <QVector2D>
#include <QVector3D>
#include <QVector>

#include <QMatrix4x4>

#include "cpumesh.h"

// TODO: Inherited from IMeshLoader
class ModelLoader
{
public:
    ModelLoader() = default;
    ~ModelLoader() = default;

public:
    const QVector<CpuMesh>& meshes() const { return m_meshes; };

    bool readOBJFile(QString const &filePath);

private:
    QVector<CpuMesh> m_meshes;
};

#endif // MODELLOADER_H
