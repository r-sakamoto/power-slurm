/*****************************************************************************\
 *  power_allocator_dynamic.c - power alloctor plugin for dynamic
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

#include "slurm/slurm.h"

#include "slurm/slurm_errno.h"

#include "src/common/plugin.h"
#include "src/common/log.h"
#include "src/common/slurm_priority.h"
#include "src/common/macros.h"
#include "src/slurmctld/slurmctld.h"

#include "src/slurmctld/locks.h"
#define Number_of_Socket 2


const char		plugin_name[]	= "SLURM Power Allocator plugin";
const char		plugin_type[]	= "power_allocator/dynamic";
const uint32_t		plugin_version	= SLURM_VERSION_NUMBER;

/* A plugin-global errno. */
static int plugin_errno = SLURM_SUCCESS;

static pthread_t powerallocator_thread = 0;

static pthread_t powerallocator_thread1 = 0;

static pthread_mutex_t thread_flag_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *_get_allocator_dynamic_loop(void);

static void stop_get_allocator_dynamic_loop(void);

static void *_get_allocator_dynamic_loop1(void);

static void stop_get_allocator_dynamic_loop1(void);

static void slurm_set_power_cap2(int *power_cap_value[], char *node_name[], int number_of_node);

int init( void )
{
	pthread_attr_t attr;
	pthread_attr_t attr1;
	verbose( "power_allocator: Power allocator DYNAMIC plugin loaded" );

//	while (1){
//		sleep (30);
//		node_power_schedule();	
//	}	
	
	slurm_mutex_lock( &thread_flag_mutex );
	if ( powerallocator_thread ) {
		debug2( "power allocator thread already running, not starting another" );
		slurm_mutex_unlock( &thread_flag_mutex );
		return SLURM_ERROR;
	}

	slurm_attr_init( &attr );
	/* since we do a join on this later we don't make it detached */
	if (pthread_create( &powerallocator_thread, &attr, _get_allocator_dynamic_loop, NULL))
	error(" pthread_create");
	slurm_mutex_unlock( &thread_flag_mutex );
	slurm_attr_destroy( &attr );
	
	
	slurm_mutex_lock( &thread_flag_mutex );
	if ( powerallocator_thread1 ) {
		debug2( "power allocator thread1 already running, not starting another" );
		slurm_mutex_unlock( &thread_flag_mutex );
		return SLURM_ERROR;
	}

	slurm_attr_init( &attr1 );
	/* since we do a join on this later we don't make it detached */
	if (pthread_create( &powerallocator_thread1, &attr1, _get_allocator_dynamic_loop1, NULL))
	error(" pthread1_create");
	slurm_mutex_unlock( &thread_flag_mutex );
	slurm_attr_destroy( &attr1 );	

	return SLURM_SUCCESS;
}

void fini( void )
{
	slurm_mutex_lock( &thread_flag_mutex );
	if (  powerallocator_thread ) {
		verbose( "Power allocator plugin shutting down" );
		stop_get_allocator_dynamic_loop();
		pthread_join( powerallocator_thread, NULL);
		 powerallocator_thread = 0;
	}
	slurm_pthread_mutex_unlock( &thread_flag_mutex );

	slurm_mutex_lock( &thread_flag_mutex );
	if (  powerallocator_thread1 ) {
		verbose( "Power allocator plugin shutting down" );
		stop_get_allocator_dynamic_loop1();
		pthread_join( powerallocator_thread1, NULL);
		 powerallocator_thread1 = 0;
	}
	slurm_pthread_mutex_unlock( &thread_flag_mutex );
	
}

