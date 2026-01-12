!GEOMETRY_TRIANGULATED_PRI {

CONFIG += GEOMETRY_TRIANGULATED_PRI

INCLUDEPATH += $$PWD

include($$PWD/../VertexData/VertexData.pri)

INCLUDEPATH += $$PWD/vendor

HEADERS += \
    $$PWD/modelloader.h \
    $$PWD/cpumesh.h

SOURCES += \
    $$PWD/modelloader.cpp
}
