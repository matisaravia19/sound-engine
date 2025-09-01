#pragma once

#include <vector>

#define IdentityMatrix4x4 {{ 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }};

namespace se
{
	typedef float Vector3[3];

	struct Vertex
	{
		Vector3 position;
	};

	// 4x4 row-major matrix
	typedef float Matrix4x4[4][4];

	struct Mesh
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		Matrix4x4 transform = IdentityMatrix4x4;
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
	struct RaytracingProgram;
}