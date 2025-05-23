#include "vulkan.h"

const std::vector<const char*> DEVICE_EXTENSIONS = {
	VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
	VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
	VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
	VK_KHR_SPIRV_1_4_EXTENSION_NAME,
	VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
};

const uint64_t STAGING_BUFFER_SIZE = 1024 * 1024 * 1; // 1 MB

vk::Instance CreateVulkanInstance()
{
	auto applicationInfo = vk::ApplicationInfo()
		.setPApplicationName("Sound Engine")
		.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
		.setPEngineName("Sound Engine")
		.setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
		.setApiVersion(vk::ApiVersion14);

	auto instanceInfo = vk::InstanceCreateInfo()
		.setPApplicationInfo(&applicationInfo)
		.setEnabledExtensionCount(0)
		.setPpEnabledExtensionNames(nullptr);

	return vk::createInstance(instanceInfo);
}

vk::PhysicalDevice GetPhysicalDevice(vk::Instance instance)
{
	auto physicalDevices = instance.enumeratePhysicalDevices();
	if (physicalDevices.empty())
	{
		throw std::runtime_error("No physical devices found");
	}

	return physicalDevices[0];
}

uint32_t GetQueueFamilyIndex(vk::PhysicalDevice& physicalDevice)
{
	auto queueFamilies = physicalDevice.getQueueFamilyProperties();
	for (uint32_t i = 0; i < queueFamilies.size(); ++i)
	{
		auto flags = queueFamilies[i].queueFlags;
		if (flags & vk::QueueFlagBits::eGraphics && flags & vk::QueueFlagBits::eCompute)
		{
			return i;
		}
	}

	throw std::runtime_error("No suitable queue family found");
}

vk::Device CreateLogicalDevice(vk::PhysicalDevice& physicalDevice, uint32_t queueFamilyIndex)
{
	auto accelerationStructureInfo = vk::PhysicalDeviceAccelerationStructureFeaturesKHR()
		.setAccelerationStructure(true);

	auto rayTracingPipelineInfo = vk::PhysicalDeviceRayTracingPipelineFeaturesKHR()
		.setRayTracingPipeline(true)
		.setPNext(&accelerationStructureInfo);

	auto bufferDeviceAddressInfo = vk::PhysicalDeviceBufferDeviceAddressFeatures()
		.setBufferDeviceAddress(true)
		.setPNext(&rayTracingPipelineInfo);

	auto deviceFeatures = vk::PhysicalDeviceFeatures2()
		.setPNext(&bufferDeviceAddressInfo);

	float queuePriority = 1.0f;
	auto queueCreateInfo = vk::DeviceQueueCreateInfo()
		.setQueueFamilyIndex(queueFamilyIndex)
		.setQueueCount(1)
		.setPQueuePriorities(&queuePriority);;

	auto deviceCreateInfo = vk::DeviceCreateInfo()
		.setQueueCreateInfoCount(1)
		.setPQueueCreateInfos(&queueCreateInfo)
		.setEnabledExtensionCount(static_cast<uint32_t>(DEVICE_EXTENSIONS.size()))
		.setPpEnabledExtensionNames(DEVICE_EXTENSIONS.data())
		.setPNext(&deviceFeatures);

	return physicalDevice.createDevice(deviceCreateInfo);
}

vk::CommandPool CreateCommandPool(vk::Device& device, uint32_t queueFamilyIndex)
{
	vk::CommandPoolCreateInfo commandPoolInfo = vk::CommandPoolCreateInfo()
		.setQueueFamilyIndex(queueFamilyIndex)
		.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

	return device.createCommandPool(commandPoolInfo);
}

vk::CommandBuffer CreateCommandBuffer(vk::Device& device, vk::CommandPool& commandPool)
{
	vk::CommandBufferAllocateInfo commandBufferInfo = vk::CommandBufferAllocateInfo()
		.setCommandPool(commandPool)
		.setLevel(vk::CommandBufferLevel::ePrimary)
		.setCommandBufferCount(1);

	auto commandBuffers = device.allocateCommandBuffers(commandBufferInfo);
	return commandBuffers[0];
}

