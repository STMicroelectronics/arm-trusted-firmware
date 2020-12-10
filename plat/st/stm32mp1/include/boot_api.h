/*
 * Copyright (c) 2017-2023, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOOT_API_H
#define BOOT_API_H

#include <stdint.h>
#include <stdio.h>

/*
 * Exported constants
 */

/*
 * Boot Context related definitions
 */

/*
 * Possible value of boot context field 'boot_action'
 */
#if STM32MP13
/* Boot action is Process Secure Boot */
#define BOOT_API_CTX_BOOT_ACTION_SECURE_BOOT_PROCESS		0x7879598EUL
/* Boot action is Process Wakeup from STANDBY  */
#define BOOT_API_CTX_BOOT_ACTION_WAKEUP_STANDBY			0x21276323UL
/* Boot action is Process Engineering Boot */
#define BOOT_API_CTX_BOOT_ACTION_ENGI_BOOT			0x4A05FE3AUL
/* Boot Action is MPU P0 Local reset */
#define BOOT_API_CTX_BOOT_ACTION_MPU_CORE0_RESET_PROCESS	0x0F6C12ABUL
#endif
#if STM32MP15
/* Boot action is Process Cold Boot */
#define BOOT_API_CTX_BOOT_ACTION_COLD_BOOT_PROCESS		0x09U
/* Boot action is Process Wakeup from CSTANDBY */
#define BOOT_API_CTX_BOOT_ACTION_WAKEUP_CSTANDBY		0x0AU
/* Boot action is Process Wakeup from STANDBY  */
#define BOOT_API_CTX_BOOT_ACTION_WAKEUP_STANDBY			0x0BU
/* Boot action is Process Engineering Boot */
#define BOOT_API_CTX_BOOT_ACTION_ENGI_BOOT			0x0CU
/* Boot Action is MPU P0 Local reset */
#define BOOT_API_CTX_BOOT_ACTION_MPU_CORE0_RESET_PROCESS	0x0F
#endif

/*
 * Possible value of boot context field 'stby_exit_status'
 */

/* The boot reason is not a STANDBY Exit reason */
#define BOOT_API_CTX_STBY_EXIT_STATUS_NO_STANDBY                0x00

/* STANDBY Exit with MPU_BEN=1, MCU_BEN=0 */
#define BOOT_API_CTX_STBY_EXIT_STATUS_WKUP_MPU_ONLY             0x01

/*
 * STANDBY Exit with MPU_BEN=1, MCU_BEN=1, MPU will go for cold boot
 * MCU restarted by bootROM
 */
#define BOOT_API_CTX_STBY_EXIT_STATUS_WKUP_ALL_CORES            0x02

/*
 * STANDBY Exit with MPU_BEN=1, MCU_BEN=1, MPU will go for cold boot
 * but MCU restart aborted (code integrity check) : have not been restarted
 * by bootROM
 */
#define BOOT_API_CTX_STBY_EXIT_STATUS_WKUP_ALL_CORES_MCU_ABT    0x03

/*
 * STANDBY Exit with MPU_BEN=0, MCU_BEN=1, MPU gone to CSTANDBY,
 * MCU restarted correctly by bootROM
 * This value should never be read by FSBL, because not executed in that case
 */
#define BOOT_API_CTX_STBY_EXIT_STATUS_WKUP_MCU_ONLY             0x04

/*
 * STANDBY Exit with MPU_BEN=0, MCU_BEN=1, MCU restart aborted
 * due code integrity check, then MPU will go for cold boot despite
 * was not planned initially
 */
#define BOOT_API_CTX_STBY_EXIT_STATUS_WKUP_MCU_ONLY_MCU_ABT     0x05

/*
 * STANDBY Exit with MPU_BEN=1, MCU_BEN=1, MCU restart aborted
 * due to MCU security perimeter issue
 */
#define \
BOOT_API_CTX_STBY_EXIT_STATUS_WKUP_ALL_CORES_MCU_ABT_SEC_PERIMETER_ISSUE 0x06

/*
 * STANDBY Exit with MPU_BEN=0, MCU_BEN=1, MCU restart aborted
 * due to MCU security perimeter issue, then MPU will go for cold boot
 * despite was not planned initially
 */
#define \
BOOT_API_CTX_STBY_EXIT_STATUS_WKUP_MCU_ONLY_MCU_ABT_SEC_PERIMETER_ISSUE	0x07

/*
 * Possible value of boot context field 'cstby_exit_status'
 */
/* The boot reason is not a CSTANDBY Exit reason */
#define BOOT_API_CTX_CSTBY_EXIT_STATUS_NO_CSTBY			0x00
/* CSTANDBY Exit with MCU detected as Not running */
#define BOOT_API_CTX_CSTBY_EXIT_STATUS_MCU_NOT_RUNNING		0x01
/* CSTANDBY Exit with MCU detected as Running  */
#define BOOT_API_CTX_CSTBY_EXIT_STATUS_MCU_RUNNING		0x02

/*
 * Possible value of boot context field 'auth_status'
 */
#if STM32MP13
 /* No authentication done */
#define BOOT_API_CTX_AUTH_NO					0x7CFDD351U
 /* Authentication done and failed */
