/* Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __BOOTLINUXLIB_H__
#define __BOOTLINUXLIB_H__

#include <Uefi.h>

#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Guid/Gpt.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/Debug.h>
#include <Library/DevicePathLib.h>
#include <Library/DrawUI.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <PiDxe.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/EFIVerifiedBoot.h>
#include <Protocol/FirmwareVolume2.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SerialIo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/EFISecRSA.h>
#include <Protocol/Hash.h>
#include <Protocol/Hash2.h>
#include <Protocol/EFIASN1X509.h>

#include "Board.h"
#include "BootImage.h"
#include "Decompress.h"
#include "DeviceInfo.h"
#include "LinuxLoaderLib.h"
#include "LocateDeviceTree.h"
#include "PartitionTableUpdate.h"
#include "Recovery.h"
#include "ShutdownServices.h"
#include "UpdateCmdLine.h"
#include "UpdateDeviceTree.h"
#include "VerifiedBoot.h"

#define ALIGN32_BELOW(addr) ALIGN_POINTER (addr - 32, 32)
#define LOCAL_ROUND_TO_PAGE(x, y) (((x) + (y - 1)) & (~(y - 1)))
#define ROUND_TO_PAGE(x, y) ((ADD_OF ((x), (y))) & (~(y)))
#define ALIGN_PAGES(x, y) (((x) + (y - 1)) / (y))
#define DECOMPRESS_SIZE_FACTOR 8
#define ALIGNMENT_MASK_4KB 4096
#define MAX_NUMBER_OF_LOADED_IMAGES 32
/* Size of the header that is used in case the boot image has
 * a uncompressed kernel + appended dtb */
#define PATCHED_KERNEL_HEADER_SIZE 20
/* String used to determine if the boot image has
 * a uncompressed kernel + appended dtb */
#define PATCHED_KERNEL_MAGIC "UNCOMPRESSED_IMG"

// Size reserved for DT image
#define DT_SIZE_2MB      (2 * 1024 * 1024)

#define KERNEL_32BIT_LOAD_OFFSET 0x8000
#define KERNEL_64BIT_LOAD_OFFSET 0x80000

#define NUM_NOMAP_REGIONS 40
#define NUM_RAM_PARTITIONS 30

#ifdef TARGET_LINUX_BOOT_CPU_ID
#define BootCpuId TARGET_LINUX_BOOT_CPU_ID
#else
#define BootCpuId 0
#endif

typedef enum {
  KERNEL_32BIT = 0,
  KERNEL_64BIT
} KernelMode;

typedef enum {
 LOAD_ADDR_NONE = 0,
 LOAD_ADDR_KERNEL,
 LOAD_ADDR_RAMDISK
} AddrType;

typedef VOID (*LINUX_KERNEL) (UINT64 ParametersBase,
                              UINT64 Reserved0,
                              UINT64 Reserved1,
                              UINT64 Reserved2);
typedef VOID (*LINUX_KERNEL32) (UINT32 Zero, UINT32 Arch, UINTN ParametersBase);

typedef enum {
        IMG_BOOT = 0,
        IMG_DTBO,
        IMG_VBMETA,
        IMG_RECOVERY,
        IMG_VENDOR_BOOT,
        IMG_INIT_BOOT,
        IMG_MAX
} img_type;

typedef struct {
  CHAR8 *Name;
  VOID *ImageBuffer;
  UINTN ImageSize;
} ImageData;

typedef struct BootInfo {
  BOOLEAN MultiSlotBoot;
  BOOLEAN FlashlessBoot;
  BOOLEAN NetworkBoot;
  BOOLEAN BootIntoRecovery;
  BOOLEAN BootReasonAlarm;
  CHAR8 SilentBootMode;
  CHAR16 Pname[MAX_GPT_NAME_SIZE];
  CHAR16 BootableSlot[MAX_GPT_NAME_SIZE];
  ImageData Images[MAX_NUMBER_OF_LOADED_IMAGES];
  UINTN NumLoadedImages;
  QCOM_VERIFIEDBOOT_PROTOCOL *VbIntf;
  boot_state_t BootState;
  CHAR8 *VBCmdLine;
  UINT32 VBCmdLineLen;
  UINT32 VBCmdLineFilledLen;
  VOID *VBData;
  UINT32 HeaderVersion;
  BOOLEAN HasBootInitRamdisk;
} BootInfo;

