/* make sure targetnames are all lowercase and no _ */
/* make sure we actually negotiate the target or abort */
/* must force datapduinorder during login */
#define _FILE_OFFSET_BITS 64

#include "scsi.h"
#include "vscsi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/mman.h>
#ifdef LINUX
#include <scsi/sg.h>
#endif
#include <time.h>



lun_data_t LUN_HD;
lun_data_t LUN_DVD_R;
lun_data_t LUN_DVD_ROM;
/* we must have a dummy lun since REPORT LUNS is called before the initiator
   knows which luns exist
*/
lun_data_t DUMMY_LUN_0;
target_data_t TARGET_HD;
target_data_t TARGET_DVD_R;
target_data_t TARGET_DVD_ROM;

target_data_t *targets=NULL;

/* the target we logged in to, is set in the parsing of login pdus */
target_data_t *target=NULL;


unsigned char
run_scsi_command(target_data_t *td, unsigned char lun, unsigned char *cdb, unsigned char *data, int data_size)
{
	unsigned char opcode;
	unsigned char status=0;
	lun_data_t *ld;

	opcode=cdb[0];

	for(ld=td->luns;ld;ld=ld->next){
		if(ld->lun==lun){
			break;
		}
	}
	/* REPORT LUNS always come in on lun0, make sure we have lun0 */
	if((!ld)&&(lun==0)){
		ld=&DUMMY_LUN_0;
	}


	if(td->type==TARGET_TYPE_EMULATION){
		if(ld->cmds[opcode].func){
			(*(ld->cmds[opcode].func))(td, ld, cdb, data, &data_size, &status);
		} else {
			status=2;
		}
	}
	return status;
}

void
insert_new_dvd_r(lun_data_t *ld)
{
	if(ld->lun_fh){
		off_t total_size;
		fseeko(ld->lun_fh,0,SEEK_END);
		total_size=ftello(ld->lun_fh);
		fclose(ld->lun_fh);
		ld->lun_fh=NULL;
		if(total_size){
			char filename[256];
			sprintf(filename,"%08x_%d.iso",time(NULL),getpid());
			rename(ld->filename, filename);
		}
	}
	ld->lun_fh=fopen(ld->filename, "w+");
	if(!ld->lun_fh){
		printf("could not open DVD-R raw image :%s\n",ld->filename);
		exit(10);
	}
}


target_data_t *
add_target(char *targetname, int targettype)
{
	target_data_t *td;
	td=malloc(sizeof(target_data_t));
	td->next=targets;
	targets=td;

	td->targetname=strdup(targetname);
	td->luns=NULL;
	td->type=targettype;
	td->pt_device=0;	
	return td;
}
target_data_t *
find_target(char *targetname)
{
	target_data_t *td;
	for(td=targets;td;td=td->next){
		if(!strcmp(targetname, td->targetname))
			break;
	}
	return td;
}
void
delete_target(char *targetname)
{
	target_data_t *t;
	if(!targets){
		return;
	}
	if(!strcmp(targets->targetname, targetname)){
		targets=targets->next;
		return;
	}
	for(t=targets;t;t=t->next){
		if(t->next){
			if(!strcmp(t->next->targetname, targetname)){
				t->next=t->next->next;
				return;
			}
		}
	}
}

lun_data_t *
add_lun(target_data_t *t, int lun, char *lunfile, int luntype, int lunsizeinmb)
{
	off_t lunsize;
	lun_data_t *l;
	char buf[25];

	lunsize=lunsizeinmb;
	lunsize*=1024;
	lunsize*=1024;

	l=malloc(sizeof(lun_data_t));
	switch(luntype){
	case LUN_TYPE_HD:
		l->device_type=0x00;	/*HardDisk*/
		l->is_blank=0;
		l->block_size=512;
		l->vendor="VirtualHD";
		l->productid="VHD";
		l->revision="0";
		sprintf(buf,"%08x",time(NULL));
		l->serial_number=strdup(buf);
		l->cmds=scsi_sbc_commands;
		l->filename=strdup(lunfile);
		l->lun_fh=fopen(lunfile, "r+");
		if(!l->lun_fh){
			l->lun_fh=fopen(lunfile, "w+");
		}
		if(!l->lun_fh){
printf("could not create lun file :%s\n",lunfile);
			return NULL;
		}
		truncate(lunfile, lunsize);
		break;
	case LUN_TYPE_DVD_R:
		l->device_type=0x05;		/*CD/DVD*/
		l->current_profile=0x0011;	/* DVD-R */
		l->is_blank=1;
		l->block_size=2048;
		l->vendor="VirtualDVDR";
		l->productid="VdR";
		l->revision="0";
		sprintf(buf,"%08x",time(NULL));
		l->serial_number=strdup(buf);
		l->cmds=scsi_mmc_commands;
		l->filename=strdup(lunfile);
		l->lun_fh=fopen(lunfile, "w+");
		if(!l->lun_fh){
printf("could not create lun file :%s\n",lunfile);
			return NULL;
		}
		break;
	case LUN_TYPE_DVD_ROM:
		l->device_type=0x05;		/*CD/DVD*/
		l->current_profile=0x0010;	/* DVD-R */
		l->is_blank=0;
		l->block_size=2048;
		l->vendor="VirtualDVDROM";
		l->productid="VdROM";
		l->revision="0";
		sprintf(buf,"%08x",time(NULL));
		l->serial_number=strdup(buf);
		l->cmds=scsi_mmc_commands;
		l->filename=strdup(lunfile);
		l->lun_fh=fopen(lunfile, "r");
		if(!l->lun_fh){
printf("could not create lun file :%s\n",lunfile);
			return NULL;
		}
		break;
	}
	l->next=t->luns;
	t->luns=l;
	l->lun=lun;
	l->luntype=luntype;
	l->lunsizeinmb=lunsizeinmb;
	return l;
}

