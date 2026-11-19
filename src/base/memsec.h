#pragma once

#include <string.h>

#include "base/types.h"

static inline void iw_memzero_secure(void *ptr, usize len) {
    if (ptr != nullptr && len > 0) {
        explicit_bzero(ptr, len);
    }
}
