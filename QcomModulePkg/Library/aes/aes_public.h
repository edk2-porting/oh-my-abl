/***************************************************************************
Copyright (c) 2021 The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************/

/*
 *  Changes from Qualcomm Innovation Center are provided under the following
 *  license:
 *
 *  Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights
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

#ifndef AES_PUBLIC_H
#define AES_PUBLIC_H

/*===========================================================================
                     INCLUDE FILES FOR MODULE
===========================================================================*/
#include <avb/libavb/libavb.h>
#include "env_comdef.h"
#include "utils.h"
#include "modes.h"

#define ARMV8_AES_MAXNR             14
#define CIPHER_AES_BLK_SIZE         16

#define CIPHER_AES_IV_SIZE          16                           /** bytes */
#define CIPHER_MAX_IV_SIZE          60                           /** bytes */

/**
 * AES Key and IV sizes
 */
#define SW_AES128_KEY_SIZE          16
#define SW_AES192_KEY_SIZE          24
#define SW_AES256_KEY_SIZE          32
#define CIPHER_MAX_KEY_SIZE         SW_AES256_KEY_SIZE
#define CIPHER_INVALID_KEY_SIZE     0xFFFF                     /** invalid value for key size */


#define SW_AES_IV_SIZE         CIPHER_AES_IV_SIZE
#define SW_AES_BLOCK_BYTE_LEN  16
#define SW_AES_MAX_IV_SIZE     60

#ifndef IOVECDEF
typedef  struct  {
  void                              *pvBase;
  uint32                            dwLen;
} IovecType/* TODO: have to resolve compiler warning properly __attribute__((packed)) */;

typedef  struct  {
  IovecType                     *iov;
  uint32                            size;
} IovecListType/* TODO: have to resolve compiler warning properly __attribute__((packed)) */;

#define IOVECDEF
#endif


/**
 * Enum for Cipher mode
 */
typedef enum
{
  SW_CIPHER_MODE_GCM          = 0x1,    ///< GCM Mode
  SW_CIPHER_MODE_MAX          = 0x7FFFFFFF
} SW_CipherModeType;


/**
 * Enum for Cipher direction
 */
typedef enum
{
  SW_CIPHER_ENCRYPT           = 0x00,
  SW_CIPHER_DECRYPT           = 0x01,
  SW_CIPHER_MAX               = 0x7FFFFFFF
} SW_CipherEncryptDir;

/**
 * Enum for Cipher parameters
 */
typedef enum
{
  SW_CIPHER_PARAM_DIRECTION   = 0x01,
  SW_CIPHER_PARAM_KEY         = 0x02,
  SW_CIPHER_PARAM_IV          = 0x03,
  SW_CIPHER_PARAM_MODE        = 0x04,
  SW_CIPHER_PARAM_BLOCKSIZE   = 0x05,
  SW_CIPHER_PARAM_KEY_SIZE    = 0x06,
  SW_CIPHER_PARAM_IV_SIZE     = 0x07,
  SW_CIPHER_PARAM_AAD         = 0x08,     //Additional plaintext data for AES-GCM
  SW_CIPHER_PARAM_TAG         = 0x09,     //Calculated TAG by AES-GCM
  SW_CIPHER_PARAM_NONCE       = 0x10,
  SW_CIPHER_PARAM_PAYLOAD_LEN = 0x11,
  SW_CIPHER_PARAM_MAX         = 0x7FFFFFFF
} SW_CipherParam;

/**
 * Enum for Cipher algorithm type
 */
typedef enum {
  SW_CIPHER_ALG_AES128            = 0x0,
  SW_CIPHER_ALG_AES256            = 0x1,
  SW_CIPHER_ALG_MAX               = 0x7FFFFFFF
} SW_Cipher_Alg_Type;
/**
 * Enum for Cipher key size
 */
