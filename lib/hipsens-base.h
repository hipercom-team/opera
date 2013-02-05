/*---------------------------------------------------------------------------
 *                    OPERA - Base Definitions/Functions
 *---------------------------------------------------------------------------
 * Author: Cedric Adjih
 * Copyright 2010-2011 Inria.
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

#ifndef _HIPSENS_BASE_H
#define _HIPSENS_BASE_H

/*---------------------------------------------------------------------------*/

#include <stdlib.h>

#include "hipsens-config.h"
#include "hipsens-macro.h"

/*---------------------------------------------------------------------------*/
/* Forced defines (never tested without) */

#define WITH_ENERGY

/*---------------------------------------------------------------------------*/

#if defined(HIPSENS_MEM_MODEL_64BITS)

typedef unsigned char  hipsens_u8;
typedef unsigned short hipsens_u16;
typedef unsigned int   hipsens_u32;
typedef signed   char  hipsens_s8;
typedef signed   short hipsens_s16;
typedef signed   int   hipsens_s32;

#elif defined(HIPSENS_MEM_MODEL_16BITS)
typedef unsigned char  hipsens_u8;
typedef unsigned short hipsens_u16;
typedef unsigned long  hipsens_u32;
typedef signed   char  hipsens_s8;
typedef signed   short hipsens_s16;
typedef signed   long  hipsens_s32;

#else
#error Model for integer sizes not defined

#endif  /* HIPSENS_MEM_MODEL */

#define HIPSENS_U_BYTE_1 hipsens_u8
#define HIPSENS_U_BYTE_2 hipsens_u16
#define HIPSENS_U_BYTE_4 hipsens_u32

#define HIPSENS_S_BYTE_1 hipsens_s8
#define HIPSENS_S_BYTE_2 hipsens_s16
#define HIPSENS_S_BYTE_4 hipsens_s32

#define HIPSENS_U(size) HIPSENS_U_BYTE_##size
#define HIPSENS_S(size) HIPSENS_S_BYTE_##size

/*---------------------------------------------------------------------------*/

typedef hipsens_u8 byte;
typedef hipsens_u8 hipsens_bool;
typedef hipsens_u8 hipsens_tristate; /* HIPSENS{_TRUE,_FALSE,_UNDEF} */

#define HIPSENS_FALSE 0
#define HIPSENS_TRUE  1
#define HIPSENS_UNDEF 2

/*---------------------------------------------------------------------------*/

#if defined(WITH_SIMUL_FRAME) && !defined(WITH_FRAME_TIME)
#define WITH_FRAME_TIME
#endif

/**
 * Time management
 */

#ifdef WITH_FRAME_TIME

/* in this model, time is expressed in cycles ; 
   XXX: We take 1 cycle = 1 sec for now
 */
#define FMT_HST "%ld"
typedef hipsens_s32 hipsens_time_t;
//#define MILLISEC_TO_HIPSENS_TIME(millisec) (((hipsens_time_t)(millisec))/1000)
#define SEC_TO_FRAME_TIME(sec) ((sec))
//#define HIPSENS_TIME_TO_SEC(time) ((time))

#elif defined(IS_EMBEDDED)        //----------------------------------------

#ifndef WITH_CONTIKI

/* in this model, time is stored in 1/100 sec */
#define FMT_HST "%ld"
typedef hipsens_s32 hipsens_time_t;
#define MILLISEC_TO_HIPSENS_TIME(millisec) (((hipsens_time_t)(millisec))/10)
#define SEC_TO_HIPSENS_TIME(sec) ((sec)*100)
#define HIPSENS_TIME_TO_SEC(time) ((time)/100)

#else /* WITH_CONTIKI */            //----------------------------------------

/* in this model, time is stored in 1/128 sec */
#define FMT_HST "%ld"
typedef hipsens_s32 hipsens_time_t;
#define MILLISEC_TO_HIPSENS_TIME(millisec) \
  ((((hipsens_time_t)(millisec))*CLOCK_CONF_SECOND)/1000)
