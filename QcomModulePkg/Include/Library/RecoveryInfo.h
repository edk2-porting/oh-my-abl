/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __RECOVERYINFO_H__
#define __RECOVERYINFO_H__

#include "PartitionTableUpdate.h"

BOOLEAN IsRecoveryInfo ();
EFI_STATUS RI_GetActiveSlot (Slot *);
#endif
