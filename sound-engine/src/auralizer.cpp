#include "sound-engine/auralizer.h"
#include <utility>
#include <iostream>
#include "vkFFT.h"
#include "impl/vulkan.h"

#define SAMPLE_RATE 44100
#define IR_DELAY_MS 1000
#define IR_DECAY 1.0f
#define IR_SIZE (SAMPLE_RATE * IR_DELAY_MS / 1000)
//#define IR_SIZE 4
#define BLOCK 250
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
//	ir[10] = 0.8f;
//	ir[20] = 0.6f;
//	ir[30] = 0.4f;
//	ir[40] = 0.2f;
	ir[IR_SIZE * 2 - 2] = IR_DECAY;

//	ir[0] = 1.0f; // Impulse at the start
//	ir[2] = 0.8f; // Decay
//	ir[4] = 0.6f; // Decay
//	ir[6] = 0.4f; // Decay

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

static float* RealToComplex(const float* data, size_t size)
{
	float* complexData = new float[size * 2];
	for (size_t i = 0; i < size; ++i)
	{
		complexData[i * 2] = data[i];
		complexData[i * 2 + 1] = 0.0f;
	}
	return complexData;
}

static std::vector<float> ComplexToReal(const std::vector<float> data)
{
	std::vector<float> realData(data.size() / 2);
	for (size_t i = 0; i < data.size() / 2; ++i)
	{
		realData[i] = data[i * 2];
	}
	return realData;
}

std::vector<float> se::Auralizer::TestInit(float* wave, size_t waveSize)
{
	vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eTransferSrc |
		vk::BufferUsageFlagBits::eTransferDst;

	size_t fftSize = NextPow2(waveSize + IR_SIZE - 1);
	float* complexWave = RealToComplex(wave, waveSize);

	blockGpuBuffer = std::make_unique<GpuBuffer>(program->GetBuffer(
		fftSize * sizeof(float) * 2,
		bufferUsage,
		vk::MemoryPropertyFlagBits::eDeviceLocal));

	kernelGpuBuffer = std::make_unique<GpuBuffer>(program->GetBuffer(
		fftSize * sizeof(float) * 2,
		bufferUsage,
		vk::MemoryPropertyFlagBits::eDeviceLocal));

	VkFFTConfiguration kernelConfig = {};
	kernelConfig.FFTdim = 1;
	kernelConfig.size[0] = fftSize;
	kernelConfig.performConvolution = 0;
	kernelConfig.kernelConvolution = 1;
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
	config.size[0] = fftSize;
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
	program->UploadToBuffer(*blockGpuBuffer, complexWave, waveSize * 2 * sizeof(float));

	program->BeginCommands();
	result = VkFFTAppend(&fftApp->app, -1, &launchParams);
	program->SubmitCommandsAndWait();

	std::vector<float> blockHost(blockGpuBuffer->size / sizeof(float));
	program->DownloadFromBuffer(*blockGpuBuffer, blockHost.data(), blockGpuBuffer->size);

	delete[] complexWave;

	return ComplexToReal(blockHost);
}

void se::Auralizer::Test()
{
	auto irHost = MakeSimpleIR();

	VkFFTConfiguration configuration = {};
	VkFFTApplication application = {};

	configuration.FFTdim = 1;
	configuration.size[0] = 1024;

}

void se::Auralizer::Init()
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
	kernelConfig.kernelConvolution = 1;
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
}

std::vector<float> se::Auralizer::Process(float* wave, size_t waveSize)
{
	// === 1) Precompute kernel FFT ===
	// Build a complex IR buffer (real=IR, imag=0), zero-pad to FFT_SIZE
	auto ir = MakeSimpleIR(); // your IR in real floats

	program->ClearBuffer(*kernelGpuBuffer);
	program->UploadToBuffer(*kernelGpuBuffer, ir.data(), ir.size() * sizeof(float));

	// Run forward FFT to put IR into frequency domain
	VkCommandBuffer cmdBuf = (VkCommandBuffer)program->commandBuffer;
	VkFFTLaunchParams lp = {};
	lp.commandBuffer = &cmdBuf;
	program->BeginCommands();
	VkFFTAppend(&fftApp->kernelApp, -1, &lp);
	program->SubmitCommandsAndWait();

	// === 2) Prepare output buffer ===
	size_t outputSize = waveSize + ir.size() - 1;
	std::vector<float> output(outputSize, 0.0f);

	// Overlap buffer to hold tail from previous block
	std::vector<float> overlap(FFT_SIZE - BLOCK, 0.0f);

	// === 3) Process input in blocks ===
	size_t numBlocks = (waveSize + BLOCK - 1) / BLOCK; // ceil division
	std::vector<float> complexBlock(FFT_SIZE * 2, 0.0f);

	for (size_t b = 0; b < numBlocks; ++b)
	{
		// Zero the block buffer
		std::fill(complexBlock.begin(), complexBlock.end(), 0.0f);

		// Copy BLOCK samples (or remaining) into real lane, imag=0
		size_t blockStart = b * BLOCK;
		size_t copyCount = std::min((size_t)BLOCK, waveSize - blockStart);
		for (size_t i = 0; i < copyCount; ++i)
			complexBlock[2 * i] = wave[blockStart + i];

		// Upload to GPU
		program->ClearBuffer(*blockGpuBuffer);
		program->UploadToBuffer(*blockGpuBuffer, complexBlock.data(), complexBlock.size() * sizeof(float));

		// Run convolution (forward * kernel * inverse)
		program->BeginCommands();
		VkFFTAppend(&fftApp->app, -1, &lp);
		program->SubmitCommandsAndWait();

		// Download result
		std::vector<float> convBlock(FFT_SIZE * 2);
		program->DownloadFromBuffer(*blockGpuBuffer, convBlock.data(), convBlock.size() * sizeof(float));

		// === 4) Overlap-add into output ===
		// Convert complex->real (imag should be ~0)
		std::vector<float> realBlock(FFT_SIZE);
		for (size_t i = 0; i < FFT_SIZE; ++i)
			realBlock[i] = convBlock[2 * i];

		// Add overlap from previous block
		for (size_t i = 0; i < overlap.size(); ++i)
			output[blockStart + i] += overlap[i];

		// Write the new part to output
		for (size_t i = 0; i < BLOCK && (blockStart + i) < outputSize; ++i)
			output[blockStart + i] += realBlock[i];

		// Save this block's tail for next overlap
		for (size_t i = 0; i < overlap.size(); ++i)
			overlap[i] = realBlock[i + BLOCK];
	}

	return output;
}
