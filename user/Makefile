CFLAGS += -I../include
all:	vscsitarget

vscsitarget:	main.c scsi.c vscsiapi.c scsi_target.c
	$(CC) $(CFLAGS) main.c scsi.c vscsiapi.c scsi_target.c -o $@

clean:
	rm -f vscsitarget
