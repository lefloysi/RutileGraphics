#include "platform/proc.h"

#include <dlfcn.h>
#include <stddef.h>

void* rtgl_find_process_proc(const char* name) {
	void* proc = dlsym(RTLD_DEFAULT, name);
	if (proc) {
		return proc;
	}
#if defined(__APPLE__)
	const char* libraries[] = { "libglfw.3.dylib", "libglfw.dylib" };
#else
	const char* libraries[] = { "libglfw.so.3", "libglfw.so" };
#endif
	for (unsigned index = 0; index < sizeof(libraries) / sizeof(*libraries); index++) {
		void* library = dlopen(libraries[index], RTLD_LAZY | RTLD_NOLOAD);
		if (!library) {
			continue;
		}
		proc = dlsym(library, name);
		dlclose(library);
		if (proc) {
			return proc;
		}
	}
	return NULL;
}