#define BOOT_API_CTX_AUTH_FAILED				0x51330884U
 /* Authentication done and success */
#define BOOT_API_CTX_AUTH_SUCCESS				0x67E8CAE1U
#endif
#if STM32MP15
/* No authentication done */
#define BOOT_API_CTX_AUTH_NO					0x0U
/* Authentication done and failed */
#define BOOT_API_CTX_AUTH_FAILED				0x1U
/* Authentication done and succeeded */
#define BOOT_API_CTX_AUTH_SUCCESS				0x2U
#endif

/*
 * Possible value of boot context field 'boot_interface_sel'
 */

/* Value of field 'boot_interface_sel' when no boot occurred */
#define BOOT_API_CTX_BOOT_INTERFACE_SEL_NO			0x0U

/* Boot occurred on SD */
#define BOOT_API_CTX_BOOT_INTERFACE_SEL_FLASH_SD		0x1U

/* Boot occurred on EMMC */
#define BOOT_API_CTX_BOOT_INTERFACE_SEL_FLASH_EMMC		0x2U

/* Boot occurred on FMC */
#define BOOT_API_CTX_BOOT_INTERFACE_SEL_FLASH_NAND_FMC		0x3U

/* Boot occurred on QSPI NOR */
#define BOOT_API_CTX_BOOT_INTERFACE_SEL_FLASH_NOR_SPI		0x4U

/* Boot occurred on UART */
#define BOOT_API_CTX_BOOT_INTERFACE_SEL_SERIAL_UART		0x5U

/* Boot occurred on USB */
#define BOOT_API_CTX_BOOT_INTERFACE_SEL_SERIAL_USB		0x6U

/* Boot occurred on QSPI NAND */
#define BOOT_API_CTX_BOOT_INTERFACE_SEL_FLASH_NAND_SPI		0x7U

/**
 * @brief  Possible value of boot context field 'EmmcXferStatus'
 */
/*
 * Possible value of boot context field 'emmc_xfer_status'
 */
#define BOOT_API_CTX_EMMC_XFER_STATUS_NOT_STARTED			0x0U
#define BOOT_API_CTX_EMMC_XFER_STATUS_DATAEND_DETECTED			0x1U
#define BOOT_API_CTX_EMMC_XFER_STATUS_XFER_OVERALL_TIMEOUT_DETECTED	0x2U
#define BOOT_API_CTX_EMMC_XFER_STATUS_XFER_DATA_TIMEOUT			0x3U

/*
 * Possible value of boot context field 'emmc_error_status'
 */
#define BOOT_API_CTX_EMMC_ERROR_STATUS_NONE                     0x0U
#define BOOT_API_CTX_EMMC_ERROR_STATUS_CMD_TIMEOUT              0x1U
#define BOOT_API_CTX_EMMC_ERROR_STATUS_ACK_TIMEOUT              0x2U
#define BOOT_API_CTX_EMMC_ERROR_STATUS_DATA_CRC_FAIL            0x3U
#define BOOT_API_CTX_EMMC_ERROR_STATUS_NOT_ENOUGH_BOOT_DATA_RX  0x4U
#define BOOT_API_CTX_EMMC_ERROR_STATUS_HEADER_NOT_FOUND         0x5U
#define BOOT_API_CTX_EMMC_ERROR_STATUS_HEADER_SIZE_ZERO         0x6U
#define BOOT_API_CTX_EMMC_ERROR_STATUS_IMAGE_NOT_COMPLETE       0x7U
#define BOOT_API_CTX_EMMC_ERROR_STATUS_ACK_ERROR                0x8U

/* Definitions relative to 'p_rom_version_info->platform_type_ver' field */
#define BOOT_API_CTX_ROM_VERSION_PLAT_VER_IC_EMU_FPGA           0xAA
#define BOOT_API_CTX_ROM_VERSION_PLAT_VER_FPGA_ONLY             0xBB

/* Image Header related definitions */

/* Definition of header version */
#if STM32MP13
#define BOOT_API_HEADER_VERSION					0x00020000U
#endif
#if STM32MP15
#define BOOT_API_HEADER_VERSION					0x00010000U
#endif

/*
 * Magic number used to detect header in memory
 * Its value must be 'S' 'T' 'M' 0x32, i.e 0x324D5453 as field
 * 'bootapi_image_header_t.magic'
 * This identifies the start of a boot image.
 */
#define BOOT_API_IMAGE_HEADER_MAGIC_NB				0x324D5453U

/* Definitions related to Authentication used in image header structure */
#define BOOT_API_ECDSA_PUB_KEY_LEN_IN_BYTES			64
#define BOOT_API_ECDSA_SIGNATURE_LEN_IN_BYTES			64
#define BOOT_API_SHA256_DIGEST_SIZE_IN_BYTES			32

/* Possible values of the field 'boot_api_image_header_t.ecc_algo_type' */
#define BOOT_API_ECDSA_ALGO_TYPE_P256NIST			1
#define BOOT_API_ECDSA_ALGO_TYPE_BRAINPOOL256			2

/*
 * Extension headers related definitions
 */
