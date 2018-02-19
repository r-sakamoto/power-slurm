/*****************************************************************************\
 *  power_schedule_slurmd_plugin.c - power allocator plugin interface
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

#include "slurm/slurm.h"
#include "src/slurmctld/slurmctld.h"

#include <pthread.h>

#include "src/common/log.h"
#include "src/common/plugrack.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"

#include "src/slurmctld/power_schedule_slurmd_plugin.h"
#include "src/slurmctld/slurmctld.h"

#include "src/common/layout.h"
#include "src/common/layouts_mgr.h"

/* ************************************************************************ */
/*  TAG(              power_schedule_slurmd_ops_t                        )  */
/* ************************************************************************ */
typedef struct power_schedule_slurmd_ops {
	int (*reconfig)(void);
	int (*send_to_slurmd_autocap_request)();
} power_schedule_slurmd_ops_t;

/*
 * Must be synchronized with slurm_sched_ops_t above.
 */
static const char *syms[] = {
	"power_schedule_slurmd_p_reconfig",
	"power_schedule_slurmd_p_send_to_slurmd_autocap_request"
};

static power_schedule_slurmd_ops_t ops;
static plugin_context_t	*g_context = NULL;
static pthread_mutex_t g_context_lock = PTHREAD_MUTEX_INITIALIZER;
static bool init_run = false;


/**
 * Initialize the power allocator adapter.
 * Returns a SLURM errno.
 */
extern int power_schedule_slurmd_init(void)
{
	int retval = SLURM_SUCCESS;
	char *plugin_type = "power_schedule_slurmd";
	char *type = NULL;

	if ( init_run && g_context )
		return retval;

	slurm_mutex_lock( &g_context_lock );

	if ( g_context )
		goto done;

	//type = slurm_get_powerallocator_type();
	type = slurm_get_powerascheduleslurmd_type();
    printf("CAO: power_schedule_slurmd_init() -> plugin_type='%s'\n", plugin_type);
    printf("CAO: power_schedule_slurmd_init() -> type       ='%s'\n", type);
	g_context = plugin_context_create(
		plugin_type, type, (void **)&ops, syms, sizeof(syms));

	if (!g_context) {
		error("cannot create %s context for %s", plugin_type, type);
		retval = SLURM_ERROR;
		goto done;
	}
	init_run = true;

done:
	slurm_mutex_unlock( &g_context_lock );
	xfree(type);
	return retval;
}

/**
 * Terminate power allocator adapter, free memory.
 * Returns a SLURM errno.
 */
extern int power_schedule_slurmd_fini(void)
{
	int rc;

	if (!g_context)
		return SLURM_SUCCESS;

	init_run = false;
	rc = plugin_context_destroy(g_context);
	g_context = NULL;

	return rc;
}

/*
 ****************************************************************************
 *                       P L U G I N   C A L L S                            *
 ****************************************************************************
 */

/**
 * Perform reconfig, re-read any configuration files
 */
int power_schedule_slurmd_g_reconfig(void)
{
	if ( power_schedule_slurmd_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.reconfig))();
}

int power_schedule_slurmd_g_send_to_slurmd_autocap_request()
{
	if ( power_schedule_slurmd_init() < 0 )
		return SLURM_ERROR;	

	return (*(ops.send_to_slurmd_autocap_request))();
}


