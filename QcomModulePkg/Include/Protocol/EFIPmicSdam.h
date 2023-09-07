/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __EFIPMICSDAM_H__
#define __EFIPMICSDAM_H__

/*===========================================================================
  MACRO DECLARATIONS
===========================================================================*/
/** @ingroup efi_pmicSdam_constants
  Protocol version.
*/
#define PMIC_SDAM_REVISION 0x0000000000010002
/** @} */ /* end_addtogroup efi_pmicSdam_constants */

/* Protocol GUID definition */
/** @ingroup efi_pmicSdam_protocol */
#define EFI_PMIC_SDAM_PROTOCOL_GUID \
{0x66bf5742, 0x3038, 0x4049, { 0x8e, 0x9a, 0x42, 0x36, 0xdb, 0x77, 0x06, 0xdd }}

/** @cond */
/**
  External reference to the PMIC Sdam Protocol GUID.
*/
extern EFI_GUID gQcomPmicSdamProtocolGuid;

/*===========================================================================
  TYPE DEFINITIONS
===========================================================================*/
/**
  PMIC SDAM UEFI typedefs
*/
typedef struct _EFI_QCOM_PMIC_SDAM_PROTOCOL   EFI_QCOM_PMIC_SDAM_PROTOCOL;
/** @endcond */

/*===========================================================================
  FUNCTION DEFINITIONS
===========================================================================*/

/* EFI_PM_SDAM_MEM_READ */
/** @ingroup efi_pmicSdam_mem_read
  @par Summary
  Read specific SDAM memory items for a specific device index.

  @param[in]  PmicDeviceIndex  Primary: 0. Secondary: 1

  @param[in]  SdamSrcIndex     SDAM to read from

  @param[in]  StartAddress     Starting address of the SDAM register block

  @param[in]  NumOfBytes       Number of SDAM registers to read

  @param[out] DataPtr          Pointer to data memory where the SDAM register
                               value will be copied into

  @return
  EFI_SUCCESS        -- Function completed successfully. \n
  EFI_PROTOCOL_ERROR -- Error occurred during the operation.
*/
typedef
EFI_STATUS (EFIAPI *EFI_PM_SDAM_MEM_READ)(
  IN   UINT32                     PmicDeviceIndex,
  IN   UINT32                     SdamSrcIndex,
  IN   UINT8                      StartAddress,
  IN   UINT32                     NumOfBytes,
  OUT  UINT8                      *DataPtr
);

/* EFI_PM_SDAM_MEM_WRITE */
/** @ingroup efi_pmicSdam_mem_write
  @par Summary
  Read specific SDAM memory items for a specific device index.

  @param[in]  PmicDeviceIndex  Primary: 0. Secondary: 1.

  @param[in]  SdamSrcIndex     SDAM to read from

  @param[in]  StartAddress     Starting address of the SDAM register block

  @param[in]  NumOfBytes       Number of SDAM registers to read

  @param[in]  DataPtr          Pointer to data memory where the SDAM register
                               value will be copied into the register

  @return
  EFI_SUCCESS        -- Function completed successfully. \n
  EFI_PROTOCOL_ERROR -- Error occurred during the operation.
*/
typedef
EFI_STATUS (EFIAPI *EFI_PM_SDAM_MEM_WRITE)(
  IN   UINT32                     PmicDeviceIndex,
  IN   UINT32                     SdamSrcIndex,
  IN   UINT8                      StartAddress,
  IN   UINT32                     NumOfBytes,
  IN   UINT8                      *DataPtr
);

/*===========================================================================
  PROTOCOL INTERFACE
===========================================================================*/
/** @ingroup efi_pmicSdam_protocol
  @par Summary
  Qualcomm PMIC Sdeam Protocol interface.

  @par Parameters
  @inputprotoparams{pmic_sdam_proto_params.tex}
*/
struct _EFI_QCOM_PMIC_SDAM_PROTOCOL {
  UINT64                               Revision;
  EFI_PM_SDAM_MEM_READ                 SdamMemRead;
  EFI_PM_SDAM_MEM_WRITE                SdamMemWrite;
};


#endif  /* __EFIPMICVERSION_H__ */
