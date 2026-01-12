#ifndef VULKANWINDOW_H
#define VULKANWINDOW_H

#include <QVulkanWindow>

#include "renderer.h"

// TODO: Rename to Viewport3D
class VulkanWindow : public QVulkanWindow
{
    Q_OBJECT

public:
    VulkanWindow(QWindow *parentWindow = nullptr);

public:
    QVulkanWindowRenderer *createRenderer() override;

    void addObject(const CpuMesh& mesh);

private:
    QVulkanInstance m_instance;
    Renderer *m_renderer = nullptr;

private:
    void pickPhysicalDevice();
};

#endif // VULKANWINDOW_H
