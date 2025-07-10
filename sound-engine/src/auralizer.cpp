#include "sound-engine/auralizer.h"
#include <utility>
#include <iostream>
#include "vkFFT.h"
#include "impl/vulkan.h"

#define SAMPLE_RATE 44100
#define IR_DELAY_MS 100
#define IR_DECAY 0.5f
#define IR_SIZE 4
#define BLOCK 4
#define FFT_SIZE (NextPow2(IR_SIZE + BLOCK - 1))
#define TAIL (FFT_SIZE - BLOCK)

constexpr static uint32_t NextPow2(uint32_t x)
{
	uint32_t p = 1;
	while (p < x) p <<= 1;
	return p;
}

static std::vector<float> MakeSimpleIR()
{
	std::vector<float> ir(IR_SIZE * 2, 0.0f);
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
		FFT_SIZE * sizeof(float) * 2,
		bufferUsage,
		vk::MemoryPropertyFlagBits::eDeviceLocal));

	kernelGpuBuffer = std::make_unique<GpuBuffer>(program->GetBuffer(
		FFT_SIZE * sizeof(float) * 2,
		bufferUsage,
		vk::MemoryPropertyFlagBits::eDeviceLocal));

	VkFFTConfiguration kernelConfig = {};
	kernelConfig.FFTdim = 1;
	kernelConfig.size[0] = FFT_SIZE;
	kernelConfig.performConvolution = 0;
	kernelConfig.kernelConvolution = 0;
	kernelConfig.normalize = 1;

	VkDevice device = (VkDevice)program->device;
	VkPhysicalDevice physicalDevice = (VkPhysicalDevice)program->physicalDevice;
	VkQueue queue = (VkQueue)program->queue;
	VkCommandPool commandPool = (VkCommandPool)program->commandPool;
	VkFence fence = (VkFence)program->fence;


	kernelConfig.device = &device;
	kernelConfig.physicalDevice = &physicalDevice;
	kernelConfig.queue = &queue;
	kernelConfig.commandPool = &commandPool;
	kernelConfig.fence = &fence;

	VkBuffer kernelBuffer = (VkBuffer)kernelGpuBuffer->buffer;
	kernelConfig.buffer = &kernelBuffer;
	kernelConfig.bufferSize = &kernelGpuBuffer->size;

	initializeVkFFT(&fftApp->kernelApp, kernelConfig);

	auto ir = MakeSimpleIR();

	program->ClearBuffer(*kernelGpuBuffer);
	program->UploadToBuffer(*kernelGpuBuffer, ir.data(), ir.size() * sizeof(float));

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

	std::vector<float> kernelHost(kernelGpuBuffer->size / sizeof(float));
	program->DownloadFromBuffer(*kernelGpuBuffer, kernelHost.data(), kernelGpuBuffer->size);

	VkFFTConfiguration config = kernelConfig;
	config.size[0] = FFT_SIZE;
	config.performConvolution = 1;
	config.kernelConvolution = 0;
	//config.performR2C = 1;

	VkBuffer buffer = (VkBuffer)blockGpuBuffer->buffer;
	config.buffer = &buffer;
	config.bufferSize = &blockGpuBuffer->size;

	config.kernel = &kernelBuffer;
	config.kernelSize = &kernelGpuBuffer->size;

	initializeVkFFT(&fftApp->app, config);

	program->ClearBuffer(*blockGpuBuffer);
	program->UploadToBuffer(*blockGpuBuffer, wave, waveSize * 2 * sizeof(float));

	program->BeginCommands();
	result = VkFFTAppend(&fftApp->app, -1, &launchParams);
	program->SubmitCommandsAndWait();

	std::vector<float> blockHost(blockGpuBuffer->size / sizeof(float));
	program->DownloadFromBuffer(*blockGpuBuffer, blockHost.data(), blockGpuBuffer->size);

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
