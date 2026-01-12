#include "vkuploadcontext.h"

void UploadContext::init(VkDevice device, QVulkanDeviceFunctions* deviceFunctions, VkCommandPool commandPool, VkQueue queue)
{
    m_device = device;
    m_deviceFunctions = deviceFunctions;
    m_commandPool = commandPool;
    m_queue = queue;

    if (!m_device || !m_deviceFunctions || !m_commandPool || !m_queue)
        qFatal("UploadContext::init: invalid init args");
}

void UploadContext::shutdown()
{
    if (!m_device || !m_deviceFunctions)
        return;

    waitAll();
    poll();

    m_pending.clear();

    m_device = VK_NULL_HANDLE;
    m_deviceFunctions = nullptr;
    m_commandPool = VK_NULL_HANDLE;
    m_queue = VK_NULL_HANDLE;
}

void UploadContext::waitAll()
{
    if (m_pending.isEmpty())
        return;

    std::vector<VkFence> fences;
    fences.reserve(size_t(m_pending.size()));

    for (const auto& job : m_pending)
        fences.push_back(job.fence);

    VkResult result = m_deviceFunctions->vkWaitForFences(m_device, uint32_t(fences.size()), fences.data(), VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS)
        qFatal("UploadContext::waitAll: vkWaitForFences failed: %d", result);
}

void UploadContext::poll()
{
    if (m_pending.isEmpty())
        return;

    int i = 0;
    while (i < m_pending.size())
    {
        Job& job = m_pending[i];

        VkResult result = m_deviceFunctions->vkGetFenceStatus(m_device, job.fence);
        if (result == VK_NOT_READY) {
            ++i;
            continue;
        }

        if (result != VK_SUCCESS)
            qFatal("UploadContext::poll: vkGetFenceStatus failed: %d", result);

        // completed
        if (job.onComplete)
            job.onComplete();
        cleanupFinishedJob(job);

        m_pending.removeAt(i);
    }
}

void UploadContext::enqueue(Job job)
{
    if (!job.record)
        qFatal("UploadContext::enqueue: job.record is empty");

    job.commandBuffer = beginSingleTimeCommands();
    job.record(job.commandBuffer);

    submit(job);
    m_pending.push_back(std::move(job));
}

VkCommandBuffer UploadContext::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkResult result = m_deviceFunctions->vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) qFatal("UploadContext: vkAllocateCommandBuffers failed: %d", result);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    result = m_deviceFunctions->vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
        qFatal("UploadContext: vkBeginCommandBuffer failed: %d", result);

    return commandBuffer;
}

void UploadContext::submit(Job& job)
{
    VkResult result = m_deviceFunctions->vkEndCommandBuffer(job.commandBuffer);
    if (result != VK_SUCCESS)
        qFatal("UploadContext: vkEndCommandBuffer failed: %d", result);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &job.commandBuffer;

    VkFenceCreateInfo fenceCreaterInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    result = m_deviceFunctions->vkCreateFence(m_device, &fenceCreaterInfo, nullptr, &job.fence);
    if (result != VK_SUCCESS)
        qFatal("UploadContext: vkCreateFence failed: %d", result);

    result = m_deviceFunctions->vkQueueSubmit(m_queue, 1, &submitInfo, job.fence);
    if (result != VK_SUCCESS)
        qFatal("UploadContext: vkQueueSubmit failed: %d", result);
}

void UploadContext::cleanupFinishedJob(Job& job)
{
    // destroy staging buffers/memory
    for (VkBuffer bufferToDestroy : job.buffersToDestroy)
    {
        if (bufferToDestroy)
            m_deviceFunctions->vkDestroyBuffer(m_device, bufferToDestroy, nullptr);
    }

    for (VkDeviceMemory memoryToDestroy : job.memoriesToFree)
    {
        if (memoryToDestroy)
            m_deviceFunctions->vkFreeMemory(m_device, memoryToDestroy, nullptr);
    }

    // free commandBuffer
    if (job.commandBuffer)
        m_deviceFunctions->vkFreeCommandBuffers(m_device, m_commandPool, 1, &job.commandBuffer);

    // destroy fence
    if (job.fence)
        m_deviceFunctions->vkDestroyFence(m_device, job.fence, nullptr);

    job.commandBuffer = VK_NULL_HANDLE;
    job.fence = VK_NULL_HANDLE;
}
