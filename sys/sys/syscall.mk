# FreeBSD system call names.
# DO NOT EDIT-- this file is automatically generated.
# $FreeBSD$
# created from FreeBSD: src/sys/kern/syscalls.master,v 1.138 2002/12/15 19:17:56 dillon Exp 
MIASM =  \
	syscall.o \
	exit.o \
	fork.o \
	read.o \
	write.o \
	open.o \
	close.o \
	wait4.o \
	link.o \
	unlink.o \
	chdir.o \
	fchdir.o \
	mknod.o \
	chmod.o \
	chown.o \
	break.o \
	getfsstat.o \
	getpid.o \
	mount.o \
	unmount.o \
	setuid.o \
	getuid.o \
	geteuid.o \
	ptrace.o \
	recvmsg.o \
	sendmsg.o \
	recvfrom.o \
	accept.o \
	getpeername.o \
	getsockname.o \
	access.o \
	chflags.o \
	fchflags.o \
	sync.o \
	kill.o \
	getppid.o \
	dup.o \
	pipe.o \
	getegid.o \
	profil.o \
	ktrace.o \
	getgid.o \
	getlogin.o \
	setlogin.o \
	acct.o \
	sigaltstack.o \
	ioctl.o \
	reboot.o \
	revoke.o \
	symlink.o \
	readlink.o \
	execve.o \
	umask.o \
	chroot.o \
	msync.o \
	vfork.o \
	sbrk.o \
	sstk.o \
	vadvise.o \
	munmap.o \
	mprotect.o \
	madvise.o \
	mincore.o \
	getgroups.o \
	setgroups.o \
	getpgrp.o \
	setpgid.o \
	setitimer.o \
	swapon.o \
	getitimer.o \
	getdtablesize.o \
	dup2.o \
	fcntl.o \
	select.o \
	fsync.o \
	setpriority.o \
	socket.o \
	connect.o \
	getpriority.o \
	bind.o \
	setsockopt.o \
	listen.o \
	gettimeofday.o \
	getrusage.o \
	getsockopt.o \
	readv.o \
	writev.o \
	settimeofday.o \
	fchown.o \
	fchmod.o \
	setreuid.o \
	setregid.o \
	rename.o \
	flock.o \
	mkfifo.o \
	sendto.o \
	shutdown.o \
	socketpair.o \
	mkdir.o \
	rmdir.o \
	utimes.o \
	adjtime.o \
	setsid.o \
	quotactl.o \
	nfssvc.o \
	statfs.o \
	fstatfs.o \
	getfh.o \
	getdomainname.o \
	setdomainname.o \
	uname.o \
	sysarch.o \
	rtprio.o \
	semsys.o \
	msgsys.o \
	shmsys.o \
	pread.o \
	pwrite.o \
	ntp_adjtime.o \
	setgid.o \
	setegid.o \
	seteuid.o \
	stat.o \
	fstat.o \
	lstat.o \
	pathconf.o \
	fpathconf.o \
	getrlimit.o \
	setrlimit.o \
	getdirentries.o \
	mmap.o \
	__syscall.o \
	lseek.o \
	truncate.o \
	ftruncate.o \
	__sysctl.o \
	mlock.o \
	munlock.o \
	undelete.o \
	futimes.o \
	getpgid.o \
	poll.o \
	__semctl.o \
	semget.o \
	semop.o \
	msgctl.o \
	msgget.o \
	msgsnd.o \
	msgrcv.o \
	shmat.o \
	shmctl.o \
	shmdt.o \
	shmget.o \
	clock_gettime.o \
	clock_settime.o \
	clock_getres.o \
	nanosleep.o \
	minherit.o \
	rfork.o \
	openbsd_poll.o \
	issetugid.o \
	lchown.o \
	getdents.o \
	lchmod.o \
	netbsd_lchown.o \
	lutimes.o \
	netbsd_msync.o \
	nstat.o \
	nfstat.o \
	nlstat.o \
	fhstatfs.o \
	fhopen.o \
	fhstat.o \
	modnext.o \
	modstat.o \
	modfnext.o \
	modfind.o \
	kldload.o \
	kldunload.o \
	kldfind.o \
	kldnext.o \
	kldstat.o \
	kldfirstmod.o \
	getsid.o \
	setresuid.o \
	setresgid.o \
	aio_return.o \
	aio_suspend.o \
	aio_cancel.o \
	aio_error.o \
	aio_read.o \
	aio_write.o \
	lio_listio.o \
	yield.o \
	mlockall.o \
	munlockall.o \
	__getcwd.o \
	sched_setparam.o \
	sched_getparam.o \
	sched_setscheduler.o \
	sched_getscheduler.o \
	sched_yield.o \
	sched_get_priority_max.o \
	sched_get_priority_min.o \
	sched_rr_get_interval.o \
	utrace.o \
	kldsym.o \
	jail.o \
	sigprocmask.o \
	sigsuspend.o \
	sigpending.o \
	__acl_get_file.o \
	__acl_set_file.o \
	__acl_get_fd.o \
	__acl_set_fd.o \
	__acl_delete_file.o \
	__acl_delete_fd.o \
	__acl_aclcheck_file.o \
	__acl_aclcheck_fd.o \
	extattrctl.o \
	extattr_set_file.o \
	extattr_get_file.o \
	extattr_delete_file.o \
	aio_waitcomplete.o \
	getresuid.o \
	getresgid.o \
	kqueue.o \
	kevent.o \
	extattr_set_fd.o \
	extattr_get_fd.o \
	extattr_delete_fd.o \
	__setugid.o \
	nfsclnt.o \
	eaccess.o \
	nmount.o \
	kse_exit.o \
	kse_wakeup.o \
	kse_create.o \
	kse_thr_interrupt.o \
	kse_release.o \
	__mac_get_proc.o \
	__mac_set_proc.o \
	__mac_get_fd.o \
	__mac_get_file.o \
	__mac_set_fd.o \
	__mac_set_file.o \
	kenv.o \
	lchflags.o \
	uuidgen.o \
	sendfile.o \
	mac_syscall.o \
	ksem_close.o \
	ksem_post.o \
	ksem_wait.o \
	ksem_trywait.o \
	ksem_init.o \
	ksem_open.o \
	ksem_unlink.o \
	ksem_getvalue.o \
	ksem_destroy.o \
	__mac_get_pid.o \
	__mac_get_link.o \
	__mac_set_link.o \
	extattr_set_link.o \
	extattr_get_link.o \
	extattr_delete_link.o \
	__mac_execve.o \
	sigaction.o \
	sigreturn.o \
	getcontext.o \
	setcontext.o \
	swapcontext.o \
	swapoff.o
