#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "zebvty/vty.h"
#include "zebvty/command.h"

#define MAX_BUFFER_SIZE		2048
#define MAX_LENC	MAX_BUFFER_SIZE
#define MAX_LENR	MAX_LENC
#define MAX_COMMAND_SIZE	2048

static void strcompact(char *str)
{
	int i, j = 0;

	for (i = 0; i < strlen(str); i++) {
		if (!isspace(str[i]))
			str[j++] = tolower(str[i]);
	}
	str[j] = 0;
}

static int strtobin(char *s, unsigned char *d, unsigned int *len)
{
	long l;
	int i, ret;

	if (*s == ':')
		s++;

	for (i = 0; i < (strlen(s) >> 1); i++) {
		if (i >= MAX_LENC || i >> *len)
			return 0;
		ret = sscanf(s + (i << 1), "%2lx", &l);
		if (ret != 1)
			return 0;
		d[i] = l & 0xff;
	}
	*len = i;

	return 1;
}

DEFUN(rc632_reg_read,
      rc632_reg_read_cmd, 
      "reg_read WORD", 
      "Read register of RC632\n")
{
#if 0
	if (send_hex_command(vty, argv[0]) < 0)
		return CMD_ERR_NO_MATCH;
#endif
	return CMD_SUCCESS;
}


#if 0
static int select_file(struct vty *v, int absolute, int path, 
			char *selector, unsigned int sellen, 
			unsigned char *rsp, unsigned int *rlen)
{
	unsigned char cmd[MAX_LENC];

	cmd[0] = 0x00;
	cmd[1] = 0xa4;
	if (absolute) {
		if (path)
			cmd[2] = 0x08;
		else
			cmd[2] = 0x00;
	} else {
		if (path)
			cmd[2] = 0x09;
		else
			cmd[2] = 0x02;
	}
	cmd[3] = 0x00; // FIXME: other templates

	cmd[4] = sellen & 0xff;

	memcpy(cmd[5], selector, sellen);
	cmd[5+sellen] = 0x00;

	send_apdu(hCard, v, cmd, 5+cmd[4], rsp, rlen);
	parse_selectfile_response(hCard, v, rsp, *rlen);
	return CMD_SUCCESS;

}

DEFUN(send_7816_select_file_absolute_fid,
	send_7816_select_file_absolute_fid_cmd,
	"select file absolute fid WORD",
	"Selects a file on the ICC\n")
{
	char *file = argv[0];
	unsigned char rsp[MAX_LENR];
	unsigned int lenr = MAX_LENR;
	unsigned char selector[255];
	unsigned int sellen = 255;

	if (strtobin(file, selector, &sellen) < 0)
		return CMD_ERR_NO_MATCH;
	
	return select_file(vty, 1, 0, selector, sellen, rsp, &lenr);
}

DEFUN(send_7816_select_file_absolute_path,
	send_7816_select_file_absolute_path_cmd,
	"select file absolute path WORD",
	"Selects a file on the ICC\n")
{
	char *file = argv[0];
	unsigned char rsp[MAX_LENR];
	unsigned int lenr = MAX_LENR;
	unsigned char selector[255];
	unsigned int sellen = 255;

	if (strtobin(file, selector, &sellen) < 0)
		return CMD_ERR_NO_MATCH;
	

	return select_file(vty, 1, 1, selector, sellen, rsp, &lenr);
}

DEFUN(send_7816_select_file_relative_fid,
	send_7816_select_file_relative_fid_cmd,
	"select file absolute fid WORD",
	"Selects a file on the ICC\n")
{
	char *file = argv[0];
	unsigned char rsp[MAX_LENR];
	unsigned int lenr = MAX_LENR;
	unsigned char selector[255];
	unsigned int sellen = 255;

	if (strtobin(file, selector, &sellen) < 0)
		return CMD_ERR_NO_MATCH;
	

	return select_file(vty, 0, 0, selector, sellen, rsp, &lenr);
}


DEFUN(send_7816_select_file_relative_path,
	send_7816_select_file_relative_path_cmd,
	"select file relative path WORD",
	"Selects a file on the ICC\n")
{
	char *file = argv[0];
	unsigned char rsp[MAX_LENR];
	unsigned int lenr = MAX_LENR;
	unsigned char selector[255];
	unsigned int sellen = 255;

	if (strtobin(file, selector, &sellen) < 0)
		return CMD_ERR_NO_MATCH;
	

	return select_file(vty, 0, 1, selector, sellen, rsp, &lenr);
}

