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

#include <helper/types.h>
#include "uwp5661.h"

static uint32_t cmd_info_buf_cache[INFO_BUF_MAX] = {0};
static uint32_t prev_cmd_info_buf_cache[INFO_BUF_MAX] = {0};
static uint32_t cmd_buf_cache_bitmap  = 0xFFF;
static uint32_t sfc_cmd_cfg_cache = 0xFFFFFFFF;
static uint32_t xip_sys_clk_cfg[2] = {0x00000100, 0x00000005};

#ifdef NEW_WRITE_PAGE
static void uwp5661_init_sw_csn(struct target *target)
{
	uint32_t rd_dat = 0;
	/* GPIO init for faster programming */
	/* GPIO_MODE1 */
	target_read_u32(target, REG_AON_GLB_RF_GPIO_MODE1, &rd_dat);
	rd_dat &= (~(BIT(12+16)));
	target_write_u32(target, REG_AON_GLB_RF_GPIO_MODE1, rd_dat);
	/* GPIO1_EB */
	target_write_u32(target, REG_AON_GLB_RF_APB_EB_SET, BIT(13));
	/* GPIO bit28 */
	/* Dir */
	target_read_u32(target, REG_AON_GPIO1_RF_GPIO_DIR, &rd_dat);
	rd_dat |= BIT(12);
	target_write_u32(target, REG_AON_GPIO1_RF_GPIO_DIR, rd_dat);
	/* Mask */
	target_read_u32(target, REG_AON_GPIO1_RF_GPIO_MSK, &rd_dat);
	rd_dat |= BIT(12);
	target_write_u32(target, REG_AON_GPIO1_RF_GPIO_MSK, rd_dat);
	/* Value */
	target_read_u32(target, REG_AON_GPIO1_RF_GPIO_VAL, &rd_dat);
	rd_dat &= (~BIT(12));
	target_write_u32(target, REG_AON_GPIO1_RF_GPIO_VAL, rd_dat);
}

static void uwp5661_force_csn(struct target *target, uint32_t op)
{
	uint32_t rd_dat;
	if (op == TRUE) {  /* use CS as GPIO */
		target_read_u32(target, REG_AON_PIN_RF_ESMCSN_CFG, &rd_dat);
		rd_dat &= (~FUNC_MSK);
		rd_dat |= FUNC_3;
		target_write_u32(target, REG_AON_PIN_RF_ESMCSN_CFG, rd_dat);
	} else {  /* not GPIO */
		target_read_u32(target, REG_AON_PIN_RF_ESMCSN_CFG, &rd_dat);
		rd_dat &= (~FUNC_MSK);
		target_write_u32(target, REG_AON_PIN_RF_ESMCSN_CFG, rd_dat);
	}
}
#endif

static void uwp5661_set_sfc_clk(struct target *target)
{
	target_write_u32(target, SFC_CLK_CFG, /* SFC_CLK_OUT_DIV_2 |  */SFC_CLK_OUT_2X_EN |
			SFC_CLK_2X_EN | SFC_CLK_SAMPLE_2X_PHASE_1 | SFC_CLK_SAMPLE_2X_EN);
	/*
	 * cgm_sfc_1x_div: clk_sfc_1x = clk_src/(bit 9:8 + 1)
	 * */
	target_write_u32(target, REG_AON_CLK_RF_CGM_SFC_1X_CFG, 0x00000100);
	/* 0: xtal MHz 1: 133MHz 2: 139MHz 3: 160MHz 4: 208MHz
	 * cgm_sfc_2x_sel: clk_sfc_1x source (bit 2:1:0)
	 * */
	target_write_u32(target, REG_AON_CLK_RF_CGM_SFC_2X_CFG, 0x00000000);
}

static void SFCDRV_Req(struct target *target)
{
	uint32_t int_status = 0;
	uint32_t int_timeout = 0;
	target_write_u32(target, SFC_SOFT_REQ, (1 << SHIFT_SOFT_REQ));
	do {
		target_read_u32(target, SFC_INT_RAW, &int_status);
		if (int_timeout++ > SFC_DRVREQ_TIMEOUT) {
			LOG_ERROR("SFCDRV Req time out!\n");
			break;
		}
	} while (int_status == 0);
	target_write_u32(target, SFC_INT_CLR , (1 << SHIFT_INT_CLR));
}

static void SFCDRV_IntCfg(struct target *target, uint32_t op)
{
	if (op == TRUE)  /* CS1 */
		target_write_u32(target, SFC_IEN, 0x000000FF);
	else  /* CS0 */
		target_write_u32(target, SFC_IEN, 0x00000000);
}

static uint32_t SFCDRV_GetInitAddr(struct target *target)
{
	uint32_t start_addr = sfc_cmd_cfg_cache ;

	if (sfc_cmd_cfg_cache == 0xFFFFFFFF)
		target_read_u32(target, SFC_CMD_CFG, &start_addr);

	start_addr = (start_addr & MSK_STS_INI_ADDR_SEL) >> SHIFT_STS_INI_ADDR_SEL;

	switch (start_addr) {
		case INI_CMD_BUF_6:
			start_addr = CMD_BUF_6;
			break;

		case INI_CMD_BUF_5:
			start_addr = CMD_BUF_5;
			break;

		case INI_CMD_BUF_4:
			start_addr = CMD_BUF_4;
			break;

		default:
			start_addr = CMD_BUF_7;
			break;
	}
	return start_addr;
}

