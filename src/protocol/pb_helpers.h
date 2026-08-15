#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>

size_t pb_encode_to_bytes(uint8_t *destbuf, size_t destbufsize, const pb_msgdesc_t *fields, const void *src_struct);
bool pb_decode_from_bytes(const uint8_t *srcbuf, size_t srcbufsize, const pb_msgdesc_t *fields, void *dest_struct);
