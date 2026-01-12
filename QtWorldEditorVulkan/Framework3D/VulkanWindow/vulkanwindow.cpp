#include "vulkanwindow.h"

#include "modelloader.h"
#include "renderer.h"

VulkanWindow::VulkanWindow(QWindow *parentWindow)
    : QVulkanWindow(parentWindow)
{
    //m_instance.setLayers({ "VK_LAYER_KHRONOS_validation" });

    //if (!m_instance.layers().contains("VK_LAYER_KHRONOS_validation")) {
    //    qWarning("VK_LAYER_KHRONOS_validation not available. Install Vulkan SDK / validation layers.");
   // }

    if (!m_instance.create())
    {
        qFatal("Failed to create Vulkan instance: %d", m_instance.errorCode());
    }

    setVulkanInstance(&m_instance);
    pickPhysicalDevice();
}

void VulkanWindow::pickPhysicalDevice()
{
    QVector<VkPhysicalDeviceProperties> devices = availablePhysicalDevices();

    int discreteGPUIndex = -1;
    int integratedGPUIndex = -1;

    for (int i = 0; i < devices.length(); ++i)
    {
        const VkPhysicalDeviceProperties &dev = devices.at(i);

        if (dev.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            discreteGPUIndex = i;
        else if (dev.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            integratedGPUIndex = i;
    }

    if (discreteGPUIndex != -1)
        setPhysicalDeviceIndex(discreteGPUIndex);
    else if (integratedGPUIndex != -1)
        setPhysicalDeviceIndex(integratedGPUIndex);
    else
        qFatal("No Vulkan capable GPU found.");
}

QVulkanWindowRenderer* VulkanWindow::createRenderer()
{
    m_renderer = new Renderer(this);
    return m_renderer;
}

void VulkanWindow::addObject(const CpuMesh& mesh)
{
    if (!m_renderer)
        return;

    m_renderer->addObject(mesh);
}