static void SFCDRV_SetCMDCfgReg(struct target *target, CMD_MODE_E cmdmode, BIT_MODE_E bitmode, INI_ADD_SEL_E iniAddSel)
{
	uint32_t nxt_sfc_cmd_cfg = ((cmdmode << SHIFT_CMD_SET)|
								(bitmode << SHIFT_RDATA_BIT_MODE)|
								(iniAddSel << SHIFT_STS_INI_ADDR_SEL));

	if (sfc_cmd_cfg_cache != nxt_sfc_cmd_cfg) {
		target_write_u32(target, SFC_CMD_CFG, nxt_sfc_cmd_cfg);
		sfc_cmd_cfg_cache = nxt_sfc_cmd_cfg;
	}
}

static void SFCDRV_SetCMDBuf(struct target *target, CMD_BUF_INDEX_E index, uint32_t value)
{
	cmd_buf_cache_bitmap |= 1<<index;
	cmd_info_buf_cache[index] = value;
}

static void SFCDRV_SetTypeInfBuf(struct target *target, CMD_BUF_INDEX_E index, BIT_MODE_E bitmode,
			BYTE_NUM_E bytenum, CMD_MODE_E cmdmode, SEND_MODE_E sendmode)
{
	switch (index) {
		case CMD_BUF_0:
			cmd_info_buf_cache[INFO_BUF_0] |=   (VALID0|
												(bitmode << SHIFT_BIT_MODE0)|
												(bytenum << SHIFT_BYTE_NUM0)|
												(cmdmode << SHIFT_OPERATION_STATUS0)|
												(sendmode << SHIFT_BYTE_SEND_MODE0));
			break;

		case CMD_BUF_1:
			cmd_info_buf_cache[INFO_BUF_0] |=   (VALID1|
												(bitmode << SHIFT_BIT_MODE1)|
												(bytenum << SHIFT_BYTE_NUM1)|
												(cmdmode << SHIFT_OPERATION_STATUS1)|
												(sendmode << SHIFT_BYTE_SEND_MODE1));
			break;

		case CMD_BUF_2:
			cmd_info_buf_cache[INFO_BUF_0] |=   (VALID2|
												(bitmode << SHIFT_BIT_MODE2)|
												(bytenum << SHIFT_BYTE_NUM2)|
												(cmdmode << SHIFT_OPERATION_STATUS2)|
												(sendmode << SHIFT_BYTE_SEND_MODE2));
			break;

		case CMD_BUF_3:
			cmd_info_buf_cache[INFO_BUF_0] |=   (VALID3|
												(bitmode << SHIFT_BIT_MODE3)|
												(bytenum << SHIFT_BYTE_NUM3)|
												(cmdmode << SHIFT_OPERATION_STATUS3)|
												(sendmode << SHIFT_BYTE_SEND_MODE3));
			break;

		case CMD_BUF_4:
			cmd_info_buf_cache[INFO_BUF_1] |=   (VALID4|
												(bitmode << SHIFT_BIT_MODE4)|
												(bytenum << SHIFT_BYTE_NUM4)|
												(cmdmode << SHIFT_OPERATION_STATUS4)|
												(sendmode << SHIFT_BYTE_SEND_MODE4));
			break;

		case CMD_BUF_5:
			cmd_info_buf_cache[INFO_BUF_1] |=   (VALID5|
												(bitmode << SHIFT_BIT_MODE5)|
												(bytenum << SHIFT_BYTE_NUM5)|
												(cmdmode << SHIFT_OPERATION_STATUS5)|
												(sendmode << SHIFT_BYTE_SEND_MODE5));
			break;

		case CMD_BUF_6:
			cmd_info_buf_cache[INFO_BUF_1] |=   (VALID6|
												(bitmode << SHIFT_BIT_MODE6)|
												(bytenum << SHIFT_BYTE_NUM6)|
												(cmdmode << SHIFT_OPERATION_STATUS6)|
												(sendmode << SHIFT_BYTE_SEND_MODE6));
			break;

		case CMD_BUF_7:
			cmd_info_buf_cache[INFO_BUF_1] |=   (VALID7|
												(bitmode << SHIFT_BIT_MODE7)|
												(bytenum << SHIFT_BYTE_NUM7)|
												(cmdmode << SHIFT_OPERATION_STATUS7)|
												(sendmode << SHIFT_BYTE_SEND_MODE7));
			break;

		case CMD_BUF_8:
			cmd_info_buf_cache[INFO_BUF_2] |=   (VALID8|
												(bitmode << SHIFT_BIT_MODE8)|
												(bytenum << SHIFT_BYTE_NUM8)|
												(cmdmode << SHIFT_OPERATION_STATUS8)|
												(sendmode << SHIFT_BYTE_SEND_MODE8));
			break;

		case CMD_BUF_9:
			cmd_info_buf_cache[INFO_BUF_2] |=   (VALID9|
												(bitmode << SHIFT_BIT_MODE9)|
												(bytenum << SHIFT_BYTE_NUM9)|
												(cmdmode << SHIFT_OPERATION_STATUS9)|
												(sendmode << SHIFT_BYTE_SEND_MODE9));
			break;

		case CMD_BUF_10:
			cmd_info_buf_cache[INFO_BUF_2] |=   (VALID10|
												(bitmode << SHIFT_BIT_MODE10)|
												(bytenum << SHIFT_BYTE_NUM10)|
												(cmdmode << SHIFT_OPERATION_STATUS10)|
												(sendmode << SHIFT_BYTE_SEND_MODE10));
			break;

		case CMD_BUF_11:
			cmd_info_buf_cache[INFO_BUF_2] |=   (VALID11|
												(bitmode << SHIFT_BIT_MODE11)|
												(bytenum << SHIFT_BYTE_NUM11)|
												(cmdmode << SHIFT_OPERATION_STATUS11)|
												(sendmode << SHIFT_BYTE_SEND_MODE11));
			break;

		default:
			break;
	}
}

