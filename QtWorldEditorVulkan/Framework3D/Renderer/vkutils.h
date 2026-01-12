#ifndef VKUTILS_H
#define VKUTILS_H

#include <QVulkanWindowRenderer>
#include <QVulkanDeviceFunctions>

namespace VkUtils
{
    void cmdCopyBufferWithBarrier(VkCommandBuffer commandBuffer, VkBuffer src, VkBuffer dst, VkDeviceSize size, VkAccessFlags dstAccess, VkPipelineStageFlags dstStage, QVulkanDeviceFunctions* deviceFunction);
    void cmdTransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, QVulkanDeviceFunctions* deviceFunction);
    void cmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, uint32_t w, uint32_t h, QVulkanDeviceFunctions* deviceFunction);

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, QVulkanDeviceFunctions* deviceFunction, QVulkanFunctions* vulkanFunction, VkDevice device, VkPhysicalDevice physicalDevice);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, QVulkanFunctions* vulkanFunction, VkPhysicalDevice physicalDevice);

    namespace VertexInput::Lit
    {
        VkVertexInputBindingDescription bindingDescription();
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions();
    }
}

namespace FileUtils
{
    QByteArray readFile(const QString &fileName);
}

#endif // VKUTILS_H
