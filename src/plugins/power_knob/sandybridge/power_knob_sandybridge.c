/*****************************************************************************\
 *  perf_mon_sandybridge.c - performance monitor plugin for intel sandybridge.
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
 *
 *  This file is patterned after jobcomp_linux.c, written by Morris Jette and
 *  Copyright (C) 2002 The Regents of the University of California.
\*****************************************************************************/

/*   perf_mon_rapl
 * This plugin does not initiate a node-level thread.
 * It will be used to load energy values from cpu/core
 * sensors when harware/drivers are available
 */

1.mod data_type
 add intel monitor secific value
  or 
 mod Intel monitor v2.11
2.add read cache perfmon
3.add add rapl set function

// 残り
// 共有データの宣言，コピー，マロックについて確認
// 実際にプラグインをロードするための記述
// reqから来る部分の修正


#include <fcntl.h>
#include <signal.h>
#include "src/common/slurm_xlator.h"
#include "src/common/slurm_perf_mon.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/slurm_protocol_defs.h"
#include "src/common/fd.h"
#include "src/slurmd/common/proctrack.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <math.h>

#include <time.h>
#include <sys/time.h>
#include <string.h>
#include "struct.h"


/* 変数の長さ */
#define	QNAME_LENGTH	16
#define	JNAME_LENGTH	16
#define	UNAME_LENGTH	16
#define	GNAME_LENGTH	16

//defined "src/common/slurm_perf_mon.h"
//#define	MAX_SOCKET_NUM	2
//#define	MAXCORES	256

/* MSR number */
#define MSR_RAPL_POWER_UNIT		0x606

#define MSR_PKG_POWER_LIMIT		0x610
#define MSR_PKG_ENERGY_STATUS		0x611
#define MSR_PKG_PERF_STATUS		0x613
#define MSR_PKG_POWER_INFO		0x614

#define MSR_DRAM_POWER_LIMIT		0x618
#define MSR_DRAM_ENERGY_STATUS		0x619
#define MSR_DRAM_PERF_STATUS		0x61B
#define MSR_DRAM_POWER_INFO		0x61C

#define MSR_PP0_POWER_LIMIT		0x638
#define MSR_PP0_ENERGY_STATUS		0x639
#define MSR_PP0_POLICY			0x63A
#define MSR_PP0_PERF_STATUS		0x63B

#define MSR_PP1_POWER_LIMIT		0x640
#define MSR_PP1_ENERGY_STATUS		0x641
#define MSR_PP1_POLICY			0x642

/* rapld function number */
#define	REQ_SET_PKG_SOCKET0_RAPL	1
#define	REQ_SET_PKG_SOCKET1_RAPL	2
#define	REQ_SET_DRAM_RAPL	3
#define	REQ_CLEAR_RAPL		4

#define	REQ_START_MEASURE	11
#define	REQ_STOP_MEASURE	12
#define	REQ_EXIT		13
#define	REQ_RAPL_INFO		21
#define	REQ_SET_INTERVAL	22
#define REQ_FREQ_ONDEMAND	31
#define REQ_FREQ_CONSERVATIVE	32
#define REQ_FREQ_POWERSAVE	33
#define REQ_FREQ_PERFORMANCE	34
#define REQ_FREQ_USERSPACE	35


/* 定数 */
#define	CPUFREQ_LOWER_LIMIT	800	/* MHz */
#define	CPUFREQ_UPPER_LIMIT	5000	/* MHz */

#define	 DEFAULT_INTERVAL	 1500	// interval in milisecond 1000 = 1.00sec

#define MAX_PKGS        4
#define	MAXCORES	256

static double power_units[MAX_PKGS];
static double energy_units[MAX_PKGS];
static double time_units[MAX_PKGS];
static double thermal_design_power[MAX_PKGS];

static double minimum_power_value;
static double maximum_power_value;

static int measuring;		/* not measuring(0), measuring(1) */
static double power_measuring_pkg[MAX_PKGS];	/* pkg power measuring */
static double power_measuring_pp0[MAX_PKGS];	/* pp0 power measuring */
static double power_measuring_dram[MAX_PKGS];	/* dram power measuring */

