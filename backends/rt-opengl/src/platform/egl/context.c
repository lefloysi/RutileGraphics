#include "platform/context.h"

#include "error.h"
#include "glfw.h"
#include "platform/proc.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>

#define RTGL_EGL_FUNCTIONS(F) \
	F(PFNEGLGETPROCADDRESSPROC, eglGetProcAddress) \
	F(PFNEGLGETERRORPROC, eglGetError) \
	F(PFNEGLINITIALIZEPROC, eglInitialize) \
	F(PFNEGLTERMINATEPROC, eglTerminate) \
	F(PFNEGLBINDAPIPROC, eglBindAPI) \
	F(PFNEGLCHOOSECONFIGPROC, eglChooseConfig) \
	F(PFNEGLCREATECONTEXTPROC, eglCreateContext) \
	F(PFNEGLDESTROYCONTEXTPROC, eglDestroyContext) \
	F(PFNEGLCREATEPBUFFERSURFACEPROC, eglCreatePbufferSurface) \
	F(PFNEGLCREATEWINDOWSURFACEPROC, eglCreateWindowSurface) \
	F(PFNEGLDESTROYSURFACEPROC, eglDestroySurface) \
	F(PFNEGLMAKECURRENTPROC, eglMakeCurrent) \
	F(PFNEGLSWAPBUFFERSPROC, eglSwapBuffers) \
	F(PFNEGLSWAPINTERVALPROC, eglSwapInterval) \
	F(PFNEGLQUERYSURFACEPROC, eglQuerySurface)

#define RTGL_EGL_DECLARE(type, name) type rtgl_##name;
RTGL_EGL_FUNCTIONS(RTGL_EGL_DECLARE)
#undef RTGL_EGL_DECLARE

pthread_once_t rtgl_egl_loader_once = PTHREAD_ONCE_INIT;
bool rtgl_egl_loader_ready;

void rtgl_egl_load_library(void) {
	void* library = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL);
	if (!library) {
		return;
	}
#define RTGL_EGL_LOAD(type, name) rtgl_##name = (type)dlsym(library, #name); if (!rtgl_##name) { return; }
	RTGL_EGL_FUNCTIONS(RTGL_EGL_LOAD)
#undef RTGL_EGL_LOAD
	rtgl_egl_loader_ready = true;
}

#define eglGetProcAddress rtgl_eglGetProcAddress
#define eglGetError rtgl_eglGetError
#define eglInitialize rtgl_eglInitialize
#define eglTerminate rtgl_eglTerminate
#define eglBindAPI rtgl_eglBindAPI
#define eglChooseConfig rtgl_eglChooseConfig
#define eglCreateContext rtgl_eglCreateContext
#define eglDestroyContext rtgl_eglDestroyContext
#define eglCreatePbufferSurface rtgl_eglCreatePbufferSurface
#define eglCreateWindowSurface rtgl_eglCreateWindowSurface
#define eglDestroySurface rtgl_eglDestroySurface
#define eglMakeCurrent rtgl_eglMakeCurrent
#define eglSwapBuffers rtgl_eglSwapBuffers
#define eglSwapInterval rtgl_eglSwapInterval
#define eglQuerySurface rtgl_eglQuerySurface

struct gl_context {
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface bootstrap;
	int platform;
	bool owns_display;
};

struct gl_surface {
	EGLDisplay display;
	EGLSurface surface;
	void* wayland_window;
	void* wayland_library;
	void (*wayland_destroy)(void*);
	void (*wayland_resize)(void*, int, int, int, int);
	GLFWwindow* window;
};

thread_local struct gl_context* rtgl_egl_current_context;