/* 'bootapi_image_header_t.extension_flag' used for authentication feature */
#define BOOT_API_AUTHENTICATION_EXTENSION_BIT			BIT(0)
/* 'bootapi_image_header_t.extension_flag' used for FSBL decryption feature */
#define BOOT_API_FSBL_DECRYPTION_EXTENSION_BIT			BIT(1)
/* 'bootapi_image_header_t.extension_flag' used for padding header feature */
#define BOOT_API_PADDING_EXTENSION_BIT				BIT(31)
/*
 * mask of bits of field 'bootapi_image_header_t.extension_flag'
 * used for extension headers
 */
#define BOOT_API_ALL_EXTENSIONS_MASK \
	(BOOT_API_AUTHENTICATION_EXTENSION_BIT | \
	 BOOT_API_FSBL_DECRYPTION_EXTENSION_BIT | \
	 BOOT_API_PADDING_EXTENSION_BIT)
/*
 * Magic number of FSBL decryption extension header
 * The value shall gives the four bytes 'S','T',0x00,0x01 in memory
 */
#define BOOT_API_FSBL_DECRYPTION_HEADER_MAGIC_NB		0x01005453U

/*
 * Magic number of PKH revocation extension header
 * The value shall gives the four bytes 'S','T',0x00,0x02 in memory
 */
#define BOOT_API_AUTHENTICATION_HEADER_MAGIC_NB			0x02005453U

/* Max number of ECDSA public key hash in table */
#define BOOT_API_AUTHENTICATION_NB_PKH_MAX			8U

/* ECDSA public key hash table size in bytes */
#define BOOT_API_AUTHENTICATION_TABLE_SIZE_BYTES \
	(BOOT_API_AUTHENTICATION_NB_PKH_MAX * \
	 BOOT_API_SHA256_DIGEST_SIZE_IN_BYTES)

/*
 * Magic number of padding extension header
 * The value shall gives the four bytes 'S','T',0xFF,0xFF in memory
 */
#define BOOT_API_PADDING_HEADER_MAGIC_NB			0xFFFF5453U

/*
 * Cores secure magic numbers
 * Constant to be stored in bakcup register
 * BOOT_API_MAGIC_NUMBER_TAMP_BCK_REG_IDX
 */
#define BOOT_API_A7_CORE0_MAGIC_NUMBER				0xCA7FACE0U
#define BOOT_API_A7_CORE1_MAGIC_NUMBER				0xCA7FACE1U

/*
 * MCU Code Integrity Check related definitions
 */

/*
 * Defines to identify RTC backup registers to be used for MCU code integrity
 * check
 */

/*
 * TAMP_BCK0R contains two bits
 * bit 0 : wanted value of 'RCC_TZCR.TZEN'
 * bit 1 : wanted value of 'RCC_TZCR.MCKPROT'
 */

/*
 * TAMP_BCK0R bit position coding wanted value of 'RCC_TZCR.TZEN'
 * trustZone aware domain enabling/disabling
 */
#define BOOT_API_MCIC_MCU_SECURITY_PERIMETER_TZEN_BIT			0

/*
 * TAMP_BCK0R bit position coding wanted value of 'RCC_TZCR.MCKPROT'
 * ability of MCU to modify some clock settings in RCC
 */
#define BOOT_API_MCIC_MCU_SECURITY_PERIMETER_MCKPROT_BIT		1

/* TAMP_BCK0R register index */
#define \
BOOT_API_MCIC_MCU_SECURITY_PERIMETER_TZEN_MCKPROT_TAMP_BCK_REG_IDX	0

/*
 * TAMP_BCK1R register index
 * This register is coding the wanted value of register 'EXTI_TZENR1'
 * to be programmed by bootROM on wakeup from STANDBY when MCUBEN=1
 * that is MCU quick restart requested
 */
#define \
BOOT_API_MCIC_MCU_SECURITY_PERIMETER_EXTI_TZENR1_TAMP_BCK_REG_IDX	1

/*
 * TAMP_BCK2R register index
 * This register is coding the wanted value of register 'EXTI_TZENR2'
 * to be programmed by bootROM on wakeup from STANDBY when MCUBEN=1
 * that is MCU quick restart requested
 */
#define \
BOOT_API_MCIC_MCU_SECURITY_PERIMETER_EXTI_TZENR2_TAMP_BCK_REG_IDX	2

/*
 * TAMP_BCK3R register index
 * This register is coding the wanted value of register 'EXTI_TZENR3'
 * to be programmed by bootROM on wakeup from STANDBY when MCUBEN=1
 * that is MCU quick restart requested
 */
#define \
BOOT_API_MCIC_MCU_SECURITY_PERIMETER_EXTI_TZENR3_TAMP_BCK_REG_IDX	3

/*
 * TAMP_BCK4R register index
 * This register is used to write a Magic Number in order to restart
 * Cortex A7 Core 1 and make it execute @ branch address from TAMP_BCK5R
 */
#define BOOT_API_CORE1_MAGIC_NUMBER_TAMP_BCK_REG_IDX		4U

/*
 * TAMP_BCK5R register index
 * This register is used to contain the branch address of
 * Cortex A7 Core 1 when restarted by a TAMP_BCK4R magic number writing
 */
#define BOOT_API_CORE1_BRANCH_ADDRESS_TAMP_BCK_REG_IDX		5U

