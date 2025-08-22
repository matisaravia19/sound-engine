#include "vulkan.h"

const std::vector<const char*> DEVICE_EXTENSIONS = {
	VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
	VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
	VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
	VK_KHR_SPIRV_1_4_EXTENSION_NAME,
	VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
};

const uint64_t STAGING_BUFFER_SIZE = 1024 * 1024 * 10; // 10 MB

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

vk::Fence CreateFence(vk::Device& device)
{
	vk::FenceCreateInfo fenceInfo = vk::FenceCreateInfo();
	return device.createFence(fenceInfo);
}

void se::GpuProgram::Init()
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

	instance = CreateVulkanInstance();
	physicalDevice = GetPhysicalDevice(instance);

	accelerationStructureProperties = physicalDevice
		.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceAccelerationStructurePropertiesKHR>()
		.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();
	raytracingProperties = physicalDevice
		.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>()
		.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

	uint32_t queueFamilyIndex = GetQueueFamilyIndex(physicalDevice);

	device = CreateLogicalDevice(physicalDevice, queueFamilyIndex);
	queue = device.getQueue(queueFamilyIndex, 0);

	commandPool = CreateCommandPool(device, queueFamilyIndex);
	commandBuffer = CreateCommandBuffer(device, commandPool);
	fence = CreateFence(device);
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

	return GpuBuffer{ buffer, bufferMemory, bufferAddress, size };
}

void se::GpuProgram::UploadToBuffer(se::GpuBuffer& buffer, void* data, vk::DeviceSize size)
{
	if (!stagingBuffer) InitStagingBuffer();

	void* mappedData = device.mapMemory(stagingBuffer->memory, 0, size);
	std::memcpy(mappedData, data, size);
	device.unmapMemory(stagingBuffer->memory);

	BeginCommands();

	vk::BufferCopy copyRegion = vk::BufferCopy()
		.setDstOffset(0)
		.setSize(size);
	commandBuffer.copyBuffer(stagingBuffer->buffer, buffer.buffer, 1, &copyRegion);

	SubmitCommandsAndWait();
}

void se::GpuProgram::DownloadFromBuffer(se::GpuBuffer& buffer, void* data, vk::DeviceSize size)
{
	if (!stagingBuffer) InitStagingBuffer();

	BeginCommands();

	vk::BufferCopy copyRegion = vk::BufferCopy()
		.setSrcOffset(0)
		.setSize(size);
	commandBuffer.copyBuffer(buffer.buffer, stagingBuffer->buffer, 1, &copyRegion);

	SubmitCommandsAndWait();

	void* mappedData = device.mapMemory(stagingBuffer->memory, 0, size);
	std::memcpy(data, mappedData, size);
	device.unmapMemory(stagingBuffer->memory);
}

void se::GpuProgram::CopyBuffer(se::GpuBuffer& src, se::GpuBuffer& dst, vk::DeviceSize size)
{
	BeginCommands();

	vk::BufferCopy copyRegion = vk::BufferCopy()
		.setSrcOffset(0)
		.setDstOffset(0)
		.setSize(size);
	commandBuffer.copyBuffer(src.buffer, dst.buffer, 1, &copyRegion);

	SubmitCommandsAndWait();
}

