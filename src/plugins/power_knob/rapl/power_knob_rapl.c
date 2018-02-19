/*****************************************************************************\
 *  power_knob_none.c - slurm power_knob plugin for rapl.
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

/*   power_knob_none
 * This plugin does not initiate a node-level thread.
 * It is the a power_knob stub.
 */

#include "src/common/slurm_xlator.h"
#include "src/common/slurm_jobacct_gather.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/slurm_protocol_defs.h"
#include "src/common/power_knob.h"
#include "src/common/fd.h"
#include "src/slurmd/common/proctrack.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#define _DEBUG 1
#define _DEBUG_ENERGY 1

#define	QNAME_LENGTH	16
#define	JNAME_LENGTH	16
#define	UNAME_LENGTH	16
#define	GNAME_LENGTH	16
#define	NNAME_LENGTH	16
#define	MAXNODES	1024

#define	MAXCORES	256
/* MSR number */
#define MSR_RAPL_POWER_UNIT		0x606

#define MSR_PKG_POWER_LIMIT		0x610
#define MSR_PKG_ENERGY_STATUS	0x611
#define MSR_PKG_PERF_STATUS		0x613
#define MSR_PKG_POWER_INFO		0x614

#define MSR_DRAM_POWER_LIMIT	0x618
#define MSR_DRAM_ENERGY_STATUS	0x619
#define MSR_DRAM_PERF_STATUS	0x61B
#define MSR_DRAM_POWER_INFO		0x61C

#define MSR_PP0_POWER_LIMIT		0x638
#define MSR_PP0_ENERGY_STATUS	0x639
#define MSR_PP0_POLICY		0x63A
#define MSR_PP0_PERF_STATUS		0x63B

#define MSR_PP1_POWER_LIMIT		0x640
#define MSR_PP1_ENERGY_STATUS	0x641
#define MSR_PP1_POLICY		0x642

#define DEFAULT_INTERVAL	 1000	// interval in milisecond 1000 = 1.00sec
#define MAX_PKGS MAX_SOCKET_NUMBER

pthread_mutex_t interval_monitor_mutex = PTHREAD_MUTEX_INITIALIZER;
bool interval_monitor_enabled = false;

struct	node_power_info {
	char	nname[NNAME_LENGTH];
	int	stat;
	double	pkg_limit[MAX_PKGS];
	double	pkg_watts[MAX_PKGS];
	double	pp0_limit[MAX_PKGS];
	double	pp0_watts[MAX_PKGS];
	double	dram_limit[MAX_PKGS];
	double	dram_watts[MAX_PKGS];
   double	pmc[MAX_PKGS][4];
};

power_current_data_t *local_power = NULL;
cache_ref_t *local_cache = NULL;

static double power_units[MAX_PKGS];
static double energy_units[MAX_PKGS];
static double time_units[MAX_PKGS];
static double thermal_design_power[MAX_PKGS];

double minimum_power_cap_value = 40; /* most of CPU allow cap value 
				      * less than specified value */
double maximum_power_cap_value = 100; /* most of CPU allow max cap value 
				       * more than thermal spec power */

static int measuring;		/* not measuring(0), measuring(1) */
static double power_measuring_pkg[MAX_PKGS];	/* pkg power measuring */
static double power_measuring_pp0[MAX_PKGS];	/* pp0 power measuring */
static double power_measuring_dram[MAX_PKGS];	/* dram power measuring */

static int interval_sec;	/* interval in sec */
static int interval_usec;	/* interval in usec */
static float _msec_;		/* interval of interrupt handler (milli second) */
static char _curr_time[32];	/* current time  */

static int num_dvfs_cores;	/* number of cores that enable dvfs */
static int num_all_cores;	/* number of cores in this node */
static int core_index_dvfs[MAXCORES];	/* core index for dvfs setting */

static double pkg_epoch[MAX_PKGS];	/* pw on each epoch (or each time interval) */
static double pp0_epoch[MAX_PKGS];
static double dram_epoch[MAX_PKGS];
static double pmc_epoch[MAX_PKGS][4];	/* average pmc each epoch (or each time interval) */

static char msr_socket_file_name[MAX_PKGS][32];	/* msr file name for rapl setting /dev/cpu/?/msr */
static char msr_core_file_name[MAXCORES][32];	/* msr file name for performance monitoring counter /dev/cpu/?/msr */

static uint32_t _start_en_val_[MAX_PKGS][3] = { {0} };	/* starting and previous value of msr (energy) */
static uint64_t _energy_val_[MAX_PKGS][3]   = { {0} };	/* store difference value of msr (energy) */
static uint64_t _start_pmc_val_[MAXCORES][4] = { {0} };	/* starting and previous value of msr (performance counter) */
static uint64_t _pmc_val_[MAXCORES][4]      = { {0} };	/* store difference value of msr (performance counter) */