static void SFCDRV_GetReadBuf(struct target *target, uint32_t *buffer, uint32_t word_cnt)
{
	uint32_t i = 0;
	uint32_t read_buf_index = SFCDRV_GetInitAddr(target);
	uint8_t tmp_buf[INFO_BUF_MAX*4] = {0};

	target_read_memory(target, SFC_CMD_BUF0+read_buf_index*4, 4, word_cnt, tmp_buf);
	for (i = 0; i < word_cnt; i++)
		buffer[i] = target_buffer_get_u32(target, tmp_buf+i*4);
}

static void SFCDRV_SetCmdData(struct target *target, uint32_t cmd_buf_index, SFC_CMD_DES_T *cmd_des_ptr)
{
	if (cmd_des_ptr != NULL) {
		SFCDRV_SetCMDBuf(target, cmd_buf_index, cmd_des_ptr->cmd);
		SFCDRV_SetTypeInfBuf(target, cmd_buf_index,
							cmd_des_ptr->bit_mode,
							cmd_des_ptr->cmd_byte_len,
							cmd_des_ptr->cmd_mode,
							cmd_des_ptr->send_mode);
	}
}

static void SFCDRV_SetReadBuf(struct target *target, uint32_t read_buf_index, SFC_CMD_DES_T *cmd_des_ptr)
{
	if (cmd_des_ptr != NULL) {
		SFCDRV_SetTypeInfBuf(target, read_buf_index,
							cmd_des_ptr->bit_mode,
							cmd_des_ptr->cmd_byte_len,
							cmd_des_ptr->cmd_mode,
							cmd_des_ptr->send_mode);
	}
}

static void create_cmd(SFC_CMD_DES_T *cmd_desc_ptr, uint32_t cmd, uint32_t byte_len,
			CMD_MODE_E cmd_mode, BIT_MODE_E bit_mode, SEND_MODE_E send_mode)
{
	cmd_desc_ptr->cmd = cmd;
	cmd_desc_ptr->cmd_byte_len = byte_len;
	cmd_desc_ptr->cmd_mode = cmd_mode;
	cmd_desc_ptr->bit_mode = bit_mode;
	cmd_desc_ptr->send_mode = send_mode;
}

static void spiflash_read_write(struct target *target, SFC_CMD_DES_T *cmd_des_ptr,
			uint32_t cmd_len, uint32_t *din)
{
	uint32_t i = 0;
	uint32_t read_count = 0;
	uint32_t read_buf_index = SFCDRV_GetInitAddr(target);
	uint8_t tmp_buf[INFO_BUF_MAX*4] = {0};
	uint32_t update_info_buf = FALSE;

	cmd_buf_cache_bitmap = 0;
	memset(cmd_info_buf_cache, 0 , sizeof(cmd_info_buf_cache));

	for (i = 0; i < cmd_len; i++) {
		cmd_des_ptr[i].is_valid = TRUE;
		if ((cmd_des_ptr[i].cmd_mode == CMD_MODE_WRITE) ||
			(cmd_des_ptr[i].cmd_mode == CMD_MODE_HIGHZ))
			SFCDRV_SetCmdData(target, i, &(cmd_des_ptr[i]));
		else if (cmd_des_ptr[i].cmd_mode == CMD_MODE_READ) {
			SFCDRV_SetCMDBuf(target, read_buf_index, 0);
			SFCDRV_SetReadBuf(target, read_buf_index, &(cmd_des_ptr[i]));
			read_buf_index++;
			read_count++;
		}
	}

	if ((prev_cmd_info_buf_cache[INFO_BUF_0] != cmd_info_buf_cache[INFO_BUF_0]) ||
		(prev_cmd_info_buf_cache[INFO_BUF_1] != cmd_info_buf_cache[INFO_BUF_1]) ||
		(prev_cmd_info_buf_cache[INFO_BUF_2] != cmd_info_buf_cache[INFO_BUF_2])) {
		for (i = INFO_BUF_0; i < INFO_BUF_MAX; i++) {
			target_buffer_set_u32(target, tmp_buf+i*4, cmd_info_buf_cache[i]);
			prev_cmd_info_buf_cache[i] = cmd_info_buf_cache[i];
		}

		update_info_buf = TRUE;
	}

	if (cmd_len <= 0) {
		for (i = CMD_BUF_0; i < CMD_BUF_MAX; i++) {
			if (cmd_buf_cache_bitmap & (1<<i))
				target_write_u32(target, SFC_CMD_BUF0+i*4, cmd_info_buf_cache[i]);
		}

		if (update_info_buf == TRUE)
			target_write_memory(target, SFC_TYPE_BUF0, 4, INFO_BUF_MAX - INFO_BUF_0, tmp_buf+INFO_BUF_0*4);
	} else {
		if (update_info_buf == TRUE) {
			for (i = CMD_BUF_0; i < INFO_BUF_MAX; i++)
				target_buffer_set_u32(target, tmp_buf+i*4, cmd_info_buf_cache[i]);
			target_write_memory(target, SFC_CMD_BUF0, 4, INFO_BUF_MAX, tmp_buf);
		} else {
			for (i = CMD_BUF_0; i < CMD_BUF_MAX; i++)
				target_buffer_set_u32(target, tmp_buf+i*4, cmd_info_buf_cache[i]);
			target_write_memory(target, SFC_CMD_BUF0, 4, CMD_BUF_MAX, tmp_buf);
		}
	}

	SFCDRV_Req(target);

	if (0 != read_count)
		SFCDRV_GetReadBuf(target, din, read_count);
}

