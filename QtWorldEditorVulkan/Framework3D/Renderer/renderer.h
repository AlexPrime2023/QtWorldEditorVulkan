#ifndef RENDERER_H
#define RENDERER_H

#include <QVulkanWindowRenderer>
#include <QVulkanDeviceFunctions>
#include <QSharedPointer>
#include <QVector>

#include "vkmaterialsystem.h"
#include "vkuploadcontext.h"
#include "vkgpuresourcecache.h"

#include "vkrenderitem.h"

class VulkanWindow;

class Renderer : public QVulkanWindowRenderer
{
public:
    explicit Renderer(VulkanWindow *window);

    void initResources() override;
    void releaseResources() override;
    void startNextFrame() override;

    // TODO Swapchain resources

public:
    void addObject(const CpuMesh& cpuMesh);

private:
    // Core Vulkan
    VulkanWindow *m_window = nullptr;
    QVulkanDeviceFunctions *m_deviceFunctions = nullptr;

    VkPipelineLayout m_pipelineLayout;

    VkPipeline m_unlitPipeline;
    VkPipeline m_litPipeline;

    // Descriptors
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_cameraSetLayout = VK_NULL_HANDLE;   // set=0

    // Frame Resources
    QVector<VkFrameResources::FrameResources> m_frames;

    // Scene / render items
    QVector<VkRenderItem*> m_renderItems;

    // Sub systems
    UploadContext m_uploadContext;
    GpuResourceCache m_gpuCache;
    MaterialSystem m_materialSystem;

private:
    // Lifecycle utils
    void createDescriptorPool(uint32_t maxSets);

    void createDescriptorSetLayouts();

    void createCameraResources();
    void createCameraDescriptorSets();

    void updateCameraUBOForCurrentFrame();

    // Pipeline
    void initPipeline();
    VkShaderModule createShaderModule(const QByteArray &code);

    // Draw Render Item
    void drawRenderItem(VkRenderItem* renderItem);
};

#endif // RENDERER_H
