#include "vkmaterialsystem.h"

void MaterialSystem::init(VkDevice device, QVulkanDeviceFunctions* deviceFunctions, VkDescriptorPool descriptorPool)
{
    m_device = device;
    m_deviceFunctions = deviceFunctions;
    m_descriptorPool = descriptorPool;

    if (!m_device || !deviceFunctions || !m_descriptorPool)
        qFatal("MaterialSystem::init: invalid args");

    createSetLayout();
    createSampler();
}

void MaterialSystem::shutdown()
{
    if (!m_device || !m_deviceFunctions)
        return;

    if (m_sampler) {
        m_deviceFunctions->vkDestroySampler(m_device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    if (m_materialSetLayout) {
        m_deviceFunctions->vkDestroyDescriptorSetLayout(m_device, m_materialSetLayout, nullptr);
        m_materialSetLayout = VK_NULL_HANDLE;
    }

    m_device = VK_NULL_HANDLE;
    m_deviceFunctions = nullptr;
    m_descriptorPool = VK_NULL_HANDLE;
}

void MaterialSystem::createSetLayout()
{
    // set=1 binding=0 sampler2D
    VkDescriptorSetLayoutBinding sampler{};
    sampler.binding = 0;
    sampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler.descriptorCount = 1;
    sampler.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &sampler;

    VkResult result = m_deviceFunctions->vkCreateDescriptorSetLayout(m_device, &info, nullptr, &m_materialSetLayout);
    if (result != VK_SUCCESS)
        qFatal("MaterialSystem: vkCreateDescriptorSetLayout failed: %d", result);
}

void MaterialSystem::createSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    VkResult result = m_deviceFunctions->vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler);
    if (result != VK_SUCCESS)
        qFatal("MaterialSystem: vkCreateSampler failed: %d", result);
}

VkGPUResources::MaterialInstance MaterialSystem::createInstance(const VkGPUResources::TextureGPU* texture)
{
    if (!texture || !texture->textureImageView)
        qFatal("MaterialSystem::createInstance: invalid texture");

    VkGPUResources::MaterialInstance materialInstance{};

    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = m_descriptorPool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &m_materialSetLayout;

    VkResult result = m_deviceFunctions->vkAllocateDescriptorSets(m_device, &alloc, &materialInstance.descriptorSet);
    if (result != VK_SUCCESS)
        qFatal("MaterialSystem: vkAllocateDescriptorSets failed: %d", result);

    VkDescriptorImageInfo descriptorImageInfo{};
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorImageInfo.imageView = texture->textureImageView;
    descriptorImageInfo.sampler = m_sampler;

    VkWriteDescriptorSet writeDescriptorSet{};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = materialInstance.descriptorSet;
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.pImageInfo = &descriptorImageInfo;

    m_deviceFunctions->vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);

    return materialInstance;
}
