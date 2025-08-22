#pragma once

#include <vector>
#include <memory>
#include "core.h"

namespace se
{
	class ImpulseResponseMapper
	{
	private:
		std::shared_ptr<GpuProgram> program;
		std::unique_ptr<RaytracingProgram> rtProgram;
		std::vector<GpuMesh> gpuMeshes;

		bool sceneUploaded = false;
		bool raytracingInitialized = false;

	public:
		explicit ImpulseResponseMapper(std::shared_ptr<GpuProgram> program);
		~ImpulseResponseMapper();

		void UploadScene(const Scene& scene);
		void InitializeRaytracing();

		// Generate impulse response from source to listener position
		ImpulseResponse GenerateImpulseResponse(
			const float sourcePosition[3],
			const float listenerPosition[3],
			float maxDistance = 100.0f,
			int sampleRate = 44100,
			float duration = 1.0f
		);

		// Test raytracing with a simple ray
		bool TestRaycast(const float origin[3], const float direction[3], float maxDistance = 100.0f);
	};
}

