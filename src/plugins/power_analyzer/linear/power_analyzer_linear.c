/*****************************************************************************\
 *  power_analyzer_none.c - power analyzer plugin for none
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

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "slurm/slurm_errno.h"

#include "src/common/plugin.h"
#include "src/common/log.h"
#include "src/common/slurm_priority.h"
#include "src/common/macros.h"
#include "src/slurmctld/slurmctld.h"
#include "slurm/slurm.h"

#define MaxCPUPower 130
#define MinCpuPower 51
#define Number_of_Socket 2
#include "src/slurmctld/locks.h"

const char		plugin_name[]	= "SLURM Power Analyzer plugin";
const char		plugin_type[]	= "power_analyzer/linear";
const uint32_t		plugin_version	= SLURM_VERSION_NUMBER;

/* A plugin-global errno. */
static int plugin_errno = SLURM_SUCCESS;

static pthread_t poweranalyzer_thread = 0;
static pthread_mutex_t thread_flag_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *_get_analyzer_linear_loop(void);


static void stop_get_analyzer_linear_loop(void);

int init( void )
{
	pthread_attr_t attr;

	verbose( "power_analyzer: Power analyzer LINEAR plugin loaded" );
	debug ("Hello from plugin LINEAR init");

	slurm_mutex_lock( &thread_flag_mutex );
	if ( poweranalyzer_thread ) {
		debug2( "power analyzer thread already running, not starting another" );
		slurm_mutex_unlock( &thread_flag_mutex );
		return SLURM_ERROR;
	}		
	
	slurm_attr_init( &attr );
	/* since we do a join on this later we don't make it detached */
	if (pthread_create( &poweranalyzer_thread, &attr, _get_analyzer_linear_loop, NULL))
	error(" pthread_create");
	slurm_mutex_unlock( &thread_flag_mutex );
	slurm_attr_destroy( &attr );

	return SLURM_SUCCESS;
}

void fini( void )
{
	verbose( "Power analyzer plugin shutting down" );
	debug ("Goodbye from plugin fini");
	
	slurm_mutex_lock( &thread_flag_mutex );
	if ( poweranalyzer_thread ) {
		verbose( "Power analyzer plugin shutting down" );
		stop_get_analyzer_linear_loop();
		pthread_join( poweranalyzer_thread, NULL);
		 poweranalyzer_thread = 0;
	}
	slurm_pthread_mutex_unlock( &thread_flag_mutex );
}


void do_analysis_power_log(void)
{
	printf("do_analysis_power_log\n");	
}

static void *_get_analyzer_linear_loop(void){
	while(1){
		sleep(slurmctld_conf.power_analyzerinterval);
		debug3(" ************************");
		debug3(" Start of power_analyzer");
		do_analysis_power_log();
	}
}

/* Terminate power thread */
static void stop_get_analyzer_linear_loop(void)
{
	slurm_mutex_lock(&thread_flag_mutex );
//	stop_power = true;
//	slurm_cond_signal(&term_cond);
	slurm_mutex_unlock(&thread_flag_mutex );
}

int power_analyzer_p_reconfig( void )
{
	//backfill_reconfig();
	return SLURM_SUCCESS;
}

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
int power_analyzer_p_estimate_job_power(struct job_record *job_ptr, bitstr_t *bitmap, 
			uint32_t min_nodes, uint32_t max_nodes, uint32_t req_nodes,
			uint32_t constraint_mode,
			uint32_t cpu_cap, uint32_t dram_cap, uint32_t frequency, 
			uint32_t *job_power)
{
	//job_power is calculate according to formular
	//job_power = (cpu_cap + dram_cap) * job_ptr->number_of_node * number_of_socket (per node)
    
	struct node_record *node_ptr;
	power_current_data_t *powers;
		
	debug ("number of nodes is %d ", job_ptr->node_cnt);
	debug ("");	
	
	*job_power = ((cpu_cap + dram_cap) * job_ptr->node_cnt * Number_of_Socket);
	   
	   return SLURM_SUCCESS;
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
int power_analyzer_p_estimate_job_time(struct job_record *job_ptr, bitstr_t *bitmap, 
			uint32_t min_nodes, uint32_t max_nodes, uint32_t req_nodes,
			uint32_t constraint_mode, uint32_t job_limit,
			uint32_t cpu_cap, uint32_t dram_cap, uint32_t frequency, 
			 float *time)
{
	
	//calculate relative excution time của job, according to the fomular
    //time = (MaxCPUPower - MinCpuPower)/(cpu_cap - MinCpuPower);
			
	*time = (MaxCPUPower - MinCpuPower)/(cpu_cap - MinCpuPower);		
	return SLURM_SUCCESS;
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
int power_analyzer_p_estimate_job_performance(struct job_record *job_ptr, bitstr_t *bitmap, 
			uint32_t min_nodes, uint32_t max_nodes, uint32_t req_nodes,
			uint32_t constraint_mode, uint32_t job_limit,
			uint32_t cpu_cap, uint32_t dram_cap, uint32_t frequency, 
			 float *degradation)
{
	*degradation = 1.0 - (cpu_cap - MinCpuPower)/(MaxCPUPower - MinCpuPower);
	
	return SLURM_SUCCESS;
}

/**
 * Analyze power log and prepare for estimation.
 * When a job have finished, This function will be called with the job id.
 * This API is for analysing a finished job.
 */
int power_analyzer_p_analyze_finished_job(struct job_record *job_ptr)
{
	return SLURM_SUCCESS;
}

/**
 * Analyze power log and prepare for estimation.
 * This API will be called periodically. 
 * This API is for analysing all of data
 */
int power_analyzer_p_analyze_interval(void)
{
	while (1){
		sleep(slurmctld_conf.power_allocatorinterval);
		debug3(" ************************");
		debug3(" Start of power_analyzer");
		do_analysis_power_log();
	}	
	return SLURM_SUCCESS;
}

