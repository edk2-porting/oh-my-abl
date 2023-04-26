/***************************************************************************
Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.

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

/*===========================================================================
                     INCLUDE FILES FOR MODULE
===========================================================================*/
#include "aes_public.h"
#include "flags.h"
#include "modes.h"  // OpenSSL definitions and GCM APIs
#include "utils.h"
#include <Library/MemoryAllocationLib.h>
#include <Library/DeviceInfo.h>
#include <Library/DrawUI.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PartitionTableUpdate.h>
#include <Library/ShutdownServices.h>

/*===========================================================================
                 DEFINITIONS AND TYPE DECLARATIONS
 ===========================================================================*/

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif


#if defined(__arm__) || defined(__arm) || defined(__aarch64__)
#define UC_LOCAL_API                     __attribute__((visibility("hidden")))
#define UC_EXPORT_API                    __attribute__((visibility("default")))
UC_LOCAL_API int aes_v8_set_encrypt_key(const uint8_t *user_key, const int bits, AES_KEY *key);
UC_LOCAL_API void aes_v8_encrypt(const uint8_t *in, uint8_t *out, const AES_KEY *key);
UC_LOCAL_API void aes_v8_ctr32_encrypt_blocks(const uint8_t *in, uint8_t *out,
                                              size_t blocks, const void *key,
                                              const uint8_t ivec[16]);
#endif

static size_t get_key_sz(SW_Cipher_Alg_Type pAlgo)
{
  switch (pAlgo) {
    case SW_CIPHER_ALG_AES256:
      return SW_AES256_KEY_SIZE;
    case SW_CIPHER_ALG_AES128:
      return SW_AES128_KEY_SIZE;
    default:
      return CIPHER_INVALID_KEY_SIZE;
  }
}

static sw_crypto_errno_enum_type initialize_gcm_ctx(AES_GCM_ARMV8_CTX *c, UINT32 index)
{
  DEBUG ((EFI_D_VERBOSE,"start : %s:%d \n", __func__, __LINE__));

#if defined(__arm__) || defined(__arm) || defined(__aarch64__)
  /* implementation is part of engine/cipher/sw/aes/asm/armv8/aes-64.S */
  aes_v8_set_encrypt_key(c->c_key, 8 * c->c_keysize, &c->key);
  DEBUG ((EFI_D_VERBOSE,"using key size %d \n", c->c_keysize));
  UC_GUARD(NULL != (c->ctx = CRYPTO_gcm128_new(&c->key,               (block128_f) &aes_v8_encrypt, index)),
                                   UC_E_DATA_TOO_LARGE);
  CRYPTO_gcm128_setiv(c->ctx, c->iv, c->iv_sz);
#endif

  DEBUG ((EFI_D_VERBOSE,"end : %s:%d \n", __func__, __LINE__));
  return UC_E_SUCCESS;
}

sw_crypto_errno_enum_type AES_GCM_ARMV8_UpdateAAD(AES_GCM_ARMV8_CTX *c,
                                    const uint8_t *aad,
                                    size_t aad_sz, UINT32 index)
{
  // Check key and iv are both set already
  int ret;
  DEBUG ((EFI_D_VERBOSE,"start : %s:%d \n", __func__, __LINE__));
  UC_GUARD(CIPHER_IS_FLAG_SET(c, UCLIB_CIPHER_KEY_READY), UC_E_CIPHER_INV_KEY);
  UC_GUARD(CIPHER_IS_FLAG_SET(c, UCLIB_CIPHER_IV_READY), UC_E_CIPHER_INV_IV);
  UC_GUARD(!XCM_IS_FLAG_SET(c, XCM_FLAG_CIPHER_IN_PROGRESS), UC_E_CIPHER_INV_AAD_UPDATE);
  
  if (!XCM_IS_FLAG_SET(c, XCM_FLAG_AAD_IN_PROGRESS)) {
   UC_GUARD(UC_E_SUCCESS == (ret = initialize_gcm_ctx(c, index)), ret);
   XCM_SET_FLAG(c, XCM_FLAG_AAD_IN_PROGRESS);
  }

  UC_GUARD(0 == (ret = CRYPTO_gcm128_aad(c->ctx, aad, aad_sz)),
      UC_E_FAILURE);

  DEBUG ((EFI_D_VERBOSE,"end : %s:%d \n", __func__, __LINE__));
  return UC_E_SUCCESS;
}

