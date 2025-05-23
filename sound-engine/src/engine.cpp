#include "sound-engine/engine.h"
#include "impl/vulkan.h"

se::Engine::Engine()
{
	program = std::make_shared<GpuProgram>();
}

se::Engine::~Engine() = default;

void se::Engine::Initialize()
{
	program = GpuProgram::Create();
}

void se::Engine::Shutdown()
{
	program->Destroy();
}
