/*****************************************************************************\
 *  power_knob.c - implementation-independent power knob plugin definitions
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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/macros.h"
#include "src/common/plugin.h"
#include "src/common/plugrack.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/power_knob.h"
#include "src/common/pack.h"

/*
** Define slurm-specific aliases for use by plugins, see slurm_xlator.h
** for details.
 */
//strong_alias(power_knob_destroy, slurm_power_knob_destroy);

typedef struct slurm_power_knob_ops {
	int (*get_data)           (enum  power_knob_type data_type, void *data);
	int (*get_cache_data)     (enum cache_type data_type, void *data);
	int (*set_data)           (void *data);
	void (*conf_set)          (void);
} slurm_power_knob_ops_t;
/*
 * These strings must be kept in the same order as the fields
 * declared for slurm_power_knob_ops_t.
 */
static const char *syms[] = {
	"power_knob_p_get_data",
	"power_knob_p_get_cache_data",
	"power_knob_p_set_data",
	"power_knob_p_conf_set",
};

static slurm_power_knob_ops_t ops;
static plugin_context_t *g_context = NULL;
static pthread_mutex_t g_context_lock =	PTHREAD_MUTEX_INITIALIZER;
static bool init_run = false;

extern int slurm_power_knob_init(void)
{
	int retval = SLURM_SUCCESS;
	char *plugin_type = "power_knob";
	char *type = NULL;

	if (init_run && g_context)
		return retval;

	slurm_mutex_lock(&g_context_lock);

	if (g_context)
		goto done;

	type = slurm_get_power_knob_type();

	g_context = plugin_context_create(
		plugin_type, type, (void **)&ops, syms, sizeof(syms));

	if (!g_context) {
		error("cannot create %s context for %s", plugin_type, type);
		retval = SLURM_ERROR;
		goto done;
	}
	init_run = true;

done:
	slurm_mutex_unlock(&g_context_lock);
	if (retval != SLURM_SUCCESS)
	fatal("can not open the %s plugin", type);
	xfree(type);

	return retval;
}

extern int power_knob_fini(void)
{
	int rc;

	if (!g_context)
		return SLURM_SUCCESS;

	init_run = false;
	rc = plugin_context_destroy(g_context);
	g_context = NULL;

	return rc;
}

extern void power_knob_data_pack(power_data_t *p_data, Buf buffer, 
						uint16_t protocol_version)
{

	int i;

		pack16(p_data->socket_cnt, buffer);
		for(i = 0; i < p_data->socket_cnt; i++){
			power_knob_cap_pack(&p_data->power_cap[i], buffer);
		}
		for(i = 0; i < p_data->socket_cnt; i++){
			power_knob_current_pack(&p_data->current_power[i], buffer);
		}
		for(i = 0; i < p_data->socket_cnt; i++ ){
			power_knob_cache_pack(&p_data->cache_reference[i], buffer);
		
		}
		pack_time(p_data->watts_update_time, buffer);
		pack32(p_data->cpu_max_watts, buffer);
		pack32(p_data->dram_max_watts, buffer);
		pack32(p_data->cpu_min_watts, buffer);
		pack32(p_data->dram_min_watts, buffer);
		pack32(p_data->power_state, buffer);
		pack64(p_data->joule_cpu, buffer);
		pack64(p_data->joule_dram, buffer);
		packdouble(p_data->miss_ratio_l1, buffer);
		packdouble(p_data->miss_ratio_l2, buffer);
		packdouble(p_data->miss_ratio_l3, buffer);
		pack32(p_data->cache_state, buffer);
		pack_time(p_data->cache_update_time, buffer);

	//}

}