void se::GpuProgram::ClearBuffer(se::GpuBuffer& buffer)
{
	BeginCommands();
	commandBuffer.fillBuffer(buffer.buffer, 0, buffer.size, 0);
	SubmitCommandsAndWait();
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

void se::GpuProgram::WaitForFence()
{
	auto result = device.waitForFences(fence, VK_TRUE, UINT64_MAX);
	if (result != vk::Result::eSuccess)
	{
		throw std::runtime_error("Failed to wait for fence");
	}

	device.resetFences(fence);
}

void se::GpuProgram::BeginCommands()
{
	vk::CommandBufferBeginInfo beginInfo = vk::CommandBufferBeginInfo()
		.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

	commandBuffer.begin(beginInfo);
}

void se::GpuProgram::SubmitCommandsAndWait()
{
	commandBuffer.end();

	vk::SubmitInfo submitInfo = vk::SubmitInfo()
		.setCommandBufferCount(1)
		.setPCommandBuffers(&commandBuffer);

	queue.submit(submitInfo, fence);
	WaitForFence();

	commandBuffer.reset();
}

se::AccelerationStructure se::GpuProgram::CreateBottomLevelAccelerationStructure(const std::vector<GpuMesh>& meshes)
{
	std::vector<vk::AccelerationStructureGeometryKHR> geometries;
	std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRangeInfos;

	for (const auto& mesh : meshes)
	{
		// Setup vertex data
		auto vertexData = vk::AccelerationStructureGeometryTrianglesDataKHR()
			.setVertexFormat(vk::Format::eR32G32B32Sfloat)
			.setVertexData(mesh.vertexBuffer.address)
			.setVertexStride(sizeof(se::Vertex))
			.setMaxVertex(static_cast<uint32_t>(mesh.vertexBuffer.size / sizeof(se::Vertex) - 1))
			.setIndexType(vk::IndexType::eUint32)
			.setIndexData(mesh.indexBuffer.address);

		auto geometry = vk::AccelerationStructureGeometryKHR()
			.setGeometryType(vk::GeometryTypeKHR::eTriangles)
			.setGeometry(vk::AccelerationStructureGeometryDataKHR().setTriangles(vertexData))
			.setFlags(vk::GeometryFlagBitsKHR::eOpaque);

		geometries.push_back(geometry);

		auto buildRangeInfo = vk::AccelerationStructureBuildRangeInfoKHR()
			.setPrimitiveCount(static_cast<uint32_t>(mesh.indexBuffer.size / sizeof(uint32_t) / 3))
			.setPrimitiveOffset(0)
			.setFirstVertex(0)
			.setTransformOffset(0);

		buildRangeInfos.push_back(buildRangeInfo);
	}

	// Get size requirements
	auto buildGeometryInfo = vk::AccelerationStructureBuildGeometryInfoKHR()
		.setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
		.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
		.setGeometryCount(static_cast<uint32_t>(geometries.size()))
		.setPGeometries(geometries.data());

	std::vector<uint32_t> maxPrimitiveCounts;
	for (const auto& info : buildRangeInfos)
	{
		maxPrimitiveCounts.push_back(info.primitiveCount);
	}

	auto sizeInfo = device.getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice,
		buildGeometryInfo,
		maxPrimitiveCounts);

	// Create acceleration structure buffer
	auto asBuffer = GetBuffer(
		sizeInfo.accelerationStructureSize,
		vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Create acceleration structure
	auto createInfo = vk::AccelerationStructureCreateInfoKHR()
		.setBuffer(asBuffer.buffer)
		.setSize(sizeInfo.accelerationStructureSize)
		.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);

	auto as = device.createAccelerationStructureKHR(createInfo);

	// Get acceleration structure address
	auto addressInfo = vk::AccelerationStructureDeviceAddressInfoKHR()
		.setAccelerationStructure(as);
	auto asAddress = device.getAccelerationStructureAddressKHR(addressInfo);

	// Create scratch buffer
	auto scratchBuffer = GetBuffer(
		sizeInfo.buildScratchSize,
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Build acceleration structure
	buildGeometryInfo.setDstAccelerationStructure(as)
		.setScratchData(scratchBuffer.address);

	const vk::AccelerationStructureBuildRangeInfoKHR* buildRangeInfoPtr = buildRangeInfos.data();

	BeginCommands();
	commandBuffer.buildAccelerationStructuresKHR(1, &buildGeometryInfo, &buildRangeInfoPtr);

	// Add memory barrier
	auto barrier = vk::MemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eAccelerationStructureWriteKHR)
		.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureReadKHR);

	commandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
		vk::PipelineStageFlagBits::eRayTracingShaderKHR,
		{}, 1, &barrier, 0, nullptr, 0, nullptr);

	SubmitCommandsAndWait();

	// Clean up scratch buffer
	FreeBuffer(scratchBuffer);

	return AccelerationStructure{ as, asBuffer, asAddress };
}