static void spiflash_enter_xip(struct target *target, uint8_t support_4addr)
{
	uint32_t i = 0;
	SFC_CMD_DES_T cmd_desc[3];

	create_cmd(&(cmd_desc[0]), CMD_FAST_READ, BYTE_NUM_1, CMD_MODE_WRITE, BIT_MODE_1, SEND_MODE_0);
	create_cmd(&(cmd_desc[1]), 0x0          , (support_4addr == TRUE) ? BYTE_NUM_4 : BYTE_NUM_3,
															CMD_MODE_WRITE, BIT_MODE_1, SEND_MODE_1);
	create_cmd(&(cmd_desc[2]), 0x0          , BYTE_NUM_1, CMD_MODE_HIGHZ, BIT_MODE_1, SEND_MODE_0);

	for (i = 0; i < 3; i++) {
		cmd_desc[i].is_valid = TRUE;
		SFCDRV_SetCmdData(target, i, &(cmd_desc[i]));
	}

	target_write_u32(target, SFC_CMD_BUF0 , cmd_info_buf_cache[CMD_BUF_0]);
	target_write_u32(target, SFC_TYPE_BUF0, cmd_info_buf_cache[INFO_BUF_0]);
	target_write_u32(target, SFC_TYPE_BUF1, 0x00000000);
	target_write_u32(target, SFC_TYPE_BUF2, 0x00000000);
	SFCDRV_SetCMDCfgReg(target, CMD_MODE_READ , BIT_MODE_1, INI_CMD_BUF_7);
	target_write_u32(target, REG_AON_CLK_RF_CGM_ARM_CFG, xip_sys_clk_cfg[1]);
	target_write_u32(target, REG_AON_CLK_RF_CGM_MTX_CFG, xip_sys_clk_cfg[0]);
}

static void spiflash_exit_xip(struct target *target)
{
	SFCDRV_SetCMDCfgReg(target, CMD_MODE_WRITE, BIT_MODE_1, INI_CMD_BUF_4);
	target_read_u32(target, REG_AON_CLK_RF_CGM_MTX_CFG, &(xip_sys_clk_cfg[1]));
	target_read_u32(target, REG_AON_CLK_RF_CGM_ARM_CFG, &(xip_sys_clk_cfg[0]));
	target_write_u32(target, REG_AON_CLK_RF_CGM_MTX_CFG, 0x00000100);
	target_write_u32(target, REG_AON_CLK_RF_CGM_ARM_CFG, 0x00000005);
}

static void spiflash_select_xip(struct target *target, uint8_t support_4addr, uint32_t op)
{
	target_write_u32(target, SFC_INT_CLR , (1 << SHIFT_INT_CLR));
	if (op == TRUE)
		spiflash_enter_xip(target, support_4addr);
	else
		spiflash_exit_xip(target);
}

static BYTE_NUM_E spi_flash_addr(uint32_t *addr, uint8_t support_4addr)
{
	uint8_t cmd[4] = {0};
	uint32_t address = *addr;

	cmd[0] = ((address >> 0) & (0xFF));
	cmd[1] = ((address >> 8) & (0xFF));
	cmd[2] = ((address >> 16) & (0xFF));
	cmd[3] = ((address >> 24) & (0xFF));

	if (support_4addr == TRUE) {
		*addr = (cmd[3] << 0)  | (cmd[2] << 8) |
				(cmd[1] << 16) | (cmd[0] << 24);
		return BYTE_NUM_4;
	} else {
		*addr = (cmd[2] << 0) | (cmd[1] << 8) | (cmd[0] << 16);
		return BYTE_NUM_3;
	}
}

static void spiflash_cmd_write(struct target *target, struct uwp_flash *flash,
			uint8_t cmd, uint32_t *data_out, uint32_t data_len, BIT_MODE_E bitmode)
{
	SFC_CMD_DES_T cmd_desc[3];
	BYTE_NUM_E byte_num = BYTE_NUM_1;
	uint32_t cmd_idx = 0;

	create_cmd(&(cmd_desc[cmd_idx++]), cmd, BYTE_NUM_1, CMD_MODE_WRITE, bitmode, SEND_MODE_0);

	if (data_len > 8)
		data_len = 8;

	if (data_len > 4) {
		create_cmd(&(cmd_desc[cmd_idx]), data_out[cmd_idx-1], BYTE_NUM_4, CMD_MODE_WRITE, bitmode, SEND_MODE_0);
		cmd_idx++;
		data_len = data_len - 4;
	}

	if (data_len > 0) {
		byte_num = BYTE_NUM_1 + (data_len - 1);
		create_cmd(&(cmd_desc[cmd_idx]), data_out[cmd_idx-1], byte_num  , CMD_MODE_WRITE, bitmode, SEND_MODE_0);
		cmd_idx++;
	}

	spiflash_read_write(target, cmd_desc, cmd_idx, NULL);
}

static void spiflash_cmd_read(struct target *target, struct uwp_flash *flash,
			uint8_t cmd, uint32_t address, uint8_t support_4addr, uint32_t *data_in, uint32_t data_len)
{
	SFC_CMD_DES_T cmd_desc[4];
	BYTE_NUM_E byte_num = BYTE_NUM_1;
	uint32_t tmp_buf[2] = {0};
	uint32_t cmd_idx = 0;

	create_cmd(&(cmd_desc[cmd_idx++]), cmd, BYTE_NUM_1, CMD_MODE_WRITE, BIT_MODE_1, SEND_MODE_0);

	if (address != 0xFFFFFFFF) {
		uint32_t dest_addr = address;
		byte_num = spi_flash_addr(&dest_addr, support_4addr);
		create_cmd(&(cmd_desc[cmd_idx++]), dest_addr, byte_num , CMD_MODE_WRITE, BIT_MODE_1, SEND_MODE_0);
	}

	if (data_len > 8)
		data_len = 8;

	if (data_len > 4) {
		create_cmd(&(cmd_desc[cmd_idx++]), 0, BYTE_NUM_4, CMD_MODE_READ, BIT_MODE_1, SEND_MODE_0);
		data_len = data_len - 4;
	}

	if (data_len > 0) {
		byte_num = BYTE_NUM_1 + (data_len - 1);
		create_cmd(&(cmd_desc[cmd_idx++]), 0, byte_num  , CMD_MODE_READ, BIT_MODE_1, SEND_MODE_0);
	}

	spiflash_read_write(target, cmd_desc, cmd_idx, tmp_buf);

	if (cmd_idx > 1)
		data_in[0] = (((tmp_buf[0] >> 24) & 0xFF) << 0) |
						(((tmp_buf[0] >> 16) & 0xFF) << 8) |
						(((tmp_buf[0] >> 8) & 0xFF) << 16) |
						(((tmp_buf[0] >> 0) & 0xFF) << 24) ;

	if (cmd_idx > 2)
		data_in[1] = (((tmp_buf[1] >> 24) & 0xFF) << 0) |
						(((tmp_buf[1] >> 16) & 0xFF) << 8) |
						(((tmp_buf[1] >> 8) & 0xFF) << 16) |
						(((tmp_buf[1] >> 0) & 0xFF) << 24) ;
}

