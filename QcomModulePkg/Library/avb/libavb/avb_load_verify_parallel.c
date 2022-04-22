/*
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

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
 */


#include "avb_slot_verify.h"
#include "avb_chain_partition_descriptor.h"
#include "avb_footer.h"
#include "avb_hash_descriptor.h"
#include "avb_kernel_cmdline_descriptor.h"
#include "avb_sha.h"
#include "avb_util.h"
#include "avb_vbmeta_image.h"
#include "avb_version.h"
#include "BootStats.h"
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#define IMAGE_SPLIT_SIZE 2

typedef struct {
  bool IsFinal;
  bool Sha256HashCheck;
  UINT32 ThreadId;
  AvbSlotVerifyResult Status;
  AvbOps* ops;
  CONST uint8_t* DescDigest;
  uint8_t* image_buf;
  char* part_name;
  uint32_t DescDigestLen;
  uint64_t ImageOffset;
  uint64_t SplitImageSize;
  uint64_t RemainImageSize;
  VOID* HashCtx;
} LoadVerifyInfo;

static AvbSlotVerifyResult VerifyPartitionSha256 (
    AvbSHA256Ctx* Sha256Ctx,
    char* part_name,
    CONST uint8_t* DescDigest,
    uint32_t DescDigestLen,
    uint8_t* image_buf,
    uint64_t ImageSize,
    bool IsFinal) {
  uint8_t *digest = NULL ;
  size_t  digest_len = 0;

  AvbSlotVerifyResult Ret = AVB_SLOT_VERIFY_RESULT_OK;

  avb_sha256_update (Sha256Ctx, image_buf, ImageSize);
  if (IsFinal == true) {
    digest = avb_sha256_final (Sha256Ctx);
    digest_len = AVB_SHA256_DIGEST_SIZE;
    if (digest_len != DescDigestLen) {
      avb_errorv (
          part_name, ": Digest in descriptor not of expected size.\n", NULL);
      Ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
      goto out;
    }

    if (avb_safe_memcmp (digest, DescDigest, digest_len) != 0) {
      avb_errorv (part_name,
                 ": Hash of data does not match digest in descriptor.\n",
                 NULL);
      Ret = AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION;
      goto out;
    } else {
      avb_debugv (part_name, ": success: Image verification completed\n", NULL);
      goto out;
    }
  } else {
      avb_debugv (part_name, ": success: Image verification in parts\n", NULL);
      Ret = AVB_SLOT_VERIFY_RESULT_OK;
      goto out;
  }

out:
  return Ret;
}

static AvbSlotVerifyResult VerifyPartitionSha512 (
    AvbSHA512Ctx* Sha512Ctx,
    char* part_name,
    CONST uint8_t* DescDigest,
    uint32_t DescDigestLen,
    uint8_t* image_buf,
    uint64_t ImageSize,
    bool IsFinal) {
  uint8_t *digest = NULL ;
  size_t  digest_len = 0;
  AvbSlotVerifyResult Ret = AVB_SLOT_VERIFY_RESULT_OK ;

  avb_sha512_update (Sha512Ctx, image_buf, ImageSize);
  if (IsFinal == true) {
    digest = avb_sha512_final (Sha512Ctx);
    digest_len = AVB_SHA512_DIGEST_SIZE;
    if (digest_len != DescDigestLen) {
      avb_errorv (
          part_name, ": Digest in descriptor not of expected size.\n", NULL);
      Ret = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
      goto out;
    }
    if (avb_safe_memcmp (digest, DescDigest, digest_len) != 0) {
      avb_errorv (part_name,
                 ": Hash of data does not match digest in descriptor.\n",
                 NULL);
      Ret = AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION;
      goto out;
    } else {
      avb_debugv (part_name, ": success: Image verification completed\n", NULL);
      goto out;
    }
  } else {
      avb_debugv (part_name, ": success: Image verification in parts\n", NULL);
      Ret = AVB_SLOT_VERIFY_RESULT_OK;
      goto out;
  }

out:
  return Ret;
}

static AvbSlotVerifyResult Load_partition_to_verify (
    AvbOps* ops,
    char* part_name,
    int64_t Offset,
    uint8_t* image_buf,
    uint64_t ImageSize) {
  AvbIOResult IoRet;
  AvbSlotVerifyResult Ret = AVB_SLOT_VERIFY_RESULT_OK;
  size_t PartNumRead = 0;

  IoRet = ops->read_from_partition (
      ops, part_name, Offset, ImageSize, (image_buf + Offset), &PartNumRead);
  if (IoRet == AVB_IO_RESULT_ERROR_OOM) {
    Ret = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
    goto out;
  } else if (IoRet != AVB_IO_RESULT_OK) {
    avb_errorv (part_name, ": Error loading data from partition.\n", NULL);
    Ret = AVB_SLOT_VERIFY_RESULT_ERROR_IO;
    goto out;
  }
  if (PartNumRead != ImageSize) {
    avb_errorv (part_name, ": Read fewer than requested bytes.\n", NULL);
    Ret = AVB_SLOT_VERIFY_RESULT_ERROR_IO;
    goto out;
  }

out:
  return Ret;
}


