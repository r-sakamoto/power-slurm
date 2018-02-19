/*****************************************************************************\
 *  power_monitor.c - power monitor interface
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

#ifndef __POWERMONITOR_H
#define __POWERMONITOR_H

#define MAX_NODES = 1024*1024
#define MAX_SOCKET = 16

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "src/common/power.h"

#include "src/common/bitstring.h"
#include "src/common/macros.h"
#include "src/common/xstring.h"
#include "src/slurmctld/locks.h"
#include "slurm/slurm.h"
#include "src/slurmctld/slurmctld.h"


pthread_mutex_t power_monitor_mutex = PTHREAD_MUTEX_INITIALIZER;
bool power_monitor_enabled = false;

static void _do_power_monitor_work(FILE *fp);
static int _init_power_monitor_config(void);
static void *_init_power_monitor(void *arg);

static void slurm_get_power_consumption(double **power_consumption_value, char *node_name[], int number_of_node);
static void slurm_get_power_cap(double **power_cap_value, char *node_name[], int number_of_node);
static void slurm_set_power_cap(int **power_cap_value, char *node_name[], int number_of_node);

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
					
					powers_cap[0].cpu_cap_watts = (uint32_t)power_cap_value[j][0];
					powers_cap[1].cpu_cap_watts = (uint32_t)power_cap_value[j][1];
					
					debug("Before assign of set power");			
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

static void slurm_set_power_cap(int **power_cap_value, char *node_name[], int number_of_node)
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
					for(k=0;k<socket_cnt;k++){
						debug("Before assign set power");
						int test;
						test = (uint32_t)power_cap_value[j][k];
											
						debug("power_cap_value [%d][%d] is %f", j,k,power_cap_value[j][k]);
						debug("test is %d", test);
							
						debug("test is %d", powers[k].cpu_current_cap_watts);
						powers_cap[k].cpu_cap_watts = (uint32_t)power_cap_value[j][k];
						debug("After assign of set power");
					} 
				}
			}
			 
			node_ptr->power_info->socket_cnt = socket_cnt; 
						
			debug(" LINH: Before Set power cap node PTR %s", node_ptr->name);
			
			for(k=0;k<socket_cnt;k++){
				debug("Cap %d is %d", k, powers_cap[k].cpu_cap_watts);
			}
			
			debug("Socket is %d", socket_cnt);
	
			if (slurm_get_node_power3(node_ptr->name, powers_cap[1].cpu_cap_watts)) 
							
			debug("_get_node_power_task: can't get info from slurmd(NODE : %s)", node_ptr->name);						
			debug("After assign all");

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
static void slurm_get_power_cap(double **power_cap_value, char *node_name[], int number_of_node)
{
	debug(" LINH: START POWER CAP");

	power_current_data_t *powers;
	uint16_t  socket_cnt;
	int i,j,k;
	struct node_record *node_ptr;

    /* Locks: Read nodes */
    slurmctld_lock_t node_read_lock = {
        NO_LOCK, READ_LOCK, NO_LOCK, NO_LOCK };
    /* Locks: Write nodes */
    slurmctld_lock_t node_write_lock = {
        NO_LOCK, WRITE_LOCK, NO_LOCK, NO_LOCK };	
				
				
	for(i=0, node_ptr = node_record_table_ptr;
	    i<node_record_count; i++, node_ptr++){
		debug3(" PMON node name : %s",node_ptr->name);
		if (slurm_get_node_power(node_ptr->name, &socket_cnt, &powers)) {
			debug("_get_node_power_task: can't get info from slurmd(NODE : %s)", node_ptr->name);
		}else{
			//Check  whether node is called
			 for (j=0; j < number_of_node; j++) {
				 debug(" LINH: node PTR %s", node_ptr->name);
				 debug(" LINH:  node_name %s", node_name[j]);
				 if (strcmp(node_name[j], node_ptr->name) == 0){
					 debug("Inside compare)");
					for(k=0;k<socket_cnt;k++){
						
						debug("Before assign");
						power_cap_value[j][k] =  (double)(powers[k].cpu_current_cap_watts +  powers[k].dram_current_cap_watts);
						
						debug("power_cap_value [j][k] is %f", power_cap_value[j][k]);
						debug("After assign");
					} 
				 }
			}
			
			lock_slurmctld(node_write_lock);		
			unlock_slurmctld(node_write_lock);
			xfree(powers);
			powers = NULL;				
		}				
	}
}