static int spiflash_cmd_poll_bit(struct target *target, struct uwp_flash *flash,
			uint32_t timeout, uint8_t cmd, uint32_t poll_bit, uint32_t bit_value)
{
	uint32_t status = 0;

	do {
		spiflash_cmd_read(target, flash, cmd, 0xFFFFFFFF, FALSE, &status, 1);
		status &= 0xFF;
		if (bit_value) {
			if ((status & poll_bit))
				return ERROR_OK;
		} else {
			if ((status & poll_bit) == 0)
				return ERROR_OK;
		}
	} while (timeout--);

	LOG_ERROR("Polling flash status time out!\n");

	return ERROR_FAIL;
}

static void spiflash_write_enable(struct target *target, struct uwp_flash *flash)
{
	spiflash_cmd_write(target, flash, CMD_WRITE_ENABLE, NULL, 0, BIT_MODE_1);
	/* spiflash_cmd_poll_bit(target, flash, SPI_FLASH_WEL_TIMEOUT, CMD_READ_STATUS1, STATUS_WEL, 1); */
}

static void spiflash_reset_anyway(struct target *target, struct uwp_flash *flash)
{
	uint32_t i = 0;
	uint32_t dummy_dat = 0;

	spiflash_cmd_write(target, flash, CMD_RSTEN, NULL, 0, BIT_MODE_4);
	spiflash_cmd_write(target, flash, CMD_RST  , NULL, 0, BIT_MODE_4);
	for (i = 0; i < 10; i++)
		target_read_u32(target, SFC_CMD_CFG, &dummy_dat);

	spiflash_cmd_write(target, flash, CMD_RSTEN, NULL, 0, BIT_MODE_1);
	spiflash_cmd_write(target, flash, CMD_RST  , NULL, 0, BIT_MODE_1);
	for (i = 0; i < 10; i++)
		target_read_u32(target, SFC_CMD_CFG, &dummy_dat);
}

static int spiflash_4addr_enable(struct target *target, struct uwp_flash *flash)
{
	spiflash_cmd_write(target, flash, CMD_ENTER_4ADDR, NULL, 0, BIT_MODE_1);

	return spiflash_cmd_poll_bit(target, flash, SPI_FLASH_ADS_TIMEOUT, CMD_READ_STATUS3, STATUS_ADS, 1);
}

static int spiflash_4addr_disable(struct target *target, struct uwp_flash *flash)
{
	spiflash_cmd_write(target, flash, CMD_EXIT_4ADDR, NULL, 0, BIT_MODE_1);

	return spiflash_cmd_poll_bit(target, flash, SPI_FLASH_ADS_TIMEOUT, CMD_READ_STATUS3, STATUS_ADS, 0);
}

static int spiflash_cmd_sector_erase(struct target *target, struct uwp_flash *flash, uint32_t offset)
{
	uint32_t addr = offset * flash->sector_size;
	BYTE_NUM_E addr_byte_num = spi_flash_addr(&addr, flash->support_4addr);
	int ret = ERROR_OK;

	spiflash_write_enable(target, flash);

	spiflash_cmd_write(target, flash, CMD_SECTOR_ERASE, &addr, addr_byte_num + 1, BIT_MODE_1);

	ret = spiflash_cmd_poll_bit(target, flash, SPI_FLASH_SECTOR_ERASE_TIMEOUT, CMD_READ_STATUS1, STATUS_WIP, 0);

	return ret;
}

static int uwp5661_erase(struct flash_bank *bank, int first, int last)
{
	struct uwp5661_flash_bank *uwp_bank = bank->driver_priv;
	struct target *target = bank->target;
	struct uwp_flash *flash = &(uwp_bank->flash);
	int i = 0;
	int ret = ERROR_OK;

	memset(prev_cmd_info_buf_cache, 0 , sizeof(prev_cmd_info_buf_cache));

	if (bank->target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted before erasing flash!\n");
		return ERROR_TARGET_NOT_HALTED;
	}

#ifdef NEW_WRITE_PAGE
	uwp5661_init_sw_csn(target);
#endif
	uwp5661_set_sfc_clk(target);

	spiflash_select_xip(target, FALSE, FALSE);

	if (flash->support_4addr == TRUE) {
		ret = spiflash_4addr_enable(target, flash);
		if (ret != ERROR_OK) {
			LOG_ERROR("uwp5661 SPI 4Byte Address mode switching ON failed!\n");
			spiflash_reset_anyway(target, NULL);
			spiflash_select_xip(target, FALSE, TRUE);
			return ret;
		}
	}

	for (i = first; i <= last; i++) {
		ret = spiflash_cmd_sector_erase(target, flash, i);
		if (ret != ERROR_OK)
			return ret;

		bank->sectors[i].is_erased = 1;
	}

	if (flash->support_4addr == TRUE) {
		ret = spiflash_4addr_disable(target, flash);
		if (ret != ERROR_OK) {
			LOG_ERROR("uwp5661 SPI 4Byte Address mode switching OFF failed!\n");
			spiflash_reset_anyway(target, NULL);
		}
	}

	spiflash_select_xip(target, FALSE, TRUE);

	return ret;
}

