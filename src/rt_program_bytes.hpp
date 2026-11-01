#ifndef RT_PROGRAM_BYTES_HPP
#define RT_PROGRAM_BYTES_HPP
#include <stddef.h>
typedef struct rt_program_bytes {
    const unsigned char* data;
    size_t size;
} rt_program_bytes;
#ifdef __cplusplus
extern "C" {
#endif
extern const rt_program_bytes rt_antialias_program;
#ifdef __cplusplus
}
#endif
#endif
