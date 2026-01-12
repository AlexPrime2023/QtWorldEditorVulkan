#ifndef VKMATERIALSYSTEM_H
#define VKMATERIALSYSTEM_H

#include "vkgpuresources.h"

class MaterialSystem
{
public:
    MaterialSystem() = default;
    ~MaterialSystem() = default;

public:
    void init(VkDevice device, QVulkanDeviceFunctions* deviceFunctions, VkDescriptorPool descriptorPool);
    void shutdown();

public:
    VkDescriptorSetLayout materialSetLayout() const { return m_materialSetLayout; }
    VkSampler sampler() const { return m_sampler; }

    // Creates set=1 (sampler2D) for a specific TextureGPU
    VkGPUResources::MaterialInstance createInstance(const VkGPUResources::TextureGPU* texture);

private:
    void createSetLayout();
    void createSampler();

private:
    VkDevice m_device = VK_NULL_HANDLE;
    QVulkanDeviceFunctions* m_deviceFunctions = nullptr;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_materialSetLayout = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};

#endif // VKMATERIALSYSTEM_H
