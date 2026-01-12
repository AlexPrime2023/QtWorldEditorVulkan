#include "renderer.h"

#include <QFile>
#include <QTime>
#include <QVulkanFunctions>
#include <array>

#include "vkutils.h"
#include "vulkanwindow.h"

static const QString DEFAULT_TEXTURE_PATH = ":/textures/default.png";
static const int MAX_DESCRIPTOR_POOL_SIZE = 1024;

Renderer::Renderer(VulkanWindow *window)
    : m_window(window)
{
}

void Renderer::initResources()
{
    VkDevice device = m_window->device();
    m_deviceFunctions = m_window->vulkanInstance()->deviceFunctions(device);

    m_uploadContext.init(device, m_deviceFunctions, m_window->graphicsCommandPool(), m_window->graphicsQueue());
    m_gpuCache.init(device, m_deviceFunctions, m_window->vulkanInstance()->functions(), m_window->physicalDevice(), &m_uploadContext);

    // TODO: Fix it
    createDescriptorPool(MAX_DESCRIPTOR_POOL_SIZE);

    createDescriptorSetLayouts();
    createCameraResources();
    createCameraDescriptorSets();

    m_materialSystem.init(device, m_deviceFunctions, m_descriptorPool);

    initPipeline();
}

void Renderer::releaseResources()
{
    VkDevice device = m_window->device();

    m_deviceFunctions->vkDeviceWaitIdle(device);
    m_uploadContext.shutdown();
    m_gpuCache.shutdown();

    for (VkRenderItem* renderItem : m_renderItems)
        delete renderItem;
    m_renderItems.clear();

    if (m_descriptorPool)
        m_deviceFunctions->vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);

    for (auto& frame : m_frames)
    {
        if (frame.cameraMapped) {
            m_deviceFunctions->vkUnmapMemory(device, frame.cameraMem);
            frame.cameraMapped = nullptr;
        }
        if (frame.cameraBuffer)
            m_deviceFunctions->vkDestroyBuffer(device, frame.cameraBuffer, nullptr);
        if (frame.cameraMem)
            m_deviceFunctions->vkFreeMemory(device, frame.cameraMem, nullptr);
    }
    m_frames.clear();

    m_materialSystem.shutdown();

    m_deviceFunctions->vkDestroyPipeline(device, m_litPipeline, nullptr);
    m_deviceFunctions->vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);

    m_deviceFunctions->vkDestroyDescriptorSetLayout(device, m_cameraSetLayout, nullptr);
}

