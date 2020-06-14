/*
 *   device.c 
 *
 *   The user space interface to vscsihba driver
 *  
 *   Author : Aboo Valappil. Email: aboo@aboo.org
 *
 *
 */

#include "vscsihba.h"

extern struct scsitap_sessions sessions;   // sessions has a list of sessions active. One session per scsi_host 

/* Structure for scheduling a work for the driver */
struct scsitap_work_t {
	struct scsitap_session *session;
	struct work_struct work;
} scsitap_work; 



/**
 *  scsitap_task_to_user  - Converts a task (One scsi_cmnd per task) to a user space buffer.
 *			    This is called in two situations.
 *			    1. User program wants the new service queued.
 *			    2. User program wants to read the data associated with a task.
 *
 * 	@task: Task queued by queuecommand
 *      @buffer: User space buffer from read system call
 *      @type: Type of the command. Mainly this is C for command, D for data. Will be more when SCSI task management is implimented.
 *
 *      Returns the number of bytes copied, else return error.
 *
 *  I have tried to use memmap to avoid the copy business. But the page cache and buffer cache making it difficult and requires some sort of ugly page table manipulation. 
 *  The memmap or some other method needs to be implimented to prevent the copy.
 *
 **/
int scsitap_task_to_user ( struct scsitap_task *task, char *buffer, char type) {

        char *sbuffer=buffer;

        put_user(type, buffer++);

        if ( task->sc->sc_data_direction == DMA_TO_DEVICE ) put_user('O', buffer++);
        else  put_user('I', buffer++);

       if ( __copy_to_user_inatomic(buffer,&task->cmd_number,4) ) {
             return -EFAULT;
       }
       buffer+=4;

        if ( __copy_to_user_inatomic(buffer,&task->lun,4) ) {
             return -EFAULT;
        }
        buffer+=4;

       put_user( task->sc->cmd_len, buffer++);
       if ( __copy_to_user_inatomic(buffer,task->sc->cmnd,MAX_COMMAND_SIZE) ) {
              return -EFAULT;
       }
       buffer+=MAX_COMMAND_SIZE;

       if ( __copy_to_user_inatomic(buffer,&task->sc->request_bufflen,4) ) {
              return -EFAULT;
       }
       buffer+=4;
       return buffer-sbuffer;
}

/**
 *  scsitap_ctl_read  - Read routine for the character device interface. Used during read system call on the character device.
 * 	@file: the file pointer.
 *      @buffer: User space buffer from read system call. This buffer needs to be mlocked in user space.
 *      @count: Number of bytes to read. If the count=SCSITAP_UHEADER_SIZE, the task desciption is sent, or the request_buffer is sent.
 *              file-f_pos gives which task the user space is operating on.
 *
 **/
ssize_t scsitap_ctl_read(struct file *file, char *buffer, size_t count,loff_t *offset)
{
    struct scsitap_session *session;
    struct scsitap_task *task,*tmp;
    int ret=0;
    loff_t fp;

    session=(struct scsitap_session *)file->private_data;
    if ( ! session ) return -EFAULT;  // Some one trying to read the device without register IOCTL.

    // I really want to hold the spinlock and then check if the task queue is empty or not, Just happy with this for time being.
    wait_event_interruptible(session->scsitap_wait_queue, ! list_empty(&session->scsitap_queue));


    spin_lock(&session->scsitap_queue_lock);

    // This following check could reduntant, it would not hurt to put it here.
    if ( session->flags!=SCSITAP_CONNECTION_READY ) {
        ret= -EINVAL;
	goto last;
    }

        fp=file->f_pos;  // fp holds the command/task number user program working on.

	// Only the command description is required to be sent.
        if (count == SCSITAP_UHEADER_SIZE ) {  
                list_for_each_entry_safe(task, tmp, &session->scsitap_queue, queue) {
                        if  (task->flags == SCSITAP_TASK_SUBMITTED ) {
                                ret=scsitap_task_to_user(task,buffer,task->type);
                                goto last;
                        }
                }
                ret=0;
        }
	// The command description and the request_buffer of the scsi command needs to be sent.
        else if ( count > SCSITAP_UHEADER_SIZE) {   

                        task=task_of_cmd(session,fp);

			// If the task is not found, some one else serviced the command, safe to return 0
                        if ( ! task ) { 	   
                                ret=0;
                                goto last;
                        }

			// Not interested in task marked as recieved as someone else serving the task.
                        if ( task->flags != SCSITAP_TASK_RECEIVED ) {  
                                ret=0;
                                goto last;
                        }

			//Just to make sure the correct size is requested.
			if ( count != (SCSITAP_UHEADER_SIZE+task->sc->request_bufflen) ) { 
                                ret=-EBADF;
                                goto last;
                        }

			// Add the task header first.
                        ret=scsitap_task_to_user(task,buffer,'D'); 
                        if ( ret < 0 ) goto last;
                        buffer+=ret;

			// If data is requested, provide the request_buffer also.
                        if ( task->sc->request_bufflen > 0 ) {  
                                if ( task->sc->use_sg == 0 ) {
                                         if (__copy_to_user_inatomic(buffer,task->sc->request_buffer,task->sc->request_bufflen) ) {
                                                 ret=-EFAULT;
                                         }
                                }
                                else {
                                         if ( copy_to_user_sg(buffer,task->sc->request_buffer,task->sc->use_sg ) ) {
                                                ret=-EFAULT;
                                         }
                                }
                        }
        }
        else ret=-EINVAL;

last:
     spin_unlock(&session->scsitap_queue_lock);
     return ret;
}

