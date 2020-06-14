
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

void printusage()
{
	printf("\nvscsitarget v1.0\n");
	printf("Usage: ");
	printf("vscsitarget -dev <special device file name> -files <lunfile1,lunfile2 ...> [-v]\n\n");
}
	

char *devname=NULL;
int verbose=0;

configure_scsi_target(int argc,char *argv[],target_data_t **scsi_target)
{
	target_data_t *t;
	int i;
	struct user_scsitap_task *task;

	char opt=0;
	char *filenames=NULL;
	char selection=0;

	for (i=1;i<argc;i++) {
                switch(opt) {

                        case 0:
                                break;

                        case 1:
                                devname=argv[i];
                                opt=0;
                                continue;
                        case 2:
                                filenames=argv[i];
                                opt=0;
                                continue;
                }


                if ( ( strcmp(argv[i],"-dev") ==0 )) {
                        opt=1;
			if (  (selection & 1) == 1 )  exit(100);
                        selection=1;
                        continue;
                }

                if ( ( strcmp(argv[i],"-files") ==0 )) {
                        opt=2;
			if (  (selection & 2) == 2 )  exit(100);
                        selection|=2;
                        continue;
                }

                if ( ( strcmp(argv[i],"-v") ==0 )) {
                        opt=0;
                        selection|=4;
			verbose=1;
                        continue;
                }

//		printusage();
		exit(100);
        }

	if ( ( (selection & 3) != 3 ) || (opt) ) {
//		printusage();
		exit(100);
	}

	t=add_target("virtual20mhd", TARGET_TYPE_EMULATION);
	*scsi_target=t;

	char *fname=malloc(64);
	char lunfiles[256][64];
	int len=strlen(filenames);
	int j=0;
	int lun=0;
	struct stat fstat;
	int k;

	for (i=0;i<=len;i++) {
		if ( i==len ) filenames[i]=',';
		if ( filenames[i] != ',' ) {
			if ( j >= 63 )  { printf("File name too long\n"); exit(1); }
			fname[j]=filenames[i];
			j++;
		}
		else {
			fname[j]=0;
			j=0;
			if ( strlen(fname) != 0 ) {
				if ( stat(fname, &fstat) < 0 ) { 
					printf("Could not stat file: %s\n", fname);
					exit(1);
				}
				if ( (fstat.st_size % (1024*1024)) != 0  ) {
					printf("File: %s, size is not correct, it should be multiple of Mega Bytes(1048576), current size=%d\n", fname, fstat.st_size);
					exit(1);
				}
				for(k=0;k<lun;k++) {
					if ( strcmp(lunfiles[k],fname) == 0 ) {
						printf("Duplicate file names: %s\n",fname);
						exit(1);
					}
				}
				strcpy(lunfiles[lun],fname);
				lun++;
			}
		}
	}
	for (i=0;i<lun;i++) {
		add_lun(t, i, lunfiles[i], 1, (fstat.st_size)/(1024*1024));
		if ( verbose ) {
			printf("Adding Lun:%d with file name:%s of size:%d\n",i,lunfiles[i],fstat.st_size);
		}
	}
}



main(int argc, char *argv[]) 
{
	target_data_t *t;
	struct user_scsitap_task *task;
	int x;
	int i;


	configure_scsi_target(argc, argv, &t);

	printf("vscsitarget Started, listening for SCSI requests on %s\n",devname);

	if ( ! (task=register_scsi_host(devname)) ) {
		printf("Could not setup a target session with device:%s\n",devname);
		exit(1);
	}

	for(;;) {

		if ( (x=get_scsi_task(task)) < 0 ) { 
			printf("Failed to get SCSI task\n"); exit(1);
		}
		if ( x ) continue;
		if ( verbose ) {
			printf("SCSI cmd Lun=%02X id=%X CDB=",(unsigned char)task->lun,task->cmd_number);
			for(i=0;i<16;i++) {
				printf("%02X ",(unsigned char)(task->cdb)[i]);
			}
			printf("\n");
		}

		if ( task->direction == 'I' )  {
			task->status=run_scsi_command(t,(char)task->lun,(unsigned char *)task->cdb,(task->request_buffer)+6,task->request_bufflen);
			if ( verbose ) {
				printf("SCSI cmd Lun=%02X id=%X completed, status=%x\n", task->lun,task->cmd_number,task->status);
			}
			if ( complete_scsi_task(task) < 0) { 
				printf("Failed to write SCSI response\n"); exit(1);; 
			}
		}
		if ( task->direction == 'O' )  {
			if ( receive_scsi_data(task) < 0) { 
				printf("Failed to read the data for request\n"); 
				exit(1); 
			}
			task->status=run_scsi_command(t,(char)task->lun,(unsigned char *)task->cdb,(task->request_buffer)+31,task->request_bufflen);
			task->request_bufflen=0;
			if ( verbose ) {
				printf("SCSI cmd Lun=%02X id=%X completed, status=%x\n", task->lun, task->cmd_number,task->status);
			}
			if ( complete_scsi_task(task) < 0 ) { 
				printf("Failed to write SCSI response\n"); 
				exit(1);
			}
		}
	}
}