#define SEC_TO_HIPSENS_TIME(sec) ((sec)*CLOCK_CONF_SECOND)
#define HIPSENS_TIME_TO_SEC(time) ((time)/CLOCK_CONF_SECOND)
#endif /* WITH_CONTIKI */

#else /*!IS_EMBEDDED*/              //----------------------------------------
//--------------------------------------------------

/* in this model, time is stored in millisec */
#if defined(HIPSENS_MEM_MODEL_16BITS)
#define FMT_HST "%ld"
#else
#define FMT_HST "%d"
#endif
typedef hipsens_s32 hipsens_time_t;
#define MILLISEC_TO_HIPSENS_TIME(millisec) (millisec)
#define SEC_TO_HIPSENS_TIME(sec) ((sec)*1000)
#define HIPSENS_TIME_TO_SEC(time) ((time)/1000)

#endif /* IS_EMBEDDED */

//--------------------------------------------------

#ifndef IS_EMBEDDED 

/* these are used only by the Python checker and placed here only to 
   because they should be kept in sync with the previous macros.
  they probably require 64-bit integers to be used properly */
#define ONE_BILLION (1000*1000*1000) /* 10^9 */
#define NANOSEC_TO_HIPSENS_TIME(sec,nanosec) \
  ((SEC_TO_HIPSENS_TIME(sec) + (nanosec)/(1000*1000)))
#define HIPSENS_TIME_TO_NANOSEC(time) ((time)*1000*1000)

#endif /* IS_EMBEDDED */
//-----------------------------------------------------------------------------!



hipsens_u8 hipsens_time_to_vtime(hipsens_time_t clock);
hipsens_time_t vtime_to_hipsens_time(hipsens_u8 vtime);

extern hipsens_time_t undefined_time;

#define HIPSENS_TIME_ADD(t1,t2) ((t1)+(t2))
#define HIPSENS_TIME_EXPIRED(t) ((t)-1)

/** Time comparison macros. They are used as: 
 * if (HIPSENS_TIME_COMPARE_LARGE_UNDEF(t1, <=, t2)) 
 *     ...
 * ...LARGE_UNDEF assumes that undefined_time is +INFINITY
 * ...SMALL_UNDEF assumes that undefined_time is -INFINITY
 * ...NO_UNDEF requires that neither t1 nor t2 are undefined_time
 */
#define HIPSENS_TIME_COMPARE_NO_UNDEF(t1, cmpoperator, t2) \
  ((hipsens_time_cmp_no_undef(t1,t2)) cmpoperator 0)

#define HIPSENS_TIME_COMPARE_LARGE_UNDEF(t1,cmpoperator,t2) \
  ((hipsens_time_cmp(t1,t2,HIPSENS_TRUE)) cmpoperator 0)

#define HIPSENS_TIME_COMPARE_SMALL_UNDEF(t1,cmpoperator,t2) \
  ((hipsens_time_cmp(t1,t2,HIPSENS_FALSE)) cmpoperator 0)



/** - set current_time to the minimum of *current_time and new_time
 *    if one is undefined_time, it is ignored.
 *  - return HIPSENS_TRUE iff current_time has been changed 
 */
hipsens_bool hipsens_time_to_min(hipsens_time_t* current_time, 
				 hipsens_time_t new_time);

/** same as hipsens_time_to_min, except the value is returned */
hipsens_time_t hipsens_time_min(hipsens_time_t t1, hipsens_time_t t2);

/** compare two times
 *  if undefinded_time_is_large then undefined_time is considered 
 *  to be +INFINITY otherwise it is considered to be -INFINITY
 *  return value follows same rules as memcmp
 */
hipsens_s8 hipsens_time_cmp(hipsens_time_t t1, hipsens_time_t t2,
			    hipsens_bool undefined_time_is_large);

/** same as hipsens_time_cmp, but neither t1 nor t2 can be undefined_time */
hipsens_s8 hipsens_time_cmp_no_undef(hipsens_time_t t1, hipsens_time_t t2);

