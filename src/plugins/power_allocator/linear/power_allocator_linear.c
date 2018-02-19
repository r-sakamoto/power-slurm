/*****************************************************************************\
 *  power_allocator_linear.c - power alloctor plugin for linear
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

#define CPU_POWER 70 
#define Number_of_Socket 2
#include "src/slurmctld/locks.h"


const char		plugin_name[]	= "SLURM Power Allocator plugin";
const char		plugin_type[]	= "power_allocator/linear";
const uint32_t		plugin_version	= SLURM_VERSION_NUMBER;

/* A plugin-global errno. */
static int plugin_errno = SLURM_SUCCESS;

static pthread_t powerallocator_thread = 0;
static pthread_mutex_t thread_flag_mutex = PTHREAD_MUTEX_INITIALIZER;


static void *_get_allocator_linear_loop(void);

static void stop_get_allocator_linear_loop(void);

int power_allocator_p_do_power_safe(void);


int init( void )
{
	pthread_attr_t attr;

	verbose( "power_allocator: Power allocator LINEAR plugin loaded" );

	slurm_mutex_lock( &thread_flag_mutex );
	if ( powerallocator_thread ) {
		debug2( "power allocator thread already running, not starting another" );
		slurm_mutex_unlock( &thread_flag_mutex );
		return SLURM_ERROR;
	}	
	
	slurm_attr_init( &attr );
	/* since we do a join on this later we don't make it detached */
	if (pthread_create( &powerallocator_thread, &attr, _get_allocator_linear_loop, NULL))
	error(" pthread_create");
	slurm_mutex_unlock( &thread_flag_mutex );
	slurm_attr_destroy( &attr );

	return SLURM_SUCCESS;
}

void fini( void )
{
	verbose( "Power allocator LINEAR plugin shutting down" );
	slurm_mutex_lock( &thread_flag_mutex );
	if (  powerallocator_thread ) {
		verbose( "Power allocator plugin shutting down" );
		stop_get_allocator_linear_loop();
		pthread_join( powerallocator_thread, NULL);
		powerallocator_thread = 0;
	}
	slurm_pthread_mutex_unlock( &thread_flag_mutex );
	
}


int power_allocator_p_do_power_safe(){
	power_current_data_t *powers;
	uint16_t  socket_cnt;
	int i,j,k;
	uint32_t sum;
	sum = 0;
	
	uint32_t sum1;
	sum1 = 0;
	
	float percentage_dif;
	
	struct node_record *node_ptr;
	
    /* Locks: Read nodes */
    slurmctld_lock_t node_read_lock = {
        NO_LOCK, READ_LOCK, NO_LOCK, NO_LOCK };
    /* Locks: Write nodes */
    slurmctld_lock_t node_write_lock = {
        NO_LOCK, WRITE_LOCK, NO_LOCK, NO_LOCK };	
//	while (1){				
	for(i=0, node_ptr = node_record_table_ptr;
	    i<node_record_count; i++, node_ptr++){
		debug3(" PMON node name : %s",node_ptr->name);
		if (slurm_get_node_power(node_ptr->name, &socket_cnt, &powers)) {
			debug("_get_node_power_task: can't get info from slurmd(NODE : %s)", node_ptr->name);
		}else{
			for(k=0;k<socket_cnt;k++){	
					sum =  sum + (uint32_t)((node_ptr->power_info->current_power+k)->dram_current_watts + (node_ptr->power_info->current_power+k)->cpu_current_watts);
					debug(" power_consumption_value CPU is %d",(node_ptr->power_info->current_power+k)->cpu_current_watts);
					
					sum1 =  sum1 + powers[k].cpu_current_watts + powers[k].dram_current_watts;
					
					debug(" NOW power_consumption_value CPU is %d",powers[k].cpu_current_watts);
					
					
					debug(" NOW dram_consumption_value CPU is %d",powers[k].dram_current_watts);
			 }
			node_ptr->power_info->socket_cnt = socket_cnt;
			lock_slurmctld(node_write_lock);
			memcpy(node_ptr->power_info->current_power, powers, sizeof(power_current_data_t) * socket_cnt);			
			unlock_slurmctld(node_write_lock);

			xfree(powers);
			powers = NULL;			
	
		}
//		}			
	}
	
	debug("Power alert is %d", slurmctld_conf.power_alert);
	debug("Power usage is %d", sum1);
	
	
	if (sum1 >slurmctld_conf.power_alert){
		percentage_dif = ((float)sum1 /slurmctld_conf.power_alert)	*100;
		printf ("Alert, power is %f%% of power budget \n", percentage_dif);
	}
	else {	
		percentage_dif = ((float)sum1 /slurmctld_conf.power_alert)	*100;
		printf ("NO Alert, power is %f%% of power budget \n", percentage_dif);
	}
	return SLURM_SUCCESS;
}


