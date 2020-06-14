#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include "scsi.h"

#define MIN(x,y) ((x<y)?x:y)



#define ILLEGAL_REQUEST \
	{ \
	static char illegal_request[28]={ 0x00, 0x1c, /*len*/	\
		/* sense data */				\
		0x70,0x00,0x05,0x00,  0x00,0x00,0x00,0x12,	\
		0x00,0x00,0x00,0x00,  0x24,0x00,0x00,0x00,	\
		0x00,0x00,0x00,0x01,  0x02,0x03,0x04,0x05,	\
		0x06,0x07 };					\
								\
		*data_size=28;					\
		*status=2; /* check condition */		\
		memcpy(data,illegal_request,*data_size);	\
		return;						\
	}


void
spc_report_luns(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	long alloc_len;
	lun_data_t *tmplun;
	int numluns, data_required;

	/* get the allocation length
	 * i.e. max amount of data we are allowed to return
	 */
	alloc_len=ntohl( *((long *)(cdb+6)) );
	if(alloc_len>*data_size){
		*status=2;return;
	}

	*data_size=alloc_len;
	memset(data, 0, alloc_len);

	/* count how many luns we have on this target
	 * and fill the data in until we run out of space in the 
	 * availabel buffer
	 */
	numluns=0;
	data_required=4;
	for(tmplun=td->luns;tmplun;tmplun=tmplun->next){
		data_required=8+1+numluns*8;
		if((8+1+numluns*8)<=alloc_len){
			data[8+1+numluns*8]=tmplun->lun;
		} else {
			/* not enough space in alloc_len
			 * so dont do anything
			 */
		}
		numluns++;
	}
	*((long *)data)=htonl(numluns*8);

	*status=0;
}

static void
spc_test_unit_ready(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	*data_size=0;
	*status=0;
}

static void
spc_request_sense(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	static char sense_is_ok[26]={ 
		/* sense data */
		0x70,0x00,0x00,0x00,  0x00,0x00,0x00,0x12,
		0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x01,  0x02,0x03,0x04,0x05,
		0x06,0x07 };

	unsigned char desc, alloc_len;
	desc=cdb[1]&0x01;
	alloc_len=cdb[4];
if(desc){
*status=2;return;
}
	memcpy(data,sense_is_ok,26);

	*data_size=alloc_len;
	*status=0;
}

static void
mmc_read_buffer_capacity(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	unsigned char block;
	unsigned short alloc_len;

	block=cdb[1]&0x01;
	alloc_len=ntohs( *((short *)(cdb+7)) );
if(block){
*status=2;return;
}

	/* data length */
	data[0]=0x00;
	data[1]=0x0a;
	/* reserved */
	data[2]=0x00;
	data[3]=0x00;
	/* length of buffer */
	data[4]=0x00;
	data[5]=0x13;
	data[6]=0x00;
	data[7]=0x00;
	/* available length of buffer */
	data[8]=0x00;
	data[9]=0x13;
	data[10]=0x00;
	data[11]=0x00;

	
	*data_size=12;
	*status=0;
}

static void
mmc_synchronize_cache(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	*data_size=0;
	*status=0;
}

static void
mmc_reserve_track(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	*data_size=0;
	*status=0;
}

static void
sbc_verify10(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	*data_size=0;
	*status=0;
}

static void
sbc_verify12(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	*data_size=0;
	*status=0;
}

static void
sbc_verify16(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	*data_size=0;
	*status=0;
}

static void
sbc_read_capacity10(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	off_t total_size;
	long blocks;

	memset(data, 0, 8);
	fseeko(ld->lun_fh,0,SEEK_END);
	total_size=ftello(ld->lun_fh);
	blocks=total_size/ld->block_size;
	if((total_size/ld->block_size)!=blocks){
		blocks--; /* round down to nearest block size */
	}

	blocks--;

	/* number of blocks */
	*((long *)(data+0))=htonl(blocks);

	/* block size */
	*((long *)(data+4))=htonl(ld->block_size);

	*data_size=8;
	*status=0;
}

static void
mmc_read_disc_information(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	unsigned short alloc_len;

	alloc_len=ntohs( *((short *)(cdb+7)) );

	memset(data, 0, alloc_len);

	switch(ld->current_profile){
	case 0x0011:	/* DVD-R */
		/* disc info len */
		data[0]=0x00;
		data[1]=0x20;
		/* not erasable, last session state empty, disc status empty*/ 
		data[2]=0x00;
		/* number of first track on disk */
		data[3]=0x01;
		/* number of sessions */
		data[4]=0x01;	/*lsb*/
		data[9]=0x00;	/*msb*/
		/*first track number */
		data[5]=0x01;	/*lsb*/
		data[10]=0x00;	/*msb*/
		/*last track number */
		data[6]=0x01;	/*lsb*/
		data[11]=0x00;	/*msb*/
		/* unrestricted use disk */
		data[7]=0x20;
		/* disc type */
		data[8]=0x00;
		break;
	case 0x0010:	/* DVD-ROM */
		/* disc info len */
		data[0]=0x00;
		data[1]=0x20;
		/* not erasable, last session state complete, disc status finalized*/ 
		data[2]=0x0e;
		/* number of first track on disk */
		data[3]=0x01;
		/* number of sessions */
		data[4]=0x01;	/*lsb*/
		data[9]=0x00;	/*msb*/
		/*first track number */
		data[5]=0x01;	/*lsb*/
		data[10]=0x00;	/*msb*/
		/*last track number */
		data[6]=0x01;	/*lsb*/
		data[11]=0x00;	/*msb*/
		/* unrestricted use disk */
		data[7]=0x20;
		/* disc type */
		data[8]=0x00;
		break;
	}

	*data_size=alloc_len;
	*status=0;
}

/*XXX should keep track of free blocks here */
static void
mmc_read_track_information(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	unsigned char address;
	unsigned short alloc_len;
	unsigned long lba;
	off_t total_size;
	long blocks;

	address=cdb[1]&0x03;
	lba=ntohl( *((long *)(cdb+2)) );
	alloc_len=ntohs( *((short *)(cdb+7)) );

if((address!=1)||(!lba)){
*status=2;return;
}
	memset(data, 0, alloc_len);

	switch(ld->current_profile){
	case 0x0011:	/* DVD-R */
		/* data length */
		data[0]=0x00;		
		data[1]=0x22;
		/* track number */
		data[2]=0x01;		
		data[32]=0x00;		
		/* session number */
		data[3]=0x01;		
		data[33]=0x00;
		/* reserved */		
		data[4]=0x00;		
		/* no damage, not a copy, track mode:data */
		data[5]=0x04;		
		/* all ecc blocks are blank, mode 1 iso/iec 10149 */
		data[6]=0x61;
		/* lra_v:1 nwa_v:1 */
		data[7]=0x03;
		/* track start address */
		data[8]=0x00;		
		data[9]=0x00;		
		data[10]=0x00;		
		data[11]=0x00;		
		/* next writeable address */
		data[12]=0x00;		
		data[13]=0x00;		
		data[14]=0x00;		
		data[15]=0x00;		
		/* free blocks */
		data[16]=0x00;		
		data[17]=0x23;		
		data[18]=0x10;		
		data[19]=0x20;		
		/* fixed packet size/blocking factor */
		data[20]=0x00;		
		data[21]=0x00;		
		data[22]=0x00;		
		data[23]=0x10;		
		/* track size */
		data[24]=0x00;		
		data[25]=0x23;		
		data[26]=0x10;		
		data[27]=0x20;		
		/* last recorded address */
		data[28]=0x00;
		data[29]=0x00;
		data[30]=0x00;
		data[31]=0x00;
		break;
	case 0x0010:	/* DVD-ROM */
		/* data length */
		data[0]=0x00;		
		data[1]=0x22;
		/* track number */
		data[2]=0x01;		
		data[32]=0x00;		
		/* session number */
		data[3]=0x01;		
		data[33]=0x00;
		/* reserved */		
		data[4]=0x00;		
		/* no damage, not a copy, track mode:data */
		data[5]=0x04;		
		/* mode 1 iso/iec 10149 */
		data[6]=0x01;
		/* lra_v:0 nwa_v:0 */
		data[7]=0x00;
		/* track start address */
		data[8]=0x00;		
		data[9]=0x00;		
		data[10]=0x00;		
		data[11]=0x00;		
		/* next writeable address */
		data[12]=0x00;		
		data[13]=0x00;		
		data[14]=0x00;		
		data[15]=0x00;		
		/* free blocks */
		data[16]=0x00;		
		data[17]=0x00;		
		data[18]=0x00;		
		data[19]=0x00;		
		/* fixed packet size/blocking factor */
		data[20]=0x00;		
		data[21]=0x00;		
		data[22]=0x00;		
		data[23]=0x10;		
		/* track size */
		total_size=ftello(ld->lun_fh);
		blocks=total_size/ld->block_size;
		if((total_size/ld->block_size)!=blocks){
			blocks--; /* round down to nearest block size */
		}
		data[24]=(blocks>>24)&0xff;
		data[25]=(blocks>>16)&0xff;
		data[26]=(blocks>> 8)&0xff;
		data[27]=(blocks    )&0xff;
		/* last recorded address */
		data[28]=0x00;
		data[29]=0x00;
		data[30]=0x00;
		data[31]=0x00;
		break;
	}

	*data_size=alloc_len;
	*status=0;
}


