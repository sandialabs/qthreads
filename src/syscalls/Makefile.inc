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
			 syscalls/sys_read.c \
			 syscalls/sys_select.c \
			 syscalls/sys_sleep.c \
			 syscalls/sys_usleep.c \
			 syscalls/sys_wait4.c \
			 syscalls/sys_write.c \
			 syscalls/sys_user_defined.c

if HAVE_DECL_SYS_SYSTEM
libqthread_la_SOURCES += \
			 syscalls/sys_system.c
endif