static int interval_sec;	/* interval in sec */
static int interval_usec;	/* interval in usec */
static float _msec_;		/* interval of interrupt handler (milli second) */
static char _curr_time[32];	/* currtime() */

static int num_dvfs_cores;	/* number of cores that enable dvfs */
static int num_all_cores;	/* number of cores in this node */
static int core_index_dvfs[MAXCORES];	/* core index for dvfs setting */

static double pkg_epoch[MAX_PKGS];	/* pw on each epoch (or each time interval) */
static double pp0_epoch[MAX_PKGS];
static double dram_epoch[MAX_PKGS];
static double pmc_epoch[MAX_PKGS][4];	/* average pmc each epoch (or each time interval) */

static char msr_socket_file_name[MAX_PKGS][32];	/* msr file name for rapl setting /dev/cpu/?/msr */
static char msr_core_file_name[MAXCORES][32];	/* msr file name for performance monitoring counter /dev/cpu/?/msr */

static uint32_t _start_en_val_[MAX_PKGS][3] = { 0 };	/* starting and previous value of msr (energy) */
static uint64_t _energy_val_[MAX_PKGS][3] = { 0 };	/* store difference value of msr (energy) */
static uint64_t _start_pmc_val_[MAXCORES][4] = { 0 };	/* starting and previous value of msr (performance counter) */
static uint64_t _pmc_val_[MAXCORES][4] = { 0 };	/* store difference value of msr (performance counter) */


/* one cpu in the package */
static int pkg2cpu[MAX_PKGS] = {[0 ... MAX_PKGS-1] = -1};
static int pkg_fd[MAX_PKGS] = {[0 ... MAX_PKGS-1] = -1};
static int nb_pkg = 0;

static void rdmsr (char *call_function, char *msr_file_name, uint32_t reg,
			 uint64_t * data);
static uint32_t read_energy_val (int cpu_socket, uint32_t reg);
static void update_energy_val (uint32_t reg, int cpu_socket, int num);
static void set_pkg_power_limit (int cpu_socket, double pwLimit, int clamp);
double get_dram_power_limit (int cpu_socket);
double get_pp0_power_limit (int cpu_socket);
double get_pkg_power_limit (int cpu_socket);
static void wrmsr (int cpu_core, uint32_t reg, uint64_t data, char *strEv,
			 char *strMa, char *strEventName);
static void wrpmc (uint32_t evnum, uint32_t umnum, uint32_t reg, char *strEv,
			 char *strMa, char *strEventName);
static void update_pmc_val (int core_id, int pcm_reg);
static void get_power_at_time_interval ();

/* From Linux sys/types.h */
#if defined(__FreeBSD__)
typedef unsigned long int	ulong;
#endif

union {
	uint64_t val;
	struct {
		uint32_t low;
		uint32_t high;
	} i;
} package_energy[MAX_PKGS], dram_energy[MAX_PKGS];

#define _DEBUG 1
#define _DEBUG_ENERGY 1

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
const char plugin_name[] = "AcctGatherEnergy RAPL plugin";
const char plugin_type[] = "perf_mon/rapl";
const uint32_t plugin_version = SLURM_VERSION_NUMBER;

static perf_mon_t *local_energy = NULL;
static uint64_t debug_flags = 0;

static int dataset_id = -1; /* id of the dataset for profile data */

/* one cpu in the package */
static char hostname[MAXHOSTNAMELEN];

static void
rdmsr (char *call_function, char *msr_file_name, uint32_t reg,
			 uint64_t * data){
	int fd;
	if ((fd = open (msr_file_name, O_RDONLY)) < 0){
		debug ("%s: rdmsr file open error at '%s'\n", call_function,
				 msr_file_name);
		//exit (1);
	}
	/* pread, pwrite - read from or write to a file descriptor at a given offset */
	/* pread(int fd, void *buf, size_t count, off_t offset); */
	if (pread (fd, data, sizeof (*data), reg) != sizeof (*data)){
		debug ("msr_file_name = %s\n", msr_file_name);
		debug ("reg           = 0x%03x\n", reg);
		debug ("data          = 0x%04x\n", (uint32_t) (*data));
		//exit (1);
	}
	close (fd);
}

