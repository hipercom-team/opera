/*---------------------------------------------------------------------------
 *               OPERA - Inter-module Exchange for Coloring
 *---------------------------------------------------------------------------
 * Authors: Ichrak Amdouni, Cedric Adjih
 * Copyright 2011 Inria.
 *
 * This file is part of the OPERA.
 *
 * The OPERA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * The OPERA is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the OPERA; see the file LICENSE.LGPLv3.  If not, see
 * http://www.gnu.org/licenses/ or write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA. 
 *---------------------------------------------------------------------------*/

#include "hipsens-all.h"
#ifndef WITH_SIMUL
#include <macari.h>
#endif

/*---------------------------------------------------------------------------*/

/* XXX: the naming of these intermediary functions is inconsistent */

/* 
   Copy the topology from 

   XXX: warning, this function might be slow [ O(M^2) where M = nb. neighbors ]
*/
int opera_set_serena_topology_info(opera_state_t* state,
				   eostc_tree_t* tree)
{
  ASSERT( IS_FOR_SERENA(*tree) );
  eostc_serena_tree_t* serena_tree = tree->serena_info;

  serena_state_t* serena_state = &state->serena_state;
  eostc_state_t* eostc_state = &state->eostc_state;
  eond_state_t* eond_state = &state->eond_state;

  /* --- set topology table */
  serena_state->nb_neighbor = 0;
  int parent_count = 0;
  int i,j;
  for (i=0; i<EOND_MAX_NEIGHBOR(eond_state); i++) {
    eond_neighbor_t* neighbor = &(eond_state->neighbor_table[i]);
    if (neighbor->state != EOND_Sym)
      continue;
    
    serena_neighbor_t* current_neigh 
      = &(serena_state->neighbor_table[serena_state->nb_neighbor]);
    serena_state->nb_neighbor ++;
    hipsens_address_copy(current_neigh->address, neighbor->address);

    if (hipsens_address_equal(neighbor->address, tree->parent_address)) {
      parent_count ++;
      current_neigh->is_parent = HIPSENS_TRUE;
    } else current_neigh->is_parent = HIPSENS_FALSE;

    current_neigh->is_child = HIPSENS_FALSE; /* updated below */
  }
  
  int unstability_count = 0;
  for (j=0; j<MAX_NEIGHBOR; j++) { //XXX: could be more?
    eostc_child_t* child = &serena_tree->child[j];
    if (child->status == Child_None)
      continue;
    if (child->status == Child_Unstable)
      unstability_count ++;
    
    serena_neighbor_t* neighbor = NULL;
    for (i=0;i<serena_state->nb_neighbor;i++)
      if (hipsens_address_equal(serena_state->neighbor_table[i].address,
				child->address))
	neighbor = &serena_state->neighbor_table[i];
    if (neighbor != NULL) {
      neighbor->is_child = HIPSENS_TRUE;
    } else unstability_count ++;
  }

  /* --- reset serena state */
  serena_state->is_started = HIPSENS_FALSE;
  serena_state->is_topology_set = HIPSENS_TRUE;
  serena_state->is_finished = HIPSENS_FALSE;

  /* --- update priority */
  hipsens_u32 priority = 0;

  if (serena_state->config->priority_mode == Priority_mode_tree) {
    priority = eostc_count_descendant(eostc_state, tree);
  } else if (serena_state->config->priority_mode == Priority_mode_fixed) {
    priority = GET_PRIORITY(serena_state->config->fixed_priority);
  } else if (serena_state->config->priority_mode == Priority_mode_2hop) {
    priority = serena_state->nb_neighbor+eond_estimate_nb_2hop(eond_state);; //ichrak
#if defined(WITH_SIMUL) && defined(WITH_PRIO_3HOP)
  } else if (serena_state->config->priority_mode == Priority_mode_3hop) {
    priority = serena_state->nb_neighbor+eond_estimate_nb_3hop(eond_state);; //ichrak
#endif /* WITH_SIMUL + WITH_PRIO_3HOP */
  } else {
    STFATAL("Unknown priority_mode (=%d).\n",
	    serena_state->config->priority_mode);
  }

    if (priority == 0) 
      priority = 1;
#ifndef WITH_LONG_PRIORITY
    if (priority > 0xff)
      priority = 0xff;
#endif /* WITH_LONG_PRIORITY */
  SET_PRIORITY(serena_state->priority, priority);
  
  /* --- return */

  if (unstability_count > 0) {
    STWARN("Inconsistent topology for SERENA: %d mismatches found.\n",
	   unstability_count);
    return HIPSENS_FALSE;
  }
  return HIPSENS_TRUE;
}

/*---------------------------------------------------------------------------*/

