# Rutile

Rutile is a small user-space graphics API with explicit runtime backend loading.
Applications load a backend such as Vulkan or DirectX 12 at startup, and can
insert layers for validation, logging, profiling, or other interception work.

## Configure and build

Rutile provides the `debug`, `release`, `ci-debug`, and `ci-release` CMake
presets. From the Rutile repository root, for example:

```powershell
cmake --preset debug
cmake --build --preset debug --target rtsl-sdk
```

The source tree provides C and C++ binding headers. CMake exports them as the
`Rutile::rutile` and `Rutile::cpp` interface targets, respectively.

## Platform Status

- Windows MSVC: builds and tests the Vulkan backend, DirectX 12 backend,
  validation layer, logging layer, RTSL compiler, and RTSL tests.
- Windows Clang and Windows GCC: build and test portable compiler coverage with
  backends disabled and layers/tests enabled.
- Linux GCC and Linux Clang: build and test portable compiler coverage with
  backends disabled and layers/tests enabled.
- macOS: the Vulkan backend uses MoltenVK to translate Vulkan to Metal. The
  integration requires verification on a Mac; it has not been runtime-tested here.

## Requirements

The OpenGL backend is a build target on Windows, Linux, and macOS. Windows uses
WGL; Linux uses EGL with X11 or Wayland and loads the system `libEGL.so.1` at
runtime. Linux builds require the EGL registry headers (`egl-registry` in vcpkg).
The renderer requires OpenGL 4.6, including SPIR-V shader specialization. macOS
native OpenGL provides only 4.1, so its CGL adapter reports this unsupported
version explicitly; compiling the target does not provide OpenGL 4.6 on macOS.

Common requirements:

- CMake 3.28 or newer
- Ninja
- Git submodules checked out recursively
- vcpkg with `VCPKG_ROOT` pointing at the vcpkg checkout

Windows requirements:

- Visual Studio 2022 with the **Desktop development with C++** workload
- Vulkan SDK if building `rt-vulkan`
- DirectX Shader Compiler dependency is pulled through vcpkg when using the
  `directx12` manifest feature

Linux requirements:

```sh
sudo apt-get update
sudo apt-get install -y libgl1-mesa-dev libx11-dev libxcursor-dev libxi-dev libxinerama-dev libxrandr-dev libc++-dev libc++abi-dev ninja-build xorg-dev
```

macOS requirements:

- Xcode and the macOS SDK.
- MoltenVK with Vulkan 1.3 support, available from the macOS Vulkan SDK.
- Target-built SPIRV-Tools, Vulkan headers, Volk, and Vulkan Memory Allocator.

Set `VULKAN_SDK` to the macOS SDK directory, or provide its dependencies through
Kiln's `dependency_prefix`. The MoltenVK prefix must contain
`include/MoltenVK/mvk_vulkan.h` and `lib/libMoltenVK.dylib`. Kiln copies the
dynamic library beside the graphics backend; CMake does the same. There is no
dependency on a native Vulkan driver or the Vulkan loader on macOS.

The backend loads the bundled MoltenVK provider, enables portability enumeration,
and enables `VK_KHR_portability_subset` when advertised by the selected device.
Applications using GLFW must pass that provider's `vkGetInstanceProcAddr` to
`glfwInitVulkanLoader` before `glfwInit`, so window surfaces and rendering use the
same provider. See the [MoltenVK integration guide](https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Runtime_UserGuide.md)
and [GLFW loader guidance](https://www.glfw.org/docs/3.4/vulkan_guide.html).

```sh
brew install ninja
```

The GitHub macOS runner is arm64, so the CI preset uses the `arm64-osx` vcpkg
triplet.

## Build

The top-level build adds the runtime backends, layers, examples, and RTSL project.
After configuring a preset, build the specific target you need:

```bat
cmake --preset debug
cmake --build --preset debug --target rutile
```

The RTSL compiler is built with Kiln. Set `RTSLC_EXECUTABLE` when configuring
CMake to use the resulting compiler for shader artifacts.

## vcpkg Features

The vcpkg manifest is split into features so optional dependencies stay behind
the targets that use them:

- `vulkan`: Vulkan backend dependencies
- `directx12`: DirectX 12 backend dependencies
- `opengl`: OpenGL backend dependencies
- `examples`: GLFW, GLM, CLI11, and stb for examples
- `tests`: Catch2 and test CLI support

The top-level project defines the available runtime targets. Build a specific
target with `cmake --build --preset debug --target <target>`; runtime targets
are excluded from the default build.

## Project Shape

- `include/rutile.h` is the public loader and core API.
- `include/rt_ext_*.h` files are optional extension packages.
- `rt-vulkan` is the Vulkan backend.
- `rt-d3d12` is the Direct3D 12 backend (Windows only).
- `rt-validation-layer` is a validation layer.
- `rt-logging-layer` is a logging layer.
- `examples` contains standalone CMake projects that show how to use Rutile. They are not added by Rutile's top-level `CMakeLists.txt`.

## Loading

Applications load a backend by name, optionally with layers:

```c
const char* layers[] = {
    "RT_VALIDATION_LAYER",
    "RT_LOGGING_LAYER",
};
if (rtLoad("rt-vulkan", layers, 2) != RT_SUCCESS) {
    /* handle load failure */
}
```

After `rtLoad` and `rtInit`, extension headers can resolve their own procs and initialize extension state against the current context:

```c
const char* features[] = { RT_FEATURE_PRESENTATION };
rtInit(features, 1);

rtClearError();
rtLoad_RT_EXT_SWAPCHAIN();
if (rtError() != RT_SUCCESS) {
    /* the loaded backend/layer chain does not expose the swapchain extension */
}

rtClearError();
rtLoad_RT_EXT_GLFW();
if (rtError() != RT_SUCCESS) {
    /* the extension exists but the current context may not support it */
}
```

For ad-hoc feature checks, ask the loaded chain for a proc:

```c
if (rtGetProc("rtMyFeatureDoThing")) {
    /* the loaded chain exposes this feature */
}
```

## Extensions

Rutile extensions are functions exported by a backend that are not part of the core.
These features should come with headers declaring what functions are exposed and what
to load.

These headers contain things like:
- public extension types
- proc typedefs
- private proc slots
- inline API wrappers
- a function (like `rtLoad_EXT_SWAPCHAIN`) that resolves names through `rtGetProc`


The application includes the extension header and loads it after the backend and core context are initialized:

```c
#include "rt_ext_my_feature.h"

rtLoad("rt-vulkan", layers, layer_count);
rtInit(features, feature_count);

rtClearError();
rtLoad_RT_EXT_MY_FEATURE();
if (rtError() == RT_SUCCESS) {
    rtMyFeatureDoThing(...);
}
```

This is one of Rutile's core philosophies: features should be able to be screwed onto forked backends
with ease.

## Layers
Layers are a feature that enable easy cross platform validation, logging, profiling or other features
enabled by intercepting calls. For example the official validation layer tracks object lifetimes and
errors when things remain alive at termination.
