!VULKAN_WINDOW_PRI {

CONFIG += VULKAN_WINDOW_PRI

INCLUDEPATH += $$PWD

include($$PWD/../Renderer/Renderer.pri)

SOURCES += \
    $$PWD/vulkanwindow.cpp

HEADERS += \
    $$PWD/vulkanwindow.h
}