std::unique_ptr<se::GpuProgram> se::GpuProgram::Create()
{
	auto applicationInfo = vk::ApplicationInfo()
		.setPApplicationName("Sound Engine")
		.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
		.setPEngineName("Sound Engine")
		.setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
		.setApiVersion(vk::ApiVersion14);

	auto instanceInfo = vk::InstanceCreateInfo()
		.setPApplicationInfo(&applicationInfo)
		.setEnabledExtensionCount(0)
		.setPpEnabledExtensionNames(nullptr);

	auto program = std::make_unique<GpuProgram>();

	program->instance = CreateVulkanInstance();
	program->physicalDevice = GetPhysicalDevice(program->instance);

	uint32_t queueFamilyIndex = GetQueueFamilyIndex(program->physicalDevice);

	program->device = CreateLogicalDevice(program->physicalDevice, queueFamilyIndex);
	program->queue = program->device.getQueue(queueFamilyIndex, 0);

	program->commandPool = CreateCommandPool(program->device, queueFamilyIndex);
	program->commandBuffer = CreateCommandBuffer(program->device, program->commandPool);

	return program;
}

void se::GpuProgram::Destroy()
{
	device.destroy();
	instance.destroy();
}

void se::GpuProgram::InitStagingBuffer()
{
	stagingBuffer = std::make_unique<GpuBuffer>(GetBuffer(STAGING_BUFFER_SIZE,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
}

uint32_t GetSuitableMemoryType(vk::PhysicalDevice& physicalDevice, vk::MemoryRequirements& memRequirements, vk::MemoryPropertyFlags properties)
{
	auto memoryProperties = physicalDevice.getMemoryProperties();
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
		if ((memRequirements.memoryTypeBits & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw std::runtime_error("No suitable memory type found");
}

se::GpuBuffer se::GpuProgram::GetBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
{
	auto bufferInfo = vk::BufferCreateInfo()
		.setSize(size)
		.setUsage(usage)
		.setSharingMode(vk::SharingMode::eExclusive);

	auto buffer = device.createBuffer(bufferInfo);

	auto memRequirements = device.getBufferMemoryRequirements(buffer);
	auto memoryTypeIndex = GetSuitableMemoryType(physicalDevice, memRequirements, properties);

	auto allocInfo = vk::MemoryAllocateInfo()
		.setAllocationSize(memRequirements.size)
		.setMemoryTypeIndex(memoryTypeIndex);

	auto bufferMemory = device.allocateMemory(allocInfo);
	device.bindBufferMemory(buffer, bufferMemory, 0);

	auto deviceAddressInfo = vk::BufferDeviceAddressInfo()
		.setBuffer(buffer);

	auto bufferAddress = device.getBufferAddress(deviceAddressInfo);

	return GpuBuffer{ buffer, bufferMemory, bufferAddress };
}

void se::GpuProgram::UploadToBuffer(se::GpuBuffer& buffer, void* data, vk::DeviceSize size)
{
	if (!stagingBuffer) InitStagingBuffer();

	void* mappedData = device.mapMemory(stagingBuffer->memory, 0, size);
	std::memcpy(mappedData, data, size);
	device.unmapMemory(stagingBuffer->memory);

	vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo()
		.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

	commandBuffer.begin(beginInfo);
	vk::BufferCopy copyRegion = vk::BufferCopy()
		.setDstOffset(0)
		.setSize(size);
	commandBuffer.copyBuffer(stagingBuffer->buffer, buffer.buffer, 1, &copyRegion);
	commandBuffer.end();

	vk::SubmitInfo submitInfo = vk::SubmitInfo()
		.setCommandBufferCount(1)
		.setPCommandBuffers(&commandBuffer);

	queue.submit(submitInfo);
	queue.waitIdle();
}

void se::GpuProgram::FreeBuffer(se::GpuBuffer& buffer)
{
	if (buffer.buffer)
	{
		device.destroyBuffer(buffer.buffer);
		buffer.buffer = VK_NULL_HANDLE;
	}

	if (buffer.memory)
	{
		device.freeMemory(buffer.memory);
		buffer.memory = VK_NULL_HANDLE;
	}

	buffer.address = 0;
}
