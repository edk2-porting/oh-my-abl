/** @file UpdateCmdLine.c
 *
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2009-2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of The Linux Foundation nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **/

#include <Library/BaseLib.h>
#include <Library/BootLinux.h>
#include <Library/PartitionTableUpdate.h>
#include <Library/PrintLib.h>
#include <Library/FdtRw.h>
#include <LinuxLoaderLib.h>
#include <Protocol/EFICardInfo.h>
#include <Protocol/EFIChargerEx.h>
#include <Protocol/EFIChipInfoTypes.h>
#include <Protocol/EFIPmicPon.h>
#include <Protocol/Print2.h>

#include "AutoGen.h"
#include <DeviceInfo.h>
#include "UpdateCmdLine.h"
#include "Recovery.h"
#include "LECmdLine.h"

#define SIZE_OF_DELIM 2
#define PARAM_DELIM "\n"
#define ADD_PARAM_LEN(BootConfigFlag, ParamLen, CmdLineL, BootConfigL) \
                     do { \
                       if (BootConfigFlag == FALSE) { \
                         CmdLineL += ParamLen; \
                       } else { \
                         BootConfigL += (ParamLen + SIZE_OF_DELIM); \
                         CmdLineL += ParamLen; \
                       }\
                     } while (0);
STATIC CONST CHAR8 *DynamicBootDeviceCmdLine =
                                      " androidboot.boot_devices=soc/";
STATIC CONST CHAR8 *BootDeviceCmdLine = " androidboot.bootdevice=";

STATIC CONST CHAR8 *UsbSerialCmdLine = " androidboot.serialno=";
STATIC CONST CHAR8 *AndroidBootMode = " androidboot.mode=";
STATIC CONST CHAR8 *LogLevel = " quite";
STATIC CONST CHAR8 *BatteryChgPause = " androidboot.mode=charger";
STATIC CONST CHAR8 *MdtpActiveFlag = " mdtp";
STATIC CONST CHAR8 *AlarmBootCmdLine = " androidboot.alarmboot=true";

/*Send slot suffix in cmdline with which we have booted*/
STATIC CHAR8 *AndroidSlotSuffix = " androidboot.slot_suffix=";
STATIC CHAR8 *RootCmdLine = " rootwait ro init=";
STATIC CHAR8 *InitCmdline = INIT_BIN;
STATIC CHAR8 *SkipRamFs = " skip_initramfs";

/* Display command line related structures */
#define MAX_DISPLAY_CMD_LINE 256
STATIC CHAR8 DisplayCmdLine[MAX_DISPLAY_CMD_LINE];
STATIC UINTN DisplayCmdLineLen = sizeof (DisplayCmdLine);

#define MAX_DTBO_IDX_STR 64
STATIC CHAR8 *AndroidBootDtboIdx = " androidboot.dtbo_idx=";
STATIC CHAR8 *AndroidBootDtbIdx = " androidboot.dtb_idx=";

STATIC CONST CHAR8 *AndroidBootForceNormalBoot =
                                      " androidboot.force_normal_boot=1";
STATIC CONST CHAR8 *AndroidBootFstabSuffix =
                                      " androidboot.fstab_suffix=";
STATIC CHAR8 *FstabSuffixEmmc = "emmc";
STATIC CHAR8 *FstabSuffixDefault = "default";

/* Memory offline arguments */
STATIC CHAR8 *MemOff = " mem=";
STATIC CONST CHAR8 *MemHpState = " memhp_default_state=online";
STATIC CONST CHAR8 *MovableNode = " movable_node";

LIST_ENTRY *BootConfigListHead = NULL;
EFI_STATUS
TargetPauseForBatteryCharge (BOOLEAN *BatteryStatus)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_PM_PON_REASON_TYPE PONReason;
  EFI_QCOM_PMIC_PON_PROTOCOL *PmicPonProtocol;
  EFI_CHARGER_EX_PROTOCOL *ChgDetectProtocol;
  BOOLEAN ChgPresent;
  BOOLEAN WarmRtStatus;
  BOOLEAN IsColdBoot;

  /* Determines whether to pause for batter charge,
   * Serves only performance purposes, defaults to return zero*/
  *BatteryStatus = 0;

  Status = gBS->LocateProtocol (&gChargerExProtocolGuid, NULL,
                                (VOID **)&ChgDetectProtocol);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((EFI_D_VERBOSE, "Charger Protocol is not available.\n"));
    return EFI_SUCCESS;
  } else if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Error finding charger protocol: %r\n", Status));
    return Status;
  }

  /* The new protocol are supported on future chipsets */
  if (ChgDetectProtocol->Revision >= CHARGER_EX_REVISION) {
    Status = ChgDetectProtocol->IsOffModeCharging (BatteryStatus);
    if (EFI_ERROR (Status))
      DEBUG (
          (EFI_D_ERROR, "Error getting off mode charging info: %r\n", Status));

    return Status;
  } else {
    Status = gBS->LocateProtocol (&gQcomPmicPonProtocolGuid, NULL,
                                  (VOID **)&PmicPonProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Error locating pmic pon protocol: %r\n", Status));
      return Status;
    }

    /* Passing 0 for PMIC device Index since the protocol infers internally */
    Status = PmicPonProtocol->GetPonReason (0, &PONReason);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Error getting pon reason: %r\n", Status));
      return Status;
    }

    Status = PmicPonProtocol->WarmResetStatus (0, &WarmRtStatus);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Error getting warm reset status: %r\n", Status));
      return Status;
    }

    IsColdBoot = !WarmRtStatus;
    Status = ChgDetectProtocol->GetChargerPresence (&ChgPresent);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Error getting charger info: %r\n", Status));
      return Status;
    }

    DEBUG ((EFI_D_INFO, " PON Reason is %d cold_boot:%d charger path: %d\n",
            PONReason, IsColdBoot, ChgPresent));
    /* In case of fastboot reboot,adb reboot or if we see the power key
     * pressed we do not want go into charger mode.
     * fastboot/adb reboot is warm boot with PON hard reset bit set.
     */
    if (IsColdBoot && (!(PONReason.HARD_RESET) && (!(PONReason.KPDPWR)) &&
                       (PONReason.PON1 || PONReason.USB_CHG) && (ChgPresent))) {
      *BatteryStatus = 1;
    } else {
      *BatteryStatus = 0;
    }

    return Status;
  }
}