/* one cpu in the package */
static int pkg2cpu[MAX_PKGS] = {[0 ... MAX_PKGS-1] = -1};
/* number of sockets on node */
static int nb_pkg = 0;

/*
 * These variables are required by the generic plugin interface.  If they
 * are not found in the plugin, the plugin loader will ignore it.
 *
 * plugin_name - a string giving a human-readable description of the
 * plugin.  There is no maximum length, but the symbol must refer to
 * a valid string.
 *
 * plugin_type - a string suggesting the type of the plugin or its
 * applicability to a particular form of data or method of data handling.
 * If the low-level plugin API is used, the contents of this string are
 * unimportant and may be anything.  SLURM uses the higher-level plugin
 * interface which requires this string to be of the form
 *
 *	<application>/<method>
 *
 * where <application> is a description of the intended application of
 * the plugin (e.g., "jobacct" for SLURM job completion logging) and <method>
 * is a description of how this plugin satisfies that application.  SLURM will
 * only load job completion logging plugins if the plugin_type string has a
 * prefix of "jobacct/".
 *
 * plugin_version - an unsigned 32-bit integer containing the Slurm version
 * (major.minor.micro combined into a single number).
 */
const char plugin_name[] = "PowerKnob RAPL plugin";
const char plugin_type[] = "power_knob/rapl";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

/***********************************************************************
  update time now
 ***********************************************************************/
char *_currtime ()
{
	time_t t;			/* time */
	struct tm *nowtm;		/* time for log */

	t = time (NULL);
	nowtm = localtime (&t);
	memset (_curr_time, 0, 32);
	sprintf (_curr_time, "%04d/%02d/%02d %02d:%02d:%02d",
			nowtm->tm_year + 1900,
			nowtm->tm_mon + 1, nowtm->tm_mday,
			nowtm->tm_hour, nowtm->tm_min, nowtm->tm_sec);
	return _curr_time;
}

/* _rdmsr(file_name, reg, *data) : read from file_name, to *data from offset = reg
 *  char *call_function just uses for debug
 */
static void _rdmsr (char *call_function, char *msr_file_name, uint32_t reg,
		uint64_t * data)
{
	int fd;
	if ((fd = open (msr_file_name, O_RDONLY)) < 0){
		exit (1);
	}
	/* pread, pwrite - read from or write to a file descriptor at a given offset */
	/* pread(int fd, void *buf, size_t count, off_t offset); */
	if (pread (fd, data, sizeof (*data), reg) != sizeof (*data)){
		exit (1);
	}
	close (fd);
}

/* _read_energy_val(int cpu_socket, uint32_t reg) : read filename[socket] from offset = reg 
 * read energy value from MSR__@@@_ENERGY_STATUS , and return it 
 */
static uint32_t _read_energy_val (int cpu_socket, uint32_t reg)
{
	uint32_t *p;
	uint64_t data;
	_rdmsr ("_read_energy_val", msr_socket_file_name[cpu_socket], reg, &data);
	p = (uint32_t *) (&data);
	return p[0];
}

/* _update_energy_val(uint32_t reg, int cpu_socket, int num): update value on [cpu_socket][num], msr offset = reg 
 */
static void _update_energy_val (uint32_t reg, int cpu_socket, int num)
{
	/* read energy value */
	uint32_t energy;
	energy = _read_energy_val (cpu_socket, reg);
	double val = 0;
	if (energy < _start_en_val_[cpu_socket][num]){				// overflow
		val = (double) (((~_start_en_val_[cpu_socket][num] + 1) +
					energy) * energy_units[cpu_socket] * 1000 / (_msec_));
		_energy_val_[cpu_socket][num] += (~_start_en_val_[cpu_socket][num] + 1) + energy;	// ~ == bitwise not
	}else{
		val = (double) ((energy - 
				_start_en_val_[cpu_socket][num]) *
				energy_units[cpu_socket] * 1000 / (_msec_));
		_energy_val_[cpu_socket][num] +=
			energy - _start_en_val_[cpu_socket][num];
	}

	/* update the power of this time interval */
	switch (reg){
		case MSR_PKG_ENERGY_STATUS:
			pkg_epoch[cpu_socket] = val;
			break;
		case MSR_PP0_ENERGY_STATUS:
			pp0_epoch[cpu_socket] = val;
			break;
		case MSR_DRAM_ENERGY_STATUS:
			dram_epoch[cpu_socket] = val;
			break;
	}
	_start_en_val_[cpu_socket][num] = energy;
	return;
}

/* set RAPL pkg power limit
 */