sw_crypto_errno_enum_type AES_GCM_ARMV8_Cipherdata(AES_GCM_ARMV8_CTX *c,
                                       const uint8_t *ibuf,
                                       size_t isz,
                                       uint8_t *obuf, UINT32 index)
{
  int ret = -1;
  DEBUG ((EFI_D_VERBOSE,"start : %s:%d \n", __func__, __LINE__));

  UC_PASS(XCM_IS_FLAG_SET(c, XCM_FLAG_AAD_IN_PROGRESS) && !isz, UC_E_SUCCESS);

  if (!XCM_IS_FLAG_SET(c, XCM_FLAG_CIPHER_IN_PROGRESS)) {
    if (!XCM_IS_FLAG_SET(c, XCM_FLAG_AAD_IN_PROGRESS)) {
      UC_GUARD(UC_E_SUCCESS == (ret = initialize_gcm_ctx(c, index)),
               ret);
    } else {
      XCM_CLR_FLAG(c, XCM_FLAG_AAD_IN_PROGRESS);
    }
    XCM_SET_FLAG(c, XCM_FLAG_CIPHER_IN_PROGRESS);
  }

  if (CIPHER_IS_FLAG_SET(c, UCLIB_CIPHER_FLAG_DIRECTION)) {
    DEBUG ((EFI_D_VERBOSE,"encrypting %s:%d \n", __func__, __LINE__));
    UC_GUARD(0 == (ret = CRYPTO_gcm128_encrypt(c->ctx, ibuf, obuf, isz)),
             UC_E_FAILURE);
  } else {
    DEBUG ((EFI_D_VERBOSE,"decrypting %s:%d \n", __func__, __LINE__));
    UC_GUARD(0 == (ret = CRYPTO_gcm128_decrypt_ctr32(c->ctx, ibuf, obuf, isz,
             aes_v8_ctr32_encrypt_blocks)),
             UC_E_FAILURE);
  }
  DEBUG ((EFI_D_VERBOSE,"end : %s:%d \n", __func__, __LINE__));
  return UC_E_SUCCESS;
}

/**
 * @brief This function encrypts/decrypts the passed message
 *        using the specified algorithm.
 *
 * @param handle       [in] Pointer to pointer to cipher context
 * @param ioVecIn      [in] Input to cipher
 * @param ioVecOut     [in] Output from cipher
 *
 * @return sw_crypto_errno_enum_type
 *
 * @see
 *
 */
sw_crypto_errno_enum_type SW_CipherData ( IovecListType    ioVecIn,
                                          IovecListType    *ioVecOut,
                                          const GcmAesStruct *ctx)
{
  sw_crypto_errno_enum_type ret_val = UC_E_SUCCESS;
  AES_GCM_ARMV8_CTX* pContext = NULL;
  uint8* 	 pt_ptr_in = NULL;
  uint8* 	 pt_ptr_out = NULL;
  DEBUG ((EFI_D_VERBOSE,"start : %s:%d \n", __func__, __LINE__));

  /* Sanity check inputs */

  if (ctx->gInitDone != true) {
    DEBUG ((EFI_D_ERROR,"%s:%d Init is not Done  \n", __func__, __LINE__));
    return UC_E_INVALID_ARG;
  }

  pContext = (AES_GCM_ARMV8_CTX*) (&ctx->gAesGcmArmV8Ctx);
  if (((ioVecOut->size!= 1)  || (!ioVecOut->iov))
   ||((ioVecIn.size!= 1) || (!ioVecIn.iov)))
  {
    return UC_E_INVALID_ARG;
  }

  if((ioVecIn.iov->dwLen % 16 != 0) &&
  (pContext->mode != SW_CIPHER_MODE_GCM))
  {
    return UC_E_INVALID_ARG;
  }

  pt_ptr_in = (uint8 *) ioVecIn.iov->pvBase;
  pt_ptr_out = (uint8 *) ioVecOut->iov->pvBase;

  ret_val =    AES_GCM_ARMV8_Cipherdata(pContext,
                                                 pt_ptr_in,
                                     ioVecIn.iov->dwLen,
                                              pt_ptr_out, ctx->InstanceId);
  DEBUG ((EFI_D_VERBOSE,"end : %s:%d \n", __func__, __LINE__));
  return ret_val;
}

/**
 * @brief This functions gets the Cipher paramaters generated by
 *        SW_CRYPTO_CipherData
 *
 * @param nParamID    [in] Cipher parameter id to get
 * @param pParam      [in] Pointer to parameter data
 * @param cParam      [in] Size of parameter data in bytes
 *
 * @return sw_crypto_errno_enum_type
 *
 * @see SW_CRYPTO_CipherData
 *
 */