/*
 * TAMP_BCK22R register index
 * This register contains offset in bytes of code to Hash in RETRAM region
 * Note : offset is intended as relative value from start of RETRAM
 */
#define \
BOOT_API_MCIC_OFFSET_IN_BYTES_CODE_TO_HASH_RETRAM_TAMP_BCK_REG_IDX	22

/*
 * TAMP_BCK23R register index
 * This register contains the size in bytes of the single consecutive region
 * of MCU Firmware in RETRAM (Retention RAM) to hash (by SHA-256)
 * Note : This is required as a MCU firmware Code Integrity Check (aka : MCIC)
 * to avoid bootROM restarting MCU on a corrupted firmware
 */
#define \
BOOT_API_MCIC_RETRAM_REGION_TO_HASH_IN_BYTES_TAMP_BCK_REG_IDX		23

/*
 * TAMP_BCK24R to TAMP_BCK31R register indexes
 * Those registers contains SHA-256 digest of RETRAM MCU Firmware code between
 * [(RETRAM_start + offset) -- (RETRAM_start + offset + size_to_hash)]
 * in this order
 * This is the MCU Code Integrity Check MCU Firmware signature
 * value on 256 bits
 */

/* First TAMP_BKP index of MCU Firmware signature : ie TAMP_BCK24R */
#define BOOT_API_MCIC_SHA_DIGEST_FIRST_TAMP_BCK_REG_IDX			24

/* Last TAMP_BKP index of MCU Firmware signature : ie TAMP_BCK31R */
#define BOOT_API_MCIC_SHA_DIGEST_LAST_TAMP_BCK_REG_IDX			31

/*
 * Possible value of boot context field 'hse_clock_value_in_hz'
 */
#define BOOT_API_CTX_HSE_CLOCK_VALUE_UNDEFINED			0U
#define BOOT_API_CTX_HSE_CLOCK_VALUE_24_MHZ			24000000U
#define BOOT_API_CTX_HSE_CLOCK_VALUE_25_MHZ			25000000U
#define BOOT_API_CTX_HSE_CLOCK_VALUE_26_MHZ			26000000U

/*
 * Possible value of boot context field 'boot_partition_used_toboot'
 */
#define BOOT_API_CTX_BOOT_PARTITION_UNDEFINED			0U

/* Used FSBL1 to boot */
#define BOOT_API_CTX_BOOT_PARTITION_FSBL1			1U

/* Used FSBL2 to boot */
#define BOOT_API_CTX_BOOT_PARTITION_FSBL2			2U

/* OTP_CFG0 */
#define BOOT_API_OTP_MODE_WORD_NB				0
/* Closed = OTP_CFG0[6] */
#define BOOT_API_OTP_MODE_CLOSED_BIT_POS			6

#define BOOT_API_RETURN_OK					0x77U

/* Mapping of OTP Word and OTP bits managing SSP and useful to FSBL-SSP */
#if STM32MP13
/* OTP_CFG9 */
#define BOOT_API_OTP_SSP_WORD_NB				9U
/* SSP_REQ = OTP_CFG9[5] */
#define BOOT_API_OTP_SSP_REQ_BIT_POS				5
/* SSP_SUCCESS = OTP_CFG9[6] */
#define BOOT_API_OTP_SSP_SUCCESS_BIT_POS			6
#endif
#if STM32MP15
/* OTP_CFG8 */
#define BOOT_API_OTP_SSP_WORD_NB				8U
/* SSP_REQ = OTP_CFG8[8] */
#define BOOT_API_OTP_SSP_REQ_BIT_POS				8
/* SSP_SUCCESS = OTP_CFG8[9] */
#define BOOT_API_OTP_SSP_SUCCESS_BIT_POS			9
#endif

/*
 * Possible values of boot context field
 * 'ssp_config_ptr_in->ssp_cmd'
 */
/* 'K' 'B' 'U' 'P' -.> 'PUBK' */
#define BOOT_API_CTX_SSP_CMD_CALC_CHIP_PUBK             0x4B425550

#if STM32MP_SSP
/* 'V' 'O' 'R' 'P' -.> 'PROV' */
#define BOOT_API_CTX_SSP_CMD_PROV_SECRET		0x564F5250
/*
 * Possible values of boot context field
 * 'ssp_config_ptr_in->ssp_cmd' written by bootROM as Acknowledge
 * of a request of SSP by FSBL.
 */

/* Written by bootROM on SSP error */
#define BOOT_API_CTX_SSP_CMD_INVALID			0x00000000
/*
 * 'A' 'B' 'U' 'P' -.> 'PUBA' : ACK of ECIES_CHIP_PUBK calculation
 * request by bootROM.
 */
#define BOOT_API_CTX_SSP_CMD_CALC_CHIP_PUBK_ACK		0x41425550
/*
 * 'A' 'O' 'R' 'P' -.> 'PROA' : ACK of OEM Secret Provisioning request
 * by bootROM.
 */
#define BOOT_API_CTX_SSP_CMD_PROV_SECRET_ACK		0x414F5250

/*
 * Constants required for SSP
 */
