#include "vscsi.h"
#include <sys/mman.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


void task_buffer_to_task(char *task_buffer, struct user_scsitap_task *task)
{
       task->type=*task_buffer++;
       task->direction=*task_buffer++;
       memcpy( &task->cmd_number, task_buffer, 4);
       task_buffer+=4;
       memcpy( &task->lun, task_buffer, 4);
       task_buffer+=4;
       task->cdblen=*task_buffer++;
       memcpy(task->cdb,task_buffer,task->cdblen);
       task_buffer+=16;
       memcpy(&task->request_bufflen,task_buffer,4);
}

struct user_scsitap_task *register_scsi_host (char *devname) 
{
	static struct user_scsitap_task tsk;

	if ( (tsk.session=open(devname,O_RDWR)) < 0 ) { return NULL; }
	ioctl(tsk.session,SCSITAP_IOCTL_REGISTER_CLIENT,getpid());
        tsk.request_buffer=malloc(BUFFER_SIZE);
	memset(tsk.request_buffer,0,BUFFER_SIZE);
	mlock(tsk.request_buffer,BUFFER_SIZE);
	tsk.status=0;
	return &tsk;
}

int get_scsi_task(struct user_scsitap_task *task)
{
	char task_buffer[32];
	int x;

	if ( (x=read(task->session,task_buffer,SCSITAP_UHEADER_SIZE)) != SCSITAP_UHEADER_SIZE )  { 
		if ( x==0 ) return 1;
		return -1;
	}
	task_buffer_to_task(task_buffer,task);
	if ( ioctl(task->session,SCSITAP_IOCTL_RECEIVE_TASK,task->cmd_number) < 0 ) {
		return -1;
	}
		
	return 0;
}

int complete_scsi_task(struct user_scsitap_task *task)
{
	*task->request_buffer='D';
	memcpy((task->request_buffer+1),&task->cmd_number,4);
	*(task->request_buffer+5)=task->status;
	if ( write(task->session,task->request_buffer,task->request_bufflen+6) != task->request_bufflen+6 ) { return -1; }
	return 0;
}

int receive_scsi_data(struct user_scsitap_task *task)
{
	if ( task->direction != 'O' )  { return 1;}
	ioctl(task->session,SCSITAP_IOCTL_SET_TASK,task->cmd_number);
	if (read(task->session,task->request_buffer,task->request_bufflen+SCSITAP_UHEADER_SIZE) != SCSITAP_UHEADER_SIZE ) { return -1; }
	return 0;
}