void opera_on_coloring_finished(struct s_serena_state_t* state)
{
  opera_state_t* opera = (opera_state_t*) state->base->opaque_opera;

  opera->serena_state.is_finished = HIPSENS_TRUE;

  int nb_color = serena_get_nb_color(&opera->serena_state);
  if (nb_color == 0) {
    STWARN("opera_on_coloring_finished but no color available.\n");
  }
  hipsens_api_set_nb_color(opera->base_state.opaque_extra_info, nb_color);
  hipsens_api_should_run_serena(opera->base_state.opaque_extra_info, 
				HIPSENS_FALSE);
}

// XXX!! remove when beacons are used
void opera_ensure_serena_stopped(eostc_state_t* state);
void opera_ensure_serena_stopped(eostc_state_t* state)
{
  opera_state_t* opera = (opera_state_t*) state->base->opaque_opera;
  opera->serena_state.is_finished = HIPSENS_TRUE;
}

void hipsens_eostc_event_my_tree_flags_change(eostc_state_t* state, 
					      eostc_tree_t* tree,
					      hipsens_u8 new_flags)
{
  ASSERT( IS_FOR_SERENA(*tree) );

  if ( (new_flags & (1<<EOSTC_FLAG_STABLE_BIT)) == 0 )
    return; /* XXX: handle the 'unstable' case */

  /* the tree became stable: copy topology, update external state,
     and start SERENA */
  opera_state_t* opera = (opera_state_t*) state->base->opaque_opera;
  opera_set_serena_topology_info(opera, tree);
  
  hipsens_api_should_run_serena(opera->base_state.opaque_extra_info, 
				HIPSENS_TRUE);

  opera->serena_state.callback_coloring_finished = &opera_on_coloring_finished;

  new__serena_start(&opera->serena_state); //ichrak
}


/*
  This function is called when a node becomes "is_locally_stable" ;
  (the first time a Tree Status stable will be generated).

  The function will s the topology informations to SERENA
 */
void hipsens_eostc_event_tree_stability(eostc_state_t* state, 
					eostc_tree_t* tree)
{
  opera_state_t* opera = (opera_state_t*) state->base->opaque_opera;
  
  ASSERT( IS_FOR_SERENA(*tree) );

  if (opera->serena_state.is_started && !opera->serena_state.is_finished) {
    /* SERENA is already running */
    int address_cmp = hipsens_address_cmp(opera->serena_state.root_address,
					  tree->root_address);
    if (address_cmp < 0)
      return; /* take the minimum address */
    if (address_cmp == 0) {
      if (hipsens_seqnum_cmp
	  (tree->serena_info->tree_seq_num, opera->serena_state.tree_seq_num) 
	  < 0)
	return;
      /* 
      if (tree->serena_info->tree_seq_num == opera->serena_state.tree_seq_num
      is strange */
    }
  }
  
  opera_set_serena_topology_info(opera, tree);
}

hipsens_bool hipsens_is_tree_being_colored(eostc_state_t* state, 
					   eostc_tree_t* tree)
{ 
  opera_state_t* opera = (opera_state_t*) state->base->opaque_opera;
  if (!IS_FOR_SERENA(*tree))
    return HIPSENS_FALSE;
  ASSERT( IS_FOR_SERENA(*tree) );
  return (
     opera->serena_state.is_started && !opera->serena_state.is_finished
     && hipsens_address_equal(opera->serena_state.root_address,
			      tree->root_address)
     && (tree->serena_info->tree_seq_num == opera->serena_state.tree_seq_num));
}

void hipsens_notify_neighbor_disappeared(base_state_t* base, address_t address)
{
  opera_state_t* opera = (opera_state_t*) base->opaque_opera;
  int neighbor_index = serena_find_neighbor_index(&opera->serena_state,address);
  if (neighbor_index >= 0) {
    serena_remove_neighbor(&opera->serena_state, neighbor_index);
  }
}

void hipsens_notify_tree_change(base_state_t* base, address_t address,
				eostc_tree_t* tree,
				hipsens_tristate is_parent, 
				hipsens_tristate is_child)
{
  opera_state_t* opera = (opera_state_t*) base->opaque_opera;
  if (!hipsens_is_tree_being_colored(&opera->eostc_state, tree))
    return;
  int neighbor_index = serena_find_neighbor_index(&opera->serena_state,address);
  if (neighbor_index < 0)
    return;
  serena_neighbor_t* neighbor
    = &(opera->serena_state.neighbor_table[neighbor_index]);

  if (is_child == HIPSENS_FALSE)     neighbor->is_child = HIPSENS_FALSE;
  else if (is_child == HIPSENS_TRUE) neighbor->is_child = HIPSENS_TRUE;

  if (is_parent == HIPSENS_FALSE)     neighbor->is_parent = HIPSENS_FALSE;
  else if (is_parent == HIPSENS_TRUE) neighbor->is_parent = HIPSENS_TRUE;
}

