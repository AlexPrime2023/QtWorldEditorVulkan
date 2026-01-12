#ifndef VKUPLOADCONTEXT_H
#define VKUPLOADCONTEXT_H

#include <QVulkanDeviceFunctions>
#include <functional>

#include <QVector>

class UploadContext
{
public:
    struct Job {
        std::function<void(VkCommandBuffer commandBuffer)> record;

        std::vector<VkBuffer> buffersToDestroy;
        std::vector<VkDeviceMemory> memoriesToFree;

        std::function<void()> onComplete;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
    };

public:
    UploadContext() = default;
    ~UploadContext() = default;

public:
    void init(VkDevice device, QVulkanDeviceFunctions* deviceFunctions, VkCommandPool commandPool, VkQueue queue);
    void shutdown();

public:
    void poll();
    void waitAll();

    void enqueue(Job job);

private:
    VkCommandBuffer beginSingleTimeCommands();
    void submit(Job& job);
    void cleanupFinishedJob(Job& job);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    QVulkanDeviceFunctions* m_deviceFunctions = nullptr;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;

private:
    QVector<Job> m_pending;
};

#endif // VKUPLOADCONTEXT_H
