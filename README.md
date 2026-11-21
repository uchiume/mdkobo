# mdkobo

A small Windows viewer that renders a textured 3D model (OBJ format) with Win32 and Vulkan.

The model, texture, and shaders are all embedded in the executable as resources, so no external files are needed at run time.

## Features

- Plain Win32 API and Vulkan 1.0 (no framework)
- Loads OBJ models, merges duplicate vertices, and draws them with an index buffer
  - Supports triangle and quad faces (quads are split into two triangles)
- Loads TGA textures (32-bit, uncompressed or RLE-compressed)
- GLSL shaders are compiled to SPIR-V at build time and embedded as resources
- Mouse controls to orbit the camera around the model and zoom in and out
- Handles window resizing and minimizing (the swapchain is recreated, and the aspect ratio follows the window)

## Requirements

- Windows 10 / 11 (x64)
- A GPU and driver that support Vulkan

## Build prerequisites

| Prerequisite | Notes |
| --- | --- |
| Visual Studio 2022 | "Desktop development with C++" workload (toolset v143) |
| [Vulkan SDK](https://vulkan.lunarg.com/) 1.4.357.0 | Expected at the default install path `C:\VulkanSDK\1.4.357.0` |
| [GLM](https://github.com/g-truc/glm) | Placed in `thirdparty\glm` next to this repository (see below) |

### Directory layout

GLM is not included in this repository. Put it in `thirdparty\glm`, next to the repository folder:

```
any-folder\
├─ mdkobo\              <- this repository
│   ├─ mdkobo.sln
│   └─ mdkobo\
└─ thirdparty\
    └─ glm\
        └─ glm\glm.hpp
```

If you installed a different version of the Vulkan SDK, or installed it somewhere else, change `VulkanSDKDir` in `mdkobo\mdkobo.vcxproj`:

```xml
<PropertyGroup Label="UserMacros">
  <VulkanSDKDir>C:\VulkanSDK\1.4.357.0</VulkanSDKDir>
</PropertyGroup>
```

## Building

Open `mdkobo.sln` in Visual Studio and build the `Debug` or `Release` configuration for the `x64` platform.

To build from the command line:

```bat
msbuild mdkobo.sln /p:Configuration=Release /p:Platform=x64
```

The executable is written to `x64\Debug\mdkobo.exe` or `x64\Release\mdkobo.exe`.

> The x86 (Win32) configurations are not supported.

## Running

Run `mdkobo.exe` to show a textured cube. Close the window to exit.

### Mouse controls

| Action | Effect |
| --- | --- |
| Drag with the left button | Orbit the camera around the origin |
| Mouse wheel | Move the camera closer to or farther from the origin (the field of view stays at 45°) |

### Using the Vulkan validation layer

The validation layer is not enabled in code. To use it, set these environment variables before starting the program:

```bat
set VK_LOADER_LAYERS_ENABLE=*validation
set VK_KHRONOS_VALIDATION_DEBUG_ACTION=VK_DBG_LAYER_ACTION_LOG_MSG
set VK_KHRONOS_VALIDATION_LOG_FILENAME=%CD%\validation.txt
x64\Debug\mdkobo.exe
```

## Source overview

| File | Contents |
| --- | --- |
| `mdkobo\mdkobo.cpp` | Entry point and message loop |
| `mdkobo\mdkview.cpp` / `.h` | `CMDKView`: window, mouse controls, Vulkan device, swapchain, and frame rendering |
| `mdkobo\mdkscene.cpp` / `.h` | `CMDKScene`: model and texture loading, pipeline, and scene drawing |
| `mdkobo\mdkobo.rc` / `resource.h` | Resources (icon, model, texture, shaders) |
| `mdkobo\res\scene.vert` / `scene.frag` | GLSL shaders (compiled to SPIR-V with `glslc` at build time) |
| `mdkobo\res\cube.txt` | Cube model (OBJ format, with a `.txt` extension) |
| `mdkobo\res\Cube.tga` | Cube texture |

## Known limitations

- Texture mipmaps are not generated.
- Indices are 16-bit, so models with more than 65,536 unique vertices are not displayed correctly.
- The model, the initial camera position, and the light are fixed in the code and resources.

## License

This project is licensed under the [MIT License](LICENSE). Some parts are based on code from [opengl-tutorial.org](https://www.opengl-tutorial.org/); see [LICENSE](LICENSE) for details.
