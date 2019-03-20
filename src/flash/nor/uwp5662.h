/***************************************************************************
 *   Copyright (C) 2019 by UNISOC                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef UWP5662_H
#define UWP5662_H

#include "imp.h"
#include <helper/binarybuffer.h>
#include <target/algorithm.h>
#include <target/armv7m.h>
#include <stdlib.h>

#define NEW_WRITE_PAGE

#define min(a, b) (((a) < (b)) ? (a) : (b))

#define BIT(x) (1 << (x))

#define TRUE        1
#define FALSE       0

#define SPI_MODE                0x0
#define QPI_MODE                0x1

#define READ_FREQ_104M          1
#define READ_FREQ_133M          2

#define DUMMY_0CLOCKS           0
#define DUMMY_2CLOCKS           1
#define DUMMY_4CLOCKS           2
#define DUMMY_6CLOCKS           3
#define DUMMY_8CLOCKS           4

#define STATUS_WIP              BIT(0)
#define STATUS_WEL              BIT(1)
#define STATUS_ADS		BIT(0)

#define CONFIG_SYS_HZ                       1000
#define SFC_DRVREQ_TIMEOUT                  (1   * CONFIG_SYS_HZ)
#define SPI_FLASH_WEL_TIMEOUT               (2   * CONFIG_SYS_HZ)
#define SPI_FLASH_ADS_TIMEOUT               (2   * CONFIG_SYS_HZ)
#define SPI_FLASH_PAGE_PROG_TIMEOUT         (20  * CONFIG_SYS_HZ)
#define SPI_FLASH_SECTOR_ERASE_TIMEOUT      (125 * CONFIG_SYS_HZ)

#define UWP5662_FLASH_BASE_ADDRESS    0x2000000

#define BASE_AON_GPIO1_REG      0x40808000
#define BASE_AON_GLB_REG        0x4083C000
#define BASE_AON_PIN_REG        0x40840000
#define BASE_AON_CLK_REG        0x40844200
#define BASE_AON_SFC_CFG        0x40890000

#define REG_AON_GPIO1_RF_GPIO_VAL           (BASE_AON_GPIO1_REG + 0x0000)
#define REG_AON_GPIO1_RF_GPIO_MSK           (BASE_AON_GPIO1_REG + 0x0004)
#define REG_AON_GPIO1_RF_GPIO_DIR           (BASE_AON_GPIO1_REG + 0x0008)
#define REG_AON_GLB_RF_GPIO_MODE1           (BASE_AON_GLB_REG + 0x020c)
#define REG_AON_GLB_RF_APB_EB_SET           (BASE_AON_GLB_REG + 0x1024)
#define REG_AON_PIN_RF_ESMCSN_CFG           (BASE_AON_PIN_REG + 0x0090)
#define REG_AON_CLK_RF_CGM_ARM_CFG          (BASE_AON_CLK_REG + 0x0020)
#define REG_AON_CLK_RF_CGM_MTX_CFG          (BASE_AON_CLK_REG + 0x0024)
#define REG_AON_CLK_RF_CGM_SFC_2X_CFG       (BASE_AON_CLK_REG + 0x002c)
#define REG_AON_CLK_RF_CGM_SFC_1X_CFG       (BASE_AON_CLK_REG + 0x0030)

#define SFC_CMD_CFG         (BASE_AON_SFC_CFG + 0x0000)
#define SFC_SOFT_REQ        (BASE_AON_SFC_CFG + 0x0004)
#define SFC_TBUF_CLR        (BASE_AON_SFC_CFG + 0x0008)
#define SFC_INT_CLR         (BASE_AON_SFC_CFG + 0x000C)
#define SFC_STATUS          (BASE_AON_SFC_CFG + 0x0010)
#define SFC_CS_TIMING_CFG   (BASE_AON_SFC_CFG + 0x0014)
#define SFC_RD_SAMPLE_CFG   (BASE_AON_SFC_CFG + 0x0018)
#define SFC_CLK_CFG         (BASE_AON_SFC_CFG + 0x001C)
#define SFC_CS_CFG          (BASE_AON_SFC_CFG + 0x0020)
#define SFC_ENDIAN_CFG      (BASE_AON_SFC_CFG + 0x0024)
#define SFC_IO_DLY_CFG      (BASE_AON_SFC_CFG + 0x0028)
#define SFC_WP_HLD_INIT     (BASE_AON_SFC_CFG + 0x002C)
#define SFC_CMD_BUF0        (BASE_AON_SFC_CFG + 0x0040)
#define SFC_CMD_BUF1        (BASE_AON_SFC_CFG + 0x0044)
#define SFC_CMD_BUF2        (BASE_AON_SFC_CFG + 0x0048)
#define SFC_CMD_BUF3        (BASE_AON_SFC_CFG + 0x004C)
#define SFC_CMD_BUF4        (BASE_AON_SFC_CFG + 0x0050)
#define SFC_CMD_BUF5        (BASE_AON_SFC_CFG + 0x0054)
#define SFC_CMD_BUF6        (BASE_AON_SFC_CFG + 0x0058)
#define SFC_CMD_BUF7        (BASE_AON_SFC_CFG + 0x005C)
#define SFC_CMD_BUF8        (BASE_AON_SFC_CFG + 0x0060)
#define SFC_CMD_BUF9        (BASE_AON_SFC_CFG + 0x0064)
#define SFC_CMD_BUF10       (BASE_AON_SFC_CFG + 0x0068)
#define SFC_CMD_BUF11       (BASE_AON_SFC_CFG + 0x006C)
#define SFC_TYPE_BUF0       (BASE_AON_SFC_CFG + 0x0070)
#define SFC_TYPE_BUF1       (BASE_AON_SFC_CFG + 0x0074)
#define SFC_TYPE_BUF2       (BASE_AON_SFC_CFG + 0x0078)

#define SFC_IEN             (BASE_AON_SFC_CFG + 0x0204)
#define SFC_INT_RAW         (BASE_AON_SFC_CFG + 0x0208)
#define SFC_INT_STS         (BASE_AON_SFC_CFG + 0x020C)

/* SFC_CMD_CFG */
#define SHIFT_CMD_SET                   0
#define SHIFT_RDATA_BIT_MODE            1
#define SHIFT_STS_INI_ADDR_SEL          3
#define MSK_STS_INI_ADDR_SEL            (3<<3)

