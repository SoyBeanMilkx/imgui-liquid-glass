#ifndef HRIDHI_H
#define HRIDHI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hridhi *hridhi_t;

typedef struct hridhi_vector {
	uint64_t lo, hi;
} hridhi_vector_t;

typedef struct __attribute__((aligned(16))) hridhi_context {
	uint64_t x[31];
	uint64_t sp, pc, nzcv;
	hridhi_vector_t v[32];
	uint32_t fpcr, fpsr;
} hridhi_context_t;

typedef void (*hridhi_breakpoint_cb)(hridhi_context_t *context);

int hridhi_available(void);
hridhi_t hridhi_hook_install(void *target, void *replacement, void **original_out);
hridhi_t hridhi_patch_code(void *target, const void *bytes, size_t size);
hridhi_t hridhi_add_breakpoint(void *target, hridhi_breakpoint_cb callback);

int hridhi_remove(hridhi_t handle);
int hridhi_release(hridhi_t handle);
const char *hridhi_strerror(int error);

#ifdef __cplusplus
}
#endif

#endif
