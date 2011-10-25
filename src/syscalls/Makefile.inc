# -*- Makefile -*-
# vim:ft=automake
#
# Copyright (c) 2011 Sandia Corporation
#

libqthread_la_SOURCES += \
			 syscalls/sys_accept.c \
			 syscalls/sys_connect.c \
			 syscalls/sys_nanosleep.c \
			 syscalls/sys_poll.c \
			 syscalls/sys_pread.c \
			 syscalls/sys_pwrite.c \
			 syscalls/sys_read.c \
			 syscalls/sys_select.c \
			 syscalls/sys_sleep.c \
			 syscalls/sys_system.c \
			 syscalls/sys_usleep.c \
			 syscalls/sys_user_defined.c \
			 syscalls/sys_wait4.c \
			 syscalls/sys_write.c
