/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __X86_IOMMU_INTEL_REG_H
#define	__X86_IOMMU_INTEL_REG_H

#define	DMAR_PAGE_SIZE	PAGE_SIZE
#define	DMAR_PAGE_MASK	(DMAR_PAGE_SIZE - 1)
#define	DMAR_PAGE_SHIFT	PAGE_SHIFT
#define	DMAR_NPTEPG	(DMAR_PAGE_SIZE / sizeof(dmar_pte_t))
#define	DMAR_NPTEPGSHIFT 9
#define	DMAR_PTEMASK	(DMAR_NPTEPG - 1)

typedef struct dmar_root_entry {
	uint64_t r1;
	uint64_t r2;
} dmar_root_entry_t;
#define	DMAR_ROOT_R1_P		1 		   /* Present */
#define	DMAR_ROOT_R1_CTP_MASK	0xfffffffffffff000 /* Mask for Context-Entry
						      Table Pointer */

#define	DMAR_CTX_CNT		(DMAR_PAGE_SIZE / sizeof(dmar_root_entry_t))

typedef	struct dmar_ctx_entry {
	uint64_t ctx1;
	uint64_t ctx2;
} dmar_ctx_entry_t;
#define	DMAR_CTX1_P		1		/* Present */
#define	DMAR_CTX1_FPD		2		/* Fault Processing Disable */
						/* Translation Type: */
#define	DMAR_CTX1_T_UNTR	0		/* only Untranslated */
#define	DMAR_CTX1_T_TR		4		/* both Untranslated
						   and Translated */
#define	DMAR_CTX1_T_PASS	8		/* Pass-Through */
#define	DMAR_CTX1_ASR_MASK	0xfffffffffffff000 /* Mask for the Address
						   Space Root */
#define	DMAR_CTX2_AW_2LVL	0		/* 2-level page tables */
#define	DMAR_CTX2_AW_3LVL	1		/* 3-level page tables */
#define	DMAR_CTX2_AW_4LVL	2		/* 4-level page tables */
#define	DMAR_CTX2_AW_5LVL	3		/* 5-level page tables */
#define	DMAR_CTX2_AW_6LVL	4		/* 6-level page tables */
#define	DMAR_CTX2_DID(x)	((x) << 8)	/* Domain Identifier */

typedef struct dmar_pte {
	uint64_t pte;
} dmar_pte_t;
#define	DMAR_PTE_R		1		/* Read */
#define	DMAR_PTE_W		(1 << 1)	/* Write */
#define	DMAR_PTE_SP		(1 << 7)	/* Super Page */
#define	DMAR_PTE_SNP		(1 << 11)	/* Snoop Behaviour */
#define	DMAR_PTE_ADDR_MASK	0xffffffffff000	/* Address Mask */
#define	DMAR_PTE_TM		(1ULL << 62)	/* Transient Mapping */

/* Version register */
#define	DMAR_VER_REG	0
#define	DMAR_MAJOR_VER(x)	(((x) >> 4) & 0xf)
#define	DMAR_MINOR_VER(x)	((x) & 0xf)

/* Capabilities register */
#define	DMAR_CAP_REG	0x8
#define	DMAR_CAP_DRD	(1ULL << 55)	/* DMA Read Draining */
#define	DMAR_CAP_DWD	(1ULL << 54)	/* DMA Write Draining */
#define	DMAR_CAP_MAMV(x) ((u_int)(((x) >> 48) & 0x3f))
					/* Maximum Address Mask */
#define	DMAR_CAP_NFR(x)	((u_int)(((x) >> 40) & 0xff) + 1)
					/* Num of Fault-recording regs */
#define	DMAR_CAP_PSI	(1ULL << 39)	/* Page Selective Invalidation */
#define	DMAR_CAP_SPS(x)	((u_int)(((x) >> 34) & 0xf)) /* Super-Page Support */
#define	DMAR_CAP_SPS_2M	0x1
#define	DMAR_CAP_SPS_1G	0x2
#define	DMAR_CAP_SPS_512G 0x4
#define	DMAR_CAP_SPS_1T	0x8
#define	DMAR_CAP_FRO(x)	((u_int)(((x) >> 24) & 0x1ff))
					/* Fault-recording reg offset */
