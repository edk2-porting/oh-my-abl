/* Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
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
 *
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
#if HIBERNATION_SUPPORT_NO_AES

#include <Library/DeviceInfo.h>
#include <Library/DrawUI.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PartitionTableUpdate.h>
#include <Library/ShutdownServices.h>
#include <Library/StackCanary.h>
#include "Hibernation.h"
#include "BootStats.h"
#include <Library/DxeServicesTableLib.h>
#include <VerifiedBoot.h>
#if HIBERNATION_SUPPORT_AES
#include <Library/aes/aes_public.h>
#include <Protocol/EFIQseecom.h>
#endif
#include "KeymasterClient.h"

#define BUG(Fmt, ...) {\
                printf ("Fatal error " Fmt, ##__VA_ARGS__); \
                while (1); \
       }

#define ALIGN_1GB(address) address &= ~((1 << 30) - 1)
#define ALIGN_2MB(address) address &= ~((1 << 21) - 1)

/* Reserved some free memory for UEFI use */
#define RESERVE_FREE_SIZE 1024 * 1024 * 10
typedef struct FreeRanges {
        UINT64 Start;
        UINT64 End;
}FreeRanges;

#if HIBERNATION_SUPPORT_AES
static struct DecryptParam Dp;
static CHAR8 *Authtags;

#define QSEECOM_ALIGN_SIZE      0x40
#define QSEECOM_ALIGN_MASK      (QSEECOM_ALIGN_SIZE - 1)
#define QSEECOM_ALIGN(x)        \
        ((x + QSEECOM_ALIGN_MASK) & (~QSEECOM_ALIGN_MASK))
#endif

/* Holds free memory ranges read from UEFI memory map */
static struct FreeRanges FreeRangeBuf[100];
static INT32 FreeRangeCount;

typedef struct MappedRange {
        UINT64 Start, End;
        struct MappedRange * Next;
}MappedRange;

/* number of data pages to be copied from swap */
static UINT32 NrCopyPages;
/* number of meta pages or pages which hold pfn indexes */
static UINT32 NrMetaPages;
/* number of image kernel pages bounced due to conflict with UEFI */
static UINT64 BouncedPages;

static struct ArchHibernateHdr *ResumeHdr;

typedef struct PfnBlock {
        UINT64 BasePfn;
        INT32 AvailablePfns;
}PfnBlock;

typedef struct KernelPfnIterator {
        UINT64 *PfnArray;
        INT32 CurIndex;
        INT32 MaxIndex;
        struct PfnBlock CurBlock;
}KernelPfnIterator;
static struct KernelPfnIterator KernelPfnIteratorObj;

static struct SwsUspHeader *SwsuspHeader;
/*
 * Bounce Pages - During the copy of pages from snapshot image to
 * RAM, certain pages can conflicts with concurrently running UEFI/ABL
 * pages. These pages are copied temporarily to bounce pages. And
 * during later stage, upon exit from UEFI boot services, these
 * bounced pages are copied to their real destination. BouncePfnEntry
 * is used to store the location of temporary bounce pages and their
 * real destination.
 */
typedef struct BouncePfnEntry {
        UINT64 DstPfn;
        UINT64 SrcPfn;
}BouncePfnEntry;

/*
 * Size of the buffer where disk IO is performed.If any kernel pages are
 * destined to be here, they will be bounced.
 */
#define DISK_BUFFER_SIZE 64 * 1024 * 1024
#define DISK_BUFFER_PAGES (DISK_BUFFER_SIZE / PAGE_SIZE)

#define OUT_OF_MEMORY -1

#define BOUNCE_TABLE_ENTRY_SIZE sizeof(struct BouncePfnEntry)
#define ENTRIES_PER_TABLE (PAGE_SIZE / BOUNCE_TABLE_ENTRY_SIZE) - 1

/*
 * Bounce Tables -  bounced pfn entries are stored in bounced tables.
 * Bounce tables are discontinuous pages linked by the last element
 * of the page. Bounced table are allocated using unused pfn allocator.
 *
 *       ---------                    ---------
 * 0   | dst | src |    ----->  0   | dst | src |
 * 1   | dst | src |    |       1   | dst | src |
 * 2   | dst | src |    |       2   | dst | src |
 * .   |           |    |       .   |           |
 * .   |           |    |       .   |           |
 * 256 | addr|     |-----       256 |addr |     |------>
 *       ---------                    ---------
 */
typedef struct BounceTable {
        struct BouncePfnEntry BounceEntry[ENTRIES_PER_TABLE];
        UINT64 NextBounceTable;
        UINT64 Padding;
}BounceTable;

typedef struct BounceTableIterator {
        struct BounceTable *FirstTable;
        struct BounceTable *CurTable;
        /* next available free table entry */
        INT32 CurIndex;
}BounceTableIterator;

#if HIBERNATION_SUPPORT_AES
typedef struct Secs2dTaHandle {
        QCOM_QSEECOM_PROTOCOL *QseeComProtocol;
        UINT32 AppId;
}Secs2dTaHandle;
#endif

struct BounceTableIterator TableIterator;

UINT64 RelocateAddress;

#define PFN_INDEXES_PER_PAGE 512
/* Final entry is used to link swap_map pages together */
#define ENTRIES_PER_SWAPMAP_PAGE (PFN_INDEXES_PER_PAGE - 1)

#define SWAP_INFO_OFFSET        (SwsuspHeader->Image + 1)
#define FIRST_PFN_INDEX_OFFSET  (SWAP_INFO_OFFSET + 1)

#define SWAP_PARTITION_NAME     L"swap_a"

/*
 * target_addr  : address where page allocation is needed
 *
 * return       : 1 if address falls in free range
 *                0 if address is not in free range
 */
static INT32 CheckFreeRanges (UINT64 TargetAddr)
{
        INT32 Iter = 0;
        while (Iter < FreeRangeCount) {
                if (TargetAddr >= FreeRangeBuf[Iter].Start &&
                        TargetAddr < FreeRangeBuf[Iter].End)
                        return 1;
                Iter++;
        }
        return 0;
}

static INT32 MemCmp (CONST VOID *S1, CONST VOID *S2, INT32 MemSize)
{
        CONST UINT8 *Us1 = S1;
        CONST UINT8 *Us2 = S2;

        if (MemSize == 0 ) {
                return 0;
        }

        while (MemSize--) {
                if (*Us1++ ^ *Us2++) {
                        return 1;
                }
        }
        return 0;
}

