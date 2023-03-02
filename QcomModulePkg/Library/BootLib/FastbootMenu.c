/* Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
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

#include "AutoGen.h"
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/Debug.h>
#include <Library/DeviceInfo.h>
#include <Library/DrawUI.h>
#include <Library/FastbootMenu.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MenuKeysDetection.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UpdateDeviceTree.h>
#include <Library/BootLinux.h>
#include <Protocol/EFIVerifiedBoot.h>
#include <Uefi.h>

STATIC OPTION_MENU_INFO gMenuInfo;

STATIC MENU_MSG_INFO mFastbootOptionTitle[] = {
    {{"START"},
     BIG_FACTOR,
     BGR_GREEN,
     BGR_BLACK,
     OPTION_ITEM,
     0,
     RESTART},
    {{"Restart bootloader"},
     BIG_FACTOR,
     BGR_RED,
     BGR_BLACK,
     OPTION_ITEM,
     0,
     FASTBOOT},
    {{"Recovery mode"},
     BIG_FACTOR,
     BGR_RED,
     BGR_BLACK,
     OPTION_ITEM,
     0,
     RECOVER},
    {{"Power off"},
     BIG_FACTOR,
     BGR_WHITE,
     BGR_BLACK,
     OPTION_ITEM,
     0,
     POWEROFF},
    {{"Boot to FFBM"},
     BIG_FACTOR,
     BGR_YELLOW,
     BGR_BLACK,
     OPTION_ITEM,
     0,
     FFBM},
    {{"Boot to QMMI"},
     BIG_FACTOR,
     BGR_YELLOW,
     BGR_BLACK,
     OPTION_ITEM,
     0,
     QMMI},
    {{"Boot to Alternate Slot"},
      BIG_FACTOR,
      BGR_RED,
      BGR_BLACK,
      OPTION_ITEM,
      0,
      ALTERNATESLOT},
};

STATIC MENU_MSG_INFO mFastbootCommonWarnMsgInfo[] = {
    {{"Press volume key to select, "
      "and press power key to select."},
     COMMON_FACTOR,
     BGR_WHITE,
     BGR_BLACK,
     COMMON,
     0,
     NOACTION},
};

STATIC MENU_MSG_INFO mFastbootAlternateWarnMsgInfo[] = {
     {{"Your device is unbootable. It may not work properly. "
     "If needed, please press power button to force boot "
     "from alternate slot."},
     COMMON_FACTOR,
     BGR_WHITE,
     BGR_BLACK,
     COMMON,
     0,
     NOACTION},
};

STATIC MENU_MSG_INFO mFastbootCommonMsgInfo[] = {
    {{"FastBoot Mode"},
     COMMON_FACTOR,
     BGR_RED,
     BGR_BLACK,
     COMMON,
     0,
     NOACTION},
    {{"PRODUCT_NAME - "},
     COMMON_FACTOR,
     BGR_WHITE,
     BGR_BLACK,
     COMMON,
     0,
     NOACTION},
    {{"VARIANT - "},
     COMMON_FACTOR,
     BGR_WHITE,
     BGR_BLACK,
     COMMON,
     0,
     NOACTION},
    {{"BOOTLOADER VERSION - "},
     COMMON_FACTOR,
     BGR_WHITE,
     BGR_BLACK,
     COMMON,
     0,
     NOACTION},
    {{"BASEBAND VERSION - "},
     COMMON_FACTOR,
     BGR_WHITE,
     BGR_BLACK,
     COMMON,
     0,
     NOACTION},
    {{"SERIAL NUMBER - "},
     COMMON_FACTOR,
     BGR_WHITE,
     BGR_BLACK,
     COMMON,
     0,
     NOACTION},
    {{"SECURE BOOT - "},
     COMMON_FACTOR,
     BGR_WHITE,
     BGR_BLACK,
     COMMON,
     0,
     NOACTION},
    {{"DEVICE STATE - "},
     COMMON_FACTOR,
     BGR_RED,
     BGR_BLACK,
     COMMON,
     0,
     NOACTION},
};

STATIC EFI_STATUS CleanMessage (UINT32 MessageLen, UINT32 Location)
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINT32 Count = 0;
  UINT32 Height = 0;
  MENU_MSG_INFO *MessageInfo = NULL;

  MessageInfo = AllocateZeroPool (sizeof (MENU_MSG_INFO));
  if (MessageInfo == NULL) {
    DEBUG ((EFI_D_ERROR, "Failed to allocate zero pool.\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  if (MessageLen > sizeof (MessageInfo->Msg)) {
    DEBUG ((EFI_D_ERROR, "Invalid message length: %d\n", MessageLen));
    return EFI_OUT_OF_RESOURCES;
  }
  for (Count = 0; Count < MessageLen; Count++) {
    AsciiStrnCatS (MessageInfo->Msg, sizeof (MessageInfo->Msg), " ", 1);
  }

  SetMenuMsgInfo (MessageInfo, MessageInfo->Msg, COMMON_FACTOR,
                  BGR_WHITE, BGR_BLACK, COMMON, Location, NOACTION);
  Status = DrawMenu (MessageInfo, &Height);

  FreePool (MessageInfo);
  return Status;
}

/**
  Update the fastboot option item
  @param[in] OptionItem  The new fastboot option item
  @param[out] pLocation  The pointer of the location
  @retval EFI_SUCCESS	 The entry point is executed successfully.
  @retval other		 Some error occurs when executing this entry point.
 **/