se::AccelerationStructure se::GpuProgram::CreateTopLevelAccelerationStructure(const std::vector<AccelerationStructure>& bottomLevelAS)
{
	// Create instances buffer
	std::vector<vk::AccelerationStructureInstanceKHR> instances;

	for (size_t i = 0; i < bottomLevelAS.size(); ++i)
	{
		// Identity matrix for transform
		std::array<std::array<float, 4>, 3> transform = {{
															 {{ 1.0f, 0.0f, 0.0f, 0.0f }},
															 {{ 0.0f, 1.0f, 0.0f, 0.0f }},
															 {{ 0.0f, 0.0f, 1.0f, 0.0f }}
														 }};

		auto instance = vk::AccelerationStructureInstanceKHR()
			.setTransform(reinterpret_cast<vk::TransformMatrixKHR&>(transform))
			.setInstanceCustomIndex(static_cast<uint32_t>(i))
			.setMask(0xFF)
			.setInstanceShaderBindingTableRecordOffset(0)
			.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
			.setAccelerationStructureReference(bottomLevelAS[i].address);

		instances.push_back(instance);
	}

	auto instancesBuffer = GetBuffer(
		instances.size() * sizeof(vk::AccelerationStructureInstanceKHR),
		vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	UploadToBuffer(instancesBuffer, instances.data(), instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));

	// Setup geometry
	auto instancesData = vk::AccelerationStructureGeometryInstancesDataKHR()
		.setArrayOfPointers(false)
		.setData(instancesBuffer.address);

	auto geometry = vk::AccelerationStructureGeometryKHR()
		.setGeometryType(vk::GeometryTypeKHR::eInstances)
		.setGeometry(vk::AccelerationStructureGeometryDataKHR().setInstances(instancesData));

	// Get size requirements
	auto buildGeometryInfo = vk::AccelerationStructureBuildGeometryInfoKHR()
		.setType(vk::AccelerationStructureTypeKHR::eTopLevel)
		.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
		.setGeometryCount(1)
		.setPGeometries(&geometry);

	uint32_t instanceCount = static_cast<uint32_t>(instances.size());
	auto sizeInfo = device.getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice,
		buildGeometryInfo,
		instanceCount);

	// Create acceleration structure buffer
	auto asBuffer = GetBuffer(
		sizeInfo.accelerationStructureSize,
		vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Create acceleration structure
	auto createInfo = vk::AccelerationStructureCreateInfoKHR()
		.setBuffer(asBuffer.buffer)
		.setSize(sizeInfo.accelerationStructureSize)
		.setType(vk::AccelerationStructureTypeKHR::eTopLevel);

	auto as = device.createAccelerationStructureKHR(createInfo);

	// Get acceleration structure address
	auto addressInfo = vk::AccelerationStructureDeviceAddressInfoKHR()
		.setAccelerationStructure(as);
	auto asAddress = device.getAccelerationStructureAddressKHR(addressInfo);

	// Create scratch buffer
	auto scratchBuffer = GetBuffer(
		sizeInfo.buildScratchSize,
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Build acceleration structure
	buildGeometryInfo.setDstAccelerationStructure(as)
		.setScratchData(scratchBuffer.address);

	auto buildRangeInfo = vk::AccelerationStructureBuildRangeInfoKHR()
		.setPrimitiveCount(instanceCount)
		.setPrimitiveOffset(0)
		.setFirstVertex(0)
		.setTransformOffset(0);

	const vk::AccelerationStructureBuildRangeInfoKHR* buildRangeInfoPtr = &buildRangeInfo;

	BeginCommands();
	commandBuffer.buildAccelerationStructuresKHR(1, &buildGeometryInfo, &buildRangeInfoPtr);

	// Add memory barrier
	auto barrier = vk::MemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eAccelerationStructureWriteKHR)
		.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureReadKHR);

	commandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
		vk::PipelineStageFlagBits::eRayTracingShaderKHR,
		{}, 1, &barrier, 0, nullptr, 0, nullptr);

	SubmitCommandsAndWait();

	// Clean up
	FreeBuffer(scratchBuffer);
	FreeBuffer(instancesBuffer);

	return AccelerationStructure{ as, asBuffer, asAddress };
}

void se::GpuProgram::DestroyAccelerationStructure(AccelerationStructure& as)
{
	if (as.handle)
	{
		device.destroyAccelerationStructureKHR(as.handle);
		as.handle = VK_NULL_HANDLE;
	}
	FreeBuffer(as.buffer);
	as.address = 0;
}

std::vector<uint32_t> se::GpuProgram::CompileShader(const std::string& source, const std::string& filename, vk::ShaderStageFlagBits stage)
{
	// This is a simplified version - in practice you'd use glslang to compile
	// For now, we'll assume pre-compiled SPIR-V binaries
	throw std::runtime_error("Shader compilation not implemented - use pre-compiled SPIR-V");
}