static VOID CopyPage (UINT64 SrcPfn, UINT64 DstPfn)
{
        UINT64 *Src = (UINT64*)(SrcPfn << PAGE_SHIFT);
        UINT64 *Dst = (UINT64*)(DstPfn << PAGE_SHIFT);

        gBS->CopyMem (Dst, Src, PAGE_SIZE);
}

static VOID InitKernelPfnIterator (UINT64 *Array)
{
        struct KernelPfnIterator *Iter = &KernelPfnIteratorObj;
        Iter->PfnArray = Array;
        Iter->MaxIndex = NrCopyPages;
}

static INT32 FindNextAvailableBlock (struct KernelPfnIterator *Iter)
{
        INT32 AvailablePfns;

        do {
                UINT64 CurPfn, NextPfn;
                Iter->CurIndex++;
                if (Iter->CurIndex >= Iter->MaxIndex) {
                        BUG ("index maxed out. Line %d\n", __LINE__);
                }
                CurPfn = Iter->PfnArray[Iter->CurIndex];
                NextPfn = Iter->PfnArray[Iter->CurIndex + 1];
                AvailablePfns = NextPfn - CurPfn - 1;
        } while (!AvailablePfns);

        Iter->CurBlock.BasePfn = Iter->PfnArray[Iter->CurIndex];
        Iter->CurBlock.AvailablePfns = AvailablePfns;
        return 0;
}

static UINT64 GetUnusedKernelPfn (VOID)
{
        struct KernelPfnIterator *Iter = &KernelPfnIteratorObj;

        if (!Iter->CurBlock.AvailablePfns) {
                FindNextAvailableBlock (Iter);
        }

        Iter->CurBlock.AvailablePfns--;
        return ++Iter->CurBlock.BasePfn;
}

/*
 * get a pfn which is unused by kernel and UEFI.
 *
 * unused pfns are pnfs which doesn't overlap with image kernel pages
 * or UEFI pages. These pfns are used for bounce pages, bounce tables
 * and relocation code.
 */
static UINT64 GetUnusedPfn ()
{
        UINT64 Pfn;

        do {
                Pfn = GetUnusedKernelPfn ();
        } while (!CheckFreeRanges (Pfn << PAGE_SHIFT));

        return Pfn;
}

/*
 * Preallocation is done for performance reason. We want to map memory
 * as big as possible. So that UEFI can create bigger page table mappings.
 * We have seen mapping single page is taking time in terms of few ms.
 * But we cannot preallocate every free page, becasue that causes allocation
 * failures for UEFI. Hence allocate most of the free pages but some(10MB)
 * are kept unallocated for UEFI to use. If kernel has any destined pages in
 * this region, that will be bounced.
 */
static VOID PreallocateFreeRanges (VOID)
{
        INT32 Iter = 0, Ret;
        INT32 ReservationDone = 0;
        UINT64 AllocAddr, RangeSize;
        UINT64 NumPages;

        for (Iter = FreeRangeCount - 1; Iter >= 0 ; Iter--) {
                RangeSize = FreeRangeBuf[Iter].End - FreeRangeBuf[Iter].Start;
                if ((!ReservationDone)
                    &&
                    (RangeSize > RESERVE_FREE_SIZE)) {
                        /*
                         * We have more buffer. Remove reserved buf and allocate
                         * rest in the range.
                         */
                        ReservationDone = 1;
                        AllocAddr = FreeRangeBuf[Iter].Start +
                                        RESERVE_FREE_SIZE;
                        RangeSize -=  RESERVE_FREE_SIZE;
                        NumPages = RangeSize / PAGE_SIZE;
                        printf ("Reserved range = 0x%lx - 0x%lx\n",
                                FreeRangeBuf[Iter].Start, AllocAddr - 1);
                        /* Modify the free range start */
                        FreeRangeBuf[Iter].Start = AllocAddr;
                } else {
                        AllocAddr = FreeRangeBuf[Iter].Start;
                        NumPages = RangeSize / PAGE_SIZE;
                }

                Ret = gBS->AllocatePages (AllocateAddress, EfiBootServicesData,
                               NumPages, &AllocAddr);
                if (Ret) {
                        printf (
                        "WARN: Prealloc falied LINE %d alloc_addr = 0x%lx\n",
                         __LINE__, AllocAddr);
                }
        }
}

/* Assumption: There is no overlap in the regions */
static struct MappedRange * AddRangeSorted (struct MappedRange * Head,
                                UINT64 Start, UINT64 End)
{
        struct MappedRange * Elem, * P;

        Elem  = AllocateZeroPool (sizeof (struct MappedRange));
        if (!Elem) {
                printf ("Failed to AllocateZeroPool %d\n", __LINE__);
                return NULL;
        }
        Elem->Start = Start;
        Elem->End = End;
        Elem->Next = NULL;

        if (Head == NULL) {
                return Elem;
        }

        if (Start <= Head->Start) {
                Elem->Next = Head;
                return Elem;
        }

        P = Head;
        while ((P->Next != NULL)
               &&
               (P->Next->Start < Start)) {
                P = P->Next;
        }

        Elem->Next = P->Next;
        P->Next = Elem;

        return Head;
}

/*
 * Get the UEFI memory map to collect ranges of
 * memory of type EfiConventional
 */