extern int power_knob_data_unpack(power_data_t **get_data, Buf buffer, 
                                                         bool need_alloc)
{

	int i;
	power_data_t *get_data_ptr;

	power_capping_data_t *p_cap;
	power_current_data_t *c_power;
	cache_ref_t *cache_ref_data;

	if (need_alloc) {
		get_data_ptr = power_knob_data_alloc();
		*get_data = get_data_ptr;
	} else {
		get_data_ptr = *get_data;
	}

	safe_unpack16(&get_data_ptr->socket_cnt, buffer);

	p_cap = get_data_ptr->power_cap =  power_knob_cap_alloc(get_data_ptr->socket_cnt);
	c_power = get_data_ptr->current_power = power_knob_current_alloc(get_data_ptr->socket_cnt);
	cache_ref_data = get_data_ptr->cache_reference = power_knob_cache_alloc(get_data_ptr->socket_cnt);


	for(i = 0; i < get_data_ptr->socket_cnt; i++){
		power_knob_cap_unpack(&p_cap[i], buffer, 0);
	}
	for(i = 0; i < get_data_ptr->socket_cnt; i++){
		power_knob_current_unpack(&c_power[i], buffer, 0);
	}

	for(i = 0; i < get_data_ptr->socket_cnt; i++){
		power_knob_cache_unpack(&cache_ref_data[i], buffer, 0);
	}
	
	safe_unpack_time(&get_data_ptr->watts_update_time, buffer);
	safe_unpack32(&get_data_ptr->cpu_max_watts, buffer);
	safe_unpack32(&get_data_ptr->dram_max_watts, buffer);
	safe_unpack32(&get_data_ptr->cpu_min_watts, buffer);
	safe_unpack32(&get_data_ptr->dram_min_watts, buffer);
	safe_unpack32(&get_data_ptr->power_state, buffer);
	safe_unpack64(&get_data_ptr->joule_cpu, buffer);
	safe_unpack64(&get_data_ptr->joule_dram, buffer);
	safe_unpackdouble(&get_data_ptr->miss_ratio_l1, buffer);
	safe_unpackdouble(&get_data_ptr->miss_ratio_l2, buffer);
	safe_unpackdouble(&get_data_ptr->miss_ratio_l3, buffer);
	safe_unpack32(&get_data_ptr->cache_state, buffer);
	safe_unpack_time(&get_data_ptr->cache_update_time, buffer);

	return SLURM_SUCCESS;

unpack_error:
	if (need_alloc) {
		power_knob_data_destroy(get_data_ptr);
		*get_data = NULL;
	} else {
		memset(get_data_ptr, 0, sizeof(power_data_t));
	}
	
	return SLURM_ERROR;

}



extern power_data_t *power_knob_data_alloc(void)
{
	power_data_t *p_data =
		xmalloc(sizeof(struct power_data) );

	return p_data;
}

extern void power_knob_data_destroy(power_data_t *p_data){
	power_knob_current_destroy(p_data->current_power);
	power_knob_cap_destroy(p_data->power_cap);
	power_knob_cache_destroy(p_data->cache_reference);
	p_data->current_power = NULL;
	p_data->power_cap = NULL;
	p_data->cache_reference = NULL;
	xfree(p_data);
}

extern power_current_data_t *power_knob_current_alloc(uint16_t cnt)
{
	power_current_data_t *current_data =
		xmalloc(sizeof(struct power_current_data) * cnt);

	return current_data;
}

extern void power_knob_current_destroy(power_current_data_t *current_data)
{
	xfree(current_data);
}

extern power_capping_data_t *power_knob_cap_alloc(uint16_t cnt)
{
	power_capping_data_t *cap_data =
		xmalloc(sizeof(struct power_capping_data) * cnt);

	return cap_data;
}

extern void power_knob_cap_destroy(power_capping_data_t *cap_data)
{
	xfree(cap_data);
}

extern cache_ref_t *power_knob_cache_alloc(uint16_t cnt)
{
	int i;
	cache_ref_t *cache_d =
		xmalloc(sizeof(struct cache_ref) * cnt);

	for(i=0;i<cnt;i++){
		(cache_d+i)->cache_mode = 5;
	}

	return cache_d;
}

extern void power_knob_cache_destroy(cache_ref_t *cache_data)
{
	xfree(cache_data);
}

extern void power_knob_current_pack(power_current_data_t *get_data, Buf buffer)
{
		
		if (!get_data) {
			pack32(0, buffer);
			pack32(0, buffer);
			pack32(0, buffer);
			pack32(0, buffer);
			pack32(0, buffer);
			pack_time(0, buffer);
			pack32(0, buffer);
			pack32(0, buffer);
			return;
		}
		pack32(get_data->dram_current_cap_watts, buffer);
		pack32(get_data->cpu_current_cap_watts, buffer);
		pack_time(get_data->poll_time, buffer);
		pack32(get_data->dram_current_frequency, buffer); 
		pack32(get_data->cpu_current_frequency, buffer); 
		pack32(get_data->dram_current_watts, buffer); 
		pack32(get_data->cpu_current_watts, buffer); 
		pack32(get_data->enable_monitor, buffer);
}

extern void power_knob_cap_pack(power_capping_data_t *set_data, Buf buffer)
{
	if (!set_data) {
		pack32(0, buffer);
		pack32(0, buffer); 
		pack32(0, buffer); 
		pack32(0, buffer); 
		pack32(0, buffer); 
		pack32(0, buffer);
		return;
	}

	 pack32(set_data->enable_cap_mode, buffer);
	 pack32(set_data->cpu_cap_watts, buffer); 
	 pack32(set_data->dram_cap_watts, buffer); 
	 pack32(set_data->cpu_cap_frequency, buffer); 
	 pack32(set_data->dram_cap_frequency, buffer); 
	 pack32(set_data->freq_mode, buffer);

}