/* '.' 'P' 'S' 'S' -.> 'SSP.' */
#define BOOT_API_SSP_BLOB_LICENSE_TYPE_SSP_NORMAL	0x2E505353
/* 'L' 'P' 'S' 'S' -.> 'SSPL' */
#define BOOT_API_SSP_BLOB_LICENSE_TYPE_SSP_LIVE		0x4C505353
/* version 1 */
#define BOOT_API_SSP_LICENSE_LAYOUT_VERSION_TO_MATCH	0x00000001
/* 'P' 'P' 'S' 'S' -.> 'SSPP' */
#define BOOT_API_SSP_BLOB_PAYLOAD_MAGIC_SSP		0x50505353
/* IV AES on 128 bits = 16 bytes and KEY AES on 128 bits = 16 bytes */
#define BOOT_API_SSP_ENCRYPTED_IV_AND_KEY_SIZE_BYTES	32
/* version 1 */
#define BOOT_API_SSP_PAYLOAD_PROTOCOL_VERSION_TO_MATCH	0x00000001
/*
 * Scalar in Elliptic curve cryptography is an integer (often a Prime)
 * the number of bytes of this scalar is defined below.
 */
#define BOOT_API_SSP_SCALAR_SIZE_BYTES				32

/*
 * In Elliptic curve cryptography coordinates of points are 2D P
 * (Px, Py) as concatenation of two scalars.
 */
#define BOOT_API_SSP_EC_COORDINATE_SIZE_BYTES \
	(2 * BOOT_API_SSP_SCALAR_SIZE_BYTES)

/* In Elliptic curve cryptography Private Keys are scalars */
#define BOOT_API_SSP_PRIVK_KEY_SIZE_BYTES \
	BOOT_API_SSP_SCALAR_SIZE_BYTES

/*
 * In ECIES algorithm the Shared Secret (SS) is
 * the x coordinate (Px) of a point P(Px,Py) obtained on reference
 * chosen NIST-P256 Elliptic curve.
 */
#define BOOT_API_SSP_ECDH_SHARED_SECRET_SIZE_BYTES \
	BOOT_API_SSP_SCALAR_SIZE_BYTES

/*
 * In Elliptic curve cryptography Public Keys are Points on chosen
 * Elliptic curve P(x,y).
 * Public Key is the x, y coordinates concatenated
 * Ecies_eph_pubk and OEM_ECDSA_PUBK are each 64 bytes = 512 bits key
 * sizes.
 */
#define BOOT_API_SSP_PUBK_KEY_SIZE_BYTES \
	BOOT_API_SSP_EC_COORDINATE_SIZE_BYTES

/*
 * Size in bytes of ECIES_Chip_pubk obtained from bootROM at end of SSP
 * phase 1 : Chip public key calculation.
 */
#define BOOT_API_SSP_ECIES_CHIP_PUBK_SIZE_BYTES \
	BOOT_API_SSP_PUBK_KEY_SIZE_BYTES

/* AES-GCM authentication tag size is 16 bytes = 128 bits */
#define BOOT_API_SSP_AES_GCM_AUTH_TAG_SIZE_BYTES		16

/* AES-GCM Symmetric Key size is 16 bytes = 128 bits */
#define BOOT_API_SSP_AES_GCM_KEY_SIZE_BYTES			16

/* AES-GCM Initialization Vector (IV) size is of 16 bytes = 128 bits */
#define BOOT_API_SSP_AES_GCM_IV_SIZE_BYTES			16

/*
 * 88 bytes (license_type, live_session_id, license_version,
 * fsbl_min_version, rfu[8], eph_ecies_pubk[])
 */
#define BOOT_API_SSP_AES_GCM_LICENSE_AAD_NB_BYTES_FROM_LICENSE	88

/*
 * 32 bytes AAD License Secret from 2nd round KDF-SHA-256
 * from ECDH Shared Secret hence KDF[32..63] aka "Authorization Token"
 */
#define BOOT_API_SSP_AES_GCM_LICENSE_AAD_NB_BYTES_FROM_KDF	32

/*
 * Total License AAD size = 88 + 32 = 120 bytes
 */
#define BOOT_API_SSP_AES_GCM_LICENSE_AAD_SIZE_BYTES \
	(BOOT_API_SSP_AES_GCM_LICENSE_AAD_NB_BYTES_FROM_LICENSE + \
	 BOOT_API_SSP_AES_GCM_LICENSE_AAD_NB_BYTES_FROM_KDF)

/*
 * AAD for Payload size : composed of :
 * payload_magic, payload_protocol_version, oem_ecdsa_pubk[], oem_secret_size
 * = 4 + 4 + 64 + 4 = 76 bytes AAD for Payload
 */
#define BOOT_API_SSP_AES_GCM_PAYLOAD_AAD_SIZE_BYTES		76

/*
 * OEM Secrets max size in bytes :
 * [OTP[95:59] + OTP_CFG56 (RMA Unlock and Relock passwords)] x 4 bytes
 * by OTP word = 152 bytes
 */
#define BOOT_API_SSP_OEM_SECRETS_MAX_SIZE_BYTES			152

/*
 * Possible values of boot context field 'ssp_status'
 * as can be read by FSBL-SSP
 */