#ifdef NEW_WRITE_PAGE
static int spiflash_write_page(struct target *target, struct uwp_flash *flash,
			uint32_t data_addr, uint8_t *data_out, uint32_t data_len)
{
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t dest_addr = data_addr;
	uint8_t *data_ptr = data_out;
	uint32_t data_tmp = 0;
	uint32_t cmd_idx = 0;
	uint32_t piece_cnt = 0;
	BYTE_NUM_E byte_num = BYTE_NUM_4;
	SFC_CMD_DES_T cmd_desc[CMD_BUF_MAX];
	int ret = ERROR_OK;

	/* using cs as GPIO bit28 and pull it up, then write cmd and all data in */
	for (i = 0; i < data_len;) {
		cmd_idx = 0;
		piece_cnt = 0;

		if (i == 0) {
			spiflash_write_enable(target, flash);

			uwp5661_force_csn(target, TRUE);

			byte_num = spi_flash_addr(&dest_addr, flash->support_4addr);
			/* write cmd and address in */
			create_cmd(&(cmd_desc[cmd_idx++]), CMD_PAGE_PROGRAM, BYTE_NUM_1,
						CMD_MODE_WRITE, BIT_MODE_1, SEND_MODE_0);
			create_cmd(&(cmd_desc[cmd_idx++]), dest_addr       , byte_num  ,
						CMD_MODE_WRITE, BIT_MODE_1, SEND_MODE_0);
		}

		piece_cnt = min((CMD_BUF_MAX - cmd_idx)*4, data_len - i);
		/* write all data in */
		for (j = 0; j < piece_cnt;) {
			if ((piece_cnt - j) >= 4) {
				byte_num = BYTE_NUM_4;
				data_tmp = (data_ptr[0] << 0)   | (data_ptr[1] << 8) |
							(data_ptr[2] << 16) | (data_ptr[3] << 24);
				data_ptr = data_ptr + 4;
				j = j + 4;
			} else {
				uint32_t tail_bytes = piece_cnt - j;
				byte_num = BYTE_NUM_1 + (tail_bytes - 1);
				switch (tail_bytes) {
					case 1: {
						data_tmp = data_ptr[0];
						break;
					}
					case 2: {
						data_tmp = (data_ptr[0] << 0) | (data_ptr[1] << 8);
						break;
					}
					case 3: {
						data_tmp = (data_ptr[0] << 0) | (data_ptr[1] << 8) | (data_ptr[2] << 16);
						break;
					}
					default:
						break;
				}
				j = piece_cnt;
			}
			create_cmd(&(cmd_desc[cmd_idx++]), data_tmp, byte_num, CMD_MODE_WRITE, BIT_MODE_1, SEND_MODE_0);
		}

		spiflash_read_write(target, cmd_desc, cmd_idx, NULL);

		i = i + piece_cnt;
	}

	uwp5661_force_csn(target, FALSE);

	ret = spiflash_cmd_poll_bit(target, flash, SPI_FLASH_PAGE_PROG_TIMEOUT, CMD_READ_STATUS1, STATUS_WIP, 0);

	return ret;
}
#else
static int spiflash_write_page(struct target *target, struct uwp_flash *flash,
			uint32_t data_addr, uint8_t *data_out, uint32_t data_len)
{
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t addr = data_addr;
	uint32_t dest_addr = addr;
	uint8_t *data_ptr = data_out;
	uint32_t data_tmp = 0;
	uint32_t cmd_idx = 0;
	uint32_t piece_cnt = 0;
	BYTE_NUM_E byte_num = BYTE_NUM_3;
	SFC_CMD_DES_T cmd_desc[CMD_BUF_MAX];
	int ret = ERROR_OK;

	for (i = 0; i < data_len;) {
		cmd_idx = 0;
		piece_cnt = 0;

		spiflash_write_enable(target, flash);

		dest_addr = addr;
		byte_num = spi_flash_addr(&dest_addr, flash->support_4addr);

		create_cmd(&(cmd_desc[cmd_idx++]), CMD_PAGE_PROGRAM, BYTE_NUM_1, CMD_MODE_WRITE, BIT_MODE_1, SEND_MODE_0);
		create_cmd(&(cmd_desc[cmd_idx++]), dest_addr       , byte_num  , CMD_MODE_WRITE, BIT_MODE_1, SEND_MODE_0);

		piece_cnt = min((CMD_BUF_MAX - cmd_idx)*4, data_len - i);

		for (j = 0; j < piece_cnt;) {
			if ((piece_cnt - j) >= 4) {
				byte_num = BYTE_NUM_4;
				data_tmp = (data_ptr[0] << 0)   | (data_ptr[1] << 8) |
							(data_ptr[2] << 16) | (data_ptr[3] << 24);
				data_ptr = data_ptr + 4;
				j = j + 4;
			} else {
				uint32_t tail_bytes = piece_cnt - j;
				byte_num = BYTE_NUM_1 + (tail_bytes - 1);
				switch (tail_bytes) {
					case 1: {
						data_tmp = data_ptr[0];
						break;
					}
					case 2: {
						data_tmp = (data_ptr[0] << 0) | (data_ptr[1] << 8);
						break;
					}
					case 3: {
						data_tmp = (data_ptr[0] << 0) | (data_ptr[1] << 8) | (data_ptr[2] << 16);
						break;
					}
					default:
						break;
				}
				j = piece_cnt;
			}
			create_cmd(&(cmd_desc[cmd_idx++]), data_tmp, byte_num, CMD_MODE_WRITE, BIT_MODE_1, SEND_MODE_0);
		}

		spiflash_read_write(target, cmd_desc, cmd_idx, NULL);
		ret = spiflash_cmd_poll_bit(target, flash, SPI_FLASH_PAGE_PROG_TIMEOUT, CMD_READ_STATUS1, STATUS_WIP, 0);
		if (ret != ERROR_OK)
			return ret;

		i = i + piece_cnt;
		addr = addr + piece_cnt;
	}

	return ret;
}
#endif