static void slurm_get_power_consumption(double **power_consumption_value, char *node_name[], int number_of_node)
{
	debug(" LINH: START POWER CONSUMPTION");
	
	power_current_data_t *powers;
	uint16_t  socket_cnt;
	int i,j,k;
	struct node_record *node_ptr;

    /* Locks: Read nodes */
    slurmctld_lock_t node_read_lock = {
        NO_LOCK, READ_LOCK, NO_LOCK, NO_LOCK };
    /* Locks: Write nodes */
    slurmctld_lock_t node_write_lock = {
        NO_LOCK, WRITE_LOCK, NO_LOCK, NO_LOCK };	
				
	for(i=0, node_ptr = node_record_table_ptr;
	    i<node_record_count; i++, node_ptr++){
		debug3(" PMON node name : %s",node_ptr->name);
		if (slurm_get_node_power(node_ptr->name, &socket_cnt, &powers)) {
			debug("_get_node_power_task: can't get info from slurmd(NODE : %s)", node_ptr->name);
		}else{
			//Check  whether node is called
			 for (j=0; j < number_of_node; j++) {
				 debug(" LINH: node name %s", node_ptr->name);
				 debug(" LINH: NODE_PTR name %s", node_name[j]);
				 if (strcmp(node_name[j], node_ptr->name) == 0){
					 debug("Inside compare)");
					for(k=0;k<socket_cnt;k++){
						debug("Before assign");
			//Wrong:	power_consumption_value[j][k] =  (double)(node_ptr->power_info->current_power+k)->dram_current_watts + (node_ptr->power_info->current_power+k)->cpu_current_watts;
						power_consumption_value[j][k] =  (double)(powers[k].cpu_current_watts +  powers[k].dram_current_watts);
						
						debug("power_consumption_value [j][k] is %f", power_consumption_value[j][k]);
						debug("After assign");
					} 
				 }
			 }
			
			lock_slurmctld(node_write_lock);		
			//memcpy if need
			unlock_slurmctld(node_write_lock);

			xfree(powers);
			powers = NULL;			
		}
	}
}

static uint32_t sum_nodes_power()
{
	debug(" LINH: START POWER CONSUMPTION");
	power_current_data_t *powers;
	uint16_t  socket_cnt;
	int i,j,k;
	uint32_t sum;
	sum = 0;
	struct node_record *node_ptr;
	
    /* Locks: Read nodes */
    slurmctld_lock_t node_read_lock = {
        NO_LOCK, READ_LOCK, NO_LOCK, NO_LOCK };
    /* Locks: Write nodes */
    slurmctld_lock_t node_write_lock = {
        NO_LOCK, WRITE_LOCK, NO_LOCK, NO_LOCK };	
						
	for(i=0, node_ptr = node_record_table_ptr;
	    i<node_record_count; i++, node_ptr++){
		debug3(" PMON node name : %s",node_ptr->name);
		if (slurm_get_node_power(node_ptr->name, &socket_cnt, &powers)) {
			debug("_get_node_power_task: can't get info from slurmd(NODE : %s)", node_ptr->name);
		}else{
			debug(" LINH: node name %s", node_ptr->name);
			for(k=0;k<socket_cnt;k++){
				
				debug("Before assign");

	//Wrong:	sum =  sum + (uint32_t)((node_ptr->power_info->current_power+k)->dram_current_watts + (node_ptr->power_info->current_power+k)->cpu_current_watts);			
				sum = sum + powers[k].cpu_current_watts + powers[k].dram_current_watts;
								
				debug3("    PMON:cpu :%4d", powers[k].cpu_current_watts);
				debug3("    PMON:dram:%4d", powers[k].dram_current_watts);
				debug3("    PMON PREV cpu  : %4d", (node_ptr->power_info->current_power+k)->cpu_current_watts);
				debug3("    PMON PREV dram : %4d", (node_ptr->power_info->current_power+k)->dram_current_watts);
			}
			
			node_ptr->power_info->socket_cnt = socket_cnt;
			
			lock_slurmctld(node_write_lock);
			memcpy(node_ptr->power_info->current_power, powers, sizeof(power_current_data_t) * socket_cnt);	//if need		
			unlock_slurmctld(node_write_lock);

			xfree(powers);
			powers = NULL;			
		}
	}
	return sum;
}

