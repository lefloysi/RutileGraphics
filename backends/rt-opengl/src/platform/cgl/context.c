#include "platform/context.h"

#include "error.h"
#include "glfw.h"
#include "platform/proc.h"

#include <OpenGL/OpenGL.h>
#include <dlfcn.h>
#include <stdlib.h>

struct gl_context {
	CGLContextObj native;
};

thread_local struct gl_context* rtgl_cgl_current_context;

struct gl_context* rtgl_create_glcontext(u08 major, u08 minor, bool presentation, struct gl_context* share) {
	/* Apple's native OpenGL implementation ends at 4.1. Do not silently
	 * downgrade: the execution backend uses 4.6 entry points and SPIR-V. */
	if (major > 4 || (major == 4 && minor > 1)) {
		rtgl_throwf(RT_UNSUPPORTED_PLATFORM, "macOS native OpenGL supports at most 4.1; rt-opengl requires %u.%u", major, minor);
		return NULL;
	}
	if (presentation) {
		rtgl_throwf(RT_UNSUPPORTED_PLATFORM, "macOS native OpenGL presentation cannot satisfy the rt-opengl 4.6 backend");
		return NULL;
	}
	const CGLPixelFormatAttribute attributes[] = {
		kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_4_1_Core,
		kCGLPFAAccelerated, kCGLPFAAllowOfflineRenderers, (CGLPixelFormatAttribute)0
	};
	CGLPixelFormatObj format = NULL;
	GLint count = 0;
	CGLError error = CGLChoosePixelFormat(attributes, &format, &count);
	if (error != kCGLNoError || !format) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to choose macOS OpenGL pixel format: %s", CGLErrorString(error));
		return NULL;
	}
	struct gl_context* context = calloc(1, sizeof(*context));
	RTGL_CHECK_ALLOC(context, sizeof(*context), "macOS OpenGL context");
	if (!context) {
		CGLDestroyPixelFormat(format);
		return NULL;
	}
	error = CGLCreateContext(format, share ? share->native : NULL, &context->native);
	CGLDestroyPixelFormat(format);
	if (error != kCGLNoError) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create macOS OpenGL context: %s", CGLErrorString(error));
		free(context);
		return NULL;
	}
	rtgl_make_glcontext_current(context, NULL);
	return context;
}

void rtgl_destroy_glcontext(struct gl_context* context) {
	if (!context) {
		return;
	}
	if (rtgl_cgl_current_context == context) {
		rtgl_release_current_context();
	}
	CGLDestroyContext(context->native);
	free(context);
}

struct gl_context* rtgl_get_current_glcontext(void) {
	return rtgl_cgl_current_context;
}

struct gl_surface* rtgl_create_glfw_surface(struct gl_context* context, GLFWwindow* window) {
	(void)context;
	(void)window;
	rtgl_throwf(RT_UNSUPPORTED_PLATFORM, "macOS native OpenGL 4.1 cannot present the rt-opengl 4.6 backend");
	return NULL;
}

void rtgl_destroy_glsurface(struct gl_surface* surface) {
	(void)surface;
}

void rtgl_swap_glsurface_buffers(struct gl_surface* surface) {
	(void)surface;
	rtgl_throwf(RT_UNSUPPORTED_PLATFORM, "macOS native OpenGL 4.1 cannot present the rt-opengl 4.6 backend");
}

void rtgl_glsurface_extent(struct gl_surface* surface, u32* width, u32* height) {
	(void)surface;
	*width = 0;
	*height = 0;
}

void rtgl_make_glcontext_current(struct gl_context* context, struct gl_surface* surface) {
	(void)surface;
	CGLError error = CGLSetCurrentContext(context ? context->native : NULL);
	if (error != kCGLNoError) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to activate macOS OpenGL context: %s", CGLErrorString(error));
		return;
	}
	rtgl_cgl_current_context = context;
}

void rtgl_release_current_context(void) {
	CGLSetCurrentContext(NULL);
	rtgl_cgl_current_context = NULL;
}

rtgl_proc_t rtgl_load_proc(struct gl_context* context, const char* name) {
	(void)context;
	return dlsym(RTLD_DEFAULT, name);
}