static INT32 GetConventionalMemoryRanges (VOID)
{
        EFI_MEMORY_DESCRIPTOR   *MemMap;
        EFI_MEMORY_DESCRIPTOR   *MemMapPtr;
        UINTN                   MemMapSize;
        UINTN                   MapKey, DescriptorSize;
        UINTN                   Index;
        UINT32                  DescriptorVersion;
        EFI_STATUS              Status;
        INT32 IndexB = 0;

        MemMapSize = 0;
        MemMap     = NULL;

        Status = gBS->GetMemoryMap (&MemMapSize, MemMap, &MapKey,
                        &DescriptorSize, &DescriptorVersion);
        if (Status != EFI_BUFFER_TOO_SMALL) {
                printf ("ERROR: Undefined response get memory map\n");
                return -1;
        }
        if (CHECK_ADD64 (MemMapSize, EFI_PAGE_SIZE)) {
                printf ("ERROR: integer Overflow while adding additional"
                                        "memory to MemMapSize");
                return -1;
        }
        MemMapSize = MemMapSize + EFI_PAGE_SIZE;
        MemMap = AllocateZeroPool (MemMapSize);
        if (!MemMap) {
                printf ("ERROR: Failed to allocate memory for memory map\n");
                return -1;
        }
        MemMapPtr = MemMap;
        Status = gBS->GetMemoryMap (&MemMapSize, MemMap, &MapKey,
                        &DescriptorSize, &DescriptorVersion);
        if (EFI_ERROR (Status)) {
                printf ("ERROR: Failed to query memory map\n");
                FreePool (MemMapPtr);
                return -1;
        }
        for (Index = 0; Index < MemMapSize / DescriptorSize; Index ++) {
                if (MemMap->Type == EfiConventionalMemory) {
                        FreeRangeBuf[IndexB].Start = MemMap->PhysicalStart;
                        FreeRangeBuf[IndexB].End =
                                MemMap->PhysicalStart +
                                        MemMap->NumberOfPages * PAGE_SIZE;
                        printf ("Free Range 0x%lx --- 0x%lx\n",
                                FreeRangeBuf[IndexB].Start,
                                FreeRangeBuf[IndexB].End);
                        IndexB++;
                }
                MemMap = (EFI_MEMORY_DESCRIPTOR *)
                                ((UINTN)MemMap + DescriptorSize);
        }
        FreeRangeCount = IndexB;
        FreePool (MemMapPtr);
        return 0;
}

typedef struct PartitionDetails {
        EFI_BLOCK_IO_PROTOCOL *BlockIo;
        EFI_HANDLE *Handle;
        INT32 BlocksPerPage;
}PartitionDetails;
static struct PartitionDetails SwapDetails;

static INT32 VerifySwapPartition (VOID)
{
        INT32 Status;
        EFI_BLOCK_IO_PROTOCOL *BlockIo = NULL;
        EFI_HANDLE *Handle = NULL;

        Status = PartitionGetInfo (SWAP_PARTITION_NAME, &BlockIo, &Handle);
        if (Status != EFI_SUCCESS) {
                return Status;
        }

        if (!Handle) {
                printf ("EFI handle for swap partition is corrupted\n");
                return -1;
        }

        if (CHECK_ADD64 (BlockIo->Media->LastBlock, 1)) {
                printf ("Integer overflow while adding LastBlock and 1\n");
                return -1;
        }

        if ((MAX_UINT64 / (BlockIo->Media->LastBlock + 1)) <
                        (UINT64)BlockIo->Media->BlockSize) {
                printf (
                "Integer overflow while multiplying LastBlock and BlockSize\n"
                );
                return -1;
        }

        SwapDetails.BlockIo = BlockIo;
        SwapDetails.Handle = Handle;
        SwapDetails.BlocksPerPage = EFI_PAGE_SIZE / BlockIo->Media->BlockSize;
        return 0;
}

static INT32 ReadImage (UINT64 Offset, VOID *Buff, INT32 NrPages)
{
        INT32 Status;
        EFI_BLOCK_IO_PROTOCOL *BlockIo = SwapDetails.BlockIo;
        EFI_LBA Lba;

        Lba = Offset * SwapDetails.BlocksPerPage;
        Status = BlockIo->ReadBlocks (BlockIo,
                        BlockIo->Media->MediaId,
                        Lba,
                        EFI_PAGE_SIZE * NrPages,
                        (VOID*)Buff);
        if (Status != EFI_SUCCESS) {
                printf ("Read image failed Line = %d\n", __LINE__);
                return Status;
        }

        return 0;
}

static INT32 IsCurrentTableFull (struct BounceTableIterator *Bti)
{
        return (Bti->CurIndex == ENTRIES_PER_TABLE);
}

static VOID AllocNextTable (struct BounceTableIterator *Bti)
{
        /* Allocate and chain next bounce table */
        Bti->CurTable->NextBounceTable = GetUnusedPfn () << PAGE_SHIFT;
        Bti->CurTable = (struct BounceTable *)
                                Bti->CurTable->NextBounceTable;
        Bti->CurIndex = 0;
}

static struct BouncePfnEntry * FindNextBounceEntry (VOID)
{
        struct BounceTableIterator *Bti = &TableIterator;

        if (IsCurrentTableFull (Bti)) {
                AllocNextTable (Bti);
        }

        return &Bti->CurTable->BounceEntry[Bti->CurIndex++];
}

static VOID UpdateBounceEntry (UINT64 DstPfn, UINT64 SrcPfn)
{
        struct BouncePfnEntry *Entry;

        Entry = FindNextBounceEntry ();
        Entry->DstPfn = DstPfn;
        Entry->SrcPfn = SrcPfn;
        BouncedPages++;
}

/*
 * Copy page to destination if page is free and is not in reserved area.
 * Bounce page otherwise.
 */
static VOID CopyPageToDst (UINT64 SrcPfn, UINT64 DstPfn)
{
        UINT64 TargetAddr = DstPfn << PAGE_SHIFT;

        if (CheckFreeRanges (TargetAddr)) {
                CopyPage (SrcPfn, DstPfn);
        } else {
                UINT64 BouncePfn = GetUnusedPfn ();
                CopyPage (SrcPfn, BouncePfn);
                UpdateBounceEntry (DstPfn, BouncePfn);
        }
}

static VOID PrintImageKernelDetails (struct SwsuspInfo *info)
{
        /*TODO: implement printing of kernel details here*/
        return;
}

/*
 * swsusp_header->image points to first swap_map page. From there onwards,
 * swap_map pages are repeated at every PFN_INDEXES_PER_PAGE intervals.
 * This function returns true if offset belongs to a swap_map page.
 */
static INT32 CheckSwapMapPage (UINT64 Offset)
{
        Offset -= SwsuspHeader->Image;
        return (Offset % PFN_INDEXES_PER_PAGE) == 0;
}