static void
spc_mode_sense6(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	unsigned char alloc_len;
	unsigned char scode, pcode, pc, dbd;
	dbd=(cdb[1]>>3)&0x01;
	pc=(cdb[2]>>6)&0x03;
	pcode=cdb[2]&0x3f;
	scode=cdb[3];
	alloc_len=cdb[4];

	/* return all pages */
	if((dbd==0)&&(pc==0)&&(pcode==0x3f)){
		int numblocks, pos;

		numblocks=0;
		pos=4;

		/* block descriptors */
		/* write all block descriptors (only one block so far)*/
		numblocks++;
		data[pos++]=0; /* density code */
		data[pos++]=0; /* num blocks MSB ==0 ==ALL blocks*/
		data[pos++]=0; /* num blocks */
		data[pos++]=0; /* num blocks LSB*/
		data[pos++]=0; /* reserved */
		data[pos++]=0; /* block len MSB  512*/
		data[pos++]=0x02; /* block len*/
		data[pos++]=0; /* block len LSB*/


		/* mode pages */
		/* read write recovery page */
		data[pos++]=0x01;
		data[pos++]=0x0a;
		data[pos++]=0xe4;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		/* disconnect reconnect mode page */
		data[pos++]=0x02;
		data[pos++]=0x0e;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0x08;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		/* format device mode page */
		data[pos++]=0x03;
		data[pos++]=0x16;
		data[pos++]=0;
		data[pos++]=0x01;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0x10;
		data[pos++]=0x02;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0x01;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0x40;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		/* rigid disk geometry mode page */
		data[pos++]=0x04;
		data[pos++]=0x16;
		data[pos++]=0x02;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0x01;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0x11;
		data[pos++]=0x30;
		data[pos++]=0;
		data[pos++]=0;
		/* control mode page */
		data[pos++]=0x0a;
		data[pos++]=0x0a;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0;
		data[pos++]=0xff;
		data[pos++]=0xff;
		data[pos++]=0;
		data[pos++]=0;
		/* protocol specific lun mode page */
		data[pos++]=0x18;
		data[pos++]=0x01;
		data[pos++]=0x05;
		/* protocol specific port mode page */
		data[pos++]=0x19;
		data[pos++]=0x01;
		data[pos++]=0x05;


		/* mode6 parameter header */
		data[0]=(unsigned char)pos-1; /* mode data length */
		data[1]=0x00; /* medium type */
		data[2]=0x80; /* device specific parameter  WP writeprotect*/ 
		data[2]=0x00; /* device specific parameter  !WP writeprotect*/ 
		data[3]=(unsigned char)numblocks*8; /*block descriptor length 8xnumber of blocks*/

		*status=0;
		*data_size=pos;
		return;
	}

	/* we dont support informational exceptions control */
	/*if((dbd==0)&&(pc==0)&&(pcode==0x1c)){*/
	if(1){
		static unsigned char sense_data[18]={0x00,0x12,
			0x70,0x00,0x05,0x00, 0x00,0x00,0x00,0x0a,
			0x00,0x00,0x00,0x00 ,0x24,0x00,0x00,0x00};
		*data_size=18;
		*status=0;
		memset(data,0,18);		
//		memcpy(data, sense_data, 18);
		return;
	}

	/* fake it, just return nothing */
	*data_size=0;
	*status=0;
}

static void
spc_mode_sense10(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	/* fake it, just return nothing */
	*data_size=0;
	*status=0;
}

static void
mmc_prevent_allow_media_removal(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	unsigned char prev;

	prev=cdb[4]&0x03;

	*data_size=0;
	*status=0;
}
void
mmc_start_stop_unit(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	unsigned char ej;

	ej=cdb[4]&0x03;

	switch(ld->current_profile){
	case 0x0011:	/* DVD-R */
		/* if this was a DVD-R  we should swap disks now */
		insert_new_dvd_r(ld);
		break;
	}
	*data_size=0;
	*status=0;
}
static void
mmc_set_cd_speed(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	/* fake it, just return good */
	*data_size=0;
	*status=0;
}
static void
mmc_set_streaming(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	/* fake it, just return good */
	*data_size=0;
	*status=0;
}

static void
spc_prevent_allow_media_removal(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	/* fake it, just return good */
	*data_size=0;
	*status=0;
}

static void
mmc_read_toc(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	short alloc_len;
	int time_flag;
	int format;
	int track;
	int control;

	time_flag=((cdb[1]&0x02)==0x02);
	format=cdb[2]&0x0f;
	track=cdb[6];
	alloc_len=ntohs( *((short *)(cdb+7)) );
	control=cdb[9];

	memset(data, 0, alloc_len);

	/* if the device is supposed to be blank we must return a 
	 * check_condition 
	 */
	if(ld->is_blank){
		static unsigned char sense_data[20]={0x00,0x12,
			0x70,0x00,0x05,0x00, 0x00,0x00,0x00,0x12,
			0x00,0x00,0x00,0x00 ,0x24,0x00,0x00,0x00,
			0x00,0x00};
		*data_size=20;
		*status=2;
		memcpy(data, sense_data, 20);
		return;
	}

	memset(data, 0, alloc_len);

	switch(format){
	case 0:
		break;
	default:
		*status=2;return;
	}

	/* length 10 */
	data[0]=0x00;
	data[1]=0x0a;

	data[2]=1; /* first track */
	data[3]=1; /* last track */

	/* faking 1 session   format 0 */
	data[4]=0; 	/* reserved */
	data[5]=0x14;	/* adr/ctrl */
	data[6]=0x01;	/* track */
	data[7]=0;		/* reserved */

	data[8]=0;		/* track start address */
	data[9]=0;
	data[10]=0;
	data[11]=0;

	*data_size=alloc_len;
	*status=0;
}


static void
mmc_get_performance(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	int data_type;
	unsigned char max_num_desc;
	unsigned long lba;
	unsigned char type;
	unsigned short alloc_len;

	static char illegal_request[28]={ 0x00, 0x1c, /*len*/
		/* sense data */
		0x70,0x00,0x05,0x00,  0x00,0x00,0x00,0x12,
		0x00,0x00,0x00,0x00,  0x24,0x00,0x00,0x00,
		0x00,0x00,0x00,0x01,  0x02,0x03,0x04,0x05,
		0x06,0x07 };

	alloc_len=0;
	data_type=cdb[1]&0x1f;
	lba=ntohl( *((long *)(cdb+2)) );
	max_num_desc=ntohs( *((short *)(cdb+8)) );
	type=cdb[10];

	switch(ld->current_profile){
	case 0x0011:	/* DVD-R */
		switch(type){
		case 0:
			/* header */
			data[0]=0x00;
			data[1]=0x00;
			data[2]=0x00;
			data[3]=0x14;
			data[4]=0x02;
			data[5]=0x00;
			data[6]=0x00;
			data[7]=0x00;
			/* data */
			data[8]=0x00;
			data[9]=0x00;
			data[10]=0x00;
			data[11]=0x00;
			data[12]=0x00;
			data[13]=0x00;
			data[14]=0x15;
			data[15]=0xa4;
			data[16]=0x00;
			data[17]=0x23;
			data[18]=0x10;
			data[19]=0x1f;
			data[20]=0x00;
			data[21]=0x00;
			data[22]=0x15;
			data[23]=0xa4;
			alloc_len=24;
			break;
		case 3: /* write speed descriptor */
			/* size of struct */
			data[0]=0x00;	
			data[1]=0x00;			
			data[2]=0x00;			
			data[3]=0x24;
			/* reserved */
			data[4]=0x00;			
			data[5]=0x00;			
			data[6]=0x00;			
			data[7]=0x00;
			/* flags */			
			data[8]=0x00;			
			data[9]=0x00;			
			data[10]=0x00;			
			data[11]=0x00;
			/* end lba */			
			data[12]=0x00;			
			data[13]=0x23;			
			data[14]=0x10;			
			data[15]=0x1f;
			/* read speed */			
			data[16]=0x00;			
			data[17]=0x00;			
			data[18]=0x12;			
			data[19]=0xb3;
			/* write speed */			
			data[20]=0x00;			
			data[21]=0x00;			
			data[22]=0x15;			
			data[23]=0xa4;			
			/* flags */			
			data[24]=0x00;			
			data[25]=0x00;			
			data[26]=0x00;			
			data[27]=0x00;
			/* end lba */			
			data[28]=0x00;			
			data[29]=0x23;			
			data[30]=0x10;			
			data[31]=0x1f;
			/* read speed */			
			data[32]=0x00;			
			data[33]=0x00;			
			data[34]=0x12;			
			data[35]=0xb3;
			/* write speed */			
			data[36]=0x00;			
			data[37]=0x00;			
			data[38]=0x0c;			
			data[39]=0xfc;
			alloc_len=40;
			break;
		default:
*status=2;return;
		}
		break;
	case 0x0010:	/* DVD-ROM */
		switch(type){
		case 0:
			data[0]=0x00;
			data[1]=0x00;
			data[2]=0x00;
			data[3]=0x04;
			data[4]=0x02;
			data[5]=0x00;
			data[6]=0x00;
			data[7]=0x00;
			alloc_len=8;
			break;
		case 3:
		default:

			*status=2; /* check condition */		
			*data_size=sizeof(illegal_request);
			memcpy(data,illegal_request,*data_size);
			return;
			break;
		}
	}

	*data_size=alloc_len;
	*status=0;
}