static void _set_pkg_power_limut (int cpu_socket, int pwLimit, int clamp)
{
	debug2("Set power limut");
	if (clamp < 0 || clamp > 1){
		exit (0);
	}
	debug2("Cpu socket is %d", cpu_socket);
	debug2("PW limit is %d", pwLimit);
	debug2("Clamp is %d", clamp);
	// read the MSR_PKG_POWER_LIMIT Register, , Intel 64 and IA-32 Architectures Software Developer's Manual, pp Vol. 3B 14-21
	uint64_t pw_limit_reg = 0;
	_rdmsr ("_set_pkg_power_limut", msr_socket_file_name[cpu_socket],
			MSR_PKG_POWER_LIMIT, &pw_limit_reg);

	// 64 bit = 16 f letters =	0xffffffffffffffff

	// 2. Set time window (bit 17 ~ 23)

	// add the time window = 1.0s (Y = 10 and Z = 0)
	// Y = 10: bit 17~21 = 01010
	// Z = 0: bit 22~23 = 00
	// we don't want to change bit 16, so it is 1
	// bit 16 ~ 23 = 00 + 01010 + 1 =	0001 0101 = 15
	uint64_t pw_limit_reg_upper;
	pw_limit_reg_upper = pw_limit_reg & 0xffffffffff01ffff;	// clear the old bit (17 ~ 23)
	pw_limit_reg_upper = pw_limit_reg_upper | 0x0000000000150000;	// set the new bit (22 ~ 23)

	// 3. Set clamp bit (bit 16)
	uint64_t pkg_clamping_limit_1_enable = 0x010000;	// bit 16 = 1
	uint64_t pkg_clamping_limit_1_disable = 0xfffffffffffeffff;	// bit 16 = 0

	if (clamp != 1)
		pw_limit_reg_upper = pw_limit_reg_upper & pkg_clamping_limit_1_disable;
	else
		pw_limit_reg_upper = pw_limit_reg_upper | pkg_clamping_limit_1_enable;

	// 4. set new power limit (bit 0~14)	 
	if (pwLimit < minimum_power_cap_value){
		pwLimit = minimum_power_cap_value;
	}
	if (pwLimit > maximum_power_cap_value){
		pwLimit = maximum_power_cap_value;
	}
	unsigned int raw_pkg_pw_limit_1_new =
		(unsigned int) (pwLimit / power_units[cpu_socket]);
	// 1. clear the old pkg power limit 1 (bit 14~0)
	// 8000 = 1000 0000 0000 0000 (bit 14 ~ 0) // bit 15 always = 1 (enable limit)
	pw_limit_reg_upper = pw_limit_reg_upper & 0xffffffffffff8000;	// clear the pkg power limit 1 (bit 14~0)
	// set the new power limit (bit 0~14)
	uint64_t pw_limit_reg_new = pw_limit_reg_upper | (raw_pkg_pw_limit_1_new);

	// write the new value to register
	int fd = open (msr_socket_file_name[cpu_socket], O_WRONLY);
	int bwrite =
		pwrite (fd, &pw_limit_reg_new, sizeof (pw_limit_reg_new),
				MSR_PKG_POWER_LIMIT);
	if (bwrite != sizeof (pw_limit_reg_new)){
		exit (1);
	}

	close (fd);
	return;
}

/* get dram power limit info
 */
double _get_dram_power_limit (int cpu_socket)
{
	// read the MSR_DRAM_POWER_LIMIT Register, , Intel 64 and IA-32 Architectures Software Developer's Manual, pp 14-38 Vol. 3B
	uint64_t pw_limit_reg = 0;
	_rdmsr ("_get_dram_power_limit", msr_socket_file_name[cpu_socket],
			MSR_DRAM_POWER_LIMIT, &pw_limit_reg);

	double dram_power_limit = power_units[cpu_socket] * (int) (pw_limit_reg & 0x7fff);	// bit 0~14	 
	return dram_power_limit;
}

/* get pp0 power limit info
 */
double _get_pp0_power_limit (int cpu_socket)
{
	// read the MSR_PP0_POWER_LIMIT Register, , Intel 64 and IA-32 Architectures Software Developer's Manual, pp 14-36 Vol. 3B
	uint64_t pw_limit_reg = 0;
	_rdmsr ("_get_pp0_power_limit", msr_socket_file_name[cpu_socket],
			MSR_PP0_POWER_LIMIT, &pw_limit_reg);

	double pp0_power_limit = power_units[cpu_socket] * (int) (pw_limit_reg & 0x7fff);	// bit 0~14	 
	return pp0_power_limit;
}

/* get dram pkg limit info
 */
double _get_pkg_power_limit (int cpu_socket)
{
	uint64_t pw_limit_reg = 0;
	_rdmsr ("_get_pkg_power_limit", msr_socket_file_name[cpu_socket],
			MSR_PKG_POWER_LIMIT, &pw_limit_reg);

	int raw_pkg_pw_limit_1 = (int) (pw_limit_reg & 0x7fff);
	double pkg_pw_limit_1 = power_units[cpu_socket] * raw_pkg_pw_limit_1;

	return pkg_pw_limit_1;

}

