obj-m += scsitap.o
scsitap-objs := hba.o device.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/kernel modules
	make -C $(PWD)/user

	@echo
	@echo
	@echo "Compilation complete                Output file"
	@echo "----------------------------------- ----------------"
	@echo "Virtual SCSI HBA kernel module :    $(PWD)/kernel/vscsihba.ko"
	@echo "Virtual SCSI Target application:    $(PWD)/user/vscsitarget"
	@echo

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/kernel clean
	make -C $(PWD)/user clean