typedef struct BootLinuxParamlist {
  //Read this from RAM partition table
  UINT64 BaseMemory;

  VOID *ImageBuffer;
  UINT64 ImageSize;
  VOID *DtboImgBuffer;

  // Valid only for boot image header version greater than 2
  VOID *VendorImageBuffer;
  UINT64 VendorImageSize;

  // Valid only with recovery ramdisk
  VOID *RecoveryImageBuffer;
  UINT64 RecoveryImageSize;

  /* Load addresses for kernel, ramdisk, dt
   * These addresses are either predefined or get from UEFI core */
  UINT64 KernelLoadAddr;
  UINT64 KernelEndAddr;
  UINT64 RamdiskLoadAddr;
  UINT64 DeviceTreeLoadAddr;
  UINT64 *HypDtboBaseAddr;
  UINT32 NumHypDtbos;

 //Get the below fields info from the bootimage header
  UINT32 PageSize;
  UINT32 KernelSize;
  UINT32 SecondSize;
  UINT32 RamdiskSize;
  UINT32 RamdiskOffset;
  UINT32 PatchedKernelHdrSize;
  UINT32 DtbOffset;
  UINT32 DtSize;
  UINT32 VendorRamdiskTableSize;
  UINT32 VendorBootconfigSize;

  // Get the below fields info from the vendor_boot image header
  // Valid only for boot image header version greater than 2
  UINT32 VendorRamdiskSize;

  // Valid only with recovery ramdisk; get from recovery image header
  UINT32 RecoveryRamdiskSize;

  //Kernel size rounded off based on the page size
  UINT32 KernelSizeActual;
  UINT32 FinalBootConfigLen;

  CHAR8 *FinalCmdLine;
  CHAR8 *FinalBootConfig;
  CHAR8 *CmdLine;
  BOOLEAN BootingWith32BitKernel;
  BOOLEAN BootingWithPatchedKernel;
  BOOLEAN BootingWithGzipPkgKernel;
  /* Valid only for boot image header version greater than 4
   * with init_boot partition
   */
  VOID *RamdiskBuffer;
} BootParamlist;

extern RamPartitionEntry UpdatedRamPartitions[NUM_NOMAP_REGIONS];
extern UINT32 NumUpdPartitions;
extern BOOLEAN UpdRamPartitionsAvail;
EFI_STATUS
BootLinux (BootInfo *Info);
EFI_STATUS
CheckImageHeader (VOID *ImageHdrBuffer,
                  UINT32 ImageHdrSize,
                  VOID *VendorImageHdrBuffer,
                  UINT32 VendorImageHdrSize,
                  UINT32 *ImageSizeActual,
                  UINT32 *PageSize,
                  BOOLEAN BootIntoRecovery,
                  VOID *RecoveryImageHdrBuffer);
EFI_STATUS
LoadImageHeader (CHAR16 *Pname, VOID **ImageHdrBuffer, UINT32 *ImageHdrSize);
EFI_STATUS
LoadImage (CHAR16 *Pname, VOID **ImageBuffer,
           UINT32 ImageSizeActual, UINT32 PageSize);
EFI_STATUS
LaunchApp (IN UINT32 Argc, IN CHAR8 **Argv);
BOOLEAN TargetBuildVariantUser (VOID);
BOOLEAN IsLEVariant (VOID);
BOOLEAN IsMultiBoot (VOID);
BOOLEAN IsBuildAsSystemRootImage (BootParamlist *BootParamlistPtr);
BOOLEAN IsBuildUseRecoveryAsBoot (VOID);
VOID SetRecoveryHasNoKernel (VOID);
BOOLEAN IsRecoveryHasNoKernel (VOID);
BOOLEAN EarlyServicesEnabled (VOID);
EFI_STATUS
GetImage (CONST BootInfo *Info,
          VOID **ImageBuffer,
          UINTN *ImageSize,
          CHAR8 *ImageName);
BOOLEAN
LoadAndValidateDtboImg (BootInfo *Info,
                        BootParamlist *BootParamlistPtr);
VOID SetBootDevImage (VOID);
VOID ResetBootDevImage (VOID);
BOOLEAN IsBootDevImage (VOID);
BOOLEAN IsABRetryCountDisabled (VOID);
BOOLEAN IsDynamicPartitionSupport (VOID);
UINT64 SetandGetLoadAddr (BootParamlist *BootParamlistPtr, AddrType Type);
BOOLEAN IsNANDSquashFsSupport (VOID);
BOOLEAN IsEnableDisplayMenuFlagSupported (VOID);
BOOLEAN IsTargetAuto (VOID);
BOOLEAN IsHibernationEnabled (VOID);
BOOLEAN IsLVBootslotEnabled (VOID);
BOOLEAN BootCpuSelectionEnabled (VOID);
#endif
