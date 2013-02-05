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

/*---------------------------------------------------------------------------*/

#include <string.h>

#include "hipsens-config.h"
#include "hipsens-base.h"
#include "hipsens-all.h" //added-ridha
/*---------------------------------------------------------------------------*/

hipsens_time_t undefined_time = -11; //XXX: arbitrary //SEC_TO_HIPSENS_TIME(0);
address_t undefined_address = { 0xFE, 0xFE }; /* XXX:TODO assuming all bytes will be set to 0xFE */
address_t broadcast_address = { 0xFF, 0xFF, }; /* 0xFF, 0xFF */

/*---------------------------------------------------------------------------*/
/*                                 Buffers                                   */
/*---------------------------------------------------------------------------*/

void buffer_init(buffer_t* buffer, byte* data, int size) 
{
  buffer->data = data;
  buffer->size = size;
  buffer->pos = 0;
  buffer->status = ND_OK;
}

void buffer_put_byte(buffer_t* buffer, byte data)
{
  if (buffer->pos < buffer->size) {
    buffer->data[buffer->pos] = data;
    (buffer->pos)++;
  } else buffer->status = ND_ERROR;
}

byte buffer_get_byte(buffer_t* buffer)
{
  if (buffer->pos < buffer->size) {
    byte result = buffer->data[buffer->pos];
    (buffer->pos)++;
    return result;
  } else {
    buffer->status = ND_ERROR;
    return 0; /* default value in case of overflow*/
  }
}

#define SHORT_SIZE 2 

void buffer_put_short(buffer_t* buffer, unsigned int data)
{
  if (buffer->pos + SHORT_SIZE <= buffer->size) {
    buffer->data[buffer->pos+1] = data&0xff;
    buffer->data[buffer->pos] = (data>>8) & 0xff;
    (buffer->pos)+=2;
  } else buffer->status = ND_ERROR;
}

unsigned int buffer_get_short(buffer_t* buffer)
{
  if (buffer->pos + SHORT_SIZE <= buffer->size) {
    int result = (buffer->data[buffer->pos+1])
      + (buffer->data[buffer->pos]<<8);
    (buffer->pos) += 2;
    return result;
  } else {
    buffer->status = ND_ERROR;
    return 0; /* default value in case of overflow*/
  }
}

void buffer_put_data(buffer_t* buffer, byte* data, int size)
{
  if (buffer->pos + size <= buffer->size) {
    memcpy(buffer->data + buffer->pos, data, size);
    (buffer->pos) += size;
  } else buffer->status = ND_ERROR;

}

void buffer_get_data(buffer_t* buffer, byte* data, int size)
{
  if (buffer->pos + size <= buffer->size) {
    memcpy(data, buffer->data + buffer->pos, size);
    (buffer->pos) += size;
  } else {
    buffer->status = ND_ERROR;
    memset(data, 0, size); /* default value in case of overflow*/
  }
}

#ifdef WITH_LONG_PRIORITY
void buffer_put_u32(buffer_t* buffer, hipsens_u32 data)
{
  if (buffer->pos + 4 <= buffer->size) {
    buffer->data[buffer->pos+3] = data&0xff;
    buffer->data[buffer->pos+2] = (data>>8) & 0xff;
    buffer->data[buffer->pos+1] = (data>>16) & 0xff;
    buffer->data[buffer->pos] = (data>>24) & 0xff;
    (buffer->pos)+=4;
  } else buffer->status = ND_ERROR;
}

hipsens_u32 buffer_get_u32(buffer_t* buffer)
{
  if (buffer->pos + 4 <= buffer->size) {
    hipsens_u32 result = (buffer->data[buffer->pos+3])
      + (buffer->data[buffer->pos+2]<<8)
      + (buffer->data[buffer->pos+1]<<16)
      + (buffer->data[buffer->pos]<<24);
    (buffer->pos) += 4;
    return result;
  } else {
    buffer->status = ND_ERROR;
    return 0; /* default value in case of overflow*/
  }
}
#endif /* WITH_LONG_PRIORITY */

/*---------------------------------------------------------------------------*/
/*                                  Utils                                    */
/*---------------------------------------------------------------------------*/

int hipsens_address_equal(address_t address1, address_t address2)
{ return memcmp(address1, address2, ADDRESS_SIZE) == 0; }

int hipsens_address_cmp(address_t address1, address_t address2)
{ return memcmp(address1, address2, ADDRESS_SIZE); }

void hipsens_address_copy(address_t toAddr, address_t fromAddr)
{ memcpy(toAddr, fromAddr, ADDRESS_SIZE); }


int address_equal(address_t* address1, address_t* address2)
{ return memcmp(*address1, *address2, ADDRESS_SIZE) == 0; }