static void
mmc_read_dvd_structure(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	unsigned long address;
	unsigned short alloc_len, agid;
	unsigned char format, layer;
	off_t total_size;
	long blocks;
	static char illegal_request[28]={ 0x00, 0x1c, /*len*/
		/* sense data */
		0x70,0x00,0x05,0x00,  0x00,0x00,0x00,0x12,
		0x00,0x00,0x00,0x00,  0x24,0x00,0x00,0x00,
		0x00,0x00,0x00,0x01,  0x02,0x03,0x04,0x05,
		0x06,0x07 };

	alloc_len=ntohs( *((short *)(cdb+8)) );
	format=cdb[7];
	layer=cdb[6];
	address=ntohl( *((long *)(cdb+2)) );
	agid=((cdb[10]&0x80)==0x80);


	memset(data, 0, alloc_len);

	switch(format){
	case 0:
		/* dvd structure data length : 520 bytes */
		data[0]=8;	
		data[1]=2;	

		/* reserved */
		data[2]=0;	
		data[3]=0;	

		switch(ld->current_profile){
		case 0x0011:	/* DVD-R */
			data[ 4]=0x25; 	/* book:DVD-R version 5 */	
			data[ 5]=0x0f;	/* disk size:120mm  no rate */
			data[ 6]=0x02;	/* 1 layer  layer contains rewriteable area */	
			data[ 7]=0x00;	/* density */	

			data[ 9]=0x03;	/* start sector 0x300000 for dvd-r */
			data[10]=0x00;	
			data[11]=0x00;	

			break;
		case 0x0010:	/* DVD-ROM */
			data[ 4]=0x01; 	/* book:DVD-ROM version 1 */	
			data[ 5]=0x02;	/* disk size 120mm  max rate 10mbit */
			data[ 6]=0x01;	/* 1 layer  recordable data */	
			data[ 7]=0x00;	/* density */	

			data[ 9]=0x03;	/* start sector 0x300000 for dvd-rom */
			data[10]=0x00;	
			data[11]=0x00;	

			/* only one lun for now: lun 1 */
			fseeko(ld->lun_fh,0,SEEK_END);
			total_size=ftello(ld->lun_fh);
			blocks=total_size/ld->block_size;		
			if((total_size/ld->block_size)!=blocks){
				blocks--; /* round down to nearest block size */
			}

			/* last block */
			data[13]=(blocks>>24)&0xff;
			data[14]=(blocks>>16)&0xff;
			data[15]=(blocks    )&0xff;
			break;
		}
		break;
	case 1:
		data[0]=0;	
		data[1]=6;	
		break;
	case 0x0d: /* prerecorded information from dvd-r/-rw lead in */
		switch(ld->current_profile){
		case 0x0011:	/* DVD-R */
			/* data length */
			data[0]=0x80;
			data[1]=0x06;
			/* the rest is just 0 */
			break;
		case 0x0010:	/* DVD-ROM */
			*status=2; /* check condition */		
			*data_size=sizeof(illegal_request);
			memcpy(data,illegal_request,*data_size);
			return;
			break;
		}
		break;
	case 0x0e: /* prerecorded information from dvd-r/-rw lead in */
		switch(ld->current_profile){
		case 0x0011:	/* DVD-R */
			/* data length */
			data[0]=0x00;
			data[1]=0x6c;
			/* reserved */
			data[2]=0x00;
			data[3]=0x00;
			/* field id */
			data[4]=0x01;
			/* disc application code */
			data[5]=0x40;
			/* disc physical code */
			data[6]=0xc1;
			/* last address of data recordable area */
			data[7]=0xfd;
			data[8]=0x9e;
			data[9]=0xd8;
			/* part version/extension code */
			data[10]=0x58;
			/* reserved */
			data[11]=0x00;
			/* field id */
			data[12]=0x02;
			/* opc suggested code */
			data[13]=0x35;
			/*wavelength code */
			data[14]=0x0d;
			/* write strategy code */
			data[15]=0x0e;
			data[16]=0x88;
			data[17]=0x9a;
			data[18]=0x80;
			/* reserved */
			data[19]=0x00;
			/* field id */
			data[20]=0x03;
			/* manufacturer id */
			data[21]=0x43;
			data[22]=0x4d;
			data[23]=0x43;
			data[24]=0x20;
			data[25]=0x4d;
			data[26]=0x41;
			/* reserved */
			data[27]=0x00;
			/* field id */
			data[28]=0x04;
			/* manufacturer id */
			data[29]=0x47;
			data[30]=0x2e;
			data[31]=0x20;
			data[32]=0x41;
			data[33]=0x45;
			data[34]=0x31;
			/* reserved */
			data[35]=0x00;
			/* field id */
			data[36]=0x05;
			/* manufacturer id */
			data[37]=0x00;
			data[38]=0x00;
			data[39]=0x00;
			data[40]=0x00;
			data[41]=0x00;
			data[42]=0x02;
			/* reserved */
			data[43]=0x00;
			/* field id */
			data[44]=0x06;
			data[45]=0x09;
			data[46]=0x0d;
			data[47]=0x11;
			data[48]=0x87;
			data[49]=0x78;
			data[50]=0x90;
			data[51]=0x00;
			data[52]=0x07;
			data[53]=0x88;
			data[54]=0x80;
			data[55]=0x00;
			data[56]=0x00;
			data[57]=0x00;
			data[58]=0x00;
			data[59]=0x00;
			data[60]=0x08;
			data[61]=0x05;
			data[62]=0x15;
			data[63]=0x0d;
			data[64]=0x0f;
			data[65]=0x0a;
			data[66]=0x09;
			data[67]=0x00;
			break;
		case 0x0010:	/* DVD-ROM */
			*status=2; /* check condition */		
			*data_size=sizeof(illegal_request);
			memcpy(data,illegal_request,*data_size);
			return;
			break;
		}
		break;
	case 0x11:
/*XXX we dont do +R yet */
		*status=2; /* check condition */		
		*data_size=sizeof(illegal_request);
		memcpy(data,illegal_request,*data_size);
		return;
	default:
		*status=2;return;
	}

	*data_size=alloc_len;
	*status=0;
}


static void
mmc_report_key(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	unsigned short alloc_len, agid;
	unsigned char key_format, key_class;
	unsigned long lba;

	alloc_len=ntohs( *((short *)(cdb+8)) );
	agid=((cdb[10])&0xc0);
	key_format=cdb[10]&0x3f;
	key_class=cdb[7];
	lba=ntohl( *((long *)(cdb+2)) );

	/* only handle format==8 */
	if((key_format!=8)){
		*status=2;return;
	}

	memset(data, 0, alloc_len);

	/* key: 6 bytes */
	data[0]=0;	
	data[1]=6;	

	data[4]=0x63;	
	data[5]=0xf7;	
	data[6]=0x01;	

	*data_size=alloc_len;
	*status=0;
}


