/*
 *   hba.c
 *
 *   This is the SCSI hba specific routines.
 *
 *   Author : Aboo Valappil. Email: aboo@aboo.org
 *
 *
 */

#include "vscsihba.h"

MODULE_AUTHOR("Aboo Valappil");
MODULE_DESCRIPTION("vscsihba");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.0.5");

/*
 * scsi_host templates
 */
struct scsi_host_template scsitap_host_template = {
        .name = "vscsihba",
        .proc_name = SCSITAP_PROC_NAME,
        .module = THIS_MODULE,
        .queuecommand = scsitap_queuecommand,
	.eh_device_reset_handler = scsitap_eh_device_reset,
        .eh_abort_handler = scsitap_eh_abort,
        .eh_host_reset_handler = scsitap_eh_host_reset,
        .this_id = -1,
        .can_queue = SCSITAP_CANQUEUE,
	.sg_tablesize=SCSITAP_MAX_SG,
        .cmd_per_lun = SCSITAP_CMDS_PER_LUN,
        .use_clustering = ENABLE_CLUSTERING,
        .max_sectors = SCSITAP_MAX_SECTORS,
        .emulated = 1,
};

// sessions has the list of all the sessions existing.
struct scsitap_sessions sessions;  

// This is used to impliment the task queue for the HBA.
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
kmem_cache_t *scsitap_task_cache;
#else
struct kmem_cache *scsitap_task_cache;
#endif


/*
 *   set_sensedata - Sets the sense buffer to all zeros.
 *  		     Called if the scsi return code is 0 as the user interface does not carry seperate sense data.
 *
 */
void set_sensedata(struct scsi_cmnd *sc, unsigned char *sense_data, unsigned int sense_len) {
	memset(sc->sense_buffer,0,SCSI_SENSE_BUFFERSIZE);
}

/*
 *   set_sensedata - Sets the sense buffer to illegal request.
 *  		     Called if the scsi return code is not 0 and no sense data accompanied the response.
 *
 */
void set_sensedata_illegal(struct scsi_cmnd *sc) {
	int i;

	struct Scsi_Host *shost = sc->device->host;
        struct scsitap_session *session = (struct scsitap_session *)shost->hostdata;
	memset(sc->sense_buffer,0,SCSI_SENSE_BUFFERSIZE);
	printk("vscsihba:%d: Received scsi status without sense code (Making it as Illegal request) CDB: ",session->dev_id);
	for(i=0;i<16;i++) printk("%02X ",sc->cmnd[i]);
	printk("\n");
        sc->sense_buffer[0] = 0x70;
        sc->sense_buffer[2] = 0; // No sense
        sc->sense_buffer[7] = 0x6;
        sc->sense_buffer[12] = 0;  // No sense
        sc->sense_buffer[13] = 0x00;
}

/*
 *  task_of_cmd - Returns the task associated with a command number for a given session.
 *
 */
struct scsitap_task *task_of_cmd(struct scsitap_session *session,unsigned int cmd_number) {

	struct scsitap_task *task,*tmp;

	list_for_each_entry_safe(task, tmp, &session->scsitap_queue, queue) {
                if ( task->cmd_number == cmd_number ) return task;
	}
	
	return NULL;

}

/*
 *  scsitap_eh_timed_out - Time out routine for a command. 
 *			   This has not been implimented properly. Just implimented for debugging purpose.
 *
 */
enum scsi_eh_timer_return scsitap_eh_timed_out(struct scsi_cmnd *sc) {

	struct Scsi_Host *shost = sc->device->host;
        struct scsitap_session *session = (struct scsitap_session *)shost->hostdata;
	printk(" vscsihba:%d: Command timed out\n",session->dev_id);
	return EH_HANDLED;

}


/*
 *  scsitap_eh_host_reset - The reset routine for scsi_host.
 *			   This has not been implimented properly. Just implimented for debugging purpose.
 *			   The also clears all the queued task in the scsi_host
 *			   This should send a SCSI management task to reset the target, i think
 *			   At the moment SCSI task management is not implimented.
 *
 */
