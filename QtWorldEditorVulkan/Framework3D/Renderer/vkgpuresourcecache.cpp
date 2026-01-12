#include "vkgpuresourcecache.h"
#include "vkutils.h"

#include <QVulkanFunctions>

void GpuResourceCache::init(VkDevice device, QVulkanDeviceFunctions* deviceFunctions, QVulkanFunctions* vulkanFunction, VkPhysicalDevice physicalDevice, UploadContext* uploadContext)
{
    m_device = device;
    m_deviceFunctions = deviceFunctions;
    m_vulkanFunction = vulkanFunction;
    m_physicalDevice = physicalDevice;
    m_uploadContext = uploadContext;

    if (!m_device || !m_deviceFunctions || !m_physicalDevice || !m_uploadContext)
        qFatal("GpuResourceCache::init: invalid args");
}

void GpuResourceCache::shutdown()
{
    if (!m_device || !m_deviceFunctions)
        return;

    for (auto it = m_meshCache.begin(); it != m_meshCache.end(); ++it)
        destroyMesh(it.value());
    m_meshCache.clear();

    for (auto it = m_textureCache.begin(); it != m_textureCache.end(); ++it)
        destroyTexture(it.value());
    m_textureCache.clear();

    m_device = VK_NULL_HANDLE;
    m_deviceFunctions = nullptr;
    m_physicalDevice = VK_NULL_HANDLE;
    m_uploadContext = nullptr;
}

VkGPUResources::MeshGPU* GpuResourceCache::getOrCreateMesh(const QString& meshId, const CpuMesh& cpuMesh)
{
    if (m_meshCache.contains(meshId))
        return m_meshCache.value(meshId);

    auto* mesh = new VkGPUResources::MeshGPU();

    // Vertex buffer
    VkDeviceSize vbSize = sizeof(VertexData) * cpuMesh.vertices.size();

    VkBuffer vbStaging = VK_NULL_HANDLE;
    VkDeviceMemory vbStagingMem = VK_NULL_HANDLE;
    VkUtils::createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vbStaging, vbStagingMem, m_deviceFunctions, m_vulkanFunction, m_device, m_physicalDevice);

    void* data = nullptr;
    m_deviceFunctions->vkMapMemory(m_device, vbStagingMem, 0, vbSize, 0, &data);
    memcpy(data, cpuMesh.vertices.constData(), size_t(vbSize));
    m_deviceFunctions->vkUnmapMemory(m_device, vbStagingMem);

    VkUtils::createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh->vertexBuffer, mesh->vertexBufferMemory, m_deviceFunctions, m_vulkanFunction, m_device, m_physicalDevice);

    // Index buffer
    mesh->indexCount = uint32_t(cpuMesh.indices.size());
    VkDeviceSize ibSize = sizeof(uint32_t) * cpuMesh.indices.size();

    VkBuffer ibStaging = VK_NULL_HANDLE;
    VkDeviceMemory ibStagingMem = VK_NULL_HANDLE;
    VkUtils::createBuffer(ibSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, ibStaging, ibStagingMem, m_deviceFunctions, m_vulkanFunction, m_device, m_physicalDevice);

    m_deviceFunctions->vkMapMemory(m_device, ibStagingMem, 0, ibSize, 0, &data);
    memcpy(data, cpuMesh.indices.constData(), size_t(ibSize));
    m_deviceFunctions->vkUnmapMemory(m_device, ibStagingMem);

    VkUtils::createBuffer(ibSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh->indexBuffer, mesh->indexBufferMemory, m_deviceFunctions, m_vulkanFunction, m_device, m_physicalDevice);

    // Upload job
    UploadContext::Job job;
    job.record = [=](VkCommandBuffer commandBuffer)
    {
        VkUtils::cmdCopyBufferWithBarrier(commandBuffer, vbStaging, mesh->vertexBuffer, vbSize, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, m_deviceFunctions);
        VkUtils::cmdCopyBufferWithBarrier(commandBuffer, ibStaging, mesh->indexBuffer, ibSize, VK_ACCESS_INDEX_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, m_deviceFunctions);
    };
    job.buffersToDestroy = { vbStaging, ibStaging };
    job.memoriesToFree   = { vbStagingMem, ibStagingMem };
    job.onComplete = [mesh]() { mesh->isReady = true; };

    m_uploadContext->enqueue(std::move(job));

    m_meshCache.insert(meshId, mesh);
    return mesh;
}

