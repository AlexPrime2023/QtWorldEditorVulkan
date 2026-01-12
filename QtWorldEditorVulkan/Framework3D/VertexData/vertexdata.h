#ifndef VERTEXDATA_H
#define VERTEXDATA_H

#include <QVector3D>
#include <QVector2D>

struct VertexData {
    QVector3D position;
    QVector2D texCoord;
    QVector3D normal;
};


#endif // VERTEXDATA_H