/* write to msr_file_name, *data from offset = reg 
 * used for set performance monitoring counter
 */
static void _wrmsr (int cpu_core, uint32_t reg, uint64_t data, char *strEv, char *strMa,
		char *strEventName)
{
	int fd;
	if ((fd = open (msr_core_file_name[cpu_core], O_WRONLY)) < 0){
		printf("ERROR in (fd=open(msr_file_name = '%s'): Event= '%s', Mask = '%s', EventName = '%s'\n",
				msr_core_file_name[cpu_core], strEv, strMa, strEventName);
		exit (1);
	}
	if (pwrite (fd, &data, sizeof (data), reg) != sizeof (data)){
		printf ("ERROR in pwrite: Event= '%s', Mask = '%s', EventName = '%s'\n",
				strEv, strMa, strEventName);
		printf ("  msr_file_name = %s\n", msr_core_file_name[cpu_core]);
		printf ("  reg           = 0x%03x\n", reg);
		printf ("  data          = 0x%04x\n", (uint32_t) data);
		exit (1);
	}
	close (fd);
}

/* read from msr_file_name, *data from offset = reg 
 * used for get performance monitoring counter
 * _wrpmc( event number, mask value, Register Address)
 * evnum = id, umnum = mask, reg = offset
 * for example _wrpmc( 0x2E, 0x4F, 0x186)
 */
static void _wrpmc (uint32_t evnum, uint32_t umnum, uint32_t reg, char *strEv, char *strMa,
		char *strEventName)
{
	uint64_t wdata;
	int i;
	// | == bitwise OR, << = shift left
	// 0x41 = 100-0001 = [INV (invert) flag (bit 23) = 1, USR (user mode) flag (bit 16) = 0]
	// mask value = bit 15~8, so it should be left shift 8 bit
	wdata = 0x410000 | (uint64_t) (umnum << 8) | (uint64_t) evnum;
	// _wrmsr() : write to msr_file_name, *data from offset = reg
	for (i = 0; i < num_all_cores; i++){
		_wrmsr (i, reg, wdata, strEv, strMa, strEventName);
	}
}

/* update pmc values
 * used for get performance monitoring counter
 */
static void _update_pmc_val (int core_id, int pcm_reg)
{
	uint32_t reg;
	uint64_t pmc;

	reg = 0xC1 + pcm_reg;		// offset

	/* _rdmsr(file_name, reg, *data) : read from file_name, to *data from offset = reg */
	_rdmsr ("pdate_pmc_val", msr_core_file_name[core_id], reg, &pmc);
	// the period is in ms, so result in sec = *1000/_msec_
	if (pmc < _start_pmc_val_[core_id][pcm_reg]){				// overflow
		_pmc_val_[core_id][pcm_reg] = (uint64_t) (((~_start_pmc_val_[core_id][pcm_reg] + 1) +
					pmc) * 1000 / _msec_);
	}else{
		_pmc_val_[core_id][pcm_reg] =(uint64_t) ((pmc -
					_start_pmc_val_[core_id][pcm_reg]) * 1000 / _msec_);
	}
	_start_pmc_val_[core_id][pcm_reg] = pmc;
}

int _get_host_name(char *buf, int n)
{
	FILE *pp;
	char *ret;
	int i;

	memset (buf, 0, n);

	pp = popen ("/bin/hostname", "r");
	if (pp == NULL){
		return -1;
	}

	ret = fgets (buf, n, pp);
	pclose (pp);
	if (ret == NULL){
		return -1;
	}

	for (i = 0; i < n && buf[i] != 0; i++){
		if (buf[i] < ' ')
		buf[i] = 0;
	}
	return 0;
}

/* get power usage and pmc values on each time interval
 * used for get performance monitoring counter
 */