static void node_power_schedule(void)
{
	debug ("Dummy inside node_power_schedule");
	ListIterator job_iterator;
	struct job_record *job_ptr = NULL;	

	struct node_record *node_ptr;
	int i, j, k, count, first, last;
	power_current_data_t *powers;
	cache_ref_t *caches;
	int **power_consumption_value_pkg;
	int **power_consumption_value_dram;
	int **PMC0;
	int **PMC1;
	int sum_pkg, sum_dram, sum_PMC0, sum_PMC1;
	int number_of_node;	
	int counter;
	int counter1, counter2;
	uint16_t  socket_cnt, socket_number;	
	char *node_name[1024];	
	int *cap_values[2];
	float perf_function;
	
	float avg_pkg, avg_dram, avg_PMC0, avg_PMC1;
	
	job_iterator = list_iterator_create(job_list);

	debug ("Dummy inside node_power_schedule 2");
	
	while ((job_ptr = (struct job_record *) list_next(job_iterator))) {
		
		debug ("Dummy inside node_power_schedule 3");
		//Name of Nodes allocated for this job is job_ptr->nodes
		//Number of Nodes allocated for this job is job_ptr->node_cnt
		// bitstr_t *node_bitmap;		/* bitmap of nodes allocated to job */
	
		sum_pkg = 0;
		sum_dram = 0;
		sum_PMC0 = 0;
		sum_PMC1 = 0 ;

		count = 0;
		counter1= 0;
		counter2 = 0;
		
		if (job_ptr->node_bitmap == NULL) break;
		first = bit_ffs(job_ptr->node_bitmap);	
		last = bit_fls(job_ptr->node_bitmap);
		
		//Get node names for the job
		for (i = first; i <= last; i++) {
			if (bit_test(job_ptr->node_bitmap, i) == 1){
				node_name[count] = node_record_table_ptr[i].name;
				debug("BIT_SET NODE NAME IS %s", node_name[count]);
				count = count +1;	
			}
		}
		
		debug ("Dummy inside node_power_schedule 4");
		
		//Get number of nodes
		  number_of_node =  bit_set_count(job_ptr->node_bitmap);
		  debug ("Number of node %d", number_of_node);
		//or  number_of_node = job_ptr->node_cnt
			debug ("Dummy inside node_power_schedule 4.5");
		
		/* Locks: Read nodes */
        slurmctld_lock_t node_read_lock = {
            NO_LOCK, READ_LOCK, NO_LOCK, NO_LOCK };
        /* Locks: Write nodes */
        slurmctld_lock_t node_write_lock = {
            NO_LOCK, WRITE_LOCK, NO_LOCK, NO_LOCK };
							
		for(i=0, node_ptr = node_record_table_ptr;
	    i<node_record_count; i++, node_ptr++){
				debug ("Dummy inside node_power_schedule 4.6");		
			if (slurm_get_node_power(node_ptr->name, &socket_cnt, &powers)) {
				debug ("Dummy inside node_power_schedule 4.75");
				debug ("Dummy inside node_power_schedule 5");
				debug("_get_node_power_task: can't get info from slurmd(NODE : %s)", node_ptr->name);
			}else{				
				socket_number = socket_cnt;
				debug("socket_number", socket_number);				
				for (j=0; j < number_of_node; j++) {
					debug ("Dummy inside node_power_schedule 5.1");				 
					 if (strcmp(node_name[j], node_ptr->name) == 0){
						 counter1 = counter1 + 1;
						 debug ("Dummy inside node_power_schedule 6");
						 debug ("counter1 =  %d", counter1);
						 for(k=0;k<socket_cnt;k++){
							sum_pkg = sum_pkg + powers[k].cpu_current_watts;
							sum_dram = sum_dram + powers[k].dram_current_watts;
							debug ("sum_pkg =  %d", sum_pkg);
							debug ("sum_dram =  %d", sum_dram);
						 }				   
					 }
				 }		
				xfree(powers);
				powers = NULL;
				debug ("Dummy inside node_power_schedule 6.4");
				lock_slurmctld(node_write_lock);
				debug ("Dummy inside node_power_schedule 6.5");
				unlock_slurmctld(node_write_lock);	
				debug ("Dummy inside node_power_schedule 7");
			}	
		   		
		    if (slurm_get_cache(node_ptr->name, &socket_cnt, &caches)) {
				debug("_get_cache: can't get info from slurmd(NODE : %s)", node_ptr->name);
						 debug ("Dummy inside node_power_schedule 7");
						 debug ("counter2 =  %d", counter2);
			}else{				
				 for (j=0; j < number_of_node; j++) {

					 if (strcmp(node_name[j], node_ptr->name) == 0){
						counter2 = counter2 + 1;
						for(k=0;k<socket_cnt;k++){
							
							sum_PMC0 = sum_PMC0 +  caches[k].all_cache_ref;
							sum_PMC1 = sum_PMC1 +  caches[k].l1_miss;

							debug ("sum_PMC0 =  %d", sum_PMC0);
							debug ("sum_PMC1 =  %d", sum_PMC1);							
							//power_PMC0[j][k] = (int) caches[k].all_cache_ref;
							//power_PMC1[j][k] = (int) caches[k].l1_miss;						 
						}		 
						
					 }
				 }				 
					debug ("Dummy inside node_power_schedule 8");
				xfree(caches);
				caches = NULL;
				lock_slurmctld(node_write_lock);

				unlock_slurmctld(node_write_lock);	
			}
			debug ("Dummy inside node_power_schedule 9");
		}
		
		//All have same socket_number (normally 2)
		debug ("Dummy inside node_power_schedule 10");
		avg_pkg = sum_pkg / counter1 / socket_number;
		debug ("Avg_pkg is %f", avg_pkg );	
		avg_dram = sum_dram / counter1 / socket_number;
		debug ("Avg_dram is %f", avg_dram );	
		avg_PMC0 = sum_PMC0 / counter2/ socket_number;
		debug ("Avg_PMC0 is %f", avg_PMC0 );
		avg_PMC1 = sum_PMC1 / counter2/ socket_number;
		debug ("Avg_PMC1 is %f", avg_PMC1 );
		perf_function = 0.1 * avg_pkg + 0.3  * avg_dram + 0.5 * avg_PMC0 + 0.7 * avg_PMC1;
		
		debug("****************************");
		debug("****************************");
		debug("****************************");
		debug("%f", perf_function);
		debug("****************************");
		debug("****************************");
		debug("****************************");
		cap_values[0] = (int) perf_function;
		cap_values[1] = (int) perf_function;
	
		slurm_set_power_cap2(cap_values, node_name,number_of_node);

	}
	
    list_iterator_destroy(job_iterator);	
}	

