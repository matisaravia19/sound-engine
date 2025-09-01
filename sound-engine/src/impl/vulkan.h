#pragma once

#include "sound-engine/core.h"

// Use Vulkan-Hpp dynamic dispatch so extension entry points
// (e.g., ray tracing KHR) are loaded at runtime instead of
// requiring static linker symbols.
#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif
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
		vk::TransformMatrixKHR transform;
	};

	struct AccelerationStructure
	{
		vk::AccelerationStructureKHR handle = VK_NULL_HANDLE;
		GpuBuffer buffer;
		vk::DeviceAddress address = 0;
	};

	struct BLAccelerationStructure : public AccelerationStructure
	{
		GpuMesh* mesh = nullptr;
	};

	struct TLAccelerationStructure : public AccelerationStructure
	{
		std::vector<BLAccelerationStructure>* bottomLevelAS;
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
		std::vector<BLAccelerationStructure> bottomLevelAS;
		TLAccelerationStructure topLevelAS;
		RaytracingPipeline rtPipeline;

		std::shared_ptr<GpuProgram> gpuProgram;

		RaytracingProgram(std::shared_ptr<GpuProgram> gpuProgram);

		void Init(std::vector<GpuMesh>& meshes);
		void Destroy();

		// Trace a single ray and report whether it hits anything
		bool TestOcclusion(const float origin[3], const float direction[3], float tmin, float tmax);
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
		BLAccelerationStructure CreateBottomLevelAccelerationStructure(GpuMesh& mesh);
		TLAccelerationStructure CreateTopLevelAccelerationStructure(std::vector<BLAccelerationStructure>& bottomLevelAS);
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