static void _get_power_at_time_interval ()
{
    debug3("CAO: power_knob_rapl.c: _get_power_at_time_interval()");
	int i, k, l;
	//struct timeval tv;
	struct itimerval timer;
	time_t current_time;
	struct node_power_info node;

	static struct timeval prev_tv;
	struct timeval now_tv;
	gettimeofday(&now_tv,NULL);
	float _sample_msec;
	_sample_msec = (float)((1000000 * (now_tv.tv_sec - prev_tv.tv_sec)) + (now_tv.tv_usec - prev_tv.tv_usec)) / (1000);
	//fprintf (stderr, " #####################################\nSAMPLE  %lf\n",_sample_msec);
	prev_tv.tv_sec = now_tv.tv_sec;
	prev_tv.tv_usec = now_tv.tv_usec;

	_msec_ = _sample_msec;

	char my_host_name[64];
	_get_host_name(&my_host_name, sizeof(my_host_name));

	timer.it_value.tv_sec = interval_sec;
	timer.it_value.tv_usec = interval_usec;
	timer.it_interval.tv_sec = interval_sec;
	timer.it_interval.tv_usec = interval_usec;
	if (setitimer (ITIMER_REAL, &timer, NULL) < 0){
		perror ("setitimer");
	}

	/* update the power of each cpu socket now */
	for (i = 0; i < nb_pkg; i++){
		_update_energy_val (MSR_PKG_ENERGY_STATUS, i, 0);	// PKG
		_update_energy_val (MSR_PP0_ENERGY_STATUS, i, 1);	// PP0
		_update_energy_val (MSR_DRAM_ENERGY_STATUS, i, 2);// DRAM
	}

	//printf("%.2f %.2f %.2f \n", pkg_epoch[0], pp0_epoch[0], dram_epoch[0]);
	/* update the power measurement if any */
	if (measuring){
		for (i = 0; i < nb_pkg; i++){
			power_measuring_pkg[i] += pkg_epoch[i];
			power_measuring_pp0[i] += pp0_epoch[i];
			power_measuring_dram[i] += dram_epoch[i];
		}
	}

	/* get 4 pmc values of all cores */
	for (l = 0; l < num_all_cores; l++){
		for (k = 0; k < 4; k++){
			_update_pmc_val (l, k);
		}
	}

	/* take average of 4 pmcs on each cpu socket */

	int num_core_each_socket = num_all_cores / nb_pkg;
	for (i = 0; i < nb_pkg; i++){
		for (k = 0; k < 4; k++){
			pmc_epoch[i][k] = 0;
		}
		int core_start = num_core_each_socket * i;
		int core_end = num_core_each_socket * (i + 1) - 1;

		for (k = 0; k < 4; k++){
			for (l = core_start; l <= core_end; l++){
				pmc_epoch[i][k] += (double) _pmc_val_[l][k];
			}
			pmc_epoch[i][k] /= (double) num_core_each_socket;
		}
	}

	current_time = time((time_t *)0);
	//fprintf(stderr, "%s ",my_host_name);
	debug3( "host:%s ",my_host_name);

	memset(&node, 0, sizeof(struct node_power_info));
	
	if(local_power==NULL){
		return;
	}
	
	for (i = 0; i < nb_pkg; i++){
		// power limit now
		node.pkg_limit[i] = _get_pkg_power_limit (i);
		node.pp0_limit[i] = _get_pp0_power_limit (i);
		node.dram_limit[i] = _get_dram_power_limit (i);
		// power usage now	 
		node.pkg_watts[i] = pkg_epoch[i];
		node.pp0_watts[i] = pp0_epoch[i];
		node.dram_watts[i] = dram_epoch[i];

		local_power[i].cpu_current_watts = (uint32_t)pkg_epoch[i];
		local_power[i].dram_current_watts = (uint32_t)dram_epoch[i];
		
		local_power[i].cpu_current_cap_watts =  node.pkg_limit[i];
// 	 	local_power[i].cpu_current_cap_watts =  (uint32_t) 51.1234;
		local_power[i].dram_current_cap_watts =  node.dram_limit[i];
//		local_power[i].dram_current_cap_watts = (uint32_t) 45.33;	
		
		local_cache[i].all_cache_ref = (uint64_t)pmc_epoch[i][0];
		local_cache[i].l1_miss = (uint64_t)pmc_epoch[i][1];
		local_cache[i].l2_miss = (uint64_t)pmc_epoch[i][2];
		local_cache[i].l3_miss = (uint64_t)pmc_epoch[i][3];
		
		for (k = 0; k < 4; k++){
			node.pmc[i][k] = pmc_epoch[i][k];
	       	
		}
	}

	local_power[0].cpu_current_frequency = (uint32_t)_get_avr_cpufreq();	

	debug3("%ld %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f ", current_time,
			node.pkg_limit[0], node.pkg_watts[0], node.pp0_limit[0], node.pp0_watts[0], node.dram_limit[0], node.dram_watts[0], 
			node.pkg_limit[1], node.pkg_watts[1], node.pp0_limit[1], node.pp0_watts[1], node.dram_limit[1], node.dram_watts[1]);

	////sprintf(file_name,"%s%s.log",RAPL_DATA_ROOT,my_host_name);
	//sprintf(file_name,"/home/.slurm/log/%s.log",my_host_name);
	////fprintf(stderr, " file name %s ",file_name);

	//fp = fopen(file_name, "a");

	//fprintf(fp,"%ld %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f ", current_time,
	//		node.pkg_limit[0], node.pkg_watts[0], node.pp0_limit[0], node.pp0_watts[0], node.dram_limit[0], node.dram_watts[0], 
	//		node.pkg_limit[1], node.pkg_watts[1], node.pp0_limit[1], node.pp0_watts[1], node.dram_limit[1], node.dram_watts[1]);
	//fprintf(fp,"%6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %s\n", 
	//		node.pmc[0][0], node.pmc[0][1], node.pmc[0][2], node.pmc[0][3], 
	//		node.pmc[1][0], node.pmc[1][1], node.pmc[1][2], node.pmc[1][3]);
	//
	//fclose(fp);

	return;
}