INT32 BootPartitionLoad (LoadVerifyInfo *ThreadBootLoad)
{
  AvbSlotVerifyResult Status;
  uint64_t ImageOffset;
  uint64_t SplitImageSize;

  if ((NULL ==  ThreadBootLoad->ops) ||
      (NULL == ThreadBootLoad->DescDigest) ||
      (NULL ==  ThreadBootLoad->image_buf) ||
      (NULL == ThreadBootLoad->part_name) ||
      (NULL == ThreadBootLoad->HashCtx)) {
    Status = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_ARGUMENT;
    ThreadBootLoad->Status = Status;
    return Status;
  }

  ImageOffset = ThreadBootLoad->ImageOffset;
  SplitImageSize = ThreadBootLoad->SplitImageSize;

  Status = Load_partition_to_verify (ThreadBootLoad->ops,
        ThreadBootLoad->part_name,
        ImageOffset,
        ThreadBootLoad->image_buf,
        SplitImageSize );

  if (Status != AVB_SLOT_VERIFY_RESULT_OK) {
    return Status;
  }

  SplitImageSize = SplitImageSize + ThreadBootLoad->RemainImageSize;
  ImageOffset = ImageOffset + SplitImageSize;

  Status = Load_partition_to_verify (ThreadBootLoad->ops,
          ThreadBootLoad->part_name,
          ImageOffset,
          ThreadBootLoad->image_buf,
          SplitImageSize );

  ThreadBootLoad->Status = Status;

  return Status;
}

INT32 BootPartitionVerify (VOID* Arg)
{
  AvbSlotVerifyResult Status;
  uint64_t ImageOffset;
  AvbSHA256Ctx *Sha256Ctx;
  AvbSHA512Ctx* Sha512Ctx;

  LoadVerifyInfo* ThreadBootVerify = (LoadVerifyInfo*) Arg;
  if ((NULL ==  ThreadBootVerify->ops) ||
      (NULL == ThreadBootVerify->DescDigest) ||
      (NULL ==  ThreadBootVerify->image_buf) ||
      (NULL == ThreadBootVerify->part_name) ||
      (NULL == ThreadBootVerify->HashCtx)) {
    Status = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_ARGUMENT;
    ThreadBootVerify->Status = Status;
  }

  if (ThreadBootVerify->Sha256HashCheck == true) {
    Sha256Ctx = (AvbSHA256Ctx*) ThreadBootVerify->HashCtx;
    Sha512Ctx = NULL;
  } else {
    Sha256Ctx = NULL;
    Sha512Ctx = (AvbSHA512Ctx*) ThreadBootVerify->HashCtx;
  }

  ImageOffset = ThreadBootVerify->ImageOffset;

  if (ThreadBootVerify->Sha256HashCheck == true) {
    Status = VerifyPartitionSha256 (Sha256Ctx,
                                  ThreadBootVerify->part_name,
                                  ThreadBootVerify->DescDigest,
                                  ThreadBootVerify->DescDigestLen,
                                  ThreadBootVerify->image_buf,
                                  ThreadBootVerify->SplitImageSize,
                                  ThreadBootVerify->IsFinal);
  } else {
   Status = VerifyPartitionSha512 (Sha512Ctx,
                                  ThreadBootVerify->part_name,
                                  ThreadBootVerify->DescDigest,
                                  ThreadBootVerify->DescDigestLen,
                                  ThreadBootVerify->image_buf,
                                  ThreadBootVerify->SplitImageSize,
                                  ThreadBootVerify->IsFinal);
  }
  DEBUG ((EFI_D_INFO, "BootPartitionVerify-First Return: %d\n", Status));

  ThreadBootVerify->IsFinal = true;
  ThreadBootVerify->SplitImageSize = ThreadBootVerify->SplitImageSize
          + ThreadBootVerify->RemainImageSize;
  ImageOffset = ImageOffset + ThreadBootVerify->SplitImageSize;

  if (Status != AVB_SLOT_VERIFY_RESULT_OK) {
    ThreadBootVerify->Status = Status;
    return Status;
  }

  if (ThreadBootVerify->Sha256HashCheck == true) {
   Status = VerifyPartitionSha256 (Sha256Ctx,
                                  ThreadBootVerify->part_name,
                                  ThreadBootVerify->DescDigest,
                                  ThreadBootVerify->DescDigestLen,
                                  ThreadBootVerify->image_buf + ImageOffset,
                                  ThreadBootVerify->SplitImageSize,
                                  ThreadBootVerify->IsFinal);
  } else {
    Status = VerifyPartitionSha512 (Sha512Ctx,
                                  ThreadBootVerify->part_name,
                                  ThreadBootVerify->DescDigest,
                                  ThreadBootVerify->DescDigestLen,
                                  ThreadBootVerify->image_buf + ImageOffset,
                                  ThreadBootVerify->SplitImageSize,
                                  ThreadBootVerify->IsFinal);
  }
  ThreadBootVerify->Status = Status;
  DEBUG ((EFI_D_INFO, "BootPartitionVerify-Second Return: %d\n", Status));

  return Status;
}

