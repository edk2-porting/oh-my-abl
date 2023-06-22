/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/DebugLib.h>
#include <Library/Board.h>
#include <Library/IntegrityIMA.h>

STATIC VOID
IMAEVMoff (CHAR8 *IntegrityIMAEVMCmdlinePtr)
{
#if INTEGRITY_LE_EVM
  CHAR8 *IMAEVMAppraiseOff = " ima_appraise=off evm=fix";
#else
  CHAR8 *IMAEVMAppraiseOff = " ima_appraise=off";
#endif

 AsciiStrnCpyS (IntegrityIMAEVMCmdlinePtr, IMA_CMDLINE_LEN,
                IMAEVMAppraiseOff, AsciiStrLen (IMAEVMAppraiseOff));
}

/*
 * IMA can be enabled/disabled during build and additionally
 * enabled/disabled at runtime. If it is enabled in the build,
 * CDT controls whether it is enabled during runtime or not.
 *
 * This function populates appropriate Linux kernel commandline
 * parameter in the input argument, based on whether device is
 * using IMA specific CDT or not.
 */
VOID
GetIntegrityIMACmdline (CHAR8 *IntegrityIMACmdlinePtr)
{
  CHAR8 *IMAAppraiseOn = " ima_policy=tcb ima_appraise_tcb";

  /* Check for sa525m (0x22E msm-id) withsubtype 2 and minor as 100 */
  if (0x22E != (BoardPlatformRawChipId () & 0x0000ffff)) {
    return IMAEVMoff (IntegrityIMACmdlinePtr);
  }
  if (100 != ((BoardTargetId () >> 8) & 0xFF)) {
    return IMAEVMoff (IntegrityIMACmdlinePtr);
  }

  AsciiStrnCpyS (IntegrityIMACmdlinePtr, IMA_CMDLINE_LEN,
                 IMAAppraiseOn, AsciiStrLen (IMAAppraiseOn));
}

/*
 * This function suggests the caller if IMA is enabled or not
 * in the build. Returns TRUE, if IMA is enabled during build
 * time itself otherwise FALSE.
 */
BOOLEAN
IsIntegrityIMAEnabled (VOID)
{
#if INTEGRITY_LE_IMA
  return TRUE;
#else
  return FALSE;
#endif
}