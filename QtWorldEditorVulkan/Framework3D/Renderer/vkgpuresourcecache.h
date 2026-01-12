#ifndef VKGPURESOURCECACHE_H
#define VKGPURESOURCECACHE_H

#include <QVulkanWindowRenderer>
#include <QVulkanDeviceFunctions>

#include "vkgpuresources.h"

#include "cpumesh.h"
#include "vkuploadcontext.h"

class GpuResourceCache
{
public:
    GpuResourceCache() = default;
    ~GpuResourceCache() = default;

public:
    void init(VkDevice device, QVulkanDeviceFunctions* deviceFunctions, QVulkanFunctions* vulkanFunction, VkPhysicalDevice physicalDevice, UploadContext* uploadContext);
    void shutdown();

public:
    VkGPUResources::MeshGPU* getOrCreateMesh(const QString& meshId, const CpuMesh& cpuMesh);
    VkGPUResources::TextureGPU* getOrCreateTexture(const QString& textureId, const QString& texturePath);

private:
    void destroyMesh(VkGPUResources::MeshGPU* mesh);
    void destroyTexture(VkGPUResources::TextureGPU* texture);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    QVulkanDeviceFunctions* m_deviceFunctions = nullptr;
    QVulkanFunctions *m_vulkanFunction = nullptr;

    UploadContext* m_uploadContext = nullptr;

private:
    QHash<QString, VkGPUResources::MeshGPU*> m_meshCache;
    QHash<QString, VkGPUResources::TextureGPU*> m_textureCache;
};

#endif // VKGPURESOURCECACHE_H
