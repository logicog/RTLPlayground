#include <string.h>
#include <stdint.h>

static inline void *fw_memset(void *dst, int v, size_t len)
{
	return memset(dst, v, (uint8_t)len);
}
#define memset fw_memset
