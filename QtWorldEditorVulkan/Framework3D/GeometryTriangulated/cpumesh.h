#ifndef CPUMESH_H
#define CPUMESH_H

#include "vertexdata.h"

struct CpuMesh {
    QVector<VertexData> vertices;
    QVector<uint32_t> indices;

    QString albedoPath;
};

#endif // CPUMESH_H
