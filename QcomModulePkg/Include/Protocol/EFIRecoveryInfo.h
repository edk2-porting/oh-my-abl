/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
/**
  @file  EFIRecoveryInfo.h
  @brief RecoveryInfo EFI protocol interface.
*/

/*=============================================================================
                              EDIT HISTORY


 when       who     what, where, why
 --------   ---     -----------------------------------------------------------
 03/17/23   ac      Initial revision

=============================================================================*/
#ifndef __EFIRECOVERYINFO_H__
#define __EFIRECOVERYINFO_H__

/** @defgroup EFI_RECOVERYINFO_PROTOCOL  EFI_RECOVERYINFO_PROTOCOL
 *  @ingroup UEFI_CORE
 */
 /** @defgroup  EFI_RECOVERYINFO_PROTOCOL_prot PROTOCOL
 *  @ingroup EFI_RECOVERYINFO_PROTOCOL
 */
 /** @defgroup  EFI_RECOVERYINFO_PROTOCOL_apis APIs
 *  @ingroup EFI_RECOVERYINFO_PROTOCOL
 */

/** @defgroup  EFI_RECOVERYINFO_PROTOCOL_data DATA_STRUCTURES
 *  @ingroup EFI_RECOVERYINFO_PROTOCOL
 */

/* Protocol declaration */
typedef struct _EFI_RECOVERYINFO_PROTOCOL EFI_RECOVERYINFO_PROTOCOL;

/*===========================================================================
MACRO DECLARATIONS
===========================================================================*/
/* Protocol version. */
#define EFI_RECOVERYINFO_PROTOCOL_REVISION 0x00010000

/* Protocol GUID definition */
#define EFI_RECOVERYINFO_PROTOCOL_GUID \
        { 0x107e9a76, 0x4fa5, 0x4b9c, {0x9f, 0xef, 0x35, 0x5e, 0xe0, 0x07, \
              0xfe, 0x61 } };

/*===========================================================================
EXTERNAL VARIABLES
===========================================================================*/
/* External reference to the RecoveryInfo Protocol GUID. */
extern EFI_GUID gEfiRecoveryInfoProtocolGuid;

/*===========================================================================
TYPE DEFINITIONS
===========================================================================*/
/** @addtogroup EFI_RECOVERYINFO_PROTOCOL_data
@{ */
/* Recovery status enum type */
typedef enum {
    RECOVERY_INFO_NO_RECOVERY,        /* GPIO_BASED_BOOT_SELECTION fuse bit
                                         is blown */
    RECOVERY_INFO_PARTITION_FAIL,     /* Recovery Info Partition not present
                                         or fail to read */
    RECOVERY_INFO_TRIAL_BOOT,         /* Trial Boot is enabled */
    RECOVERY_INFO_RECOVERY,           /* Recovery Info is valid */
}RECOVERY_STATUS_STATE;
/** @} */

/** @addtogroup EFI_RECOVERYINFO_PROTOCOL_data
@{ */
/* Boot set enum type */
typedef enum {
    SET_A             = 0x0U,
    SET_B             = 0x1U,
    SET_INVALID       = 0xFFFF,
}BootSetType;
/** @} */

/*=============================================================================

                             API IMPLEMENTATION

=============================================================================*/

/* ============================================================================
**  Function : EFI_Get_Boot_Set
** ============================================================================
*/
/** @ingroup EFI_RECOVERYINFO_PROTOCOL_apis
  @par Summary
  Gets the boot set to boot

  @param[in]      This               Pointer to the EFI_RECOVERYINFO_PROTOCOL
                                     instance.
  @param[out]     BootSet            Pointer to a BootSetType passed by the
                                     caller that
                                     will be populated by the driver.

  @return
  EFI_SUCCESS           -- Function completed successfully. \n
  EFI_INVALID_PARAMETER -- Input parameter is INVALID. \n
*/

typedef
EFI_STATUS
(EFIAPI *EFI_GET_BOOT_SET)(
   IN EFI_RECOVERYINFO_PROTOCOL  *This,
   OUT BootSetType               *BootSet
   );

/* ============================================================================
**  Function : EFI_Handle_Failed_Set
** ============================================================================
*/
/** @ingroup EFI_RECOVERYINFO_PROTOCOL_apis
  @par Summary
  Handler for Failed Boot set

  In case of Recovery-
    -Marks the status of FailedBootSet and reset if more set to try
    -Assert if no set to try

  In case of Trial Boot-
    -Reset if trial boot max attempt not reached
    -Assert if trial boot max attempt reached

  Other Cases - Assert

  @param[in]   This                  Pointer to the EFI_RECOVERYINFO_PROTOCOL
                                     instance.
  @param[in]   FailedBootSet         Boot set failed to boot

  @return
  EFI_INVALID_PARAMETER -- Input parameter is INVALID. \n
  EFI_UNSUPPORTED       -- In case of GPIO_BASED_BOOT_SELECTION fuse is blown
                           or recoveryinfo Partiton Failure \n
*/

typedef
EFI_STATUS
(EFIAPI *EFI_HANDLE_FAILED_SET)(
   IN EFI_RECOVERYINFO_PROTOCOL *This,
   IN BootSetType                FailedBootSet
   );



/*===========================================================================
  PROTOCOL INTERFACE
===========================================================================*/
/** @ingroup EFI_RECOVERYINFO_PROTOCOL_prot
  @par Summary
  RecoveryInfo Information Protocol interface.

  @par Parameters
*/
struct _EFI_RECOVERYINFO_PROTOCOL {
   UINT64                     Revision;
   EFI_GET_BOOT_SET           GetBootSet;
   EFI_HANDLE_FAILED_SET      HandleFailedSet;
};


#endif /* __EFIRECOVERYINFO_H__ */
