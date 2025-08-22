#include "sound-engine/impulse.h"
#include "impl/vulkan.h"
#include <stdexcept>
#include <utility>
#include <cmath>

se::ImpulseResponseMapper::ImpulseResponseMapper(std::shared_ptr<GpuProgram> program)
{
	this->program = std::move(program);
}

se::ImpulseResponseMapper::~ImpulseResponseMapper()
{
	if (raytracingInitialized)
	{
		rtProgram->Destroy();
	}

	for (auto& mesh : gpuMeshes)
	{
		program->FreeBuffer(mesh.vertexBuffer);
		program->FreeBuffer(mesh.indexBuffer);
	}

	gpuMeshes.clear();
}

void se::ImpulseResponseMapper::UploadScene(const se::Scene& scene)
{
	if (sceneUploaded) throw std::runtime_error("Scene already uploaded");

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

	sceneUploaded = true;
}

void se::ImpulseResponseMapper::InitializeRaytracing()
{
	if (!sceneUploaded) throw std::runtime_error("Scene must be uploaded before initializing raytracing");
	if (raytracingInitialized) throw std::runtime_error("Raytracing already initialized");

	rtProgram = std::make_unique<RaytracingProgram>(program);
	rtProgram->Init(gpuMeshes);

	raytracingInitialized = true;
}

se::ImpulseResponse se::ImpulseResponseMapper::GenerateImpulseResponse(
	const float sourcePosition[3],
	const float listenerPosition[3],
	float maxDistance,
	int sampleRate,
	float duration)
{
	if (!raytracingInitialized) throw std::runtime_error("Raytracing not initialized");

	// Calculate distance between source and listener
	float distance = std::sqrt(
		std::pow(sourcePosition[0] - listenerPosition[0], 2) +
			std::pow(sourcePosition[1] - listenerPosition[1], 2) +
			std::pow(sourcePosition[2] - listenerPosition[2], 2)
	);

	// Generate simple impulse response based on distance
	int numSamples = static_cast<int>(duration * sampleRate);
	std::vector<float> impulseData(numSamples, 0.0f);

	if (distance < maxDistance)
	{
		// Direct path - arrives first
		int directSample = static_cast<int>(distance / 343.0f * sampleRate); // Speed of sound ~343 m/s
		if (directSample < numSamples)
		{
			float amplitude = 1.0f / (1.0f + distance); // Simple distance attenuation
			impulseData[directSample] = amplitude;
		}

		// TODO: Use raytracing to find reflections and add them to the impulse response
		// For now, add some simple simulated reflections
		for (int reflection = 1; reflection <= 3; ++reflection)
		{
			float reflectionDistance = distance * (1.0f + reflection * 0.5f);
			int reflectionSample = static_cast<int>(reflectionDistance / 343.0f * sampleRate);
			if (reflectionSample < numSamples)
			{
				float reflectionAmplitude = 1.0f / (1.0f + distance) * std::pow(0.6f, reflection);
				impulseData[reflectionSample] += reflectionAmplitude;
			}
		}
	}

	return ImpulseResponse{ impulseData, static_cast<float>(sampleRate) };
}

bool se::ImpulseResponseMapper::TestRaycast(const float origin[3], const float direction[3], float maxDistance)
{
	if (!raytracingInitialized) throw std::runtime_error("Raytracing not initialized");

	// For now, this is a placeholder implementation
	// In a full implementation, you would dispatch rays using the raytracing pipeline
	// and check for intersections with the scene geometry

	// Simple test: check if ray intersects with a bounding box of the scene
	// This is just a stub - real raytracing would use the GPU pipeline
	return false;
}
