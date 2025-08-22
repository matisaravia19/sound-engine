#pragma once

#include "sound-engine/core.h"
#include <vulkan/vulkan.hpp>

namespace se
{
	struct GpuBuffer
	{
		vk::Buffer buffer = VK_NULL_HANDLE;
		vk::DeviceMemory memory = VK_NULL_HANDLE;
		vk::DeviceAddress address = 0;
		vk::DeviceSize size = 0;
	};

	struct GpuMesh
	{
		GpuBuffer vertexBuffer;
		GpuBuffer indexBuffer;
	};

	struct AccelerationStructure
	{
		vk::AccelerationStructureKHR handle = VK_NULL_HANDLE;
		GpuBuffer buffer;
		vk::DeviceAddress address = 0;
	};

	struct RaytracingPipeline
	{
		vk::Pipeline pipeline = VK_NULL_HANDLE;
		vk::PipelineLayout layout = VK_NULL_HANDLE;
		vk::DescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
		vk::DescriptorPool descriptorPool = VK_NULL_HANDLE;
		vk::DescriptorSet descriptorSet = VK_NULL_HANDLE;

		GpuBuffer raygenShaderBindingTable;
		GpuBuffer missShaderBindingTable;
		GpuBuffer hitShaderBindingTable;
	};

	struct RaytracingProgram
	{
		std::vector<AccelerationStructure> bottomLevelAS;
		AccelerationStructure topLevelAS;
		RaytracingPipeline rtPipeline;

		std::shared_ptr<GpuProgram> gpuProgram;

		RaytracingProgram(std::shared_ptr<GpuProgram> gpuProgram);

		void Init(const std::vector<GpuMesh>& meshes);
		void Destroy();
	};

	struct GpuProgram
	{
	private:
		std::unique_ptr<GpuBuffer> stagingBuffer;
		void InitStagingBuffer();
		void WaitForFence();

		vk::PhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties;
		vk::PhysicalDeviceRayTracingPipelinePropertiesKHR raytracingProperties;

	public:
		vk::Instance instance;
		vk::PhysicalDevice physicalDevice;
		vk::Device device;
		vk::Queue queue;
		vk::CommandPool commandPool;
		vk::CommandBuffer commandBuffer;
		vk::Fence fence;

		void Init();

		GpuBuffer GetBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
		void UploadToBuffer(se::GpuBuffer& buffer, void* data, vk::DeviceSize size);
		void DownloadFromBuffer(se::GpuBuffer& buffer, void* data, vk::DeviceSize size);
		void CopyBuffer(se::GpuBuffer& src, se::GpuBuffer& dst, vk::DeviceSize size);
		void ClearBuffer(se::GpuBuffer& buffer);
		void FreeBuffer(GpuBuffer& buffer);

		// Raytracing methods
		AccelerationStructure CreateBottomLevelAccelerationStructure(const std::vector<GpuMesh>& meshes);
		AccelerationStructure CreateTopLevelAccelerationStructure(const std::vector<AccelerationStructure>& bottomLevelAS);
		void DestroyAccelerationStructure(AccelerationStructure& as);

		std::vector<uint32_t> CompileShader(const std::string& source, const std::string& filename, vk::ShaderStageFlagBits stage);
		RaytracingPipeline CreateRaytracingPipeline();
		void DestroyRaytracingPipeline(RaytracingPipeline& pipeline);

		void BeginCommands();
		void SubmitCommandsAndWait();

		void Destroy();
		~GpuProgram()
		{
			Destroy();
		}
	};
}