int address_cmp(address_t* address1, address_t* address2)
{ return memcmp(*address1, *address2, ADDRESS_SIZE); }

void address_copy(address_t* toAddr, address_t* fromAddr)
{ memcpy(*toAddr, *fromAddr, ADDRESS_SIZE); }



//------------------------------------------------------------------------!OCARI
#ifdef WITH_PRINTF
/* XXX: should be removed */
void printf_address(address_t* address)
{ 
  int i;
  printf("<");
  for (i=0; i<ADDRESS_SIZE; i++)
    printf("%02x", (*address)[i]);
  printf(">");
}

/* XXX: should be removed */
void pyprintf_address(address_t* address)
{ 
  int i;
  printf("'");
  for (i=0; i<ADDRESS_SIZE; i++)
    printf("\\x%02x", (*address)[i]);
  printf("'");
}

void address_write(outstream_t out, address_t address)
{
  int i;
  FPRINTF(out, "<");
  for (i=0; i<ADDRESS_SIZE; i++)
    FPRINTF(out, "%02x", address[i]);
  FPRINTF(out, ">");
}

#ifdef WITH_SIMUL_ADDR
int simul_address_to_id(address_t address); /* in hipsens-simul.c */
void address_pywrite(outstream_t out, address_t address)
{ FPRINTF(out, "%d", simul_address_to_id(address)); }
#else
void address_pywrite(outstream_t out, address_t address)
{
  int i;
  FPRINTF(out, "ADDR('");
  for (i=0; i<ADDRESS_SIZE; i++)
    FPRINTF(out, "%02x", address[i]);
  FPRINTF(out, "')");
}
#endif /* WITH_SIMUL_ADDR */

void data_write(outstream_t out, byte* data, int data_size)
{
  int i;
  for (i=0; i<data_size; i++)
    FPRINTF(out, "%02x ", data[i]);
}

void data_pywrite(outstream_t out, byte* data, int data_size)
{
  int i;
  FPRINTF(out,"'");
  for (i=0; i<data_size; i++)
    FPRINTF(out, "\\x%02x", data[i]);
  FPRINTF(out, "'");
}


void bitset_pywrite(outstream_t out, byte* data, int max_bit)
{
  int i;
  byte first = HIPSENS_TRUE;
  FPRINTF(out,"[");
  for (i=0;i<max_bit;i++)
    if (data[BYTE_OF_BIT(i)] 
	& (unsigned int)MASK_OF_INDEX(INDEX_OF_BIT(i))) {
      if (first) first=0;
      else FPRINTF(out, ",");
      FPRINTF(out, "%d",i);
    }
  FPRINTF(out,"]");
}

#endif
//-----------------------------------------------------------------------------!



/*--------------------------------------------------*/

#ifndef WITH_SIMUL
extern opera_state_t opera;
#endif

/* XXX: remove */
int hipsens_fatal(void* state, const char* message)
{ 
#ifndef WITH_SIMUL
  opera.base->error_count++;
#endif
#ifdef WITH_PRINTF
  printf("%s\n", message);
#endif
#ifndef IS_EMBEDDED
  abort();
#endif /* IS_EMBEDDED */
  return 0;
}
/*ridha
*/
void hipsens_abort(void);

#ifdef USB_UART
extern void hipsens_cc2531_abort(void);
#endif

void hipsens_abort(void)
{
#ifndef IS_EMBEDDED
  abort();
#else
  
#ifdef USB_UART
  hipsens_cc2531_abort();
#else
  // DON'T LOOP FOREVER
  // for (;;) /* loop forever */ ;
#endif /* USB_UART */
  
#endif /* IS_EMBEDDED */
}

/*--------------------------------------------------*/


//------------------------------------------------------------------------!OCARI

typedef struct {
  int error_count;
  int warn_count;
} error_counter;

error_counter global_error_counter = {0,0};

#ifdef WITH_FILE_IO

#ifndef SWIG /* swig generates a warning for whose */
FILE* default_out_file = NULL;
FILE* default_err_file = NULL;
#endif /*SWIG*/

void hipsens_set_default_output(FILE* out, FILE* err)
{ 
  default_out_file = out;
  default_err_file = out;
}

void hipsens_set_no_output(void)
{ 
 FILE* out = fopen("/dev/null", "w"); /* only for Unix systems */
  if (out == NULL) {
    FPRINTF(stderr, "Fatal: cannot open /dev/null (%s)\n",
	    strerror(errno));
    hipsens_abort();
  }
  if (default_out_file != NULL && default_out_file != stdout
      && default_out_file != stderr && default_out_file != default_err_file)
    fclose(default_out_file);

  if (default_err_file != NULL && default_err_file != stdout
      && default_err_file != stderr)
    fclose(default_err_file);

  default_out_file = out;
  default_err_file = out;
}

