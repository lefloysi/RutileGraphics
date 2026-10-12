#ifndef RTGL_BUFFER_H
#define RTGL_BUFFER_H

#include "config.h"
#include "glad/gl.h"
#include "resource.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_API rt_buffer rtBufferCreate(void);
RTGL_API void rtBufferDestroy(rt_buffer buffer);
RTGL_API void rtBufferResize(rt_buffer buffer, enum rt_memory_type memory_type, usize size);
RTGL_API void rtBufferRead(rt_buffer buffer, rt_buffer_range range, u08* data, usize data_size);
RTGL_API u08* rtBufferMap(rt_buffer buffer, rt_buffer_range range);
RTGL_API void rtBufferUnmap(rt_buffer buffer);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtgl_buffer {
	struct rtgl_resource_base base;
	struct rtgl_buffer_storage* storage;
	struct rtgl_buffer_storage* reusable_storage;
	enum rt_memory_type memory_type;
	rt_buffer_range mapped_range;
	bool mapped;
};
RTGL_DECLARE_NEW_RESOURCE(buffer)

struct rtgl_buffer_storage {
	struct rtgl_context* ctx;
	struct rtgl_buffer_storage* next;
	GLuint gl_buffer;
	GLuint gl_texture_buffer;
	usize size;
	enum rt_memory_type memory_type;
	u08* shadow_data;
	u32 ref_count;
	rt_timepoint last_write;
};

RTGL_EXTERN_C_ENTER
void rtgl_buffer_resize(struct rtgl_context* ctx, struct rtgl_buffer* buffer, enum rt_memory_type memory_type, usize size);
void rtgl_buffer_subdata(struct rtgl_context* ctx, struct rtgl_buffer* buffer, rt_buffer_range range, const u08* data);
struct rtgl_buffer_storage* rtgl_buffer_prepare_recorded_write(struct rtgl_context*, struct rtgl_buffer*);
struct rtgl_buffer_storage* rtgl_buffer_prepare_write(struct rtgl_context* ctx, struct rtgl_buffer* buffer);
void rtgl_buffer_read(struct rtgl_context* ctx, struct rtgl_buffer* buffer, rt_buffer_range range, u08* data);
u08* rtgl_buffer_map(struct rtgl_context* ctx, struct rtgl_buffer* buffer, rt_buffer_range range);
void rtgl_buffer_unmap(struct rtgl_context* ctx, struct rtgl_buffer* buffer);
void rtgl_buffer_storage_retain(struct rtgl_buffer_storage* storage);
void rtgl_buffer_storage_release(struct rtgl_buffer_storage* storage);
void rtgl_buffer_storage_wait(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage);
void rtgl_buffer_storage_mark_write(struct rtgl_context* ctx, struct rtgl_buffer_storage* storage, rt_timepoint timepoint);
RTGL_EXTERN_C_EXIT

#endif /* RTGL_BUFFER_H */
