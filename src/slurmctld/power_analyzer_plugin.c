/*****************************************************************************\
 *  power_analyzer_plugin.c - power analyzer plugin interface
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

#include "src/slurmctld/power_analyzer_plugin.h"
#include "src/slurmctld/slurmctld.h"

/* ************************************************************************ */
/*  TAG(                     power_analyzer_ops_t                        )  */
/* ************************************************************************ */
typedef struct power_analyzer_ops {
	int (*reconfig)	(void);
	int (*estimate_job_power)
			(struct job_record *, bitstr_t *, 
			uint32_t, uint32_t, uint32_t, uint32_t,
			uint32_t, uint32_t, uint32_t, uint32_t);
	int (*estimate_job_time)
			(struct job_record *, bitstr_t *, 
			uint32_t , uint32_t , uint32_t ,uint32_t , 
			uint32_t , uint32_t , uint32_t , uint32_t , float *);
	int (*estimate_job_performance)
			(struct job_record *, bitstr_t *, 
			uint32_t, uint32_t, uint32_t, uint32_t, 
			uint32_t, uint32_t, uint32_t, uint32_t,	float *);
	int (*analyze_finished_job)
			(struct job_record *);
	int (*analyze_interval)(void);
} power_analyzer_ops_t;

/*
 * Must be synchronized with power_analyzer_ops_t above.
 */
static const char *syms[] = {
	"power_analyzer_p_reconfig",
	"power_analyzer_p_estimate_job_power", 
	"power_analyzer_p_estimate_job_time",
	"power_analyzer_p_estimate_job_performance", 
	"power_analyzer_p_analyze_finished_job",
	"power_analyzer_p_analyze_interval"
};

static power_analyzer_ops_t ops;
static plugin_context_t	*g_context = NULL;
static pthread_mutex_t g_context_lock = PTHREAD_MUTEX_INITIALIZER;
static bool init_run = false;

/* *********************************************************************** */
/*  TAG(                        slurm_sched_init                        )  */
/* Initialize the external power analyzer adapter.                         */
/* Returns a SLURM errno.                                                  */
/*                                                                         */
/* *********************************************************************** */
extern int power_analyzer_init(void)
{
	int retval = SLURM_SUCCESS;
	char *plugin_type = "power_analyzer";
	char *type = NULL;

	debug3("power_analyzer_init 0");	

	if ( init_run && g_context )
		return retval;

	debug3("power_analyzer_init 1");	
	slurm_mutex_lock( &g_context_lock );

	debug3("power_analyzer_init 2");	
	if ( g_context )
		goto done;

	debug3("power_analyzer_init 3");	
	type = slurm_get_poweranalyzer_type();
	g_context = plugin_context_create(
		plugin_type, type, (void **)&ops, syms, sizeof(syms));

	debug3("power_analyzer_init 4");	
	if (!g_context) {
		error("cannot create %s context for %s", plugin_type, type);
		retval = SLURM_ERROR;
		goto done;
	}
	init_run = true;

	debug3("power_analyzer_init 5");	
done:
	slurm_mutex_unlock( &g_context_lock );
	debug3("power_analyzer_init 6");	
	xfree(type);
	return retval;
}

/* *********************************************************************** */
/*  TAG(                      power_analyzer_fini                       )  */
/* *********************************************************************** */
/**
 * Terminate power analyzer adapter, free memory.
 * Returns a SLURM errno.
 */
extern int power_analyzer_fini(void)
{
	int rc;

	if (!g_context)
		return SLURM_SUCCESS;

	init_run = false;
	rc = plugin_context_destroy(g_context);
	g_context = NULL;

	return rc;
}


/* *********************************************************************** */
/*  TAG(                      power_analyzer_g_reconfig                 )  */
/* *********************************************************************** */
/**
 * Perform reconfig, re-read any configuration files
 */
int power_analyzer_g_reconfig(void)
{
	if ( power_analyzer_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.reconfig))();
}

/* *********************************************************************** */
/*  TAG(               power_analyzer_g_estimate_job_power              )  */
/* *********************************************************************** */
/**
 * Estimate a job power consumption under specified constraints
 * IN job_ptr - pointer to job being considered for estimation
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
 * OUT job_power - estimated power consumption by under constraints
 * RET zero on success, EINVAL otherwise
 */
int power_analyzer_g_estimate_job_power(struct job_record *job_ptr, bitstr_t *bitmap, 
			uint32_t min_nodes, uint32_t max_nodes, uint32_t req_nodes,
			uint32_t constraint_mode,
			uint32_t cpu_cap, uint32_t dram_cap, uint32_t frequency, 
			uint32_t job_power)
{
	if ( power_analyzer_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.estimate_job_power))
		(job_ptr, bitmap,  min_nodes, max_nodes, req_nodes,
		constraint_mode, cpu_cap, dram_cap, frequency,
		job_power);
}

