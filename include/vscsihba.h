#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mempool.h>

#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/version.h>

#include "vscsi.h"

/*
 *  scsitap_task - the task structure 
 *  This holds all the information required for a task.
 */
struct scsitap_task {
        struct list_head  queue;
        unsigned int cmd_number;   // The unique way of identifying a task in user targets.
        unsigned int lun;
        unsigned int flags;
        unsigned char type;  //type of task.
        struct scsi_cmnd *sc; // SCSI command attached to this task.
        struct scsitap_session  *session; // Which session this task belong to.
};

/* 
 *   scsitap_sessions - Used to hold information about all the sessions
 *
 */
struct scsitap_sessions {
        struct list_head        session_queue;
        unsigned int            last_id;
        spinlock_t              session_lock; // Protects everything in this structure.
};

/*
 *   scsitap_session - Used to hold information about the session, scsi_host, control device id, etc.
 *
 */
struct scsitap_session {

        struct list_head        list;
        struct Scsi_Host        *shost;
        unsigned int           dev_id; // The minor number of the control device which controls this session.
        unsigned int           pid;   // The user target pid registered with this session.
        unsigned int            flags; // State of the session, ready or not ready.
        unsigned char 	Device_Open;  // How many user target exist for this session.
        spinlock_t                      scsitap_queue_lock;  //Protects everything in this struct including the task queue.
        unsigned int                    cmd_numbers; // Number of tasks/cmds in the task queue
        unsigned int                    next_cmdnumber; // Next cmd number to be allocated
        wait_queue_head_t               scsitap_wait_queue;
        struct list_head                scsitap_queue;
};


/* 
 * This just to make sure that it works with 2.6.20 kernel as the paramter count has changed
 *
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
        #define SCSITAP_INIT_WORK(work, func)     INIT_WORK((work), (func), (void *)(work));
#else
        #define SCSITAP_INIT_WORK(work, func)     INIT_WORK((work), ( void (*) (struct work_struct *))(func));
#endif

struct scsitap_task *scsitap_alloc_task(struct scsitap_session *);
void set_sensedata_commfailure(struct scsi_cmnd *);
void set_sensedata_illegal(struct scsi_cmnd *);
int scsitap_queuecommand(struct scsi_cmnd *, void (*) (struct scsi_cmnd *));
int scsitap_eh_host_reset(struct scsi_cmnd *);
int scsitap_eh_device_reset(struct scsi_cmnd *);
int scsitap_eh_abort(struct scsi_cmnd *);
void scsitap_complete_task(struct scsitap_task *);
int scsitap_register_interface(void);
void scsitap_unregister_interface(void);
int scsitap_ctl_ioctl(struct inode *, struct file *, unsigned int , unsigned long );
ssize_t scsitap_ctl_read(struct file *, char *, size_t ,loff_t *);
struct scsitap_task *task_of_cmd(struct scsitap_session *,unsigned int );
void set_sensedata(struct scsi_cmnd *, unsigned char *, unsigned int );
int scsitap_task_to_user(struct scsitap_task *, char *,char);
ssize_t scsitap_ctl_write(struct file *, const char *, size_t , loff_t *);
unsigned int copy_from_user_sg(struct scatterlist *, const char *, unsigned int);
unsigned int copy_to_user_sg(char *,struct scatterlist *, unsigned int);
struct scsitap_session *scsitap_create_session(unsigned int);
int scsitap_destroy_session(struct scsitap_session *);
struct scsitap_session *scsitap_get_session ( unsigned int);
loff_t scsitap_ctl_seek( struct file *, loff_t, int);
int scsitap_ctl_open( struct inode *, struct file *);
int scsitap_ctl_release( struct inode *, struct file *);
void scsitap_scan_hba (void *);