extern void power_knob_cache_pack(cache_ref_t *cache_data, Buf buffer)
{
	if (!cache_data) {
		pack32(0, buffer);
		pack64(0, buffer);
		pack64(0, buffer);
		pack64(0, buffer);
		pack64(0, buffer);
		pack_time(0, buffer);
		return;
	}

	pack32(cache_data->cache_mode, buffer);
	pack64(cache_data->all_cache_ref, buffer);
	pack64(cache_data->l1_miss, buffer);
	pack64(cache_data->l2_miss, buffer);
	pack64(cache_data->l3_miss, buffer);
	pack_time(cache_data->poll_time, buffer);

}

extern int power_knob_current_unpack(power_current_data_t *get_data, Buf buffer,
							 bool need_alloc)
{
	power_current_data_t *get_data_ptr;

	if (need_alloc) {
		get_data_ptr = power_knob_current_alloc(1);
		get_data = get_data_ptr;
	} else {
		get_data_ptr = get_data;
	}
	safe_unpack32(&get_data_ptr->dram_current_cap_watts, buffer); 
	safe_unpack32(&get_data_ptr->cpu_current_cap_watts, buffer); 
	safe_unpack_time(&get_data_ptr->poll_time, buffer);
	safe_unpack32(&get_data_ptr->dram_current_frequency, buffer); 
	safe_unpack32(&get_data_ptr->cpu_current_frequency, buffer); 
	safe_unpack32(&get_data_ptr->dram_current_watts, buffer); 
	safe_unpack32(&get_data_ptr->cpu_current_watts, buffer); 
	safe_unpack32(&get_data_ptr->enable_monitor, buffer);
	
	return SLURM_SUCCESS;

unpack_error:
	if (need_alloc) {
		power_knob_current_destroy(get_data_ptr);
		get_data = NULL;
	} else
		memset(get_data_ptr, 0, sizeof(power_current_data_t));

	return SLURM_ERROR;
}

extern int power_knob_cap_unpack(power_capping_data_t *set_data, Buf buffer,
							 bool need_alloc)
{
	power_capping_data_t *set_data_ptr;

	if (need_alloc) {
		set_data = set_data_ptr =  power_knob_cap_alloc(1);
	} else {
		set_data_ptr = set_data;
	}

	safe_unpack32(&set_data_ptr->enable_cap_mode, buffer);
	safe_unpack32(&set_data_ptr->cpu_cap_watts, buffer); 
	safe_unpack32(&set_data_ptr->dram_cap_watts, buffer); 
	safe_unpack32(&set_data_ptr->cpu_cap_frequency, buffer); 
	safe_unpack32(&set_data_ptr->dram_cap_frequency, buffer); 
	safe_unpack32(&set_data_ptr->freq_mode, buffer); 

	return SLURM_SUCCESS;

unpack_error:
	if (need_alloc) {
		power_knob_cap_destroy(set_data_ptr);
		set_data = NULL;
	} else
		memset(set_data_ptr, 0, sizeof(power_capping_data_t));

	return SLURM_ERROR;
}

extern int power_knob_cache_unpack(cache_ref_t *cache_data, Buf buffer,
							 bool need_alloc)
{
	safe_unpack32(&cache_data->cache_mode, buffer);
	safe_unpack64(&cache_data->all_cache_ref, buffer);
	safe_unpack64(&cache_data->l1_miss, buffer);
	safe_unpack64(&cache_data->l2_miss, buffer);
	safe_unpack64(&cache_data->l3_miss, buffer);
	safe_unpack_time(&cache_data->poll_time, buffer);

	return SLURM_SUCCESS;

unpack_error:
	if (need_alloc) {
		power_knob_cache_destroy(cache_data);
		cache_data = NULL;
	} else
		memset(cache_data, 0, sizeof(cache_ref_t));

	return SLURM_ERROR;
}

extern int power_knob_g_get_data(enum  power_knob_type data_type, void *data)
{
	debug("Power_knob_g_get_data running");
	int retval = SLURM_ERROR;

	if (slurm_power_knob_init() < 0)
		return retval;

	retval = (*(ops.get_data))(data_type, data);

	return retval;
}

extern int power_knob_g_get_cache_data(enum cache_type data_type, void *data)
{
	int retval = SLURM_ERROR;

	if (slurm_power_knob_init() < 0)
		return retval;

	retval = (*(ops.get_cache_data))(data_type, data);

	return retval;
}
extern int power_knob_g_set_data( void *data)
{
	int retval = SLURM_ERROR;

	if (slurm_power_knob_init() < 0)
		return retval;

	retval = (*(ops.set_data))(data);

	return retval;
}

extern void power_knob_g_conf_set(void)
{
	debug3("power_knob_g_conf_set");
	if (slurm_power_knob_init() < 0)
		return ;

	(*(ops.conf_set))();

	return;

}

