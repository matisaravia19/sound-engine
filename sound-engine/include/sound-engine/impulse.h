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
		std::vector<GpuMesh> gpuMeshes;

	public:
		ImpulseResponseMapper(std::shared_ptr<GpuProgram> program);
		~ImpulseResponseMapper();

		void UploadScene(const Scene& scene);
	};
}

