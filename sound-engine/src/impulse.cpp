#include "sound-engine/impulse.h"
#include "impl/vulkan.h"
#include <stdexcept>

se::ImpulseResponseMapper::ImpulseResponseMapper(std::shared_ptr<GpuProgram> program)
{
	this->program = program;
}

se::ImpulseResponseMapper::~ImpulseResponseMapper()
{
	for (auto& mesh : gpuMeshes)
	{
		program->FreeBuffer(mesh.vertexBuffer);
		program->FreeBuffer(mesh.indexBuffer);
	}

	gpuMeshes.clear();
}

void se::ImpulseResponseMapper::UploadScene(const se::Scene& scene)
{
	if (!gpuMeshes.empty()) throw std::runtime_error("Scene already uploaded");

	for (const auto& mesh : scene.meshes)
	{
		vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;

		vk::DeviceSize vertexBufferSize = sizeof(mesh.vertices[0]) * mesh.vertices.size();
		vk::BufferUsageFlags vertexBufferUsage = vk::BufferUsageFlagBits::eVertexBuffer |
			vk::BufferUsageFlagBits::eShaderDeviceAddress |
			vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;

		auto vertexBuffer = program->GetBuffer(vertexBufferSize, vertexBufferUsage, memoryProperties);
		program->UploadToBuffer(vertexBuffer, (void*)mesh.vertices.data(), vertexBufferSize);

		vk::DeviceSize indexBufferSize = sizeof(mesh.indices[0]) * mesh.indices.size();
		vk::BufferUsageFlags indexBufferUsage = vk::BufferUsageFlagBits::eIndexBuffer |
			vk::BufferUsageFlagBits::eShaderDeviceAddress |
			vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;

		auto indexBuffer = program->GetBuffer(indexBufferSize, indexBufferUsage, memoryProperties);
		program->UploadToBuffer(indexBuffer, (void*)mesh.indices.data(), indexBufferSize);

		gpuMeshes.push_back(GpuMesh{ vertexBuffer, indexBuffer });
	}
}