int scsitap_eh_host_reset(struct scsi_cmnd *sc) {
	struct Scsi_Host *shost = sc->device->host;
        struct scsitap_session *session = (struct scsitap_session *)shost->hostdata;
	struct scsitap_task *task,*tmp;

	printk( "vscsihba:%d: In Reset Host, Aborting all the commands Queued\n",session->dev_id);
	printk( "vscsihba:%d: Total Command pending : %d\n", session->dev_id,session->cmd_numbers);

	spin_lock(&session->scsitap_queue_lock);
	list_for_each_entry_safe(task, tmp, &session->scsitap_queue, queue) {
		task->flags=SCSITAP_TASK_ABORTED;
		scsitap_complete_task(task);
		
	}
	spin_unlock(&session->scsitap_queue_lock);
	
	return SUCCESS;
}


/*
 *  scsitap_eh_device_reset - The reset routine for device rest.
 *			   This has not been implimented properly. Just implimented for debugging purpose.
 *			   This should send a SCSI management task to reset the LUN.
 *			   At the moment SCSI task management is not implimented.
 *
 */
int scsitap_eh_device_reset(struct scsi_cmnd *sc) {
	struct Scsi_Host *shost = sc->device->host;
        struct scsitap_session *session = (struct scsitap_session *)shost->hostdata;
	printk( "vscsihba:%d: In Reset Device\n",session->dev_id);
	return SUCCESS;
}

/*
 *    set_sensedata_commfailure - This is just to set the sense buffer to a communication failure when the HBA/session not ready.
 *				  This happens when all the user space target existed for the HBA/session.
 *				  Called from queuecommand and scsitap_destroy_session
 *
 */
void set_sensedata_commfailure(struct scsi_cmnd *sc)
{
        sc->sense_buffer[0] = 0x70;
        sc->sense_buffer[2] = NOT_READY;
        sc->sense_buffer[7] = 0x6;
        sc->sense_buffer[12] = 0x08; // Lun Communication failure
        sc->sense_buffer[13] = 0x00;
}

/*
 *   scsitap_complete_task - The scsi command/task is completed.
 *			     Remove the task from the queue and call scsi done if it is not an abort request
 */
void scsitap_complete_task(struct scsitap_task *task) 
{
	struct scsi_cmnd *sc =  task->sc;
	struct Scsi_Host *shost = sc->device->host;
	struct scsitap_session *session = (struct scsitap_session *)shost->hostdata;

	session->cmd_numbers--;
	list_del(&task->queue);
	kmem_cache_free(scsitap_task_cache, task);
	sc->SCp.ptr = NULL;
	if ( task->flags != SCSITAP_TASK_ABORTED ) sc->scsi_done(sc);
}

/*
 *  scsitap_eh_abort - The abort routine for a command. 
 *		       Need more work on this. For now just remove the task from the queue.
 */
int scsitap_eh_abort(struct scsi_cmnd *sc)
{

	struct Scsi_Host *shost = sc->device->host;
	struct scsitap_session *session = (struct scsitap_session *)shost->hostdata;
	struct scsitap_task *task;
	spin_unlock_irq(shost->host_lock);
	spin_lock(&session->scsitap_queue_lock);

	task = (struct scsitap_task *)sc->SCp.ptr;
	if (!task) goto done;
	
	printk( "vscsihba:%d: Abortng command serial number : %x\n", session->dev_id,task->cmd_number);
	printk( "vscsihba:%d: Total Command pending : %d\n", session->dev_id,session->cmd_numbers);
	task->flags=SCSITAP_TASK_ABORTED;
	scsitap_complete_task(task);
done:
	spin_unlock(&session->scsitap_queue_lock);
	spin_lock_irq(shost->host_lock);
	return SUCCESS;
	
}

/*
 *  scsitap_queuecommand - The queue command routine for scsi_host.
 */