#if HIBERNATION_SUPPORT_AES
static INT32 DecryptPage (VOID *EncryptData)
{
        SW_CipherEncryptDir Dir = SW_CIPHER_DECRYPT;
        SW_CipherModeType Mode = SW_CIPHER_MODE_GCM;
        IovecListType   ioVecIn;
        IovecListType   ioVecOut;
        IovecType       IovecIn;
        IovecType       IovecOut;

        ioVecIn.size = 1;
        ioVecIn.iov = &IovecIn;
        ioVecIn.iov[0].dwLen = PAGE_SIZE;
        ioVecIn.iov[0].pvBase = EncryptData;
        ioVecOut.size = 1;
        ioVecOut.iov = &IovecOut;
        ioVecOut.iov[0].dwLen = PAGE_SIZE;
        ioVecOut.iov[0].pvBase = Dp.Out;

        if (SW_Cipher_Init (SW_CIPHER_ALG_AES256)) {
                return -1;
        }
        if (SW_Cipher_SetParam (SW_CIPHER_PARAM_DIRECTION, &Dir,
                sizeof (SW_CipherEncryptDir)))
                return -1;
        if (SW_Cipher_SetParam (SW_CIPHER_PARAM_MODE, &Mode, sizeof (Mode))) {
                return -1;
        }
        if (SW_Cipher_SetParam (SW_CIPHER_PARAM_KEY, Dp.UnwrappedKey,
                sizeof (Dp.UnwrappedKey)))
                return -1;
        if (SW_Cipher_SetParam (SW_CIPHER_PARAM_IV, Dp.Iv, sizeof (Dp.Iv))) {
                return -1;
        }
        if (SW_Cipher_SetParam (SW_CIPHER_PARAM_AAD, (VOID *)Dp.Aad,
                sizeof (Dp.Aad)))
                return -1;
        if (SW_CipherData (ioVecIn, &ioVecOut)) {
                return -1;
        }
        if (SW_Cipher_GetParam (SW_CIPHER_PARAM_TAG, (VOID*)(Dp.AuthCur),
                Dp.Authsize))
                return -1;
        if (MemCmp (Dp.AuthCur, Authtags, Dp.Authsize)) {
                printf ("Auth Comparsion failed\n");
                return -1;
        }

        gBS->CopyMem ((VOID *)(EncryptData), (VOID *)(Dp.Out), PAGE_SIZE);
        SW_Cipher_DeInit ();
        Authtags = Authtags + Dp.Authsize;
        return 0;
}
#else

static INT32 DecryptPage (VOID *encrypt_data)
{
        return 0;
}
#endif

static INT32 ReadSwapInfoStruct (VOID)
{
        struct SwsuspInfo *Info;

        BootStatsSetTimeStamp (BS_KERNEL_LOAD_START);

        Info = AllocateZeroPool (PAGE_SIZE);
        if (!Info) {
                printf ("Memory alloc failed Line %d\n", __LINE__);
                return -1;
        }
        if (ReadImage (SWAP_INFO_OFFSET, Info, 1)) {
                printf ("Failed to read Line %d\n", __LINE__);
                FreePages (Info, 1);
                return -1;
        }
        if (DecryptPage (Info)) {
                printf ("Decryption of swsusp_info failed\n");
                return -1;
        }
        ResumeHdr = (struct ArchHibernateHdr *)Info;
        NrMetaPages = Info->Pages - Info->ImagePages - 1;
        NrCopyPages = Info->ImagePages;
        printf ("Total pages to copy = %lu Total meta pages = %lu\n",
                                NrCopyPages, NrMetaPages);
        PrintImageKernelDetails (Info);
        return 0;
}

/*
 * Reads image kernel pfn indexes by stripping off interleaved swap_map pages.
 *
 * swap_map pages are particularly useful when swap slot allocations are
 * randomized. For bootloader based hibernation we have disabled this for
 * performance reasons. But swap_map pages are still interleaved because
 * kernel/power/snapshot.c is written to handle both scenarios(sequential
 * and randomized swap slot).
 *
 * Snapshot layout in disk with randomization disabled for swap allocations in
 * kernel looks likes:
 *
 *                      disk offsets
 *                              |
 *                              |
 *                              V
 *                                 -----------------------
 *                              0 |     header            |
 *                                |-----------------------|
 *                              1 |  swap_map page 0      |
 *                                |-----------------------|           ------
 *                              2 |  swsusp_info struct   |              ^
 *      ------                    |-----------------------|              |
 *        ^                     3 |  PFN INDEX Page 0     |              |
 *        |                       |-----------------------|              |
 *        |                     4 |  PFN INDEX Page 1     |              |
 *        |                       |-----------------------| 511 swap map entries
 * 510 pfn index pages            |     :       :         |              |
 *        |                       |     :       :         |              |
 *        |                       |     :       :         |              |
 *        |                       |-----------------------|              |
 *        |                   512 |  PFN INDEX Page 509   |              V
 *      ------                    |-----------------------|            ------
 *                            513 |  swap_map page 1      |
 *      ------                    |-----------------------|            ------
 *        ^                   514 |  PFN INDEX Page 510   |              ^
 *        |                       |-----------------------|              |
 *        |                   515 |  PFN INDEX Page 511   |              |
 *        |                       |-----------------------|              |
 * 511 pfn index pages            |     :       :         | 511 swap map entries
 *        |                       |     :       :         |              |
 *        |                       |     :       :         |              |
 *        |                       |-----------------------|              |
 *        V                  1024 |  PFN INDEX Page 1021  |              V
 *      ------                    |-----------------------|            ------
 *                           1025 |  swap_map page 2      |
 *      ------                    |-----------------------|
 *        ^                  1026 |  PFN INDEX Page 1022  |
 *        |                       |-----------------------|
 *        |                  1027 |  PFN INDEX Page 1023  |
 *        |                       |-----------------------|
 * 511 pfn index pages            |     :       :         |
 *        |                       |     :       :         |
 *        |                       |     :       :         |
 *        |                       |-----------------------|
 *        V                  1536 |  PFN INDEX Page 1532  |
 *      ------                    |-----------------------|
 *                           1537 |  swap_map page 3      |
 *                                |-----------------------|
 *                           1538 |  PFN INDEX Page 1533  |
 *                                |-----------------------|
 *                           1539 |  PFN INDEX Page 1534  |
 *                                |-----------------------|
 *                                |     :       :         |
 *                                |     :       :         |
 */