#define BOOT_API_CTX_SSP_STATUS_NO_SSP				0
#define BOOT_API_CTX_SSP_STATUS_CHIP_PUBK_CALC_FINISHED		1
#define BOOT_API_CTX_SSP_STATUS_OEM_SEC_PROV_FINISHED		2
#define BOOT_API_CTX_SSP_STATUS_OEM_SEC_PROV_FORBIDDEN		3

/*
 * Reserved size for future use
 */
#define BOOT_API_SSP_HSM_OEM_RFU_SIZE				8

/*
 * Exported types
 */

/*
 * SSP related definitions
 */
/*
 * SSP BLOB License structure : Binary Large OBject License structure
 * Should be written by FSBL-SSP to provide bootROM with SSP OEM Secret
 * provisioning.
 * License information data, the structure is read by bootROM.
 */
typedef struct {
	/*
	 * License Type provided by HSM-OEM tool
	 * should match Normal SSP License of Live SSP License.
	 */
	uint32_t license_type;

	/* Live Session ID provided by HSM-OEM tool */
	uint32_t live_session_id;

	/*
	 * Version of the License Protocol (Structure)
	 * should be incremented each time a new.
	 */
	uint32_t license_version;

	/*
	 * Minimum FSBL version to be compared
	 * with FSBL Header field 'imageVersion'.
	 */
	uint32_t fsbl_min_version;

	/* RFU provided by HSM-OEM tool */
	uint8_t  rfu[BOOT_API_SSP_HSM_OEM_RFU_SIZE];

	/*
	 * Ephemeral ECIES Public Key from HSM-OEM
	 * 64 bytes = 512 bits.
	 */
	uint8_t  eph_ecies_pubk[BOOT_API_SSP_PUBK_KEY_SIZE_BYTES];

	/*
	 * Encrypted (IV,Key) : with Shared Secret based on
	 * 'Ephemeral ECIES Key pair' and 'ECIES Chip Key pair'.
	 */
	uint8_t encrypted_iv_and_key
		[BOOT_API_SSP_ENCRYPTED_IV_AND_KEY_SIZE_BYTES];

	/*
	 * AUTH_TAG AES-GCM from encryption of (IV, Key)
	 * in HSM-OEM with License AAD scheme
	 * License Tag is 16 bytes = 128 bits.
	 */
	uint8_t  license_tag[BOOT_API_SSP_AES_GCM_AUTH_TAG_SIZE_BYTES];

} boot_api_ssp_blob_license_t;

/*
 * SSP BLOB Payload structure : Binary Large OBject Payload Structure
 * Should be written by FSBL-SSP to provide bootROM with SSP OEM Secret
 * provisioning input data, the structure is read by bootROM
 * The BLOB Payload size is fixed to a max size of 244 bytes based
 * on a max number of bytes of OEM secret derived from OTP upper free
 * area in STM32MP15xx cut 2.0.In this table oem_encrypted_secrets[]
 * of max size only the first 'p_blob_payload->oem_secret_size_bytes'
 * bytes will be considered and used by bootROM.
 */
typedef struct {
	/*
	 * BLOB Payload Magic : for memory validity check of BLOB Payload
	 * to match against BOOT_API_SSP_BLOB_PAYLOAD_MAGIC_SSP by bootROM.
	 */
	uint32_t payload_magic;

	/*
	 * SSP Payload protocol version : on 32 bits
	 * to be checked by bootROM for equality with
	 * BOOT_API_SSP_PAYLOAD_PROTOCOL_VERSION_TO_MATCH
	 * ie : 0x00000001 : version 1 of SSP Payload
	 */
	uint32_t payload_protocol_version;

#if STM32MP13
	/* OEM_RPKTH OEM Root Public Key table hash is the
	 * hash[ hash(algo + pubK i) for i=0..N-1] where N
	 * is the number of OEM public keys in product for
	 * key revocation feature
	 *
	 * N = 8 for STM32MP13xx
	 * size of field = 32 bytes = 256 bits
	 */
	uint8_t  oem_rpkth[BOOT_API_SHA256_DIGEST_SIZE_IN_BYTES];
#endif
#if STM32MP15
	/*
	 * OEM_ECDSA_PUBK Public Key defined by OEM
	 * 64 bytes = 512 bits
	 */
	uint8_t  oem_ecdsa_pubk[BOOT_API_SSP_PUBK_KEY_SIZE_BYTES];
#endif

	/*
	 * Size of Table of OEM Secrets encrypted with AES-GCM (Key,IV) from
	 * License field 'encrypted_iv_and_key[]'
	 * should be <= BOOT_API_SSP_OEM_SECRETS_MAX_SIZE_BYTES:
	 * is verified by bootROM.
	 */
	uint32_t oem_secret_size_bytes;

	/*
	 * AUTH_TAG AES-GCM computed by HSM-OEM when encrypting OEM Secrets with
	 * (Key,IV) using special AAD scheme for Payload.
	 * 16 bytes = 128 bits
	 */
	uint8_t  payload_tag[BOOT_API_SSP_AES_GCM_AUTH_TAG_SIZE_BYTES];

	/*
	 * OEM Secrets encrypted with AES-GCM (IV, Key) from
	 * License field 'encrypted_iv_and_key[]'.
	 * The payload size is 'oem_secret_size_bytes'
	 * should be <= BOOT_API_SSP_OEM_SECRETS_MAX_SIZE_BYTES =
	 *  152 bytes : OEM Secrets max size in bytes :
	 * [OTP_CFG56, OTP_CFG59, OTP_CFG60..95] x 4 bytes by OTP word.
	 */
	uint8_t oem_encrypted_secrets[BOOT_API_SSP_OEM_SECRETS_MAX_SIZE_BYTES];

} boot_api_ssp_blob_payload_t;
#endif

