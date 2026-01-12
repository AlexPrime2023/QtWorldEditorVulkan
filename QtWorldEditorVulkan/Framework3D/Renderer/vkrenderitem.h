#ifndef VKRENDERITEM_H
#define VKRENDERITEM_H

#include <QVulkanWindowRenderer>
#include <QVulkanDeviceFunctions>

#include "vkgpuresources.h"

struct VkRenderItem
{
    VkGPUResources::MeshGPU* meshGPU = nullptr;          // non-owning

    VkGPUResources::MaterialInstance materialInstance;

    QMatrix4x4 modelMatrix;
};


#endif // VKRENDERITEM_H
