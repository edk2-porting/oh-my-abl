/*********************************************************************
Copyright (c) 2021 The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************/

#include "utils.h"
#include <Library/MemoryAllocationLib.h>

static void _memcpy(void* dest, const void* src,size_t len)
{
    char *csrc = (char *)src;
    char *cdest = (char *)dest;
    if (len == 0) return;
    do {
        *cdest++ = *csrc++; /* ??? to be unrolled */
    } while (--len != 0);
}

static int _memcmp(const void* s1, const void* s2,size_t len)
{
    size_t j;
    char *cs1 = (char *)s1;
    char *cs2 = (char *)s2;
    for (j = 0; j < len; j++) {
        if (cs1[j] != cs2[j]) return 2*(cs1[j] > cs2[j])-1;
    }
    return 0;
}

static void *_memzero(void *dest, size_t len)
{
    char *cdest = (char *)dest;
    if (len == 0) return 0;
    do {
        *cdest++ = 0;  /* ??? to be unrolled */
    } while (--len != 0);
    return dest;
}

static inline size_t
_memscpy(void *dst, size_t dst_size, const void *src, size_t src_size)
{
  size_t min_size = dst_size < src_size ? dst_size : src_size;
  _memcpy(dst, src, min_size);
  return min_size;
}

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

void* ENV_sec_memzero(void *ptr, size_t size)
{
  return _memzero(ptr, size);
}

int CRYPTO_memcmp(const void *ptr1, const void *ptr2, size_t size)
{
  return _memcmp(ptr1, ptr2, size);
}

size_t ENV_sec_memcpy(void *dest, size_t dst_size, const void *source, size_t src_size)
{
  return _memscpy(dest, dst_size, source, src_size);
}