/* profiles we support : */
#define PROFILE_LIST_SIZE 8
unsigned short profile_list[PROFILE_LIST_SIZE]={
	0x0008,	/* cdrom */
	0x0009,	/* cd-r */
	0x000a,	/* cd-rw */
	0x0010,	/* dvd-rom */
	0x0011,	/* dvd-r */
	0x0014,	/* dvd-rw */
	0x001a,	/* dvd+rw */
	0x001b,	/* dvd+r */
};
/* features  in each profile */
/* profile:0x11 DVD-R sequential   mmc:5.4.12 
   mandatory features :
	0x0000   profile list
	0x0001   core
	0x0002   morhping
	0x0003   removable medium
	0x0010   random readable
	0x001f   dvd read
	0x0021   incremental streaming writable
	0x002f   dvd-r/-rw write
	0x0100   power management
	0x0105   timeout
	0x0107   real-time streaming
	0x0108   logical unit serial number
*/
static void
mmc_get_configuration(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	unsigned char rt;
	unsigned short starting_feature_number, alloc_len;
	unsigned char *ptr, *al;
	int i;

	rt=cdb[1]&0x03;
	starting_feature_number=ntohs( *((short *)(cdb+2)) );
	alloc_len=ntohs( *((short *)(cdb+7)) );
/*XXX
if(rt!=2){
*status=2;return;
}
*/
	memset(data, 0, alloc_len);

	ptr=data+8;
	*ptr++=(starting_feature_number>>8)&0xff;	/* feature code */
	*ptr++=(starting_feature_number   )&0xff;
	switch(starting_feature_number){
	case 0x0000:   /* profile list MMC:5.3.1*/
		*ptr++=0x03;	/* persistent and current */
		al=ptr;	/* save additional len for later */
		for(i=0;i<PROFILE_LIST_SIZE;i++){
			*++ptr=(profile_list[i]>>8)&0xff; /* profile number */
			*++ptr=(profile_list[i]   )&0xff;
			*++ptr=(profile_list[i]==ld->current_profile)?0x01:0x00;
			*++ptr=0;			  /* reserved */
		}
		*al=(ptr-al);
		break;
	case 0x0001:   /* core  MMC:5.3.2*/
		*ptr++=0x07;	/* v1 persistent and current */
		al=ptr;	/* save additional len for later */
		/* physical interface standard : SCSI */
		*++ptr=0;
		*++ptr=0;
		*++ptr=0;
		*++ptr=1;
		/* 4 reserved bytes */
		*++ptr=0;
		*++ptr=0;
		*++ptr=0;
		*++ptr=0;
		*al=(ptr-al);
		break;
	case 0x0002:   /* morhping MMC:5.3.3 */
		*ptr++=0x03;	/* persistent and current */
		al=ptr;	/* save additional len for later */
		*++ptr=0; /*ocevent=0 async=0*/
		/* three reserved bytes */
		*++ptr=0;
		*++ptr=0;
		*++ptr=0;
		*al=(ptr-al);
		break;
	case 0x0003:   /* removable medium MMC:5.3.4*/
		*ptr++=0x03;	/* persistent and current */
		al=ptr;	/* save additional len for later */
		/* bit 0: can lock media */
		/* bit 2: prevent jumper */
		/* bit 3: eject capability */
		/* bit 5-7: loading mechanism type */
		*++ptr=0x29;
		/* three reserved bytes */
		*++ptr=0;
		*++ptr=0;
		*++ptr=0;
		*al=(ptr-al);
		break;
	case 0x0010:   /* random readable  MMC:5.3.6*/
		switch(ld->current_profile){
		case 0x0011:	/* DVD-R */
			/* can not read from this disk */
			*ptr++=0x00;	/* persistent and current */
			break;
		case 0x0010:	/* DVD-ROM */
			/* can read from this disk */
			*ptr++=0x01;	/* persistent and current */
			break;
		}
		al=ptr;	/* save additional len for later */
		/* logical block size */
		*++ptr=(ld->block_size>>24)&0xff;
		*++ptr=(ld->block_size>>16)&0xff;
		*++ptr=(ld->block_size>> 8)&0xff;
		*++ptr=(ld->block_size    )&0xff;
		/* blocking */
		*++ptr=0;
		*++ptr=0x10;
		/* pp */
		*++ptr=0;
		/* reserved */
		*++ptr=0;
		*al=(ptr-al);
		break;
	case 0x001f:   /* dvd read  MMC:5.3.9 */
		switch(ld->current_profile){
		case 0x0011:	/* DVD-R */
			/* can not read from this disk */
			*ptr++=0x00;	/* persistent and current */
			break;
		case 0x0010:	/* DVD-ROM */
			/* can read from this disk */
			*ptr++=0x01;	/* persistent and current */
			break;
		}
		al=ptr;	/* save additional len for later */
		/* nothing */
		*al=(ptr-al);
		break;
	case 0x0021:   /* incremental streaming writable  MMC:5.3.11*/
		switch(ld->current_profile){
		case 0x0011:	/* DVD-R */
			/* can write to this disk */
			*ptr++=0x05;	/* persistent and current */
			break;
		case 0x0010:	/* DVD-ROM */
			/* can not write to this disk */
			*ptr++=0x04;	/* persistent and current */
			break;
		}
		al=ptr;	/* save additional len for later */
		*++ptr=0x3f;
		*++ptr=0x0b;
		*++ptr=0x01;
		*++ptr=0x01;
		*++ptr=0;
		*++ptr=0;
		*++ptr=0;
		*++ptr=0;
		*al=(ptr-al);
		break;
	case 0x002f:   /* dvd-r/-rw write  MMC:5.3.25*/
		switch(ld->current_profile){
		case 0x0011:	/* DVD-R */
			/* can write to this disk */
			*ptr++=0x05;	/* persistent and current */
			break;
		case 0x0010:	/* DVD-ROM */
			/* can not write to this disk */
			*ptr++=0x04;	/* persistent and current */
			break;
		}
		al=ptr;	/* save additional len for later */
		/* BUF test dvd-rw */
		*++ptr=0x46;
		/* reserved */
		*++ptr=0;
		*++ptr=0;
		*++ptr=0;
		*al=(ptr-al);
		break;
	case 0x0100:   /* power management  MMC:5.3.30 */
		*ptr++=0x03;	/* persistent and current */
		al=ptr;	/* save additional len for later */
		/* nothing */
		*al=(ptr-al);
		break;
	case 0x0105:   /* timeout  MMC:5.3.35 */
		*ptr++=0x03;	/* persistent and current */
		al=ptr;	/* save additional len for later */
		/* group3 */
		*++ptr=0x00;
		/* reserved */
		*++ptr=0;
		/* unit length */
		*++ptr=0;
		*++ptr=0;
		*al=(ptr-al);
		break;
	case 0x0107:   /* real-time streaming  MMC:5.3.37 */
		*ptr++=0x0d;	/* persistent and current */
		al=ptr;	/* save additional len for later */
		*++ptr=0x1f;
		*++ptr=0;
		*++ptr=0;
		*++ptr=0;
		*al=(ptr-al);
		break;
	case 0x0108:   /* logical unit serial number*/
		*ptr++=0x03;	/* persistent and current */
		al=ptr;	/* save additional len for later */
		for(i=0;i<strlen(ld->serial_number);i++){
			*++ptr=ld->serial_number[i];
		}
		while(i&0x03){
			i++;
			*++ptr=' ';
		}
		*al=(ptr-al);
		break;
	default:
		ptr=data+7;
	}

	/* data length */
	i=ptr-data;
	i-=3;
	data[0]=(i>>24)&0xff;
	data[1]=(i>>16)&0xff;
	data[2]=(i>> 8)&0xff;
	data[3]=(i    )&0xff;

	/* reserved */
	data[4]=0;
	data[5]=0;

	/* current profile (default is DVD-R) */
	data[6]=(ld->current_profile>>8)&0xff;
	data[7]=ld->current_profile&0xff;;	


	/*XXX may need to truncate it here */
	*data_size=alloc_len;
	*status=0;
}

static void
sbc_read10(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	long alloc_len, transfer_len;
	int count;
	off_t lba;
	lba=ntohl( *((long *)(cdb+2)) );
	transfer_len=ntohs( *((short *)(cdb+7)) );

	/* blocks are 2048 bytes in size for a cdrom */
	lba*=ld->block_size;
	transfer_len*=ld->block_size;

	if ( lba+transfer_len > ld->lunsizeinmb*1024*1024 ) {
		*status=2;
		return;
	}

	alloc_len=transfer_len;

	count=0;
	while(count<transfer_len){
		fseeko(ld->lun_fh,lba+count,SEEK_SET);
		count+=fread(data+count, 1, transfer_len-count, ld->lun_fh);
	}

	*data_size=alloc_len;
	*status=0;
}