static void _do_power_monitor_work(FILE *fp)
{
    debug3("LINH: _do_power_monitor_work");

	char filename[255];
	struct tm *timenow;
	power_current_data_t *powers;
	cache_ref_t *caches; // cao
	int i,j;
	uint16_t socket_cnt;
	uint16_t  cache_socket_cnt;
	struct node_record *node_ptr;

    /* Locks: Read nodes */
    slurmctld_lock_t node_read_lock = {
        NO_LOCK, READ_LOCK, NO_LOCK, NO_LOCK };
    /* Locks: Write nodes */
    slurmctld_lock_t node_write_lock = {
        NO_LOCK, WRITE_LOCK, NO_LOCK, NO_LOCK };
	 	 
	for(i=0, node_ptr = node_record_table_ptr;
	    i<node_record_count; i++, node_ptr++){
		debug3(" PMON node name : %s",node_ptr->name);
		if (slurm_get_node_power(node_ptr->name, &socket_cnt, &powers)) {
			debug("_get_node_power_task: can't get info from slurmd(NODE : %s)", node_ptr->name);
		}else{
			debug3(" PMON! socket_cnt %d",socket_cnt);
			debug3(" avr freq : %d",powers[0].cpu_current_frequency);
			for(j=0;j<socket_cnt;j++){
				debug3(" PMON: socket[%d]",j);
				debug3("    PMON:cpu :%4d", powers[j].cpu_current_watts);
				debug3("    PMON:dram:%4d", powers[j].dram_current_watts);
				debug3("    PMON PREV cpu  : %4d", (node_ptr->power_info->current_power+j)->cpu_current_watts);
				debug3("    PMON PREV dram : %4d", (node_ptr->power_info->current_power+j)->dram_current_watts);
				debug3("Power cap data is");
				debug3("    CPU cap: %4d", powers[j].cpu_current_cap_watts);
				debug3("    DRAM cap: %4d", powers[j].dram_current_cap_watts);
			    
				fprintf(fp, "node = %d, socket = %d, current_watt = %d, current_watt_limit = %d,  dram= %d, dramlimit =  %d "  , i,j,powers[j].cpu_current_watts,powers[j].cpu_current_cap_watts,powers[j].dram_current_watts,powers[j].dram_current_cap_watts);
			}
			node_ptr->power_info->socket_cnt = socket_cnt;

			lock_slurmctld(node_write_lock);
			memcpy(node_ptr->power_info->current_power, powers, sizeof(power_current_data_t) * socket_cnt);
			unlock_slurmctld(node_write_lock);

			xfree(powers);
			powers = NULL;
		}
		if (slurm_get_cache(node_ptr->name, &cache_socket_cnt, &caches)) {
				debug("_get_cache: can't get info from slurmd(NODE : %s)", node_ptr->name);
		}else{
			debug3(" Get caches! socket_cnt %d",cache_socket_cnt);
			for(j=0;j<cache_socket_cnt;j++){
				debug3(" PMON: socket[%d]",j);
				debug3(" Cache 1 :%4d", caches[j].all_cache_ref);
				debug3(" Cache 2 :%4d", caches[j].l1_miss);
				debug3(" Cache 3 :%4d", caches[j].l2_miss);
				debug3(" Cache 4 :%4d", caches[j].l3_miss);
			}
			node_ptr->power_info->socket_cnt = cache_socket_cnt; //line 1400 slurm.h
			
			lock_slurmctld(node_write_lock);
			memcpy(node_ptr->power_info->cache_reference, caches, sizeof(cache_ref_t) * cache_socket_cnt);
			unlock_slurmctld(node_write_lock);	
			
			xfree(caches);
			caches = NULL;
				
		}
	}
	fprintf(fp,"\n");
//	fclose(fp);
}

