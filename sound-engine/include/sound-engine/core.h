#pragma once

#include <vector>

namespace se
{
	struct Vertex
	{
		float position[3];
	};

	struct Mesh
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
	};

	struct Scene
	{
		std::vector<Mesh> meshes;
	};

	struct ImpulseResponse
	{
		std::vector<float> data;
		float sampleRate;
	};

	struct GpuBuffer;
	struct GpuMesh;
	struct GpuProgram;
}