static void
sbc_write12(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	long alloc_len, transfer_len;
	int res, count;
	off_t lba;
	lba=ntohl( *((long *)(cdb+2)) );
	transfer_len=ntohl( *((long *)(cdb+6)) );

	lba*=ld->block_size;
	transfer_len*=ld->block_size;

	alloc_len=transfer_len;

/*
if(lba>(5000000000)){
*status=2;return;
}
if(transfer_len>(100000)){
*status=2;return;
}
*/

        if ( lba+transfer_len > ld->lunsizeinmb*1024*1024 ) {
                *status=2;
                return;
        }

	count=0;
	while(count<transfer_len){
		fseeko(ld->lun_fh,lba+count,SEEK_SET);
#ifdef FAKE_WRITES
		res=transfer_len;
#else
		res=fwrite(data+count, 1, transfer_len-count, ld->lun_fh);
#endif
		count+=res;
	}

	*data_size=alloc_len;
	*status=0;
}

static void
sbc_write10(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	long alloc_len, transfer_len;
	int res, count;
	off_t lba;
	lba=ntohl( *((long *)(cdb+2)) );
	transfer_len=ntohs( *((short *)(cdb+7)) );

	lba*=ld->block_size;
	transfer_len*=ld->block_size;

	alloc_len=transfer_len;

/*
if(lba>(5000000000)){
*status=2;return;
}
if(transfer_len>(100000)){
*status=2;return;
}
*/

        if ( lba+transfer_len > ld->lunsizeinmb*1024*1024 ) {
                *status=2;
                return;
        }


	count=0;
	while(count<transfer_len){
		fseeko(ld->lun_fh,lba+count,SEEK_SET);
#ifdef FAKE_WRITES
		res=transfer_len;
#else
		res=fwrite(data+count, 1, transfer_len-count, ld->lun_fh);
#endif
		count+=res;
	}

	*data_size=alloc_len;
	*status=0;
}

static void
spc_mode_select10(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	*data_size=0;
	*status=0;
}


static void
sbc_write6(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	long alloc_len, transfer_len;
	int res, count;
	off_t lba;
	lba=ntohs( *((short *)(cdb+2)) );
	transfer_len=cdb[4];

	lba*=ld->block_size;
	transfer_len*=ld->block_size;

	alloc_len=transfer_len;

/*
if(lba>(5000000000)){
*status=2;return;
}
if(transfer_len>(100000)){
*status=2;return;
}
*/

        if ( lba+transfer_len > ld->lunsizeinmb*1024*1024 ) {
                *status=2;
                return;
        }

	count=0;
	while(count<transfer_len){
		fseeko(ld->lun_fh,lba+count,SEEK_SET);
#ifdef FAKE_WRITES
		res=transfer_len;
#else
		res=fwrite(data+count, 1, transfer_len-count, ld->lun_fh);
#endif
		count+=res;
	}

	*data_size=alloc_len;
	*status=0;
}


static void
spc_persistent_reserve_out(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	unsigned long alloc_len;
	unsigned char sa,scope,type;

	sa=cdb[1]&0x1f;
	scope=(cdb[2]>>4)&0x0f;
	type=cdb[2]&0x0f;
	alloc_len=cdb[5];
	alloc_len=(alloc_len<<8)|cdb[6];
	alloc_len=(alloc_len<<8)|cdb[7];
	alloc_len=(alloc_len<<8)|cdb[8];

	switch(sa){
	case 0x00: /*SA_READ_KEYS*/
	case 0x01: /*SA_READ_RESERVATIONS*/
	case 0x02: /*SA_REPORT_CAPABILITIES*/
	case 0x03: /*SA_READ_FULL_STATUS*/
		break;
	default:
		ILLEGAL_REQUEST;
	};

	*data_size=alloc_len;
	*status=0;
}


static void
spc_persistent_reserve_in(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	unsigned short alloc_len;
	unsigned char sa;

	sa=cdb[1]&0x1f;
	alloc_len=cdb[7];
	alloc_len=(alloc_len<<8)|cdb[8];

	memset(data, 0, alloc_len);
	switch(sa){
	case 0x00: /*SA_READ_KEYS*/
	case 0x01: /*SA_READ_RESERVATIONS*/
	case 0x02: /*SA_REPORT_CAPABILITIES*/
	case 0x03: /*SA_READ_FULL_STATUS*/
		break;
	default:
		ILLEGAL_REQUEST;
	};

	*data_size=alloc_len;
	*status=0;
}


static void
spc_inquiry(target_data_t *td, lun_data_t *ld, unsigned char *cdb, unsigned char *data, int *data_size, unsigned char *status)
{
	unsigned short alloc_len;
	unsigned char evpd, page_code;

	evpd=cdb[1];
	page_code=cdb[2];
	alloc_len=cdb[3];
	alloc_len=(alloc_len<<8)|cdb[4];
	memset(data, 0, alloc_len);
	if(evpd&0x01){
	  switch(page_code){
	    case 0: /* supported vpd pages */
		data[0]=0x05; /* attached to lun : cdrom/dvd mmc-4 */
		data[1]=0;
		data[2]=0;
		data[3]=1;
		data[4]=0;
		break;
	    case 0x83: /* device identification */
		data[0]=0x05; /* attached to lun : cdrom/dvd mmc-4 */
		data[1]=0x83;
		break;
	  default:
*status=2;return;
	  }
	} else {
		/* standard inquiry data */
		data[0]=ld->device_type; /*attached device_type */
		data[1]=0x00; /* removable */
		data[2]=0x05;
		data[3]=0x02; /*naca:0, hisup:0, resp format:2 */
		data[4]=31; /* additional length n-4 */
		data[5]=0;
		data[6]=0x80; /*BQue:1 */
		data[7]=0;

		/* vendor id */
		memset(data+8, ' ', 8);
		strncpy((char *)data+8, ld->vendor, MIN(strlen(ld->vendor),8));

		/* product id */
		memset(data+16, ' ', 16);
		strncpy((char *)data+16, ld->productid, MIN(strlen(ld->productid),16));

		/* revision */
		memset(data+32, ' ', 4);
		strncpy((char *)data+32, ld->revision, MIN(strlen(ld->revision),4));
	}

	*data_size=alloc_len;
	*status=0;
}