static int _init_power_monitor_config(void){
	return SLURM_SUCCESS;
}

static void *_init_power_monitor(void *arg){
	FILE *fp;
	char filename[255];
	struct tm *timenow;
	int i = 0;
	int j,k,l;
	//char node_name[1024][256];
	char *node_name[1024];	
	int number_of_node;
	int SOCKET_Number;
	number_of_node = 1;
	SOCKET_Number =2;
	int *cap_values[2];
	int sum;
//************************* TEST Power Information on the nodes   ***********************//	
	node_info_msg_t *slres;
	int node_num,i1;
	time_t update_time;
	
	int node_total_power = 0;
	int system_total_power = 0;
	power_current_data_t *p_val;
	
	node_info_t *node_info;
	power_data_t *p_info;
	
	slurm_load_node(update_time, &slres, 1);
	
	node_num = slres->record_count;
	
	for(i1=0; i1<node_num; i1++){
		debug("node name [%d] : %s\n",i1 ,slres->node_array[i1].name);
	}
	
	printf("b\n");
	node_num = slres->record_count;
	for(i1=0, p_info = (slres->node_array[i1].power_info); i1<node_num; i1++){
		printf("node name [%d] : %s\n",i1 ,slres->node_array[i1].name);
		printf("  socket_num %d\n", slres->node_array[i1].sockets);
		printf("  socket_num %d\n", slres->node_array[i1].power_info->socket_cnt);

		printf("  cpu w  %d\n", slres->node_array[i1].power_info->current_power[0].cpu_current_watts);
		printf("  dram w %d\n", slres->node_array[i1].power_info->current_power[0].dram_current_watts);
		printf("  cpu0  f %d\n", slres->node_array[i1].power_info->current_power[0].cpu_current_frequency);
		printf("  cpu1  f %d\n", slres->node_array[i1].power_info->current_power[1].cpu_current_frequency);
	}
	
	slurm_free_node_info_msg(slres);

//************************* Finish test  ***********************//	
	
	//for power_get_consumption:
	double **power_consumption_value = malloc(sizeof *power_consumption_value * number_of_node);
	if (power_consumption_value) {
		for (j = 0; j < number_of_node; j++){
			power_consumption_value[j] = malloc(sizeof *power_consumption_value[j] * SOCKET_Number);
			}
	}
	//For power_get_cap:
	double **power_cap_value = malloc(sizeof *power_cap_value * number_of_node);
	if (power_cap_value) {
		for (j = 0; j < number_of_node; j++){
			power_cap_value[j] = malloc(sizeof *power_cap_value[j] * SOCKET_Number);
			}
	}
	
	//Wrong: for power_set_cap:
//	int **power_cap_value3 = malloc(sizeof *power_cap_value3 * number_of_node);
//	if (power_cap_value3) {
//		for (j = 0; j < number_of_node; j++){
//			power_cap_value3[j] = malloc(sizeof *power_cap_value3[j] * SOCKET_Number);
//			}
//	}	


//	for (j=0; j < number_of_node; j++) {
//		for (k = 0; k < SOCKET_Number; k++){
//			power_cap_value3[j][k] = 11;
//		}
//	}	
		
	debug2 ("Currently, Power budget is %d", slurmctld_conf.z_32);
	_init_power_monitor_config();
	if (slurmctld_config.shutdown_time == 0){

		node_name[0] = "pompp00";
//		node_name[1] = "pompp01";
		debug(" SET _POWER__CAP");
		cap_values[0] = 17;
		cap_values[1] = 22;
		//Wrong:	slurm_set_power_cap(power_cap_value3, node_name, number_of_node);
		//OK:		slurm_set_power_cap2(cap_values, node_name, number_of_node);		
	
	}
	
	while(slurmctld_config.shutdown_time == 0){
		sleep(slurmctld_conf.power_monitorinterval);

		node_name[0] = "pompp00";
//		node_name[1] = "pompp01";
//		debug("POWER_CAP");
		cap_values[0] = 17;
		cap_values[1] = 22;
		
		debug("POWER_CONSUMPTION");
		debug("Node before function call %s",node_name[0]);
	//OK:	slurm_get_power_consumption(power_consumption_value, node_name, number_of_node);
	//OK:	slurm_get_power_cap(power_cap_value, node_name, number_of_node);		
		
	//Wrong:		slurm_set_power_cap(power_cap_value3, node_name, number_of_node);
	//OK:		slurm_set_power_cap2(cap_values, node_name, number_of_node);		
		
	//Check sum function()		sum = sum_nodes_power();

	//Check sum function()		debug ("Current power usage is %d",sum);
	//Check sum function()		debug ("Currently, SUM is %d",sum_nodes_power());
	//Check sum function()		debug ("Current power usage is %d",sum);
	//Check sum function()		debug ("Currently, SUM is %d",sum_nodes_power());
	//Check sum function()		debug ("Power Budget is %d",slurmctld_conf.z_32);
	
		for (j = 0; j<number_of_node; j++){
			for (k = 0; k <SOCKET_Number; k++){
				//Check power_get function: debug2("power_consumption_value: node[%d] socket[%d] is %f", j,k,power_consumption_value[j][k]);
			}
		}
		
		for (j = 0; j<number_of_node; j++){
			for (k = 0; k <SOCKET_Number; k++){
				//Check power_get function: debug2("power_cap_value: node[%d] socket[%d] is %f", j,k,power_cap_value[j][k]);
			}
		}
		if (i == 0) {
			debug3(" New time stamp");
			i = i +1;	
			time_t now = time(NULL);
			timenow = localtime(&now);		
			strftime(filename, sizeof(filename), "/var/log/slurm/power_monitor/%Y-%m-%d_%H:%M:%S.txt", timenow);
			fp = fopen(filename,"w");
			debug2(" !!!!File name %s",filename);
			if (fp == NULL){
				printf("I couldn't open results.dat for writing i == 0 .\n");
				exit(0);
			}	
			//Check power_monitor function:	_do_power_monitor_work(fp);
			fclose(fp);	
		}
		
		else if (i == 4){
			i = 0;		
			fp = fopen(filename,"a");
			if (fp == NULL){
				printf("I couldn't open results.dat for writing i == 4.\n");
				exit(0);
			}			
			//Check power_monitor function:	_do_power_monitor_work(fp);
			fclose(fp);	
		}
		else {
			i = i +1;
			fp = fopen(filename,"a");
			if (fp == NULL){
				printf("I couldn't open results.dat for writing i = i+1.\n");
				exit(0);
			}	
			//Check power_monitor function:	_do_power_monitor_work(fp);
			fclose(fp);
		}
	}
	free(power_consumption_value);
	free(power_cap_value);
	//free(power_cap_value3);
	
	return ;

}

