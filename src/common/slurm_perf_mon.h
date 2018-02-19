/*****************************************************************************\
 *  slurm_perf_mon.h - implementation-node performance monitor
 *  plugin definitions
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

#ifndef __SLURM_PERF_MON_H__
#define __SLURM_PERF_MON_H__

#if HAVE_CONFIG_H
#  include "config.h"
#  if HAVE_INTTYPES_H
#    include <inttypes.h>
#  else
#    if HAVE_STDINT_H
#      include <stdint.h>
#    endif
#  endif			/* HAVE_INTTYPES_H */
#else				/* !HAVE_CONFIG_H */
#  include <inttypes.h>
#endif				/*  HAVE_CONFIG_H */

#include <sys/resource.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "slurm/slurm.h"
#include "slurm/slurmdb.h"

#include "src/common/macros.h"
#include "src/common/pack.h"
#include "src/common/list.h"
#include "src/common/xmalloc.h"

#define MAX_SOCKET_NUM (4)
#define	MAXCORES	256

#define MSR_PKG_RAPL_POWER_LIMIT        0x610
#define MSR_PKG_ENERGY_STATUS           0x611
#define MSR_PKG_PERF_STATUS             0x613
#define MSR_PKG_POWER_INFO              0x614

/* DRAM RAPL Domain */
#define MSR_DRAM_POWER_LIMIT            0x618
#define MSR_DRAM_ENERGY_STATUS          0x619




/*
struct	node_info	{
	char	nname[NNAME_LENGTH];
	int	stat;
	double	pkg_limit[NUM_CPUS];
	double	pkg_watts[NUM_CPUS];
	double	pp0_limit[NUM_CPUS];
	double	pp0_watts[NUM_CPUS];
	double	dram_limit[NUM_CPUS];
	double	dram_watts[NUM_CPUS];
   double	pmc0[NUM_CPUS];
   double	pmc1[NUM_CPUS];
   double	pmc2[NUM_CPUS];
   double	pmc3[NUM_CPUS];
};
*/
/*
typedef struct perf_mon_data {
	time_t		sample_time;
	double	cpu_power[MAX_SOCKET_NUM];
	double	cpu_limit[MAX_SOCKET_NUM];
	double	dram_power[MAX_SOCKET_NUM];
	double	dram_limit[MAX_SOCKET_NUM];
	uint64_t	energy;

//	uint64_t	l2_misses;
//	uint64_t	l3_misses;
//	double	l2_missratio;
//	double	l3_missratio;
//	uint64_t	cpu_freq;
//	uint64_t	cap_cpu_freq;
   double	pmc0[MAX_SOCKET_NUM];
   double	pmc1[MAX_SOCKET_NUM];
   double	pmc2[MAX_SOCKET_NUM];
   double	pmc3[MAX_SOCKET_NUM];

} perf_mon_data_t;
*/

extern int perf_mon_init(void); /* load the plugin */
extern int perf_mon_fini(void); /* unload the plugin */

//??extern acct_gather_energy_t *acct_gather_energy_alloc(uint16_t cnt);

//extern void perf_mon_destroy(acct_gather_energy_t *energy);
//extern void perf_mon_pack(acct_gather_energy_t *energy, Buf buffer,
//				    uint16_t protocol_version);
//extern int perf_mon_unpack(acct_gather_energy_t **energy, Buf buffer,
//				     uint16_t protocol_version,
//				     bool need_alloc);

extern int perf_mon_g_update_node_energy(void);
extern int perf_mon_g_get_data(enum acct_energy_type data_type,
					 void *data);
extern int perf_mon_g_set_data(enum acct_energy_type data_type,
					 void *data);
extern int perf_mon_startpoll(uint32_t frequency);
extern void perf_mon_g_conf_options(s_p_options_t **full_options,
					      int *full_options_cnt);
extern void perf_mon_g_conf_set(s_p_hashtbl_t *tbl);

/* Get the values from the plugin that are setup in the .conf
 * file. This function should most likely only be called from
 * src/common/slurm_acct_gather.c (acct_gather_get_values())
 */
extern void perf_mon_g_conf_values(void *data);

#endif /*__SLURM_PERF_MON_H__*/
