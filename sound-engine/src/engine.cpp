#include "sound-engine/engine.h"
#include "impl/vulkan.h"
#include "glslang/Public/ShaderLang.h"

se::Engine::Engine()
{
	program = std::make_shared<GpuProgram>();
	auralizer = std::make_shared<Auralizer>(program);
	irMapper = std::make_shared<ImpulseResponseMapper>(program);
}

se::Engine::~Engine() = default;

void se::Engine::Initialize()
{
	//glslang::InitializeProcess();
	program->Init();
}

void se::Engine::Shutdown()
{
	program->Destroy();
	//glslang::FinalizeProcess();
}
