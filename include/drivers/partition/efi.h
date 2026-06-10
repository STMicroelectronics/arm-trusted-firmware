/*
 * Copyright (c) 2021, Linaro Limited
 * Copyright (c) 2022, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef DRIVERS_PARTITION_EFI_H
#define DRIVERS_PARTITION_EFI_H

#include <string.h>

#include <tools_share/uuid.h>

#define EFI_NAMELEN		36

static inline int guidcmp(const void *g1, const void *g2)
{
	return memcmp(g1, g2, sizeof(struct efi_guid));
}

static inline void *guidcpy(void *dst, const void *src)
{
	return memcpy(dst, src, sizeof(struct efi_guid));
}

#define EFI_GUID(a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
	{ (a) & 0xffffffffU,		\
	  (b) & 0xffffU,			\
	  (c) & 0xffffU,			\
	  { (d0), (d1), (d2), (d3), (d4), (d5), (d6), (d7) } }

#define FWU_METADATA_GUID \
	EFI_GUID(0x8A7A84A0U, 0x8387U, 0x40F6U, \
		 0xABU, 0x41U, 0xA8U, 0xB9U, 0xA5U, 0xA6U, 0x0DU, 0x23U)

#define FWU_METADATA1_PART_GUID \
	EFI_GUID(0x5F91C128U, 0xE120U, 0x48D7U, \
		 0xBBU, 0x64U, 0x9FU, 0xA9U, 0xAEU, 0xD4U, 0xC7U, 0xE3U)

#define FWU_METADATA2_PART_GUID \
	EFI_GUID(0xF469F981U, 0x5985U, 0x4206U, \
		 0x8FU, 0xB3U, 0x83U, 0x99U, 0x56U, 0x03U, 0x5AU, 0x65U)

#define LINUX_FILE_SYSTEM_DATA_GUID \
	EFI_GUID(0x0FC63DAFU, 0x8483U, 0x4772U, \
		 0x8EU, 0x79U, 0x3DU, 0x69U, 0xD8U, 0x47U, 0x7DU, 0xE4U)

#define XBOOTLDR_GUID \
	EFI_GUID(0xBC13C2FFU, 0x59E6U, 0x4262U, \
		 0xA3U, 0x52U, 0xB2U, 0x75U, 0xFDU, 0x6FU, 0x71U, 0x72U)

#define NULL_GUID \
	EFI_GUID(0x00000000U, 0x0000U, 0x0000U, 0x00U, 0x00U, \
		 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U)

#endif /* DRIVERS_PARTITION_EFI_H */