/* SFC_SOFT_REQ */
#define SHIFT_SOFT_REQ                  0

/* SFC_TBUF_CLR */
#define SHIFT_TBUF_CLR                  0

/* SFC_INT_CLR */
#define SHIFT_INT_CLR                   0

/* SFC_CLK_CFG */
#define SHIFT_CLK_DIV_MODE              0
#define SHIFT_CLK_POLARITY              2
#define SHIFT_CLK_OUT_DLY_INV           3
#define SHIFT_CLK_OUT_DLY_SEL           4
#define SHIFT_CLK_SAMPLE_DLY_INV        8
#define SHIFT_CLK_SAMPLE_DLY_SEL        9
#define SHIFT_CLK_OUT_EN_DLY_INV        13

/* SFC_CLK_CFG */
#define SFC_CLK_OUT_DIV_1               (0x0)
#define SFC_CLK_OUT_DIV_2               BIT(0)
#define SFC_CLK_OUT_DIV_4               BIT(1)
#define SFC_CLK_SAMPLE_DELAY_SEL        BIT(2)
#define SFC_CLK_SAMPLE_2X_EN            BIT(7)
#define SFC_CLK_SAMPLE_2X_PHASE_1       BIT(8)
#define SFC_CLK_OUT_2X_EN               BIT(9)
#define SFC_CLK_2X_EN                   BIT(10)

/* SFC_TYPE_BUF0 */
#define VALID0                          BIT(0)
#define SHIFT_BIT_MODE0                 1
#define SHIFT_BYTE_NUM0                 3
#define SHIFT_OPERATION_STATUS0         5
#define SHIFT_BYTE_SEND_MODE0           7

#define VALID1                          BIT(8)
#define SHIFT_BIT_MODE1                 9
#define SHIFT_BYTE_NUM1                 11
#define SHIFT_OPERATION_STATUS1         13
#define SHIFT_BYTE_SEND_MODE1           15

#define VALID2                          BIT(16)
#define SHIFT_BIT_MODE2                 17
#define SHIFT_BYTE_NUM2                 19
#define SHIFT_OPERATION_STATUS2         21
#define SHIFT_BYTE_SEND_MODE2           23

#define VALID3                          BIT(24)
#define SHIFT_BIT_MODE3                 25
#define SHIFT_BYTE_NUM3                 27
#define SHIFT_OPERATION_STATUS3         29
#define SHIFT_BYTE_SEND_MODE3           31

/* SFC_TYPE_BUF1 */
#define VALID4                          BIT(0)
#define SHIFT_BIT_MODE4                 1
#define SHIFT_BYTE_NUM4                 3
#define SHIFT_OPERATION_STATUS4         5
#define SHIFT_BYTE_SEND_MODE4           7

#define VALID5                          BIT(8)
#define SHIFT_BIT_MODE5                 9
#define SHIFT_BYTE_NUM5                 11
#define SHIFT_OPERATION_STATUS5         13
#define SHIFT_BYTE_SEND_MODE5           15