extern void start_power_monitor(pthread_t *thread_id)
{
	pthread_attr_t thread_attr;

	slurm_mutex_lock(&power_monitor_mutex);
	if (power_monitor_enabled) {     /* Already running */
		slurm_mutex_unlock(&power_monitor_mutex);
		return;
	}
	power_monitor_enabled = true;
	slurm_mutex_unlock(&power_monitor_mutex);

	slurm_attr_init(&thread_attr);
	while (pthread_create(thread_id, &thread_attr, _init_power_monitor,
			      NULL)) {
		error("pthread_create %m");
		sleep(1);
	}
	slurm_attr_destroy(&thread_attr);
}

/*
extern power_monitor_data_update()

int _send_set_cap(
		bitstr_t *node_bitmap,
		node_record *node_record_table_ptr, 


)
int _send_set_free_cap()
int _send_set_free_freq()
int _send_get_current()
int _send_get_perf()


int _make_task_data()
int _make_node_list()

*/


/**
 * get_remote_nodes_power - update nodes power consumption
 * 	check current power consumption 
 * 	and update node_info->power_perf
 * IN node_bitmap - pointer to list of update nodes
 * IN/OUT node_record_table_ptr - pointer to node_info list
 * OUT total_power - total power consumption specified by the node_bitmap
 * IN synq_mode - SYNC_MODE_BLOCK     : wait until updating
 * 		  SYNC_MODE_NON_BLOCK 
 * 		     : no-wait. when finished updating, calls callback function.
 * IN callback - pointer to callback function (for non_blocking mode)
 * RET zero on success, EINVAL otherwise
 */