static int uwp5661_write(struct flash_bank *bank, const uint8_t *buffer,
			uint32_t offset, uint32_t count)
{
	struct uwp5661_flash_bank *uwp5661_info = bank->driver_priv;
	struct uwp_flash *flash = &(uwp5661_info->flash);
	struct target *target = bank->target;
	uint32_t page_size = flash->page_size;
	uint32_t page_addr = 0;
	uint32_t byte_addr = 0;
	uint32_t chunk_len = 0;
	uint32_t actual    = 0;
	uint32_t data_len  = 0;
	uint32_t space_len = 0;
	int ret = ERROR_OK;

	memset(prev_cmd_info_buf_cache, 0 , sizeof(prev_cmd_info_buf_cache));

	if (bank->target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted before writing flash!\n");
		return ERROR_TARGET_NOT_HALTED;
	}

#ifdef NEW_WRITE_PAGE
	uwp5661_init_sw_csn(target);
#endif
	uwp5661_set_sfc_clk(target);

	spiflash_select_xip(target, FALSE, FALSE);

	if (flash->support_4addr == TRUE) {
		ret = spiflash_4addr_enable(target, flash);
		if (ret != ERROR_OK) {
			LOG_ERROR("uwp5661 SPI 4Byte Address mode switching ON failed!\n");
			spiflash_reset_anyway(target, NULL);
			spiflash_select_xip(target, FALSE, TRUE);
			return ret;
		}
	}

	if (offset != 0) {
		page_addr = offset / page_size;
		byte_addr = offset % page_size;
	}

	for (actual = 0; actual < count; actual += chunk_len) {
		data_len = count - actual;
		space_len = page_size - byte_addr;
		chunk_len = min(data_len, space_len);

		ret = spiflash_write_page(target, flash, (page_addr * page_size + byte_addr),
					(uint8_t *)(buffer + actual), chunk_len);

		if (ret != ERROR_OK) {
			LOG_ERROR("Flash write failed\n");
			break;
		}

		page_addr++;
		byte_addr = 0;
	}

	if (flash->support_4addr == TRUE) {
		ret = spiflash_4addr_disable(target, flash);
		if (ret != ERROR_OK) {
			LOG_ERROR("uwp5661 SPI 4Byte Address mode switching OFF failed!\n");
			spiflash_reset_anyway(target, NULL);
		}
	}

	spiflash_select_xip(target, FALSE, TRUE);

	return ret;
}

static void spiflash_data_read(struct target *target, struct uwp_flash *flash,
		uint32_t offset, uint32_t count, uint8_t *buf)
{

	uint32_t i = 0;
	uint32_t addr = offset;
	uint32_t piece_cnt = 0;
	uint8_t tmp_buf[256] = {0};
	uint8_t *data_ptr = buf;

	for (i = 0; i < count;) {
		piece_cnt = min(count - i, 256-(addr%256));
		target_read_memory(target, UWP5661_FLASH_BASE_ADDRESS+(addr&0xFFFFFF00), 4, 64, tmp_buf);
		memcpy(data_ptr, tmp_buf+(addr%256), piece_cnt);
		i = i + piece_cnt;
		addr = addr + piece_cnt;
		data_ptr = data_ptr + piece_cnt;
	}
}

static int uwp5661_read(struct flash_bank *bank, uint8_t *buffer,
			uint32_t offset, uint32_t count)
{
	struct uwp5661_flash_bank *uwp5661_info = bank->driver_priv;
	struct uwp_flash *flash = &(uwp5661_info->flash);
	struct target *target = bank->target;
	int ret = ERROR_OK;

	if (bank->target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted before reading flash!\n");
		return ERROR_TARGET_NOT_HALTED;
	}

#ifdef NEW_WRITE_PAGE
	uwp5661_init_sw_csn(target);
#endif
	uwp5661_set_sfc_clk(target);

	spiflash_select_xip(target, flash->support_4addr, TRUE);

	if (flash->support_4addr == TRUE) {
		ret = spiflash_4addr_enable(target, flash);
		if (ret != ERROR_OK) {
			LOG_ERROR("uwp5661 SPI 4Byte Address mode switching ON failed!\n");
			spiflash_reset_anyway(target, NULL);
			spiflash_select_xip(target, FALSE, TRUE);
			return ret;
		}
	}

	spiflash_data_read(target, flash, offset, count, buffer);

	if (flash->support_4addr == TRUE) {
		ret = spiflash_4addr_disable(target, flash);
		if (ret != ERROR_OK) {
			LOG_ERROR("uwp5661 SPI 4Byte Address mode switching OFF failed!\n");
			spiflash_reset_anyway(target, NULL);
			spiflash_select_xip(target, FALSE, TRUE);
			return ret;
		}
	}

	spiflash_select_xip(target, FALSE, TRUE);

	return ret;
}