static UINT64* ReadKernelImagePfnIndexes (UINT64 *Offset)
{
        UINT64 *PfnArray, *ArrayIndex;
        UINT64 PendingPages = NrMetaPages;
        UINT64 PagesToRead, PagesRead = 0;
        UINT64 DiskOffset;
        VOID *PfnArrayStart;
        INT32 Loop = 0, Ret;

        PfnArray = AllocatePages (NrMetaPages);
        if (!PfnArray) {
                printf ("Memory alloc failed Line %d\n", __LINE__);
                return NULL;
        }

        PfnArrayStart = PfnArray;
        DiskOffset = FIRST_PFN_INDEX_OFFSET;
        /*
         * First swap_map page has one less pfn_index page
         * because of presence of swsusp_info struct. Handle
         * it separately.
         */
        PagesToRead = MIN (PendingPages, ENTRIES_PER_SWAPMAP_PAGE - 1);
        ArrayIndex = PfnArray;
        do {
                Ret = ReadImage (DiskOffset, ArrayIndex, PagesToRead);
                if (Ret) {
                        printf ("Disk read failed Line %d\n", __LINE__);
                        goto err;
                }
                PagesRead += PagesToRead;
                PendingPages -= PagesToRead;
                if (!PendingPages) {
                        break;
                }
                Loop++;
                /*
                 * SwsuspHeader->Image points to first SwapMap page.
                 * From there onwards,
                 * swap_map pages are repeated at PFN_INDEXES_PER_PAGE interval.
                 * pfn_index pages follows the swap map page.
                 * So we can arrive at next pfn_index by using below formula,
                 *
                 * base_swap_map_slot + PFN_INDEXES_PER_PAGE * n + 1
                 */
                DiskOffset = SwsuspHeader->Image +
                                (PFN_INDEXES_PER_PAGE * Loop) + 1;
                PagesToRead = MIN (PendingPages, ENTRIES_PER_SWAPMAP_PAGE);
                ArrayIndex = PfnArray + PagesRead * PFN_INDEXES_PER_PAGE;
        } while (1);

        *Offset = DiskOffset + PagesToRead;
        while (PendingPages != NrMetaPages) {
                if (DecryptPage (PfnArrayStart)) {
                        printf ("Decryption failed for pfn array\n");
                        return NULL;
                }
                PfnArrayStart = (CHAR8 *)PfnArrayStart + PAGE_SIZE;
                PendingPages++;
        }

        return PfnArray;
err:
        FreePages (PfnArray, NrMetaPages);
        return NULL;
}

static INT32 ReadDataPages (UINT64 *KernelPfnIndexes,
                        UINT64 Offset, VOID *DiskReadBuffer)
{
        UINT32 PendingPages, NrReadPages;
        UINT64 Temp, DiskReadMs = 0;
        UINT64 CopyPageMs = 0;
        UINT64 SrcPfn, DstPfn;
        UINT64 PfnIndex = 0;
        UINT64 MBs, MBPS, DDR_MBPS;
        INT32 Ret;

        PendingPages = NrCopyPages;
        while (PendingPages > 0) {
                /* read pages in chunks to improve disk read performance */
                NrReadPages = PendingPages > DISK_BUFFER_PAGES ?
                                        DISK_BUFFER_PAGES : PendingPages;
                Temp = GetTimerCountms ();
                Ret = ReadImage (Offset, DiskReadBuffer, NrReadPages);
                DiskReadMs += (GetTimerCountms () - Temp);
                if (Ret < 0) {
                        printf ("Disk read failed Line %d\n", __LINE__);
                        return -1;
                }
                SrcPfn = (UINT64) DiskReadBuffer >> PAGE_SHIFT;
                while (NrReadPages > 0) {
                        /* skip swap_map pages */
                        if (!CheckSwapMapPage (Offset)) {
                                DstPfn = KernelPfnIndexes[PfnIndex++];
                                PendingPages--;
                                Temp = GetTimerCountms ();
                                if (DecryptPage ((VOID *)(SrcPfn <<
                                                        PAGE_SHIFT))) {
                                        printf (
                                        "Decryption failed for Data pages\n"
                                        );
                                        return -1;
                                }
                                CopyPageToDst (SrcPfn, DstPfn);
                                CopyPageMs += (GetTimerCountms () - Temp);
                        }
                        SrcPfn++;
                        NrReadPages--;
                        Offset++;
                }
        }

        BootStatsSetTimeStamp (BS_KERNEL_LOAD_DONE);

        MBs = (NrCopyPages * PAGE_SIZE) / (1024 * 1024);
        if ((DiskReadMs == 0)
            ||
            (CopyPageMs == 0)) {
                return 0;
        }

        MBPS = (MBs * 1000) / DiskReadMs;
        DDR_MBPS = (MBs * 1000) /CopyPageMs;

        printf ("Image size = %lu MBs\n", MBs);
        printf ("Time spend - disk IO = %lu msecs (BW = %llu MBps)\n",
                DiskReadMs, MBPS);
        printf ("Time spend - DDR copy = %llu msecs (BW = %llu MBps)\n",
                CopyPageMs, DDR_MBPS);

        return 0;
}

static struct MappedRange * GetUefiSortedMemoryMap ()
{
        EFI_MEMORY_DESCRIPTOR   *MemMap;
        EFI_MEMORY_DESCRIPTOR   *MemMapPtr;
        UINTN                   MemMapSize;
        UINTN                   MapKey, DescriptorSize;
        UINTN                   Index;
        UINT32                  DescriptorVersion;
        EFI_STATUS              Status;

        struct MappedRange * UefiMap = NULL;
        MemMapSize = 0;
        MemMap     = NULL;

        Status = gBS->GetMemoryMap (&MemMapSize, MemMap, &MapKey,
                                &DescriptorSize, &DescriptorVersion);
        if (Status != EFI_BUFFER_TOO_SMALL) {
                printf ("ERROR: Undefined response get memory map\n");
                return NULL;
        }
        if (CHECK_ADD64 (MemMapSize, EFI_PAGE_SIZE)) {
                printf ("ERROR: integer Overflow while adding additional"
                                "memory to MemMapSize");
                return NULL;
        }
        MemMapSize = MemMapSize + EFI_PAGE_SIZE;
        MemMap = AllocateZeroPool (MemMapSize);
        if (!MemMap) {
                printf ("ERROR: Failed to allocate memory for memory map\n");
                return NULL;
        }
        MemMapPtr = MemMap;
        Status = gBS->GetMemoryMap (&MemMapSize, MemMap, &MapKey,
                                &DescriptorSize, &DescriptorVersion);
        if (EFI_ERROR (Status)) {
                printf ("ERROR: Failed to query memory map\n");
                FreePool (MemMapPtr);
                return NULL;
        }
        for (Index = 0; Index < MemMapSize / DescriptorSize; Index ++) {
                UefiMap = AddRangeSorted (UefiMap,
                        MemMap->PhysicalStart,
                        MemMap->PhysicalStart +
                        MemMap->NumberOfPages * PAGE_SIZE);

                if (!UefiMap) {
                        printf ("ERROR: UefiMap is NULL\n");
                        return NULL;
                }

                MemMap = (EFI_MEMORY_DESCRIPTOR *)((UINTN)MemMap +
                                DescriptorSize);
        }
        FreePool (MemMapPtr);
        return UefiMap;
}