#define	DMAR_CAP_ISOCH	(1 << 23)	/* Isochrony */
#define	DMAR_CAP_ZLR	(1 << 22)	/* Zero-length reads */
#define	DMAR_CAP_MGAW(x) ((u_int)(((x) >> 16) & 0x3f))
					/* Max Guest Address Width */
#define DMAR_CAP_SAGAW(x) ((u_int)(((x) >> 8) & 0x1f))
					/* Adjusted Guest Address Width */
#define	DMAR_CAP_SAGAW_2LVL	0x01
#define	DMAR_CAP_SAGAW_3LVL	0x02
#define	DMAR_CAP_SAGAW_4LVL	0x04
#define	DMAR_CAP_SAGAW_5LVL	0x08
#define	DMAR_CAP_SAGAW_6LVL	0x10
#define	DMAR_CAP_CM	(1 << 7)	/* Caching mode */
#define	DMAR_CAP_PHMR	(1 << 6)	/* Protected High-mem Region */
#define	DMAR_CAP_PLMR	(1 << 5)	/* Protected Low-mem Region */
#define	DMAR_CAP_RWBF	(1 << 4)	/* Required Write-Buffer Flushing */
#define	DMAR_CAP_AFL	(1 << 3)	/* Advanced Fault Logging */
#define	DMAR_CAP_ND(x)	((u_int)((x) & 0x3))	/* Number of domains */

/* Extended Capabilities register */
#define	DMAR_ECAP_REG	0x10
#define	DMAR_ECAP_MHMV(x) ((u_int)(((x) >> 20) & 0xf))
					/* Maximum Handle Mask Value */
#define	DMAR_ECAP_IRO(x)  ((u_int)(((x) >> 8) & 0x3ff))
					/* IOTLB Register Offset */
#define	DMAR_ECAP_SC	(1 << 7)	/* Snoop Control */
#define	DMAR_ECAP_PT	(1 << 6)	/* Pass Through */
#define	DMAR_ECAP_EIM	(1 << 4)	/* Extended Interrupt Mode */
#define	DMAR_ECAP_IR	(1 << 3)	/* Interrupt Remapping */
#define	DMAR_ECAP_DI	(1 << 2)	/* Device IOTLB */
#define	DMAR_ECAP_QI	(1 << 1)	/* Queued Invalidation */
#define	DMAR_ECAP_C	(1 << 0)	/* Coherency */

/* Global Command register */
#define	DMAR_GCMD_REG	0x18
#define	DMAR_GCMD_TE	(1 << 31)	/* Translation Enable */
#define	DMAR_GCMD_SRTP	(1 << 30)	/* Set Root Table Pointer */
#define	DMAR_GCMD_SFL	(1 << 29)	/* Set Fault Log */
#define	DMAR_GCMD_EAFL	(1 << 28)	/* Enable Advanced Fault Logging */
#define	DMAR_GCMD_WBF	(1 << 27)	/* Write Buffer Flush */
#define	DMAR_GCMD_QIE	(1 << 26)	/* Queued Invalidation Enable */
#define DMAR_GCMD_IRE	(1 << 25)	/* Interrupt Remapping Enable */
#define	DMAR_GCMD_SIRTP	(1 << 24)	/* Set Interrupt Remap Table Pointer */
#define	DMAR_GCMD_CFI	(1 << 23)	/* Compatibility Format Interrupt */

/* Global Status register */
#define	DMAR_GSTS_REG	0x1c
#define	DMAR_GSTS_TES	(1 << 31)	/* Translation Enable Status */
#define	DMAR_GSTS_RTPS	(1 << 30)	/* Root Table Pointer Status */
#define	DMAR_GSTS_FLS	(1 << 29)	/* Fault Log Status */
#define	DMAR_GSTS_AFLS	(1 << 28)	/* Advanced Fault Logging Status */
#define	DMAR_GSTS_WBFS	(1 << 27)	/* Write Buffer Flush Status */
#define	DMAR_GSTS_QIES	(1 << 26)	/* Queued Invalidation Enable Status */
#define	DMAR_GSTS_IRES	(1 << 25)	/* Interrupt Remapping Enable Status */
#define	DMAR_GSTS_IRTPS	(1 << 24)	/* Interrupt Remapping Table
					   Pointer Status */
