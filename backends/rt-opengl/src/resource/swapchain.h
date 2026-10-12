#ifndef RTGL_SWAPCHAIN_H
#define RTGL_SWAPCHAIN_H

#include "config.h"
#include "framebuffer.h"
#include "platform/context.h"
#include "resource.h"
#include "texture.h"

RTGL_EXTERN_C_ENTER

typedef struct GLFWwindow GLFWwindow;

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_API rt_swapchain rtSwapchainCreate(void);
RTGL_API void rtSwapchainDestroy(rt_swapchain swapchain);
RTGL_API void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height);
RTGL_API rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain);
RTGL_API void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered);
RTGL_API void rtSwapchainBindGLFW(rt_swapchain swapchain, GLFWwindow* window);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtgl_swapchain_frame {
	struct rtgl_image_base base;
	struct rtgl_framebuffer* framebuffer;
	struct rtgl_texture_view* color_view;
};

struct rtgl_swapchain_resize {
	u32 width, height;
	struct rtgl_swapchain_frame** frames;
	u32 image_count;
	rt_timepoint completion;
	enum rt_error error;
};

struct rtgl_swapchain {
	struct rtgl_resource_base base;
	struct gl_surface* surface;
	struct rtgl_swapchain_frame** frames;
	u32 image_count;
	u32 current_frame_index;
	struct rtgl_swapchain_resize* resize;
};
RTGL_DECLARE_NEW_RESOURCE(swapchain)

void rtgl_swapchain_frame_init(struct rtgl_context* ctx, struct rtgl_swapchain_frame* frame);
void rtgl_swapchain_frame_finish(struct rtgl_swapchain_frame* frame);
void rtgl_swapchain_create_images(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, u32 width, u32 height);
void rtgl_swapchain_resize(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, u32 width, u32 height);
rt_swapchain_acquire_result rtgl_swapchain_acquire(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain);
void rtgl_swapchain_present(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, rt_timepoint rendered);
void rtgl_swapchain_bind_glfw(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, GLFWwindow* window, u32 width, u32 height);

RTGL_EXTERN_C_EXIT
#endif /* RTGL_SWAPCHAIN_H */