EFI_STATUS
UpdateFastbootOptionItem (UINT32 OptionItem, UINT32 *pLocation)
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINT32 Location = 0;
  UINT32 Height = 0;
  UINT32 LineHeight = 0;
  UINT32 ClearStrLen = 0;
  MENU_MSG_INFO *FastbootLineInfo = NULL;
  UINT32 AlternateMsgLen = AsciiStrLen (mFastbootAlternateWarnMsgInfo[0].Msg);
  UINT32 CommonMsgLen = AsciiStrLen (mFastbootCommonWarnMsgInfo[0].Msg);
  UINT32 MaxLineLen = 0;

  FastbootLineInfo = AllocateZeroPool (sizeof (MENU_MSG_INFO));
  if (FastbootLineInfo == NULL) {
    DEBUG ((EFI_D_ERROR, "Failed to allocate zero pool.\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  SetMenuMsgInfo (FastbootLineInfo, "__________", COMMON_FACTOR,
                  mFastbootOptionTitle[OptionItem].FgColor,
                  mFastbootOptionTitle[OptionItem].BgColor, LINEATION, Location,
                  NOACTION);
  Status = DrawMenu (FastbootLineInfo, &Height);
  if (Status != EFI_SUCCESS)
    goto Exit;
  Location += Height;
  LineHeight = Height;

  mFastbootOptionTitle[OptionItem].Location = Location;
  Status = DrawMenu (&mFastbootOptionTitle[OptionItem], &Height);
  if (Status != EFI_SUCCESS)
    goto Exit;
  Location += Height;

  FastbootLineInfo->Location = Location;
  Status = DrawMenu (FastbootLineInfo, &Height);
  if (Status != EFI_SUCCESS)
    goto Exit;
  /* Add one more black line for message info */
  Location += Height * 2;

  /* Clear the screen before drawing */
  MaxLineLen = AsciiStrLen (FastbootLineInfo->Msg);
  if (FixedPcdGetBool (EnableForceBootAlternateSlot) &&
    IsSlotsUbootable ()) {
    ClearStrLen = AlternateMsgLen;
    if (AlternateMsgLen < CommonMsgLen) {
      ClearStrLen = CommonMsgLen;
    }
    if (ClearStrLen % MaxLineLen) {
      ClearStrLen = (ClearStrLen / MaxLineLen + 1) * MaxLineLen;
    }
    Status = CleanMessage (ClearStrLen, Location);
    if (Status != EFI_SUCCESS) {
     goto Exit;
    }
  }

  if (mFastbootOptionTitle[OptionItem].Action == ALTERNATESLOT) {
    mFastbootAlternateWarnMsgInfo[0].Location = Location;
    Status = DrawMenu (&mFastbootAlternateWarnMsgInfo[0], &Height);
  } else {
    mFastbootCommonWarnMsgInfo[0].Location = Location;
    Status = DrawMenu (&mFastbootCommonWarnMsgInfo[0], &Height);
  }
  if (Status != EFI_SUCCESS) {
    goto Exit;
  }
  /* Add two black lines for next message */
  Location += Height + LineHeight * 2;

Exit:
  FreePool (FastbootLineInfo);
  FastbootLineInfo = NULL;

  if (pLocation != NULL)
    *pLocation = Location;

  return Status;
}

/**
  Draw the fastboot menu
  @param[out] OptionMenuInfo  Fastboot option info
  @retval     EFI_SUCCESS     The entry point is executed successfully.
  @retval     other           Some error occurs when executing this entry point.
 **/
STATIC EFI_STATUS
FastbootMenuShowScreen (OPTION_MENU_INFO *OptionMenuInfo)
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINT32 Location = 0;
  UINT32 OptionItem = 0;
  UINT32 Height = 0;
  UINT32 i = 0;
  CHAR8 StrTemp[MAX_RSP_SIZE] = "";
  CHAR8 StrTemp1[MAX_RSP_SIZE] = "";
  CHAR8 VersionTemp[MAX_VERSION_LEN] = "";
  UINT32 OptionTotal = ARRAY_SIZE (mFastbootOptionTitle);

  ZeroMem (&OptionMenuInfo->Info, sizeof (MENU_OPTION_ITEM_INFO));

  /* Only add alternate boot option when device is unbootable */
  if (FixedPcdGetBool (EnableForceBootAlternateSlot) &&
     !IsSlotsUbootable ()) {
      OptionTotal = OptionTotal - 1;
  }

  /* Update fastboot option title */
  OptionMenuInfo->Info.MsgInfo = mFastbootOptionTitle;
  for (i = 0; i < OptionTotal; i++) {
    OptionMenuInfo->Info.OptionItems[i] = i;
  }
  OptionItem =
      OptionMenuInfo->Info.OptionItems[OptionMenuInfo->Info.OptionIndex];
  Status = UpdateFastbootOptionItem (OptionItem, &Location);
  if (Status != EFI_SUCCESS)
    return Status;

  /* Update fastboot common message */
  for (i = 0; i < ARRAY_SIZE (mFastbootCommonMsgInfo); i++) {
    switch (i) {
    case 0:
      break;
    case 1:
      /* Get product name */
      AsciiStrnCatS (mFastbootCommonMsgInfo[i].Msg,
        sizeof (mFastbootCommonMsgInfo[i].Msg), PRODUCT_NAME,
        AsciiStrLen (PRODUCT_NAME));
      break;
    case 2:
      /* Get variant value */
      BoardHwPlatformName (StrTemp, sizeof (StrTemp));
      GetRootDeviceType (StrTemp1, sizeof (StrTemp1));

      AsciiStrnCatS (mFastbootCommonMsgInfo[i].Msg,
                     sizeof (mFastbootCommonMsgInfo[i].Msg), StrTemp,
                     sizeof (StrTemp));
      AsciiStrnCatS (mFastbootCommonMsgInfo[i].Msg,
                     sizeof (mFastbootCommonMsgInfo[i].Msg), " ",
                     AsciiStrLen (" "));
      AsciiStrnCatS (mFastbootCommonMsgInfo[i].Msg,
                     sizeof (mFastbootCommonMsgInfo[i].Msg), StrTemp1,
                     sizeof (StrTemp1));
      break;
    case 3:
      /* Get bootloader version */
      GetBootloaderVersion (VersionTemp, sizeof (VersionTemp));
      AsciiStrnCatS (mFastbootCommonMsgInfo[i].Msg,
                     sizeof (mFastbootCommonMsgInfo[i].Msg), VersionTemp,
                     sizeof (VersionTemp));
      break;
    case 4:
      /* Get baseband version */
      ZeroMem (VersionTemp, sizeof (VersionTemp));
      GetRadioVersion (VersionTemp, sizeof (VersionTemp));
      AsciiStrnCatS (mFastbootCommonMsgInfo[i].Msg,
                     sizeof (mFastbootCommonMsgInfo[i].Msg), VersionTemp,
                     sizeof (VersionTemp));
      break;
    case 5:
      /* Get serial number */
      ZeroMem (StrTemp, sizeof (StrTemp));
      BoardSerialNum (StrTemp, MAX_RSP_SIZE);
      AsciiStrnCatS (mFastbootCommonMsgInfo[i].Msg,
                     sizeof (mFastbootCommonMsgInfo[i].Msg), StrTemp,
                     sizeof (StrTemp));
      break;
    case 6:
      /* Get secure boot value */
      AsciiStrnCatS (
          mFastbootCommonMsgInfo[i].Msg, sizeof (mFastbootCommonMsgInfo[i].Msg),
          IsSecureBootEnabled () ? "yes" : "no",
          IsSecureBootEnabled () ? AsciiStrLen ("yes") : AsciiStrLen ("no"));
      break;
    case 7:
      /* Get device status */
      AsciiStrnCatS (
          mFastbootCommonMsgInfo[i].Msg, sizeof (mFastbootCommonMsgInfo[i].Msg),
          IsUnlocked () ? "unlocked" : "locked",
          IsUnlocked () ? AsciiStrLen ("unlocked") : AsciiStrLen ("locked"));
      break;
    }

    mFastbootCommonMsgInfo[i].Location = Location;
    Status = DrawMenu (&mFastbootCommonMsgInfo[i], &Height);
    if (Status != EFI_SUCCESS)
      return Status;
    Location += Height;
  }

  OptionMenuInfo->Info.MenuType = DISPLAY_MENU_FASTBOOT;
  OptionMenuInfo->Info.OptionNum = OptionTotal;

  return Status;
}

/* Draw the fastboot menu and start to detect the key's status */
VOID DisplayFastbootMenu (VOID)
{
  EFI_STATUS Status;
  OPTION_MENU_INFO *OptionMenuInfo;

  if (IsEnableDisplayMenuFlagSupported ()) {
    OptionMenuInfo = &gMenuInfo;
    DrawMenuInit ();
    OptionMenuInfo->LastMenuType = OptionMenuInfo->Info.MenuType;

    Status = FastbootMenuShowScreen (OptionMenuInfo);
    if (Status != EFI_SUCCESS) {
      DEBUG ((EFI_D_ERROR, "Unable to show fastboot menu on screen: %r\n",
              Status));
      return;
    }

    MenuKeysDetectionInit (OptionMenuInfo);
    DEBUG ((EFI_D_VERBOSE, "Creating fastboot menu keys detect event\n"));
  } else {
    DEBUG ((EFI_D_INFO, "Display menu is not enabled!\n"));
  }
}