/**
 *  scsitap_ctl_write  - Write routine for the character device interface. Used during write system call on the character device.
 * 			 This called in two situations.
 *			 1. Command completed on target.
 *			 2. Command completed with data or sense buffer.
 *
 * 	@file: the file pointer.
 *      @buffer: User space buffer from write system call. This buffer needs to be mlocked in user space.
 *      @count: Number of bytes to read. 
 *              file-f_pos gives which task the user space is operating on.
 *
 **/
ssize_t scsitap_ctl_write(struct file *filp, const char *buffer, size_t count, loff_t *offset)
{
        struct scsitap_session *session;
        struct scsitap_task *task;
        char type;
        unsigned int cmd_number;
        unsigned char scsi_status;
        int ret=0;

        session=(struct scsitap_session *)filp->private_data;
	if ( ! session ) { return -EFAULT; } // Some one trying to write to device with using register IOCTL.

        spin_lock(&session->scsitap_queue_lock);

    	if ( session->flags!=SCSITAP_CONNECTION_READY ) {
        	ret= -EINVAL;
		goto last;
    	}

        get_user(type,buffer++);

        switch (type) {

        case 'D':

                if (__copy_from_user_inatomic(&cmd_number,buffer,4) ) {
                        ret=-EFAULT;
                        goto last;
                }
                buffer+=4;
                task=task_of_cmd(session,cmd_number);

		// If we can not find the task, some else already completed the task. Safe to return with success.
                if ( !task ) {
                        ret=count;
                        goto last;
                }

                get_user(scsi_status,buffer++);

		// If the scsi_status is 0, the buffer has the command data, else it will have sense information.
		if ( ! scsi_status ) {

			// If the count is greater than 6, the user program is giving the requested data along with command completion.
	                if ( count > 6 ) {
       		                 if ( task->sc->use_sg == 0 ) {

					 // Accept the buffer to command request_buffer, no sg used.
               		                 if (__copy_from_user_inatomic(task->sc->request_buffer,buffer,task->sc->request_bufflen) ) {
                       		                 ret=-EFAULT;
                               		         goto last;
                                	 }
                        	 }
	                         else {

					 // Accept the buffer to command request_buffer. But the request buffer is scatter gather buffer.
        	                        if ( copy_from_user_sg(task->sc->request_buffer,buffer,task->sc->use_sg ) ) {
               		                         ret=-EFAULT;
                       		                 goto last;
                               		 }
                        	}
                	}
                	set_sensedata(task->sc,NULL,0);
		}
		else {
			 // If the MSB is set, the sense data is in buffer, if not, set our own sense data.
			 if ( ! (scsi_status >> 7) ) {
				set_sensedata_illegal(task->sc);
			 }
			 else {
		                 if (__copy_from_user_inatomic(task->sc->sense_buffer,buffer,14) ) {
     			                 ret=-EFAULT;
       		       		         goto last;
				 }
				 scsi_status &= 0x1f;	
			 }
		}

                ret=count;
	        task->sc->result=scsi_status;
	        scsitap_complete_task(task);  //Complete the scsi command and remove the command from the queue.

                break;


        }
last:
        spin_unlock(&session->scsitap_queue_lock);
        return ret;
}


