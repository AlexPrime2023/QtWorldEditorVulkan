#include "vkutils.h"

#include <QFile>

#include "vertexdata.h"

void VkUtils::cmdCopyBufferWithBarrier(VkCommandBuffer commandBuffer, VkBuffer src, VkBuffer dst, VkDeviceSize size, VkAccessFlags dstAccess, VkPipelineStageFlags dstStage, QVulkanDeviceFunctions* deviceFunction)
{
    VkBufferCopy bufferCopyRegion{0, 0, size};
    deviceFunction->vkCmdCopyBuffer(commandBuffer, src, dst, 1, &bufferCopyRegion);

    VkBufferMemoryBarrier bufferMemoryBarrier{};
    bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufferMemoryBarrier.dstAccessMask = dstAccess;
    bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferMemoryBarrier.buffer = dst;
    bufferMemoryBarrier.offset = 0;
    bufferMemoryBarrier.size = size;

    deviceFunction->vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage, 0, 0, nullptr, 1, &bufferMemoryBarrier, 0, nullptr);
}

void VkUtils::cmdTransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, QVulkanDeviceFunctions* deviceFunction)
{
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = oldLayout;
    imageMemoryBarrier.newLayout = newLayout;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = 0, dstStage = 0;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    } else {
        qFatal("UploadContext: Unsupported layout transition!");
    }

    deviceFunction->vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}

void VkUtils::cmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, uint32_t w, uint32_t h, QVulkanDeviceFunctions* deviceFunction)
{
    VkBufferImageCopy bufferCopyRegion{};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent = { w, h, 1 };

    deviceFunction->vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
}

void VkUtils::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, QVulkanDeviceFunctions* deviceFunction, QVulkanFunctions* vulkanFunction, VkDevice device, VkPhysicalDevice physicalDevice)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = deviceFunction->vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS)
        qFatal("Failed to create buffer: %d", result);

    VkMemoryRequirements memRequirements{};
    deviceFunction->vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VkUtils::findMemoryType(memRequirements.memoryTypeBits, properties, vulkanFunction, physicalDevice);

    result = deviceFunction->vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    if (result != VK_SUCCESS)
        qFatal("Failed to allocate buffer memory: %d", result);

    deviceFunction->vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

uint32_t VkUtils::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, QVulkanFunctions* vulkanFunction, VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceMemoryProperties memProperties;

    vulkanFunction->vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    qFatal("Failed to find suitable memory type!");
    return -1;
}

namespace VkUtils::VertexInput
{
    VkVertexInputBindingDescription Lit::bindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription = {};

        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescription.stride = sizeof(VertexData);
        bindingDescription.binding = 0;

        return bindingDescription;
    }

    std::array<VkVertexInputAttributeDescription, 3> Lit::attributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(VertexData, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(VertexData, texCoord);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(VertexData, normal);

        return attributeDescriptions;
    }
}

QByteArray FileUtils::readFile(const QString &fileName)
{
    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly))
        QDebug(QtFatalMsg) << QLatin1String("Failed to open file:") << fileName;

    QByteArray content = file.readAll();
    file.close();

    return content;
}
