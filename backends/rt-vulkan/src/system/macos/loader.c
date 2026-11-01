#include "context.h"

#include <dlfcn.h>

/* Volk's pointers and GLFW's surfaces must use the same MoltenVK instance.
 * Keep the provider loaded for the process lifetime, as Volk's default loader does. */
void* rtvk_moltenvk_module;

VkResult rtvk_initialize_moltenvk(void) {
	if (!rtvk_moltenvk_module) {
		const char* paths[] = {
			"@loader_path/libMoltenVK.dylib",
			"@rpath/libMoltenVK.dylib",
			"libMoltenVK.dylib",
			"MoltenVK.framework/MoltenVK",
		};
		for (usize i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
			rtvk_moltenvk_module = dlopen(paths[i], RTLD_NOW | RTLD_LOCAL);
			if (rtvk_moltenvk_module) {
				break;
			}
		}
	}
	if (!rtvk_moltenvk_module) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	PFN_vkGetInstanceProcAddr get_instance_proc_addr = (PFN_vkGetInstanceProcAddr)dlsym(rtvk_moltenvk_module, "vkGetInstanceProcAddr");
	if (!get_instance_proc_addr) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	volkInitializeCustom(get_instance_proc_addr);
	return VK_SUCCESS;
}
