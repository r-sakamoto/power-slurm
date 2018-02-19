/*****************************************************************************\
 *  power_allocator_plugin.c - power allocator plugin interface
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

#include "src/slurmctld/power_allocator_plugin.h"
#include "src/slurmctld/slurmctld.h"

#include "src/common/layout.h"
#include "src/common/layouts_mgr.h"

/* ************************************************************************ */
/*  TAG(              slurm_power_allocator_ops_t                        )  */
/* ************************************************************************ */
typedef struct slurm_power_allocator_ops {
	int (*reconfig)(void);
	int (*do_power_safe)(void);
	int (*job_test_power)(struct job_record *, bitstr_t *, 
			uint32_t , uint32_t , uint32_t ,
			uint32_t ,
			uint32_t , uint32_t , uint32_t , 
			uint32_t);
	int (*job_test_old)(struct job_record *);	
	int (*job_test)(void);		
	int (*schedule)(void);
	int (*alert)(uint32_t);
	int (*job_prolog)(struct job_record *);
	int (*job_resized)(struct job_record *,struct node_record *);
	int (*job_signal)(struct job_record *,int );
	int (*job_fini)(struct job_record *);
	int (*job_suspend)(struct job_record *, bool );
	int (*job_resume)(struct job_record *, bool);
} slurm_power_allocator_ops_t;

/*
 * Must be synchronized with slurm_sched_ops_t above.
 */
static const char *syms[] = {
	"power_allocator_p_reconfig",
	"power_allocator_p_do_power_safe",
	"power_allocator_p_job_test_power",
	"power_allocator_p_job_test_old",
	"power_allocator_p_job_test",
	"power_allocator_p_schedule",
	"power_allocator_p_alert",
	"power_allocator_p_job_prolog",
	"power_allocator_p_job_resized",
	"power_allocator_p_job_signal",
	"power_allocator_p_job_fini",
	"power_allocator_p_job_suspend",
	"power_allocator_p_job_resume"
};

static slurm_power_allocator_ops_t ops;
static plugin_context_t	*g_context = NULL;
static pthread_mutex_t g_context_lock = PTHREAD_MUTEX_INITIALIZER;
static bool init_run = false;


/**
 * Initialize the power allocator adapter.
 * Returns a SLURM errno.
 */
extern int power_allocator_init(void)
{
	int retval = SLURM_SUCCESS;
	char *plugin_type = "power_allocator";
	char *type = NULL;

	if ( init_run && g_context )
		return retval;

	slurm_mutex_lock( &g_context_lock );

	if ( g_context )
		goto done;

	type = slurm_get_powerallocator_type();
    printf("CAO: power_allocator_init() -> plugin_type='%s'\n", plugin_type);
    printf("CAO: power_allocator_init() -> type       ='%s'\n", type);
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
extern int power_allocator_fini(void)
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
int power_allocator_g_reconfig(void)
{
	if ( power_allocator_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.reconfig))();
}
/**
 * Estimate a job start time under specified constraints
 * IN/OUT job_ptr - pointer to job being considered for estimation
 * IN/OUT bitmap - map of nodes being considered for estimation on input,
 *                map of nodes actually to be assigned on output
 * IN min_nodes - minimum number of nodes to allocate to job
 * IN max_nodes - maximum number of nodes to allocate to job
 * IN req_nodes - requested (or desired) count of nodes
 * IN constraint_mode - choose how to estimate power.
 *                      give flags which constraints are used.
 *                       PA_FLAG_CPU_CAP : use maximum cpu cap
 *                       PA_FLAG_DRAM_CAP : use maximum dram cap
 *                       PA_FLAG_FREQ : use maximum frequency 
 * IN cpu_cap - maximum number of power to allocate to a cpu socket
 * IN dram_cap - maximum number of power to allocate to a memory channel
 *            currently sopported Intel RAPL
 * IN frequency - maximum number of frequency to a cpu
 * IN job_power - estimated power consumption by under constraints
 * // TODO OUT job_power_layout - best power allocation table sfeciied kvs
 * RET zero on success, EINVAL otherwise
 */
int power_allocator_g_job_test_power(struct job_record *job_ptr, bitstr_t *bitmap, 
			uint32_t min_nodes, uint32_t max_nodes, uint32_t req_nodes,
			uint32_t constraint_mode,
			uint32_t cpu_cap, uint32_t dram_cap, uint32_t frequency, 
			uint32_t job_power)
{
	if ( power_allocator_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.job_test_power))
		(job_ptr, bitmap,
		 min_nodes, max_nodes,
		 req_nodes, constraint_mode,
		 cpu_cap, dram_cap, frequency, 
		 job_power);
//, job_power_layout);

}

/**
 * Estimate a job start time under specified constraints
 * IN/OUT job_ptr - pointer to job being considered for estimation
 * IN/OUT bitmap - map of nodes being considered for estimation on input,
 *                map of nodes actually to be assigned on output
 * IN min_nodes - minimum number of nodes to allocate to job
 * IN max_nodes - maximum number of nodes to allocate to job
 * IN req_nodes - requested (or desired) count of nodes
 * IN constraint_mode - choose how to estimate power.
 *                      give flags which constraints are used.
 *                       PA_FLAG_CPU_CAP : use maximum cpu cap
 *                       PA_FLAG_DRAM_CAP : use maximum dram cap
 *                       PA_FLAG_FREQ : use maximum frequency 
 * IN cpu_cap - maximum number of power to allocate to a cpu socket
 * IN dram_cap - maximum number of power to allocate to a memory channel
 *            currently sopported Intel RAPL
 * IN frequency - maximum number of frequency to a cpu
 * IN job_power - estimated power consumption by under constraints
 * // TODO OUT job_power_layout - best power allocation table sfeciied kvs
 * RET zero on success, EINVAL otherwise
 */
int power_allocator_g_job_test_old(struct job_record *job_ptr)
{
	if ( power_allocator_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.job_test_old))
		(job_ptr);
//, job_power_layout);

}

int power_allocator_g_job_test(void)
{
	if ( power_allocator_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.job_test));
//		(job_ptr);
//, job_power_layout);

}



int power_allocator_g_schedule(void)
{
	if ( power_allocator_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.schedule))();
}

int power_allocator_g_alert(uint32_t power_usage)
{
	if ( power_allocator_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.alert))(power_usage);
}

/**
 * Note initiation of job is about to begin. Called immediately
 * after select_p_job_test(). Executed from slurmctld.
 * update job constraint. if the allocator plugin supports a mutable job, 
 * update job_ptr here.
 * IN job_ptr - pointer to job being initiated
 */
//uint32_t power_allocator_g_update_job_constraint(struct job_record *job_ptr);
int power_allocator_g_job_prolog(struct job_record *job_ptr)
{
	if ( power_allocator_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.job_prolog))(job_ptr);
}


int  power_allocator_g_do_power_safe (void )
{
	if ( power_allocator_init() < 0 )
	return SLURM_ERROR;

	return (*(ops.do_power_safe))();
}
// TODO
//int power_allocator_g_job_resized(struct job_record *job_ptr,struct node_record *node_ptr);
//int power_allocator_g_job_signal(struct job_record *job_ptr,int signal);
//int power_allocator_g_job_suspend(struct job_record *job_ptr, bool indf_susp);
//int power_allocator_g_job_resume(struct job_record *job_ptr, bool indf_susp);
//uint32_t _power_allocator_g_job_power_kvs_table2node_list();
//uint32_t _power_allocator_g_node_power_kvs_table2node_power();


