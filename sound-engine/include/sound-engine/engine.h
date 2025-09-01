#pragma once

#include <memory>
#include "core.h"
#include "auralizer.h"
#include "impulse.h"

namespace se
{
	class Engine
	{
	private:
		std::shared_ptr<GpuProgram> program;
		std::shared_ptr<Auralizer> auralizer;
		std::shared_ptr<ImpulseResponseMapper> irMapper;

	public:
		Engine();
		~Engine();

		void Initialize();
		void Shutdown();

		std::shared_ptr<GpuProgram> GetGpuProgram() const
		{
			return program;
		}

		std::shared_ptr<Auralizer> GetAuralizer()
		{
			return auralizer;
		}

		std::shared_ptr<ImpulseResponseMapper> GetImpulseResponseMapper()
		{
			return irMapper;
		}
	};
}