DEFUN(send_7816_select_dir,
	send_7816_select_dir_cmd,
	"select directory (MF|PARENT|CHILD) (FID|AID)",
	"Selects a directory on the ICC\n")
{
	char *file = argv[1];
	char *type = argv[0];
	char cmd[22];
	char rsp[MAX_LENR];
	int len, lenr = MAX_LENR;
	int empty = 0;

	if (!type)
		return CMD_ERR_NO_MATCH;

	memset(cmd, 0, sizeof(cmd));
	cmd[1] = 0xa4;

	if (!strcmp(type, "MF")) {
		cmd[2] = 0x00;
		cmd[3] = 0x00;
		cmd[4] = 2;	/* length */
		cmd[5] = 0x3f;	/* 3ff0 */
		cmd[6] = 0xf0;
		empty = 1;
	} else if (!strcmp(type, "PARENT")) {
		cmd[2] = 0x03;
		cmd[3] = 0x00;
		cmd[4] = 0x00;
		empty = 1;
	} else if (!strcmp(file, "CHILD")) {
		cmd[2] = 0x01;
		cmd[3] = 0x00;
	} else {
		cmd[2] = 0x00;
		cmd[3] = 0x00;
	}

	if (!empty) {
		len = 16;
		/* convert hex string of identifier to bytecode */
		strtobin(file, &cmd[5], &len);
		cmd[4] = len & 0xff;
	}
	send_apdu(hCard, vty, cmd, 5+cmd[4], rsp, &lenr);
	parse_selectfile_response(hCard, vty, rsp, lenr);
	return CMD_SUCCESS;
}

DEFUN(send_7816_ls,
	send_7816_ls_cmd,
	"ls",
	"Tries to list all files on a 7816-4 compliant ICC\n")
{
	return CMD_SUCCESS;
}

DEFUN(send_7816_tree,
	send_7816_tree_cmd,
	"tree",
	"Tries to list a full DF hiararchy tree on a 7816-4 compliant ICC\n")
{

	return CMD_SUCCESS;
}

DEFUN(send_7816_read_binary,
	send_7816_read_binary_cmd,
	"read binary OFFSET LENGTH",
	"Read bytes form a previously selected EF\n")
{
	unsigned char cmd[] = { 0x00, 0xb0, 0x00, 0x00, 0x00 };
	unsigned char rsp[MAX_LENR]; // FIXME
	unsigned int lenr = MAX_LENR;
	
	unsigned long datalen;
	unsigned long offset;

	offset = strtoul(argv[0], NULL, 0);
	if (offset == ULONG_MAX || offset > 0xffff)
		return CMD_ERR_NO_MATCH;

	datalen = strtoul(argv[1], NULL, 0);
	if (datalen == ULONG_MAX || datalen > 0xff)
		return CMD_ERR_NO_MATCH;

	cmd[2] = (offset >> 8) & 0xff;
	cmd[3] = offset & 0xff;
	cmd[4] = datalen & 0xff;

	send_apdu(hCard, vty, cmd, 5+cmd[4], rsp, &lenr);
	if (lenr < 2)
		return CMD_SUCCESS; // FIXME

	parse_sw(vty, rsp[lenr-2], rsp[lenr-1]);

	return CMD_SUCCESS;
}

DEFUN(send_7816_write_binary,
	send_7816_write_binary_cmd,
	"write binary OFFSET LENGTH DATA",
	"Write bytes to a previously selected EF\n")
{
	unsigned char cmd[MAX_LENC];
	unsigned char rsp[MAX_LENR];
	unsigned int lenr;

	unsigned long datalen;
	unsigned long offset;

	offset = strtoul(argv[0], NULL, 0);
	if (offset == ULONG_MAX || offset > 0xffff)
		return CMD_ERR_NO_MATCH;

	datalen = strtoul(argv[1], NULL, 0);
	if (datalen == ULONG_MAX || datalen > 0xff)
		return CMD_ERR_NO_MATCH;

	memset(cmd, 0, sizeof(cmd));

	cmd[1] = 0xd0;
	cmd[2] = (offset >> 8) & 0xff;
	cmd[3] = offset & 0xff;

	if (!strtobin(argv[2], cmd+5, &datalen)) {
		vty_out(vty, "command decoding error%s", vty_newline(vty));
		return -1;
	}

	cmd[4] = datalen & 0xff;

	send_apdu(hCard, vty, cmd, 5+cmd[4], rsp, &lenr);
	if (lenr < 2)
		return CMD_SUCCESS; // FIXME

	parse_sw(vty, rsp[lenr-2], rsp[lenr-1]);

	return CMD_SUCCESS;

	return CMD_SUCCESS;
} 

DEFUN(send_7816_update_binary,
	send_7816_update_binary_cmd,
	"update binary OFFSET LENGTH DATA",
	"Update bytes of a previously selected EF\n")
{
	unsigned char cmd[MAX_LENC];
	unsigned char rsp[MAX_LENR];
	unsigned int lenr;

	unsigned long datalen;
	unsigned long offset;

	offset = strtoul(argv[0], NULL, 0);
	if (offset == ULONG_MAX || offset > 0xffff)
		return CMD_ERR_NO_MATCH;

	datalen = strtoul(argv[1], NULL, 0);
	if (datalen == ULONG_MAX || datalen > 0xff)
		return CMD_ERR_NO_MATCH;

	memset(cmd, 0, sizeof(cmd));

	cmd[1] = 0xd6;
	cmd[2] = (offset >> 8) & 0xff;
	cmd[3] = offset & 0xff;

	if (!strtobin(argv[2], cmd+5, &datalen)) {
		vty_out(vty, "command decoding error%s", vty_newline(vty));
		return -1;
	}

	cmd[4] = datalen & 0xff;

	send_apdu(hCard, vty, cmd, 5+cmd[4], rsp, &lenr);
	if (lenr < 2)
		return CMD_SUCCESS; // FIXME

	parse_sw(vty, rsp[lenr-2], rsp[lenr-1]);

	return CMD_SUCCESS;
} 