extern uint32_t get_remote_nodes_power(
		bitstr_t *node_bitmap,
		struct node_record *node_record_table_ptr, 
		uint32_t total_power, 
		uint32_t synq_mode, 
		void *callback)
{
	//int i;
	//int rc =  SLURM_ERROR;
/*
	if(synq_mode == SYNC_MODE_BLOCK){
		
<<<<<<< HEAD
	}else if(synq_mode == SYNC_MODE_NON_BLOCK){
		
	}else{
	}
//	for(i=0; i<node_record_count; i++){
//		if(bit_test(node_bitmap, i) == 1){
//			node_record_table_ptr[i].power_perf.xxi = get_power();
//		}
	}

	_make_task_data();
	_send_get_current();

	if(synq_mode == SYNC_MODE_BLOCK){	
	}else if(synq_mode == SYNC_MODE_NON_BLOCK){
	}

*/
	return SLURM_SUCCESS;
}



/**
 * get_remote_job_power - update nodes power consumption
 * 	check current power consumption 
 * 	update node_info->power_perf
 * IN job_ptr - pointer to job_record
 * IN/OUT node_record_table_ptr - pointer to node_info list
 * OUT total_power - total power consumption specified by the job_record
 * IN synq_mode - SYNC_MODE_BLOCK     : wait until updating
 * 		  SYNC_MODE_NON_BLOCK 
 * 		     : no-wait. when finished updating, calls callback function.
 * IN callback - pointer to callback function (for non_blocking mode)
 * RET zero on success, EINVAL otherwise
 */
extern uint32_t get_remote_job_power(
		struct job_record job_ptr,
		struct node_record *node_record_table_ptr,
		uint32_t total_power,
		uint32_t synq_mode, 
		void *callback);

/**
 * get_remote_nodes_frequency - update nodes frequency
 * 	check current frequency
 * 	update node_info->power_perf
 * IN node_bitmap - pointer to list of update nodes
 * IN/OUT node_record_table_ptr - pointer to node_info list
 * OUT total_power - total power consumption specified by the node_bitmap
 * IN synq_mode - SYNC_MODE_BLOCK     : wait until updating
 * 		  SYNC_MODE_NON_BLOCK 
 * 		     : no-wait. when finished updating, calls callback function.
 * IN callback - pointer to callback function (for non_blocking mode)
 * RET zero on success, EINVAL otherwise
 */