typedef struct s_hipsens_wakeup_condition {
  hipsens_time_t wakeup_time; /**< */
  hipsens_time_t wakeup_time_buffer; /**< */
} hipsens_wakeup_condition_t;

void wakeup_condition_init(hipsens_wakeup_condition_t* condition);

#define FLAG_TIME_WITH_BUFFER (1<<0)
#define FLAG_TIME_WITHOUT_BUFFER (1<<1)

int wakeup_condition_update(hipsens_wakeup_condition_t* condition,
			    hipsens_wakeup_condition_t* more_condition);

void wakeup_condition_ensure_minimum_time(hipsens_wakeup_condition_t* condition,
					  hipsens_time_t minimum_time);

#define HIPSENS_GET_MY_ADDRESS(base_state,varname) \
  address_t varname; \
  hipsens_api_get_my_address(base_state->opaque_extra_info, varname);

/*---------------------------------------------------------------------------*/

#define BITS_PER_BYTE 8

#define BYTE_OF_BIT(bitIndex) ((bitIndex) / BITS_PER_BYTE)
#define INDEX_OF_BIT(bitIndex) ((bitIndex) % BITS_PER_BYTE)
#define MASK_OF_INDEX(index) (1<<(index))

/*---------------------------------------------------------------------------*/

// XXX: remove
#define ND_OK 1
#define ND_ERROR 0

typedef struct s_buffer_t {
  byte* data;
  int size;
  int pos;
  int status; /* XXX: hipsens_bool status_ok */
} buffer_t;

void buffer_init(buffer_t* buffer, byte* data, int size);
void buffer_put_byte(buffer_t* buffer, byte data);
byte buffer_get_byte(buffer_t* buffer);
void buffer_put_short(buffer_t* buffer, unsigned int data);
unsigned int buffer_get_short(buffer_t* buffer);
void buffer_put_data(buffer_t* buffer, byte* data, int size);
void buffer_get_data(buffer_t* buffer, byte* data, int size);

/* XXX: these should completly replace buffer_put_byte/short */
#define buffer_put_u8 buffer_put_byte
#define buffer_get_u8 buffer_get_byte
#define buffer_put_u16 buffer_put_short
#define buffer_get_u16 buffer_get_short
#ifdef WITH_LONG_PRIORITY
void buffer_put_u32(buffer_t* buffer, hipsens_u32 data);
hipsens_u32 buffer_get_u32(buffer_t* buffer);
#endif /* WITH_LONG_PRIORITY - XXX: ifdef should be more general */
#define buffer_remaining(buffer) ((buffer)->size - (buffer)->pos)

#define buffer_put_ADDRESS(buffer, data) \
      (buffer_put_data((buffer),(data), ADDRESS_SIZE))
#define buffer_get_ADDRESS(buffer, data) \
      (buffer_get_data((buffer),(data), ADDRESS_SIZE))

/*---------------------------------------------------------------------------*/

int hipsens_fatal(void* state, const char* message);

/*---------------------------------------------------------------------------*/

/** A number generator which is somewhat random, returns numbers between
 * [0, 2^16-2], its period is 2^16-1 */
typedef struct s_somewhat_rand_gen_t {
  hipsens_u16 lfsr_state;
} somewhat_rand_gen_t;

void somewhat_rand_init(somewhat_rand_gen_t* gen);

hipsens_u16 somewhat_rand_draw(somewhat_rand_gen_t* gen);

void somewhat_rand_seed(somewhat_rand_gen_t* gen,
			hipsens_u16 new_seed, 
			hipsens_bool accumulate);
					     
/*---------------------------------------------------------------------------*/

typedef byte address_t[ADDRESS_SIZE];

int address_equal(address_t* address1, address_t* address2);
int address_cmp(address_t* address1, address_t* address2);
void address_copy(address_t* toAddr, address_t* fromAddr);

/* XXX: these should be used instead of the ones given above */
int hipsens_address_equal(address_t address1, address_t address2);
int hipsens_address_cmp(address_t address1, address_t address2);
void hipsens_address_copy(address_t toAddr, address_t fromAddr);