se::RaytracingPipeline se::GpuProgram::CreateRaytracingPipeline()
{
	RaytracingPipeline pipeline{};

	// Create descriptor set layout
	std::vector<vk::DescriptorSetLayoutBinding> bindings = {
		vk::DescriptorSetLayoutBinding()
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR),
		vk::DescriptorSetLayoutBinding()
			.setBinding(1)
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR)
	};

	auto layoutInfo = vk::DescriptorSetLayoutCreateInfo()
		.setBindingCount(static_cast<uint32_t>(bindings.size()))
		.setPBindings(bindings.data());

	pipeline.descriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);

	// Create pipeline layout
	auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
		.setSetLayoutCount(1)
		.setPSetLayouts(&pipeline.descriptorSetLayout);

	pipeline.layout = device.createPipelineLayout(pipelineLayoutInfo);

	// For now, we'll create a minimal pipeline without shaders
	// In practice, you'd load and compile the shader SPIR-V here

	// Create descriptor pool
	std::vector<vk::DescriptorPoolSize> poolSizes = {
		vk::DescriptorPoolSize()
			.setType(vk::DescriptorType::eAccelerationStructureKHR)
			.setDescriptorCount(1),
		vk::DescriptorPoolSize()
			.setType(vk::DescriptorType::eStorageImage)
			.setDescriptorCount(1)
	};

	auto poolInfo = vk::DescriptorPoolCreateInfo()
		.setPoolSizeCount(static_cast<uint32_t>(poolSizes.size()))
		.setPPoolSizes(poolSizes.data())
		.setMaxSets(1);

	pipeline.descriptorPool = device.createDescriptorPool(poolInfo);

	// Allocate descriptor set
	auto allocInfo = vk::DescriptorSetAllocateInfo()
		.setDescriptorPool(pipeline.descriptorPool)
		.setDescriptorSetCount(1)
		.setPSetLayouts(&pipeline.descriptorSetLayout);

	auto descriptorSets = device.allocateDescriptorSets(allocInfo);
	pipeline.descriptorSet = descriptorSets[0];

	return pipeline;
}

void se::GpuProgram::DestroyRaytracingPipeline(RaytracingPipeline& pipeline)
{
	if (pipeline.pipeline)
	{
		device.destroyPipeline(pipeline.pipeline);
		pipeline.pipeline = VK_NULL_HANDLE;
	}

	if (pipeline.layout)
	{
		device.destroyPipelineLayout(pipeline.layout);
		pipeline.layout = VK_NULL_HANDLE;
	}

	if (pipeline.descriptorSetLayout)
	{
		device.destroyDescriptorSetLayout(pipeline.descriptorSetLayout);
		pipeline.descriptorSetLayout = VK_NULL_HANDLE;
	}

	if (pipeline.descriptorPool)
	{
		device.destroyDescriptorPool(pipeline.descriptorPool);
		pipeline.descriptorPool = VK_NULL_HANDLE;
	}

	FreeBuffer(pipeline.raygenShaderBindingTable);
	FreeBuffer(pipeline.missShaderBindingTable);
	FreeBuffer(pipeline.hitShaderBindingTable);
}

se::RaytracingProgram::RaytracingProgram(std::shared_ptr<GpuProgram> gpuProgram)
{
	this->gpuProgram = std::move(gpuProgram);
}

void se::RaytracingProgram::Init(const std::vector<GpuMesh>& meshes)
{
	if (meshes.empty()) throw std::runtime_error("No meshes provided for raytracing");

	for (const auto& mesh : meshes)
	{
		bottomLevelAS.push_back(gpuProgram->CreateBottomLevelAccelerationStructure({ mesh }));
	}

	topLevelAS = gpuProgram->CreateTopLevelAccelerationStructure(bottomLevelAS);
	rtPipeline = gpuProgram->CreateRaytracingPipeline();

	// Setup shader binding tables (SBT)
	// For simplicity, we'll assume the SBT is already set up in the shaders
}

void se::RaytracingProgram::Destroy()
{
	gpuProgram->DestroyRaytracingPipeline(rtPipeline);

	gpuProgram->DestroyAccelerationStructure(topLevelAS);
	for (auto& blas : bottomLevelAS)
	{
		gpuProgram->DestroyAccelerationStructure(blas);
	}
}
