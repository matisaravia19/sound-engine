# Raytracing Setup for Impulse Response Generation

## Overview

This document describes the raytracing setup implemented for building impulse responses of 3D scenes. The implementation uses Vulkan's raytracing extensions to perform acoustic path tracing.

## Architecture

### Core Components

1. **AccelerationStructure**: Represents Vulkan acceleration structures (BLAS/TLAS)
2. **RaytracingPipeline**: Contains the raytracing pipeline and shader binding tables
3. **ImpulseResponseMapper**: Main class for acoustic raytracing and impulse response generation

### Key Features Implemented

1. **Bottom Level Acceleration Structures (BLAS)**
   - One BLAS per mesh in the scene
   - Built from vertex and index buffers with proper flags for raytracing
   - Uses triangles geometry type with opaque flags

2. **Top Level Acceleration Structure (TLAS)**
   - Contains instances of all BLAS objects
   - Uses identity transform matrix for simplicity
   - Enables fast raytracing queries across the entire scene

3. **Raytracing Pipeline**
   - Ray generation shader for casting acoustic rays
   - Miss shader for handling rays that don't hit geometry
   - Closest hit shader for processing ray-surface intersections
   - Descriptor sets for acceleration structure and output buffers

## Shader Setup

Three shaders were created for the raytracing pipeline:

### impulse.rgen (Ray Generation)
- Casts rays from source positions
- Samples different directions for acoustic path tracing
- Writes results to output buffer

### impulse.rmiss (Miss Shader)
- Handles rays that escape the scene
- Returns background/ambient acoustic response

### impulse.rchit (Closest Hit)
- Processes ray-surface intersections
- Calculates acoustic reflections and material properties
- Handles secondary ray bouncing for reverberation

## Integration with Sound Engine

The `ImpulseResponseMapper` class provides:

1. **Scene Upload**: Converts raylib/user meshes to GPU acceleration structures
2. **Raytracing Initialization**: Builds BLAS/TLAS and creates raytracing pipeline
3. **Impulse Response Generation**: Uses raytracing to calculate acoustic paths
4. **Distance-based Attenuation**: Simple 1/distance falloff for direct paths
5. **Reflection Simulation**: Placeholder for raytracing-based reflections

## Usage Example

```cpp
// Create impulse response mapper
auto impulseMapper = std::make_unique<se::ImpulseResponseMapper>(gpuProgram);

// Upload scene geometry
se::Scene scene;
// ... populate scene with meshes ...
impulseMapper->UploadScene(scene);

// Initialize raytracing acceleration structures
impulseMapper->InitializeRaytracing();

// Generate impulse response between two points
float sourcePos[3] = {0.0f, 0.0f, 0.0f};
float listenerPos[3] = {2.0f, 0.0f, 0.0f};
auto impulseResponse = impulseMapper->GenerateImpulseResponse(sourcePos, listenerPos);
```

## Current Status and Next Steps

### Implemented Features
- ✅ Vulkan raytracing extension setup
- ✅ Acceleration structure creation (BLAS/TLAS)
- ✅ Basic raytracing pipeline structure
- ✅ Raytracing shader skeletons
- ✅ Scene upload and GPU buffer management
- ✅ Basic impulse response generation framework

### TODO for Full Implementation
- [ ] Complete shader compilation integration with glslang
- [ ] Implement actual GPU raytracing dispatch
- [ ] Add material properties for acoustic reflectance
- [ ] Implement multi-bounce acoustic path tracing
- [ ] Add frequency-dependent absorption models
- [ ] Optimize performance for real-time applications
- [ ] Add support for dynamic scene updates

### Technical Considerations

1. **Performance**: Current implementation prioritizes correctness over performance
2. **Memory**: Uses device-local memory for all acceleration structures
3. **Compatibility**: Requires GPU with raytracing support (RTX/RDNA2+)
4. **Threading**: Single-threaded implementation, could benefit from async compute

## Building and Testing

The implementation is integrated into the existing CMake build system. To test:

1. Ensure Vulkan SDK is installed with raytracing support
2. Build the project with raytracing-capable GPU drivers
3. Run the playground application to test impulse response generation

Note: The current build may have environment-specific issues that need resolution based on the development setup.
