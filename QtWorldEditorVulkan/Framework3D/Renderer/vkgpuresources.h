#ifndef VK_GPU_RESOURCES_H
#define VK_GPU_RESOURCES_H

#include <QVulkanDeviceFunctions>

#include <QMatrix4x4>

namespace VkGPUResources
{
    struct MeshGPU
    {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
        uint32_t indexCount = 0;

        bool isReady = false;
    };

    struct TextureGPU
    {
        VkImage textureImage = VK_NULL_HANDLE;
        VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
        VkImageView textureImageView = VK_NULL_HANDLE;

        bool isReady = false;
    };

    struct MaterialInstance
    {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    };
}

namespace VkFrameResources
{
    struct FrameResources
    {
        VkBuffer cameraBuffer = VK_NULL_HANDLE;
        VkDeviceMemory cameraMem = VK_NULL_HANDLE;
        void* cameraMapped = nullptr;

        VkDescriptorSet cameraSet = VK_NULL_HANDLE;
    };

    struct CameraUBO
    {
        QMatrix4x4 view;
        QMatrix4x4 projection;
    };

    struct PushModel
    {
        QMatrix4x4 modelMatrix;
    };
}

#endif // VK_GPU_RESOURCES_H