/**
  Check battery status
  @param[out] BatteryPresent  The pointer to battry's presence status.
  @param[out] ChargerPresent  The pointer to battry's charger status.
  @param[out] BatteryVoltage  The pointer to battry's voltage.
  @retval     EFI_SUCCESS     Check battery status successfully.
  @retval     other           Failed to check battery status.
**/
STATIC EFI_STATUS
TargetCheckBatteryStatus (BOOLEAN *BatteryPresent,
                          BOOLEAN *ChargerPresent,
                          UINT32 *BatteryVoltage)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_CHARGER_EX_PROTOCOL *ChgDetectProtocol;

  Status = gBS->LocateProtocol (&gChargerExProtocolGuid, NULL,
                                (void **)&ChgDetectProtocol);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((EFI_D_VERBOSE, "Charger Protocol is not available.\n"));
    return EFI_SUCCESS;
  } else if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Error locating charger detect protocol\n"));
    return EFI_PROTOCOL_ERROR;
  }

  Status = ChgDetectProtocol->GetBatteryPresence (BatteryPresent);
  if (EFI_ERROR (Status)) {
    /* Not critical. Hence, loglevel priority is low*/
    DEBUG ((EFI_D_VERBOSE, "Error getting battery presence: %r\n", Status));
    return Status;
  }

  Status = ChgDetectProtocol->GetBatteryVoltage (BatteryVoltage);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Error getting battery voltage: %r\n", Status));
    return Status;
  }

  Status = ChgDetectProtocol->GetChargerPresence (ChargerPresent);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Error getting charger presence: %r\n", Status));
    return Status;
  }

  return Status;
}

VOID
ParseVBCmdLine (CHAR8* VBCmdLine, UINT32 Len) {
  for (UINT32 Iter = 0 ; Iter < Len ; Iter++) {
    if (VBCmdLine[Iter] == ' ') {
      VBCmdLine[Iter] = '\n';
    }
  }
}
/**
   Add safeguards such as refusing to flash if the battery levels is lower than
 the min voltage
   or bypass if the battery is not present.
   @param[out] BatteryVoltage  The current voltage of battery
   @retval     BOOLEAN         The value whether the device is allowed to flash
 image.
 **/
BOOLEAN
TargetBatterySocOk (UINT32 *BatteryVoltage)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_CHARGER_EX_PROTOCOL *ChgDetectProtocol = NULL;
  EFI_CHARGER_EX_FLASH_INFO FlashInfo = {0};
  BOOLEAN BatteryPresent = FALSE;
  BOOLEAN ChargerPresent = FALSE;

  *BatteryVoltage = 0;
  Status = gBS->LocateProtocol (&gChargerExProtocolGuid, NULL,
                                (VOID **)&ChgDetectProtocol);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((EFI_D_VERBOSE, "Charger Protocol is not available.\n"));
    return TRUE;
  } else if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Error locating charger detect protocol\n"));
    return FALSE;
  }

  /* The new protocol are supported on future chipsets */
  if (ChgDetectProtocol->Revision >= CHARGER_EX_REVISION) {
    Status = ChgDetectProtocol->IsPowerOk (
        EFI_CHARGER_EX_POWER_FLASH_BATTERY_VOLTAGE_TYPE, &FlashInfo);
    if (EFI_ERROR (Status)) {
      /* But be bypassable where the device doesn't even have a battery */
      if (Status == EFI_UNSUPPORTED)
        return TRUE;

      DEBUG ((EFI_D_ERROR, "Error getting the info of charger: %r\n", Status));
      return FALSE;
    }

    *BatteryVoltage = FlashInfo.BattCurrVoltage;
    if (!(FlashInfo.bCanFlash) ||
        (*BatteryVoltage < FlashInfo.BattRequiredVoltage))
    {
      DEBUG ((EFI_D_ERROR, "Error battery voltage: %d "
        "Requireed voltage: %d, can flash: %d\n", *BatteryVoltage,
        FlashInfo.BattRequiredVoltage, FlashInfo.bCanFlash));
      return FALSE;
    }
    return TRUE;
  } else {
    Status = TargetCheckBatteryStatus (&BatteryPresent, &ChargerPresent,
                                       BatteryVoltage);
    if (((Status == EFI_SUCCESS) &&
         (!BatteryPresent ||
          (BatteryPresent && (*BatteryVoltage > BATT_MIN_VOLT)))) ||
        (Status == EFI_UNSUPPORTED)) {
      return TRUE;
    }

    DEBUG ((EFI_D_ERROR, "Error battery check status: %r voltage: %d\n",
        Status, *BatteryVoltage));
    return FALSE;
  }
}

STATIC VOID GetDisplayCmdline (VOID)
{
  EFI_STATUS Status;

  Status = gRT->GetVariable ((CHAR16 *)L"DisplayPanelConfiguration",
                             &gQcomTokenSpaceGuid, NULL, &DisplayCmdLineLen,
                             DisplayCmdLine);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to get Panel Config, %r\n", Status));
  }
}

/*
 * Returns length = 0 when there is failure.
 */
