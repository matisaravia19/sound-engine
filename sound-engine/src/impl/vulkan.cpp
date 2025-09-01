#include "vulkan.h"
#include "glslang/Public/ShaderLang.h"
#include "SPIRV/GlslangToSpv.h"
#include <fstream>

// Define storage for Vulkan-Hpp's default dynamic dispatcher
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

const std::vector<const char*> DEVICE_EXTENSIONS = {
	VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
	VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
	VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
	VK_KHR_SPIRV_1_4_EXTENSION_NAME,
	VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
};

const uint64_t STAGING_BUFFER_SIZE = 1024 * 1024 * 10; // 10 MB

static void InitializeVulkanDispatcher()
{
	vk::detail::DynamicLoader dl;
	auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
}

vk::Instance CreateVulkanInstance()
{
	InitializeVulkanDispatcher();

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

	auto instance = vk::createInstance(instanceInfo);
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

	return instance;
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

	auto device = physicalDevice.createDevice(deviceCreateInfo);
	VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

	return device;
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

se::BLAccelerationStructure se::GpuProgram::CreateBottomLevelAccelerationStructure(GpuMesh& mesh)
{
	// Setup geometries for each mesh
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

	auto primitiveCount = static_cast<uint32_t>(mesh.indexBuffer.size / sizeof(uint32_t) / 3);

	auto buildRangeInfo = vk::AccelerationStructureBuildRangeInfoKHR()
		.setPrimitiveCount(primitiveCount)
		.setPrimitiveOffset(0)
		.setFirstVertex(0)
		.setTransformOffset(0);

	// Get size requirements
	auto buildGeometryInfo = vk::AccelerationStructureBuildGeometryInfoKHR()
		.setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
		.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
		.setGeometryCount(1)
		.setPGeometries(&geometry);

	auto sizeInfo = device.getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice,
		buildGeometryInfo,
		primitiveCount);

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
		{},
		1,
		&barrier,
		0,
		nullptr,
		0,
		nullptr);

	SubmitCommandsAndWait();

	// Clean up scratch buffer
	FreeBuffer(scratchBuffer);

	return BLAccelerationStructure{ as, asBuffer, asAddress, &mesh };
}

