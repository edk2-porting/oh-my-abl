/* Copyright (c) 2019, 2021 The Linux Foundation. All rights reserved.
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
 *  Changes from Qualcomm Innovation Center are provided under the following
 *  license:
 *  Copyright (c) 2021 - 2023 Qualcomm Innovation Center, Inc. All rights
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

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/DebugLib.h>
#include <Library/BootLinux.h>
#include <EarlyEthernet.h>
#include "LinuxLoaderLib.h"
#include "AutoGen.h"

/*
 * Read a page from NAND into a page in RAM, parse
 * ip address string and fill into caller supplied buffer.
 */
EFI_STATUS
GetEarlyEthInfoFromPartition (CHAR8 *ipv4buf, CHAR8 *ipv6buf, CHAR8 *macbuf,
                              CHAR8 *phyaddrbuf, CHAR8 *ifacebuf,
                              CHAR8 *speedbuf)
{
  EFI_STATUS Status;
  VOID *Buffer;
  CHAR8 *rawbuf;
  UINT32 DataSize = 0;
  UINT32 Pidx;
  UINT32 Qidx;
  UINT32 Qcount, QPhycount, QIfacecount, QSpeedcount;
  CHAR8 BootDeviceType[BOOT_DEV_NAME_SIZE_MAX];

  memset (ipv4buf, '\0', MAX_IP_ADDR_BUF);
  memset (ipv6buf, '\0', MAX_IP_ADDR_BUF);
  memset (macbuf, '\0', MAX_IP_ADDR_BUF);
  memset (phyaddrbuf, '\0', MAX_IP_ADDR_BUF);
  memset (ifacebuf, '\0', MAX_IP_ADDR_BUF);
  memset (speedbuf, '\0', MAX_IP_ADDR_BUF);
#if EARLY_ETH_AS_DLKM
  AsciiStrnCpyS (ipv4buf, MAX_IP_ADDR_BUF, " dwmac_qcom_eth.eipv4=", 22);
  AsciiStrnCpyS (ipv6buf, MAX_IP_ADDR_BUF, " dwmac_qcom_eth.eipv6=", 22);
  AsciiStrnCpyS (macbuf, MAX_IP_ADDR_BUF, " dwmac_qcom_eth.ermac=", 22);
#else
  AsciiStrnCpyS (ipv4buf, MAX_IP_ADDR_BUF, " eipv4=", 7);
  AsciiStrnCpyS (ipv6buf, MAX_IP_ADDR_BUF, " eipv6=", 7);
  AsciiStrnCpyS (macbuf, MAX_IP_ADDR_BUF, " ermac=", 7);
  AsciiStrnCpyS (phyaddrbuf, MAX_IP_ADDR_BUF, " ephyaddr=", 10);
  AsciiStrnCpyS (ifacebuf, MAX_IP_ADDR_BUF, " eiface=", 8);
  AsciiStrnCpyS (speedbuf, MAX_IP_ADDR_BUF, " espeed=", 8);
#endif

  GetRootDeviceType (BootDeviceType, BOOT_DEV_NAME_SIZE_MAX);

  if (!AsciiStrnCmp (BootDeviceType, "EMMC", AsciiStrLen ("EMMC"))) {
    DataSize = BOOT_IMG_MAX_PAGE_SIZE;
  } else {
    GetPageSize (&DataSize);
  }

  if (!ADD_OF (DataSize, ALIGNMENT_MASK_4KB - 1)) {
    DEBUG ((EFI_D_ERROR, "Integer overflow: in alignment mask 4k addition\n"));
    return EFI_BAD_BUFFER_SIZE;
  }

  Buffer = AllocatePages (ALIGN_PAGES (DataSize, ALIGNMENT_MASK_4KB));
  if (!Buffer) {
    DEBUG ((EFI_D_ERROR, "Failed to allocate memory for early ip address\n"));
    return EFI_BAD_BUFFER_SIZE;
  }

  Status = LoadImageFromPartition (Buffer, &DataSize, (CHAR16 *)L"emac");
  if (Status != EFI_SUCCESS) {
    FreePages (Buffer, 1);
    DEBUG ((EFI_D_ERROR, "Failed to load data for early ip address:%u\n",
                                                                  Status));
    return Status;
  }

  rawbuf = (CHAR8 *)Buffer;

  /* Extract ipv4 address string */
#if EARLY_ETH_AS_DLKM
  Qcount = 22;
#else
  Qcount = 7;
#endif
QPhycount = 10;
QIfacecount = QSpeedcount = 8;
  Pidx = IP_ADDR_STR_OFFSET;
  Qidx = 0;
  while (((CHAR8)rawbuf[Pidx] !=
         EARLY_ADDR_TERMINATOR) &&
         (Qidx < 16)) {
    if ((rawbuf[Pidx] == '.') ||
       ((rawbuf[Pidx] > 47) &&
       (rawbuf[Pidx] < 58))) {
      ipv4buf[Qidx + Qcount] = rawbuf[Pidx];
      Pidx++;
      Qidx++;
    } else {
        FreePages (Buffer, 1);
        DEBUG ((EFI_D_VERBOSE, "Invalid char for early ipv4 0x%x at %d\n",
                                                     rawbuf[Pidx], Pidx));
        return EFI_INVALID_PARAMETER;
    }
  }

  /* Extract ipv6 address string */
  ++Pidx;
  Qidx = 0;
  while ((CHAR8)rawbuf[Pidx] !=
          EARLY_ADDR_TERMINATOR) {
     if ((rawbuf[Pidx] == '.') ||
        (rawbuf[Pidx] == ':') ||
        (rawbuf[Pidx] == '/') ||
        ((rawbuf[Pidx] > 47) &&
        (rawbuf[Pidx] < 58)) ||
        ((rawbuf[Pidx] > 96) &&
        (rawbuf[Pidx] < 103)) ||
        ((rawbuf[Pidx] > 64) &&
        (rawbuf[Pidx] < 71))) {
       ipv6buf[Qidx + Qcount] = rawbuf[Pidx];
       Pidx++;
       Qidx++;
    } else {
        FreePages (Buffer, 1);
        DEBUG ((EFI_D_VERBOSE, "Invalid char for early ipv6 0x%x at %d\n",
                                                     rawbuf[Pidx], Pidx));
        return EFI_INVALID_PARAMETER;
    }
  }

  /* Extract mac address string */
  ++Pidx;
  Qidx = 0;
  while (((CHAR8)rawbuf[Pidx] !=
         EARLY_ADDR_TERMINATOR) &&
         (Qidx < MAC_ADDR_LEN)) {
    if ((rawbuf[Pidx] == ':') ||
       ((rawbuf[Pidx] > 47) &&
       (rawbuf[Pidx] < 58)) ||
       ((rawbuf[Pidx] > 96) &&
       (rawbuf[Pidx] < 103)) ||
       ((rawbuf[Pidx] > 64) &&
       (rawbuf[Pidx] < 71))) {
       macbuf[Qidx + Qcount] = rawbuf[Pidx];
       Pidx++;
       Qidx++;
    } else {
        FreePages (Buffer, 1);
        DEBUG ((EFI_D_VERBOSE, "Invalid char for early mac 0x%x at %d\n",
                                                    rawbuf[Pidx], Pidx));
        return EFI_INVALID_PARAMETER;
    }
  }

  /* Extract phy address string */
  ++Pidx;
  Qidx = 0;
  while (((CHAR8)rawbuf[Pidx] !=
         EARLY_ADDR_TERMINATOR) &&
         (Qidx < PHY_ADDR_LEN)) {
       phyaddrbuf[Qidx + QPhycount] = rawbuf[Pidx];
       Pidx++;
       Qidx++;
  }

  /* Extract iface string */
  ++Pidx;
  Qidx = 0;
  while (((CHAR8)rawbuf[Pidx] !=
         EARLY_ADDR_TERMINATOR) &&
         (Qidx < IFACE_LEN)) {
       ifacebuf[Qidx + QIfacecount] = rawbuf[Pidx];
       Pidx++;
       Qidx++;
  }

  /* Extract speed string */
  ++Pidx;
  Qidx = 0;
  while (((CHAR8)rawbuf[Pidx] !=
         EARLY_ADDR_TERMINATOR) &&
         (Qidx < SPEED_LEN)) {
       speedbuf[Qidx + QSpeedcount] = rawbuf[Pidx];
       Pidx++;
       Qidx++;
  }

  FreePages (Buffer, 1);
  return EFI_SUCCESS;
}

/*
 * Return 1 if build has early ethernet feature enabled otherwise 0.
 * Applicable for both Linux and Android builds.
 */
BOOLEAN
EarlyEthEnabled (VOID)
{
#if EARLY_ETH_ENABLED
  return 1;
#else
  return 0;
#endif
}
