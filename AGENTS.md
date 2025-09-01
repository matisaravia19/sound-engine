Project Working Notes for Coding Agents

Audience: automated assistants and collaborators working on this repo.

Project Overview
- Goal: real‑time, ray‑tracing‑based sound engine. For each audio source and the listener, compute an acoustic impulse response (IR) of the room using GPU ray tracing, then auralize (convolve) the dry sound with that IR.
- Modules:
  - auralizer: FFT‑based convolution given an IR + input sound to synthesize the perceived sound.
  - impulse: builds IRs using GPU ray tracing (Vulkan 1.3 + KHR acceleration structure + ray tracing pipeline).
- Current prototype: static scene. IR is computed once at startup; auralization uses that fixed IR. Target state: dynamic, continuous updates (per source/listener) each frame.

Data Flow (prototype)
- meshes -> BLAS -> TLAS -> ray tracing pass -> IR buffer -> auralizer FFT convolution -> output audio.
- Minimal occlusion shaders are embedded in code for portability; glslang is used at runtime to compile GLSL to SPIR‑V in `GpuProgram::CompileShader`.

Build And Tooling
- Target platform: Windows x64.
- Compiler: MSVC (Visual Studio 2022) x64 kit; avoid x86 kits.
- Generator: Ninja.
- CMake: use presets in CMakePresets.json (playground-debug). Do not change build dirs.
- Parallel builds: use -j 10 unless the user says otherwise.
- Submodules: raylib, VkFFT, glslang. Ensure they are initialized.
- Vulkan SDK: required and available via VULKAN_SDK environment variable.

Vulkan-Hpp Dispatch
- Define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 in compile definitions.
- Initialize the default dispatcher with vkGetInstanceProcAddr, then with the created instance and device. See sound-engine/src/impl/vulkan.cpp and vulkan.h.

Repo Layout
- sound-engine: static library with Vulkan + ray tracing helpers.
- playground: application used to test sound-engine; this is the build entry.

Runtime Expectations
- Vulkan ray tracing extensions must be supported by the device.
- Acceleration structures and scratch buffers are created on device‑local memory; staging buffer is host‑visible for uploads/downloads.
- For dynamic/realtime plans: expect double‑buffering of IRs and async compute/transfer to avoid audio glitches.

VS Code Setup
- Preset: Playground - Debug (Ninja).
- Default tasks: Build (playground-debug) and Clean.
- Launch config: Debug playground (MSVC) runs the built executable.

Line Endings
- Use CRLF for all text files edited by agents (C/C++, headers, CMake, shaders, JSON, Markdown). If adding scripts/config, prefer CRLF as well.

Coding Notes
- C++20 only; keep changes minimal and focused on the task.
- Avoid adding unrelated dependencies or changing filenames.
- Do not implement shader compilation in CompileShader for now; shaders are assumed precompiled.
- If you see LNK4272 (x86 vs x64), reselect the MSVC amd64 kit and delete CMake cache.
- Prefer dynamic dispatch in Vulkan‑Hpp (already configured) so KHR functions resolve at runtime.

Future Work (high‑level)
- Move from one‑shot IR to incremental/continuous updates; consider per‑source TLAS instances with transform updates.
- Add proper shader compilation pipeline (glslang or offline) and SBT setup.
- Introduce tests for FFT/auralizer correctness with small synthetic IRs.

Quick Build Steps (Local)
- Select kit: Visual Studio Community 2022 – amd64.
- Run: Build (playground-debug) task or press F5 to build + run.

Contact
- If something requires deviation (new presets, different generator), confirm with the user first.