static EFI_STATUS CreateMapping (UINTN Addr, UINTN Size)
{
        EFI_STATUS Status;
        EFI_GCD_MEMORY_SPACE_DESCRIPTOR Descriptor;
        printf ("Address: %llx Size: %llx\n", Addr, Size);

        Status = gDS->GetMemorySpaceDescriptor (Addr, &Descriptor);
        if (EFI_ERROR (Status)) {
                printf ("Failed getMemorySpaceDescriptor Line %d\n", __LINE__);
                return Status;
        }

        if (Descriptor.GcdMemoryType != EfiGcdMemoryTypeMemoryMappedIo) {
                if (Descriptor.GcdMemoryType != EfiGcdMemoryTypeNonExistent) {
                        Status = gDS->RemoveMemorySpace (Addr, Size);
                        printf ("Falied RemoveMemorySpace %d: %d\n",
                                __LINE__, Status);
                }
                Status = gDS->AddMemorySpace (EfiGcdMemoryTypeReserved,
                                Addr, Size, EFI_MEMORY_UC);
                if (EFI_ERROR (Status)) {
                        printf ("Failed to AddMemorySpace 0x%x, size 0x%x\n",
                                Addr, Size);
                        return Status;
                }

                Status = gDS->SetMemorySpaceAttributes (Addr, Size,
                                                        EFI_MEMORY_UC);
                if (EFI_ERROR (Status)) {
                        printf (
                        "Failed to SetMemorySpaceAttributes 0x%x, size 0x%x\n",
                         Addr, Size);
                                return Status;
                }
        }

        return EFI_SUCCESS;
}

/*
 * Determine the unmapped uefi memory from the list 'uefi_mapped_sorted_list'
 * and map all the unmapped regions.
 */
static EFI_STATUS UefiMapUnmapped ()
{
        struct MappedRange * UefiMappedSortedList, * Cur, * Next;
        EFI_STATUS Status;

        UefiMappedSortedList = GetUefiSortedMemoryMap ();
        if (!UefiMappedSortedList) {
                printf ("ERROR: Unable to get UEFI memory map\n");
                return -1;
        }

        Cur = UefiMappedSortedList;
        Next = Cur->Next;

        while (Cur) {
                if (Next &&
                    ((Next->Start) >
                    (Cur->End))) {
                        Status = CreateMapping (Cur->End,
                                                Next->Start - Cur->End);
                        if (Status != EFI_SUCCESS) {
                                printf ("ERROR: Mapping failed\n");
                                return Status;
                        }
                }
                Status = gBS->FreePool (Cur);
                if (Status != EFI_SUCCESS) {
                        printf ("FreePool failed %d\n", __LINE__);
                        return -1;
                }
                Cur = Next;
                if (Next) {
                        Next = Next->Next;
                }
        }

        return EFI_SUCCESS;
}

#define PT_ENTRIES_PER_LEVEL 512

static VOID SetRwPerm (UINT64 *Entry)
{
        /* Clear AP perm bits */
        *Entry &= ~(0x3UL << 6);
}

static VOID SetExPerm (UINT64 *Entry)
{
        /* Clear UXN and PXN bits */
        *Entry &= ~(0x3UL << 53);
}

static INT32 RelocatePagetables (INT32 Level, UINT64 *Entry, INT32 PtCount)
{
        INT32 Iter;
        UINT64 Mask;
        UINT64 *PageAddr;
        UINT64 ApPerm;

        ApPerm = *Entry & (0x3 << 6);
        ApPerm = ApPerm >> 6;
        /* Strip out lower and higher page attribute fields */
        Mask = ~(0xFFFFUL << 48 | 0XFFFUL);

        /* Invalid entry */
        if ((Level > 3)
               ||
            (!(*Entry & 0x1))) {
                return PtCount;
        }

        if (Level == 3 ) {
                if ((*Entry & Mask) == RelocateAddress) {
                        SetExPerm (Entry);
                }
                if ((ApPerm == 2)
                        ||
                    (ApPerm == 3)) {
                        SetRwPerm (Entry);
                }
                return PtCount;
        }

        /* block entries */
        if ((*Entry & 0x3) == 1) {
                UINT64 Addr = RelocateAddress;
                if (Level == 1) {
                        ALIGN_1GB (Addr);
                }
                if (Level == 2) {
                        ALIGN_2MB (Addr);
                }
                if ((*Entry & Mask) == Addr) {
                        SetExPerm (Entry);
                }
                if ((ApPerm == 2)
                        ||
                    (ApPerm == 3)) {
                        SetRwPerm (Entry);
                }
                return PtCount;
        }

        /* Control reaches here only if it is a table entry */

        PageAddr = (UINT64*)(GetUnusedPfn () << PAGE_SHIFT);

        gBS->CopyMem ((VOID *)(PageAddr), (VOID *)(*Entry & Mask), PAGE_SIZE);
        PtCount++;
        /* Clear off the old address alone */
        *Entry &= ~Mask;
        /* Fill new table address */
        *Entry |= (UINT64 )PageAddr;

        for (Iter = 0 ; Iter < PT_ENTRIES_PER_LEVEL; Iter++)
                PtCount = RelocatePagetables (Level + 1, PageAddr + Iter,
                                PtCount);

        return PtCount;
}

static UINT64 GetTtbr0 ()
{
        UINT64 Base;

        asm __volatile__ (
        "mrs %[ttbr0_base], ttbr0_el1\n"
        :[ttbr0_base] "=r" (Base)
        :
        :"memory");

        return Base;
}