/**
 *  copy_from_user_sg  - For sending data from flat syscall buffer to sg buffer.
 *
 *
 **/
unsigned int copy_from_user_sg(struct scatterlist *sg, const char *buffer, unsigned int sgcount) {

        int r;
        int i=0;
        void *address;
        void *kaddr;

        for(i=0;i<sgcount;i++) {

                kaddr=page_address(sg[i].page);

                if ( ! kaddr ) return 1;

                address=kaddr+sg[i].offset;

                if ( (r=__copy_from_user_inatomic(address,buffer,sg[i].length)) ) {
                        printk( "vscsihba: Copy User failed : %d:%d:%d\n", i,sg[i].length,r);
                        return r;
                }
                buffer+=sg[i].length;
        }
        return 0;
}

/**
 *  copy_to_user_sg  - For getting data from sg buffer to syscall buffer.
 *
 *
 **/
unsigned int copy_to_user_sg(char *buffer,struct scatterlist *sg, unsigned int sgcount) {

        int r;
        int i=0;
        void *address;
        void *kaddr;

        for(i=0;i<sgcount;i++) {


                kaddr=page_address(sg[i].page);

                if ( ! kaddr ) return 1;

                address=kaddr+sg[i].offset;


                if ( (r=__copy_to_user_inatomic(buffer,address,sg[i].length)) ) {
                        printk( "vscsihba: Copy User failed : %d:%d:%d\n", i,sg[i].length,r);
                        return r;
                }
                buffer+=sg[i].length;
        }
        return 0;
}

/**
 *  scsitap_scan_hba  - Scan the scsi_host, invoked when a new scsi_host is created or attached.
 *    			Used with schedule_work.
 *
 *
 **/
void scsitap_scan_hba (void *work) 
{
	struct scsitap_work_t *sw=container_of(work,struct scsitap_work_t, work);
	struct scsitap_session *session=sw->session;

        printk("vscsihba:%d: Scanning SCSI Bus, Started by pid %u\n",session->dev_id,(unsigned int)session->pid);
        scsi_scan_host(session->shost);
        printk("vscsihba:%d: Scanning SCSI Bus, Finished by pid %u\n",session->dev_id,(unsigned int)session->pid);
}

/**
 *  scsitap_ctl_ioctl  - ioctl routine for the character device interface. Used during ioctl system call on the character device.
 * 			 Three IOCTLs are implimented
 *			 1. SCSITAP_IOCTL_REGISTER_CLIENT = Register a user space SCSI target.
 *			 2. SCSITAP_IOCTL_RECIEVE_TASK = Recive a task from the task queue, just to make sure that no one else pick up this task.
 *			 3. SCSITAP_IOCTL_SET_TASK = Set the file position to let the read routine know what command the user space working on.
 *			 4. SCSITAP_IOCTL_UNREGISTER_CLIENT = Unregister the user scsi target.
 *
 *
 **/