/* read_energy_val(int cpu_socket, uint32_t reg) : read filename[socket] from offset = reg 
/* read energy value from MSR__@@@_ENERGY_STATUS , and return it 
 * added by CAO 20160615 
 */
static uint32_t
read_energy_val (int cpu_socket, uint32_t reg)
{
	uint32_t *p;
	uint64_t data;
	rdmsr ("read_energy_val", msr_socket_file_name[cpu_socket], reg, &data);
	p = (uint32_t *) (&data);
	return p[0];
}

/* update_energy_val(uint32_t reg, int cpu_socket, int num): update value on [cpu_socket][num], msr offset = reg 
 *	added by CAO 20160615 
 */
static void
update_energy_val (uint32_t reg, int cpu_socket, int num)
{
	/* read energy value */
	uint32_t energy;
	energy = read_energy_val (cpu_socket, reg);
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
 * added by CAO 20160617 
 */
static void
set_pkg_power_limit (int cpu_socket, double pwLimit, int clamp)
{
	debug ("\n==================\n");
	debug ("Set Rapl Power Limit on %s\n",
		 msr_socket_file_name[cpu_socket]);
	if (clamp < 0 || clamp > 1){

		debug ("clampbit should be 0 or 1\n");
		//exit (0);
	}

	// read the MSR_PKG_POWER_LIMIT Register, , Intel 64 and IA-32 Architectures Software Developer's Manual, pp Vol. 3B 14-21
	uint64_t pw_limit_reg = 0;
	rdmsr ("set_pkg_power_limit", msr_socket_file_name[cpu_socket],
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
	if (pwLimit < minimum_power_value){
		pwLimit = minimum_power_value;
		debug ("pwLimit(%f) < PW_CAP_MIN(%i), set the power cap as PW_CAP_MIN\n",
				 pwLimit, minimum_power_value);
	}
	if (pwLimit > maximum_power_value){
		pwLimit = thermal_design_power[0];

		debug ("pwLimit(%f) > PW_CAP_MAX(%i), set the power cap as PW_CAP_MAX\n",
				 pwLimit, maximum_power_value);
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
	if (fd < 0){
		debug ("open('%s', O_WRONLY) has error\n",
				 msr_socket_file_name[cpu_socket]);
	}
	int bwrite =
		pwrite (fd, &pw_limit_reg_new, sizeof (pw_limit_reg_new),
			MSR_PKG_POWER_LIMIT);
	if (bwrite != sizeof (pw_limit_reg_new)){
		debug ("pw_limit_reg_new = %X\n", pw_limit_reg_new);
		debug ("size of pw_limit_reg_new = %i\n",
			 sizeof (pw_limit_reg_new));
		debug ("ERROR in pwrite at msr_file_name = %s\n",
			 msr_socket_file_name[cpu_socket]);
		debug ("bwrite = %i, sizeof(pw_limit_reg_new) = %i\n", bwrite,
			 sizeof (pw_limit_reg_new));
		//exit (1);
	}

	close (fd);
	return;
}

/* get dram power limit info
 * added by CAO 20160617 
 */
double
get_dram_power_limit (int cpu_socket){
	// read the MSR_DRAM_POWER_LIMIT Register, , Intel 64 and IA-32 Architectures Software Developer's Manual, pp 14-38 Vol. 3B
	uint64_t pw_limit_reg = 0;
	rdmsr ("get_dram_power_limit", msr_socket_file_name[cpu_socket],
	 MSR_DRAM_POWER_LIMIT, &pw_limit_reg);

	double dram_power_limit = power_units[cpu_socket] * (int) (pw_limit_reg & 0x7fff);	// bit 0~14	 
	debug ("\n==================\n");
	debug ("DRAM Information on %s\n",
		 msr_socket_file_name[cpu_socket]);

	debug ("pw_limit_reg = %X\n", pw_limit_reg);
	debug ("Power units	= %.3f W\n", power_units[cpu_socket]);
	debug ("Energy units = %.8f J\n", energy_units[cpu_socket]);
	debug ("Time units	 = %.8f s\n", time_units[cpu_socket]);
	debug ("\n");

	// Show dram power limit info
	int enable_bit = (pw_limit_reg >> 15) & 0x1;	// bit 15
	int clamp_bit = (pw_limit_reg >> 16) & 0x1;	// bit 16

	debug ("power_limit		 = %f\n", dram_power_limit);
	debug ("enable_bit			= %i\n", enable_bit);
	debug ("clamp_bit			 = %i\n", clamp_bit);

	return dram_power_limit;
}

/* get pp0 power limit info
 * added by CAO 20160617 
 */
double
get_pp0_power_limit (int cpu_socket){

	// read the MSR_PP0_POWER_LIMIT Register, , Intel 64 and IA-32 Architectures Software Developer's Manual, pp 14-36 Vol. 3B
	uint64_t pw_limit_reg = 0;
	rdmsr ("get_pp0_power_limit", msr_socket_file_name[cpu_socket],
	 MSR_PP0_POWER_LIMIT, &pw_limit_reg);

	double pp0_power_limit = power_units[cpu_socket] * (int) (pw_limit_reg & 0x7fff);	// bit 0~14	 
	debug ("\n==================\n");
	debug ("PP0 Information on %s\n",
		 msr_socket_file_name[cpu_socket]);
	debug ("pw_limit_reg = %X\n", pw_limit_reg);
	debug ("Power units	= %.3f W\n", power_units[cpu_socket]);
	debug ("Energy units = %.8f J\n", energy_units[cpu_socket]);
	debug ("Time units	 = %.8f s\n", time_units[cpu_socket]);
	debug ("\n");

	// Show PP0 power limit info
	int enable_bit = (pw_limit_reg >> 15) & 0x1;	// bit 15
	int clamp_bit = (pw_limit_reg >> 16) & 0x1;	// bit 16

	debug ("power_limit		 = %f\n", pp0_power_limit);
	debug ("enable_bit			= %i\n", enable_bit);
	debug ("clamp_bit			 = %i\n", clamp_bit);

	return pp0_power_limit;
}

/* get dram pkg limit info
 * added by CAO 20160617 
 */
double
get_pkg_power_limit (int cpu_socket){

	uint64_t pw_limit_reg = 0;
	rdmsr ("get_pkg_power_limit", msr_socket_file_name[cpu_socket],
	 MSR_PKG_POWER_LIMIT, &pw_limit_reg);

	int raw_pkg_pw_limit_1 = (int) (pw_limit_reg & 0x7fff);
	double pkg_pw_limit_1 = power_units[cpu_socket] * raw_pkg_pw_limit_1;

	debug ("PKG Information on %s\n",
		 msr_socket_file_name[cpu_socket]);
	debug ("Power units	= %.3f W\n", power_units[cpu_socket]);
	debug ("Energy units = %.8f J\n", energy_units[cpu_socket]);
	debug ("Time units	 = %.8f s\n", time_units[cpu_socket]);
	debug ("\n");

	// Show package power info
	uint64_t pw_info_reg = 0;
	rdmsr ("get_pkg_power_limit", msr_socket_file_name[cpu_socket],
	 MSR_PKG_POWER_INFO, &pw_info_reg);
	double thermal_spec_power =
		power_units[cpu_socket] * (double) (pw_info_reg & 0x7fff);
	double minimum_power =
		power_units[cpu_socket] * (double) ((pw_info_reg >> 16) & 0x7fff);
	double maximum_power =
		power_units[cpu_socket] * (double) ((pw_info_reg >> 32) & 0x7fff);
	double time_window =
		time_units[cpu_socket] * (double) ((pw_info_reg >> 48) & 0x7fff);

	debug ("Package thermal spec :	 %.3fW\n", thermal_spec_power);
	debug ("Package minimum power:	 %.3fW\n", minimum_power);
	debug ("Package maximum power:	 %.3fW\n", maximum_power);
	debug ("Package max time window: %.3fs\n", time_window);
	debug ("\n");

	// real all value of the MSR_PKG_POWER_LIMIT
	int pkg_enable_limit_1 = (int) ((pw_limit_reg >> 15) & 0x1);
	int pkg_clamping_limit_1 = (int) ((pw_limit_reg >> 16) & 0x1);
	unsigned int Y1 = (unsigned int) ((pw_limit_reg >> 17) & 0x1f);
	unsigned int Z1 = (unsigned int) ((pw_limit_reg >> 22) & 0x3);
	double pkg_time_window_limit_1 =
		time_units[cpu_socket] * pow (2, Y1) * (1 + Z1 / 4.0);

	int raw_pkg_pw_limit_2 = (int) ((pw_limit_reg >> 32) & 0x7fff);	// bit 32~46 = 15 bit
	int pkg_enable_limit_2 = (int) ((pw_limit_reg >> 47) & 0x1);
	int pkg_clamping_limit_2 = (int) ((pw_limit_reg >> 48) & 0x1);
	unsigned int Y2 = (unsigned int) ((pw_limit_reg >> 49) & 0x1f);
	unsigned int Z2 = (unsigned int) ((pw_limit_reg >> 54) & 0x3);
	double pkg_time_window_limit_2 =
		time_units[cpu_socket] * pow (2, Y2) * (1 + Z2 / 4.0);


	double pkg_pw_limit_2 = power_units[cpu_socket] * raw_pkg_pw_limit_2;

	debug ("pkg_pw_limit_1:					%.2f W\n", pkg_pw_limit_1);
	debug ("pkg_enable_limit_1:			%i(1 = enable)\n",
		 pkg_enable_limit_1);
	debug ("pkg_clamping_limit_1:		%i(0 = disable)\n",
		 pkg_clamping_limit_1);
	debug ("pkg_time_window_limit_1: %fs\n", pkg_time_window_limit_1);

	debug ("pkg_pw_limit_2:					%.2f W\n", pkg_pw_limit_2);
	debug ("pkg_enable_limit_2:			%i\n", pkg_enable_limit_2);
	debug ("pkg_clamping_limit_2:		%i\n", pkg_clamping_limit_2);
	debug ("pkg_time_window_limit_2: %fs\n", pkg_time_window_limit_2);

	return pkg_pw_limit_1;

}

/* write to msr_file_name, *data from offset = reg 
 * used for set performance monitoring counter
 * added by CAO 20160617 
 */
static void
wrmsr (int cpu_core, uint32_t reg, uint64_t data, char *strEv, char *strMa,
			 char *strEventName){
	int fd;
	if ((fd = open (msr_core_file_name[cpu_core], O_WRONLY)) < 0){
		debug("ERROR in (fd=open(msr_file_name = '%s'): Event= '%s', Mask = '%s', EventName = '%s'\n",
	 		msr_core_file_name[cpu_core], strEv, strMa, strEventName);
		exit (1);
	}
	if (pwrite (fd, &data, sizeof (data), reg) != sizeof (data)){
		debug ("ERROR in pwrite: Event= '%s', Mask = '%s', EventName = '%s'\n",
			strEv, strMa, strEventName);
		debug ("  msr_file_name = %s\n", msr_core_file_name[cpu_core]);
		debug ("  reg           = 0x%03x\n", reg);
		debug ("  data          = 0x%04x\n", (uint32_t) data);
		exit (1);
	}
	close (fd);
}

/* read from msr_file_name, *data from offset = reg 
 * used for get performance monitoring counter
 * added by CAO 20160617 
 * wrpmc( event number, mask value, Register Address)
 * evnum = id, umnum = mask, reg = offset
 * for example wrpmc( 0x2E, 0x4F, 0x186)
 */
static void
wrpmc (uint32_t evnum, uint32_t umnum, uint32_t reg, char *strEv, char *strMa,
			 char *strEventName)
{
	uint64_t wdata;
	int i;
	// | == bitwise OR, << = shift left
	// 0x41 = 100-0001 = [INV (invert) flag (bit 23) = 1, USR (user mode) flag (bit 16) = 0]
	// mask value = bit 15~8, so it should be left shift 8 bit
	wdata = 0x410000 | (uint64_t) (umnum << 8) | (uint64_t) evnum;

	// wrmsr() : write to msr_file_name, *data from offset = reg

	for (i = 0; i < num_all_cores; i++){
		wrmsr (i, reg, wdata, strEv, strMa, strEventName);
	}
}

/* update pmc values
 * used for get performance monitoring counter
 * added by CAO 20160617 
 */
static void
update_pmc_val (int core_id, int pcm_reg)
{
	uint32_t reg;
	uint64_t pmc;

	reg = 0xC1 + pcm_reg;		// offset

	/* rdmsr(file_name, reg, *data) : read from file_name, to *data from offset = reg */
	rdmsr ("pdate_pmc_val", msr_core_file_name[core_id], reg, &pmc);
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

/* get power usage and pmc values on each time interval
 * used for get performance monitoring counter
 * added by CAO 20160615 
 */
static void
get_power_at_time_interval ()
{
	int i, k, l;
	struct timeval tv;
	struct itimerval timer;

	timer.it_value.tv_sec = interval_sec;
	timer.it_value.tv_usec = interval_usec;
	timer.it_interval.tv_sec = interval_sec;
	timer.it_interval.tv_usec = interval_usec;
	if (setitimer (ITIMER_REAL, &timer, NULL) < 0){
		perror ("setitimer");
	}

	/* update the power of each cpu socket now */
	for (i = 0; i < nb_pkg; i++){
		update_energy_val (MSR_PKG_ENERGY_STATUS, i, 0);	// PKG
		update_energy_val (MSR_PP0_ENERGY_STATUS, i, 1);	// PP0
		update_energy_val (MSR_DRAM_ENERGY_STATUS, i, 2);	// DRAM
	}
	
	//debug("%.2f %.2f %.2f \n", pkg_epoch[0], pp0_epoch[0], dram_epoch[0]);
	/* update the power measurement if any */
	if (measuring){
		for (i = 0; i < nb_pkg; i++){
			power_measuring_pkg[i] += pkg_epoch[i];
			power_measuring_pp0[i] += pp0_epoch[i];
			power_measuring_dram[i] += dram_epoch[i];
		}
	}

	/* get pmc values now */
	for (i = 0; i < num_all_cores; i++){
		for (k = 0; k < 4; k++){
			update_pmc_val (i, k);
		}
	}

	/* take average of 4 pmcs on each cpu socket */

	int num_core_each_socket = num_all_cores / nb_pkg;
	if (num_all_cores % nb_pkg != 0){
		debug ("there might be error on number of cores\n");
	}

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

	debug
		("%6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f \n",
		 pkg_epoch[0], pp0_epoch[0], dram_epoch[0], pkg_epoch[1], pp0_epoch[1],
		 dram_epoch[1], pmc_epoch[0][0], pmc_epoch[0][1], pmc_epoch[0][2],
		 pmc_epoch[0][3], pmc_epoch[1][0], pmc_epoch[1][1], pmc_epoch[1][2],
		 pmc_epoch[1][3]);
	return;
}

///////////////////////////////////////////////////////////////////
void
_send_rapl_info (f)
		 int f;
{
	int i;
	struct node_info node;

	for (i = 0; i < nb_pkg; i++){
		// power limit now
		node.pkg_limit[i] = get_pkg_power_limit (i);
		node.pp0_limit[i] = get_pp0_power_limit (i);
		node.dram_limit[i] = get_dram_power_limit (i);
		// power usage now	 
		node.pkg_watts[i] = pkg_epoch[i];
		node.pp0_watts[i] = pp0_epoch[i];
		node.dram_watts[i] = dram_epoch[i];
		node.pmc0[i] = pmc_epoch[i][0];
		node.pmc1[i] = pmc_epoch[i][1];
		node.pmc2[i] = pmc_epoch[i][2];
		node.pmc3[i] = pmc_epoch[i][3];
	}

	debug ("%10.2f %10.2f %10.2f %10.2f %10.2f %10.2f \n",
		 node.pkg_watts[0], node.pp0_watts[0], node.dram_watts[0],
		 node.pkg_watts[1], node.pp0_watts[1], node.dram_watts[1]);
	debug (stderr,
		 "%10.2f %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f\n",
		 node.pmc0[0], node.pmc1[0], node.pmc2[0], node.pmc3[0],
		 node.pmc0[1], node.pmc1[1], node.pmc2[1], node.pmc3[1]);
	//send (f, &(node.pkg_limit[0]), sizeof (double) * nb_pkg * 10, 0);
	return;
}
///////////////////////////////////////////////////////////////////////////////






///////////////////////////////////////
// 結局これらのデータはslurm.hにおいてある
// 多分関連malocも同様では？要確認
///////////////////////////////////////

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



















extern int perf_mon_p_update_node_energy(void)
{
	int rc = SLURM_SUCCESS;

	xassert(_run_in_daemon());

	if (local_energy->current_watts == NO_VAL)
		return rc;

	_get_joules_task(local_energy);

	return rc;
}

/*
 * init() is called when the plugin is loaded, before any other functions
 * are called.  Put global initialization here.
 */
extern int init(void)
{
	debug_flags = slurm_get_debug_flags();

	gethostname(hostname, MAXHOSTNAMELEN);

	/* put anything that requires the .conf being read in
	   perf_mon_p_conf_parse
	*/

	return SLURM_SUCCESS;
}

extern int fini(void)
{
	int i;

	if (!_run_in_daemon())
		return SLURM_SUCCESS;

	for (i = 0; i < nb_pkg; i++) {
		if (pkg_fd[i] != -1) {
			close(pkg_fd[i]);
			pkg_fd[i] = -1;
		}
	}

	perf_mon_destroy(local_energy);
	local_energy = NULL;
	return SLURM_SUCCESS;
}

extern int perf_mon_p_get_data(enum acct_energy_type data_type,
					 void *data)
{
	int rc = SLURM_SUCCESS;
	perf_mon_t *energy = (perf_mon_t *)data;
	time_t *last_poll = (time_t *)data;
	uint16_t *sensor_cnt = (uint16_t *)data;

	xassert(_run_in_daemon());

/*
	switch (data_type) {
	case ENERGY_DATA_JOULES_TASK:
	case ENERGY_DATA_NODE_ENERGY_UP:
		if (local_energy->current_watts == NO_VAL)
			energy->consumed_energy = NO_VAL;
		else
			_get_joules_task(energy);
		break;
	case ENERGY_DATA_STRUCT:
	case ENERGY_DATA_NODE_ENERGY:
		memcpy(energy, local_energy, sizeof(perf_mon_t));
		break;
	case ENERGY_DATA_LAST_POLL:
		*last_poll = local_energy->poll_time;
		break;
	case ENERGY_DATA_SENSOR_CNT:
		*sensor_cnt = 1;
		break;
	default:
		error("perf_mon_p_get_data: unknown enum %d",
		      data_type);
		rc = SLURM_ERROR;
		break;
	}
	return rc;
*/

	switch (data_type) {
	case PERF_DATA_UPDATE:
		if (local_energy->current_watts == NO_VAL)
			energy->consumed_energy = NO_VAL;
		else
			_get_joules_task(energy);
		break;:
	case PERF_DATA_UPDATE:
		memcpy(energy, local_energy, sizeof(perf_mon_t));
		break;
	default:
		error("perf_mon_p_get_data: unknown enum %d",
		      data_type);
		rc = SLURM_ERROR;
		break;
	}
	return rc;


}

extern int perf_mon_p_set_data(enum acct_energy_type data_type,
					 void *data)
{
	int rc = SLURM_SUCCESS;

	xassert(_run_in_daemon());

/*
	switch (data_type) {
	case ENERGY_DATA_RECONFIG:
		debug_flags = slurm_get_debug_flags();
		break;
	case ENERGY_DATA_PROFILE:
		_get_joules_task(local_energy);
		_send_profile();
		break;
	default:
		error("perf_mon_p_set_data: unknown enum %d",
		      data_type);
		rc = SLURM_ERROR;
		break;
	}
	return rc;
*/
	switch (data_type) {
	case PERF_DATA_SET_POWER_LIMIT:
		debug_flags = slurm_get_debug_flags();


			set_pkg_power_limit (0, (double) creq[1], 1);
			set_pkg_power_limit (1, (double) creq[1], 1);

		break;


	case PERF_DATA_SET_CLEAR:
		_get_joules_task(local_energy);
		_send_profile();


			set_pkg_power_limit (0, thermal_design_power[0], 0);
			set_pkg_power_limit (1, thermal_design_power[1], 0);

		break;
	default:
		error("perf_mon_p_set_data: unknown enum %d",
		      data_type);
		rc = SLURM_ERROR;
		break;
	}
	return rc;

}

extern void perf_mon_p_conf_options(s_p_options_t **full_options,
					      int *full_options_cnt)
{
	return;
}

extern void perf_mon_p_conf_set(s_p_hashtbl_t *tbl)
{
	int i;
	uint64_t result;

	if (!_run_in_daemon())
		return;

	_hardware();
	for (i = 0; i < nb_pkg; i++)
		pkg_fd[i] = _open_msr(pkg2cpu[i]);

	//この関数は作らないといけない
	local_energy = perf_mon_alloc(1);

	result = _read_msr(pkg_fd[0], MSR_RAPL_POWER_UNIT);
	if (result == 0)
		local_energy->current_watts = NO_VAL;




	for (i = 0; i < nb_pkg; i++){
		//Calculate the units used
		uint64_t pw_unit_reg = 0;
		uint64_t pw_info_reg = 0;
		rdmsr ("main", msr_socket_file_name[i], MSR_RAPL_POWER_UNIT,
		 &pw_unit_reg);
		rdmsr ("main", msr_socket_file_name[i], MSR_PKG_POWER_INFO,
		 &pw_info_reg);

		power_units[i] = pow (0.5, (double) (pw_unit_reg & 0xf));
		energy_units[i] = pow (0.5, (double) ((pw_unit_reg >> 8) & 0x1f));
		time_units[i] = pow (0.5, (double) ((pw_unit_reg >> 16) & 0xf));
		thermal_design_power[i] =
			power_units[i] * (double) (pw_info_reg & 0x7fff);

		_start_en_val_[i][0] = read_energy_val (i, MSR_PKG_ENERGY_STATUS);	// PKG
		_start_en_val_[i][1] = read_energy_val (i, MSR_PP0_ENERGY_STATUS);	// PP0
		_start_en_val_[i][2] = read_energy_val (i, MSR_DRAM_ENERGY_STATUS);	// DRAM	 

		//debug ("core %2d :\n", first_cores[i]);
		debug ("	Power Units = %.3fW\n", power_units[i]);
		debug ("	Energy Units = %.8fJ\n", energy_units[i]);
		debug ("	Time Units = %.8fs\n", time_units[i]);
		debug ("	Package thermal spec: %.3fW\n",
				 thermal_design_power[i]);

		minimum_power_value =
			power_units[i] * (double) ((pw_info_reg >> 16) & 0x7fff);
		maximum_power_value =
			power_units[i] * (double) ((pw_info_reg >> 32) & 0x7fff);
		double time_window =
			time_units[i] * (double) ((pw_info_reg >> 48) & 0x7fff);

		debug ("	Package minimum power: %.3fW\n", minimum_power_value);
		debug ("	Package maximum power: %.3fW\n", maximum_power_value);
		debug ("	Package maximum time window: %.3fs\n", time_window);
	}


	/* signal 処理 */
	interval_sec = (DEFAULT_INTERVAL) / 1000;
	interval_usec = (DEFAULT_INTERVAL % 1000) * 1000;
	_msec_ =
		((float) (interval_sec) + (float) (interval_usec) * 0.000001) * 1000;

	debug ("interval_sec=%i\n", interval_sec);
	debug ("interval_usec=%i\n", interval_usec);
	debug ("_msec_=%f\n", _msec_);

	signal (SIGALRM, get_power_at_time_interval);	/* 定期実行 */
	get_power_at_time_interval ();	// 20150701: cao added

	debug("%s loaded", plugin_name);

	return;
}

extern void perf_mon_p_conf_values(List *data)
{
	return;
}