static void *_get_allocator_linear_loop(void){
	while(1){
		power_allocator_p_do_power_safe();
		sleep(10);
		//sleep (slurmctld_conf.power_allocatorinterval);
	}
}

/* Terminate power thread */
static void stop_get_allocator_linear_loop(void)
{
	slurm_mutex_lock(&thread_flag_mutex );
//	stop_power = true;
//	slurm_cond_signal(&term_cond);
	slurm_mutex_unlock(&thread_flag_mutex );
}


int power_allocator_p_reconfig( void )
{
	//backfill_reconfig();
	return SLURM_SUCCESS;
}

int power_allocator_p_job_test_power(struct job_record *job_ptr, bitstr_t *bitmap, 
			uint32_t min_nodes, uint32_t max_nodes, uint32_t req_nodes,
			uint32_t constraint_mode,
			uint32_t cpu_cap, uint32_t dram_cap, uint32_t frequency, 
			uint32_t job_power)
{
	//slurmctld_conf.z_32: power_budget
	uint32_t power_of_nodes;
}
	
	
int power_allocator_p_job_test()
{

	// slurmctld_conf.z_32: power_budget
	uint32_t power_of_job;
	uint32_t power_available;
	
	debug("Inside power_allocator_p_job_test Power budget is %d", slurmctld_conf.z_32);
	ListIterator job_iterator;
	struct job_record *job_ptr = NULL;	

	job_iterator = list_iterator_create(job_list);	

	power_of_job = 0;
	
	while ((job_ptr = (struct job_record *) list_next(job_iterator))) {
		power_of_job = power_of_job + CPU_POWER * job_ptr->node_cnt * Number_of_Socket;
	}
	
	list_iterator_destroy(job_iterator);	
	
	power_available = slurmctld_conf.z_32 -  power_of_job;
	
	if (power_available > 0 )
		return 1;
	
	else return 0;	
}
	
	
int power_allocator_p_job_test_old(struct job_record *job_ptr)
{

	//slurmctld_conf.z_32: power_budget
	uint32_t power_of_job;
	uint32_t power_available;
	
	debug("Inside power_allocator_p_job_test Power budget is %d", slurmctld_conf.z_32);
	ListIterator job_iterator;
	//struct job_record *job_ptr = NULL;	

	job_iterator = list_iterator_create(job_list);	

	power_of_job = 0;
	
	while ((job_ptr = (struct job_record *) list_next(job_iterator))) {

		power_of_job = power_of_job + CPU_POWER * job_ptr->node_cnt * Number_of_Socket;
	}
	
	list_iterator_destroy(job_iterator);	
	
	power_available = slurmctld_conf.z_32 -  power_of_job;
	
	if (power_available > 0 )
		return 1;
	
	else return 0;	
	
	//Wrong: only one job
	//power_of_job = CPU_POWER * job_ptr->node_cnt * Number_of_Socket;
	//power_available = slurmctld_conf.z_32 -  power_of_job;
	//if (power_available > 0 )
	//		return 1;
	//	else return 0;
	
	}
	
int power_allocator_p_schedule(void)
{
	return SLURM_SUCCESS;
}

int power_allocator_p_alert(uint32_t current_power){
	return SLURM_SUCCESS;
}

int power_allocator_p_job_prolog(struct job_record *job){
	return SLURM_SUCCESS;
}

int power_allocator_p_job_resized(struct job_record *job ,struct node_record *node){
	return SLURM_SUCCESS;
}

int power_allocator_p_job_signal(struct job_record *job, int i){
	return SLURM_SUCCESS;
}

int power_allocator_p_job_fini(struct job_record *job){
	return SLURM_SUCCESS;
}

int power_allocator_p_job_suspend(struct job_record *job , bool i){
	return SLURM_SUCCESS;
}

int power_allocator_p_job_resume(struct job_record *job, bool i){
	return SLURM_SUCCESS;
}

