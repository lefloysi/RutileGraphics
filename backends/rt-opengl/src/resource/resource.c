#include "resource.h"
#include "system/atomic.h"
#include "buffer.h"
#include "command_buffer.h"
#include "error.h"
#include "execution/execution.h"
#include "framebuffer.h"
#include "program.h"
#include "queue.h"
#include "sampler.h"
#include "swapchain.h"
#include "texture.h"

#include <assert.h>
#include <stdlib.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/
void* rtgl_alloc_resource(usize size) {
	void* resource = calloc(1, size);
	if (!resource) {
		rtgl_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for graphics resource", size);
	}
	return resource;
}

void rtgl_init_resource_base(struct rtgl_context* ctx, struct rtgl_resource_base* base, rtgl_resource_type type) {
	assert(base);
	base->type = type;
	base->ctx = ctx;
	base->ref_count = 1;
	base->job_count = 0;
	base->zombie = false;
}

void rtgl_finish_resource_base(struct rtgl_resource_base* base) {
	assert(base);
	base->ctx = NULL;
	base->zombie = true;
}

void rtgl_resource_try_free(struct rtgl_resource_base* base) {
	if (base->type == RTGL_RESOURCE_SWAPCHAIN_FRAME) {
		struct rtgl_context* ctx = base->ctx;
		rtgl_execution_lock(ctx);
		const bool ready = rtgl_resource_ready_to_destroy(base);
		if (ready) { rt_atomic_inc(&base->job_count); }
		rtgl_execution_unlock(ctx);
		if (ready) { rtgl_execution_frame_finalize(ctx, base); }
		return;
	}
	if (rtgl_resource_ready_to_destroy(base)) {
		rtgl_resource_finalize(base);
		free(base);
	}
}

void rtgl_resource_retain(struct rtgl_resource_base* base) {
	if (!base) {
		return;
	}
	assert(base);
	rt_atomic_inc(&base->ref_count);
}

void rtgl_resource_release(struct rtgl_resource_base* base) {
	if (!base) {
		return;
	}
	assert(base);
	assert(rt_atomic_load(&base->ref_count) > 0);
	if (rt_atomic_dec(&base->ref_count) == 0) { rtgl_resource_try_free(base); }
}

void rtgl_resource_retire(struct rtgl_resource_base* base) {
	assert(base);
	rt_atomic_bool_store(&base->zombie, true);
	rtgl_resource_release(base);
}

void rtgl_resource_finalize(struct rtgl_resource_base* base) {
	assert(base);
	switch (base->type) {
	case RTGL_RESOURCE_BUFFER:
		rtgl_buffer_finish((struct rtgl_buffer*)base);
		break;
	case RTGL_RESOURCE_FRAMEBUFFER:
		rtgl_framebuffer_finish((struct rtgl_framebuffer*)base);
		break;
	case RTGL_RESOURCE_SWAPCHAIN:
		rtgl_swapchain_finish((struct rtgl_swapchain*)base);
		break;
	case RTGL_RESOURCE_SWAPCHAIN_FRAME:
		rtgl_swapchain_frame_finish((struct rtgl_swapchain_frame*)base);
		break;
	case RTGL_RESOURCE_TEXTURE:
		rtgl_texture_finish((struct rtgl_texture*)base);
		break;
	case RTGL_RESOURCE_TEXTURE_VIEW:
		rtgl_texture_view_finish((struct rtgl_texture_view*)base);
		break;
	case RTGL_RESOURCE_COMMAND_BUFFER:
		rtgl_command_buffer_finish((struct rtgl_command_buffer*)base);
		break;
	case RTGL_RESOURCE_PROGRAM:
		rtgl_program_finish((struct rtgl_program*)base);
		break;
	case RTGL_RESOURCE_QUEUE:
		rtgl_queue_finish((struct rtgl_queue*)base);
		break;
	case RTGL_RESOURCE_SAMPLER:
		rtgl_sampler_finish((struct rtgl_sampler*)base);
		break;
	case RTGL_RESOURCE_UNKNOWN:
		break;
	}
}

bool rtgl_resource_ready_to_destroy(struct rtgl_resource_base* base) {
	assert(base);
	return rt_atomic_bool_load(&base->zombie) && rt_atomic_load(&base->ref_count) == 0 && rt_atomic_load(&base->job_count) == 0;
}