/**
 * Estimate a job execution time under specified constraints
 * IN job_ptr - pointer to job being considered for estimation
 * IN/OUT bitmap - map of nodes being considered for estimation on input,
 *                map of nodes actually to be assigned on output
 * IN min_nodes - minimum number of nodes to allocate to job
 * IN max_nodes - maximum number of nodes to allocate to job
 * IN req_nodes - requested (or desired) count of nodes
 * IN constraint_mode - choose how to estimate performance degradation.
 *                      give flags which constraints are used.
 *                       PA_FLAG_CPU_CAP : use maximum cpu cap
 *                       PA_FLAG_DRAM_CAP : use maximum dram cap
 *                       PA_FLAG_FREQ : use maximum frequency 
 *                       PA_FLAG_JOB : use maximum job power limit
 * IN job_limit - maximum number of power to allocate to job
 * IN cpu_cap - maximum number of power to allocate to a cpu socket
 * IN dram_cap - maximum number of power to allocate to a memory channel
 *            currently sopported Intel RAPL
 * IN frequency - maximum number of frequency to a cpu
 * OUT time - estimated job time. Determined by under constraints
 * RET zero on success, EINVAL otherwise
 */
int power_analyzer_g_estimate_job_time(struct job_record *job_ptr, bitstr_t *bitmap, 
			uint32_t min_nodes, uint32_t max_nodes, uint32_t req_nodes,
			uint32_t constraint_mode, uint32_t job_limit,
			uint32_t cpu_cap, uint32_t dram_cap, uint32_t frequency, 
			 float *time)
{
	if ( power_analyzer_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.estimate_job_time))
		(job_ptr, bitmap,  min_nodes, max_nodes, req_nodes,
		constraint_mode, job_limit, cpu_cap, dram_cap, frequency,
		time);
}

/**
 * Estimate a performance degradation under specified constraints
 * IN job_ptr - pointer to job being considered for estimation
 * IN/OUT bitmap - map of nodes being considered for estimation on input,
 *                map of nodes actually to be assigned on output
 * IN min_nodes - minimum number of nodes to allocate to job
 * IN max_nodes - maximum number of nodes to allocate to job
 * IN req_nodes - requested (or desired) count of nodes
 * IN constraint_mode - choose how to estimate performance degradation.
 *                      give flags which constraints are used.
 *                       PA_FLAG_CPU_CAP : use maximum cpu cap
 *                       PA_FLAG_DRAM_CAP : use maximum dram cap
 *                       PA_FLAG_FREQ : use maximum frequency 
 *                       PA_FLAG_JOB : use maximum job power limit
 * IN job_limit - maximum number of power to allocate to job
 * IN cpu_cap - maximum number of power to allocate to a cpu socket
 * IN dram_cap - maximum number of power to allocate to a memory channel
 *            currently sopported Intel RAPL
 * IN frequency - maximum number of frequency to a cpu
 * OUT degradation - estimated job performance degradation. Determined by under constraints
 * RET zero on success, EINVAL otherwise
 */
int power_analyzer_g_estimate_job_performance(struct job_record *job_ptr, bitstr_t *bitmap, 
			uint32_t min_nodes, uint32_t max_nodes, uint32_t req_nodes,
			uint32_t constraint_mode, uint32_t job_limit,
			uint32_t cpu_cap, uint32_t dram_cap, uint32_t frequency, 
			 float *degradation)
{
	if ( power_analyzer_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.estimate_job_performance))
		(job_ptr, bitmap,  min_nodes, max_nodes, req_nodes,
		constraint_mode, job_limit, cpu_cap, dram_cap, frequency,
		degradation);
}

/* *********************************************************************** */
/*  TAG(                      power_analyzer_g_analyze_finished_job                  )  */
/* *********************************************************************** */
/**
 * analyze power log and prepare for estimation
 */
int power_analyzer_g_analyze_finished_job(struct job_record *job_ptr)
{
	if ( power_analyzer_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.analyze_finished_job))(job_ptr);
}

/* *********************************************************************** */
/*  TAG(                      power_analyzer_g_analyze_interval                  )  */
/* *********************************************************************** */
/**
 * analyze power log and prepare for estimation
 */
int power_analyzer_g_analyze_interval(void)
{
	if ( power_analyzer_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.analyze_interval))();
}



