#define LINUX 1

#include <stdio.h>

#define DIRECTION_NO_DATA       -1
#define DIRECTION_READ          -3
#define DIRECTION_WRITE         -2
#define DIRECTION_READWRITE     -4

#define STATUS_U                0x80
#define STATUS_O                0x40

typedef struct _lun_data_t lun_data_t;
typedef struct _target_data_t target_data_t;

typedef void (*scsi_command)(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status);

typedef struct _scsi_command_table {
	scsi_command	func;
	int 		cdb_size;
	int		direction;
} scsi_command_table;


struct _lun_data_t {
	struct _lun_data_t *next;
	unsigned char device_type;	/* 0x00 : SBC Disk */
					/* 0x05 : MMC CD/DVD */
	unsigned short block_size;	/*  512 for Disks */
					/* 2048 for CD/DVD */
	char *vendor;			/*  8 char ASCII name */
	char *productid;		/* 16 char ASCII name */
	char *revision;			/*  4 char ASCII version */
	char *serial_number;		/*  x char ascii */
	unsigned char lun;
	int luntype;
	int lunsizeinmb;
	char *filename;
	FILE *lun_fh;
	unsigned char cdb[16];	/* CDB saved here for DATA OUT PDUs */
	int edtl;
	scsi_command_table *cmds;

	/* MMC : CD/DVD specific fields */
	int	is_blank;	/* 0: the disk has a TOC */
				/* 1: the disk is blank, no TOC */
	int current_profile;    /*		0008	cdrom
						0009	cd-r
						000a	cd-rw
					0010	dvd-rom
					0011	dvd-r
						0014	dvd-rw
						001a	dvd+rw
						001b	dvd+r
				 */

};

#define TARGET_TYPE_EMULATION		0

struct _target_data_t {
	struct _target_data_t *next;
	int type;
	char *targetname;
	int pt_device;
	lun_data_t *luns;
	unsigned char cdb[16];   /* more this to LUN */
};
extern target_data_t *targets;


extern scsi_command_table scsi_mmc_commands[256];
extern scsi_command_table scsi_sbc_commands[256];

void spc_report_luns(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status);


void mmc_start_stop_unit(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status);
void mmc_send_key(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status);

void insert_new_dvd_r(lun_data_t *ld);

void parse_management_command(int c);

target_data_t *add_target(char *targetname, int targettype);
target_data_t *find_target(char *targetname);
void delete_target(char *targetname);

#define LUN_TYPE_HD		1
#define LUN_TYPE_DVD_R		2
#define LUN_TYPE_DVD_ROM	3
lun_data_t *add_lun(target_data_t *t, int lun, char *lunfile, int luntype, int lunsize);
