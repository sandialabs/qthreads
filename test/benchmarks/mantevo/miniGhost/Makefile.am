.PHONY: buildall buildextra benchmarks

benchmarks: minighost
buildall: minighost
buildextra: minighost

EXTRA_PROGRAMS = minighost
CLEANFILES = minighost

AM_CPPFLAGS = -I$(top_srcdir)/include
#qthreadlib = $(top_builddir)/src/libqthread.la

#LDADD = $(qthreadlib)

$(qthreadlib):
	$(MAKE) -C $(top_builddir)/src #libqthread.la

minighost_SOURCES = main.c \
mg_bc.c \
mg_bex.c \
mg_bex_SR.c \
mg_bex_SiR.c \
mg_bex_diags_SR.c \
mg_bex_diags_SiR.c \
mg_bex_diags_iSR.c \
mg_bex_diags_iSiR.c \
mg_bex_iSR.c \
mg_bex_iSiR.c \
mg_block_def_comm_first_rand.c \
mg_block_def_random.c \
mg_block_init.c \
mg_comm_utils.c \
mg_fake_work.c \
mg_flux_accumulate.c \
mg_flux_accumulate_2d5pt3d7pt.c \
mg_flux_accumulate_2d9pt.c \
mg_flux_accumulate_3d27pt.c \
mg_grid_utils.c \
mg_profiling.c \
mg_spikes.c \
mg_stencils.c \
mg_sum_grid.c \
mg_utils.c \
mpiPart.cc \
mg_finepoints.c

# integrate these later
# mg_bex_SR_omp.c
# mpiPart.c

# TODO(npe) allow selecting mpiq or not
minighost_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS) -DUSING_QTHREADS -D_MG_QT -D_MG_MPIF -D_MG_TIMING_L0 -D_MG_DOUBLE -D_MG_INT8 -DWALL -D_MG_MPI
minighost_CFLAGS = $(AM_CFLAGS) $(CFLAGS) $(MPI_CFLAGS)
minighost_LDFLAGS = $(MPI_CLDFLAGS) -L../../../../src/.libs #-lqthread

