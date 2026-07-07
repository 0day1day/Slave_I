#pragma once

#include "esp_err.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "imlib.h"

#ifdef __cplusplus
extern "C" {
#endif

int image_jpg_read(image_t** img_out, char* path);

int utf8_to_unicode(const char* utf8_in, uint64_t* unicode_out);

#ifdef __cplusplus
}
#endif