#define	DMAR_GSTS_CFIS	(1 << 23)	/* Compatibility Format
					   Interrupt Status */

/* Root-Entry Table Address register */
#define	DMAR_RTADDR_REG	0x20

/* Context Command register */
#define	DMAR_CCMD_REG	0x28
#define	DMAR_CCMD_ICC	(1ULL << 63)	/* Invalidate Context-Cache */
#define	DMAR_CCMD_ICC32	(1 << 31)
#define	DMAR_CCMD_CIRG_MASK	(0x3ULL << 61)	/* Context Invalidation
						   Request Granularity */
#define	DMAR_CCMD_CIRG_GLOB	(0x1ULL << 61)	/* Global */
#define	DMAR_CCMD_CIRG_DOM	(0x2ULL << 61)	/* Domain */
#define	DMAR_CCMD_CIRG_DEV	(0x3ULL << 61)	/* Device */
#define	DMAR_CCMD_CAIG(x)	(((x) >> 59) & 0x3) /* Context Actual
						    Invalidation Granularity */
#define	DMAR_CCMD_CAIG_GLOB	0x1		/* Global */
#define	DMAR_CCMD_CAIG_DOM	0x2		/* Domain */
#define	DMAR_CCMD_CAIG_DEV	0x3		/* Device */
#define	DMAR_CCMD_FM		(0x3UUL << 32)	/* Function Mask */
#define	DMAR_CCMD_SID(x)	(((x) & 0xffff) << 16) /* Source-ID */
#define	DMAR_CCMD_DID(x)	((x) & 0xffff)	/* Domain-ID */

/* Invalidate Address register */
#define	DMAR_IVA_REG_OFF	0
#define	DMAR_IVA_IH		(1 << 6)	/* Invalidation Hint */
#define	DMAR_IVA_AM(x)		((x) & 0x1f)	/* Address Mask */
#define	DMAR_IVA_ADDR(x)	((x) & ~0xfffULL) /* Address */

/* IOTLB Invalidate register */
#define	DMAR_IOTLB_REG_OFF	0x8
#define	DMAR_IOTLB_IVT		(1ULL << 63)	/* Invalidate IOTLB */
#define	DMAR_IOTLB_IVT32	(1 << 31)
#define	DMAR_IOTLB_IIRG_MASK	(0x3ULL << 60)	/* Invalidation Request
						   Granularity */
#define	DMAR_IOTLB_IIRG_GLB	(0x1ULL << 60)	/* Global */
#define	DMAR_IOTLB_IIRG_DOM	(0x2ULL << 60)	/* Domain-selective */
#define	DMAR_IOTLB_IIRG_PAGE	(0x3ULL << 60)	/* Page-selective */
#define	DMAR_IOTLB_IAIG_MASK	(0x3ULL << 57)	/* Actual Invalidation
						   Granularity */
#define	DMAR_IOTLB_IAIG_INVLD	0		/* Hw detected error */
#define	DMAR_IOTLB_IAIG_GLB	(0x1ULL << 57)	/* Global */
#define	DMAR_IOTLB_IAIG_DOM	(0x2ULL << 57)	/* Domain-selective */
#define	DMAR_IOTLB_IAIG_PAGE	(0x3ULL << 57)	/* Page-selective */
#define	DMAR_IOTLB_DR		(0x1ULL << 49)	/* Drain Reads */
#define	DMAR_IOTLB_DW		(0x1ULL << 48)	/* Drain Writes */
#define	DMAR_IOTLB_DID(x)	(((uint64_t)(x) & 0xffff) << 32) /* Domain Id */

