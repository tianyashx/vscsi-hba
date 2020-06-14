# vscsi-hba
This is a kernel driver which allows the user programs (Running in user mode) to create, delete and service SCSI low level HBA (LLD). The user program can create and register its own virtual HBA in the kernel through SCSI APIs provided. The user program will be able to service SCSI requests from kernel's SCSI mid layer. Basic code and description are from http://vscsihba.sourceforge.net/

More details in README-BASE file.