/* SSP Configuration structure */
typedef struct {
	/* SSP Command*/
	uint32_t ssp_cmd;
#if STM32MP_SSP
	/* ECIES chip public key */
	uint8_t *p_chip_pubk;
	/* Blob License Address */
	boot_api_ssp_blob_license_t *p_blob_license;
	/* Blob Payload Address */
	boot_api_ssp_blob_payload_t *p_blob_payload;
	/* Secrets Decrypted Address */
	uint8_t *p_ssp_oem_secrets_decrypted;
	/* Reserved for Future Use (RFU) */
	uint32_t padding_rfu;
#else
	uint8_t	reserved[20];
#endif
} boot_api_ssp_config_t;

/*
 * bootROM version information structure definition
 * Total size = 24 bytes = 6 uint32_t
 */
typedef struct {
	/* Chip Version */
	uint32_t chip_ver;

	/* Cut version within a fixed chip version */
	uint32_t cut_ver;

	/* Version of ROM Mask within a fixed cut version */
	uint32_t rom_mask_ver;

	/* Internal Version of bootROM code */
	uint32_t bootrom_ver;

	/* Version of bootROM adapted */
	uint32_t for_chip_design_rtl_ver;

	/* Restriction on compiled platform when it applies */
	uint32_t platform_type_ver;

} boot_api_rom_version_info_t;

/*
 * Boot Context related definitions
 */

/*
 * Boot core boot configuration structure
 * Specifies all items of the cold boot configuration
 * Memory and peripheral part.
 */
typedef struct {
	/*
	 * Boot interface used to boot : take values from defines
	 * BOOT_API_CTX_BOOT_INTERFACE_SEL_XXX above
	 */
	uint16_t boot_interface_selected;
	uint16_t boot_interface_instance;
#if STM32MP13
	uint32_t reserved1[12];
#endif
#if STM32MP15
	uint32_t reserved1[13];
#endif
	uint32_t otp_afmux_values[3];
	uint32_t reserved[2];
	/*
	 * Log to boot context, what was the kind of boot action
	 * takes values from defines BOOT_API_BOOT_ACTION_XXX above
	 */
	uint32_t boot_action;
#if STM32MP15
	/*
	 * STANDBY Exit status to be checked by FSBL in case
	 * field 'boot_action' == BOOT_API_CTX_BOOT_ACTION_WAKEUP_STANDBY
	 * take values from defines above 'BOOT_API_CTX_STBY_EXIT_STATUS_XXX'
	 * depending on encountered situation
	 */
	uint32_t stby_exit_status;
	/*
	 * CSTANDBY Exit status to be checked by FSBL in case
	 * boot_action == BOOT_API_CTX_BOOT_ACTION_WAKEUP_CSTANDBY
	 * take values from defines above 'BOOT_API_CTX_CSTBY_EXIT_STATUS_XXX'
	 * depending on encountered situation
	 */
	uint32_t cstby_exit_status;
#endif
	uint32_t auth_status;

#if STM32MP15
	/*
	 * Pointers to bootROM External Secure Services
	 * - ECDSA verify signature
	 */
	uint32_t reserved3;
	uint32_t (*bootrom_ecdsa_verify_signature)(uint8_t *hash_in,
						   uint8_t *pubkey_in,
						   uint8_t *signature,
						   uint32_t ecc_algo);
	uint32_t reserved4;
#endif
	/*
	 * Information specific to an SD boot
	 * Updated each time an SD boot is at least attempted,
	 * even if not successful
	 * Note : This is useful to understand why an SD boot failed
	 * in particular
	 */
	uint32_t sd_err_internal_timeout_cnt;
	uint32_t sd_err_dcrc_fail_cnt;
	uint32_t sd_err_dtimeout_cnt;
	uint32_t sd_err_ctimeout_cnt;
	uint32_t sd_err_ccrc_fail_cnt;
	uint32_t sd_overall_retry_cnt;
	/*
	 * Information specific to an eMMC boot
	 * Updated each time an eMMC boot is at least attempted,
	 * even if not successful
	 * Note : This is useful to understand why an eMMC boot failed
	 * in particular
	 */
	uint32_t emmc_xfer_status;
	uint32_t emmc_error_status;
	uint32_t emmc_nbbytes_rxcopied_tosysram_download_area;
	uint32_t hse_clock_value_in_hz;
	/*
	 * Boot partition :
	 * ie FSBL partition on which the boot was successful
	 */
	uint32_t boot_partition_used_toboot;
	/*
	 * Address of SSP configuration structure :
	 * given and defined by bootROM
	 * and used by FSBL. The structure is of type
	 * 'boot_api_ssp_config_t'
	 */
	boot_api_ssp_config_t *p_ssp_config;
	/*
	 * boot context field containing bootROM updated SSP Status
	 * Values can be of type BOOT_API_CTX_SSP_STATUS_XXX
	 */
	uint32_t	ssp_status;

	/* Pointer on ROM constant containing ROM information */
	const boot_api_rom_version_info_t *p_rom_version_info;

} __packed boot_api_context_t;