sw_crypto_errno_enum_type SW_Cipher_GetParam ( SW_CipherParam       nParamID,
                                               void                 *pParam,
                                               uint32               cParam,
                                               const GcmAesStruct *ctx )
{
  sw_crypto_errno_enum_type ret_val = UC_E_SUCCESS;
  AES_GCM_ARMV8_CTX* pContext = NULL;

   DEBUG ((EFI_D_VERBOSE,"start : %s:%d \n", __func__, __LINE__));
  /* Sanity check inputs */

  if (ctx->gInitDone != true) {
     DEBUG ((EFI_D_ERROR,"%s:%d param ID:0x%x Init is not Done  \n", __func__, __LINE__, nParamID));
    return UC_E_INVALID_ARG;
  }

  if ( !pParam ) {
    DEBUG ((EFI_D_ERROR,"%s:%d param ID:0x%x Invalid Param  Passed \n", __func__, __LINE__, nParamID));
    return UC_E_INVALID_ARG;
  }

  pContext = (AES_GCM_ARMV8_CTX*) (&ctx->gAesGcmArmV8Ctx);
  switch ( nParamID )
  {
  case SW_CIPHER_PARAM_TAG:
      if((cParam > sizeof(pContext->itag)) ||
       pParam == NULL) {
         return UC_E_INVALID_ARG;
      }
      DEBUG ((EFI_D_VERBOSE,"start : %s:%d calculating tag \n", __func__, __LINE__));
      CRYPTO_gcm128_tag(pContext->ctx, pContext->itag, sizeof(pContext->itag));
      ENV_sec_memcpy(pParam, cParam, (void*)pContext->itag, CIPHER_AES_BLK_SIZE);
      break;
  default:
     return UC_E_INVALID_ARG;
  }
  DEBUG ((EFI_D_VERBOSE,"end : %s:%d \n", __func__, __LINE__));
  return ret_val;
}

/**
 * @brief This functions sets the Cipher paramaters used by
 *        SW_CRYPTO_CipherData
 *
 * @param handle       [in] Pointer to pointer to cipher context
 * @param nParamID  [in] Cipher parameter id to set
 * @param pParam      [in] Pointer to parameter data
 * @param cParam      [in] Size of parameter data in bytes
 *
 * @return sw_crypto_errno_enum_type
 *
 * @see SW_CRYPTO_CipherData
 *
 */
sw_crypto_errno_enum_type SW_Cipher_SetParam ( SW_CipherParam  nParamID,
                                               const void           *pParam,
                                               uint32               cParam,
                                               const GcmAesStruct *ctx)
{
  sw_crypto_errno_enum_type ret_val = UC_E_SUCCESS;
  AES_GCM_ARMV8_CTX* pContext = NULL;
  DEBUG ((EFI_D_VERBOSE,"%s: param ID:0x%x and cParam %d\n", __func__, nParamID, cParam));

  /* Sanity check inputs */
  if (ctx->gInitDone != true) {
     DEBUG ((EFI_D_ERROR,"%s:%d param ID:0x%x Init is not Done  \n", __func__, __LINE__, nParamID));
    return UC_E_INVALID_ARG;
  }

  if (!pParam && nParamID != SW_CIPHER_PARAM_AAD) {
     DEBUG ((EFI_D_ERROR,"%s:%d param ID:0x%x UC_E_INVALID_ARG \n", __func__, __LINE__, nParamID));
    return UC_E_INVALID_ARG;
  }

  pContext = (AES_GCM_ARMV8_CTX*) (&ctx->gAesGcmArmV8Ctx);

  switch ( nParamID )
  {
     case SW_CIPHER_PARAM_DIRECTION:
       pContext->dir = (SW_CipherEncryptDir)*((int *)pParam);
       /*set only for encrypt*/
       if (SW_CIPHER_ENCRYPT == pContext->dir)
         CIPHER_FLAG_SET(pContext, UCLIB_CIPHER_FLAG_DIRECTION);
       break;

    case SW_CIPHER_PARAM_KEY:
      if ( cParam > sizeof(pContext->c_key)) {
        DEBUG ((EFI_D_ERROR,"%s:%d param ID:0x%x UC_E_INVALID_ARG \n", __func__, __LINE__, nParamID));
        return UC_E_INVALID_ARG;
      }
      if(((SW_AES128_KEY_SIZE == cParam) || (SW_AES192_KEY_SIZE == cParam) || (SW_AES256_KEY_SIZE == cParam)) && (pParam != 0))
      {
        if (pContext->c_keysize != cParam) {
          DEBUG ((EFI_D_ERROR,"%s:%d param ID:0x%x UC_E_INVALID_ARG \n", __func__, __LINE__, nParamID));
          return UC_E_INVALID_ARG;
        }
        if(pContext->mode == SW_CIPHER_MODE_GCM)
        {
          ENV_sec_memcpy(pContext->c_key, CIPHER_MAX_KEY_SIZE, (void*) pParam,                  cParam);
        }
        else
        {
          DEBUG ((EFI_D_ERROR,"%s:%d param ID:0x%x UC_E_INVALID_ARG \n", __func__, __LINE__, nParamID));
          return UC_E_INVALID_ARG;
        }
      }
      else
      {
          DEBUG ((EFI_D_ERROR,"%s:%d param ID:0x%x UC_E_INVALID_ARG \n", __func__, __LINE__, nParamID));
          return UC_E_INVALID_ARG;
      }
      CIPHER_FLAG_SET(pContext, UCLIB_CIPHER_KEY_READY);
      break;

    case SW_CIPHER_PARAM_IV:
        if (SW_AES_IV_SIZE != cParam && SW_CIPHER_MODE_GCM != pContext->mode )
        {
          DEBUG ((EFI_D_ERROR,"%s:%d param ID:0x%x UC_E_INVALID_ARG \n", __func__, __LINE__, nParamID));
          return UC_E_INVALID_ARG;
        }
        ENV_sec_memcpy( pContext->iv, CIPHER_MAX_IV_SIZE, (void *)pParam, cParam );
        pContext->iv_sz = cParam;
        CIPHER_FLAG_SET(pContext, UCLIB_CIPHER_IV_READY);
        break;

    case SW_CIPHER_PARAM_MODE:
      switch ( *(SW_CipherModeType*)pParam )
      {
        case SW_CIPHER_MODE_GCM:
          pContext->mode = *(SW_CipherModeType*)pParam;
          break;
        default:
          ret_val =    UC_E_NOT_SUPPORTED;
          break;
      }
      break;

    case SW_CIPHER_PARAM_AAD:
      if(pParam == NULL && cParam != 0)
      {
         DEBUG ((EFI_D_ERROR,"%s:%d param ID:0x%x UC_E_INVALID_ARG \n", __func__, __LINE__, nParamID));
         return UC_E_INVALID_ARG;
      }
      ret_val = AES_GCM_ARMV8_UpdateAAD(pContext, (uint8*)pParam, cParam, ctx->InstanceId);
      break;

    case SW_CIPHER_PARAM_TAG:
       if((cParam > sizeof(pContext->itag)) || pParam == NULL) {
         ret_val = UC_E_INVALID_ARG;
       }
       ENV_sec_memcpy((void*)&pContext->itag, CIPHER_AES_BLK_SIZE, (void*)pParam, cParam);
       pContext->tag_sz = cParam;
       break;

     default:
       DEBUG ((EFI_D_ERROR,"%s:%d param ID:0x%x UC_E_INVALID_ARG \n", __func__, __LINE__, nParamID));
       return UC_E_INVALID_ARG;
  }

  DEBUG ((EFI_D_VERBOSE,"%s: param ID:0x%x ret_val %d\n", __func__, nParamID, ret_val));
  DEBUG ((EFI_D_VERBOSE,"end : %s:%d \n", __func__, __LINE__));
  return ret_val;
}

