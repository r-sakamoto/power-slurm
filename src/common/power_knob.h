/*****************************************************************************\
 *  power_knob.h - implementation-independent power knob plugin definitions
 *****************************************************************************
 *
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

#ifndef __SLURM_POWER_KNOB_H__
#define __SLURM_POWER_KNOB_H__

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

extern int slurm_power_knob_init(void);
extern int power_knob_fini(void);
extern power_data_t *power_knob_data_alloc(void);
extern void power_knob_data_destroy(power_data_t *p_data);
extern void power_knob_data_pack(power_data_t *p_data, Buf buffer, 
					uint16_t protocol_version);
extern int power_knob_data_unpack(power_data_t **get_data, Buf buffer,
							 bool need_alloc);
extern power_current_data_t *power_knob_current_alloc(uint16_t cnt);
extern void power_knob_current_destroy(power_current_data_t *current_data);
extern power_capping_data_t *power_knob_cap_alloc(uint16_t cnt);
extern void power_knob_cap_destroy(power_capping_data_t *cap_data);
extern cache_ref_t *power_knob_cache_alloc(uint16_t cnt);
extern void power_knob_cache_destroy(cache_ref_t *cache_data);
extern void power_knob_current_pack(power_current_data_t *get_data, Buf buffer);
extern void power_knob_cap_pack(power_capping_data_t *set_data, Buf buffer);
extern void power_knob_cache_pack(cache_ref_t *cache_data, Buf buffer);
extern int power_knob_current_unpack(power_current_data_t *get_data, Buf buffer,
							 bool need_alloc);
extern int power_knob_cap_unpack(power_capping_data_t *set_data, Buf buffer,
							 bool need_alloc);
extern int power_knob_cache_unpack(cache_ref_t *cache_data, Buf buffer,
							 bool need_alloc);
extern int power_knob_g_get_data(enum  power_knob_type data_type, void *data);
extern int power_knob_g_get_cache_data(enum cache_type data_type, void *data);
extern int power_knob_g_set_data(void *data);
extern void power_knob_g_conf_set(void);

#endif /*__SLURM_POWER_KNOB_H__*/