/* MMC : commands used by a CD/DVD device */
scsi_command_table scsi_mmc_commands[256]={
/*0x00*/ {spc_test_unit_ready, 6, DIRECTION_NO_DATA},
/*0x01*/ {NULL, 0, 0},
/*0x02*/ {NULL, 0, 0},
/*0x03*/ {spc_request_sense, 6, DIRECTION_READ},
/*0x04*/ {NULL, 0, 0},
/*0x05*/ {NULL, 0, 0},
/*0x06*/ {NULL, 0, 0},
/*0x07*/ {NULL, 0, 0},
/*0x08*/ {NULL, 0, 0},
/*0x09*/ {NULL, 0, 0},
/*0x0a*/ {NULL, 0, 0},
/*0x0b*/ {NULL, 0, 0},
/*0x0c*/ {NULL, 0, 0},
/*0x0d*/ {NULL, 0, 0},
/*0x0e*/ {NULL, 0, 0},
/*0x0f*/ {NULL, 0, 0},
/*0x10*/ {NULL, 0, 0},
/*0x11*/ {NULL, 0, 0},
/*0x12*/ {spc_inquiry, 6, DIRECTION_READ},
/*0x13*/ {NULL, 0, 0},
/*0x14*/ {NULL, 0, 0},
/*0x15*/ {NULL, 0, 0},
/*0x16*/ {NULL, 0, 0},
/*0x17*/ {NULL, 0, 0},
/*0x18*/ {NULL, 0, 0},
/*0x19*/ {NULL, 0, 0},
/*0x1a*/ {spc_mode_sense6, 6, DIRECTION_READ},
/*0x1b*/ {mmc_start_stop_unit, 6, DIRECTION_NO_DATA},
/*0x1c*/ {NULL, 0, 0},
/*0x1d*/ {NULL, 0, 0},
/*0x1e*/ {mmc_prevent_allow_media_removal, 6, DIRECTION_NO_DATA},
/*0x1f*/ {NULL, 0, 0},
/*0x20*/ {NULL, 0, 0},
/*0x21*/ {NULL, 0, 0},
/*0x22*/ {NULL, 0, 0},
/*0x23*/ {NULL, 0, 0},
/*0x24*/ {NULL, 0, 0},
/*0x25*/ {sbc_read_capacity10, 10, DIRECTION_READ},
/*0x26*/ {NULL, 0, 0},
/*0x27*/ {NULL, 0, 0},
/*0x28*/ {sbc_read10, 10, DIRECTION_READ},
/*0x29*/ {NULL, 0, 0},
/*0x2a*/ {sbc_write10, 10, DIRECTION_WRITE},
/*0x2b*/ {NULL, 0, 0},
/*0x2c*/ {NULL, 0, 0},
/*0x2d*/ {NULL, 0, 0},
/*0x2e*/ {NULL, 0, 0},
/*0x2f*/ {NULL, 0, 0},
/*0x30*/ {NULL, 0, 0},
/*0x31*/ {NULL, 0, 0},
/*0x32*/ {NULL, 0, 0},
/*0x33*/ {NULL, 0, 0},
/*0x34*/ {NULL, 0, 0},
/*0x35*/ {mmc_synchronize_cache, 10, DIRECTION_NO_DATA},
/*0x36*/ {NULL, 0, 0},
/*0x37*/ {NULL, 0, 0},
/*0x38*/ {NULL, 0, 0},
/*0x39*/ {NULL, 0, 0},
/*0x3a*/ {NULL, 0, 0},
/*0x3b*/ {NULL, 0, 0},
/*0x3c*/ {NULL, 0, 0},
/*0x3d*/ {NULL, 0, 0},
/*0x3e*/ {NULL, 0, 0},
/*0x3f*/ {NULL, 0, 0},
/*0x40*/ {NULL, 0, 0},
/*0x41*/ {NULL, 0, 0},
/*0x42*/ {NULL, 0, 0},
/*0x43*/ {mmc_read_toc, 10, DIRECTION_READ},
/*0x44*/ {NULL, 0, 0},
/*0x45*/ {NULL, 0, 0},
/*0x46*/ {mmc_get_configuration, 10, DIRECTION_READ},
/*0x47*/ {NULL, 0, 0},
/*0x48*/ {NULL, 0, 0},
/*0x49*/ {NULL, 0, 0},
/*0x4a*/ {NULL, 0, 0},
/*0x4b*/ {NULL, 0, 0},
/*0x4c*/ {NULL, 0, 0},
/*0x4d*/ {NULL, 0, 0},
/*0x4e*/ {NULL, 0, 0},
/*0x4f*/ {NULL, 0, 0},
/*0x50*/ {NULL, 0, 0},
/*0x51*/ {mmc_read_disc_information, 10, DIRECTION_READ},
/*0x52*/ {mmc_read_track_information, 10, DIRECTION_READ},
/*0x53*/ {mmc_reserve_track, 10, DIRECTION_NO_DATA},
/*0x54*/ {NULL, 0, 0},
/*0x55*/ {spc_mode_select10, 10, DIRECTION_WRITE},
/*0x56*/ {NULL, 0, 0},
/*0x57*/ {NULL, 0, 0},
/*0x58*/ {NULL, 0, 0},
/*0x59*/ {NULL, 0, 0},
/*0x5a*/ {spc_mode_sense10, 10, DIRECTION_READ},
/*0x5b*/ {NULL, 0, 0},
/*0x5c*/ {mmc_read_buffer_capacity, 10, DIRECTION_READ},
/*0x5d*/ {NULL, 0, 0},
/*0x5e*/ {NULL, 0, 0},
/*0x5f*/ {NULL, 0, 0},
/*0x60*/ {NULL, 0, 0},
/*0x61*/ {NULL, 0, 0},
/*0x62*/ {NULL, 0, 0},
/*0x63*/ {NULL, 0, 0},
/*0x64*/ {NULL, 0, 0},
/*0x65*/ {NULL, 0, 0},
/*0x66*/ {NULL, 0, 0},
/*0x67*/ {NULL, 0, 0},
/*0x68*/ {NULL, 0, 0},
/*0x69*/ {NULL, 0, 0},
/*0x6a*/ {NULL, 0, 0},
/*0x6b*/ {NULL, 0, 0},
/*0x6c*/ {NULL, 0, 0},
/*0x6d*/ {NULL, 0, 0},
/*0x6e*/ {NULL, 0, 0},
/*0x6f*/ {NULL, 0, 0},
/*0x70*/ {NULL, 0, 0},
/*0x71*/ {NULL, 0, 0},
/*0x72*/ {NULL, 0, 0},
/*0x73*/ {NULL, 0, 0},
/*0x74*/ {NULL, 0, 0},
/*0x75*/ {NULL, 0, 0},
/*0x76*/ {NULL, 0, 0},
/*0x77*/ {NULL, 0, 0},
/*0x78*/ {NULL, 0, 0},
/*0x79*/ {NULL, 0, 0},
/*0x7a*/ {NULL, 0, 0},
/*0x7b*/ {NULL, 0, 0},
/*0x7c*/ {NULL, 0, 0},
/*0x7d*/ {NULL, 0, 0},
/*0x7e*/ {NULL, 0, 0},
/*0x7f*/ {NULL, 0, 0},
/*0x80*/ {NULL, 0, 0},
/*0x81*/ {NULL, 0, 0},
/*0x82*/ {NULL, 0, 0},
/*0x83*/ {NULL, 0, 0},
/*0x84*/ {NULL, 0, 0},
/*0x85*/ {NULL, 0, 0},
/*0x86*/ {NULL, 0, 0},
/*0x87*/ {NULL, 0, 0},
/*0x88*/ {NULL, 0, 0},
/*0x89*/ {NULL, 0, 0},
/*0x8a*/ {NULL, 0, 0},
/*0x8b*/ {NULL, 0, 0},
/*0x8c*/ {NULL, 0, 0},
/*0x8d*/ {NULL, 0, 0},
/*0x8e*/ {NULL, 0, 0},
/*0x8f*/ {NULL, 0, 0},
/*0x90*/ {NULL, 0, 0},
/*0x91*/ {NULL, 0, 0},
/*0x92*/ {NULL, 0, 0},
/*0x93*/ {NULL, 0, 0},
/*0x94*/ {NULL, 0, 0},
/*0x95*/ {NULL, 0, 0},
/*0x96*/ {NULL, 0, 0},
/*0x97*/ {NULL, 0, 0},
/*0x98*/ {NULL, 0, 0},
/*0x99*/ {NULL, 0, 0},
/*0x9a*/ {NULL, 0, 0},
/*0x9b*/ {NULL, 0, 0},
/*0x9c*/ {NULL, 0, 0},
/*0x9d*/ {NULL, 0, 0},
/*0x9e*/ {NULL, 0, 0},
/*0x9f*/ {NULL, 0, 0},
/*0xa0*/ {spc_report_luns, 12, DIRECTION_READ},
/*0xa1*/ {NULL, 0, 0},
/*0xa2*/ {NULL, 0, 0},
/*0xa3*/ {NULL, 0, 0},
/*0xa4*/ {mmc_report_key, 12, DIRECTION_READ},
/*0xa5*/ {NULL, 0, 0},
/*0xa6*/ {NULL, 0, 0},
/*0xa7*/ {NULL, 0, 0},
/*0xa8*/ {NULL, 0, 0},
/*0xa9*/ {NULL, 0, 0},
/*0xaa*/ {NULL, 0, 0},
/*0xab*/ {NULL, 0, 0},
/*0xac*/ {mmc_get_performance, 12, DIRECTION_READ},
/*0xad*/ {mmc_read_dvd_structure, 12, DIRECTION_READ},
/*0xae*/ {NULL, 0, 0},
/*0xaf*/ {NULL, 0, 0},
/*0xb0*/ {NULL, 0, 0},
/*0xb1*/ {NULL, 0, 0},
/*0xb2*/ {NULL, 0, 0},
/*0xb3*/ {NULL, 0, 0},
/*0xb4*/ {NULL, 0, 0},
/*0xb5*/ {NULL, 0, 0},
/*0xb6*/ {mmc_set_streaming, 12, DIRECTION_WRITE},
/*0xb7*/ {NULL, 0, 0},
/*0xb8*/ {NULL, 0, 0},
/*0xb9*/ {NULL, 0, 0},
/*0xba*/ {NULL, 0, 0},
/*0xbb*/ {mmc_set_cd_speed, 12, DIRECTION_NO_DATA},
/*0xbc*/ {NULL, 0, 0},
/*0xbd*/ {NULL, 0, 0},
/*0xbe*/ {NULL, 0, 0},
/*0xbf*/ {NULL, 0, 0},
/*0xc0*/ {NULL, 0, 0},
/*0xc1*/ {NULL, 0, 0},
/*0xc2*/ {NULL, 0, 0},
/*0xc3*/ {NULL, 0, 0},
/*0xc4*/ {NULL, 0, 0},
/*0xc5*/ {NULL, 0, 0},
/*0xc6*/ {NULL, 0, 0},
/*0xc7*/ {NULL, 0, 0},
/*0xc8*/ {NULL, 0, 0},
/*0xc9*/ {NULL, 0, 0},
/*0xca*/ {NULL, 0, 0},
/*0xcb*/ {NULL, 0, 0},
/*0xcc*/ {NULL, 0, 0},
/*0xcd*/ {NULL, 0, 0},
/*0xce*/ {NULL, 0, 0},
/*0xcf*/ {NULL, 0, 0},
/*0xd0*/ {NULL, 0, 0},
/*0xd1*/ {NULL, 0, 0},
/*0xd2*/ {NULL, 0, 0},
/*0xd3*/ {NULL, 0, 0},
/*0xd4*/ {NULL, 0, 0},
/*0xd5*/ {NULL, 0, 0},
/*0xd6*/ {NULL, 0, 0},
/*0xd7*/ {NULL, 0, 0},
/*0xd8*/ {NULL, 0, 0},
/*0xd9*/ {NULL, 0, 0},
/*0xda*/ {NULL, 0, 0},
/*0xdb*/ {NULL, 0, 0},
/*0xdc*/ {NULL, 0, 0},
/*0xdd*/ {NULL, 0, 0},
/*0xde*/ {NULL, 0, 0},
/*0xdf*/ {NULL, 0, 0},
/*0xe0*/ {NULL, 0, 0},
/*0xe1*/ {NULL, 0, 0},
/*0xe2*/ {NULL, 0, 0},
/*0xe3*/ {NULL, 0, 0},
/*0xe4*/ {NULL, 0, 0},
/*0xe5*/ {NULL, 0, 0},
/*0xe6*/ {NULL, 0, 0},
/*0xe7*/ {NULL, 0, 0},
/*0xe8*/ {NULL, 0, 0},
/*0xe9*/ {NULL, 0, 0},
/*0xea*/ {NULL, 0, 0},
/*0xeb*/ {NULL, 0, 0},
/*0xec*/ {NULL, 0, 0},
/*0xed*/ {NULL, 0, 0},
/*0xee*/ {NULL, 0, 0},
/*0xef*/ {NULL, 0, 0},
/*0xf0*/ {NULL, 0, 0},
/*0xf1*/ {NULL, 0, 0},
/*0xf2*/ {NULL, 0, 0},
/*0xf3*/ {NULL, 0, 0},
/*0xf4*/ {NULL, 0, 0},
/*0xf5*/ {NULL, 0, 0},
/*0xf6*/ {NULL, 0, 0},
/*0xf7*/ {NULL, 0, 0},
/*0xf8*/ {NULL, 0, 0},
/*0xf9*/ {NULL, 0, 0},
/*0xfa*/ {NULL, 0, 0},
/*0xfb*/ {NULL, 0, 0},
/*0xfc*/ {NULL, 0, 0},
/*0xfd*/ {NULL, 0, 0},
/*0xfe*/ {NULL, 0, 0},
/*0xff*/ {NULL, 0, 0}
};