/**
 * @brief Deintialize a cipher context
 *
 * @param pAlgo         [in] Cipher algorithm type
 *
 * @return errno_enum_type
 *
 * @see
 *
 */

sw_crypto_errno_enum_type SW_Cipher_DeInit(GcmAesStruct *ctx)
{
  DEBUG ((EFI_D_VERBOSE,"start : %s:%d \n", __func__, __LINE__));
  ENV_sec_memzero(&ctx->gAesGcmArmV8Ctx, sizeof(AES_GCM_ARMV8_CTX));
  ctx->gInitDone = false;
  DEBUG ((EFI_D_VERBOSE,"end : %s:%d \n", __func__, __LINE__));
  return UC_E_SUCCESS;
}

/**
 * @brief Intialize a cipher context
 *
 * @param pAlgo        [in] Cipher algorithm type
 *
 * @return sw_crypto_errno_enum_type
 *
 * @see
 *
 */
sw_crypto_errno_enum_type SW_Cipher_Init(SW_Cipher_Alg_Type pAlgo, GcmAesStruct *ctx)
{
  sw_crypto_errno_enum_type ret_val = UC_E_SUCCESS;

  DEBUG ((EFI_D_VERBOSE,"start : %s:%d \n", __func__, __LINE__));

  /* Sanity check inputs, only AES 128, AES 192 and AES 256 is supported currently */
  if (SW_CIPHER_ALG_AES128 != pAlgo && SW_CIPHER_ALG_AES256 != pAlgo)
  {
     return UC_E_NOT_SUPPORTED;
  }

  ENV_sec_memzero(&ctx->gAesGcmArmV8Ctx, sizeof(AES_GCM_ARMV8_CTX));
  ctx->gAesGcmArmV8Ctx.c_keysize = get_key_sz(pAlgo);
  ctx->gAesGcmArmV8Ctx.algo      = (SW_Cipher_Alg_Type) pAlgo;
  ctx->gInitDone                 = true;
  DEBUG ((EFI_D_VERBOSE,"end : %s:%d \n", __func__, __LINE__));
  return ret_val;
}
