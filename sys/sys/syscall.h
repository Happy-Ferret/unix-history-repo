/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from FreeBSD: src/sys/kern/syscalls.master,v 1.63 1999/08/28 00:46:19 peter Exp 
 */

#define	SYS_syscall	0
#define	SYS_exit	1
#define	SYS_fork	2
#define	SYS_read	3
#define	SYS_write	4
#define	SYS_open	5
#define	SYS_close	6
#define	SYS_wait4	7
				/* 8 is old creat */
#define	SYS_link	9
#define	SYS_unlink	10
				/* 11 is obsolete execv */
#define	SYS_chdir	12
#define	SYS_fchdir	13
#define	SYS_mknod	14
#define	SYS_chmod	15
#define	SYS_chown	16
#define	SYS_break	17
#define	SYS_getfsstat	18
				/* 19 is old lseek */
#define	SYS_getpid	20
#define	SYS_mount	21
#define	SYS_unmount	22
#define	SYS_setuid	23
#define	SYS_getuid	24
#define	SYS_geteuid	25
#define	SYS_ptrace	26
#define	SYS_recvmsg	27
#define	SYS_sendmsg	28
#define	SYS_recvfrom	29
#define	SYS_accept	30
#define	SYS_getpeername	31
#define	SYS_getsockname	32
#define	SYS_access	33
#define	SYS_chflags	34
#define	SYS_fchflags	35
#define	SYS_sync	36
#define	SYS_kill	37
				/* 38 is old stat */
#define	SYS_getppid	39
				/* 40 is old lstat */
#define	SYS_dup	41
#define	SYS_pipe	42
#define	SYS_getegid	43
#define	SYS_profil	44
#define	SYS_ktrace	45
#define	SYS_sigaction	46
#define	SYS_getgid	47
#define	SYS_sigprocmask	48
#define	SYS_getlogin	49
#define	SYS_setlogin	50
#define	SYS_acct	51
#define	SYS_sigpending	52
#define	SYS_sigaltstack	53
#define	SYS_ioctl	54
#define	SYS_reboot	55
#define	SYS_revoke	56
#define	SYS_symlink	57
#define	SYS_readlink	58
#define	SYS_execve	59
#define	SYS_umask	60
#define	SYS_chroot	61
				/* 62 is old fstat */
				/* 63 is old getkerninfo */
				/* 64 is old getpagesize */
#define	SYS_msync	65
#define	SYS_vfork	66
				/* 67 is obsolete vread */
				/* 68 is obsolete vwrite */
#define	SYS_sbrk	69
#define	SYS_sstk	70
				/* 71 is old mmap */
#define	SYS_vadvise	72
#define	SYS_munmap	73
#define	SYS_mprotect	74
#define	SYS_madvise	75
				/* 76 is obsolete vhangup */
				/* 77 is obsolete vlimit */
#define	SYS_mincore	78
#define	SYS_getgroups	79
#define	SYS_setgroups	80
#define	SYS_getpgrp	81
#define	SYS_setpgid	82
#define	SYS_setitimer	83
				/* 84 is old wait */
#define	SYS_swapon	85
#define	SYS_getitimer	86
				/* 87 is old gethostname */
				/* 88 is old sethostname */
#define	SYS_getdtablesize	89
#define	SYS_dup2	90
#define	SYS_fcntl	92
#define	SYS_select	93
#define	SYS_fsync	95
#define	SYS_setpriority	96
#define	SYS_socket	97
#define	SYS_connect	98
				/* 99 is old accept */
#define	SYS_getpriority	100
				/* 101 is old send */
				/* 102 is old recv */
#define	SYS_sigreturn	103
#define	SYS_bind	104
#define	SYS_setsockopt	105
#define	SYS_listen	106
				/* 107 is obsolete vtimes */
				/* 108 is old sigvec */
				/* 109 is old sigblock */
				/* 110 is old sigsetmask */
#define	SYS_sigsuspend	111
				/* 112 is old sigstack */
				/* 113 is old recvmsg */
				/* 114 is old sendmsg */
				/* 115 is obsolete vtrace */
#define	SYS_gettimeofday	116
#define	SYS_getrusage	117
#define	SYS_getsockopt	118
#define	SYS_readv	120
#define	SYS_writev	121
#define	SYS_settimeofday	122
#define	SYS_fchown	123
#define	SYS_fchmod	124
				/* 125 is old recvfrom */
#define	SYS_setreuid	126
#define	SYS_setregid	127
#define	SYS_rename	128
				/* 129 is old truncate */
				/* 130 is old ftruncate */