UINT32
GetSystemPath (CHAR8 **SysPath, BootInfo *Info)
{
  INT32 Index;
  UINT32 Lun;
  CHAR16 PartitionName[MAX_GPT_NAME_SIZE];
  Slot CurSlot = GetCurrentSlotSuffix ();
  CHAR8 LunCharMapping[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
  CHAR8 RootDevStr[BOOT_DEV_NAME_SIZE_MAX];

  *SysPath = AllocateZeroPool (sizeof (CHAR8) * MAX_PATH_SIZE);
  if (!*SysPath) {
    DEBUG ((EFI_D_ERROR, "Failed to allocated memory for System path query\n"));
    return 0;
  }

  if (IsLEVariant () &&
      Info->BootIntoRecovery) {
    StrnCpyS (PartitionName, MAX_GPT_NAME_SIZE, (CONST CHAR16 *)L"recoveryfs",
            StrLen ((CONST CHAR16 *)L"recoveryfs"));
  } else {
    StrnCpyS (PartitionName, MAX_GPT_NAME_SIZE, (CONST CHAR16 *)L"system",
            StrLen ((CONST CHAR16 *)L"system"));
  }

  /* Append slot info for A/B Variant */
  if (Info->MultiSlotBoot) {
     StrnCatS (PartitionName, MAX_GPT_NAME_SIZE, CurSlot.Suffix,
            StrLen (CurSlot.Suffix));
  }

  Index = GetPartitionIndex (PartitionName);
  if (Index == INVALID_PTN || Index >= MAX_NUM_PARTITIONS) {
    DEBUG ((EFI_D_ERROR, "System partition does not exist\n"));
    FreePool (*SysPath);
    *SysPath = NULL;
    return 0;
  }

  Lun = GetPartitionLunFromIndex (Index);
  GetRootDeviceType (RootDevStr, BOOT_DEV_NAME_SIZE_MAX);
  if (!AsciiStrCmp ("Unknown", RootDevStr)) {
    FreePool (*SysPath);
    *SysPath = NULL;
    return 0;
  }

  if (!AsciiStrCmp ("EMMC", RootDevStr)) {
    AsciiSPrint (*SysPath, MAX_PATH_SIZE, " root=/dev/mmcblk0p%d", Index);
  } else if (!AsciiStrCmp ("NAND", RootDevStr)) {
    /* NAND is being treated as GPT partition, hence reduce the index by 1 as
     * PartitionIndex (0) should be ignored for correct mapping of partition.
     */
    if (IsNANDSquashFsSupport ()) {
      // The gluebi device that is to be passed to "root=" will be the first one
      // after all "regular" mtd devices have been populated.
      UINT32 PartitionCount = 0;
      GetPartitionCount (&PartitionCount);
      AsciiSPrint (*SysPath, MAX_PATH_SIZE,
                   " rootfstype=squashfs root=/dev/mtdblock%d ubi.mtd=%d",
                   (PartitionCount - 1), (Index - 1));
    } else {
      AsciiSPrint (*SysPath, MAX_PATH_SIZE,
          " rootfstype=ubifs rootflags=bulk_read root=ubi0:rootfs ubi.mtd=%d",
          (Index - 1));
    }
  } else if (!AsciiStrCmp ("UFS", RootDevStr)) {
    AsciiSPrint (*SysPath, MAX_PATH_SIZE, " root=/dev/sd%c%d",
                 LunCharMapping[Lun],
                 GetPartitionIdxInLun (PartitionName, Lun));
  } else {
    DEBUG ((EFI_D_ERROR, "Unknown Device type\n"));
    FreePool (*SysPath);
    *SysPath = NULL;
    return 0;
  }
  DEBUG ((EFI_D_VERBOSE, "System Path - %a \n", *SysPath));

  return AsciiStrLen (*SysPath);
}

STATIC
EFI_STATUS
GetMemoryLimit (VOID *fdt, CHAR8 *MemOffAmt)
{
  UINT64 DdrSize = 0;
  UINT64 MemLimit;
  UINT32 i = 0;
  INT32 MemOfflineOffset;
  UINT64 *MemTable;
  INT32 PropLen;
  EFI_STATUS Status;

  if (IsLEVariant ()) {
    goto Unsupported;
  }

  Status = GetDdrSize (&DdrSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Error getting DDR size %r\n", Status));
    return Status;
  }

  MemLimit = DdrSize;
  MemOfflineOffset = FdtPathOffset (fdt, "/mem-offline");

  if (DdrSize < MEM_OFF_MIN ||
      MemOfflineOffset < 0) {
    goto Unsupported;
  }

  /* get table of offline sizes and subtract the size based off of DDR size */
  MemTable = (UINT64 *)fdt_getprop_w (fdt, MemOfflineOffset, "offline-sizes",
                                      &PropLen);
  if (!MemTable ||
       PropLen < 0) {
    goto Unsupported;
  }

  if (DdrSize >= SwapBytes64 (MemTable[0])) {
    for (i = (PropLen / sizeof (UINT64)) - 2; i >= 0; i -= 2) {
      if (DdrSize >= SwapBytes64 (MemTable[i])) {
        MemLimit -= SwapBytes64 (MemTable[i + 1]);
        break;
      }
    }
  }

  MemLimit /= MB_SIZE;

  AsciiSPrint (MemOffAmt, MEM_OFF_SIZE, "%dMB", MemLimit);

  return EFI_SUCCESS;

Unsupported:
  DEBUG ((EFI_D_INFO, "Offlining Memory Not Supported\n"));
  return EFI_UNSUPPORTED;
}

STATIC
EFI_STATUS
UpdateCmdLineParams (UpdateCmdLineParamList *Param,
                     CHAR8 **FinalCmdLine)
{
  CONST CHAR8 *Src;
  CHAR8 *Dst;
  UINT32 MaxCmdLineLen = Param->CmdLineLen;
  BOOLEAN BootConfigFlag = FALSE;

  Dst = AllocateZeroPool (MaxCmdLineLen);
  if (!Dst) {
    DEBUG ((EFI_D_ERROR, "CMDLINE: Failed to allocate destination buffer\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  /* Save start ptr for debug print */
  *FinalCmdLine = Dst;

  if (Param->HaveCmdLine) {
    Src = Param->CmdLine;
    AsciiStrCpyS (Dst, MaxCmdLineLen, Src);
  }

  if (Param->VBCmdLine != NULL) {
    if (Param->HeaderVersion <= BOOT_HEADER_VERSION_THREE) {
        Src = Param->VBCmdLine;
        AsciiStrCatS (Dst, MaxCmdLineLen, Src);
    } else {
        BootConfigFlag = IsAndroidBootParam (Param->VBCmdLine,
                              AsciiStrLen (Param->VBCmdLine),
                                       Param->HeaderVersion);
      ParseVBCmdLine ((CHAR8*) Param->VBCmdLine,
                 AsciiStrLen (Param->VBCmdLine));
      AddtoBootConfigList (BootConfigFlag, Param->VBCmdLine, NULL,
                  BootConfigListHead, AsciiStrLen (Param->VBCmdLine), 0);
    }
  }

  if ((Param->BootDevBuf) &&
      (Param->HeaderVersion <= BOOT_HEADER_VERSION_THREE)) {
    Src = Param->BootDeviceCmdLine;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);

    Src = Param->BootDevBuf;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);

    Src = Param->AndroidBootFstabSuffix;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);

    Src = Param->FstabSuffix;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);

    /* Dynamic partition append boot_devices for super partition */
    if (IsDynamicPartitionSupport ()) {
      Src = DynamicBootDeviceCmdLine;
      AsciiStrCatS (Dst, MaxCmdLineLen, Src);

      Src = Param->BootDevBuf;
      AsciiStrCatS (Dst, MaxCmdLineLen, Src);
    }

    FreePool (Param->BootDevBuf);
    Param->BootDevBuf = NULL;
  }

  if (Param->HeaderVersion <= BOOT_HEADER_VERSION_THREE) {
    Src = Param->UsbSerialCmdLine;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);
    Src = Param->StrSerialNum;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);
  }
  if (Param->FfbmStr &&
      (Param->FfbmStr[0] != '\0')) {
    if (Param->HeaderVersion <= BOOT_HEADER_VERSION_THREE) {
      Src = Param->AndroidBootMode;
      AsciiStrCatS (Dst, MaxCmdLineLen, Src);

      Src = Param->FfbmStr;
      AsciiStrCatS (Dst, MaxCmdLineLen, Src);
    }

    Src = Param->LogLevel;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);
  } else if (Param->PauseAtBootUp) {
      if (Param->HeaderVersion <= BOOT_HEADER_VERSION_THREE) {
        Src = Param->BatteryChgPause;
        AsciiStrCatS (Dst, MaxCmdLineLen, Src);
      }
  } else if (Param->AlarmBoot) {
      if (Param->HeaderVersion <= BOOT_HEADER_VERSION_THREE) {
        Src = Param->AlarmBootCmdLine;
        AsciiStrCatS (Dst, MaxCmdLineLen, Src);
      }
  }

  if (Param->HeaderVersion <= BOOT_HEADER_VERSION_THREE) {
    Src = BOOT_BASE_BAND;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);

    gBS->SetMem (Param->ChipBaseBand, CHIP_BASE_BAND_LEN, 0);
    AsciiStrnCpyS (Param->ChipBaseBand, CHIP_BASE_BAND_LEN,
                 BoardPlatformChipBaseBand (),
                 (CHIP_BASE_BAND_LEN - 1));
    ToLower (Param->ChipBaseBand);
    Src = Param->ChipBaseBand;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);

  }

  Src = Param->DisplayCmdLine;
  AsciiStrCatS (Dst, MaxCmdLineLen, Src);

  if (Param->MdtpActive) {
    Src = Param->MdtpActiveFlag;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);
  }

  if (Param->MultiSlotBoot &&
     !IsBootDevImage ()) {
     /* Slot suffix */
    if (Param->HeaderVersion <= BOOT_HEADER_VERSION_THREE) {
      Src = Param->AndroidSlotSuffix;
      AsciiStrCatS (Dst, MaxCmdLineLen, Src);
    }

    UnicodeStrToAsciiStr (GetCurrentSlotSuffix ().Suffix,
                          Param->SlotSuffixAscii);
    if (Param->HeaderVersion <= BOOT_HEADER_VERSION_THREE) {
      Src = Param->SlotSuffixAscii;
      AsciiStrCatS (Dst, MaxCmdLineLen, Src);
    } else {
      AddtoBootConfigList (BootConfigFlag, Param->AndroidSlotSuffix,
                     Param->SlotSuffixAscii,
                     BootConfigListHead,
                     AsciiStrLen (Param->AndroidSlotSuffix),
                     AsciiStrLen (Param->SlotSuffixAscii));
    }
  }

  if ((IsBuildAsSystemRootImage () &&
      !Param->MultiSlotBoot) ||
      (Param->MultiSlotBoot &&
      !IsBootDevImage ())) {

       /* Skip Initramfs*/
       if (!IsDynamicPartitionSupport () &&
           !Param->Recovery) {
         Src = Param->SkipRamFs;
         AsciiStrCatS (Dst, MaxCmdLineLen, Src);
       }

     /* Add root command line */
     Src = Param->RootCmdLine;
     AsciiStrCatS (Dst, MaxCmdLineLen, Src);

     /* Add init value*/
     Src = Param->InitCmdline;
     AsciiStrCatS (Dst, MaxCmdLineLen, Src);
   }

  if ((Param->DtboIdxStr != NULL) &&
       (Param->HeaderVersion <= BOOT_HEADER_VERSION_THREE)) {
    Src = Param->DtboIdxStr;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);
  }

  if ((Param->DtbIdxStr != NULL) &&
       (Param->HeaderVersion <= BOOT_HEADER_VERSION_THREE)) {
    Src = Param->DtbIdxStr;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);
  }

  if (((IsBuildUseRecoveryAsBoot () ||
      IsRecoveryHasNoKernel ()) &&
      IsDynamicPartitionSupport () &&
      !Param->Recovery) ||
      (!Param->MultiSlotBoot &&
       !IsBuildUseRecoveryAsBoot ())) {
    if (Param->HeaderVersion <= BOOT_HEADER_VERSION_THREE) {
      Src = AndroidBootForceNormalBoot;
      AsciiStrCatS (Dst, MaxCmdLineLen, Src);
    }
  }

  if (Param->LEVerityCmdLine != NULL) {
    Src = Param->LEVerityCmdLine;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);
    FreePool (Param->LEVerityCmdLine);
    Param->LEVerityCmdLine = NULL;
  }

  if (Param->MemOffAmt != NULL) {
    Src = MemOff;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);
    Src = Param->MemOffAmt;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);
    Src = MemHpState;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);
    Src = MovableNode;
    AsciiStrCatS (Dst, MaxCmdLineLen, Src);
  }

  return EFI_SUCCESS;
}
CHAR8* RemoveSpace (CHAR8* param, UINT32 ParamLen)
{
    UINT32 Iter, NewIter;
    CHAR8 *NewParam = param;

    for (Iter = 0, NewIter = 0; Iter < ParamLen; Iter++, NewIter++)
    {
        if (param[Iter] != ' ') {
          NewParam[NewIter] = param[Iter];
        }
        else {
          NewIter--;
        }
    }
    NewParam[NewIter] = '\0';
    return NewParam;
}
UINT32
ChangeFormattoBootConfig (CHAR8 *Param, UINT32 ParamLen)
{
  Param = RemoveSpace (Param, ParamLen);
  return (AsciiStrLen (Param));
}
struct BootConfigParamNode* AllocateBootConfigNode (UINT32 ParamLen)
{
  struct BootConfigParamNode *Node = NULL;
  Node = (struct BootConfigParamNode *)
         AllocateZeroPool (sizeof (struct BootConfigParamNode));
  if (!Node) {
    return NULL;
  }
  Node->param = (CHAR8 *)
                 AllocateZeroPool (ParamLen + SIZE_OF_DELIM);//to add \n
  if (!Node->param) {
    FreePool (Node);
    return NULL;
  }
  return Node;
}
VOID
AddParamToList (BOOLEAN BootConfigFlag,
                CONST CHAR8 *ParamKey,
                LIST_ENTRY *list,
                UINT32 ParamKeyLen)
{
  return;
}
VOID
AddtoBootConfigList (BOOLEAN BootConfigFlag,
                CONST CHAR8 *ParamKey,
                CONST CHAR8 *ParamValue,
                LIST_ENTRY *list,
                UINT32 ParamKeyLen,
                UINT32 ParamValueLen)
{
  struct BootConfigParamNode* NewNode = NULL;
  if (!BootConfigFlag) {
    return;
  }
  NewNode = (struct BootConfigParamNode *)
               AllocateBootConfigNode (ParamKeyLen + SIZE_OF_DELIM +
               SIZE_OF_DELIM + ParamValueLen);
  gBS->CopyMem (NewNode->param, (CHAR8*)ParamKey, ParamKeyLen);
  if (ParamValue) {
    gBS->CopyMem (&NewNode->param[ParamKeyLen], (CHAR8*)ParamValue,
                  ParamValueLen);
  }
  NewNode->ParamLen = ChangeFormattoBootConfig (NewNode->param,
                                               (ParamKeyLen + ParamValueLen));
  InsertTailList (list, &(NewNode->ListNode));
}