int scsitap_queuecommand(struct scsi_cmnd *sc, void (*done) (struct scsi_cmnd *))
{
        struct Scsi_Host *host = sc->device->host;
        struct scsitap_session *session = (struct scsitap_session *)host->hostdata;
        struct scsitap_task *task;

	spin_unlock_irq(host->host_lock);
	spin_lock(&session->scsitap_queue_lock);

	// If the session is not ready (No user space target exist to serve the request), just return the command immediately with a sense code.
	if ( session->flags!=SCSITAP_CONNECTION_READY ) {
		spin_unlock(&session->scsitap_queue_lock);
		spin_lock_irq(host->host_lock);
		sc->result = DID_NO_CONNECT << 16;
        	sc->resid = sc->request_bufflen;
        	set_sensedata_commfailure(sc);
        	done(sc);
	        return 0;
	}

        sc->scsi_done = done;
        sc->result = 0;
        memset(&sc->SCp, 0, sizeof(sc->SCp));


        task = scsitap_alloc_task(session);

	// Memory pressure that we do not get a cache slot?
        if (!task) {
		spin_unlock(&session->scsitap_queue_lock);
		spin_lock_irq(host->host_lock);
                return SCSI_MLQUEUE_HOST_BUSY;
        }

	task->lun = sc->device->lun;
	task->flags=SCSITAP_TASK_SUBMITTED;
	task->sc = sc;

        // Assign a serial number to this task, this is the only unique way of identifying a task in user space.
	task->cmd_number = session->next_cmdnumber = session->next_cmdnumber ? session->next_cmdnumber : 1;

	// Increment the queued tasks.
	session->cmd_numbers++;

	// Increment the next command number
	session->next_cmdnumber++;

        // It is a type command.
	task->type='C';
	sc->SCp.ptr = (char *)task;
	
	// Add it to the task queue.
	list_add_tail(&task->queue, &session->scsitap_queue);
	
	spin_unlock(&session->scsitap_queue_lock);
	spin_lock_irq(host->host_lock);

	// Wake up any sleeping processes which were put to sleep when there was no command in the queue.
	wake_up_interruptible(&session->scsitap_wait_queue);

	return 0;

}

/*
 *    scsitap_alloc_task - Allocate a cache slot for the new task. 
 *			   Only called from queue command at the moment.
 *
 */
struct scsitap_task *scsitap_alloc_task(struct scsitap_session *session)
{
        struct scsitap_task *task;

        task = kmem_cache_alloc(scsitap_task_cache, GFP_ATOMIC);
        if (!task)  return NULL;
        memset(task, 0, sizeof(*task));
        task->flags = SCSITAP_TASK_EMPTY;
        INIT_LIST_HEAD(&task->queue);
        task->sc = NULL;	
        task->session = session;

        return task;
}

/* scsitap_get_session - Get a session from the list of sessions with a dev_id
 *			 dev_id is the minor number of the control device in operation.
 *			 Called from control device routine to understand which device belong to which session.
 *
 */
struct scsitap_session *scsitap_get_session (unsigned int id) 
{
	struct scsitap_session *session,*tmp;
	list_for_each_entry_safe(session, tmp, &(sessions.session_queue), list) {
                if ( session->dev_id == id ) return session;
	}
	return NULL;
}

/*
 *    scsitap_create_session - Create a new session.
 *			       This is called when the SCSITAP_IOCTL_REGISTER_CLIENT is called.
 *			       This allocates and creates a new scsi_host. One scsi_host per session.
 *			       Initializes the session paramters.
 *
 */
struct scsitap_session *scsitap_create_session(unsigned int id) 
{
	struct Scsi_Host *shost;
	struct scsitap_session *session;
	int rc;

	shost = scsi_host_alloc(&scsitap_host_template, sizeof(*session));

	if (!shost) {
		session=NULL;
		goto last;
	}

	session = (struct scsitap_session *)shost->hostdata;
	memset(session, 0, sizeof(*session));

        // Initializes the task queue for the session.
	INIT_LIST_HEAD(&session->scsitap_queue);
	session->cmd_numbers=0;  // Total number commands pending on this session
	session->next_cmdnumber=1;   // Next task serial number to be assigned.

