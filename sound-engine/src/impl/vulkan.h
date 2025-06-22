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

	struct GpuProgram
	{
	private:
		std::unique_ptr<GpuBuffer> stagingBuffer;
		void InitStagingBuffer();

		void WaitForFence();
		void SubmitCommandsAndWait();

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
		void ClearBuffer(se::GpuBuffer& buffer);
		void FreeBuffer(GpuBuffer& buffer);

		void Destroy();
		~GpuProgram()
		{
			Destroy();
		}
	};
}