/* IsAndroidBootParam: Checks if the parameter is an androidboot.* kernel
 *                      parameter.
 * @param Param string
 * @param Param length
 * @return true if the parameter is androidboot.*
 */
BOOLEAN IsAndroidBootParam (CONST CHAR8 *param,
                            UINT32 ParamLen,
                            UINT32 HeaderVersion)
{
  if (ParamLen < 12) {
    return FALSE;
  }
  if (HeaderVersion <= BOOT_HEADER_VERSION_THREE) {
    return FALSE;
  }
  if (AsciiStrStr (param, "androidboot.")) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}
EFI_STATUS
UpdateBootConfigParams (LIST_ENTRY *BootConfigListHead,
                        UINT32 BootConfigLen,
                        CHAR8 **FinalBootConfig,
                        UINT32 *FinalBootConfigLen)
{
  CHAR8* Dst = NULL;
  LIST_ENTRY *Link = BootConfigListHead;
  struct BootConfigParamNode* Node = NULL;
  BootConfigLen += SIZE_OF_DELIM;

  if (BootConfigLen == 0) {
    return EFI_D_ERROR;
  }

  if (!BootConfigListHead) {
    return EFI_D_ERROR;
  }

  Dst = (CHAR8 *) AllocateZeroPool (BootConfigLen + SIZE_OF_DELIM);
  if (!Dst) {
    return EFI_OUT_OF_RESOURCES;
  }
  Link = GetFirstNode (BootConfigListHead);
  if (!Link) {
    DEBUG ((EFI_D_INFO, "Error in Node entry \n"));
  }

  gBS->CopyMem (Dst, "\n", SIZE_OF_DELIM);

  while (!IsNull (BootConfigListHead, Link)) {
    Node = BASE_CR (Link, struct BootConfigParamNode, ListNode);
    if (!Node) {
      DEBUG ((EFI_D_INFO, "Unable to read bsae struct \n"));
      return EFI_INVALID_PARAMETER;
    }

    AsciiStrCatS (Dst, BootConfigLen, (CHAR8*)Node->param);
    AsciiStrCatS (Dst, BootConfigLen, "\n");
    Link = GetNextNode (BootConfigListHead, Link);

  }
  Dst[AsciiStrLen (Dst) + 1] = '\0';
  *FinalBootConfig = Dst;
  *FinalBootConfigLen = AsciiStrLen (Dst) + 1;
  return EFI_SUCCESS;
}
VOID
ClearBootConfigList (LIST_ENTRY* BootConfigListHead)
{
  LIST_ENTRY *Link = NULL;
  LIST_ENTRY *NewLink = NULL;
  struct BootConfigParamNode* Node = NULL;

  if (!BootConfigListHead) {
    return;
  }

  Link = GetFirstNode (BootConfigListHead);
  if (!Link) {
    DEBUG ((EFI_D_INFO, "Error in Node entry \n"));
  }

  while (!IsNull (BootConfigListHead, Link)) {

    NewLink = RemoveEntryList (Link);

    Node = BASE_CR (Link, struct BootConfigParamNode, ListNode);

    if (!Node) {
      break;
    }

    if (!Node->param) {
      break;
    }

    FreePool (Node->param);
    FreePool (Node);
    Link = NewLink;

  }
  FreePool (BootConfigListHead);

  return;

}
/*Update command line: appends boot information to the original commandline
 *that is taken from boot image header*/
