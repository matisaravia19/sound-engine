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
	};

	struct GpuMesh
	{
		GpuBuffer vertexBuffer;
		GpuBuffer indexBuffer;
	};

	class GpuProgram
	{
	private:
		vk::Instance instance;
		vk::PhysicalDevice physicalDevice;
		vk::Device device;
		vk::Queue queue;
		vk::CommandPool commandPool;
		vk::CommandBuffer commandBuffer;
		std::unique_ptr<GpuBuffer> stagingBuffer;

		void InitStagingBuffer();
	public:
		static std::unique_ptr<GpuProgram> Create();

		GpuBuffer GetBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
		void UploadToBuffer(se::GpuBuffer& buffer, void* data, vk::DeviceSize size);
		void FreeBuffer(GpuBuffer& buffer);

		void Destroy();
		~GpuProgram()
		{
			Destroy();
		}
	};
}