#endif /* WITH_FILE_IO */
//-----------------------------------------------------------------------------!

/*---------------------------------------------------------------------------*/

/* 
   Code for 16 bit Fibonacci LFSRs from Wikipedia
   <http://en.wikipedia.org/wiki/Linear_feedback_shift_register>

   This is a HORRIBLE random generator. For good ones refer for instance to
   "TestU01: A C Library for Empirical Testing of RNGs"
   P. L'Ecuyer and R. Simard, ACM Trans. Math. Soft. Aug 2007
 */

void somewhat_rand_init(somewhat_rand_gen_t* gen)
{ gen->lfsr_state = 0XACE1u; }

void somewhat_rand_seed(somewhat_rand_gen_t* gen,
			hipsens_u16 new_seed, 
			hipsens_bool accumulate)
{ 
  if (accumulate) {
    hipsens_u16 unused = somewhat_rand_draw(gen);
    UNUSED(unused);
    gen->lfsr_state ^= new_seed;
  } else gen->lfsr_state = new_seed;
  
  if (gen->lfsr_state == 0xffffu)
    gen->lfsr_state = 0;
}

hipsens_u16 somewhat_rand_draw(somewhat_rand_gen_t* gen)
{
  /* taps: 16 14 13 11; characteristic polynomial: 
     x^16 + x^14 + x^13 + x^11 + 1 */
  hipsens_u16 lfsr = gen->lfsr_state;
  hipsens_u16 bit = (((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) 
		     & 1) ^ 1;
  gen->lfsr_state = (lfsr >> 1) | (bit << 15);
  return gen->lfsr_state;
}

/*---------------------------------------------------------------------------*/
/*                       Time management functions                           */
/*---------------------------------------------------------------------------*/

/* copied from OOLSR/src/general.cc (C) 2003-2005
   - modified to remove floating point operations
   - sticking to the algorithm in RFC 3626 (Page 64) as I wrote it */

#ifdef WITH_FRAME_TIME
const hipsens_time_t Cref = 16; /* expressed in 1/16 of cycle */
#define DIV_BY_C(x) ((x)/Cref)
#else
const hipsens_time_t Cref = SEC_TO_HIPSENS_TIME(1) / 16; /* 1/16 second */
#define DIV_BY_C(x) ((x)/Cref)
#endif

const hipsens_u8 MaxVTime = 0xffu;
const hipsens_u8 MinVTime = 0x00u;

hipsens_u8 hipsens_time_to_vtime(hipsens_time_t clock)
{  
  int b=0;
  /* this is a full 'round up', any value which is slightly above than 
   the value is rounded up ; use 'C/2' for >=0.5 rounding */
  hipsens_time_t round_up = Cref-1;
  hipsens_time_t relative_time = DIV_BY_C(16*clock + round_up);

  while( relative_time >= (1<<(b+1)) ) /* @@3595 */
    b++;

  if (b < 4) /* too small */
    return MinVTime;
  b -= 4;

  hipsens_time_t a = (relative_time - (1<<(b+4))) >> b;

  if(a==16) {
    b++;
    a = 0;
  }

  if (b>15) 
    return MaxVTime; /* too large to fit */

  return a*16+b; /* @@3603 */
}


hipsens_time_t vtime_to_hipsens_time(hipsens_u8 vtime)
{
  hipsens_time_t a = (vtime>>4); /* @@3571-3572 */
  int b = (vtime&0xf); /* @@3572-3573 */
  return (Cref*((16+a)<<b))/16; /* @@3569 */
}


hipsens_bool hipsens_time_to_min(hipsens_time_t* current_time, 
				 hipsens_time_t new_time) 
{
  if (HIPSENS_TIME_COMPARE_LARGE_UNDEF(*current_time, <=, new_time))
    return HIPSENS_FALSE;

  *current_time = new_time;
  return HIPSENS_TRUE;
}

hipsens_time_t hipsens_time_min(hipsens_time_t t1, hipsens_time_t t2)
{ 
  if (HIPSENS_TIME_COMPARE_LARGE_UNDEF(t1, <=, t2))
    return t1;
  else return t2;
}

hipsens_s8 hipsens_time_cmp_no_undef(hipsens_time_t t1, hipsens_time_t t2)
{
  if (t1 < t2) return -1;
  else if (t1 > t2) return +1;
  else return 0;
}

hipsens_s8 hipsens_time_cmp(hipsens_time_t t1, hipsens_time_t t2,
				hipsens_bool time_undef_is_large)
{
  if (t1 == undefined_time) {
    if (t2 == undefined_time) 
      return 0;
    else if (time_undef_is_large)
      return +1;
    else return -1;
  } else if (t2 == undefined_time) {
    if (time_undef_is_large)
      return -1;
    else return +1;
  }

  ASSERT( t1 != undefined_time && t2 != undefined_time );
  return hipsens_time_cmp_no_undef(t1, t2);
}

void wakeup_condition_init(hipsens_wakeup_condition_t* condition)
{
  condition->wakeup_time_buffer = undefined_time;
  condition->wakeup_time = undefined_time;
}

int wakeup_condition_update(hipsens_wakeup_condition_t* condition,
			    hipsens_wakeup_condition_t* more_condition)
{
  int result = 0;
  if (hipsens_time_to_min(&condition->wakeup_time_buffer,
			  more_condition->wakeup_time_buffer))
    result |= FLAG_TIME_WITH_BUFFER;
  if (hipsens_time_to_min(&condition->wakeup_time, more_condition->wakeup_time))
    result |= FLAG_TIME_WITHOUT_BUFFER;
  return result;
}

void wakeup_condition_ensure_minimum_time(hipsens_wakeup_condition_t* condition,
					  hipsens_time_t minimum_time)
{
  if (condition->wakeup_time != undefined_time 
      && (HIPSENS_TIME_COMPARE_SMALL_UNDEF
	  (condition->wakeup_time, <, minimum_time)))
    condition->wakeup_time = minimum_time;
  if (condition->wakeup_time_buffer != undefined_time
      && (HIPSENS_TIME_COMPARE_SMALL_UNDEF
	  (condition->wakeup_time_buffer, <, minimum_time)))
    condition->wakeup_time_buffer = minimum_time;
}

/*---------------------------------------------------------------------------*/
/*                             Base state                                    */
/*---------------------------------------------------------------------------*/

void base_state_init(base_state_t* state, void* opaque_extra_info)
{
  state->opaque_extra_info = opaque_extra_info;
  state->opaque_opera = NULL;
  state->current_time = 0;

#ifdef WITH_FILE_IO
  if (default_out_file != NULL)
    state->log = default_out_file;
  else state->log = stdout;
#endif /* WITH_FILE_LOG */
  state->warning_count = 0;
  state->error_count = 0;

#ifdef WITH_OPERA_SYSTEM_INFO
  state->sys_info = 0;
  state->sys_info_color = 0;
  state->sys_info_stability = 0;
  state->sys_command[0] = 0;
  memset(state->str_info, 0, sizeof(state->str_info));
#endif /* WITH_OPERA_SYSTEM_INFO */  
  
#ifdef WITH_OPERA_INPACKET_MSG
  state->int_info = 0;
  state->str_info[0] = 0;
  memset(state->str_info, 0, sizeof(state->str_info));
#endif /* WITH_OPERA_INPACKET_MSG */  
  
#ifdef WITH_ENERGY
  int i;
  state->energy_class = ENERGY_CLASS_HIGH;
  for (i=0; i<ENERGY_CLASS_NB; i++) {
    state->cfg_energy_coef[ENERGY_RECEPTION][i] = 0;
    state->cfg_energy_coef[ENERGY_TRANSMISSION][i] 
      = 1<<(4*(ENERGY_CLASS_NB - 1 -i));
  }
#endif /* WITH_ENERGY */
}

void base_state_abort(base_state_t* state)
{ hipsens_abort(); }

#ifdef WITH_OPERA_INPACKET_MSG
#warning "[CA] remove WITH_OPERA_INPACKET_MSG in production code (consumes [flash] memory)"
void base_state_set_info(base_state_t* state, hipsens_u8 info_type, 
                         char const __code* str_info, int int_info)
{
  memset(state->str_info, 0, sizeof(state->str_info));
  state->int_info = int_info;
  if (str_info != NULL) {
    int i = 0; 
    while(*str_info != 0) {
      state->str_info[i] = *str_info;
      str_info ++;
      i = i+1;
      if (i >= sizeof(state->str_info))
        i = 0;
    }
    state->str_info[i] = 0;
  }
}
#endif /* WITH_OPERA_INPACKET_MSG */

hipsens_time_t base_state_time_after_delay_jitter
(base_state_t* base_state, hipsens_time_t delay, hipsens_time_t jitter)
{
#ifndef WITH_CONTIKI
  /* XXX: actually generate jitter */
  return base_state->current_time + delay;
#else
  extern hipsens_time_t max_jitter;
  return base_state->current_time + delay 
    - (((random_rand()&0xff)*max_jitter)>>8);
#endif
}

/*---------------------------------------------------------------------------*/