EFI_STATUS
UpdateCmdLine (CONST CHAR8 *CmdLine,
               CHAR8 *FfbmStr,
               BOOLEAN Recovery,
               BOOLEAN AlarmBoot,
               CONST CHAR8 *VBCmdLine,
               CHAR8 **FinalCmdLine,
               CHAR8 **FinalBootConfig,
               UINT32 *FinalBootConfigLen,
               UINT32 HeaderVersion,
               VOID *fdt)
{
  EFI_STATUS Status;
  UINT32 CmdLineLen = 0;
  UINT32 BootConfigLen = 0;
  UINT32 ParamLen = 0;
  UINT32 HaveCmdLine = 0;
  UINT32 PauseAtBootUp = 0;
  CHAR8 SlotSuffixAscii[MAX_SLOT_SUFFIX_SZ];
  BOOLEAN MultiSlotBoot;
  CHAR8 ChipBaseBand[CHIP_BASE_BAND_LEN];
  CHAR8 *BootDevBuf = NULL;
  BOOLEAN BatteryStatus;
  CHAR8 StrSerialNum[SERIAL_NUM_SIZE];
  BOOLEAN MdtpActive = FALSE;
  UpdateCmdLineParamList Param = {0};
  CHAR8 DtboIdxStr[MAX_DTBO_IDX_STR] = "\0";
  CHAR8 DtbIdxStr[MAX_DTBO_IDX_STR] = "\0";
  INT32 DtboIdx = INVALID_PTN;
  INT32 DtbIdx = INVALID_PTN;
  CHAR8 *LEVerityCmdLine = NULL;
  UINT32 LEVerityCmdLineLen = 0;
  CHAR8 RootDevStr[BOOT_DEV_NAME_SIZE_MAX];
  CHAR8 MemOffAmt[MEM_OFF_SIZE];
  BOOLEAN BootConfigFlag = FALSE;

  BootConfigListHead = (LIST_ENTRY*) AllocateZeroPool (sizeof (LIST_ENTRY));
  InitializeListHead (BootConfigListHead);

  Status = BoardSerialNum (StrSerialNum, sizeof (StrSerialNum));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Error Finding board serial num: %x\n", Status));
    return Status;
  }

  if (CmdLine && CmdLine[0]) {
    CmdLineLen = AsciiStrLen (CmdLine);
    HaveCmdLine = 1;
  }

  if (FixedPcdGetBool (EnableMdtpSupport)) {
    Status = IsMdtpActive (&MdtpActive);

    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Failed to get activation state for MDTP, "
                           "Status=%r. Considering MDTP as active\n",
              Status));
      MdtpActive = TRUE;
    }
  }

  if (VBCmdLine != NULL) {
    DEBUG ((EFI_D_VERBOSE, "UpdateCmdLine VBCmdLine present len %d\n",
            AsciiStrLen (VBCmdLine)));
    ParamLen = AsciiStrLen (VBCmdLine);
    BootConfigFlag = IsAndroidBootParam (VBCmdLine, ParamLen,
                                              HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen,
                                         BootConfigLen);
  }

  if (HaveCmdLine) {
    if (IsLEVerity ()) {
      Status = GetLEVerityCmdLine (CmdLine, &LEVerityCmdLine,
                                   &LEVerityCmdLineLen);
      if (Status != EFI_SUCCESS) {
        DEBUG ((EFI_D_ERROR, "Failed to get LEVerityCmdLine: %r\n", Status));
      }
      CmdLineLen += LEVerityCmdLineLen;
    }
  }

  BootDevBuf = AllocateZeroPool (sizeof (CHAR8) * BOOT_DEV_MAX_LEN);
  if (BootDevBuf == NULL) {
    DEBUG ((EFI_D_ERROR, "Boot device buffer: Out of resources\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetBootDevice (BootDevBuf, BOOT_DEV_MAX_LEN);
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Failed to get Boot Device: %r\n", Status));
    FreePool (BootDevBuf);
    BootDevBuf = NULL;
  } else {
    ParamLen = AsciiStrLen (BootDeviceCmdLine);
    BootConfigFlag = IsAndroidBootParam (BootDeviceCmdLine, ParamLen,
                                         HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen, BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, BootDeviceCmdLine, BootDevBuf,
                    BootConfigListHead, ParamLen, AsciiStrLen (BootDevBuf));
    ADD_PARAM_LEN (BootConfigFlag, AsciiStrLen (BootDevBuf),
                   CmdLineLen, BootConfigLen);

    if (IsDynamicPartitionSupport ()) {
      ParamLen = AsciiStrLen (DynamicBootDeviceCmdLine);
      BootConfigFlag = IsAndroidBootParam (DynamicBootDeviceCmdLine,
                                           ParamLen, HeaderVersion);
      ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen,
                                           BootConfigLen);
      AddtoBootConfigList (BootConfigFlag, DynamicBootDeviceCmdLine, BootDevBuf,
                 BootConfigListHead, ParamLen, AsciiStrLen (BootDevBuf));
      ADD_PARAM_LEN (BootConfigFlag, AsciiStrLen (BootDevBuf),
                     CmdLineLen, BootConfigLen);
    }
  }

  ParamLen = AsciiStrLen (UsbSerialCmdLine);
  BootConfigFlag = IsAndroidBootParam (UsbSerialCmdLine, ParamLen,
                                       HeaderVersion);
  ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen,
                                       BootConfigLen);
  AddtoBootConfigList (BootConfigFlag, UsbSerialCmdLine, StrSerialNum,
                     BootConfigListHead, ParamLen, AsciiStrLen (StrSerialNum));
  ADD_PARAM_LEN (BootConfigFlag, AsciiStrLen (StrSerialNum), CmdLineLen,
                                       BootConfigLen);

  /* Ignore the EFI_STATUS return value as the default Battery Status = 0 and is
   * not fatal */
  TargetPauseForBatteryCharge (&BatteryStatus);

  if (FfbmStr && FfbmStr[0] != '\0') {
    ParamLen = AsciiStrLen (AndroidBootMode);
    BootConfigFlag = IsAndroidBootParam (AndroidBootMode,
                                ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen,
                                         BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, AndroidBootMode, FfbmStr,
                      BootConfigListHead, ParamLen, AsciiStrLen (FfbmStr));
    ADD_PARAM_LEN (BootConfigFlag, AsciiStrLen (FfbmStr), CmdLineLen,
                                         BootConfigLen);
    /* reduce kernel console messages to speed-up boot */
    ParamLen = AsciiStrLen (LogLevel);
    BootConfigFlag = IsAndroidBootParam (LogLevel,
                         ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen,
                 CmdLineLen, BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, LogLevel, NULL,
               BootConfigListHead, ParamLen, 0);
  } else if (BatteryStatus &&
             IsChargingScreenEnable () &&
             !Recovery) {
    DEBUG ((EFI_D_INFO, "Device will boot into off mode charging mode\n"));
    PauseAtBootUp = 1;
    ParamLen = AsciiStrLen (BatteryChgPause);
    BootConfigFlag = IsAndroidBootParam (BatteryChgPause,
                                ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen,
                                         BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, BatteryChgPause, NULL,
                      BootConfigListHead, ParamLen, 0);
  } else if (AlarmBoot) {
    ParamLen = AsciiStrLen (AlarmBootCmdLine);
    BootConfigFlag = IsAndroidBootParam (AlarmBootCmdLine,
                                 ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen,
                                         BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, AlarmBootCmdLine, NULL,
                       BootConfigListHead, ParamLen, 0);
  }

  if (NULL == BoardPlatformChipBaseBand ()) {
    DEBUG ((EFI_D_ERROR, "Invalid BaseBand String\n"));
    FreePool (BootDevBuf);
    BootDevBuf = NULL;
    return EFI_NOT_FOUND;
  }

  ParamLen = AsciiStrLen (BOOT_BASE_BAND);
  BootConfigFlag = IsAndroidBootParam (BOOT_BASE_BAND,
                             ParamLen, HeaderVersion);
  ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen,
                                       BootConfigLen);
  AddtoBootConfigList (BootConfigFlag, BOOT_BASE_BAND,
                  BoardPlatformChipBaseBand (),
                  BootConfigListHead, ParamLen,
                  AsciiStrLen (BoardPlatformChipBaseBand ()));
  ADD_PARAM_LEN (BootConfigFlag, AsciiStrLen (BoardPlatformChipBaseBand ()),
                 CmdLineLen, BootConfigLen);

  if (MdtpActive) {
    ParamLen = AsciiStrLen (MdtpActiveFlag);
    BootConfigFlag = IsAndroidBootParam (MdtpActiveFlag,
                               ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen,
                                         BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, MdtpActiveFlag, NULL,
                     BootConfigListHead, ParamLen, 0);
  }
  MultiSlotBoot = PartitionHasMultiSlot ((CONST CHAR16 *)L"boot");
  if (MultiSlotBoot &&
     !IsBootDevImage ()) {
    /* Add additional length for slot suffix */
    ParamLen = AsciiStrLen (AndroidSlotSuffix) + MAX_SLOT_SUFFIX_SZ;
    BootConfigFlag = IsAndroidBootParam (AndroidSlotSuffix,
                               ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen,
                                         BootConfigLen);
  }

  if ((IsBuildAsSystemRootImage () &&
      !MultiSlotBoot) ||
      (MultiSlotBoot &&
      !IsBootDevImage ())) {
    ParamLen = AsciiStrLen (RootCmdLine);
    BootConfigFlag = IsAndroidBootParam (RootCmdLine,
                            ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen,
                 CmdLineLen, BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, RootCmdLine, NULL,
                  BootConfigListHead, ParamLen, 0);
    ParamLen = AsciiStrLen (InitCmdline);
    BootConfigFlag = IsAndroidBootParam (InitCmdline,
                            ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen,
                 CmdLineLen, BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, InitCmdline, NULL,
                  BootConfigListHead, ParamLen, 0);

       if (!IsDynamicPartitionSupport () &&
           !Recovery) {
         ParamLen = AsciiStrLen (SkipRamFs);
         BootConfigFlag = IsAndroidBootParam (SkipRamFs,
                               ParamLen, HeaderVersion);
         ADD_PARAM_LEN (BootConfigFlag, ParamLen,
                      CmdLineLen, BootConfigLen);
         AddtoBootConfigList (BootConfigFlag, SkipRamFs, NULL,
                     BootConfigListHead, ParamLen, 0);
       }
  }

  GetDisplayCmdline ();
  ParamLen = AsciiStrLen (DisplayCmdLine);
  BootConfigFlag = IsAndroidBootParam (DisplayCmdLine,
                             ParamLen, HeaderVersion);
  ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen,
                                       BootConfigLen);
  AddtoBootConfigList (BootConfigFlag, DisplayCmdLine, NULL,
                   BootConfigListHead, ParamLen, 0);

  if (!IsLEVariant ()) {
    DtboIdx = GetDtboIdx ();
    if (DtboIdx != INVALID_PTN) {
      AsciiSPrint (DtboIdxStr, sizeof (DtboIdxStr),
                   "%a%d", AndroidBootDtboIdx, DtboIdx);
      ParamLen = AsciiStrLen (DtboIdxStr);
      BootConfigFlag = IsAndroidBootParam (DtboIdxStr,
                             ParamLen, HeaderVersion);
      ADD_PARAM_LEN (BootConfigFlag, ParamLen,
                   CmdLineLen, BootConfigLen);
      AddtoBootConfigList (BootConfigFlag, DtboIdxStr, NULL,
                   BootConfigListHead, ParamLen, 0);
    }

    DtbIdx = GetDtbIdx ();
    if (DtbIdx != INVALID_PTN) {
      AsciiSPrint (DtbIdxStr, sizeof (DtbIdxStr),
                   "%a%d", AndroidBootDtbIdx, DtbIdx);
      ParamLen = AsciiStrLen (DtbIdxStr);
      BootConfigFlag = IsAndroidBootParam (DtbIdxStr,
                            ParamLen, HeaderVersion);
      ADD_PARAM_LEN (BootConfigFlag, ParamLen,
                   CmdLineLen, BootConfigLen);
      AddtoBootConfigList (BootConfigFlag, DtbIdxStr, NULL,
                  BootConfigListHead, ParamLen, 0);
    }
  }

  if (((IsBuildUseRecoveryAsBoot () ||
      IsRecoveryHasNoKernel ()) &&
      IsDynamicPartitionSupport () &&
      !Recovery) ||
      (!MultiSlotBoot &&
       !IsBuildUseRecoveryAsBoot ())) { 
    ParamLen = AsciiStrLen (AndroidBootForceNormalBoot);
    BootConfigFlag = IsAndroidBootParam (AndroidBootForceNormalBoot,
                                           ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen,
                                         BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, AndroidBootForceNormalBoot, NULL,
                    BootConfigListHead, ParamLen, 0);
  }

  ParamLen = AsciiStrLen (AndroidBootFstabSuffix);
  BootConfigFlag = IsAndroidBootParam (AndroidBootFstabSuffix, ParamLen,
                                       HeaderVersion);
  ADD_PARAM_LEN (BootConfigFlag, ParamLen,
                 CmdLineLen,
                 BootConfigLen);
  GetRootDeviceType (RootDevStr, BOOT_DEV_NAME_SIZE_MAX);
  if (!AsciiStriCmp (FstabSuffixEmmc, RootDevStr)) {
    Param.FstabSuffix = FstabSuffixEmmc;
  } else {
    Param.FstabSuffix = FstabSuffixDefault;
  }
  Param.AndroidBootFstabSuffix = AndroidBootFstabSuffix;
  AddtoBootConfigList (BootConfigFlag, AndroidBootFstabSuffix,
                  Param.FstabSuffix,
                  BootConfigListHead, ParamLen,
                  AsciiStrLen (Param.FstabSuffix));
  ADD_PARAM_LEN (BootConfigFlag, AsciiStrLen (Param.FstabSuffix),
                 CmdLineLen,
                 BootConfigLen);

  Status = GetMemoryLimit (fdt, MemOffAmt);
  /* Don't override "mem" argument if coded into boot image */
  if (Status == EFI_SUCCESS &&
      !AsciiStrStr (CmdLine, "mem=")) {
    ParamLen = AsciiStrLen (MemOff);
    BootConfigFlag = IsAndroidBootParam (MemOff, ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen, BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, MemOff, NULL, BootConfigListHead,
                                                             ParamLen, 0);
    ParamLen = AsciiStrLen (MemOffAmt);
    BootConfigFlag = IsAndroidBootParam (MemOffAmt, ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen, BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, MemOffAmt, NULL,
               BootConfigListHead, ParamLen, 0);
    ParamLen = AsciiStrLen (MemHpState);
    BootConfigFlag = IsAndroidBootParam (MemHpState, ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen, BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, MemHpState, NULL,
               BootConfigListHead, ParamLen, 0);
    ParamLen = AsciiStrLen (MovableNode);
    BootConfigFlag = IsAndroidBootParam (MovableNode, ParamLen, HeaderVersion);
    ADD_PARAM_LEN (BootConfigFlag, ParamLen, CmdLineLen, BootConfigLen);
    AddtoBootConfigList (BootConfigFlag, MovableNode, NULL,
               BootConfigListHead, ParamLen, 0);

    Param.MemOffAmt = MemOffAmt;
  } else {
    Param.MemOffAmt = NULL;
  }

  /* 1 extra byte for NULL */
  CmdLineLen += 1;

  Param.Recovery = Recovery;
  Param.MultiSlotBoot = MultiSlotBoot;
  Param.AlarmBoot = AlarmBoot;
  Param.MdtpActive = MdtpActive;
  Param.CmdLineLen = CmdLineLen;
  Param.HaveCmdLine = HaveCmdLine;
  Param.PauseAtBootUp = PauseAtBootUp;
  Param.StrSerialNum = StrSerialNum;
  Param.SlotSuffixAscii = SlotSuffixAscii;
  Param.ChipBaseBand = ChipBaseBand;
  Param.DisplayCmdLine = DisplayCmdLine;
  Param.CmdLine = CmdLine;
  Param.AlarmBootCmdLine = AlarmBootCmdLine;
  Param.MdtpActiveFlag = MdtpActiveFlag;
  Param.BatteryChgPause = BatteryChgPause;
  Param.UsbSerialCmdLine = UsbSerialCmdLine;
  Param.VBCmdLine = VBCmdLine;
  Param.LogLevel = LogLevel;
  Param.BootDeviceCmdLine = BootDeviceCmdLine;
  Param.AndroidBootMode = AndroidBootMode;
  Param.BootDevBuf = BootDevBuf;
  Param.FfbmStr = FfbmStr;
  Param.AndroidSlotSuffix = AndroidSlotSuffix;
  Param.SkipRamFs = SkipRamFs;
  Param.RootCmdLine = RootCmdLine;
  Param.InitCmdline = InitCmdline;
  Param.DtboIdxStr = DtboIdxStr;
  Param.DtbIdxStr = DtbIdxStr;
  Param.LEVerityCmdLine = LEVerityCmdLine;
  Param.HeaderVersion = HeaderVersion;

  Status = UpdateCmdLineParams (&Param, FinalCmdLine);
  if (Status != EFI_SUCCESS) {
    return Status;
  }
  Status = UpdateBootConfigParams (BootConfigListHead,
           BootConfigLen,
           FinalBootConfig, FinalBootConfigLen);
  if (Status != EFI_SUCCESS) {
    return Status;
  }

  ClearBootConfigList (BootConfigListHead);
  if (*FinalCmdLine) {
    DEBUG ((EFI_D_INFO, "Cmdline: %a\n", *FinalCmdLine));
  }
  DEBUG ((EFI_D_INFO, "\n"));
  if (*FinalBootConfig) {
    DEBUG ((EFI_D_INFO, "BootConfig: %a\n", *FinalBootConfig));
  }

  return EFI_SUCCESS;
}