void Renderer::startNextFrame()
{
    updateCameraUBOForCurrentFrame();
    m_uploadContext.poll();

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_window->defaultRenderPass();
    renderPassInfo.framebuffer = m_window->currentFramebuffer();
    renderPassInfo.renderArea.offset.x = 0;
    renderPassInfo.renderArea.offset.y = 0;
    const QSize swapChainImageSize = m_window->swapChainImageSize();
    renderPassInfo.renderArea.extent.width = swapChainImageSize.width();
    renderPassInfo.renderArea.extent.height = swapChainImageSize.height();

    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {0.3f, 0.3f, 0.3f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    VkCommandBuffer commandBuffer = m_window->currentCommandBuffer();
    m_deviceFunctions->vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = swapChainImageSize.width();
    viewport.height = swapChainImageSize.height();
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    m_deviceFunctions->vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = viewport.width;
    scissor.extent.height = viewport.height;
    m_deviceFunctions->vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    for (auto renderItem : m_renderItems)
        drawRenderItem(renderItem);

    m_deviceFunctions->vkCmdEndRenderPass(commandBuffer);

    m_window->frameReady();
    m_window->requestUpdate();
}

void Renderer::addObject(const CpuMesh& cpuMesh)
{
    auto* renderItem = new VkRenderItem();

    // TODO: Fix names
    QString meshId = QString::number(cpuMesh.vertices.size()) + QString::number(cpuMesh.indices.size());
    VkGPUResources::MeshGPU* meshGPU = m_gpuCache.getOrCreateMesh(meshId, cpuMesh);
    renderItem->meshGPU = meshGPU;

    QString texPath = cpuMesh.albedoPath.isEmpty() ? DEFAULT_TEXTURE_PATH : cpuMesh.albedoPath;
    QString texId = "tex:" + texPath;
    VkGPUResources::TextureGPU* textureGPU = m_gpuCache.getOrCreateTexture(texId, texPath);

    renderItem->materialInstance = m_materialSystem.createInstance(textureGPU);

    m_renderItems.push_back(renderItem);

    m_window->requestUpdate();
}

void Renderer::createDescriptorPool(uint32_t maxObjects)
{
    const uint32_t frameCount = uint32_t(m_window->concurrentFrameCount());

    // set=1: one COMBINED_IMAGE_SAMPLER per object/material
    // set=0: one UNIFORM_BUFFER (camera) per frame (frames-in-flight)
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = maxObjects;

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = frameCount;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = uint32_t(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    // Total number of VkDescriptorSet allocated:
    // - maxObjects (materials set=1)
    // - frameCount (camera set=0)
    poolInfo.maxSets = maxObjects + frameCount;

    VkDevice device = m_window->device();

    if (m_descriptorPool) {
        m_deviceFunctions->vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    VkResult result = m_deviceFunctions->vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool);
    if (result != VK_SUCCESS)
        qFatal("Failed to create descriptor pool: %d", result);
}

void Renderer::createDescriptorSetLayouts()
{
    VkDevice device = m_window->device();

    // --- set=0 : Camera UBO (binding=0) ---
    VkDescriptorSetLayoutBinding cameraUbo{};
    cameraUbo.binding = 0;
    cameraUbo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraUbo.descriptorCount = 1;
    cameraUbo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo camLayoutInfo{};
    camLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    camLayoutInfo.bindingCount = 1;
    camLayoutInfo.pBindings = &cameraUbo;

    if (m_cameraSetLayout) {
        m_deviceFunctions->vkDestroyDescriptorSetLayout(device, m_cameraSetLayout, nullptr);
        m_cameraSetLayout = VK_NULL_HANDLE;
    }

    VkResult result = m_deviceFunctions->vkCreateDescriptorSetLayout(device, &camLayoutInfo, nullptr, &m_cameraSetLayout);
    if (result != VK_SUCCESS)
        qFatal("Failed to create camera set layout: %d", result);
}

void Renderer::createCameraResources()
{
    // TODO Check of swapChainImageCount return 0
    int frameCount = m_window->concurrentFrameCount();
    m_frames.resize(frameCount);

    VkDevice device = m_window->device();
    VkDeviceSize deviceSize = sizeof(VkFrameResources::CameraUBO);

    for (int i = 0; i < m_frames.size(); ++i)
    {
        VkUtils::createBuffer(deviceSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_frames[i].cameraBuffer, m_frames[i].cameraMem, m_deviceFunctions, m_window->vulkanInstance()->functions(), device, m_window->physicalDevice());

        VkResult result = m_deviceFunctions->vkMapMemory(device, m_frames[i].cameraMem, 0, deviceSize, 0, &m_frames[i].cameraMapped);
        if (result != VK_SUCCESS || !m_frames[i].cameraMapped)
            qFatal("vkMapMemory camera failed r=%d ptr=%p", result, m_frames[i].cameraMapped);
    }
}

void Renderer::createCameraDescriptorSets()
{
    VkDevice device = m_window->device();

    for (int i = 0; i < m_frames.size(); ++i)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
        descriptorSetAllocateInfo.descriptorSetCount = 1;
        descriptorSetAllocateInfo.pSetLayouts = &m_cameraSetLayout;

        VkResult result = m_deviceFunctions->vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &m_frames[i].cameraSet);
        if (result != VK_SUCCESS)
            qFatal("Failed to allocate camera descriptor set: %d", result);

        VkDescriptorBufferInfo descriptorBufferInfo{};
        descriptorBufferInfo.buffer = m_frames[i].cameraBuffer;
        descriptorBufferInfo.offset = 0;
        descriptorBufferInfo.range  = sizeof(VkFrameResources::CameraUBO);

        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = m_frames[i].cameraSet;
        writeDescriptorSet.dstBinding = 0;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;

        m_deviceFunctions->vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
    }
}