#define VALID6                          BIT(16)
#define SHIFT_BIT_MODE6                 17
#define SHIFT_BYTE_NUM6                 19
#define SHIFT_OPERATION_STATUS6         21
#define SHIFT_BYTE_SEND_MODE6           23

#define VALID7                          BIT(24)
#define SHIFT_BIT_MODE7                 25
#define SHIFT_BYTE_NUM7                 27
#define SHIFT_OPERATION_STATUS7         29
#define SHIFT_BYTE_SEND_MODE7           31

/* SFC_TYPE_BUF2 */
#define VALID8                          BIT(0)
#define SHIFT_BIT_MODE8                 1
#define SHIFT_BYTE_NUM8                 3
#define SHIFT_OPERATION_STATUS8         5
#define SHIFT_BYTE_SEND_MODE8           7

#define VALID9                          BIT(8)
#define SHIFT_BIT_MODE9                 9
#define SHIFT_BYTE_NUM9                 11
#define SHIFT_OPERATION_STATUS9         13
#define SHIFT_BYTE_SEND_MODE9           15

#define VALID10                         BIT(16)
#define SHIFT_BIT_MODE10                17
#define SHIFT_BYTE_NUM10                19
#define SHIFT_OPERATION_STATUS10        21
#define SHIFT_BYTE_SEND_MODE10          23

#define VALID11                         BIT(24)
#define SHIFT_BIT_MODE11                25
#define SHIFT_BYTE_NUM11                27
#define SHIFT_OPERATION_STATUS11        29
#define SHIFT_BYTE_SEND_MODE11          31

#define CMD_READ_ID             0x9f
#define CMD_RSTEN               0x66
#define CMD_RST                 0x99
#define CMD_PE_SUSPEND          0x75
#define CMD_PE_RESUME           0x7A
#define CMD_ENTER_QPI           0x38
#define CMD_EXIT_QPI            0xFF
#define CMD_WRITE_DISABLE       0x04
#define CMD_WRITE_ENABLE        0x06
#define CMD_NORMAL_READ         0x03
#define CMD_FAST_READ           0x0B
#define CMD_READ_1_1_2          0x3B
#define CMD_READ_1_1_4          0x6B
#define CMD_2IO_READ            0xBB
#define CMD_4IO_READ            0xEB

#define CMD_ENTER_4ADDR         0xB7
#define CMD_EXIT_4ADDR          0xE9

#define CMD_WRITE_STATUS        0x01
#define CMD_READ_STATUS1        0x05
#define CMD_READ_STATUS2        0x35
#define CMD_READ_STATUS3        0x15

#define CMD_PAGE_PROGRAM        0x02

#define CMD_SECTOR_ERASE        0x20
#define CMD_CHIP_ERASE          0xC7

#define CMD_SETBURST            0xC0

#define RD_CMD_1BIT             (0x00 << 0)
#define RD_CMD_2BIT             (0x01 << 0)
#define RD_CMD_4BIT             (0x02 << 0)
#define RD_CMD_MSK              (0x03 << 0)

#define RD_ADDR_1BIT            (0x00 << 2)
#define RD_ADDR_2BIT            (0x01 << 2)
#define RD_ADDR_4BIT            (0x02 << 2)
#define RD_ADDR_MSK             (0x03 << 2)

#define RD_DUMY_1BIT            (0x00 << 4)
#define RD_DUMY_2BIT            (0x01 << 4)
#define RD_DUMY_4BIT            (0x02 << 4)
#define RD_DUMY_MSK             (0x03 << 4)

#define RD_DATA_1BIT            (0x00 << 6)
#define RD_DATA_2BIT            (0x01 << 6)
#define RD_DATA_4BIT            (0x02 << 6)
#define RD_DATA_MSK             (0x03 << 6)

#define WR_CMD_1BIT             (0x00 << 8)
#define WR_CMD_2BIT             (0x01 << 8)
#define WR_CMD_4BIT             (0x02 << 8)
#define WR_CMD_MSK              (0x03 << 8)

#define WR_ADDR_1BIT            (0x00 << 10)
#define WR_ADDR_2BIT            (0x01 << 10)
#define WR_ADDR_4BIT            (0x02 << 10)
#define WR_ADDR_MSK             (0x03 << 10)

#define WR_DATA_1BIT            (0x00 << 14)
#define WR_DATA_2BIT            (0x01 << 14)
#define WR_DATA_4BIT            (0x02 << 14)
#define WR_DATA_MSK             (0x03 << 14)

typedef enum CMD_MODE_E_TAG {
	CMD_MODE_WRITE = 0,
	CMD_MODE_READ,
	CMD_MODE_HIGHZ,
	CMD_MODE_MAX
} CMD_MODE_E;

