/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * $FreeBSD$
 * created from FreeBSD: stable/10/sys/i386/linux/syscalls.master 293582 2016-01-09 17:41:00Z dchagin 
 */

#define	LINUX_SYS_linux_exit	1
#define	LINUX_SYS_linux_fork	2
#define	LINUX_SYS_read	3
#define	LINUX_SYS_write	4
#define	LINUX_SYS_linux_open	5
#define	LINUX_SYS_close	6
#define	LINUX_SYS_linux_waitpid	7
#define	LINUX_SYS_linux_creat	8
#define	LINUX_SYS_linux_link	9
#define	LINUX_SYS_linux_unlink	10
#define	LINUX_SYS_linux_execve	11
#define	LINUX_SYS_linux_chdir	12
#define	LINUX_SYS_linux_time	13
#define	LINUX_SYS_linux_mknod	14
#define	LINUX_SYS_linux_chmod	15
#define	LINUX_SYS_linux_lchown16	16
#define	LINUX_SYS_linux_stat	18
#define	LINUX_SYS_linux_lseek	19
#define	LINUX_SYS_linux_getpid	20
#define	LINUX_SYS_linux_mount	21
#define	LINUX_SYS_linux_oldumount	22
#define	LINUX_SYS_linux_setuid16	23
#define	LINUX_SYS_linux_getuid16	24
#define	LINUX_SYS_linux_stime	25
#define	LINUX_SYS_linux_ptrace	26
#define	LINUX_SYS_linux_alarm	27
#define	LINUX_SYS_linux_fstat	28
#define	LINUX_SYS_linux_pause	29
#define	LINUX_SYS_linux_utime	30
#define	LINUX_SYS_linux_access	33
#define	LINUX_SYS_linux_nice	34
#define	LINUX_SYS_sync	36
#define	LINUX_SYS_linux_kill	37
#define	LINUX_SYS_linux_rename	38
#define	LINUX_SYS_linux_mkdir	39
#define	LINUX_SYS_linux_rmdir	40
#define	LINUX_SYS_dup	41
#define	LINUX_SYS_linux_pipe	42
#define	LINUX_SYS_linux_times	43
#define	LINUX_SYS_linux_brk	45
#define	LINUX_SYS_linux_setgid16	46
#define	LINUX_SYS_linux_getgid16	47
#define	LINUX_SYS_linux_signal	48
#define	LINUX_SYS_linux_geteuid16	49
#define	LINUX_SYS_linux_getegid16	50
#define	LINUX_SYS_acct	51
#define	LINUX_SYS_linux_umount	52
#define	LINUX_SYS_linux_ioctl	54
#define	LINUX_SYS_linux_fcntl	55
#define	LINUX_SYS_setpgid	57
#define	LINUX_SYS_linux_olduname	59
#define	LINUX_SYS_umask	60
#define	LINUX_SYS_chroot	61
#define	LINUX_SYS_linux_ustat	62
#define	LINUX_SYS_dup2	63
#define	LINUX_SYS_linux_getppid	64
#define	LINUX_SYS_getpgrp	65
#define	LINUX_SYS_setsid	66
#define	LINUX_SYS_linux_sigaction	67
#define	LINUX_SYS_linux_sgetmask	68
#define	LINUX_SYS_linux_ssetmask	69
#define	LINUX_SYS_linux_setreuid16	70
#define	LINUX_SYS_linux_setregid16	71
#define	LINUX_SYS_linux_sigsuspend	72
#define	LINUX_SYS_linux_sigpending	73
#define	LINUX_SYS_linux_sethostname	74
#define	LINUX_SYS_linux_setrlimit	75
#define	LINUX_SYS_linux_old_getrlimit	76
#define	LINUX_SYS_getrusage	77
#define	LINUX_SYS_gettimeofday	78
#define	LINUX_SYS_settimeofday	79
#define	LINUX_SYS_linux_getgroups16	80
#define	LINUX_SYS_linux_setgroups16	81
#define	LINUX_SYS_linux_old_select	82
#define	LINUX_SYS_linux_symlink	83
#define	LINUX_SYS_linux_lstat	84
#define	LINUX_SYS_linux_readlink	85
#define	LINUX_SYS_linux_uselib	86
#define	LINUX_SYS_swapon	87
#define	LINUX_SYS_linux_reboot	88
#define	LINUX_SYS_linux_readdir	89
#define	LINUX_SYS_linux_mmap	90
#define	LINUX_SYS_munmap	91
#define	LINUX_SYS_linux_truncate	92
#define	LINUX_SYS_linux_ftruncate	93
#define	LINUX_SYS_fchmod	94
#define	LINUX_SYS_fchown	95
#define	LINUX_SYS_linux_getpriority	96
#define	LINUX_SYS_setpriority	97
#define	LINUX_SYS_linux_statfs	99
#define	LINUX_SYS_linux_fstatfs	100
#define	LINUX_SYS_linux_ioperm	101
#define	LINUX_SYS_linux_socketcall	102
#define	LINUX_SYS_linux_syslog	103
#define	LINUX_SYS_linux_setitimer	104
#define	LINUX_SYS_linux_getitimer	105
#define	LINUX_SYS_linux_newstat	106
#define	LINUX_SYS_linux_newlstat	107
#define	LINUX_SYS_linux_newfstat	108
#define	LINUX_SYS_linux_uname	109
#define	LINUX_SYS_linux_iopl	110
#define	LINUX_SYS_linux_vhangup	111
#define	LINUX_SYS_linux_vm86old	113
#define	LINUX_SYS_linux_wait4	114
#define	LINUX_SYS_linux_swapoff	115
#define	LINUX_SYS_linux_sysinfo	116
#define	LINUX_SYS_linux_ipc	117
#define	LINUX_SYS_fsync	118
#define	LINUX_SYS_linux_sigreturn	119
#define	LINUX_SYS_linux_clone	120
#define	LINUX_SYS_linux_setdomainname	121
#define	LINUX_SYS_linux_newuname	122
#define	LINUX_SYS_linux_modify_ldt	123
#define	LINUX_SYS_linux_adjtimex	124
#define	LINUX_SYS_linux_mprotect	125
#define	LINUX_SYS_linux_sigprocmask	126
#define	LINUX_SYS_linux_create_module	127
#define	LINUX_SYS_linux_init_module	128
#define	LINUX_SYS_linux_delete_module	129
#define	LINUX_SYS_linux_get_kernel_syms	130
#define	LINUX_SYS_linux_quotactl	131
#define	LINUX_SYS_getpgid	132
#define	LINUX_SYS_fchdir	133
#define	LINUX_SYS_linux_bdflush	134
#define	LINUX_SYS_linux_sysfs	135
#define	LINUX_SYS_linux_personality	136
#define	LINUX_SYS_linux_setfsuid16	138
#define	LINUX_SYS_linux_setfsgid16	139
#define	LINUX_SYS_linux_llseek	140
#define	LINUX_SYS_linux_getdents	141
#define	LINUX_SYS_linux_select	142
#define	LINUX_SYS_flock	143
#define	LINUX_SYS_linux_msync	144
#define	LINUX_SYS_readv	145
#define	LINUX_SYS_writev	146
#define	LINUX_SYS_linux_getsid	147
#define	LINUX_SYS_linux_fdatasync	148
#define	LINUX_SYS_linux_sysctl	149
#define	LINUX_SYS_mlock	150
#define	LINUX_SYS_munlock	151
#define	LINUX_SYS_mlockall	152
#define	LINUX_SYS_munlockall	153
#define	LINUX_SYS_linux_sched_setparam	154
#define	LINUX_SYS_linux_sched_getparam	155
#define	LINUX_SYS_linux_sched_setscheduler	156
#define	LINUX_SYS_linux_sched_getscheduler	157
#define	LINUX_SYS_sched_yield	158
#define	LINUX_SYS_linux_sched_get_priority_max	159
#define	LINUX_SYS_linux_sched_get_priority_min	160
#define	LINUX_SYS_linux_sched_rr_get_interval	161
#define	LINUX_SYS_linux_nanosleep	162
#define	LINUX_SYS_linux_mremap	163
#define	LINUX_SYS_linux_setresuid16	164
#define	LINUX_SYS_linux_getresuid16	165
#define	LINUX_SYS_linux_vm86	166
#define	LINUX_SYS_linux_query_module	167
#define	LINUX_SYS_poll	168
#define	LINUX_SYS_linux_nfsservctl	169
#define	LINUX_SYS_linux_setresgid16	170
#define	LINUX_SYS_linux_getresgid16	171
#define	LINUX_SYS_linux_prctl	172
#define	LINUX_SYS_linux_rt_sigreturn	173
#define	LINUX_SYS_linux_rt_sigaction	174
#define	LINUX_SYS_linux_rt_sigprocmask	175
#define	LINUX_SYS_linux_rt_sigpending	176
#define	LINUX_SYS_linux_rt_sigtimedwait	177
#define	LINUX_SYS_linux_rt_sigqueueinfo	178
#define	LINUX_SYS_linux_rt_sigsuspend	179
#define	LINUX_SYS_linux_pread	180
#define	LINUX_SYS_linux_pwrite	181
#define	LINUX_SYS_linux_chown16	182
#define	LINUX_SYS_linux_getcwd	183
#define	LINUX_SYS_linux_capget	184
#define	LINUX_SYS_linux_capset	185
#define	LINUX_SYS_linux_sigaltstack	186
#define	LINUX_SYS_linux_sendfile	187
#define	LINUX_SYS_linux_vfork	190
#define	LINUX_SYS_linux_getrlimit	191
#define	LINUX_SYS_linux_mmap2	192
#define	LINUX_SYS_linux_truncate64	193
#define	LINUX_SYS_linux_ftruncate64	194
#define	LINUX_SYS_linux_stat64	195
#define	LINUX_SYS_linux_lstat64	196
#define	LINUX_SYS_linux_fstat64	197
#define	LINUX_SYS_linux_lchown	198
#define	LINUX_SYS_linux_getuid	199
#define	LINUX_SYS_linux_getgid	200
#define	LINUX_SYS_geteuid	201
#define	LINUX_SYS_getegid	202
#define	LINUX_SYS_setreuid	203
#define	LINUX_SYS_setregid	204
#define	LINUX_SYS_linux_getgroups	205
#define	LINUX_SYS_linux_setgroups	206
#define	LINUX_SYS_setresuid	208
#define	LINUX_SYS_getresuid	209
#define	LINUX_SYS_setresgid	210
#define	LINUX_SYS_getresgid	211
#define	LINUX_SYS_linux_chown	212
#define	LINUX_SYS_setuid	213
#define	LINUX_SYS_setgid	214
#define	LINUX_SYS_linux_setfsuid	215
#define	LINUX_SYS_linux_setfsgid	216
#define	LINUX_SYS_linux_pivot_root	217
#define	LINUX_SYS_linux_mincore	218
#define	LINUX_SYS_madvise	219
#define	LINUX_SYS_linux_getdents64	220
#define	LINUX_SYS_linux_fcntl64	221
#define	LINUX_SYS_linux_gettid	224
#define	LINUX_SYS_linux_setxattr	226
#define	LINUX_SYS_linux_lsetxattr	227
#define	LINUX_SYS_linux_fsetxattr	228
#define	LINUX_SYS_linux_getxattr	229
#define	LINUX_SYS_linux_lgetxattr	230
#define	LINUX_SYS_linux_fgetxattr	231
#define	LINUX_SYS_linux_listxattr	232
#define	LINUX_SYS_linux_llistxattr	233
#define	LINUX_SYS_linux_flistxattr	234
#define	LINUX_SYS_linux_removexattr	235
#define	LINUX_SYS_linux_lremovexattr	236
#define	LINUX_SYS_linux_fremovexattr	237
#define	LINUX_SYS_linux_tkill	238
#define	LINUX_SYS_linux_sys_futex	240
#define	LINUX_SYS_linux_sched_setaffinity	241
#define	LINUX_SYS_linux_sched_getaffinity	242
#define	LINUX_SYS_linux_set_thread_area	243
#define	LINUX_SYS_linux_get_thread_area	244
#define	LINUX_SYS_linux_fadvise64	250
#define	LINUX_SYS_linux_exit_group	252
#define	LINUX_SYS_linux_lookup_dcookie	253
#define	LINUX_SYS_linux_epoll_create	254
#define	LINUX_SYS_linux_epoll_ctl	255
#define	LINUX_SYS_linux_epoll_wait	256
#define	LINUX_SYS_linux_remap_file_pages	257
#define	LINUX_SYS_linux_set_tid_address	258
#define	LINUX_SYS_linux_timer_create	259
#define	LINUX_SYS_linux_timer_settime	260
#define	LINUX_SYS_linux_timer_gettime	261
#define	LINUX_SYS_linux_timer_getoverrun	262
#define	LINUX_SYS_linux_timer_delete	263
#define	LINUX_SYS_linux_clock_settime	264
#define	LINUX_SYS_linux_clock_gettime	265
#define	LINUX_SYS_linux_clock_getres	266
#define	LINUX_SYS_linux_clock_nanosleep	267
#define	LINUX_SYS_linux_statfs64	268
#define	LINUX_SYS_linux_fstatfs64	269
#define	LINUX_SYS_linux_tgkill	270
#define	LINUX_SYS_linux_utimes	271
#define	LINUX_SYS_linux_fadvise64_64	272
#define	LINUX_SYS_linux_mbind	274
#define	LINUX_SYS_linux_get_mempolicy	275
#define	LINUX_SYS_linux_set_mempolicy	276
#define	LINUX_SYS_linux_mq_open	277
#define	LINUX_SYS_linux_mq_unlink	278
#define	LINUX_SYS_linux_mq_timedsend	279
#define	LINUX_SYS_linux_mq_timedreceive	280
#define	LINUX_SYS_linux_mq_notify	281
#define	LINUX_SYS_linux_mq_getsetattr	282
#define	LINUX_SYS_linux_kexec_load	283
#define	LINUX_SYS_linux_waitid	284
#define	LINUX_SYS_linux_add_key	286
#define	LINUX_SYS_linux_request_key	287
#define	LINUX_SYS_linux_keyctl	288
#define	LINUX_SYS_linux_ioprio_set	289
#define	LINUX_SYS_linux_ioprio_get	290
#define	LINUX_SYS_linux_inotify_init	291
#define	LINUX_SYS_linux_inotify_add_watch	292
#define	LINUX_SYS_linux_inotify_rm_watch	293
#define	LINUX_SYS_linux_migrate_pages	294
#define	LINUX_SYS_linux_openat	295
#define	LINUX_SYS_linux_mkdirat	296
#define	LINUX_SYS_linux_mknodat	297
#define	LINUX_SYS_linux_fchownat	298
#define	LINUX_SYS_linux_futimesat	299
#define	LINUX_SYS_linux_fstatat64	300
#define	LINUX_SYS_linux_unlinkat	301
#define	LINUX_SYS_linux_renameat	302
#define	LINUX_SYS_linux_linkat	303
#define	LINUX_SYS_linux_symlinkat	304
#define	LINUX_SYS_linux_readlinkat	305
#define	LINUX_SYS_linux_fchmodat	306
#define	LINUX_SYS_linux_faccessat	307
#define	LINUX_SYS_linux_pselect6	308
#define	LINUX_SYS_linux_ppoll	309
#define	LINUX_SYS_linux_unshare	310
#define	LINUX_SYS_linux_set_robust_list	311
#define	LINUX_SYS_linux_get_robust_list	312
#define	LINUX_SYS_linux_splice	313
#define	LINUX_SYS_linux_sync_file_range	314
#define	LINUX_SYS_linux_tee	315
#define	LINUX_SYS_linux_vmsplice	316
#define	LINUX_SYS_linux_move_pages	317
#define	LINUX_SYS_linux_getcpu	318
#define	LINUX_SYS_linux_epoll_pwait	319
#define	LINUX_SYS_linux_utimensat	320
#define	LINUX_SYS_linux_signalfd	321
#define	LINUX_SYS_linux_timerfd_create	322
#define	LINUX_SYS_linux_eventfd	323
#define	LINUX_SYS_linux_fallocate	324
#define	LINUX_SYS_linux_timerfd_settime	325
#define	LINUX_SYS_linux_timerfd_gettime	326
#define	LINUX_SYS_linux_signalfd4	327
#define	LINUX_SYS_linux_eventfd2	328
#define	LINUX_SYS_linux_epoll_create1	329
#define	LINUX_SYS_linux_dup3	330
#define	LINUX_SYS_linux_pipe2	331
#define	LINUX_SYS_linux_inotify_init1	332
#define	LINUX_SYS_linux_preadv	333
#define	LINUX_SYS_linux_pwritev	334
#define	LINUX_SYS_linux_rt_tsigqueueinfo	335
#define	LINUX_SYS_linux_perf_event_open	336
#define	LINUX_SYS_linux_recvmmsg	337
#define	LINUX_SYS_linux_fanotify_init	338
#define	LINUX_SYS_linux_fanotify_mark	339
#define	LINUX_SYS_linux_prlimit64	340
#define	LINUX_SYS_linux_name_to_handle_at	341
#define	LINUX_SYS_linux_open_by_handle_at	342
#define	LINUX_SYS_linux_clock_adjtime	343
#define	LINUX_SYS_linux_syncfs	344
#define	LINUX_SYS_linux_sendmmsg	345
#define	LINUX_SYS_linux_setns	346
#define	LINUX_SYS_linux_process_vm_readv	347
#define	LINUX_SYS_linux_process_vm_writev	348
#define	LINUX_SYS_MAXSYSCALL	350