#if 0
/* XXX:remove old API */
void opera_event_serena_run_change
(opera_state_t* state, hipsens_bool should_run_serena)
{
  if (state->is_colored_tree_root) {
    STWARN("opera_event_serena_run_change called on a tree root,\n");
    return;
  }
  
  new__serena_start(&state->serena_state); //without => start is delayed by two rounds
}
#endif 
    
/*---------------------------------------------------------------------------*/

/* call to OPERA, to indicate that it should be running SERENA */
void opera_external_notify_should_run_serena(opera_state_t* state,
					     hipsens_bool should_run)
{
#ifdef WITH_OPERA_SYSTEM_INFO
  state->base->sys_info |= OPERA_SYSTEM_INFO_HAS_COLORING_MODE_INDICATION;
  if (should_run) state->base->sys_info |= OPERA_SYSTEM_INFO_COLORING_MODE;
  else state->base->sys_info &= ~OPERA_SYSTEM_INFO_COLORING_MODE;
#endif /* WITH_OPERA_SYSTEM_INFO */
      
  if (state->is_colored_tree_root) {
    STWARN("opera_external_notify_should_run_serena called on a tree root.\n");
    return;
  }

  if (should_run) {
    state->should_start_serena = HIPSENS_TRUE;
  } else {
    state->serena_state.is_finished = HIPSENS_TRUE;
  }
  
  hipsens_api_response_should_run_serena(state->base_state.opaque_extra_info);
}


void opera_external_notify_color_use(opera_state_t* state,
				     hipsens_bool should_use_color)
{
#ifdef WITH_OPERA_SYSTEM_INFO
  state->base->sys_info |= OPERA_SYSTEM_INFO_HAS_MAX_COLOR_INDICATION;
#endif /* WITH_OPERA_SYSTEM_INFO */  
  if (should_use_color) {
    serena_state_t* serena = &state->serena_state;
    if (serena->final_node_color != OCARI_NO_COLOR) {
      hipsens_api_set_color_info(state->base_state.opaque_extra_info,
				 serena->final_node_color,
				 serena->final_nb_neighbor_color,
				 serena->final_neighbor_color_list);
    } else {
      STWARN("external_notify_color_use, but no colors are available.\n");
      hipsens_api_set_color_info(state->base_state.opaque_extra_info,
				 OCARI_NO_COLOR, 0,
				 serena->final_neighbor_color_list);
    }
  }
}

/*---------------------------------------------------------------------------*
 * Part of the Interface with OCARI
 *---------------------------------------------------------------------------*/

#ifndef WITH_SIMUL

#ifdef WITH_OPERA_SYSTEM_INFO
extern opera_state_t opera;
#endif

//#warning "[CA] no call OPERA -> MACARI primitives"

/* acknowledges an opera_external_notify_should_run_serena(...) */
void hipsens_api_response_should_run_serena(void* opaque_extra_info)
{
  //#warning "[CA] The following should be commented out:"
  // MaCARIColoringModeOnResponse();
}

/* Called by OPERA at tree root, to announce that SERENA should be
   started on all nodes */
void hipsens_api_should_run_serena
(void* opaque_extra_info, hipsens_bool should_run)
{
#ifdef WITH_OPERA_SYSTEM_INFO
  opera.base->sys_info |= OPERA_SYSTEM_INFO_HAS_COLORING_MODE_REQUEST;
  if (should_run) opera.base->sys_info |= OPERA_SYSTEM_INFO_COLORING_MODE;
  else opera.base->sys_info &= ~OPERA_SYSTEM_INFO_COLORING_MODE;
#endif /* WITH_OPERA_SYSTEM_INFO */
  MaCARIColoringModeOnRequest(should_run);
}

/* Called by OPERA at tree root, to announce the number of colors
    on all nodes */
void hipsens_api_set_nb_color
(void* opaque_extra_info, int nb_color)
{
#ifdef WITH_OPERA_SYSTEM_INFO
  opera.base->sys_info |= OPERA_SYSTEM_INFO_HAS_MAX_COLOR_REQUEST;
  opera.base->sys_info_color = (opera.base->sys_info_color & 0x0f)
                               | (nb_color << 4);
#endif
  MaCARIMaxColorRequest(nb_color);
}

/* Called by OPERA to give the coloring information:
   the colors passed are numbered starting from 1 */
void hipsens_api_set_color_info
(void* opaque_extra_info, byte node_color, 
 byte nb_neighbor_color, byte* neighbor_color_list)
{
#ifdef WITH_OPERA_SYSTEM_INFO
  opera.base->sys_info |= OPERA_SYSTEM_INFO_HAS_MAX_COLOR_RESPONSE;
  opera.base->sys_info_color = (opera.base->sys_info_color & 0xf0)
                               | (node_color & 0xf);
#endif  
  MaCARIMaxColorResponse(node_color, nb_neighbor_color, neighbor_color_list);
}

#endif // WITH_SIMUL

/*---------------------------------------------------------------------------*/