typedef enum
{
  SW_CIPHER_KEY_SIZE_AES128      = 0x0,
  SW_CIPHER_KEY_SIZE_AES256      = 0x1,
  SW_CIPHER_KEY_SIZE_MAX         = 0x7FFFFFFF
} SW_Cipher_Key_Size;

typedef struct
{
  uint32_t  rd_key[4 * (ARMV8_AES_MAXNR + 1)];
  int       rounds;
} AES_KEY;

typedef struct AES_GCM_ARMV8_ctx_s
{
  AES_KEY key;
  GCM128_CONTEXT *ctx;
  uint8_t  itag[CIPHER_AES_BLK_SIZE];
  size_t   tag_sz;
  uint32_t xcm_flags;
  uint32_t flags;
  unsigned char iv[CIPHER_MAX_IV_SIZE]; /** Pointer to IV buffer   */
  size_t   iv_sz;
  uint8_t  c_key[CIPHER_MAX_KEY_SIZE]; /** Pointer to Cipher Key  */
  size_t   c_keysize;                  /** Key Size in byte   */
  SW_Cipher_Alg_Type algo;
  SW_CipherModeType        mode; /** currently supports GCM only*/
  SW_CipherEncryptDir dir;
} AES_GCM_ARMV8_CTX;

/* MAX_GCM_CONTEXTS is the max number of contexts */
typedef struct GcmAesStruct {
  AES_GCM_ARMV8_CTX gAesGcmArmV8Ctx;
  boolean gInitDone;
  UINT32 InstanceId;
}GcmAesStruct;

#ifndef _DEFINED_CRYPTOCNTXHANDLE
#define _DEFINED_CRYPTOCNTXHANDLE
typedef void CryptoCntxHandle;
#endif

/** below APIs are non thread safe , client has to make sure APIs are called in single thread context*/

/**
 * @brief Intialize a cipher context
 *
 * @param pAlgo        [in] Cipher algorithm type
 *
 * @return sw_crypto_errno_enum_type
 *
 */

sw_crypto_errno_enum_type SW_Cipher_Init( SW_Cipher_Alg_Type pAlgo, GcmAesStruct *ctx);

/**
 * @brief Deintialize a cipher context
 *
 * @param pAlgo        [in] Cipher algorithm type
 *
 * @return errno_enum_type
 *
 */

sw_crypto_errno_enum_type SW_Cipher_DeInit(GcmAesStruct *ctx);

/**
 * @brief This function encrypts/decrypts the passed message
 *        using the specified algorithm.
 *
 * @param ioVecIn      [in] Input to cipher
 * @param ioVecOut     [in] Output from cipher
 *
 * @return sw_crypto_errno_enum_type
 *
 */

sw_crypto_errno_enum_type SW_CipherData (               IovecListType    ioVecIn,
                              IovecListType    *ioVecOut, const GcmAesStruct *ctx);



/**
 * @brief This functions sets the Cipher paramaters used by
 *        SW_CRYPTO_CipherData
 *
 * @param nParamID  [in] Cipher parameter id to set
 * @param pParam    [in] Pointer to parameter data
 * @param cParam    [in] Size of parameter data in bytes
 *
 * @return sw_crypto_errno_enum_type
 *
 */
sw_crypto_errno_enum_type SW_Cipher_SetParam (                 SW_CipherParam  nParamID,
                                  const void           *pParam,
                                  uint32               cParam ,
                                  const GcmAesStruct *ctx);

/**
 * @brief This functions gets the Cipher paramaters created by
 *        SW_CRYPTO_CipherData
 *
 * @param nParamID    [in] Cipher parameter id to get
 * @param pParam      [in] Pointer to parameter data
 * @param cParam      [in] Size of parameter data in bytes
 *
 * @return sw_crypto_errno_enum_type
 *
 */
sw_crypto_errno_enum_type SW_Cipher_GetParam (                 SW_CipherParam       nParamID,
                                              void                 *pParam,
                                              uint32               cParam ,
                                              const GcmAesStruct *ctx);

#endif /* AES_SHARED */
