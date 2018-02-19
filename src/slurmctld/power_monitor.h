/*****************************************************************************\
 *  power_monitor.h - power monitor interface
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

#include "slurm/slurm.h"
#include "src/slurmctld/slurmctld.h"

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
		node_record *node_record_table_ptr, 
		uint32_t total_power, 
		uint32_t synq_mode, 
		void *callback);

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
		node_record *node_record_table_ptr,
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
		node_record *node_record_table_ptr, 
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
		node_record *node_record_table_ptr, 
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
		node_record *node_record_table_ptr,
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
		node_record *node_record_table_ptr,
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
		bitstr_t *node_bitmap
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
		node_record *node_record_table_ptr,
		uint32_t synq_mode, 
		void *callback);

#endif /* !__POWERMONITOR_H */