AvbSlotVerifyResult LoadAndVerifyBootHashPartition (
    AvbOps* ops,
    AvbHashDescriptor HashDesc,
    char* part_name,
    CONST uint8_t* DescDigest,
    CONST uint8_t* DescSalt,
    uint8_t* image_buf,
    uint64_t ImageSize) {
  AvbSHA256Ctx Sha256Ctx;
  AvbSHA512Ctx Sha512Ctx;
  AvbSlotVerifyResult Status;
  uint64_t ImagePartLoop = 0;
  uint64_t ImageOffset = 0;
  uint64_t SplitImageSize = 0;
  uint64_t RemainImageSize = 0;
  bool Sha256Hash = false;
  EFI_STATUS TStatus = EFI_SUCCESS;

  if (image_buf == NULL) {
    Status = AVB_SLOT_VERIFY_RESULT_ERROR_OOM;
    goto out;
  }

  if (avb_strncmp ( (CONST CHAR8*)HashDesc.hash_algorithm, "sha256",
                 avb_strlen ("sha256")) == 0) {
    Sha256Hash  = true;
    avb_sha256_init (&Sha256Ctx);
    avb_sha256_update (&Sha256Ctx, DescSalt, HashDesc.salt_len);
  } else if (avb_strncmp ( (CONST CHAR8*)HashDesc.hash_algorithm, "sha512",
                  avb_strlen ("sha512")) == 0) {
    avb_sha512_init (&Sha512Ctx);
    avb_sha512_update (&Sha512Ctx, DescSalt, HashDesc.salt_len);
  } else {
    avb_errorv (part_name, ": Unsupported hash algorithm.\n", NULL);
    Status = AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA;
    goto out;
  }

  SplitImageSize = ImageSize / IMAGE_SPLIT_SIZE;
  RemainImageSize = ImageSize % IMAGE_SPLIT_SIZE;
  ImageOffset = 0;
  LoadVerifyInfo* ThreadLoadInfo = AllocateZeroPool (sizeof (LoadVerifyInfo));
  LoadVerifyInfo* ThreadVerifyInfo = AllocateZeroPool (sizeof (LoadVerifyInfo));

  ThreadLoadInfo->part_name = part_name;
  ThreadLoadInfo->ops = ops;
  ThreadLoadInfo->image_buf = image_buf;
  ThreadLoadInfo->DescDigest = DescDigest;
  ThreadLoadInfo->DescDigestLen = HashDesc.digest_len;
  ThreadLoadInfo->ThreadId = ImagePartLoop;
  ThreadLoadInfo->ImageOffset = ImageOffset;
  ThreadLoadInfo->SplitImageSize = SplitImageSize;
  ThreadLoadInfo->IsFinal = false;
  ThreadLoadInfo->RemainImageSize = RemainImageSize;


  if (Sha256Hash == true) {
     ThreadLoadInfo->HashCtx = &Sha256Ctx;
  } else {
     ThreadLoadInfo->HashCtx = &Sha512Ctx;
  }
  ThreadLoadInfo->Sha256HashCheck = Sha256Hash;

  ThreadVerifyInfo->part_name = part_name;
  ThreadVerifyInfo->ops = ops;
  ThreadVerifyInfo->image_buf = image_buf;
  ThreadVerifyInfo->DescDigest = DescDigest;
  ThreadVerifyInfo->DescDigestLen = HashDesc.digest_len;
  ThreadVerifyInfo->ThreadId = 1;
  ThreadVerifyInfo->ImageOffset = ImageOffset;
  ThreadVerifyInfo->SplitImageSize = SplitImageSize;
  ThreadVerifyInfo->IsFinal = false;
  ThreadVerifyInfo->RemainImageSize = RemainImageSize;
  if (Sha256Hash == true) {
     ThreadVerifyInfo->HashCtx = &Sha256Ctx;
  } else {
     ThreadVerifyInfo->HashCtx = &Sha512Ctx;
  }
  ThreadVerifyInfo->Sha256HashCheck = Sha256Hash;

  TStatus = BootPartitionLoad (ThreadLoadInfo);
  if (TStatus) {
      Status = TStatus;
      DEBUG ((EFI_D_INFO, "avb_load_verify_parallel: load Error\n"));
      goto out;
  }

  TStatus = BootPartitionVerify (ThreadVerifyInfo);
  if (TStatus) {
    Status = TStatus;
    DEBUG ((EFI_D_INFO,
        "avb_load_verify_parallel: verify Error\n"));
    goto out;
  }

 /*Wait for threads to complete*/
  if (ThreadLoadInfo->Status != AVB_SLOT_VERIFY_RESULT_OK) {
    Status = ThreadLoadInfo->Status;
    goto out;
  }

  if (ThreadVerifyInfo->Status != AVB_SLOT_VERIFY_RESULT_OK) {
    Status = ThreadVerifyInfo->Status;
    goto out;
  }

  Status = AVB_SLOT_VERIFY_RESULT_OK;
  /*Free and assign NUL to ThreadLoadVerifyInfo_1*/
  gBS->FreePool (ThreadLoadInfo);
  gBS->FreePool (ThreadVerifyInfo);

out:
  return Status;
}
