#pragma once

#include <memory>
#include "core.h"
#include "auralizer.h"

namespace se
{
	class Engine
	{
	private:
		std::shared_ptr<GpuProgram> program;
		std::shared_ptr<Auralizer> auralizer;

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
	};
}