void Renderer::updateCameraUBOForCurrentFrame()
{
    if (m_frames.isEmpty())
        return;

    int frameIndex = m_window->currentFrame();

    // TODO: Integrate the camera
    VkFrameResources::CameraUBO cameraUBO{};
    QVector3D eye(1.0f ,1.0f, 1.0f), center(0.0f ,0.0f ,0.0f), up(0.0f, 1.0f, 0.0f);

    cameraUBO.view.setToIdentity();
    cameraUBO.view.lookAt(eye, center, up);

    QSize size = m_window->swapChainImageSize();
    float aspect = float(size.width()) / float(size.height());

    cameraUBO.projection = m_window->clipCorrectionMatrix();
    cameraUBO.projection.perspective(45.0f, aspect, 0.01f, 100.0f);

    if (!m_frames[frameIndex].cameraMapped)
        qFatal("Camera buffer not mapped (frame=%d)", frameIndex);

    // QMatrix4x4 = 64 bytes (16 floats)
    memcpy(static_cast<char*>(m_frames[frameIndex].cameraMapped) + 0,  cameraUBO.view.constData(), 64);
    memcpy(static_cast<char*>(m_frames[frameIndex].cameraMapped) + 64, cameraUBO.projection.constData(), 64);
}

void Renderer::initPipeline()
{
    VkDevice device = m_window->device();
    QByteArray vertShaderCode = FileUtils::readFile(":shaders/shader.vert.spv");
    QByteArray fragShaderCode = FileUtils::readFile(":shaders/shader.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo = {};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = 2;
    dynamicInfo.pDynamicStates = dynamicStates;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription = VkUtils::VertexInput::Lit::bindingDescription();
    auto attributeDescriptions = VkUtils::VertexInput::Lit::attributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterizationInfo = {};
    rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationInfo.depthClampEnable = VK_FALSE;
    rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationInfo.depthBiasEnable = VK_FALSE;
    rasterizationInfo.depthBiasConstantFactor = 0.0f;
    rasterizationInfo.depthBiasClamp = 0.0f;
    rasterizationInfo.depthBiasSlopeFactor = 0.0f;
    rasterizationInfo.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(VkFrameResources::PushModel);

    VkDescriptorSetLayout setLayouts[2] = { m_cameraSetLayout, m_materialSystem.materialSetLayout() };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    VkResult result = m_deviceFunctions->vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    if (result != VK_SUCCESS)
        qFatal("Failed to create pipeline layout: %d", result);

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pDynamicState = &dynamicInfo;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizationInfo;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_window->defaultRenderPass();
    pipelineInfo.subpass = 0;
    pipelineInfo.pDepthStencilState = &depthStencil;

    result = m_deviceFunctions->vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_litPipeline);

    if (result != VK_SUCCESS)
        qFatal("Failed to graphics pipeline: %d", result);

    m_deviceFunctions->vkDestroyShaderModule(device, fragShaderModule, nullptr);
    m_deviceFunctions->vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

VkShaderModule Renderer::createShaderModule(const QByteArray &code)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size_t(code.size());
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.constData());

    VkShaderModule shaderModule;
    VkDevice device = m_window->device();
    VkResult result = m_deviceFunctions->vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS)
        QDebug(QtFatalMsg) << QLatin1String("Failed to create shader module:") << result;

    return shaderModule;
}

void Renderer::drawRenderItem(VkRenderItem* renderItem)
{
    if (!renderItem)
        return;

    if (!renderItem->meshGPU || !renderItem->meshGPU->isReady)
        return;

    VkCommandBuffer commandBuffer = m_window->currentCommandBuffer();
    m_deviceFunctions->vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_litPipeline);

    const int frameIndex = m_window->currentFrame();

    VkDescriptorSet sets[2] = {
        m_frames[frameIndex].cameraSet,             // set=0
        renderItem->materialInstance.descriptorSet  // set=1
    };

    m_deviceFunctions->vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 2, sets, 0, nullptr);

    // model -> push constants
    VkFrameResources::PushModel pushModelConstant{};
    pushModelConstant.modelMatrix.setToIdentity();
    pushModelConstant.modelMatrix *= renderItem->modelMatrix;

    m_deviceFunctions->vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkFrameResources::PushModel), &pushModelConstant);

    VkBuffer buffer[] = { renderItem->meshGPU->vertexBuffer };
    VkDeviceSize offset[] = { 0 };
    m_deviceFunctions->vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffer, offset);
    m_deviceFunctions->vkCmdBindIndexBuffer(commandBuffer, renderItem->meshGPU->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    m_deviceFunctions->vkCmdDrawIndexed(commandBuffer, renderItem->meshGPU->indexCount, 1, 0, 0, 0);
}