/***********************************************************************
 set cpu freqency
 ***********************************************************************/
int _set_cpufreq (gov, mhz)		/* ret: 0(ok), -1(error) */
	char *gov;			/* I: scaling_governor */
	int mhz;			/* I: MHz, 0 */
{
	FILE *fp;
	char fname[BUFSIZ];
	int i;

	for (i = 0; i < num_dvfs_cores; i++){
		snprintf (fname, BUFSIZ,
				"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor",
				core_index_dvfs[i]);
		fp = fopen (fname, "w");
		if (fp == NULL)
			continue;
		fprintf (fp, "%s", gov);
		fclose (fp);
		if (mhz > 0){
			snprintf (fname, BUFSIZ,
					"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed",
					core_index_dvfs[i]);
			fp = fopen (fname, "w");
			if (fp == NULL)
				continue;
			fprintf (fp, "%d", mhz * 1000);
			fclose (fp);
		}
	}
	return 0;
}

/***********************************************************************
 get cpu frequency
 ***********************************************************************/
int _get_cpufreq (buf, n)		/* ret: 0(ok), -1(error) */
	char *buf;			/* O: strings */
	int n;			/* I: size of buf */
{
	FILE *fp;
	char fname[BUFSIZ];
	int i;
	int khz;
	int p;

	memset (buf, 0, n);
	p = 0;
	for (i = 0; i < num_dvfs_cores; i++){
		snprintf (fname, BUFSIZ,
				"/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_cur_freq",
				core_index_dvfs[i]);
		fp = fopen (fname, "r");
		if (fp == NULL)
			continue;
		fscanf (fp, "%d", &khz);
		fclose (fp);

		p += snprintf (&buf[p], BUFSIZ - p, "%d ", khz / 1000);
	}

	if (p > 0){
		buf[p - 1] = 0;
		return 0;
	}else{
		return -1;
	}
}

/***********************************************************************
 get average cpu frequency
 ***********************************************************************/
int _get_avr_cpufreq (void)		/* ret: 0(ok), -1(error) */
{
	FILE *fp;
	char fname[BUFSIZ];
	int i;
	int khz;
	int p;
	double avr_freq = 0;

	p = 0;
	for (i = 0; i < num_dvfs_cores; i++){
		snprintf (fname, BUFSIZ,
				"/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_cur_freq",
				core_index_dvfs[i]);
		fp = fopen (fname, "r");
		if (fp == NULL)
			continue;
		fscanf (fp, "%d", &khz);
		fclose (fp);

		debug3("core:%d, freq:%dkhz",i,khz);

		avr_freq += (khz / 1000);
	
	}

	avr_freq = avr_freq / num_dvfs_cores;

	debug3(" avr cpu freq : %d",(int) avr_freq);

	return (int)avr_freq;
}

/***********************************************************************
  send RAPL info
 ***********************************************************************/
void _send_rapl_info (f)
	int f;
{
	int i, k;
	struct node_power_info node;

	memset(&node, 0, sizeof(struct node_power_info));

	for (i = 0; i < nb_pkg; i++){
		// power limit now
		node.pkg_limit[i] = _get_pkg_power_limit (i);
		node.pp0_limit[i] = _get_pp0_power_limit (i);
		node.dram_limit[i] = _get_dram_power_limit (i);
		// power usage now	 
		node.pkg_watts[i] = pkg_epoch[i];
		node.pp0_watts[i] = pp0_epoch[i];
		node.dram_watts[i] = dram_epoch[i];
		for (k = 0; k < 4; k++){
			node.pmc[i][k] = pmc_epoch[i][k];
		}
	}

	/* send and receive should use the same data size 
	 * so here we use data for all MAX_PKGS
	 */
	send (f, &(node.pkg_limit[0]), sizeof (double) * MAX_PKGS * 10, 0);
	return;
}