DEFUN(send_7816_erase_binary,
	send_7816_erase_binary_cmd,
	"erase binary OFFSET LENGTH",
	"Erase bytes of a previously selected EF\n")
{
	unsigned char cmd[8] = { 0x00, 0x0e, 0x00, 0x00,
				 0x00, 0x00, 0x00, 0x00 };
	unsigned char rsp[MAX_LENR];
	unsigned int lenr;

	unsigned long datalen;
	unsigned long offset;

	offset = strtoul(argv[0], NULL, 0);
	if (offset == ULONG_MAX || offset > 0xffff)
		return CMD_ERR_NO_MATCH;

	datalen = strtoul(argv[1], NULL, 0);
	if (datalen == ULONG_MAX || (offset+datalen > 0xffff))
		return CMD_ERR_NO_MATCH;

	cmd[2] = (offset >> 8) & 0xff;
	cmd[3] = offset & 0xff;
	cmd[4] = 0x02;
	cmd[5] = ((offset+datalen) >> 8) & 0xff;
	cmd[6] = (offset+datalen) & 0xff;

	send_apdu(hCard, vty, cmd, 5+cmd[4], rsp, &lenr);
	if (lenr < 2)
		return CMD_SUCCESS; // FIXME

	parse_sw(vty, rsp[lenr-2], rsp[lenr-1]);

	return CMD_SUCCESS;
}

DEFUN(send_7816_get_response,
	send_7816_get_response_cmd,
	"get response LENGTH",
	"Get more data from the ICC\n")
{
	unsigned char cmd[6] = { 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00 };
	unsigned char rsp[MAX_LENR];
	unsigned int lenr = MAX_LENR;

	unsigned long length;

	length = strtoul(argv[0], NULL, 0);
	if (length == ULONG_MAX || length > 0xff)
		return CMD_ERR_NO_MATCH;

	cmd[5] = length & 0xff;

	send_apdu(hCard, vty, cmd, 6, rsp, &lenr);
	if (lenr < 2)
		return CMD_SUCCESS; // FIXME

	parse_sw(vty, rsp[lenr-2], rsp[lenr-1]);

	return CMD_SUCCESS;

}
#endif
 
DEFUN(rc632,
	rc632_cmd,
	"rc632", "Commands related to low-level RC632 access\n")
{
	vty->node = RC632_NODE;
	return CMD_SUCCESS;
}

/* dummy */
static int send_config_write(struct vty *v)
{
	return CMD_SUCCESS;
}

struct cmd_node rc632_node = {
	RC632_NODE,
	"%s(rc632)# ",
	1,
};

static int opcdshell_init()
{
	cmd_init(1);
	vty_init();

	install_node(&rc632_node, send_config_write);

	install_element(RC632_NODE, &rc632_reg_read_cmd);

#if 0
	install_element(RC632_NODE, &send_7816_select_file_absolute_fid_cmd);
	install_element(RC632_NODE, &send_7816_select_file_absolute_path_cmd);
	install_element(RC632_NODE, &send_7816_select_file_relative_fid_cmd);
	install_element(RC632_NODE, &send_7816_select_file_relative_path_cmd);

	install_element(RC632_NODE, &send_7816_select_dir_cmd);
	install_element(RC632_NODE, &send_7816_ls_cmd);
	install_element(RC632_NODE, &send_7816_tree_cmd);

	install_element(RC632_NODE, &send_7816_read_binary_cmd);
	install_element(RC632_NODE, &send_7816_write_binary_cmd);
	install_element(RC632_NODE, &send_7816_update_binary_cmd);
	install_element(RC632_NODE, &send_7816_erase_binary_cmd);

	install_element(RC632_NODE, &send_7816_get_response_cmd);
#endif

	install_default(RC632_NODE);

	install_element(VIEW_NODE, &rc632_cmd);

	return 0;
}

static int opcdshell(void)
{
	struct vty *v;

	v = vty_create(0);
	if (!v)
		return -1;
	while (1) {
		vty_read(v);
	}
	return 0;
}


/***********************************************************************
 * main program, copied from pcsc-lite 'testpcsc.c'
 ***********************************************************************/

int main(int argc, char **argv)
{
	opcdshell_init();

	printf("\nopcd_shell (C) 2006 Harald Welte <hwelte@hmw-consulting.de>\n\n");

	opcdshell();

	return 0;
}