static UINT64 CopyPageTables ()
{
        UINT64 OldTtbr0 = GetTtbr0 ();
        UINT64 NewTtbr0;
        INT32  PtCount = 0;

        NewTtbr0 = GetUnusedPfn () << PAGE_SHIFT;
        gBS->CopyMem ((VOID *)(NewTtbr0), (VOID *)(OldTtbr0), PAGE_SIZE);
        PtCount = RelocatePagetables (0, (UINT64 *)NewTtbr0, 1);

        printf ("Copied %d Page Tables\n", PtCount);
        return NewTtbr0;
}

static INT32 RestoreSnapshotImage (VOID)
{
        INT32 Ret;
        VOID *DiskReadBuffer;
        UINT64 StartMs, Offset;
        UINT64 *KernelPfnIndexes;
        struct BounceTableIterator *Bti = &TableIterator;

        StartMs = GetTimerCountms ();
        Ret = ReadSwapInfoStruct ();
        if (Ret < 0) {
                return Ret;
        }

        KernelPfnIndexes = ReadKernelImagePfnIndexes (&Offset);
        if (!KernelPfnIndexes) {
                return -1;
        }
        InitKernelPfnIterator (KernelPfnIndexes);

        DiskReadBuffer =  AllocatePages (DISK_BUFFER_PAGES);
        if (!DiskReadBuffer) {
                printf ("Memory alloc failed Line %d\n", __LINE__);
                return -1;
        } else {
                printf ("Disk buffer alloction at 0x%p - 0x%p\n",
                        DiskReadBuffer,
                        DiskReadBuffer + DISK_BUFFER_SIZE - 1);
        }

        printf ("Mapping Regions:\n");
        Ret = UefiMapUnmapped ();
        if (Ret < 0) {
                printf ("Error mapping unmapped regions\n");
                goto err;
        }

        /*
         * No dynamic allocation beyond this point. If not honored it will
         * result in corruption of pages.
         */
        GetConventionalMemoryRanges ();
        PreallocateFreeRanges ();

        Bti->FirstTable = (struct BounceTable *)
                                (GetUnusedPfn () << PAGE_SHIFT);
        Bti->CurTable = Bti->FirstTable;

        Ret = ReadDataPages (KernelPfnIndexes, Offset, DiskReadBuffer);
        if (Ret < 0) {
                printf ("error in restore_snapshot_image\n");
                goto err;
        }

        printf ("Time loading image (excluding bounce buffers) = %lu msecs\n",
                (GetTimerCountms () - StartMs));
        printf ("Image restore Completed...\n");
        printf ("Total bounced Pages = %d (%lu MBs)\n",
                BouncedPages, (BouncedPages * PAGE_SIZE)/(1024 * 1024));
err:
        FreePages (DiskReadBuffer, DISK_BUFFER_PAGES);
        return Ret;
}

static VOID CopyBounceAndBootKernel ()
{
        INT32 Status;
        struct BounceTableIterator *Bti = &TableIterator;
        UINT64 CpuResume = (UINT64) ResumeHdr->PhysReEnterKernel;
        UINT64 Ttbr0;
        UINT64 StackPointer;

        /*
         * After copying the bounce pages, there is a chance of corrupting
         * old stack which might be kernel memory.
         * To avoid kernel memory corruption, Use a free page for the stack.
         */
        StackPointer = GetUnusedPfn () << PAGE_SHIFT;
        StackPointer = StackPointer + PAGE_SIZE - 16;

        /*
         * The restore routine "JumpToKernel" copies the bounced pages after
         * iterating through the bounce entry table and passes control to
         * hibernated kernel after calling _PreparePlatformHarware
         *
         * Disclaimer: JumpToKernel.s is less than PAGE_SIZE
         */
        gBS->CopyMem ((VOID*)RelocateAddress, (VOID*)&JumpToKernel, PAGE_SIZE);
        Ttbr0 = CopyPageTables ();

        printf ("Disable UEFI Boot services\n");
        printf ("Kernel entry point = 0x%lx\n", CpuResume);
        printf ("Relocation code at = 0x%lx\n", RelocateAddress);

        /* Shut down UEFI boot services */
        Status = ShutdownUefiBootServices ();
        if (EFI_ERROR (Status)) {
                printf ("ERROR: Can not shutdown UEFI boot services."
                        " Status=0x%X\n", Status);
                return;
        }

        asm __volatile__ (
                "mov    x18, %[ttbr_reg]\n"
                "msr    ttbr0_el1, x18\n"
                "dsb    sy\n"
                "isb\n"
                "ic     iallu\n"
                "dsb    sy\n"
                "isb\n"
                "tlbi   vmalle1\n"
                "dsb    sy\n"
                "isb\n"
                :
                :[ttbr_reg] "r" (Ttbr0)
                :"x18", "memory");

        asm __volatile__ (
                "mov x18, %[table_base]\n"
                "mov x19, %[count]\n"
                "mov x21, %[resume]\n"
                "mov sp, %[sp]\n"
                "mov x22, %[relocate_code]\n"
                "br x22"
                :
                :[table_base] "r" (Bti->FirstTable),
                [count] "r" (BouncedPages),
                [resume] "r" (CpuResume),
                [sp] "r" (StackPointer),
                [relocate_code] "r" (RelocateAddress)
                :"x18", "x19", "x21", "x22", "memory");
}

static INT32 CheckForValidHeader (VOID)
{
        SwsuspHeader = AllocatePages (1);
        if (!SwsuspHeader) {
                printf ("Memory alloc failed Line %d\n", __LINE__);
                return -1;
        }

        if (VerifySwapPartition ()) {
                printf ("Failled VerifySwapPartition\n");
                goto read_image_error;
        }

        if (ReadImage (0, SwsuspHeader, 1)) {
                printf ("Disk read failed Line %d\n", __LINE__);
                goto read_image_error;
        }

        if (MemCmp (HIBERNATE_SIG, SwsuspHeader->Sig, 10)) {
                printf ("Signature not found. Aborting hibernation\n");
                goto read_image_error;
        }

        printf ("Image slot at 0x%lx\n", SwsuspHeader->Image);
        if (SwsuspHeader->Image != 1) {
                printf ("Invalid swap slot. Aborting hibernation!");
                goto read_image_error;
        }

        printf ("Signature found. Proceeding with disk read...\n");
        return 0;

read_image_error:
        FreePages (SwsuspHeader, 1);
        return -1;
}