////////////////////////////////////////////////////////////////////////
// check cpu num / socket num
////////////////////////////////////////////////////////////////////////
static void _hardware(void)
{
	char buf[1024];
	FILE *fd;
	int cpu = 0, pkg = 0;
	int i;
	int ret;
	char fname[BUFSIZ];		/* work */

	struct stat stat_p;		/* work */

	if ((fd = fopen("/proc/cpuinfo", "r")) == 0)
		fprintf (stderr, "RAPL: error on attempt to open /proc/cpuinfo");
	while (fgets(buf, 1024, fd)) {
		if (strncmp(buf, "processor", sizeof("processor") - 1) == 0) {
			sscanf(buf, "processor\t: %d", &cpu);
			//printf("proc %d\n",cpu);
			sprintf (msr_core_file_name[num_all_cores], "/dev/cpu/%d/msr", cpu);
			num_all_cores++;
			continue;
		}
		if (!strncmp(buf, "physical id", sizeof("physical id") - 1)) {
			sscanf(buf, "physical id\t: %d", &pkg);

			if (pkg > MAX_PKGS)
				fprintf (stderr, "Slurm can only handle %d sockets for "
						"rapl, you seem to have more than that.  "
						"Update src/plugins/perf_mon/"
						"rapl/perf_mon_rapl.h "
						"(MAX_PKGS) and recompile.", MAX_PKGS);
			if (pkg2cpu[pkg] == -1) {
				sprintf (msr_socket_file_name[nb_pkg], "/dev/cpu/%d/msr", cpu);
				nb_pkg++;
				pkg2cpu[pkg] = cpu;
			}
			continue;
		}
	}
	fclose(fd);

	/* number of cores that enable cpufreq */
	num_dvfs_cores = 0;
	for (i = 0; i < num_all_cores; i++){
		snprintf (fname, BUFSIZ,
				"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
		ret = stat (fname, &stat_p);
		if (ret == 0){
			core_index_dvfs[num_dvfs_cores] = i;
			num_dvfs_cores++;
		}
	}


}

int _init_rapl()
{
	int i;
	for (i = 0; i < nb_pkg; i++){
		//Calculate the units used
		uint64_t pw_unit_reg = 0;
		uint64_t pw_info_reg = 0;
		_rdmsr ("main", msr_socket_file_name[i], MSR_RAPL_POWER_UNIT,
				&pw_unit_reg);
		_rdmsr ("main", msr_socket_file_name[i], MSR_PKG_POWER_INFO,
				&pw_info_reg);

		power_units[i] = pow (0.5, (double) (pw_unit_reg & 0xf));
		energy_units[i] = pow (0.5, (double) ((pw_unit_reg >> 8) & 0x1f));
		time_units[i] = pow (0.5, (double) ((pw_unit_reg >> 16) & 0xf));
		thermal_design_power[i] =
			power_units[i] * (double) (pw_info_reg & 0x7fff);

		_start_en_val_[i][0] = _read_energy_val (i, MSR_PKG_ENERGY_STATUS);	// PKG
		_start_en_val_[i][1] = _read_energy_val (i, MSR_PP0_ENERGY_STATUS);	// PP0
		_start_en_val_[i][2] = _read_energy_val (i, MSR_DRAM_ENERGY_STATUS);	// DRAM	 

#ifdef CODE_DEBUG
		fprintf (stderr, "	Power Units = %.3fW\n", power_units[i]);
		fprintf (stderr, "	Energy Units = %.8fJ\n", energy_units[i]);
		fprintf (stderr, "	Time Units = %.8fs\n", time_units[i]);
		fprintf (stderr, "	Package thermal spec: %.3fW\n",
				thermal_design_power[i]);
#endif

		/* min and max power of the package */
		double minimum_power_value = 0;
		double maximum_power_value = 0;

		minimum_power_value =
			power_units[i] * (double) ((pw_info_reg >> 16) & 0x7fff);

		maximum_power_value =
			power_units[i] * (double) ((pw_info_reg >> 32) & 0x7fff);

		/* use thermal_spec_power as max power cap value */
		maximum_power_cap_value =
			power_units[i] * (double) (pw_info_reg & 0x7fff);

#ifdef CODE_DEBUG
		double time_window =
			time_units[i] * (double) ((pw_info_reg >> 48) & 0x7fff);

		fprintf (stderr, "	Package minimum power: %.3fW\n", minimum_power_value);
		fprintf (stderr, "	Package maximum power: %.3fW\n", maximum_power_value);
		fprintf (stderr, "	Package max power cap: %.3fW\n", maximum_power_cap_value);
		fprintf (stderr, "	Package maximum time window: %.3fs\n", time_window);
#endif
	}
	return 0;
}

int _init_pmc()
{
	int i,k;

	_wrpmc (0x2E, 0x4F, 0x186, "0x2E", "0x4F", "PMC1");	// 2EH 4FH LONGEST_LAT_CACHE.REFERENCE This event counts requests originating from the core that reference a cache line in the last level cache.
	_wrpmc (0x2E, 0x41, 0x187, "0x2E", "0x41", "PMC2");	// 2EH 41H LONGEST_LAT_CACHE.MISS This event counts each cache miss condition for references to the last level cache.
	_wrpmc (0x24, 0x30, 0x188, "0x24", "0x30", "PMC3");	// 24H 30H L2_RQSTS.ALL_CODE_RD Counts all L2 code requests.
	_wrpmc (0x24, 0x80, 0x189, "0x24", "0x80", "PMC4");	// 24H 80H L2_RQSTS.PF_MISS Counts all L2 HW prefetcher requests that missed L2.

	/* get pmc started values */
	for (i = 0; i < num_all_cores; i++){
		for (k = 0; k < 4; k++){
			_update_pmc_val (i, k);
		}
	}
	return 0;
}

static void *_get_power_loop(void *arg){
	while(1){
		_get_power_at_time_interval();
		sleep(1);
	}
}

int _set_interval()
{
	pthread_attr_t thread_attr;
	pthread_t thread_id;

	slurm_mutex_lock(&interval_monitor_mutex);
	if(interval_monitor_enabled){
		slurm_mutex_unlock(&interval_monitor_mutex);
		return SLURM_SUCCESS;
	}

	interval_monitor_enabled = true;
	slurm_mutex_unlock(&interval_monitor_mutex);

	while(pthread_create(&thread_id, NULL, _get_power_loop, NULL)){
		error(" pthread_create");
		sleep(1);
	}

}

/*
 * init() is called when the plugin is loaded, before any other functions
 * are called.  Put global initialization here.
 */
extern int init(void)
{
	debug("%s loaded", plugin_name);
	debug("power_knob/rapl plugin");
    debug("CAO: power_knob/rapl plugin INIT");
	return SLURM_SUCCESS;
}

extern int fini(void)
{
	power_knob_current_destroy(local_power);
	power_knob_cache_destroy(local_cache);
	local_power = NULL;
	local_cache = NULL;
	return SLURM_SUCCESS;
}

extern int power_knob_p_get_data(enum power_knob_type data_type,
					 void *data)
{
	debug("Test power_knob_p_get_data run");
	int rc = SLURM_SUCCESS;
	power_current_data_t *power = (power_current_data_t *)data;
	uint16_t *socket_cnt = (uint16_t *)data;

	switch (data_type) {
	case POWER_KNOB_DATA_NODE_POWER:
		//_get_power_at_time_interval ();
		memcpy(power, local_power, sizeof(power_current_data_t) * nb_pkg );
		break;
	case POWER_KNOB_DATA_SOCKET_CNT:
		*socket_cnt = nb_pkg;
		break;
	default:
		error("power_knob_p_get_data: unknown enum %d",
		      data_type);
		rc = SLURM_ERROR;
		break;
	}
	return rc;

}

extern int power_knob_p_get_cache_data(enum cache_type data_type, void *cache_data)
{
	int rc = SLURM_SUCCESS;
	cache_ref_t *cache = (cache_ref_t *) cache_data;
	uint16_t *cache_socket_cnt = (uint16_t *)cache_data;
	
	switch(data_type){
	case CACHE_POWER_KNOB_DATA_NODE_POWER:
		memcpy(cache, local_cache, sizeof(cache_ref_t) * nb_pkg );
		break;
	case CACHE_POWER_KNOB_DATA_SOCKET_CNT:
		*cache_socket_cnt = nb_pkg;
		break;
		
	default:
		error("power_knob_p_get_cache_data: unknown enum %d",
		      data_type);
		rc = SLURM_ERROR;
		break;
	}
	return SLURM_SUCCESS;
}

//extern int power_knob_p_set_data(power_capping_data_t *cap_data)
extern int power_knob_p_set_data(power_knob_cap_req_msg_t *cap_msg)
{
	//define local val
	int i;
	//power_knob_set_req_msg_t;

	debug3("       power_knob_p_set_data 2");
		debug3("   before assign");
//	debug3("              socket_cnt    [%d] ",cap_msg->socket_cnt);
	debug3("              pkg cap 1 val [%d]", cap_msg->cap_info);
	debug3("              pkg cap 2 val [%d]", cap_msg->cap_info2);
	debug3("   before limit");
	//debug3("       power_knob_p_set_data");

	//check cap_data
		

	//for pkg
		//update capping value based on eneable_cap_mode;
		// _set_pkg_power_limut
		// _set_cpufreq
		//
		
		//_set_pkg_power_limut (int cpu_socket, double pwLimit, int clamp)
		
//	_set_pkg_power_limut (cap_msg->socket_cnt, cap_msg->cap_info[cap_msg->socket_cnt].cpu_cap_watts, 1);
	
	_set_pkg_power_limut (0, cap_msg->cap_info, 1);
	_set_pkg_power_limut (1, cap_msg->cap_info2, 1);
	//for (i = 0; i < nb_pkg; i++){	
	//	local_power_set = set_pkg_power_limut (,);
	
	
	//}
	
	return SLURM_SUCCESS;
}

extern void power_knob_p_conf_set(void){
	debug("power_knob_p_conf_set start");

	_hardware();
	_init_rapl();
	_init_pmc();;
	local_power = power_knob_current_alloc(nb_pkg);
	local_cache = power_knob_cache_alloc(nb_pkg);
	_get_power_at_time_interval ();
	_set_interval();

	debug("power_knob_p_conf_set done");
	return;	
}