/* Fault Status register */
#define	DMAR_FSTS_REG		0x34
#define	DMAR_FSTS_FRI(x)	(((x) >> 8) & 0xff) /* Fault Record Index */
#define	DMAR_FSTS_ITE		(1 << 6)	/* Invalidation Time-out */
#define	DMAR_FSTS_ICE		(1 << 5)	/* Invalidation Completion */
#define	DMAR_FSTS_IQE		(1 << 4)	/* Invalidation Queue */
#define	DMAR_FSTS_APF		(1 << 3)	/* Advanced Pending Fault */
#define	DMAR_FSTS_AFO		(1 << 2)	/* Advanced Fault Overflow */
#define	DMAR_FSTS_PPF		(1 << 1)	/* Primary Pending Fault */
#define	DMAR_FSTS_PFO		1		/* Fault Overflow */

/* Fault Event Control register */
#define	DMAR_FECTL_REG		0x38
#define	DMAR_FECTL_IM		(1 << 31)	/* Interrupt Mask */
#define	DMAR_FECTL_IP		(1 << 30)	/* Interrupt Pending */

/* Fault Event Data register */
#define	DMAR_FEDATA_REG		0x3c

/* Fault Event Address register */
#define	DMAR_FEADDR_REG		0x40

/* Fault Event Upper Address register */
#define	DMAR_FEUADDR_REG	0x44

/* Advanced Fault Log register */
#define	DMAR_AFLOG_REG		0x58

/* Fault Recording Register, also usable for Advanced Fault Log records */
#define	DMAR_FRCD2_F		(1ULL << 63)	/* Fault */
#define	DMAR_FRCD2_F32		(1 << 31)
#define	DMAR_FRCD2_T(x)		((int)((x >> 62) & 1))	/* Type */
#define	DMAR_FRCD2_T_W		0		/* Write request */
#define	DMAR_FRCD2_T_R		1		/* Read or AtomicOp */
#define	DMAR_FRCD2_AT(x)	((int)((x >> 60) & 0x3)) /* Address Type */
#define	DMAR_FRCD2_FR(x)	((int)((x >> 32) & 0xff)) /* Fault Reason */
#define	DMAR_FRCD2_SID(x)	((int)(x & 0xffff))	/* Source Identifier */
#define	DMAR_FRCS1_FI_MASK	0xffffffffff000	/* Fault Info, Address Mask */

/* Protected Memory Enable register */
#define	DMAR_PMEN_REG		0x64
#define	DMAR_PMEN_EPM		(1 << 31)	/* Enable Protected Memory */
#define	DMAR_PMEN_PRS		1		/* Protected Region Status */

/* Protected Low-Memory Base register */
#define	DMAR_PLMBASE_REG	0x68

/* Protected Low-Memory Limit register */
#define	DMAR_PLMLIMIT_REG	0x6c

/* Protected High-Memory Base register */
#define	DMAR_PHMBASE_REG	0x70

/* Protected High-Memory Limit register */
#define	DMAR_PHMLIMIT_REG	0x78

/* Invalidation Queue Head register */
#define	DMAR_IQH_REG		0x80

/* Invalidation Queue Tail register */
#define	DMAR_IQT_REG		0x88

/* Invalidation Queue Address register */
#define	DMAR_IQA_REG		0x90

 /* Invalidation Completion Status register */
#define	DMAR_ICS_REG		0x9c
#define	DMAR_ICS_IWC		1		/* Invalidation Wait
						   Descriptor Complete */

/* Invalidation Event Control register */
#define	DMAR_IECTL_REG		0xa0
#define	DMAR_IECTL_IM		(1 << 31)	/* Interrupt Mask */
#define	DMAR_IECTL_IP		(1 << 30)	/* Interrupt Pending */

/* Invalidation Event Data register */
#define	DMAR_IEDATA_REG		0xa4

/* Invalidation Event Address register */
#define	DMAR_IEADDR_REG		0xa8

/* Invalidation Event Upper Address register */
#define	DMAR_IEUADDR_REG	0xac

/* Interrupt Remapping Table Address register */
#define	DMAR_IRTA_REG		0xb8

#endif