#ifdef WITH_PRINTF
/*void printf_address(address_t* address);*/
void pyprintf_address(address_t* address);

/** output an address */
void address_write(outstream_t out, address_t address);
/** output an address as a python string */
void address_pywrite(outstream_t out, address_t address);


/** output a block of bytes */
void data_write(outstream_t out, byte* data, int data_size);
/** output a block of bytes as a python string */
void data_pywrite(outstream_t out, byte* data, int data_size);

/** output a set (in data) as a python list */
void bitset_pywrite(outstream_t out, byte* data, int max_bit);
#endif

/*---------------------------------------------------------------------------*/
// XXX: move to a better place

#define ENERGY_CLASS_LOW 0
#define ENERGY_CLASS_MEDIUM 1
#define ENERGY_CLASS_HIGH 2

#define ENERGY_CLASS_NB 3

#define ENERGY_RECEPTION 0
#define ENERGY_TRANSMISSION 1

/*--------------------------------------------------*/

#define OPERA_MAX_COMMAND_SIZE 10

typedef struct s_base_state_t {
  void* opaque_extra_info;
  void* opaque_opera; /* optionally point to opera_state_t when used */
  //XXX: base_config_t config;
  hipsens_time_t current_time;

#ifdef WITH_ENERGY
  hipsens_u8 energy_class;
  hipsens_u16 cfg_energy_coef[2][ENERGY_CLASS_NB];
#endif /* WITH_ENERGY */

#ifdef WITH_FILE_IO
  FILE* log;
#else
  int log; /*not used*/
#endif /* WITH_FILE_IO */
    
#ifdef WITH_OPERA_SYSTEM_INFO
  hipsens_u16 sys_info;
  hipsens_u8 sys_info_color;
  hipsens_u8 sys_info_stability;
  hipsens_u8 sys_command[OPERA_MAX_COMMAND_SIZE];
#endif /* WITH_OPERA_SYSTEM_INFO */

#ifdef WITH_OPERA_INPACKET_MSG
  char str_info[30];
  int int_info;
#endif
  
  hipsens_u8 warning_count;
  hipsens_u8 error_count;
} base_state_t;

void base_state_init(base_state_t* state, void* opaque_extra_info);
void base_state_abort(base_state_t* state);

hipsens_time_t base_state_time_after_delay_jitter
(base_state_t* base_state, hipsens_time_t delay, hipsens_time_t jitter);

/*---------------------------------------------------------------------------*/

#define DEFAULT_JITTER_MILLISEC 500 /* millisec */

extern address_t undefined_address;
extern address_t broadcast_address;

#ifdef WITH_OPERA_SYSTEM_INFO
#define OPERA_SYSTEM_INFO_HAS_BROADCAST_OVERFLOW (1<<8)
#define OPERA_SYSTEM_INFO_HAS_COLORING_MODE_REQUEST (1<<7)
#define OPERA_SYSTEM_INFO_HAS_MAX_COLOR_REQUEST (1<<6)
#define OPERA_SYSTEM_INFO_HAS_MAX_COLOR_RESPONSE (1<<5)
#define OPERA_SYSTEM_INFO_HAS_COLORING_MODE_INDICATION (1<<4)
#define OPERA_SYSTEM_INFO_HAS_MAX_COLOR_INDICATION (1<<3)
#define OPERA_SYSTEM_INFO_COLORING_MODE (1<<2)
#define OPERA_SYSTEM_INFO_HAS_WARNING (1<<1)
#define OPERA_SYSTEM_INFO_HAS_ERROR (1<<0)
#endif /* WITH_OPERA_SYSTEM_INFO */

#ifdef WITH_OPERA_INPACKET_MSG
void base_state_set_info(base_state_t* state, hipsens_u8 info_type, 
                         char const __code* str_info, int int_info);
#endif /* WITH_OPERA_INPACKET_MSG */

/*---------------------------------------------------------------------------*/

#endif /* _HIPSENS_BASE_H */