se::TLAccelerationStructure se::GpuProgram::CreateTopLevelAccelerationStructure(std::vector<BLAccelerationStructure>& bottomLevelAS)
{
	// Create instances buffer
	std::vector<vk::AccelerationStructureInstanceKHR> instances;
	for (size_t i = 0; i < bottomLevelAS.size(); ++i)
	{
		auto asInstance = vk::AccelerationStructureInstanceKHR()
			.setTransform(bottomLevelAS[i].mesh->transform)
			.setInstanceCustomIndex(static_cast<uint32_t>(i))
			.setMask(0xFF)
			.setInstanceShaderBindingTableRecordOffset(0)
			.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
			.setAccelerationStructureReference(bottomLevelAS[i].address);

		instances.push_back(asInstance);
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

	return TLAccelerationStructure{ as, asBuffer, asAddress, &bottomLevelAS };
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
	// Compile GLSL to SPIR-V using glslang
	EShLanguage lang = EShLangRayGen;
	switch (stage)
	{
	case vk::ShaderStageFlagBits::eRaygenKHR:
		lang = EShLangRayGen;
		break;
	case vk::ShaderStageFlagBits::eMissKHR:
		lang = EShLangMiss;
		break;
	case vk::ShaderStageFlagBits::eClosestHitKHR:
		lang = EShLangClosestHit;
		break;
	default:
		throw std::runtime_error("Unsupported shader stage");
	}

	glslang::InitializeProcess();
	glslang::TShader shader(lang);
	const char* src = source.c_str();
	shader.setStrings(&src, 1);
	shader.setEntryPoint("main");
	shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientVulkan, 460);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_4);

	TBuiltInResource resources = {};
	// Use glslang default limits
	resources.maxLights = 32;
	resources.maxClipPlanes = 6;
	resources.maxTextureUnits = 32;
	resources.maxTextureCoords = 32;
	resources.maxVertexAttribs = 64;
	resources.maxVertexUniformComponents = 4096;
	resources.maxVaryingFloats = 64;
	resources.maxVertexTextureImageUnits = 32;
	resources.maxCombinedTextureImageUnits = 80;
	resources.maxTextureImageUnits = 32;
	resources.maxFragmentUniformComponents = 4096;
	resources.maxDrawBuffers = 32;
	resources.maxVertexUniformVectors = 128;
	resources.maxVaryingVectors = 8;
	resources.maxFragmentUniformVectors = 16;
	resources.maxVertexOutputVectors = 16;
	resources.maxFragmentInputVectors = 15;
	resources.minProgramTexelOffset = -8;
	resources.maxProgramTexelOffset = 7;
	resources.maxClipDistances = 8;
	resources.maxComputeWorkGroupCountX = 65535;
	resources.maxComputeWorkGroupCountY = 65535;
	resources.maxComputeWorkGroupCountZ = 65535;
	resources.maxComputeWorkGroupSizeX = 1024;
	resources.maxComputeWorkGroupSizeY = 1024;
	resources.maxComputeWorkGroupSizeZ = 64;
	resources.maxComputeUniformComponents = 1024;
	resources.maxComputeTextureImageUnits = 16;
	resources.maxComputeImageUniforms = 8;
	resources.maxComputeAtomicCounters = 8;
	resources.maxComputeAtomicCounterBuffers = 1;
	resources.maxVaryingComponents = 60;
	resources.maxVertexOutputComponents = 64;
	resources.maxGeometryInputComponents = 64;
	resources.maxGeometryOutputComponents = 128;
	resources.maxFragmentInputComponents = 128;
	resources.maxImageUnits = 8;
	resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
	resources.maxCombinedShaderOutputResources = 8;
	resources.maxImageSamples = 0;
	resources.maxVertexImageUniforms = 0;
	resources.maxTessControlImageUniforms = 0;
	resources.maxTessEvaluationImageUniforms = 0;
	resources.maxGeometryImageUniforms = 0;
	resources.maxFragmentImageUniforms = 8;
	resources.maxCombinedImageUniforms = 8;
	resources.maxGeometryTextureImageUnits = 16;
	resources.maxGeometryOutputVertices = 256;
	resources.maxGeometryTotalOutputComponents = 1024;
	resources.maxGeometryUniformComponents = 1024;
	resources.maxGeometryVaryingComponents = 64;
	resources.maxTessControlInputComponents = 128;
	resources.maxTessControlOutputComponents = 128;
	resources.maxTessControlTextureImageUnits = 16;
	resources.maxTessControlUniformComponents = 1024;
	resources.maxTessControlTotalOutputComponents = 4096;
	resources.maxTessEvaluationInputComponents = 128;
	resources.maxTessEvaluationOutputComponents = 128;
	resources.maxTessEvaluationTextureImageUnits = 16;
	resources.maxTessEvaluationUniformComponents = 1024;
	resources.maxTessPatchComponents = 120;
	resources.maxPatchVertices = 32;
	resources.maxTessGenLevel = 64;
	resources.maxViewports = 16;
	resources.maxVertexAtomicCounters = 0;
	resources.maxTessControlAtomicCounters = 0;
	resources.maxTessEvaluationAtomicCounters = 0;
	resources.maxGeometryAtomicCounters = 0;
	resources.maxFragmentAtomicCounters = 8;
	resources.maxCombinedAtomicCounters = 8;
	resources.maxAtomicCounterBindings = 1;
	resources.maxVertexAtomicCounterBuffers = 0;
	resources.maxTessControlAtomicCounterBuffers = 0;
	resources.maxTessEvaluationAtomicCounterBuffers = 0;
	resources.maxGeometryAtomicCounterBuffers = 0;
	resources.maxFragmentAtomicCounterBuffers = 1;
	resources.maxCombinedAtomicCounterBuffers = 1;
	resources.maxAtomicCounterBufferSize = 16384;
	resources.maxTransformFeedbackBuffers = 4;
	resources.maxTransformFeedbackInterleavedComponents = 64;
	resources.maxCullDistances = 8;
	resources.maxCombinedClipAndCullDistances = 8;
	resources.maxSamples = 4;
	resources.limits.nonInductiveForLoops = 1;
	resources.limits.whileLoops = 1;
	resources.limits.doWhileLoops = 1;
	resources.limits.generalUniformIndexing = 1;
	resources.limits.generalAttributeMatrixVectorIndexing = 1;
	resources.limits.generalVaryingIndexing = 1;
	resources.limits.generalSamplerIndexing = 1;
	resources.limits.generalVariableIndexing = 1;
	resources.limits.generalConstantMatrixVectorIndexing = 1;

	EShMessages messages = (EShMessages)(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);
	if (!shader.parse(&resources, 460, false, messages))
	{
		throw std::runtime_error(std::string("glslang parse failed: ") + shader.getInfoLog());
	}

	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(messages))
	{
		throw std::runtime_error(std::string("glslang link failed: ") + program.getInfoLog());
	}

	std::vector<uint32_t> spirv;
	glslang::GlslangToSpv(*program.getIntermediate(lang), spirv);
	return spirv;
}

