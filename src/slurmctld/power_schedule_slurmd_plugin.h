/*****************************************************************************\
 *  power_schedule_slurmd_plugin.h - power allocator plugin interface
 *****************************************************************************
 *  Copyright (C) "The PomPP research team" supported by the JST, 
 *  CREST research program. <http://www.hal.ipc.i.u-tokyo.ac.jp/research/pompp/>
 *  Written by Ryuichi Sakamoto <r-sakamoto@hal.ipc.i.u-tokyo.ac.jp>
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#ifndef __POWERSCHEDULESLURMD_PLUGIN_API_H
#define __POWERSCHEDULESLURMD_PLUGIN_API_H

#include "slurm/slurm.h"
#include "src/slurmctld/slurmctld.h"

#include "src/common/layouts_mgr.h"

/**
 * Initialize the power allocator adapter.
 * Returns a SLURM errno.
 */
extern int power_schedule_slurmd_init(void);


/**
 * Terminate power allocator adapter, free memory.
 * Returns a SLURM errno.
 */
extern int power_schedule_slurmd_fini(void);

/*
 ****************************************************************************
 *                       P L U G I N   C A L L S                            *
 ****************************************************************************
 */

/**
 * Perform reconfig, re-read any configuration files
 */
int power_schedule_slurmd_g_reconfig(void);






#endif /* !__POWERSCHEDULESLURMD_PLUGIN_API_H */