struct gl_context* rtgl_create_glcontext(u08 major, u08 minor, bool presentation, struct gl_context* share) {
	pthread_once(&rtgl_egl_loader_once, rtgl_egl_load_library);
	if (!rtgl_egl_loader_ready) {
		rtgl_throwf(RT_UNSUPPORTED_PLATFORM, "libEGL.so.1 and its core EGL entry points are required");
		return NULL;
	}
	struct gl_context* context = calloc(1, sizeof(*context));
	RTGL_CHECK_ALLOC(context, sizeof(*context), "EGL context");
	if (!context) {
		return NULL;
	}
	context->display = EGL_NO_DISPLAY;
	context->context = EGL_NO_CONTEXT;
	context->bootstrap = EGL_NO_SURFACE;
	if (share) {
		context->display = share->display;
		context->platform = share->platform;
	}
	else {
		void* native_display = NULL;
		EGLenum platform = EGL_PLATFORM_SURFACELESS_MESA;
		if (presentation) {
			int (*get_platform)(void) = (int (*)(void))rtgl_find_process_proc("glfwGetPlatform");
			context->platform = get_platform ? get_platform() : (rtgl_find_process_proc("glfwGetX11Display") ? 0x00060004 : 0x00060003);
			const char* getter;
			if (context->platform == 0x00060003) {
				platform = EGL_PLATFORM_WAYLAND_KHR;
				getter = "glfwGetWaylandDisplay";
			}
			else if (context->platform == 0x00060004) {
				platform = EGL_PLATFORM_X11_KHR;
				getter = "glfwGetX11Display";
			}
			else {
				rtgl_throwf(RT_UNSUPPORTED_PLATFORM, "EGL presentation requires the GLFW X11 or Wayland platform");
				goto fail;
			}
			void* (*get_display)(void) = (void* (*)(void))rtgl_find_process_proc(getter);
			native_display = get_display ? get_display() : NULL;
			if (!native_display) {
				rtgl_throwf(RT_UNSUPPORTED_PLATFORM, "%s is unavailable or returned no display", getter);
				goto fail;
			}
		}
		PFNEGLGETPLATFORMDISPLAYEXTPROC get_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
		context->display = get_display ? get_display(platform, native_display, NULL) : EGL_NO_DISPLAY;
		if (context->display == EGL_NO_DISPLAY || !eglInitialize(context->display, NULL, NULL)) {
			rtgl_throwf(RT_PLATFORM_FAILURE, "failed to initialize EGL display (0x%x)", eglGetError());
			goto fail;
		}
		context->owns_display = true;
	}
	const EGLint config_attributes[] = {
		EGL_SURFACE_TYPE, presentation ? EGL_WINDOW_BIT | EGL_PBUFFER_BIT : EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, context->platform == 0x00060004 ? 0 : 8, EGL_NONE
	};
	EGLint count = 0;
	if (!eglBindAPI(EGL_OPENGL_API) || !eglChooseConfig(context->display, config_attributes, &context->config, 1, &count) || !count) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to choose an EGL OpenGL configuration (0x%x)", eglGetError());
		goto fail;
	}
	const EGLint attributes[] = { EGL_CONTEXT_MAJOR_VERSION_KHR, major, EGL_CONTEXT_MINOR_VERSION_KHR, minor,
		EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR, EGL_NONE };
	context->context = eglCreateContext(context->display, context->config, share ? share->context : EGL_NO_CONTEXT, attributes);
	if (context->context == EGL_NO_CONTEXT) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create EGL OpenGL %u.%u context (0x%x)", major, minor, eglGetError());
		goto fail;
	}
	const EGLint pbuffer_attributes[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
	context->bootstrap = eglCreatePbufferSurface(context->display, context->config, pbuffer_attributes);
	if (context->bootstrap == EGL_NO_SURFACE) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create EGL bootstrap pbuffer (0x%x)", eglGetError());
		goto fail;
	}
	rtgl_make_glcontext_current(context, NULL);
	if (rtgl_error() != RT_SUCCESS) {
		goto fail;
	}
	return context;
fail:
	rtgl_destroy_glcontext(context);
	return NULL;
}

void rtgl_destroy_glcontext(struct gl_context* context) {
	if (!context) {
		return;
	}
	if (rtgl_egl_current_context == context) {
		rtgl_release_current_context();
	}
	if (context->display != EGL_NO_DISPLAY) {
		if (context->context != EGL_NO_CONTEXT) {
			eglDestroyContext(context->display, context->context);
		}
		if (context->bootstrap != EGL_NO_SURFACE) {
			eglDestroySurface(context->display, context->bootstrap);
		}
		if (context->owns_display) {
			eglTerminate(context->display);
		}
	}
	free(context);
}

struct gl_context* rtgl_get_current_glcontext(void) {
	return rtgl_egl_current_context;
}

