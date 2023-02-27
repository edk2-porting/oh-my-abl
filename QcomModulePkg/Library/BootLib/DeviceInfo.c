/* Copyright (c) 2016-2018, 2021 The Linux Foundation. All rights reserved.
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
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include "AutoGen.h"
#include "LinuxLoaderLib.h"
#include "Board.h"
#include <FastbootLib/FastbootCmds.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PartitionTableUpdate.h>
#include <Library/Recovery.h>
#include <Library/StackCanary.h>

#define GOLDEN_SNAPSHOT_MAGIC 0x575757

STATIC DeviceInfo DevInfo;
STATIC BOOLEAN FirstReadDevInfo = TRUE;

BOOLEAN IsSnapshotGolden (VOID)
{
  return (DevInfo.GoldenSnapshot == GOLDEN_SNAPSHOT_MAGIC) ? TRUE : FALSE;
}

BOOLEAN IsUnlocked (VOID)
{
  return DevInfo.is_unlocked;
}

BOOLEAN IsUnlockCritical (VOID)
{
  return DevInfo.is_unlock_critical;
}

BOOLEAN IsEnforcing (VOID)
{
  return DevInfo.verity_mode;
}

BOOLEAN IsChargingScreenEnable (VOID)
{
  return DevInfo.is_charger_screen_enabled;
}

VOID
GetDevInfo (DeviceInfo **DevInfoPtr)
{
  *DevInfoPtr = &DevInfo;
}
VOID
GetBootloaderVersion (CHAR8 *BootloaderVersion, UINT32 Len)
{
  AsciiSPrint (BootloaderVersion, Len, "%a", DevInfo.bootloader_version);
}

VOID
GetRadioVersion (CHAR8 *RadioVersion, UINT32 Len)
{
  AsciiSPrint (RadioVersion, Len, "%a", DevInfo.radio_version);
}

EFI_STATUS
EnableChargingScreen (BOOLEAN IsEnabled)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (IsChargingScreenEnable () != IsEnabled) {
    DevInfo.is_charger_screen_enabled = IsEnabled;
    Status = ReadWriteDeviceInfo (WRITE_CONFIG, &DevInfo, sizeof (DevInfo));
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Error %a charger screen: %r\n",
              (IsEnabled ? "Enabling" : "Disabling"), Status));
      return Status;
    }
  }

  return Status;
}

EFI_STATUS
StoreAudioFrameWork (CONST CHAR8 *CmdLine, UINT32 CmdLineLen)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (CmdLineLen > ARRAY_SIZE (DevInfo.AudioFramework)) {
    DEBUG ((EFI_D_ERROR, "Audio framework is invalid, size too large!\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  gBS->SetMem (DevInfo.AudioFramework, sizeof (DevInfo.AudioFramework), 0);
  gBS->CopyMem (DevInfo.AudioFramework, (CHAR8 *) CmdLine, CmdLineLen);

  Status =
      ReadWriteDeviceInfo (WRITE_CONFIG, (VOID *)&DevInfo, sizeof (DevInfo));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to store audio framework: %r\n", Status));
    return Status;
  }
  return Status;
}

EFI_STATUS
ReadAudioFrameWork (CHAR8 **CmdLine, UINT32 *CmdLineLen)
{
  EFI_STATUS Status = EFI_SUCCESS;

  Status =
      ReadWriteDeviceInfo (READ_CONFIG, (VOID *)&DevInfo, sizeof (DevInfo));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to read audio framework: %r\n", Status));
    return Status;
  }

  *CmdLine = DevInfo.AudioFramework;
  *CmdLineLen = ARRAY_SIZE (DevInfo.AudioFramework);

  return Status;
}

EFI_STATUS
EnableEnforcingMode (BOOLEAN IsEnabled)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (IsEnforcing () != IsEnabled) {
    DevInfo.verity_mode = IsEnabled;
    Status = ReadWriteDeviceInfo (WRITE_CONFIG, &DevInfo, sizeof (DevInfo));
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "VBRwDeviceState Returned error: %r\n", Status));
      return Status;
    }
  }

  return Status;
}

STATIC EFI_STATUS
SetUnlockValue (BOOLEAN State)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (IsUnlocked () != State) {
    DevInfo.is_unlocked = State;
    Status = ReadWriteDeviceInfo (WRITE_CONFIG, &DevInfo, sizeof (DevInfo));
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Unable set the unlock value: %r\n", Status));
      return Status;
    }
  }

  return Status;
}

STATIC EFI_STATUS
SetUnlockCriticalValue (BOOLEAN State)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (IsUnlockCritical () != State) {
    DevInfo.is_unlock_critical = State;
    Status = ReadWriteDeviceInfo (WRITE_CONFIG, &DevInfo, sizeof (DevInfo));
    if (Status != EFI_SUCCESS) {
      DEBUG (
          (EFI_D_ERROR, "Unable set the unlock critical value: %r\n", Status));
      return Status;
    }
  }
  return Status;
}

EFI_STATUS
SetDeviceUnlockValue (UINT32 Type, BOOLEAN State)
{
  EFI_STATUS Status = EFI_SUCCESS;
  struct RecoveryMessage Msg;
  EFI_GUID Ptype = gEfiMiscPartitionGuid;
  MemCardType CardType = UNKNOWN;

  switch (Type) {
  case UNLOCK:
    Status = SetUnlockValue (State);
    break;
  case UNLOCK_CRITICAL:
    Status = SetUnlockCriticalValue (State);
    break;
  default:
    Status = EFI_UNSUPPORTED;
    break;
  }
  if (Status != EFI_SUCCESS)
    return Status;

  Status = ResetDeviceState ();
  if (Status != EFI_SUCCESS) {
    if (Type == UNLOCK)
      SetUnlockValue (!State);
    else if (Type == UNLOCK_CRITICAL)
      SetUnlockCriticalValue (!State);

    DEBUG ((EFI_D_ERROR, "Unable to set the Value: %r", Status));
    return Status;
  }

  gBS->SetMem ((VOID *)&Msg, sizeof (Msg), 0);
  Status = AsciiStrnCpyS (Msg.recovery, sizeof (Msg.recovery),
                          RECOVERY_WIPE_DATA, AsciiStrLen (RECOVERY_WIPE_DATA));
  if (Status == EFI_SUCCESS) {
    CardType = CheckRootDeviceType ();
    if (CardType == NAND) {
      Status = GetNandMiscPartiGuid (&Ptype);
      if (Status != EFI_SUCCESS) {
        return Status;
      }
    }

    Status = WriteToPartition (&Ptype, &Msg, sizeof (Msg));
  }

  return Status;
}

EFI_STATUS
UpdateDevInfo (CHAR16 *Pname, CHAR8 *ImgVersion)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (!StrCmp ((CONST CHAR16 *)Pname, (CONST CHAR16 *)L"bootloader")) {
    AsciiStrnCpyS (DevInfo.bootloader_version, MAX_VERSION_LEN, PRODUCT_NAME,
                   AsciiStrLen (PRODUCT_NAME));
    AsciiStrnCatS (DevInfo.bootloader_version, MAX_VERSION_LEN, "-",
                   AsciiStrLen ("-"));
    AsciiStrnCatS (DevInfo.bootloader_version, MAX_VERSION_LEN, ImgVersion,
                   AsciiStrLen (ImgVersion));
  } else {
    AsciiStrnCpyS (DevInfo.radio_version, MAX_VERSION_LEN, PRODUCT_NAME,
                   AsciiStrLen (PRODUCT_NAME));
    AsciiStrnCatS (DevInfo.radio_version, MAX_VERSION_LEN, "-",
                   AsciiStrLen ("-"));
    AsciiStrnCatS (DevInfo.radio_version, MAX_VERSION_LEN, ImgVersion,
                   AsciiStrLen (ImgVersion));
  }

  Status =
      ReadWriteDeviceInfo (WRITE_CONFIG, (VOID *)&DevInfo, sizeof (DevInfo));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to Write Device Info: %r\n", Status));
  }
  return Status;
}

EFI_STATUS DeviceInfoInit (VOID)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (FirstReadDevInfo) {
    Status =
        ReadWriteDeviceInfo (READ_CONFIG, (VOID *)&DevInfo, sizeof (DevInfo));
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Unable to Read Device Info: %r\n", Status));
      return Status;
    }

    FirstReadDevInfo = FALSE;
  }

  if (CompareMem (DevInfo.magic, DEVICE_MAGIC, DEVICE_MAGIC_SIZE)) {
    DEBUG ((EFI_D_ERROR, "Device Magic does not match\n"));
    gBS->SetMem (&DevInfo, sizeof (DevInfo), 0);
    gBS->CopyMem (DevInfo.magic, DEVICE_MAGIC, DEVICE_MAGIC_SIZE);
    DevInfo.user_public_key_length = 0;
    gBS->SetMem (DevInfo.rollback_index, sizeof (DevInfo.rollback_index), 0);
    gBS->SetMem (DevInfo.user_public_key, sizeof (DevInfo.user_public_key), 0);
    if (IsSecureBootEnabled ()) {
      DevInfo.is_unlocked = FALSE;
      DevInfo.is_unlock_critical = FALSE;
    } else {
      DevInfo.is_unlocked = TRUE;
      DevInfo.is_unlock_critical = TRUE;
    }
    DevInfo.is_charger_screen_enabled = FALSE;
    DevInfo.verity_mode = TRUE;
    Status =
        ReadWriteDeviceInfo (WRITE_CONFIG, (VOID *)&DevInfo, sizeof (DevInfo));
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Unable to Write Device Info: %r\n", Status));
      return Status;
    }
  }

  return Status;
}

EFI_STATUS
ReadRollbackIndex (UINT32 Loc, UINT64 *RollbackIndex)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (FirstReadDevInfo) {
    Status = EFI_NOT_STARTED;
    DEBUG ((EFI_D_ERROR, "ReadRollbackIndex DeviceInfo not initalized \n"));
    return Status;
  }

  if (Loc >= ARRAY_SIZE (DevInfo.rollback_index)) {
    Status = EFI_INVALID_PARAMETER;
    DEBUG ((EFI_D_ERROR, "ReadRollbackIndex Loc out of range, "
                         "index: %d, array len: %d\n",
            Loc, ARRAY_SIZE (DevInfo.rollback_index)));
    return Status;
  }

  *RollbackIndex = DevInfo.rollback_index[Loc];
  return Status;
}

EFI_STATUS
WriteRollbackIndex (UINT32 Loc, UINT64 RollbackIndex)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (FirstReadDevInfo) {
    Status = EFI_NOT_STARTED;
    DEBUG ((EFI_D_ERROR, "WriteRollbackIndex DeviceInfo not initalized \n"));
    return Status;
  }

  if (Loc >= ARRAY_SIZE (DevInfo.rollback_index)) {
    Status = EFI_INVALID_PARAMETER;
    DEBUG ((EFI_D_ERROR, "WriteRollbackIndex Loc out of range, "
                         "index: %d, array len: %d\n",
            Loc, ARRAY_SIZE (DevInfo.rollback_index)));
    return Status;
  }

  DevInfo.rollback_index[Loc] = RollbackIndex;
  Status =
      ReadWriteDeviceInfo (WRITE_CONFIG, (VOID *)&DevInfo, sizeof (DevInfo));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to Write Device Info: %r\n", Status));
    return Status;
  }
  return Status;
}

EFI_STATUS
StoreUserKey (CHAR8 *UserKey, UINT32 UserKeySize)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (FirstReadDevInfo) {
    Status = EFI_NOT_STARTED;
    DEBUG ((EFI_D_ERROR, "StoreUserKey DeviceInfo not initalized \n"));
    return Status;
  }

  if (UserKeySize > ARRAY_SIZE (DevInfo.user_public_key)) {
    DEBUG ((EFI_D_ERROR, "StoreUserKey, UserKeySize too large!\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  gBS->CopyMem (DevInfo.user_public_key, UserKey, UserKeySize);
  DevInfo.user_public_key_length = UserKeySize;
  Status =
      ReadWriteDeviceInfo (WRITE_CONFIG, (VOID *)&DevInfo, sizeof (DevInfo));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to Write Device Info: %r\n", Status));
    return Status;
  }
  return Status;
}

EFI_STATUS EraseUserKey (VOID)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (FirstReadDevInfo) {
    Status = EFI_NOT_STARTED;
    DEBUG ((EFI_D_ERROR, "EraseUserKey DeviceInfo not initalized \n"));
    return Status;
  }

  gBS->SetMem (DevInfo.user_public_key, sizeof (DevInfo.user_public_key), 0);
  DevInfo.user_public_key_length = 0;
  Status =
      ReadWriteDeviceInfo (WRITE_CONFIG, (VOID *)&DevInfo, sizeof (DevInfo));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to Write Device Info: %r\n", Status));
    return Status;
  }
  return Status;
}

EFI_STATUS
GetUserKey (CHAR8 **UserKey, UINT32 *UserKeySize)
{
  if (FirstReadDevInfo) {
    DEBUG ((EFI_D_ERROR, "GetUserKey DeviceInfo not initalized \n"));
    return EFI_NOT_STARTED;
  }

  *UserKey = DevInfo.user_public_key;
  *UserKeySize = DevInfo.user_public_key_length;
  return EFI_SUCCESS;
}


EFI_STATUS ReadPersistentValue (CONST UINT8 *Name, UINTN NameSize,
                                UINT8 *Value, UINTN *ValueSize)
{
   BOOLEAN  NameFound = FALSE;
   UINT32   i;

   if (FirstReadDevInfo) {
      DEBUG ((EFI_D_ERROR, "ReadPersistentValue DeviceInfo not initalized \n"));
      return EFI_NOT_STARTED;
   }

   if ( Name == NULL ||
        NameSize == 0 ||
        ValueSize == NULL  )
      return EFI_INVALID_PARAMETER;

   for ( i = 0; i < MAX_ENTRY_SIZE; ++i )
   {
      if ( DevInfo.persistent_value[i].in_use == 1 &&
          NameSize == DevInfo.persistent_value[i].name_size &&
          CompareMem ( Name, DevInfo.persistent_value[i].name, NameSize ) == 0 )
      {
         NameFound = TRUE;
         break;
      }
   }

   if ( NameFound == FALSE ) {
      return EFI_NOT_FOUND;
   }

   if ( DevInfo.persistent_value[i].value_size == 0 ) {
      return EFI_NOT_FOUND;
   }

   if ( *ValueSize < DevInfo.persistent_value[i].value_size )
   {
      *ValueSize =  DevInfo.persistent_value[i].value_size;
      return EFI_BUFFER_TOO_SMALL;
   }

   if ( Value == NULL ) {
      return EFI_INVALID_PARAMETER;
   }

   gBS->CopyMem ( Value,
                  DevInfo.persistent_value[i].value,
                  DevInfo.persistent_value[i].value_size);
   *ValueSize =  DevInfo.persistent_value[i].value_size;

   return EFI_SUCCESS;
}


EFI_STATUS WritePersistentValue (CONST UINT8 *Name, UINTN NameSize,
                                 CONST UINT8 *Value, UINTN ValueSize)
{
   EFI_STATUS Status = EFI_SUCCESS;
   BOOLEAN    NameFound = FALSE;
   BOOLEAN    SlotFound = FALSE;
   UINT32     NameFoundIndex;
   UINT32     SlotFoundIndex;
   UINT32     i;

   if (FirstReadDevInfo) {
      DEBUG ((EFI_D_ERROR, "ReadPersistentValue DeviceInfo not initalized \n"));
      return EFI_NOT_STARTED;
   }

   if ( Name == NULL ||
        NameSize == 0 ) {
      return EFI_INVALID_PARAMETER;
   }

   if ( NameSize > MAX_NAME_SIZE ) {
      return EFI_NOT_FOUND;
   }

   if ( ValueSize > MAX_VALUE_SIZE ) {
      return EFI_BAD_BUFFER_SIZE;
   }

   for ( i = 0; i < MAX_ENTRY_SIZE; ++i )
   {
      if ( DevInfo.persistent_value[i].in_use == 1 )
      {
          if ( NameSize == DevInfo.persistent_value[i].name_size &&
               CompareMem ( Name, DevInfo.persistent_value[i].name, NameSize )
               == 0 )
          {
             NameFound = TRUE;
             NameFoundIndex = i;
             break;
          }
       }
       else
       {
          if ( SlotFound == FALSE )
          {
             SlotFound = TRUE;
             SlotFoundIndex = i;
          }
       }
   }

   DEBUG ((EFI_D_ERROR, "NameFound %d NameIndex %d SlotFound %d SlotIndex %d\n",
           NameFound, NameFoundIndex, SlotFound, SlotFoundIndex ));

   if ( NameFound == TRUE )
   {
      if ( ValueSize == 0 )
      {
         gBS->SetMem ( &DevInfo.persistent_value[NameFoundIndex],
                       sizeof (persistent_value_type),
                       0 );
      }
      else
      {
         if ( Value == NULL ) {
            return EFI_INVALID_PARAMETER;
         }

         gBS->CopyMem ( (void *)DevInfo.persistent_value[NameFoundIndex].value,
                        (void *)Value,
                         ValueSize);
         DevInfo.persistent_value[NameFoundIndex].value_size =(UINT16)ValueSize;
      }
   }
   else if ( SlotFound == TRUE )
   {
      if ( Value == NULL ) {
         return EFI_INVALID_PARAMETER;
      }

      if ( ValueSize == 0 ) {
         return EFI_SUCCESS;
      }

      gBS->CopyMem ( (void *)DevInfo.persistent_value[SlotFoundIndex].name,
                     (void *)Name,
                      NameSize);
      DevInfo.persistent_value[SlotFoundIndex].name_size = NameSize;
      gBS->CopyMem ( (void *)DevInfo.persistent_value[SlotFoundIndex].value,
                     (void *)Value,
                     ValueSize);
      DevInfo.persistent_value[SlotFoundIndex].value_size = (UINT16)ValueSize;
      DevInfo.persistent_value[SlotFoundIndex].in_use = 1;
   }
   else
   {
      DEBUG ((EFI_D_ERROR, "No more slot available\n"));
      return EFI_INVALID_PARAMETER;
   }

   Status = ReadWriteDeviceInfo ( WRITE_CONFIG,
                                  (VOID *)&DevInfo,
                                  sizeof (DevInfo) );
   if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Unable to Write Device Info: 0x%x\n", Status));
   }

   return Status;
}

EFI_STATUS
SetSnapshotGolden (UINTN Val)
{
  EFI_STATUS Status = EFI_SUCCESS;

  if (FirstReadDevInfo) {
    Status = EFI_NOT_STARTED;
    DEBUG ((EFI_D_ERROR, "Erase swap on restore DeviceInfo not initalized \n"));
    return Status;
  }

  DevInfo.GoldenSnapshot = Val;
  Status = ReadWriteDeviceInfo (WRITE_CONFIG,
                        (VOID *)&DevInfo, sizeof (DevInfo));
  if (Status != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Unable to Write Device Info: %r\n", Status));
  }
  return Status;
}