extern uint32_t get_remote_nodes_frequency(
		bitstr_t *node_bitmap,
		struct node_record *node_record_table_ptr, 
		uint32_t synq_mode, 
		void *callback);

/**
 * get_remote_job_frequency - update nodes frequency
 * 	check current frequency
 * 	update node_info->power_perf
 * IN job_ptr - pointer to job_record
 * IN/OUT node_record_table_ptr - pointer to node_info list
 * OUT total_power - total power consumption specified by the job_record
 * IN synq_mode - SYNC_MODE_BLOCK     : wait until updating
 * 		  SYNC_MODE_NON_BLOCK 
 * 		     : no-wait. when finished updating, calls callback function.
 * IN callback - pointer to callback function (for non_blocking mode)
 * RET zero on success, EINVAL otherwise
 */
extern uint32_t get_remote_job_frequency(
		struct job_record *job_ptr,  
		struct node_record *node_record_table_ptr, 
		uint32_t synq_mode,
		void *callback);

/**
 * set_remote_nodes_power - update nodes power cap
 * 	set power capping and update node_info->power_perf
 * IN node_bitmap - pointer to list of cap nodes
 * IN node_record_table_ptr - pointer to node_info list (containing power capping)
 * IN synq_mode - SYNC_MODE_BLOCK     : wait until updating
 * 		  SYNC_MODE_NON_BLOCK 
 * 		     : no-wait. when finished updating, calls callback function.
 * IN callback - pointer to callback function (for non_blocking mode)
 * RET zero on success, EINVAL otherwise
 */
extern uint32_t set_remote_nodes_power(
		bitstr_t *node_bitmap,
		struct node_record *node_record_table_ptr,
		uint32_t synq_mode,
		void *callback);

/**
 * set_remote_nodes_freqency - update nodes frequency cap
 * 	set frequency capping and update node_info->power_perf
 * IN node_bitmap - pointer to list of cap nodes
 * IN node_record_table_ptr - pointer to node_info list (containing frequency capping)
 * IN synq_mode - SYNC_MODE_BLOCK     : wait until updating
 * 		  SYNC_MODE_NON_BLOCK 
 * 		     : no-wait. when finished updating, calls callback function.
 * IN callback - pointer to callback function (for non_blocking mode)
 * RET zero on success, EINVAL otherwise
 */
extern uint32_t set_remote_nodes_freqency(
		bitstr_t *node_bitmap,
		struct node_record *node_record_table_ptr,
		uint32_t synq_mode, 
		void *callback);

/**
 * set_free_remote_nodes_power - remove power cap
 * 	remove power capping and update node_info->power_perf
 * IN node_bitmap - pointer to list of nodes
 * IN synq_mode - SYNC_MODE_BLOCK     : wait until updating
 * 		  SYNC_MODE_NON_BLOCK 
 * 		     : no-wait. when finished updating, calls callback function.
 * IN callback - pointer to callback function (for non_blocking mode)
 * RET zero on success, EINVAL otherwise
 */
extern uint32_t set_free_remote_nodes_power(
		bitstr_t *node_bitmap,
		uint32_t synq_mode,
		void *callback);

/**
 * set_free_remote_nodes_freqency - remove frequency cap
 * 	remove frequency capping and update node_info->power_perf
 * IN node_bitmap - pointer to list of nodes
 * IN synq_mode - SYNC_MODE_BLOCK     : wait until updating
 * 		  SYNC_MODE_NON_BLOCK 
 * 		     : no-wait. when finished updating, calls callback function.
 * IN callback - pointer to callback function (for non_blocking mode)
 * RET zero on success, EINVAL otherwise
 */
extern uint32_t set_free_remote_nodes_freqency(
		bitstr_t *node_bitmap,
		struct node_record *node_record_table_ptr,
		uint32_t synq_mode, 
		void *callback);

#endif /* !__POWERMONITOR_H */
