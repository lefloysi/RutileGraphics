#include "system/atomic.h"

#include <atomic>

extern "C" {

u32 rt_atomic_load(const u32* value) {
	return std::atomic_ref<const u32>(*value).load(std::memory_order_relaxed);
}

u32 rt_atomic_inc(u32* value) {
	return std::atomic_ref<u32>(*value).fetch_add(1, std::memory_order_relaxed) + 1;
}

u32 rt_atomic_dec(u32* value) {
	return std::atomic_ref<u32>(*value).fetch_sub(1, std::memory_order_acq_rel) - 1;
}

bool rt_atomic_bool_load(const bool* value) {
	return std::atomic_ref<const bool>(*value).load(std::memory_order_relaxed);
}

void rt_atomic_bool_store(bool* value, bool next) {
	std::atomic_ref<bool>(*value).store(next, std::memory_order_relaxed);
}

}