int scsitap_ctl_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{

        struct scsitap_session *session;
        struct scsitap_task *task;

        switch ( cmd ) {

        case SCSITAP_IOCTL_REGISTER_CLIENT:
		spin_lock(&(sessions.session_lock));

		// Get the session from the minor number of the device being open.
		// Minor number will tell the dev_id of the session.
		session=scsitap_get_session(MINOR(inode->i_rdev)); 

		// If no session is found, just create one.
		if ( ! session ) { 
			session=scsitap_create_session(MINOR(inode->i_rdev));
			if ( ! session ) {
				spin_unlock(&(sessions.session_lock));
				return -1;
			}
		}

		// Attach the private_data to the session, so that other file operation on this device will know which session they are dealing with.
		file->private_data=session;
		session->Device_Open++;
                session->pid=arg;
                printk(" vscsihba:%d: Virtual SCSI Target registered, pid=%u\n",session->dev_id,(unsigned int)arg);

		// If the connection is not ready, Scan the HBA and let the upper layer know about the devices on this scsi_host.
		// I have to impliment this as a scheduler work because scsi_scan_host is blocking, and the user target can not sleep at this moment.
		if ( session->flags == SCSITAP_CONNECTION_NOT_READY) {
              		session->flags=SCSITAP_CONNECTION_READY;
			scsitap_work.session=session;
			SCSITAP_INIT_WORK(&scsitap_work.work,scsitap_scan_hba);
			schedule_work(&scsitap_work.work);
		}
              	session->flags=SCSITAP_CONNECTION_READY;
		spin_unlock(&(sessions.session_lock));
                return SUCCESS;
                break;

        case SCSITAP_IOCTL_RECEIVE_TASK:
		session=file->private_data;
		if ( ! session ) return -1;
                spin_lock(&session->scsitap_queue_lock);
		if ( session->flags!=SCSITAP_CONNECTION_READY ) return -1;

                task=task_of_cmd(session,arg);

		// If we can not find the task, someone else already finished this task, safe to return success.
                if ( task ) {
                        task->flags=SCSITAP_TASK_RECEIVED;
        		spin_unlock(&session->scsitap_queue_lock);
                        return SUCCESS;
                }
        	spin_unlock(&session->scsitap_queue_lock);
		return 0; 
                break;

        case SCSITAP_IOCTL_SET_TASK:
		file->f_pos=arg;
		return SUCCESS;
		break;

        case SCSITAP_IOCTL_UNREGISTER_CLIENT:
		spin_lock(&(sessions.session_lock));
		session=scsitap_get_session(MINOR(inode->i_rdev));
		if ( ! session ) {
			spin_unlock(&(sessions.session_lock)); 
			return -1;
		}
		if ( scsitap_destroy_session(session) ) { 
			printk("vscsihba:%d: Can not remove, control device is open\n",session->dev_id);
			spin_unlock(&(sessions.session_lock));
			return -1;
		}
		spin_unlock(&(sessions.session_lock)); 
		return SUCCESS;  
		break; 

        }

	return -1;
}

/**
 *  scsitap_ctl_open  - The open routine for the control device. Just do not do anything except inciment the module count.
 *
 *
 **/
int scsitap_ctl_open( struct inode *inode, struct file *file) 
{
	try_module_get(THIS_MODULE);
	return 0;
}

/**
 *  scsitap_ctl_release  - The close routine for the control device.
 *			   Assumes that this is called when the user target dies.
 *
 *
 **/
int scsitap_ctl_release( struct inode *inode, struct file *file) 
{
    	struct scsitap_session *session;
	module_put(THIS_MODULE);
	session=(struct scsitap_session *)file->private_data;

	// If we can not find the session associated with it, it is a bogus open by some one.
	if ( session ) {
		spin_lock(&(sessions.session_lock));
		session->Device_Open--;

		// If this is the last close on the device, we need to make the session not ready so that queuecommand will start rejecting IO from upper layer.
		if ( ! session->Device_Open ) {
			printk("vscsihba:%d: Last user target exited, Setting the HBA to not ready\n",session->dev_id);
			session->flags=SCSITAP_CONNECTION_NOT_READY;
		}		
		spin_unlock(&(sessions.session_lock));
	}

	return 0;
}

/*
 *  File operations on control device.
 *
 */
static struct file_operations control_fops = {
      .owner = THIS_MODULE,
      .read = scsitap_ctl_read,
      .ioctl = scsitap_ctl_ioctl,
      .write = scsitap_ctl_write,
      .open = scsitap_ctl_open, 
      .release = scsitap_ctl_release, 
};

static const char *control_name = "vscsihba";
static int control_major=0;

/*
 *  Register the control device when the module is loaded.
 *  this is called from module_init
 *
 */
int scsitap_register_interface(void)
{
        control_major = register_chrdev(0, control_name, &control_fops);
        if (control_major < 0) {
                printk( "vscsihba: Failed to register the control device\n");
                return 0;
        }

        return control_major;
}


/*
 *  Unregister the control device when the module is unloaded.
 *  This is called from module_exit
 *
 */
void scsitap_unregister_interface(void)
{
        unregister_chrdev(control_major, control_name);
}