typedef enum SEND_MODE_E_TAG {
	SEND_MODE_0 = 0,
	SEND_MODE_1,
	SEND_MODE_MAX
} SEND_MODE_E;

typedef enum BIT_MODE_E_TAG {
	BIT_MODE_1 = 0,
	BIT_MODE_2,
	BIT_MODE_4,
	BIT_MODE_MAX
} BIT_MODE_E;

typedef enum BYTE_NUM_E_TAG {
	BYTE_NUM_1 = 0,
	BYTE_NUM_2,
	BYTE_NUM_3,
	BYTE_NUM_4,
	BYTE_NUM_MAX
} BYTE_NUM_E;

typedef enum INI_ADD_SEL_E_TAG {
	INI_CMD_BUF_7 = 0,
	INI_CMD_BUF_6,
	INI_CMD_BUF_5,
	INI_CMD_BUF_4,
	INI_CMD_BUF_MAX
} INI_ADD_SEL_E;

typedef enum CMD_BUF_INDEX_E_TAG {
	CMD_BUF_0 = 0,
	CMD_BUF_1,
	CMD_BUF_2,
	CMD_BUF_3,
	CMD_BUF_4,
	CMD_BUF_5,
	CMD_BUF_6,
	CMD_BUF_7,
	CMD_BUF_8,
	CMD_BUF_9,
	CMD_BUF_10,
	CMD_BUF_11,
	CMD_BUF_MAX
} CMD_BUF_INDEX_E;

typedef enum INFO_BUF_INDEX_E_TAG {
	INFO_BUF_0 = CMD_BUF_MAX,
	INFO_BUF_1,
	INFO_BUF_2,
	INFO_BUF_MAX
} INFO_BUF_INDEX_E;

typedef enum READ_CMD_TYPE_E_TAG {
	READ_SPI = 0,
	READ_SPI_FAST,
	READ_SPI_2IO,
	READ_SPI_4IO,
	READ_QPI_FAST,
	READ_QPI_4IO,
} READ_CMD_TYPE_E;

typedef struct _sfc_cmd_des {
	uint32_t cmd;
	uint32_t cmd_byte_len;
	uint32_t is_valid;
	CMD_MODE_E cmd_mode;
	BIT_MODE_E bit_mode;
	SEND_MODE_E send_mode;
} SFC_CMD_DES_T;

struct uwp_flash_param {
	uint16_t idcode1;
	uint16_t idcode2;
	uint16_t page_size;
	uint16_t sector_size;
	uint16_t nr_sectors;
	uint16_t nr_blocks;
	uint16_t support_qpi;
	uint16_t read_freq_max;
	uint16_t dummy_clocks;
	const char *name;
};

struct uwp_flash {
	uint32_t cs;
	const char *name;
	uint32_t size;
	uint32_t page_size;
	uint32_t sector_size;
	uint32_t dummy_clocks;
	uint8_t work_mode;
	uint8_t support_4addr;
	int spi_rw_mode;
};

struct uwp5662_flash_bank {
	int probed;
	uint32_t id;
	const struct uwp_flash_param *param;
	struct uwp_flash flash;
};

#define GIGA_MFID                   0XC8
#define WINBOND_MFID                0XEF

#define WINBOND_ID_W25X16           0x3015
#define WINBOND_ID_W25X32           0x3016
#define WINBOND_ID_W25X64           0x3017
#define WINBOND_ID_W25Q32DW         0x6016
#define WINBOND_ID_W25Q32JW         0x8016
#define WINBOND_ID_W25Q64FW         0x6017
#define WINBOND_ID_W25Q128FW        0x6018
#define WINBOND_ID_W25Q16           0x4015
#define WINBOND_ID_W25Q32FV         0x4016
#define WINBOND_ID_W25Q64FV         0x4017
#define WINBOND_ID_W25Q128FV        0x4018
#define WINBOND_ID_W25Q256FV        0x4019

static struct uwp_flash_param winbond_flash_table[] = {
	{
		WINBOND_ID_W25Q32DW,
		0,
		256,
		16,
		16,
		64,
		QPI_MODE,
		READ_FREQ_104M,
		DUMMY_8CLOCKS,
		"W25Q32DW",
	},
	{
		WINBOND_ID_W25Q32JW,
		0,
		256,
		16,
		16,
		64,
		SPI_MODE,
		READ_FREQ_104M,
		DUMMY_4CLOCKS,
		"W25Q32JW",
	},
};

struct spi_flash_spec_s {
	uint8_t manufacturer_id;
	uint16_t table_num;
	struct uwp_flash_param *table;
};

static struct spi_flash_spec_s spi_flash_spec_table[] = {
	{
		WINBOND_MFID,
		ARRAY_SIZE(winbond_flash_table),
		winbond_flash_table,
	},
};

#endif /* UWP5662_H */