static VOID EraseSwapSignature (VOID)
{
        EFI_STATUS Status;
        EFI_BLOCK_IO_PROTOCOL *BlockIo = SwapDetails.BlockIo;

        SwsuspHeader->Sig[0] = ' ';
        Status = BlockIo->WriteBlocks (BlockIo, BlockIo->Media->MediaId, 0,
                        EFI_PAGE_SIZE, (VOID*)SwsuspHeader);
        if (Status != EFI_SUCCESS) {
                printf ("Failed to erase swap signature\n");
        }
}

#if HIBERNATION_SUPPORT_AES
static INT32 InitTaAndGetKey (struct Secs2dTaHandle *TaHandle)
{
        INT32 Status;
        CmdReq Req = {0};
        CmdRsp Rsp = {0};
        UINT32 ReqLen, RspLen;

        Status = gBS->LocateProtocol (&gQcomQseecomProtocolGuid, NULL,
                (VOID **)&(TaHandle->QseeComProtocol));
        if (Status) {
                printf ("Error in locating Qseecom protocol Guid\n");
                return -1;
        }
        Status = TaHandle->QseeComProtocol->QseecomStartApp (
                TaHandle->QseeComProtocol, "secs2d_a", &(TaHandle->AppId));
        if (Status) {
                printf ("Error in secs2d app loading\n");
                return -1;
        }

        ReqLen = sizeof (CmdReq);
        if (ReqLen & QSEECOM_ALIGN_MASK) {
                ReqLen = QSEECOM_ALIGN (ReqLen);
        }

        RspLen = sizeof (CmdRsp);
        if (RspLen & QSEECOM_ALIGN_MASK) {
                RspLen = QSEECOM_ALIGN (RspLen);
        }

        Req.Cmd = UNWRAP_KEY_CMD;
        Req.UnwrapkeyReq.WrappedKeySize = WRAPPED_KEY_SIZE;
        gBS->CopyMem ((VOID *)Req.UnwrapkeyReq.WrappedKeyBuffer,
                        (VOID *)Dp.KeyBlob, sizeof (Dp.KeyBlob));
        Req.UnwrapkeyReq.CurrTime.Hour = 4;
        Status = TaHandle->QseeComProtocol->QseecomSendCmd (
                TaHandle->QseeComProtocol, TaHandle->AppId,
                        (UINT8 *)&Req, ReqLen,
                        (UINT8 *)&Rsp, RspLen);
        if (Status) {
                printf ("Error in conversion wrappeded key to unwrapped key\n");
                return -1;
        }
        gBS->CopyMem ((VOID *)Dp.UnwrappedKey,
                        (VOID *)Rsp.UnwrapkeyRsp.KeyBuffer, 32);
        return 0;
}


static VOID *memset (VOID *Destination, INT32 Value, INT32 Count)
{
  CHAR8 *Ptr = Destination;
  while (Count--) {
        *Ptr++ = Value;
  }

  return Destination;
}

static INT32 InitAesDecrypt (VOID)
{
        INT32 AuthslotStart;
        INT32 AuthslotCount;
        Secs2dTaHandle TaHandle = {0};

        memset (&Dp, 0, sizeof (Dp));
        AuthslotStart = SwsuspHeader->AuthslotStart;
        AuthslotCount = SwsuspHeader->AuthslotCount;

        Authtags = AllocatePages (AuthslotCount);
        if (!Authtags) {
                return -1;
        }
        if (ReadImage (AuthslotStart, Authtags, AuthslotCount)) {
                return -1;
        }

        Dp.Authsize = SwsuspHeader->Authsize;
        gBS->CopyMem ((VOID *)(&Dp.KeyBlob), (VOID *)(SwsuspHeader->KeyBlob),
                        sizeof (SwsuspHeader->KeyBlob));
        gBS->CopyMem ((VOID *)(&Dp.Iv), (VOID *)(SwsuspHeader->Iv),
                        sizeof (SwsuspHeader->Iv));
        gBS->CopyMem ((VOID *)(&Dp.Aad), (VOID *)(SwsuspHeader->Aad),
                        sizeof (SwsuspHeader->Aad));

        Dp.Out = AllocatePages (1);
        if (!Dp.Out) {
                return -1;
        }
        Dp.AuthCur = AllocateZeroPool (Dp.Authsize);
        if (!Dp.AuthCur) {
                return -1;
        }
        if (InitTaAndGetKey (&TaHandle)) {
                return -1;
        }

        printf ("Hibernation: AES init done\n");
        return 0;
}
#else

static INT32 InitAesDecrypt (VOID)
{
        return 0;
}
#endif

VOID BootIntoHibernationImage (BootInfo *Info, BOOLEAN *SetRotAndBootState)
{
        INT32 Ret;
        EFI_STATUS Status = EFI_SUCCESS;
        printf ("Entrying Hibernation restore\n");

        if (CheckForValidHeader () < 0) {
                return;
        }

        if (InitAesDecrypt ()) {
                printf ("AES initialization failed\n");
                return;
        }

        if (!SetRotAndBootState) {
                printf ("SetRotAndBootState cannot be NULL.\n");
                goto err;
        }

        Status = LoadImageAndAuth (Info, TRUE, FALSE);
        if (Status != EFI_SUCCESS) {
                printf ("Failed to set ROT and Bootstate : %r\n", Status);
                goto err;
        }

        /* ROT and BootState are set only once per boot.
         * set variable to TRUE to Avoid setting second
         * time incase hbernation resume fails at restore
         * snapshot stage..
         */
         *SetRotAndBootState = TRUE;

        Status = KeyMasterFbeSetSeed ();
        if (Status != EFI_SUCCESS) {
                printf ("Failed to set seed for fbe : %r\n", Status);
                goto err;
        }

        Ret = RestoreSnapshotImage ();
        if (Ret) {
                printf ("Failed restore_snapshot_image \n");
                goto err;
        }

        RelocateAddress = GetUnusedPfn () << PAGE_SHIFT;

        /* Reset swap signature now */
        if (!IsSnapshotGolden ()) {
                EraseSwapSignature ();
        }

        CopyBounceAndBootKernel ();
        /* Control should not reach here */

err:    /*
         * Erase swap signature to avoid kernel restoring the
         * hibernation image
         */
        if (!IsSnapshotGolden ()) {
                EraseSwapSignature ();
        }
        return;
}
#endif