/*
 * Image Header related definitions
 */

/*
 * Structure used to define the common Header format used for FSBL, xloader,
 * ... and in particular used by bootROM for FSBL header readout.
 * FSBL header size is 256 Bytes = 0x100
 */
typedef struct {
	/* BOOT_API_IMAGE_HEADER_MAGIC_NB */
	uint32_t magic;
	uint8_t image_signature[BOOT_API_ECDSA_SIGNATURE_LEN_IN_BYTES];
	/*
	 * Checksum of payload
	 * 32-bit sum all payload bytes considered as 8 bit unsigned
	 * numbers, discarding any overflow bits.
	 * Use to check UART/USB downloaded image integrity when signature
	 * is not used
	 */
	uint32_t payload_checksum;
	/* Image header version : should have value BOOT_API_HEADER_VERSION */
	uint32_t header_version;
	/* Image length in bytes */
	uint32_t image_length;
	/*
	 * Image Entry point address : should be in the SYSRAM area
	 * and at least within the download area range
	 */
	uint32_t image_entry_point;
	/* Reserved */
	uint32_t reserved1;
	/*
	 * Image load address : not used by bootROM but to be consistent
	 * with header format for other packages (xloader, ...)
	 */
	uint32_t load_address;
	/* Reserved */
	uint32_t reserved2;
	/* Image version to be compared by bootROM with monotonic
	 * counter value in OTP_CFG4 prior executing the downloaded image
	 */
	uint32_t image_version;

#if STM32MP13
	/*
	 * Extension flags :
	 *
	 * Bit 0 : Authentication extension header
	 *      value 0 : No signature check request
	 * Bit 1 : Encryption extension header
	 * Bit 2 : Padding extension header
	 */
	uint32_t extension_flags;
	/* Length in bytes of all extension headers */
	uint32_t extension_headers_length;
	/* Add binary type information */
	uint32_t binary_type;
	/* Pad up to 128 byte total size */
	uint8_t pad[16];
	/* Followed by extension header */
	uint8_t ext_header[];
#endif
#if STM32MP15
	/*
	 * Option flags:
	 * Bit 0 : No signature check request : 'No_sig_check'
	 *      value 1 : for No signature check request
	 *      value 0 : No request to bypass the signature check
	 * Note : No signature check is never allowed on a Secured chip
	 */
	uint32_t option_flags;
	/*
	 * Type of ECC algorithm to use  :
	 * value 1 : for P-256 NIST algorithm
	 * value 2 : for Brainpool 256 algorithm
	 * See definitions 'BOOT_API_ECDSA_ALGO_TYPE_XXX' above.
	 */
	uint32_t ecc_algo_type;
	/*
	 * OEM ECC Public Key (aka Root pubk) provided in header on 512 bits.
	 * The SHA-256 hash of the OEM ECC pubk must match the one stored
	 * in OTP cells.
	 */
	uint8_t ecc_pubk[BOOT_API_ECDSA_PUB_KEY_LEN_IN_BYTES];
	/* Pad up to 256 byte total size */
	uint8_t pad[83];
	/* Add binary type information */
	uint8_t binary_type;
#endif
} __packed boot_api_image_header_t;

typedef uint8_t boot_api_sha256_t[BOOT_API_SHA256_DIGEST_SIZE_IN_BYTES];

typedef struct {
	/* Extension header type:
	 * BOOT_API_FSBL_DECRYPTION_HEADER_MAGIC_NB or
	 * BOOT_API_AUTHENTICATION_HEADER_MAGIC_NB
	 * BOOT_API_PADDING_HEADER_MAGIC_NB
	 */
	uint32_t type;
	/* Extension header len in byte */
	uint32_t len;
	/* parameters of this extension */
	uint8_t  params[];
} __packed boot_extension_header_t;

typedef struct {
	/* Idx of ECDSA public key to be used in table */
	uint32_t pk_idx;
	/* Number of ECDSA public key in table */
	uint32_t nb_pk;
	/*
	 * Type of ECC algorithm to use  :
	 * value 1 : for P-256 NIST algorithm
	 * value 2 : for Brainpool 256 algorithm
	 * See definitions 'BOOT_API_ECDSA_ALGO_TYPE_XXX' above.
	 */
	uint32_t ecc_algo_type;
	/* ECDSA public key to be used to check signature. */
	uint8_t ecc_pubk[BOOT_API_ECDSA_PUB_KEY_LEN_IN_BYTES];
	/* table of Hash of Algo+ECDSA public key */
	boot_api_sha256_t pk_hashes[];
} __packed boot_ext_header_params_authentication_t;

typedef struct {
	/* Size of encryption key (128 or 256) */
	uint32_t key_size;
	uint32_t derivation_cont;
	/* 128 msb bits of plain payload SHA256 */
	uint32_t hash[4];
} __packed boot_ext_header_params_encrypted_fsbl_t;

#endif /* BOOT_API_H */