/* SBC : commands used by a HARD DISK device */
scsi_command_table scsi_sbc_commands[256]={
/*0x00*/ {spc_test_unit_ready, 6, DIRECTION_NO_DATA},
/*0x01*/ {NULL, 0, 0},
/*0x02*/ {NULL, 0, 0},
/*0x03*/ {spc_request_sense, 6, DIRECTION_READ},
/*0x04*/ {NULL, 0, 0},
/*0x05*/ {NULL, 0, 0},
/*0x06*/ {NULL, 0, 0},
/*0x07*/ {NULL, 0, 0},
/*0x08*/ {NULL, 0, 0},
/*0x09*/ {NULL, 0, 0},
/*0x0a*/ {sbc_write6,  6, DIRECTION_WRITE},
/*0x0b*/ {NULL, 0, 0},
/*0x0c*/ {NULL, 0, 0},
/*0x0d*/ {NULL, 0, 0},
/*0x0e*/ {NULL, 0, 0},
/*0x0f*/ {NULL, 0, 0},
/*0x10*/ {NULL, 0, 0},
/*0x11*/ {NULL, 0, 0},
/*0x12*/ {spc_inquiry, 6, DIRECTION_READ},
/*0x13*/ {NULL, 0, 0},
/*0x14*/ {NULL, 0, 0},
/*0x15*/ {NULL, 0, 0},
/*0x16*/ {NULL, 0, 0},
/*0x17*/ {NULL, 0, 0},
/*0x18*/ {NULL, 0, 0},
/*0x19*/ {NULL, 0, 0},
/*0x1a*/ {spc_mode_sense6, 6, DIRECTION_READ},
/*0x1b*/ {NULL, 0, 0},
/*0x1c*/ {NULL, 0, 0},
/*0x1d*/ {NULL, 0, 0},
/*0x1e*/ {spc_prevent_allow_media_removal, 6, DIRECTION_NO_DATA},
/*0x1f*/ {NULL, 0, 0},
/*0x20*/ {NULL, 0, 0},
/*0x21*/ {NULL, 0, 0},
/*0x22*/ {NULL, 0, 0},
/*0x23*/ {NULL, 0, 0},
/*0x24*/ {NULL, 0, 0},
/*0x25*/ {sbc_read_capacity10, 10, DIRECTION_READ},
/*0x26*/ {NULL, 0, 0},
/*0x27*/ {NULL, 0, 0},
/*0x28*/ {sbc_read10, 10, DIRECTION_READ},
/*0x29*/ {NULL, 0, 0},
/*0x2a*/ {sbc_write10, 10, DIRECTION_WRITE},
/*0x2b*/ {NULL, 0, 0},
/*0x2c*/ {NULL, 0, 0},
/*0x2d*/ {NULL, 0, 0},
/*0x2e*/ {NULL, 0, 0},
/*0x2f*/ {sbc_verify10, 10, DIRECTION_NO_DATA},
/*0x30*/ {NULL, 0, 0},
/*0x31*/ {NULL, 0, 0},
/*0x32*/ {NULL, 0, 0},
/*0x33*/ {NULL, 0, 0},
/*0x34*/ {NULL, 0, 0},
/*0x35*/ {NULL, 0, 0},
/*0x36*/ {NULL, 0, 0},
/*0x37*/ {NULL, 0, 0},
/*0x38*/ {NULL, 0, 0},
/*0x39*/ {NULL, 0, 0},
/*0x3a*/ {NULL, 0, 0},
/*0x3b*/ {NULL, 0, 0},
/*0x3c*/ {NULL, 0, 0},
/*0x3d*/ {NULL, 0, 0},
/*0x3e*/ {NULL, 0, 0},
/*0x3f*/ {NULL, 0, 0},
/*0x40*/ {NULL, 0, 0},
/*0x41*/ {NULL, 0, 0},
/*0x42*/ {NULL, 0, 0},
/*0x43*/ {NULL, 0, 0},
/*0x44*/ {NULL, 0, 0},
/*0x45*/ {NULL, 0, 0},
/*0x46*/ {NULL, 0, 0},
/*0x47*/ {NULL, 0, 0},
/*0x48*/ {NULL, 0, 0},
/*0x49*/ {NULL, 0, 0},
/*0x4a*/ {NULL, 0, 0},
/*0x4b*/ {NULL, 0, 0},
/*0x4c*/ {NULL, 0, 0},
/*0x4d*/ {NULL, 0, 0},
/*0x4e*/ {NULL, 0, 0},
/*0x4f*/ {NULL, 0, 0},
/*0x50*/ {NULL, 0, 0},
/*0x51*/ {NULL, 0, 0},
/*0x52*/ {NULL, 0, 0},
/*0x53*/ {NULL, 0, 0},
/*0x54*/ {NULL, 0, 0},
/*0x55*/ {spc_mode_select10, 10, DIRECTION_WRITE},
/*0x56*/ {NULL, 0, 0},
/*0x57*/ {NULL, 0, 0},
/*0x58*/ {NULL, 0, 0},
/*0x59*/ {NULL, 0, 0},
/*0x5a*/ {NULL, 0, 0},
/*0x5b*/ {NULL, 0, 0},
/*0x5c*/ {NULL, 0, 0},
/*0x5d*/ {NULL, 0, 0},
/*0x5e*/ {spc_persistent_reserve_in, 10, DIRECTION_READ},
/*0x5f*/ {spc_persistent_reserve_out, 10, DIRECTION_WRITE},
/*0x60*/ {NULL, 0, 0},
/*0x61*/ {NULL, 0, 0},
/*0x62*/ {NULL, 0, 0},
/*0x63*/ {NULL, 0, 0},
/*0x64*/ {NULL, 0, 0},
/*0x65*/ {NULL, 0, 0},
/*0x66*/ {NULL, 0, 0},
/*0x67*/ {NULL, 0, 0},
/*0x68*/ {NULL, 0, 0},
/*0x69*/ {NULL, 0, 0},
/*0x6a*/ {NULL, 0, 0},
/*0x6b*/ {NULL, 0, 0},
/*0x6c*/ {NULL, 0, 0},
/*0x6d*/ {NULL, 0, 0},
/*0x6e*/ {NULL, 0, 0},
/*0x6f*/ {NULL, 0, 0},
/*0x70*/ {NULL, 0, 0},
/*0x71*/ {NULL, 0, 0},
/*0x72*/ {NULL, 0, 0},
/*0x73*/ {NULL, 0, 0},
/*0x74*/ {NULL, 0, 0},
/*0x75*/ {NULL, 0, 0},
/*0x76*/ {NULL, 0, 0},
/*0x77*/ {NULL, 0, 0},
/*0x78*/ {NULL, 0, 0},
/*0x79*/ {NULL, 0, 0},
/*0x7a*/ {NULL, 0, 0},
/*0x7b*/ {NULL, 0, 0},
/*0x7c*/ {NULL, 0, 0},
/*0x7d*/ {NULL, 0, 0},
/*0x7e*/ {NULL, 0, 0},
/*0x7f*/ {NULL, 0, 0},
/*0x80*/ {NULL, 0, 0},
/*0x81*/ {NULL, 0, 0},
/*0x82*/ {NULL, 0, 0},
/*0x83*/ {NULL, 0, 0},
/*0x84*/ {NULL, 0, 0},
/*0x85*/ {NULL, 0, 0},
/*0x86*/ {NULL, 0, 0},
/*0x87*/ {NULL, 0, 0},
/*0x88*/ {NULL, 0, 0},
/*0x89*/ {NULL, 0, 0},
/*0x8a*/ {NULL, 0, 0},
/*0x8b*/ {NULL, 0, 0},
/*0x8c*/ {NULL, 0, 0},
/*0x8d*/ {NULL, 0, 0},
/*0x8e*/ {NULL, 0, 0},
/*0x8f*/ {sbc_verify16, 16, DIRECTION_NO_DATA},
/*0x90*/ {NULL, 0, 0},
/*0x91*/ {NULL, 0, 0},
/*0x92*/ {NULL, 0, 0},
/*0x93*/ {NULL, 0, 0},
/*0x94*/ {NULL, 0, 0},
/*0x95*/ {NULL, 0, 0},
/*0x96*/ {NULL, 0, 0},
/*0x97*/ {NULL, 0, 0},
/*0x98*/ {NULL, 0, 0},
/*0x99*/ {NULL, 0, 0},
/*0x9a*/ {NULL, 0, 0},
/*0x9b*/ {NULL, 0, 0},
/*0x9c*/ {NULL, 0, 0},
/*0x9d*/ {NULL, 0, 0},
/*0x9e*/ {NULL, 0, 0},
/*0x9f*/ {NULL, 0, 0},
/*0xa0*/ {spc_report_luns, 12, DIRECTION_READ},
/*0xa1*/ {NULL, 0, 0},
/*0xa2*/ {NULL, 0, 0},
/*0xa3*/ {NULL, 0, 0},
/*0xa4*/ {NULL, 0, 0},
/*0xa5*/ {NULL, 0, 0},
/*0xa6*/ {NULL, 0, 0},
/*0xa7*/ {NULL, 0, 0},
/*0xa8*/ {NULL, 0, 0},
/*0xa9*/ {NULL, 0, 0},
/*0xaa*/ {sbc_write12,  12, DIRECTION_WRITE},
/*0xab*/ {NULL, 0, 0},
/*0xac*/ {NULL, 0, 0},
/*0xad*/ {NULL, 0, 0},
/*0xae*/ {NULL, 0, 0},
/*0xaf*/ {sbc_verify12, 12, DIRECTION_NO_DATA},
/*0xb0*/ {NULL, 0, 0},
/*0xb1*/ {NULL, 0, 0},
/*0xb2*/ {NULL, 0, 0},
/*0xb3*/ {NULL, 0, 0},
/*0xb4*/ {NULL, 0, 0},
/*0xb5*/ {NULL, 0, 0},
/*0xb6*/ {NULL, 0, 0},
/*0xb7*/ {NULL, 0, 0},
/*0xb8*/ {NULL, 0, 0},
/*0xb9*/ {NULL, 0, 0},
/*0xba*/ {NULL, 0, 0},
/*0xbb*/ {NULL, 0, 0},
/*0xbc*/ {NULL, 0, 0},
/*0xbd*/ {NULL, 0, 0},
/*0xbe*/ {NULL, 0, 0},
/*0xbf*/ {NULL, 0, 0},
/*0xc0*/ {NULL, 0, 0},
/*0xc1*/ {NULL, 0, 0},
/*0xc2*/ {NULL, 0, 0},
/*0xc3*/ {NULL, 0, 0},
/*0xc4*/ {NULL, 0, 0},
/*0xc5*/ {NULL, 0, 0},
/*0xc6*/ {NULL, 0, 0},
/*0xc7*/ {NULL, 0, 0},
/*0xc8*/ {NULL, 0, 0},
/*0xc9*/ {NULL, 0, 0},
/*0xca*/ {NULL, 0, 0},
/*0xcb*/ {NULL, 0, 0},
/*0xcc*/ {NULL, 0, 0},
/*0xcd*/ {NULL, 0, 0},
/*0xce*/ {NULL, 0, 0},
/*0xcf*/ {NULL, 0, 0},
/*0xd0*/ {NULL, 0, 0},
/*0xd1*/ {NULL, 0, 0},
/*0xd2*/ {NULL, 0, 0},
/*0xd3*/ {NULL, 0, 0},
/*0xd4*/ {NULL, 0, 0},
/*0xd5*/ {NULL, 0, 0},
/*0xd6*/ {NULL, 0, 0},
/*0xd7*/ {NULL, 0, 0},
/*0xd8*/ {NULL, 0, 0},
/*0xd9*/ {NULL, 0, 0},
/*0xda*/ {NULL, 0, 0},
/*0xdb*/ {NULL, 0, 0},
/*0xdc*/ {NULL, 0, 0},
/*0xdd*/ {NULL, 0, 0},
/*0xde*/ {NULL, 0, 0},
/*0xdf*/ {NULL, 0, 0},
/*0xe0*/ {NULL, 0, 0},
/*0xe1*/ {NULL, 0, 0},
/*0xe2*/ {NULL, 0, 0},
/*0xe3*/ {NULL, 0, 0},
/*0xe4*/ {NULL, 0, 0},
/*0xe5*/ {NULL, 0, 0},
/*0xe6*/ {NULL, 0, 0},
/*0xe7*/ {NULL, 0, 0},
/*0xe8*/ {NULL, 0, 0},
/*0xe9*/ {NULL, 0, 0},
/*0xea*/ {NULL, 0, 0},
/*0xeb*/ {NULL, 0, 0},
/*0xec*/ {NULL, 0, 0},
/*0xed*/ {NULL, 0, 0},
/*0xee*/ {NULL, 0, 0},
/*0xef*/ {NULL, 0, 0},
/*0xf0*/ {NULL, 0, 0},
/*0xf1*/ {NULL, 0, 0},
/*0xf2*/ {NULL, 0, 0},
/*0xf3*/ {NULL, 0, 0},
/*0xf4*/ {NULL, 0, 0},
/*0xf5*/ {NULL, 0, 0},
/*0xf6*/ {NULL, 0, 0},
/*0xf7*/ {NULL, 0, 0},
/*0xf8*/ {NULL, 0, 0},
/*0xf9*/ {NULL, 0, 0},
/*0xfa*/ {NULL, 0, 0},
/*0xfb*/ {NULL, 0, 0},
/*0xfc*/ {NULL, 0, 0},
/*0xfd*/ {NULL, 0, 0},
/*0xfe*/ {NULL, 0, 0},
/*0xff*/ {NULL, 0, 0}
};
