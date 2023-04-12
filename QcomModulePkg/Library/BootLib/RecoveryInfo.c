/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "RecoveryInfo.h"
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Protocol/EFIRecoveryInfo.h>
#include <Library/UefiBootServicesTableLib.h>


STATIC INT64 HasRecoveryInfo = -1;

BOOLEAN IsRecoveryInfo ()
{
  EFI_STATUS Status = EFI_SUCCESS ;
  EFI_RECOVERYINFO_PROTOCOL *pRecoveryInfoProtocol = NULL;

  if (HasRecoveryInfo == -1 ) {
    Status = gBS->LocateProtocol (& gEfiRecoveryInfoProtocolGuid, NULL,
                                  (VOID **) & pRecoveryInfoProtocol);
    if (Status == EFI_SUCCESS) {
      HasRecoveryInfo = 1;
    } else {
      HasRecoveryInfo = 0;
    }
  }

  return (HasRecoveryInfo == 1);
}
