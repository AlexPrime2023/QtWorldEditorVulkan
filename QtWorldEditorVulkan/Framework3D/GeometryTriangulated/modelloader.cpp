#include "modelloader.h"

#include <unordered_map>

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#endif

bool ModelLoader::readOBJFile(const QString& filePath)
{
    tinyobj::attrib_t attribs;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    const bool result = tinyobj::LoadObj(
        &attribs, &shapes, &materials, &warn, &err,
        filePath.toStdString().c_str(),
        /*mtl_basedir*/ nullptr,
        /*triangulate*/ true
        );

    if (!warn.empty()) qDebug("%s", warn.c_str());
    if (!err.empty())  qDebug("%s", err.c_str());
    if (!result) {
        qWarning("Could not open file: %s", filePath.toStdString().c_str());
        return false;
    }

    m_meshes.clear();
    m_meshes.reserve(int(shapes.size()));

    auto readPos = [&](int vi) -> QVector3D {
        if (vi < 0) return {0,0,0};
        const size_t base = size_t(vi) * 3;
        if (base + 2 >= attribs.vertices.size()) return {0,0,0};
        return { attribs.vertices[base], attribs.vertices[base + 1], attribs.vertices[base + 2] };
    };

    auto readUV = [&](int vti) -> QVector2D {
        if (vti < 0 || attribs.texcoords.empty()) return {0,0};
        const size_t base = size_t(vti) * 2;
        if (base + 1 >= attribs.texcoords.size()) return {0,0};
        return { attribs.texcoords[base], 1.0f - attribs.texcoords[base + 1] };
    };

    auto readNormal = [&](int vni) -> QVector3D {
        if (vni < 0 || attribs.normals.empty()) return {0,0,0};
        const size_t base = size_t(vni) * 3;
        if (base + 2 >= attribs.normals.size()) return {0,0,0};
        return { attribs.normals[base], attribs.normals[base + 1], attribs.normals[base + 2] };
    };

    auto safeNormalize = [&](const QVector3D& n) -> QVector3D {
        const float len2 = QVector3D::dotProduct(n, n);
        if (len2 <= 1e-20f) return {0,0,1};
        return n / std::sqrt(len2);
    };

    struct Key {
        int v;
        int vt;
        int vn;
        bool operator==(const Key& o) const noexcept { return v==o.v && vt==o.vt && vn==o.vn; }
    };

    struct KeyHash {
        size_t operator()(const Key& k) const noexcept {
            size_t h1 = std::hash<int>{}(k.v);
            size_t h2 = std::hash<int>{}(k.vt);
            size_t h3 = std::hash<int>{}(k.vn);
            size_t h = h1;
            h ^= (h2 + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
            h ^= (h3 + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
            return h;
        }
    };

    for (const auto& shape : shapes) {
        CpuMesh out;

        out.vertices.clear();
        out.indices.clear();
        out.vertices.reserve(shape.mesh.indices.size());
        out.indices.reserve(shape.mesh.indices.size());

        std::unordered_map<Key, uint32_t, KeyHash> unique;
        unique.reserve(shape.mesh.indices.size());

        QVector3D minD(+std::numeric_limits<float>::infinity(),
                       +std::numeric_limits<float>::infinity(),
                       +std::numeric_limits<float>::infinity());
        QVector3D maxD(-std::numeric_limits<float>::infinity(),
                       -std::numeric_limits<float>::infinity(),
                       -std::numeric_limits<float>::infinity());

        auto updateBounds = [&](const QVector3D& p) {
            minD.setX(std::min(minD.x(), p.x()));
            minD.setY(std::min(minD.y(), p.y()));
            minD.setZ(std::min(minD.z(), p.z()));
            maxD.setX(std::max(maxD.x(), p.x()));
            maxD.setY(std::max(maxD.y(), p.y()));
            maxD.setZ(std::max(maxD.z(), p.z()));
        };

        const auto& mesh = shape.mesh;
        size_t indexOffset = 0;

        for (size_t f = 0; f < mesh.num_face_vertices.size(); ++f) {
            const int fv = mesh.num_face_vertices[f];
            if (fv != 3) {
                indexOffset += size_t(std::max(fv, 0));
                continue;
            }

            const tinyobj::index_t i0 = mesh.indices[indexOffset + 0];
            const tinyobj::index_t i1 = mesh.indices[indexOffset + 1];
            const tinyobj::index_t i2 = mesh.indices[indexOffset + 2];
            indexOffset += 3;

            if (i0.vertex_index < 0 || i1.vertex_index < 0 || i2.vertex_index < 0)
                continue;

            const bool needFaceNormal =
                attribs.normals.empty() ||
                i0.normal_index < 0 || i1.normal_index < 0 || i2.normal_index < 0;

            QVector3D faceN(0,0,1);
            if (needFaceNormal) {
                const QVector3D p0 = readPos(i0.vertex_index);
                const QVector3D p1 = readPos(i1.vertex_index);
                const QVector3D p2 = readPos(i2.vertex_index);
                faceN = safeNormalize(QVector3D::crossProduct(p1 - p0, p2 - p0));
            }

            const tinyobj::index_t tri[3] = { i0, i1, i2 };

            for (int k = 0; k < 3; ++k) {
                const auto& idx = tri[k];

                if (needFaceNormal) {
                    VertexData v{};
                    v.position = readPos(idx.vertex_index);
                    v.texCoord = readUV(idx.texcoord_index);
                    v.normal = faceN;

                    const uint32_t newIndex = uint32_t(out.vertices.size());
                    out.vertices.push_back(v);
                    out.indices.push_back(newIndex);
                    updateBounds(v.position);
                    continue;
                }

                Key key{ idx.vertex_index, idx.texcoord_index, idx.normal_index };

                auto it = unique.find(key);
                if (it == unique.end()) {
                    VertexData v{};
                    v.position = readPos(idx.vertex_index);
                    v.texCoord = readUV(idx.texcoord_index);
                    v.normal = safeNormalize(readNormal(idx.normal_index));

                    const uint32_t newIndex = uint32_t(out.vertices.size());
                    out.vertices.push_back(v);
                    unique.emplace(key, newIndex);
                    out.indices.push_back(newIndex);
                    updateBounds(v.position);
                } else {
                    out.indices.push_back(it->second);
                }
            }
        }

        out.albedoPath = "F:\\default.png";

        if (!out.vertices.isEmpty() && !out.indices.isEmpty())
            m_meshes.push_back(std::move(out));
        else
            return false;
    }

    return true;
}
