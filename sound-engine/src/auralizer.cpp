#include "sound-engine/auralizer.h"
#include <utility>
#include <iostream>
#include "vkFFT.h"
#include "impl/vulkan.h"

#define SAMPLE_RATE 44100
#define IR_DELAY_MS 100
#define IR_DECAY 0.5f
#define IR_SIZE (SAMPLE_RATE * IR_DELAY_MS / 1000 + 1)
#define BLOCK (SAMPLE_RATE * 10) // 10 seconds block
#define FFT_SIZE (NextPow2(IR_SIZE + BLOCK + 1))
#define TAIL (FFT_SIZE - BLOCK)

constexpr static uint32_t NextPow2(uint32_t x)
{
	uint32_t p = 1;
	while (p < x) p <<= 1;
	return p;
}

static std::vector<float> MakeSimpleIR()
{
	std::vector<float> ir(8, 0.0f);
	ir[0] = 1.0f;
	ir[4] = -1.0f;

//	std::vector<float> ir(IR_SIZE, 0.0f);
//	ir[0] = 1.0f;
//	ir.back() = IR_DECAY;
	return ir;
}

struct se::FFTApplication
{
	VkFFTApplication app = {};
	VkFFTApplication kernelApp = {};
};

se::Auralizer::Auralizer(std::shared_ptr<GpuProgram> gpuProgram)
{
	this->program = std::move(gpuProgram);
	this->fftApp = std::make_unique<FFTApplication>();
}

se::Auralizer::~Auralizer() = default;

std::vector<float> se::Auralizer::TestInit(float* wave, size_t waveSize)
{
	vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eTransferSrc |
		vk::BufferUsageFlagBits::eTransferDst;

	blockGpuBuffer = std::make_unique<GpuBuffer>(program->GetBuffer(
		FFT_SIZE * sizeof(float),
		bufferUsage,
		vk::MemoryPropertyFlagBits::eDeviceLocal));

	kernelGpuBuffer = std::make_unique<GpuBuffer>(program->GetBuffer(
		(FFT_SIZE / 2 + 1) * sizeof(float) * 2,
		bufferUsage,
		vk::MemoryPropertyFlagBits::eDeviceLocal));

	VkFFTConfiguration config = {};
	config.FFTdim = 1;
	config.size[0] = FFT_SIZE;
	config.performConvolution = 1;
	config.normalize = 1;

	VkBuffer buffer = (VkBuffer)blockGpuBuffer->buffer;
	config.buffer = &buffer;
	config.bufferSize = &blockGpuBuffer->size;

	VkBuffer kernelBuffer = (VkBuffer)kernelGpuBuffer->buffer;
	config.kernel = &kernelBuffer;
	config.kernelSize = &kernelGpuBuffer->size;

	VkDevice device = (VkDevice)program->device;
	VkPhysicalDevice physicalDevice = (VkPhysicalDevice)program->physicalDevice;
	VkQueue queue = (VkQueue)program->queue;
	VkCommandPool commandPool = (VkCommandPool)program->commandPool;
	VkFence fence = (VkFence)program->fence;

	config.device = &device;
	config.physicalDevice = &physicalDevice;
	config.queue = &queue;
	config.commandPool = &commandPool;
	config.fence = &fence;

	initializeVkFFT(&fftApp->app, config);

	VkFFTConfiguration kernelConfig = config;
	kernelConfig.size[0] = 4;
	kernelConfig.performConvolution = 0;
	kernelConfig.kernelConvolution = 1;

	initializeVkFFT(&fftApp->kernelApp, kernelConfig);

	program->ClearBuffer(*blockGpuBuffer);

	auto ir = MakeSimpleIR();
	program->UploadToBuffer(*blockGpuBuffer, ir.data(), ir.size() * sizeof(float));

	VkCommandBuffer commandBuffer = (VkCommandBuffer)program->commandBuffer;

	VkFFTLaunchParams launchParams = {};
	launchParams.commandBuffer = &commandBuffer;

	program->BeginCommands();

	auto result = VkFFTAppend(&fftApp->kernelApp, -1, &launchParams);
	if (result != VKFFT_SUCCESS)
	{
		std::cerr << "Failed to append kernel application: " << result << std::endl;
		throw std::runtime_error("Failed to initialize FFT application");
	}

	program->SubmitCommandsAndWait();

	std::vector<float> kernelHost(blockGpuBuffer->size / sizeof(float));
	program->DownloadFromBuffer(*blockGpuBuffer, kernelHost.data(), blockGpuBuffer->size);

	program->CopyBuffer(*blockGpuBuffer, *kernelGpuBuffer, kernelGpuBuffer->size);

	program->ClearBuffer(*blockGpuBuffer);
	program->UploadToBuffer(*blockGpuBuffer, wave, waveSize * sizeof(float));

	program->BeginCommands();
	result = VkFFTAppend(&fftApp->app, -1, &launchParams);
	//result = VkFFTAppend(&fftApp->app, 1, &launchParams);
	program->SubmitCommandsAndWait();

	std::vector<float> blockHost(blockGpuBuffer->size);
	program->DownloadFromBuffer(*blockGpuBuffer, blockHost.data(), blockGpuBuffer->size);

//	bool allZero = true;
//	for (const auto& value : kernelHost)
//	{
//		if (value != 0.0f)
//		{
//			allZero = false;
//			break;
//		}
//	}
//
//	if (allZero)
//	{
//		std::cerr << "Kernel buffer is empty after initialization" << std::endl;
//		throw std::runtime_error("Kernel buffer is empty");
//	}

	return blockHost;
}

void se::Auralizer::Test()
{
	auto irHost = MakeSimpleIR();

	VkFFTConfiguration configuration = {};
	VkFFTApplication application = {};

	configuration.FFTdim = 1;
	configuration.size[0] = 1024;

}