#define	SYS_flock	131
#define	SYS_mkfifo	132
#define	SYS_sendto	133
#define	SYS_shutdown	134
#define	SYS_socketpair	135
#define	SYS_mkdir	136
#define	SYS_rmdir	137
#define	SYS_utimes	138
				/* 139 is obsolete 4.2 sigreturn */
#define	SYS_adjtime	140
				/* 141 is old getpeername */
				/* 142 is old gethostid */
				/* 143 is old sethostid */
				/* 144 is old getrlimit */
				/* 145 is old setrlimit */
				/* 146 is old killpg */
#define	SYS_setsid	147
#define	SYS_quotactl	148
				/* 149 is old quota */
				/* 150 is old getsockname */
#define	SYS_nfssvc	155
				/* 156 is old getdirentries */
#define	SYS_statfs	157
#define	SYS_fstatfs	158
#define	SYS_getfh	161
#define	SYS_getdomainname	162
#define	SYS_setdomainname	163
#define	SYS_uname	164
#define	SYS_sysarch	165
#define	SYS_rtprio	166
#define	SYS_semsys	169
#define	SYS_msgsys	170
#define	SYS_shmsys	171
#define	SYS_pread	173
#define	SYS_pwrite	174
#define	SYS_ntp_adjtime	176
#define	SYS_setgid	181
#define	SYS_setegid	182
#define	SYS_seteuid	183
#define	SYS_stat	188
#define	SYS_fstat	189
#define	SYS_lstat	190
#define	SYS_pathconf	191
#define	SYS_fpathconf	192
#define	SYS_getrlimit	194
#define	SYS_setrlimit	195
#define	SYS_getdirentries	196
#define	SYS_mmap	197
#define	SYS___syscall	198
#define	SYS_lseek	199
#define	SYS_truncate	200
#define	SYS_ftruncate	201
#define	SYS___sysctl	202
#define	SYS_mlock	203
#define	SYS_munlock	204
#define	SYS_undelete	205
#define	SYS_futimes	206
#define	SYS_getpgid	207
#define	SYS_poll	209
#define	SYS___semctl	220
#define	SYS_semget	221
#define	SYS_semop	222
#define	SYS_semconfig	223
#define	SYS_msgctl	224
#define	SYS_msgget	225
#define	SYS_msgsnd	226
#define	SYS_msgrcv	227
#define	SYS_shmat	228
#define	SYS_shmctl	229
#define	SYS_shmdt	230
#define	SYS_shmget	231
#define	SYS_clock_gettime	232
#define	SYS_clock_settime	233
#define	SYS_clock_getres	234
#define	SYS_nanosleep	240
#define	SYS_minherit	250
#define	SYS_rfork	251
#define	SYS_openbsd_poll	252
#define	SYS_issetugid	253
#define	SYS_lchown	254
#define	SYS_getdents	272
#define	SYS_lchmod	274
#define	SYS_netbsd_lchown	275
#define	SYS_lutimes	276
#define	SYS_netbsd_msync	277
#define	SYS_nstat	278
#define	SYS_nfstat	279
#define	SYS_nlstat	280
#define	SYS_modnext	300
#define	SYS_modstat	301
#define	SYS_modfnext	302
#define	SYS_modfind	303
#define	SYS_kldload	304
#define	SYS_kldunload	305
#define	SYS_kldfind	306
#define	SYS_kldnext	307
#define	SYS_kldstat	308
#define	SYS_kldfirstmod	309
#define	SYS_getsid	310
				/* 313 is obsolete signanosleep */
#define	SYS_aio_return	314
#define	SYS_aio_suspend	315
#define	SYS_aio_cancel	316
#define	SYS_aio_error	317
#define	SYS_aio_read	318
#define	SYS_aio_write	319
#define	SYS_lio_listio	320
#define	SYS_yield	321
#define	SYS_thr_sleep	322
#define	SYS_thr_wakeup	323
#define	SYS_mlockall	324
#define	SYS_munlockall	325
#define	SYS___getcwd	326
#define	SYS_sched_setparam	327
#define	SYS_sched_getparam	328
#define	SYS_sched_setscheduler	329
#define	SYS_sched_getscheduler	330
#define	SYS_sched_yield	331
#define	SYS_sched_get_priority_max	332
#define	SYS_sched_get_priority_min	333
#define	SYS_sched_rr_get_interval	334
#define	SYS_utrace	335
#define	SYS_sendfile	336
#define	SYS_kldsym	337
#define	SYS_jail	338
#define	SYS_MAXSYSCALL	340
