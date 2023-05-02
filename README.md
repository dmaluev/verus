<img src="https://download.verushub.com/images/cover1280.jpg" width="100%" alt="Verus Engine">

# Verus Engine

## What is Verus Engine?
Verus Engine is a modern, platform-agnostic 3D game engine. It is developed using **C++** and **HLSL**. It is based on **Vulkan** and **Direct3D 12** graphics APIs. The code is user friendly and well optimized. The engine is intended to be a full-featured solution for making games. It includes modules to handle input, audio, networking and other things. Hence there is no need to search for third-party libraries or write custom code.

## Supported systems
* **Windows 10**, 8.1, 8, 7 *(via Vulkan and Direct3D 11 renderers)*
* **64-bit only**, 32-bit is not supported

## Supported graphics libraries
* Vulkan 1.1
* Direct3D 11 *(Shader Model 5.0)*
* Direct3D 12 *(Shader Model 5.1)*

## Features
* Native cross-platform code
* Same HLSL shader code for all APIs
* Deferred shading
* PBR/GGX with Metallic-Roughness workflow
* Efficient textures and streaming
* Large terrain support
* Built-in audio system
* Multiplayer support
* Custom user interface module
* Collision and physics support
* A bunch of post-processing effects
* Filesystem abstraction
* Other things

## VerusEdit
VerusEdit is an official editor for Verus Engine.

You can download the latest version of VerusEdit here: https://verushub.com/page/verusedit.

![VerusEdit](https://en.verushub.com/img/verusedit/screenshot0_tn.jpg)
![VerusEdit](https://en.verushub.com/img/verusedit/screenshot1_tn.jpg)

## Roadmap
These features should be implemented in version **1.x**:

- [x] glTF file format support
- [x] Materials v2.0: PBR/GGX with Metallic-Roughness workflow
- [x] Direct3D 11
- [x] OpenXR
- [x] Shadow maps for all lights
- [x] Ambient occlusion volumes
- [ ] Triggers
- [ ] Quest system, dialogue system
- [ ] Soft particles
- [ ] Directional lightmaps
- [ ] Waypoints and A-Star navigation
- [ ] Lua scripting
- [ ] Linux support

These features should be implemented in version **2.x**:

- [ ] Signed distance field (SDF) fonts
- [ ] Ray tracing
- [ ] Mesh shaders
- [ ] Multithreaded renderer
- [ ] Some form of inverse kinematics (IK)
- [ ] Android and iOS support

## License
Verus Engine is free for non-commercial use