struct gl_surface* rtgl_create_glfw_surface(struct gl_context* context, GLFWwindow* window) {
	EGLNativeWindowType native_window = (EGLNativeWindowType)0;
	void* wayland_window = NULL;
	void* wayland_library = NULL;
	void (*wayland_destroy)(void*) = NULL;
	void (*wayland_resize)(void*, int, int, int, int) = NULL;
	if (context->platform == 0x00060003) {
		void* (*get_window)(GLFWwindow*) = (void* (*)(GLFWwindow*))rtgl_find_process_proc("glfwGetWaylandWindow");
		void* library = dlopen("libwayland-egl.so.1", RTLD_NOW | RTLD_LOCAL);
		wayland_library = library;
		void* (*create_window)(void*, int, int) = library ? (void* (*)(void*, int, int))dlsym(library, "wl_egl_window_create") : NULL;
		wayland_destroy = library ? (void (*)(void*))dlsym(library, "wl_egl_window_destroy") : NULL;
		wayland_resize = library ? (void (*)(void*, int, int, int, int))dlsym(library, "wl_egl_window_resize") : NULL;
		if (!get_window || !create_window || !wayland_destroy || !wayland_resize) {
			if (library) {
				dlclose(library);
			}
			rtgl_throwf(RT_UNSUPPORTED_PLATFORM, "Wayland EGL window functions are unavailable");
			return NULL;
		}
		int width = 0, height = 0;
		rtgl_glfw_get_framebuffer_size(window, &width, &height);
		wayland_window = create_window(get_window(window), width, height);
		native_window = (EGLNativeWindowType)wayland_window;
	}
	else if (context->platform == 0x00060004) {
		unsigned long (*get_window)(GLFWwindow*) = (unsigned long (*)(GLFWwindow*))rtgl_find_process_proc("glfwGetX11Window");
		native_window = get_window ? (EGLNativeWindowType)get_window(window) : (EGLNativeWindowType)0;
	}
	if (!native_window) {
		if (wayland_library) {
			dlclose(wayland_library);
		}
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to obtain a native EGL window");
		return NULL;
	}
	struct gl_surface* surface = calloc(1, sizeof(*surface));
	RTGL_CHECK_ALLOC(surface, sizeof(*surface), "EGL window surface");
	if (!surface) {
		if (wayland_window) {
			wayland_destroy(wayland_window);
		}
		if (wayland_library) {
			dlclose(wayland_library);
		}
		return NULL;
	}
	surface->display = context->display;
	surface->wayland_window = wayland_window;
	surface->wayland_library = wayland_library;
	surface->wayland_destroy = wayland_destroy;
	surface->wayland_resize = wayland_resize;
	surface->window = window;
	surface->surface = eglCreateWindowSurface(context->display, context->config, native_window, NULL);
	if (surface->surface == EGL_NO_SURFACE) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create EGL window surface (0x%x)", eglGetError());
		rtgl_destroy_glsurface(surface);
		return NULL;
	}
	return surface;
}

void rtgl_destroy_glsurface(struct gl_surface* surface) {
	if (!surface) {
		return;
	}
	if (surface->surface != EGL_NO_SURFACE) {
		eglDestroySurface(surface->display, surface->surface);
	}
	if (surface->wayland_window) {
		surface->wayland_destroy(surface->wayland_window);
	}
	if (surface->wayland_library) {
		dlclose(surface->wayland_library);
	}
	free(surface);
}

void rtgl_swap_glsurface_buffers(struct gl_surface* surface) {
	if (surface && surface->wayland_window) {
		int width = 0, height = 0;
		rtgl_glfw_get_framebuffer_size(surface->window, &width, &height);
		if (width > 0 && height > 0) {
			surface->wayland_resize(surface->wayland_window, width, height, 0, 0);
		}
	}
	if (surface && !eglSwapBuffers(surface->display, surface->surface)) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to swap EGL window buffers (0x%x)", eglGetError());
	}
}

void rtgl_glsurface_extent(struct gl_surface* surface, u32* width, u32* height) {
	EGLint native_width = 0, native_height = 0;
	if (surface) {
		eglQuerySurface(surface->display, surface->surface, EGL_WIDTH, &native_width);
		eglQuerySurface(surface->display, surface->surface, EGL_HEIGHT, &native_height);
	}
	*width = (u32)native_width;
	*height = (u32)native_height;
}

void rtgl_make_glcontext_current(struct gl_context* context, struct gl_surface* surface) {
	if (!context) {
		rtgl_release_current_context();
		return;
	}
	EGLSurface drawable = surface ? surface->surface : context->bootstrap;
	if (!eglMakeCurrent(context->display, drawable, drawable, context->context)) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to activate EGL OpenGL context (0x%x)", eglGetError());
		return;
	}
	if (surface) {
		eglSwapInterval(context->display, 0);
	}
	rtgl_egl_current_context = context;
}

void rtgl_release_current_context(void) {
	if (rtgl_egl_current_context) {
		eglMakeCurrent(rtgl_egl_current_context->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		rtgl_egl_current_context = NULL;
	}
}

rtgl_proc_t rtgl_load_proc(struct gl_context* context, const char* name) {
	(void)context;
	return (rtgl_proc_t)eglGetProcAddress(name);
}
