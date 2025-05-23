#pragma once

#include <memory>
#include "core.h"

namespace se
{
	class Engine
	{
	private:
		std::shared_ptr<GpuProgram> program;

	public:
		Engine();
		~Engine();

		void Initialize();
		void Shutdown();
	};
}