std::string se::GpuProgram::LoadShader(const std::string& filename)
{
	std::ifstream file(filename, std::ios::in | std::ios::binary);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open shader file: " + filename);
	}
	std::string contents;
	file.seekg(0, std::ios::end);
	contents.resize(static_cast<size_t>(file.tellg()));
	file.seekg(0, std::ios::beg);
	file.read(contents.data(), contents.size());
	file.close();
	return contents;
}

se::RaytracingPipeline se::GpuProgram::CreateRaytracingPipeline()
{
	RaytracingPipeline pipeline{};

	// Descriptor set: binding0=TLAS, binding1=Params SSBO
	std::vector<vk::DescriptorSetLayoutBinding> bindings = {
		vk::DescriptorSetLayoutBinding()
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR),
		vk::DescriptorSetLayoutBinding()
			.setBinding(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR)
	};

	auto layoutInfo = vk::DescriptorSetLayoutCreateInfo()
		.setBindingCount(static_cast<uint32_t>(bindings.size()))
		.setPBindings(bindings.data());
	pipeline.descriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);

	auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
		.setSetLayoutCount(1)
		.setPSetLayouts(&pipeline.descriptorSetLayout);
	pipeline.layout = device.createPipelineLayout(pipelineLayoutInfo);

	// Load minimal occlusion shaders from files
	const std::string rgenSrc = LoadShader("C:/Users/matis/OneDrive/Documentos/Fing/Tesis/codigo/sound-engine/src/shaders/occlusion.rgen");
	const std::string rmissSrc = LoadShader("C:/Users/matis/OneDrive/Documentos/Fing/Tesis/codigo/sound-engine/src/shaders/occlusion.rmiss");
	const std::string rchitSrc = LoadShader("C:/Users/matis/OneDrive/Documentos/Fing/Tesis/codigo/sound-engine/src/shaders/occlusion.rchit");

	auto rgenSpv = CompileShader(rgenSrc, "occlusion.rgen", vk::ShaderStageFlagBits::eRaygenKHR);
	auto rmissSpv = CompileShader(rmissSrc, "occlusion.rmiss", vk::ShaderStageFlagBits::eMissKHR);
	auto rchitSpv = CompileShader(rchitSrc, "occlusion.rchit", vk::ShaderStageFlagBits::eClosestHitKHR);

	auto makeModule = [&](const std::vector<uint32_t>& code)
	{
		vk::ShaderModuleCreateInfo ci{};
		ci.codeSize = code.size() * sizeof(uint32_t);
		ci.pCode = code.data();
		return device.createShaderModule(ci);
	};

	auto rgenModule = makeModule(rgenSpv);
	auto rmissModule = makeModule(rmissSpv);
	auto rchitModule = makeModule(rchitSpv);

	std::vector<vk::PipelineShaderStageCreateInfo> stages;
	stages.push_back(vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eRaygenKHR, rgenModule, "main"));
	stages.push_back(vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eMissKHR, rmissModule, "main"));
	stages.push_back(vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eClosestHitKHR, rchitModule, "main"));

	std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups;
	groups.push_back(vk::RayTracingShaderGroupCreateInfoKHR()
		.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral)
		.setGeneralShader(0) // rgen
		.setClosestHitShader(VK_SHADER_UNUSED_KHR)
		.setAnyHitShader(VK_SHADER_UNUSED_KHR)
		.setIntersectionShader(VK_SHADER_UNUSED_KHR));
	groups.push_back(vk::RayTracingShaderGroupCreateInfoKHR()
		.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral)
		.setGeneralShader(1) // miss
		.setClosestHitShader(VK_SHADER_UNUSED_KHR)
		.setAnyHitShader(VK_SHADER_UNUSED_KHR)
		.setIntersectionShader(VK_SHADER_UNUSED_KHR));
	groups.push_back(vk::RayTracingShaderGroupCreateInfoKHR()
		.setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup)
		.setGeneralShader(VK_SHADER_UNUSED_KHR)
		.setClosestHitShader(2) // chit
		.setAnyHitShader(VK_SHADER_UNUSED_KHR)
		.setIntersectionShader(VK_SHADER_UNUSED_KHR));

	vk::RayTracingPipelineCreateInfoKHR pci{};
	pci.setStages(stages).setGroups(groups).setMaxPipelineRayRecursionDepth(1).setLayout(pipeline.layout);

	auto result = device.createRayTracingPipelineKHR({}, {}, pci, nullptr);
	if (result.result != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create ray tracing pipeline");

	pipeline.pipeline = result.value;

	// Create descriptor pool and allocate set
	std::vector<vk::DescriptorPoolSize> poolSizes = {
		vk::DescriptorPoolSize(vk::DescriptorType::eAccelerationStructureKHR, 1),
		vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1)
	};
	auto poolInfo = vk::DescriptorPoolCreateInfo().setPoolSizeCount(poolSizes.size()).setPPoolSizes(poolSizes.data()).setMaxSets(1);
	pipeline.descriptorPool = device.createDescriptorPool(poolInfo);
	auto allocInfo = vk::DescriptorSetAllocateInfo().setDescriptorPool(pipeline.descriptorPool).setDescriptorSetCount(1).setPSetLayouts(&pipeline.descriptorSetLayout);
	pipeline.descriptorSet = device.allocateDescriptorSets(allocInfo)[0];

	// Create SBTs (host-visible for simplicity)
	auto handleSize = raytracingProperties.shaderGroupHandleSize;
	auto baseAlign = raytracingProperties.shaderGroupBaseAlignment;
	auto alignUp = [](uint32_t v, uint32_t a)
	{ return (v + a - 1) & ~(a - 1); };
	uint32_t sbtStride = alignUp(handleSize, baseAlign);

	std::vector<uint8_t> handles(3 * handleSize);
	device.getRayTracingShaderGroupHandlesKHR(pipeline.pipeline, 0, 3, handles.size(), handles.data());

	// Create three SBT buffers
	auto makeSbt = [&](size_t index)
	{
		GpuBuffer buf = GetBuffer(sbtStride,
			vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		void* mapped = device.mapMemory(buf.memory, 0, sbtStride);
		std::memcpy(mapped, handles.data() + index * handleSize, handleSize);
		device.unmapMemory(buf.memory);
		return buf;
	};

	pipeline.raygenShaderBindingTable = makeSbt(0);
	pipeline.missShaderBindingTable = makeSbt(1);
	pipeline.hitShaderBindingTable = makeSbt(2);

	// Cleanup modules
	device.destroyShaderModule(rgenModule);
	device.destroyShaderModule(rmissModule);
	device.destroyShaderModule(rchitModule);

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

void se::RaytracingProgram::Init(std::vector<GpuMesh>& meshes)
{
	if (meshes.empty()) throw std::runtime_error("No meshes provided for raytracing");

	for (auto& mesh : meshes)
	{
		bottomLevelAS.push_back(gpuProgram->CreateBottomLevelAccelerationStructure(mesh));
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

bool se::RaytracingProgram::TestOcclusion(const float origin[3], const float direction[3], float tmin, float tmax)
{
	// Create params buffer (host-visible)
	struct Params
	{
		alignas(16) float origin[4];
		alignas(16) float direction[4];
		float tmin;
		float tmax;
		uint32_t result;
	};
	Params p{};
	p.origin[0] = origin[0];
	p.origin[1] = origin[1];
	p.origin[2] = origin[2];
	p.origin[3] = 0.0f;
	p.direction[0] = direction[0];
	p.direction[1] = direction[1];
	p.direction[2] = direction[2];
	p.direction[3] = 0.0f;
	p.tmin = tmin;
	p.tmax = tmax;
	p.result = 0u;

	auto paramsBuf = gpuProgram->GetBuffer(sizeof(Params),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	void* mapped = gpuProgram->device.mapMemory(paramsBuf.memory, 0, sizeof(Params));
	std::memcpy(mapped, &p, sizeof(Params));
	gpuProgram->device.unmapMemory(paramsBuf.memory);

	// Write descriptors (TLAS + params)
	vk::WriteDescriptorSetAccelerationStructureKHR asInfo{};
	asInfo.setAccelerationStructureCount(1).setPAccelerationStructures(&topLevelAS.handle);

	vk::DescriptorBufferInfo bufInfo{};
	bufInfo.setBuffer(paramsBuf.buffer).setOffset(0).setRange(sizeof(Params));

	std::vector<vk::WriteDescriptorSet> writes;
	writes.push_back(vk::WriteDescriptorSet()
		.setDstSet(rtPipeline.descriptorSet)
		.setDstBinding(0)
		.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
		.setDescriptorCount(1)
		.setPNext(&asInfo));
	writes.push_back(vk::WriteDescriptorSet()
		.setDstSet(rtPipeline.descriptorSet)
		.setDstBinding(1)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setPBufferInfo(&bufInfo));

	gpuProgram->device.updateDescriptorSets(writes, {});

	// Prepare SBT regions
	vk::StridedDeviceAddressRegionKHR raygen{}, miss{}, hit{}, callable{};
	raygen.setDeviceAddress(rtPipeline.raygenShaderBindingTable.address).setStride(rtPipeline.raygenShaderBindingTable.size)
		.setSize(rtPipeline.raygenShaderBindingTable.size);
	miss.setDeviceAddress(rtPipeline.missShaderBindingTable.address).setStride(rtPipeline.missShaderBindingTable.size).setSize(rtPipeline.missShaderBindingTable.size);
	hit.setDeviceAddress(rtPipeline.hitShaderBindingTable.address).setStride(rtPipeline.hitShaderBindingTable.size).setSize(rtPipeline.hitShaderBindingTable.size);
	callable.setDeviceAddress(0).setStride(0).setSize(0);

	gpuProgram->BeginCommands();
	gpuProgram->commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, rtPipeline.pipeline);
	gpuProgram->commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, rtPipeline.layout, 0, 1, &rtPipeline.descriptorSet, 0, nullptr);
	gpuProgram->commandBuffer.traceRaysKHR(raygen, miss, hit, callable, 1, 1, 1);
	gpuProgram->SubmitCommandsAndWait();

	// Read back result
	Params* pmapped = (Params*)gpuProgram->device.mapMemory(paramsBuf.memory, 0, sizeof(Params));
	uint32_t hitResult = pmapped->result;
	gpuProgram->device.unmapMemory(paramsBuf.memory);

	gpuProgram->FreeBuffer(paramsBuf);
	return hitResult != 0u;
}