static struct uwp_flash_param *spiflash_scan(struct target *target)
{
	uint32_t i = 0;
	uint32_t read_data = 0;
	uint16_t jedec = 0;
	uint16_t manufacturer_id = 0;
	SFC_CMD_DES_T cmd_desc[2];
	struct spi_flash_spec_s *flash_spec = NULL;
	struct uwp_flash_param *params = NULL;

	create_cmd(&(cmd_desc[0]), CMD_READ_ID, BYTE_NUM_1, CMD_MODE_WRITE, BIT_MODE_1, SEND_MODE_0);
	create_cmd(&(cmd_desc[1]),           0, BYTE_NUM_3, CMD_MODE_READ , BIT_MODE_1, SEND_MODE_0);

	spiflash_read_write(target, cmd_desc, 2, &read_data);

	manufacturer_id = (read_data >> 24) & 0xFF;
	jedec = (read_data >> 8) & 0xFFFF;

	for (i = 0; i < ARRAY_SIZE(spi_flash_spec_table); i++) {
		flash_spec = &spi_flash_spec_table[i];
		if (flash_spec->manufacturer_id == manufacturer_id)
			break;
	}

	if (i == ARRAY_SIZE(spi_flash_spec_table)) {
		LOG_ERROR("Unsupported manufacture %04x\n", manufacturer_id);
		return NULL;
	}

	for (i = 0; i < flash_spec->table_num; i++) {
		params = &(flash_spec->table)[i];
		if (params->idcode1 == jedec)
			break;
	}

	if (flash_spec->table_num == i) {
		LOG_ERROR("Unsupported ID %04x\n", jedec);
		return NULL;
	} else
		return params;
}

static int init_flash(struct flash_bank *bank, struct target *target, struct uwp_flash *flash,
			const struct uwp_flash_param **params)
{
	int i = 0;
	struct uwp_flash_param *p = spiflash_scan(target);

	if (p == NULL) {
		LOG_ERROR("Flash scan failed!\n");
		return ERROR_FAIL;
	}

	flash->cs = 1;
	flash->name = p->name;
	flash->size = p->page_size * p->sector_size * p->nr_sectors * p->nr_blocks;
	flash->page_size = p->page_size;
	flash->sector_size = p->page_size * p->sector_size;
	flash->dummy_clocks = p->dummy_clocks;
	flash->work_mode = SPI_MODE;    /* Force using SPI 1bit mode */
	flash->support_4addr = (flash->size > (1<<24)) ? TRUE : FALSE;

	bank->num_sectors = p->nr_sectors * p->nr_blocks;
	bank->sectors = malloc(sizeof(struct flash_sector) * bank->num_sectors);
	for (i = 0; i < (bank->num_sectors); i++) {
		bank->sectors[i].size = flash->sector_size;
		bank->sectors[i].offset = i * flash->sector_size;
		bank->sectors[i].is_erased = -1;
	}

	*params = p;

	return ERROR_OK;
}

static int uwp5661_probe(struct flash_bank *bank)
{
	struct uwp5661_flash_bank *uwp5661_info = bank->driver_priv;
	struct target *target = bank->target;
	struct uwp_flash *flash = &(uwp5661_info->flash);
	int ret = ERROR_OK;

	bank->base = UWP5661_FLASH_BASE_ADDRESS;
	uwp5661_info->probed = 0;

	memset(prev_cmd_info_buf_cache, 0 , sizeof(prev_cmd_info_buf_cache));

	if (bank->target->state != TARGET_HALTED) {
		LOG_ERROR("Target not halted before probing flash!\n");
		return ERROR_TARGET_NOT_HALTED;
	}

#ifdef NEW_WRITE_PAGE
	uwp5661_init_sw_csn(target);
	uwp5661_force_csn(target, FALSE);
#endif
	uwp5661_set_sfc_clk(target);

	spiflash_select_xip(target, FALSE, FALSE);

	SFCDRV_IntCfg(target, FALSE);

	spiflash_reset_anyway(target, NULL);

	ret = init_flash(bank, target, flash, &(uwp5661_info->param));
	if (ret != ERROR_OK) {
		LOG_ERROR("uwp5661 SPI flash init failed!\n");
		return ret;
	}

	uwp5661_info->probed = 1;

	spiflash_select_xip(target, FALSE, TRUE);

	return ret;
}

static int uwp5661_auto_probe(struct flash_bank *bank)
{
	return uwp5661_probe(bank);
}

static const struct command_registration uwp5661_exec_command_handlers[] = {
	COMMAND_REGISTRATION_DONE
};

static const struct command_registration uwp5661_command_handlers[] = {
	{
		.name = "uwp5661",
		.mode = COMMAND_ANY,
		.help = "uwp5661 flash command group",
		.usage = "",
		.chain = uwp5661_exec_command_handlers,
	},
	COMMAND_REGISTRATION_DONE
};

FLASH_BANK_COMMAND_HANDLER(uwp5661_flash_bank_command)
{
	struct uwp5661_flash_bank *uwp5661_info = NULL;

	if (CMD_ARGC < 6)
		return ERROR_COMMAND_SYNTAX_ERROR;

	uwp5661_info = malloc(sizeof(struct uwp5661_flash_bank));

	if (uwp5661_info == NULL)
		return ERROR_FAIL;

	bank->driver_priv = uwp5661_info;
	uwp5661_info->probed = 0;

	return ERROR_OK;
}

struct flash_driver uwp5661_flash = {
	.name = "uwp5661",
	.commands = uwp5661_command_handlers,
	.flash_bank_command = uwp5661_flash_bank_command,
	.erase = uwp5661_erase,
	.write = uwp5661_write,
	.read = uwp5661_read,
	.probe = uwp5661_probe,
	.auto_probe = uwp5661_auto_probe,
	.free_driver_priv = default_flash_free_driver_priv,
};
