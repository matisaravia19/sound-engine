#pragma once

#include <memory>
#include "core.h"

namespace se
{
	struct FFTApplication;

	class Auralizer
	{
	private:
		std::shared_ptr<GpuProgram> program;
		std::unique_ptr<FFTApplication> fftApp;

		std::unique_ptr<GpuBuffer> blockGpuBuffer;
		std::unique_ptr<GpuBuffer> kernelGpuBuffer;

	public:
		explicit Auralizer(std::shared_ptr<GpuProgram> gpuProgram);
		~Auralizer();

		std::vector<float> TestInit(float* wave, size_t waveSize);
		void Test();
	};
}