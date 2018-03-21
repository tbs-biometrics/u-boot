/*
 * Copyright (C) 2018 Touchless Biometric Systems s.r.o.
 * Tomas Novotny <tomas.novotny@tbs-biometrics.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <command.h>
#include <common.h>

#include <asm/arch/pmic_bus.h>
#include <asm/io.h>
#include <axp_pmic.h>

#define SID_OEM			0x10
#define SID_CONTENT_BASE	0x01c14200
#define SID_PRCTL		0x01c14040
#define SID_PRKEY		0x01c14050

#define	BI_STATUS_UNSET	"1"
#define	BI_STATUS_ALREADY_DIFFERENT "2"
#define	BI_STATUS_ALREADY_SAME "3"
#define	BI_STATUS_ERROR_TIMEOUT "4"
#define	BI_STATUS_WRITTEN "5"

u16 batch_ident_read(void)
{
	u32 sid_oem = readl(SID_CONTENT_BASE + SID_OEM);
	return ((sid_oem & 0xff) << 8) | ((sid_oem >> 8) & 0xff);
}

int do_batch_ident_read(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	printf("OEM: 0x%04x\n", batch_ident_read());

	return CMD_RET_SUCCESS;
}

int do_batch_ident_write(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	char *batch_ident;
	u16 sid;
	int attempts = 0;

	if(env_set("batch_ident_status", BI_STATUS_UNSET)) {
		printf("Cannot set 'batch_ident_status' variable.\n");
		return CMD_RET_FAILURE;
	}

	batch_ident = env_get("batch_ident");
	if(!batch_ident) {
		printf("'batch_ident' variable is not defined.\n");
		return CMD_RET_USAGE;
	}

	if(strlen(batch_ident) != 2) {
		printf("'batch_ident' variable is length is %d. It must be 2.\n",
				strlen(batch_ident));
		return CMD_RET_USAGE;
	}

	sid = batch_ident_read();
	if(sid) {
		if(batch_ident[0] == (sid >> 8) && batch_ident[1] == (sid & 0xff)) {
			printf("batch is already written to the same value, exitting\n");
			env_set("batch_ident_status", BI_STATUS_ALREADY_SAME);
			return CMD_RET_SUCCESS;
		}
		else {
			printf("batch is already written, but different value, exitting\n");
			env_set("batch_ident_status", BI_STATUS_ALREADY_DIFFERENT);
			return CMD_RET_FAILURE;
		}
	}

	printf("Writing SID...\n");
	writel((batch_ident[1] << 8) | batch_ident[0], SID_PRKEY);
	writel((SID_OEM << 16) | 0xac01, SID_PRCTL);

	/* Wait for write */
	while(readl(SID_PRCTL) & 0x0001) {
		mdelay(1);
		if(attempts++ >= 3000) {
			env_set("batch_ident_status", BI_STATUS_ERROR_TIMEOUT);
			return CMD_RET_FAILURE;
		}
	}

	env_set("batch_ident_status", BI_STATUS_WRITTEN);

	return CMD_RET_SUCCESS;
}

int do_batch_status_read(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int ret;
	u8 val;

	ret = pmic_bus_read(AXP818_DATA0, &val);
	if (ret) {
		printf("Error reading batch identification status: %d\n", ret);
		return CMD_RET_FAILURE;
	}

	printf("Batch identfication status: 0x%02x\n", val);

	return CMD_RET_SUCCESS;
}

int do_batch_status_write(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int ret;
	unsigned long val;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (strict_strtoul(argv[1], 10, &val) < 0) {
		printf("Cannot parse number.\n");
		return CMD_RET_USAGE;
	}

	if (val > 0xff) {
		printf("'status' argument (%lu) is out of range [0, 255].\n", val);
		return CMD_RET_USAGE;
	}

	printf("Batch identification status (0x%02x) was written\n", (unsigned int)val);

	ret = pmic_bus_write(AXP818_DATA0, val);
	if (ret) {
		printf("Error writing batch identification status: %d\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(batchir, 1, 1, do_batch_ident_read,
		"Read batch identification", "");
U_BOOT_CMD(batchiw, 1, 1, do_batch_ident_write,
		"Write batch identification", "(content to write is taken from 'batch_ident' env variable; 2 bytes)");
U_BOOT_CMD(batchsr, 1, 1, do_batch_status_read,
		"Read batch identification status", "");
U_BOOT_CMD(batchsw, 2, 1, do_batch_status_write,
		"Write batch identification status", "<status>");
