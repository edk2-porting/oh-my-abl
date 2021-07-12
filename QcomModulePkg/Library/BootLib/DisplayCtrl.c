/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

#include "DisplayCtrl.h"
#include "libfdt.h"
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtRw.h>

#define DISPLAY_PLL_CODES_MAX_SIZE              2048
#define DISPLAY_DSI_PLL_CODES_SERVICE_VARIABLE  L"DisplayDSIPLLCodesInfo"
#define DISPLAY_DSI_PLL_CODES_DT_NODE           "/soc/dsi_pll_codes"

STATIC EFI_STATUS
UpdatePLLCodesInfoInternel (VOID *fdt, CHAR16 *service_variable, CHAR8 *dt_node)
{
  EFI_STATUS                  Status           = EFI_SUCCESS;
  UINTN                       PllCodesInfoSize = 0;
  UINTN                       SizeTmp;
  UINT32                     *PllCodesInfo     = NULL;
  CONST  struct fdt_property *Prop             = NULL;
  UINT32                     *tmp              = NULL;
  INT32                       PropLen;
  INT32                       ret;
  UINT32                      offset;
  UINT32                      i;

  if ((NULL == fdt) ||
      (NULL == service_variable) ||
      (NULL == dt_node)) {
    DEBUG ((EFI_D_WARN, "Invalid parameter\n"));
    Status = EFI_INVALID_PARAMETER;
    goto error;
  }

  Status = gRT->GetVariable (service_variable,
                             &gQcomTokenSpaceGuid,
                             NULL,
                             &PllCodesInfoSize,
                             NULL);

  if (EFI_ERROR (Status) &&
     (Status != EFI_BUFFER_TOO_SMALL)) {
    DEBUG ((EFI_D_WARN, "Variable not exist, skip update pll codes\n"));
    Status = EFI_SUCCESS;
    goto error;
  }

  if ((0                              == PllCodesInfoSize) ||
      (DISPLAY_PLL_CODES_MAX_SIZE     <  PllCodesInfoSize)) {
    DEBUG ((EFI_D_WARN, "Invalid Pll codes size:%d\n", PllCodesInfoSize));
    Status = EFI_OUT_OF_RESOURCES;
    goto error;
  }

  ret = FdtPathOffset (fdt, dt_node);
  if (ret < 0) {
    /* Just return success if display node not exists */
    DEBUG ((EFI_D_WARN, "Cannot find dt node:%a\n", dt_node));
    Status = EFI_NOT_FOUND;
    goto error;
  }

  offset = (UINT32)ret;
  Prop = fdt_get_property (fdt, offset, "reg", &PropLen);

  if (!Prop ||
      !Prop->data) {
    DEBUG ((EFI_D_WARN, "Invalid pll codes data property\n"));
    Status = EFI_NOT_FOUND;
    goto error;
  }

  /* Round up to align with 4 bytes */
  PllCodesInfoSize = ((PllCodesInfoSize + 3) & ~3);

  if (PropLen < PllCodesInfoSize) {
    DEBUG ((EFI_D_WARN, "Not enough space in DT node\n"));
    Status = EFI_INVALID_PARAMETER;
    goto error;
  }

  PllCodesInfo = (UINT32*) AllocateZeroPool (PllCodesInfoSize);

  if (PllCodesInfo == NULL) {
    DEBUG ((EFI_D_WARN, "Fail to alloc memory for pll codes\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto error;
  }

  SizeTmp = PllCodesInfoSize;

  Status = gRT->GetVariable (DISPLAY_DSI_PLL_CODES_SERVICE_VARIABLE,
                             &gQcomTokenSpaceGuid,
                             NULL,
                             &SizeTmp,
                             PllCodesInfo);
  if (EFI_ERROR (Status) ||
     (0 == SizeTmp)) {
    DEBUG ((EFI_D_WARN, "Fail get pll codes data from service variable\n"));
    goto error;
  }

  tmp = (UINT32 *)Prop->data;

  for (i = 0; i < PllCodesInfoSize / sizeof (UINT32); i++)
  {
    *tmp = cpu_to_fdt32 (PllCodesInfo[i]);
    tmp++;
  }

  ret = fdt_setprop_inplace (fdt,
                             offset,
                             "reg",
                             Prop->data,
                             PropLen);
  if (ret < 0) {
    DEBUG ((EFI_D_WARN, "Fail to update pll codes data to DT node\n"));
    Status = EFI_NO_MAPPING;
  }

error:
  if (NULL != PllCodesInfo) {
    FreePool (PllCodesInfo);
  }

  return Status;
}

EFI_STATUS
UpdatePLLCodesInfo (VOID *fdt)
{
  EFI_STATUS Status  = EFI_SUCCESS;

  if (NULL == fdt) {
    Status = EFI_INVALID_PARAMETER;
    goto error;
  }

  Status = UpdatePLLCodesInfoInternel (fdt,
                                       DISPLAY_DSI_PLL_CODES_SERVICE_VARIABLE,
                                       DISPLAY_DSI_PLL_CODES_DT_NODE);

error:
  return Status;
}