VkGPUResources::TextureGPU* GpuResourceCache::getOrCreateTexture(const QString& textureId, const QString& texturePath)
{
    if (m_textureCache.contains(textureId))
        return m_textureCache.value(textureId);

    auto* texture = new VkGPUResources::TextureGPU();
    texture->isReady = false;

    QImage image(texturePath);
    if (image.isNull())
        qFatal("Failed to load texture image: %s", qPrintable(texturePath));

    image = image.convertToFormat(QImage::Format_RGBA8888);
    VkDeviceSize imageSize = image.sizeInBytes();

    // staging
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VkUtils::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMem, m_deviceFunctions, m_vulkanFunction, m_device, m_physicalDevice);

    void* data = nullptr;
    m_deviceFunctions->vkMapMemory(m_device, stagingMem, 0, imageSize, 0, &data);
    memcpy(data, image.constBits(), size_t(imageSize));
    m_deviceFunctions->vkUnmapMemory(m_device, stagingMem);

    // image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = { uint32_t(image.width()), uint32_t(image.height()), 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = m_deviceFunctions->vkCreateImage(m_device, &imageInfo, nullptr, &texture->textureImage);
    if (result != VK_SUCCESS)
        qFatal("Failed to create image: %d", result);

    VkMemoryRequirements memReq{};
    m_deviceFunctions->vkGetImageMemoryRequirements(m_device, texture->textureImage, &memReq);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;
    alloc.memoryTypeIndex = VkUtils::findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vulkanFunction, m_physicalDevice);

    result = m_deviceFunctions->vkAllocateMemory(m_device, &alloc, nullptr, &texture->textureImageMemory);
    if (result != VK_SUCCESS)
        qFatal("Failed to allocate image memory: %d", result);

    m_deviceFunctions->vkBindImageMemory(m_device, texture->textureImage, texture->textureImageMemory, 0);

    // view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture->textureImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    result = m_deviceFunctions->vkCreateImageView(m_device, &viewInfo, nullptr, &texture->textureImageView);
    if (result != VK_SUCCESS)
        qFatal("Failed to create texture image view: %d", result);

    // upload job
    UploadContext::Job job;
    job.record = [=](VkCommandBuffer commandBuffer)
    {
        VkUtils::cmdTransitionImageLayout(commandBuffer, texture->textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_deviceFunctions);
        VkUtils::cmdCopyBufferToImage(commandBuffer, stagingBuffer, texture->textureImage, uint32_t(image.width()), uint32_t(image.height()), m_deviceFunctions);
        VkUtils::cmdTransitionImageLayout(commandBuffer, texture->textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_deviceFunctions);

    };
    job.buffersToDestroy = { stagingBuffer };
    job.memoriesToFree   = { stagingMem };
    job.onComplete = [texture]() { texture->isReady = true; };

    m_uploadContext->enqueue(std::move(job));

    m_textureCache.insert(textureId, texture);
    return texture;
}

void GpuResourceCache::destroyMesh(VkGPUResources::MeshGPU* mesh)
{
    if (!mesh)
        return;

    if (mesh->vertexBuffer)
        m_deviceFunctions->vkDestroyBuffer(m_device, mesh->vertexBuffer, nullptr);

    if (mesh->vertexBufferMemory)
        m_deviceFunctions->vkFreeMemory(m_device, mesh->vertexBufferMemory, nullptr);

    if (mesh->indexBuffer)
        m_deviceFunctions->vkDestroyBuffer(m_device, mesh->indexBuffer, nullptr);

    if (mesh->indexBufferMemory)
        m_deviceFunctions->vkFreeMemory(m_device, mesh->indexBufferMemory, nullptr);

    delete mesh;
}

void GpuResourceCache::destroyTexture(VkGPUResources::TextureGPU* texture)
{
    if (!texture)
        return;

    if (texture->textureImageView)
        m_deviceFunctions->vkDestroyImageView(m_device, texture->textureImageView, nullptr);

    if (texture->textureImage)
        m_deviceFunctions->vkDestroyImage(m_device, texture->textureImage, nullptr);

    if (texture->textureImageMemory)
        m_deviceFunctions->vkFreeMemory(m_device, texture->textureImageMemory, nullptr);

    delete texture;
}

