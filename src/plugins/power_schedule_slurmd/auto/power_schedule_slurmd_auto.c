/*****************************************************************************\
 *  power_schedule_slurmd_auto.c - power schedule_slurmd plugin for auto
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


const char		plugin_name[]	= "SLURM Power schedule_slurmd AUTO plugin";
const char		plugin_type[]	= "power_schedule_slurmd/auto";
const uint32_t		plugin_version	= SLURM_VERSION_NUMBER;

/* A plugin-global errno. */
static int plugin_errno = SLURM_SUCCESS;

static pthread_t powerschedule_slurmd_thread = 0;
static pthread_mutex_t thread_flag_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *_get_schedule_slurmd_auto_loop(void);

static void stop_get_schedule_slurmd_auto_loop(void);


int power_schedule_slurmd_p_send_to_slurmd_autocap_request();


int init( void )
{
	pthread_attr_t attr;

	verbose( "power schedule: Power schedule_slurmd plugin AUTO  loaded" );
	debug ("Hello from plugin SCHEDULE init");

	slurm_mutex_lock( &thread_flag_mutex );
	if ( powerschedule_slurmd_thread ) {
		debug2( "power schedule slurmd thread already running, not starting another" );
		slurm_mutex_unlock( &thread_flag_mutex );
		return SLURM_ERROR;
	}
	slurm_attr_init( &attr );
	
	/* since we do a join on this later we don't make it detached */
	if (pthread_create( &powerschedule_slurmd_thread, &attr, _get_schedule_slurmd_auto_loop, NULL))
	error(" pthread_create");
	slurm_mutex_unlock( &thread_flag_mutex );
	slurm_attr_destroy( &attr );

	return SLURM_SUCCESS;
}

void fini( void )
{
	verbose( "Power schedule_slurmd plugin shutting down" );
	debug ("Goodbye from plugin fini");
	slurm_mutex_lock( &thread_flag_mutex );
	
	if ( powerschedule_slurmd_thread ) {
		verbose( "Power schedule auto plugin shutting down" );
		stop_get_schedule_slurmd_auto_loop();
		pthread_join( powerschedule_slurmd_thread, NULL);
		 powerschedule_slurmd_thread = 0;
	}
	slurm_pthread_mutex_unlock( &thread_flag_mutex );
	
	
}

static void *_get_schedule_slurmd_auto_loop(void){
	int i;
	while(1){
		power_schedule_slurmd_p_send_to_slurmd_autocap_request();
		sleep(10);
		//sleep (slurmctld_conf.power_allocatorinterval);
	}
}


/* Terminate power thread */
static void stop_get_schedule_slurmd_auto_loop(void)
{
	slurm_mutex_lock(&thread_flag_mutex );
//	stop_power = true;
//	slurm_cond_signal(&term_cond);
	slurm_mutex_unlock(&thread_flag_mutex );
}



int power_schedule_slurmd_p_reconfig( void )
{
	//backfill_reconfig();
	return SLURM_SUCCESS;
}

int power_schedule_slurmd_p_send_to_slurmd_autocap_request()
{
	int rc;
	int i;
	slurm_msg_t req_msg;
	slurm_msg_t resp_msg;
	char *host;
	char *this_addr;
	struct node_record *node_ptr;
	
	power_schedule_slurmd_req_msg_t req;
	
	uint32_t cluster_flags = slurmdb_setup_cluster_flags();

	slurm_msg_t_init(&req_msg);
	slurm_msg_t_init(&resp_msg);
	
	
	for(i=0, node_ptr = node_record_table_ptr;
		i<node_record_count; i++, node_ptr++){
		
		debug3(" PMON node name : %s",node_ptr->name);
		host = node_ptr->name;
	if (host)
		slurm_conf_get_addr(host, &req_msg.address);
	else if (cluster_flags & CLUSTER_FLAG_MULTSD) {
		if ((this_addr = getenv("SLURMD_NODENAME"))) {
			slurm_conf_get_addr(this_addr, &req_msg.address);
		} else {
			this_addr = "localhost";
			slurm_set_addr(&req_msg.address,
				       (uint16_t)slurm_get_slurmd_port(),
				       this_addr);
		}
	}
	else {
		char this_host[256];
		/*
		 *  Set request message address to slurmd on localhost
		 */
		gethostname_short(this_host, sizeof(this_host));
		this_addr = slurm_conf_get_nodeaddr(this_host);
		if (this_addr == NULL)
			this_addr = xstrdup("localhost");
		slurm_set_addr(&req_msg.address,
			       (uint16_t)slurm_get_slurmd_port(),
			       this_addr);
		xfree(this_addr);
	}

	req_msg.msg_type = REQUEST_POWER_SCHEDULE_SLURMD;
	req_msg.data     = &req;
    
    debug3("LINH: REQUEST_POWER_SCHEDULE_SLURMD: do slurm_send_recv_node_msg. *host = '%s'", host);
	rc = slurm_send_recv_node_msg(&req_msg, &resp_msg, 0);
	
	// Return signal does nothing, so it is chosen randomly as RESPONSE_POWER_KNOB_GET_INFO:
	
	if (rc != 0 || !resp_msg.auth_cred) {
		error("slurm_get_node_energy: %m");
		if (resp_msg.auth_cred)
			g_slurm_auth_destroy(resp_msg.auth_cred);
		return SLURM_ERROR;
	}
	if (resp_msg.auth_cred)
		g_slurm_auth_destroy(resp_msg.auth_cred);
	switch (resp_msg.msg_type) {
	case RESPONSE_POWER_KNOB_GET_INFO:
//		socket_cnt = ((power_knob_get_info_node_resp_msg_t *) 
//			resp_msg.data)->socket_cnt;
//		set_info = ((power_knob_get_info_node_resp_msg_t *) 
//			resp_msg.data)->power_info;
//		((power_knob_get_info_node_resp_msg_t *) resp_msg.data)->power_info = NULL;
//		slurm_free_power_knob_get_info_node_resp_msg(resp_msg.data);
		break;
	case RESPONSE_SLURM_RC:
	        rc = ((return_code_msg_t *) resp_msg.data)->return_code;
		slurm_free_return_code_msg(resp_msg.data);
		if (rc)
			slurm_seterrno_ret(rc);
		break;
	default:
		slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
		break;
	}
	
	//Finish of the RESPONSE
		}
	return SLURM_SUCCESS;
}
