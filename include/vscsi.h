
#define SCSITAP_MAX_CMD_LEN       16
#define SCSITAP_CANQUEUE          8192
#define SCSITAP_MAX_LUNS          256
#define SCSITAP_MAX_TARGETS       1
#define SCSITAP_MAX_CHANNELS      0
#define SCSITAP_CMDS_PER_LUN      128
#define SCSITAP_MAX_SG      SG_ALL
#define SCSITAP_MAX_SECTORS      256

#define SCSITAP_PROC_NAME   "scsitap"


#define SCSITAP_CONNECTION_READY 1
#define SCSITAP_CONNECTION_NOT_READY 0

#define SCSITAP_TASK_EMPTY 0
#define SCSITAP_TASK_SUBMITTED 1
#define SCSITAP_TASK_CMD_RECEIVED 2
#define SCSITAP_TASK_DATA_RECEIVED 3
#define SCSITAP_TASK_RECEIVED 4
#define SCSITAP_TASK_FINISHED 5
#define SCSITAP_TASK_ABORTED 6

#define SCSITAP_UHEADER_SIZE 31
#define SCSITAP_SCAN_TIMER 5

#define SCSITAP_IOCTL_REGISTER_CLIENT 0xAB00
#define SCSITAP_IOCTL_SCAN_HOST 0xAB01
#define SCSITAP_IOCTL_RECEIVE_TASK 0xAB02
#define SCSITAP_IOCTL_FINISH_TASK 0xAB03 
#define SCSITAP_IOCTL_UNREGISTER_CLIENT 0xAB04
#define SCSITAP_IOCTL_SET_TASK 0xAB05 

#define BUFFER_SIZE 135168

struct user_scsitap_task {
        unsigned int cmd_number;
        unsigned char type;
        unsigned char direction;
        unsigned int lun;
        unsigned char cdblen;
        char cdb[16];
        int request_bufflen;
        unsigned char *request_buffer;
	int status;
	int session;
};

void task_buffer_to_task(char *task_buffer, struct user_scsitap_task *task);
struct user_scsitap_task *register_scsi_host (char *devname);
int get_scsi_task(struct user_scsitap_task *task);
int complete_scsi_task(struct user_scsitap_task *task);
int receive_scsi_data(struct user_scsitap_task *task);