static void *_get_allocator_dynamic_loop(void){
	while(1){
		node_power_schedule();
		//sleep(30);
		sleep (slurmctld_conf.power_allocatorinterval);
	}
}

/* Terminate power thread */
static void stop_get_allocator_dynamic_loop(void)
{
	slurm_mutex_lock(&thread_flag_mutex );
//	stop_power = true;
//	slurm_cond_signal(&term_cond);
	slurm_mutex_unlock(&thread_flag_mutex );
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
	percentage_dif = ((float)sum1 /slurmctld_conf.power_alert)	*100;
	
	printf ("NO Alert, power is %f%% of power budget \n", percentage_dif);
	
	return SLURM_SUCCESS;
}


static void *_get_allocator_dynamic_loop1(void){
	while(1){
		power_allocator_p_do_power_safe();
		sleep(10);
		//sleep (slurmctld_conf.power_allocatorinterval);
	}
}

/* Terminate power thread */
static void stop_get_allocator_dynamic_loop1(void)
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
	return SLURM_SUCCESS;
}


int power_allocator_p_job_test()
{

//	 slurmctld_conf.z_32: power_budget
	uint32_t power_of_job;
	uint32_t power_available;
	
	debug("Inside power_allocator_p_job_test Power budget is %d", slurmctld_conf.z_32);
//No need	ListIterator job_iterator;
//No need	struct job_record *job_ptr = NULL;	

//No need	job_iterator = list_iterator_create(job_list);	

//No need	power_of_job = 0;
	
//No need	while ((job_ptr = (struct job_record *) list_next(job_iterator))) {

//No need		power_of_job = power_of_job + CPU_POWER * job_ptr->node_cnt * Number_of_Socket;
//No need	}
	
//No need	list_iterator_destroy(job_iterator);	
	
//No need	power_available = slurmctld_conf.z_32 -  power_of_job;
	
//No need	if (power_available > 0 )
		return 1;
	
//No need	else return 0;	
}
	
	
int power_allocator_p_job_test_old(struct job_record *job_ptr)
{

//	 slurmctld_conf.z_32: power_budget
	uint32_t power_of_job;
	uint32_t power_available;
	
	debug("Inside power_allocator_p_job_test Power budget is %d", slurmctld_conf.z_32);
	ListIterator job_iterator;
	/*WRONG
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
	
	power_of_job = CPU_POWER * job_ptr->node_cnt * Number_of_Socket;
	power_available = slurmctld_conf.z_32 -  power_of_job;
	if (power_available > 0 )
	*/
	return 1;
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

static void slurm_set_power_cap2(int *power_cap_value[], char *node_name[], int number_of_node)
{
	debug(" LINH: SET POWER CAP");

	power_current_data_t *powers;
	power_capping_data_t *powers_cap;
	powers_cap = (power_capping_data_t*)malloc(sizeof(power_capping_data_t));

	uint16_t  socket_cnt;
	int i,j,k;
	struct node_record *node_ptr;

	/* Locks: Read nodes */
    slurmctld_lock_t node_read_lock = {
        NO_LOCK, READ_LOCK, NO_LOCK, NO_LOCK };
    /* Locks: Write nodes */
    slurmctld_lock_t node_write_lock = {
        NO_LOCK, WRITE_LOCK, NO_LOCK, NO_LOCK };	
					
	debug(" LINH: node_record_count: %d", node_record_count);		
	for(i=0, node_ptr = node_record_table_ptr;
	    i<node_record_count; i++, node_ptr++){
		debug3(" PMON node name : %s",node_ptr->name);
		if (slurm_get_node_power(node_ptr->name, &socket_cnt, &powers)) {
			debug("_get_node_power_task: can't get info from slurmd(NODE : %s)", node_ptr->name);
		}else{		
			//Check  whether node is called
			debug3(" Get socket_cnt : %d",socket_cnt);
			 for (j=0; j < number_of_node; j++) {
				 debug(" LINH: ___Set power cap node PTR %s", node_ptr->name);
				 debug(" LINH:  Set power cap node_name %s", node_name[j]);
				 if (strcmp(node_name[j], node_ptr->name) == 0){
					debug("Inside compare set power)");
					debug("Before assign set power");
					debug(" LINH: Before Set power cap node PTR %s", node_ptr->name);					
					if (slurm_set_node_power4(node_ptr->name, power_cap_value[0], power_cap_value[1] )) 
						debug("_get_node_power_task: can't get info from slurmd(NODE : %s)", node_ptr->name);							
						debug("After assign of set power");
				 }
			}
			
			debug("Socket is %d", socket_cnt);						
			lock_slurmctld(node_write_lock);	
			memcpy(node_ptr->power_info->power_cap, powers_cap, sizeof(power_capping_data_t) * socket_cnt);			
			unlock_slurmctld(node_write_lock);

			xfree(powers);
			free(powers_cap);
			powers = NULL;	
			powers_cap = NULL;
		}
	}
}