        // Connection is not ready when it is created, queuecommand can not queue any commands.
	session->flags=SCSITAP_CONNECTION_NOT_READY;
	session->Device_Open=0;

	shost->max_id = SCSITAP_MAX_TARGETS;
	shost->max_lun = SCSITAP_MAX_LUNS;
	shost->max_channel = SCSITAP_MAX_CHANNELS;
	shost->max_cmd_len = SCSITAP_MAX_CMD_LEN;

	session->shost=shost;
	INIT_LIST_HEAD(&session->list);

	// Wait queue associated with task queue. Process needs to put to sleep if any tasks are pending. Otherwise, user process will use lots and lots of CPU.
	init_waitqueue_head (&session->scsitap_wait_queue);
	spin_lock_init(&session->scsitap_queue_lock);

	rc = scsi_add_host(shost, NULL);

	if (rc) {
		printk( "vscsihba: Adding SCSI HBA:%d failed\n",id);
        	scsi_host_put(shost);
		session=NULL;
		goto last;
	}

	session->dev_id=id;
	session->pid=0;
 
        // Adding the new session created to the global session list.
	list_add_tail(&session->list, &(sessions.session_queue));
	sessions.last_id++;
	printk( "vscsihba: Added SCSI HBA with dev_id=%d\n",session->dev_id);

last:
	return session;

}

/*
 *      scsitap_destroy_session - Destroys a session. 
 *  				  Does not destroy the session if any one has the control device open.
 *				  
 */
int scsitap_destroy_session(struct scsitap_session *session)
{
	struct Scsi_Host *shost;
	struct scsitap_task *task,*tmp;
	struct scsi_cmnd *sc;

	shost=session->shost;

	if ( session->Device_Open )  return 1; 

	spin_lock(&session->scsitap_queue_lock);

	// If any commands are pending in the task queue, just finish them with proper sense code.
	list_for_each_entry_safe(task, tmp, &session->scsitap_queue, queue) {
		sc = task->sc;
		sc->result = DID_NO_CONNECT << 16;
        	sc->resid = sc->request_bufflen;
        	set_sensedata_commfailure(sc);
        	scsitap_complete_task(task);
	}
	spin_unlock(&session->scsitap_queue_lock);

	list_del(&session->list);
	sessions.last_id--;
	printk("vscsihba: Removing SCSI HBA with dev_id=%d\n",session->dev_id);
        scsi_remove_host(shost);
        scsi_host_put(shost);
        return 0;
}

/*
 *	scsitap_init - Initialize the things. Called when the module is loaded.
 *
 */
int __init scsitap_init(void) {

	int major;

	// Creates a cache slot for task qeueu.
	scsitap_task_cache = kmem_cache_create("scsitap_task_cache",
                                     sizeof(struct scsitap_task), 0,
                                     0, NULL, NULL);

	// Memory pressure?
	if (!scsitap_task_cache) {
        	return -ENOMEM;
	}

	// Register the control character device
	if ( (major=scsitap_register_interface()) ) { 
		printk("vscsihba: Device Major=%d\n",major);
		INIT_LIST_HEAD(&(sessions.session_queue));
		spin_lock_init(&(sessions.session_lock));
		sessions.last_id=0;
		return 0;
	}

        // Undo things if anything goes wrong.
	kmem_cache_destroy(scsitap_task_cache);
	printk ("vscsihba: Failed to Init the driver\n");

	return -ENODEV;
}


/*
 *	scsitap_cleanup - cleanup routine during module unload.
 *
 */
void __exit scsitap_cleanup(void)
{
	struct scsitap_session *session,*tmp;

        kmem_cache_destroy(scsitap_task_cache);
	spin_lock(&(sessions.session_lock));
        
	// Make sure that all the scsi_hosts associated with all the sessions are destroyed.
	list_for_each_entry_safe(session, tmp, &(sessions.session_queue), list) {
                scsitap_destroy_session(session);
	}
	spin_unlock(&(sessions.session_lock));
	scsitap_unregister_interface();
	printk("vscsihba: Removed due to module unload\n");
}

module_init(scsitap_init);
module_exit(scsitap_cleanup);
