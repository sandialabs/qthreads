/**********************************************************************************************/
/*  This program is part of the Barcelona OpenMP Tasks Suite                                  */
/*  Copyright (C) 2009 Barcelona Supercomputing Center - Centro Nacional de Supercomputacion  */
/*  Copyright (C) 2009 Universitat Politecnica de Catalunya                                   */
/*                                                                                            */
/*  This program is free software; you can redistribute it and/or modify                      */
/*  it under the terms of the GNU General Public License as published by                      */
/*  the Free Software Foundation; either version 2 of the License, or                         */
/*  (at your option) any later version.                                                       */
/*                                                                                            */
/*  This program is distributed in the hope that it will be useful,                           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of                            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                             */
/*  GNU General Public License for more details.                                              */
/*                                                                                            */
/*  You should have received a copy of the GNU General Public License                         */
/*  along with this program; if not, write to the Free Software                               */
/*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA            */
/**********************************************************************************************/

#include "omp-tasks-app.h"
#include "health.h"

#define BOTS_APP_NAME "Health"
#define BOTS_APP_PARAMETERS_DESC "%s"
#define BOTS_APP_PARAMETERS_LIST ,bots_arg_file

//#define BOTS_APP_SELF_TIMING

#define BOTS_APP_USES_ARG_FILE
#define BOTS_APP_DEF_ARG_FILE "Input filename"
#define BOTS_APP_DESC_ARG_FILE "Health input file (mandatory)"

#define BOTS_CUTOFF_DEF_VALUE 2

#define BOTS_APP_INIT \
   struct Village *top;\
   read_input_data(bots_arg_file);

#define KERNEL_INIT \
   allocate_village(&top, NULL, NULL, sim_level, 0);

#define KERNEL_CALL sim_village_main_par(top);
 
#define KERNEL_FINI

//#define KERNEL_SEQ_INIT
//#define KERNEL_SEQ_CALL
//#define KERNEL_SEQ_FINI

#define KERNEL_CHECK check_village(top);

