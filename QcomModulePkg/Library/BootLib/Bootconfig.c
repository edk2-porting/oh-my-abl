/* Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 *  with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 *   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
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

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/Debug.h>
#include "Bootconfig.h"

#include <Library/DeviceInfo.h>
#include <Library/DrawUI.h>
#include <Library/PartitionTableUpdate.h>
#include <Library/ShutdownServices.h>
#include <Library/VerifiedBootMenu.h>
#include <Library/HypervisorMvCalls.h>
#include <Protocol/EFIMdtp.h>
#include <Protocol/EFIScmModeSwitch.h>
#include <libufdt_sysdeps.h>
#include <Protocol/EFIKernelInterface.h>

#define V4_BOOTCONFIG_MAGIC_SIZE 12
#define V4_BOOTCONFIG_SIZE_SIZE 4
#define V4_BOOTCONFIG_CHECKSUM_SIZE 4
#define V4_BOOTCONFIG_TRAILER V4_BOOTCONFIG_MAGIC_SIZE + \
                                V4_BOOTCONFIG_SIZE_SIZE + \
                                V4_BOOTCONFIG_CHECKSUM_SIZE

CHAR8 Magic[] = "#BOOTCONFIG\n";
/*
 * Check if the bootconfig trailer is present within the bootconfig section.
 *
 * @param BootconfigEndAddr address of the End of the bootconfig section. If
 *        the trailer is present, it will be directly preceding this address.
 * @return true if the trailer is present, false if not.
 */
static BOOLEAN IsTrailerPresent (UINT64 BootconfigEndAddr)
{
  CHAR8 TrailMagicBuffer[V4_BOOTCONFIG_MAGIC_SIZE + 1];

  gBS->CopyMem (TrailMagicBuffer,
    (CHAR8*) (BootconfigEndAddr - V4_BOOTCONFIG_MAGIC_SIZE),
    V4_BOOTCONFIG_MAGIC_SIZE);

  if ( TrailMagicBuffer[V4_BOOTCONFIG_MAGIC_SIZE + 1] != '\0') {
    TrailMagicBuffer[V4_BOOTCONFIG_MAGIC_SIZE + 1] = '\0';
  }
  return !AsciiStrnCmp ((CHAR8*)(TrailMagicBuffer),
            Magic, V4_BOOTCONFIG_MAGIC_SIZE);
}
/*
 * Add a string of boot config parameters to memory appended by the trailer.
 *
 * @param Params starting addr of the string
 * @param ParamSize Size of string to be copied
 * @param BootconfigStartAddr addr of the start of the bootconfig section in
 *        memory.
 * @param BootconfigSize Size of bootconfig
 * @return End Address of Bootconfig
 *         0 if there is an error
 */
UINT64 AddBootconfigParameters (CHAR8* Params, UINT32 ParamSize
  , UINT64 BootconfigStartAddr, UINT32 BootconfigSize)
{
  INT32 CopiedBytes = 0;
  if (ParamSize == 0) {
      return 0;
  }
  if (!Params) {
      return 0;
  }
  if (!BootconfigStartAddr) {
      return 0;
  }
  UINT64 End = BootconfigStartAddr + BootconfigSize;
  if (IsTrailerPresent (End)) {
    End -= V4_BOOTCONFIG_TRAILER;
    CopiedBytes -= V4_BOOTCONFIG_TRAILER;
    DEBUG ((EFI_D_INFO, "TRAILER ALREADY PRESENT \n"));
  }
  gBS->CopyMem ((VOID *)End, Params, ParamSize);
  CopiedBytes += ParamSize;
  return (AddBootConfigTrailer (BootconfigStartAddr,
        BootconfigSize + CopiedBytes));
}
/*
 * Simple Checksum for a buffer.
 * Returns the checksum for a buffer passed for the amount of bytes
 *                 passed. (The size passed here is the word size.)
 *
 * @param addr pointer to the start of the buffer.
 * @param size size of the buffer in bytes.
 * @return check sum.
 */
static UINT32 Checksum (CONST UINT8* CONST Buffer, UINT32 Size)
{
    UINT32 Sum = 0;
    for (UINT32 Iter = 0; Iter < Size; Iter++) {
        Sum += Buffer[Iter];
    }
    return Sum;
}
/*
 * Add boot config trailer to the bootconfig section.
 * @param BootconfigStartAddr addr of the start of the bootconfig section in
 *        memory.
 * @param BootconfigSize Size of bootconfig
 * @return End Address of Bootconfig
 *         0 if there is an error
 */
UINT64 AddBootConfigTrailer (UINT64 BootconfigStartAddr
  , UINT32 BootconfigSize)
{
  if (!BootconfigStartAddr) {
    return 0;
  }
  if (BootconfigSize == 0) {
    return 0;
  }
  UINT64 End = BootconfigStartAddr + BootconfigSize;
  End = ALIGN (End, 4);
  BootconfigSize = End - BootconfigStartAddr;
  if (IsTrailerPresent (End)) {
    // no need to overwrite the current trailers
    return 0;
  }
  // size
  gBS->CopyMem ((VOID *)(End), &BootconfigSize, V4_BOOTCONFIG_SIZE_SIZE);
  // Checksum
  UINT32 Sum = Checksum ((UINT8 *) BootconfigStartAddr, BootconfigSize);
  gBS->CopyMem ((VOID *)(End + V4_BOOTCONFIG_SIZE_SIZE), &Sum
    , V4_BOOTCONFIG_CHECKSUM_SIZE);
  // Magic
  gBS->CopyMem ((VOID *)(End + V4_BOOTCONFIG_SIZE_SIZE +
    V4_BOOTCONFIG_CHECKSUM_SIZE)
    , Magic, V4_BOOTCONFIG_MAGIC_SIZE);
  return (End +
    V4_BOOTCONFIG_MAGIC_SIZE +
    V4_BOOTCONFIG_SIZE_SIZE +
    V4_BOOTCONFIG_CHECKSUM_SIZE);
}
