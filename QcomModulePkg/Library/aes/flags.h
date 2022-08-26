#ifndef _UCLIB_FLAGS
#define _UCLIB_FLAGS

/***************************************************************************
Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.

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
*************************************************************************/


/*===========================================================================
                     INCLUDE FILES FOR MODULE
===========================================================================*/
#include <stdint.h>

#define UCLIB_CONTROL_FLAG(flag)                ((flag)>31?0:(uint32_t)1<<(flag))

/*===========================================================================
                              MAC Control Flags
 ===========================================================================*/
#define UCLIB_MAC_FLAG_INVALID              0
#define UCLIB_MAC_KEY_READY                     UCLIB_CONTROL_FLAG(0)
#define UCLIB_MAC_IV_READY                      UCLIB_CONTROL_FLAG(1)
#define UCLIB_CIPHER_FLAG_DIRECTION             UCLIB_CONTROL_FLAG(2)
#define UCLIB_CIPHER_IV_READY                   UCLIB_CONTROL_FLAG(3)
#define UCLIB_CIPHER_KEY_READY                  UCLIB_CONTROL_FLAG(4)
#endif /* _UCLIB_FLAGS */
