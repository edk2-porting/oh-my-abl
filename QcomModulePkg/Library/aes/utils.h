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

/*
 *  Changes from Qualcomm Innovation Center are provided under the following
 *  license:
 *
 *  Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights
 *  reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted (subject to the limitations in the
 *  disclaimer below) provided that the following conditions are met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 *  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 *  GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 *  HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 *  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <avb/libavb/libavb.h>
#include <Library/DebugLib.h>

void   *ENV_sec_memzero(void *ptr, size_t size);

int    CRYPTO_memcmp(const void *ptr1, const void *ptr2, size_t size);

size_t ENV_sec_memcpy(void *dest, size_t dst_size, const void *source, size_t src_size);

#define XCM_CONTROL_FLAG(flg)                         ((flg)>31?0:UINT32_C(1)<<(flg))

#define XCM_FLAG_HDR_SIZE_READY                       UCLIB_CONTROL_FLAG(0)
#define XCM_FLAG_PAYLOAD_SIZE_READY                   UCLIB_CONTROL_FLAG(1)
#define XCM_FLAG_TAG_SIZE_READY                       UCLIB_CONTROL_FLAG(2)
#define XCM_FLAG_TAG_BUF_SET                          UCLIB_CONTROL_FLAG(3)
#define XCM_FLAG_NONCE_READY                          UCLIB_CONTROL_FLAG(4)
#define XCM_FLAG_B0_INIT_DONE                         UCLIB_CONTROL_FLAG(5)
#define XCM_FLAG_AAD_IN_PROGRESS                      UCLIB_CONTROL_FLAG(6)
#define XCM_FLAG_CIPHER_IN_PROGRESS                   UCLIB_CONTROL_FLAG(7)
#define XCM_FLAG_CIPHER_IN_FINAL                      UCLIB_CONTROL_FLAG(8)

#define XCM_IS_FLAG_SET(me, flgs)                     ((me->xcm_flags & (flgs)) == (flgs))
#define XCM_SET_FLAG(me, flgs)                        me->xcm_flags |= (flgs)
#define XCM_CLR_FLAG(me, flgs)                        me->xcm_flags &= (~(flgs))
#define OPENSSL_cleanse(ptr, sz)                      memset(ptr, 0, sz)
#define OPENSSL_memcpy(dst, dst_sz, src, src_sz)      ENV_sec_memcpy(dst, dst_sz, src, src_sz)
#define OPENSSL_memset(ptr, val, sz)                  memset(ptr, val, sz)
#define CIPHER_IS_FLAG_SET(me, flgs)                  ((me->flags & (flgs)) == (flgs))
#define CIPHER_FLAG_SET(me,flgs)                      (me->flags |= (flgs))
#define UC_GUARD(cond, st)                            if (!(cond)) {                                                          \
                                                        DEBUG ((EFI_D_ERROR,"%s(%d), ret=%x", __FUNCTION__, __LINE__, st));   \
                                                        return (st);                                                          \
                                                     }

#define UC_PASS(cond, st)                            if (cond) return (st)
