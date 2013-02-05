/*---------------------------------------------------------------------------
 *              OPERA - Various Macros (assert/print/log/...)
 *---------------------------------------------------------------------------
 * Author: Cedric Adjih
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

#ifndef _HIPSENS_MACRO_H
#define _HIPSENS_MACRO_H

/*---------------------------------------------------------------------------*/

#define BEGIN_MACRO do {
#define END_MACRO } while(__LINE__==-1)

#define UNUSED(x) do { } while(__LINE__==-1 && (&(x) != &(x)+1))


/* #define HIPSENS_MAX(x,y) ((x)>(y) ? (x) : (y)) */

/*---------------------------------------------------------------------------
 * If not already defined, debugging flags are set to default 0
 *---------------------------------------------------------------------------*/

#ifndef DBGnd
#define DBGnd 0
#endif

#ifndef DBGstc
#define DBGstc 0
#endif

#ifndef DBGmsg
#define DBGmsg 0
#endif

#ifndef DBGmsgdat
#define DBGmsgdat 0
#endif

#ifndef DBGsim
#define DBGsim 0
#endif

#ifndef DBGsimmsg
#define DBGsimmsg 0
#endif

#ifndef DBGsrn
#define DBGsrn 0
#endif

#ifndef DBGnode
#define DBGnode 0
#endif

/*---------------------------------------------------------------------------
 * Logging, debugging and formatting
 *---------------------------------------------------------------------------*/

#ifndef OUTLOG
#define OUTLOG ( (default_err_file!=NULL) ? default_err_file : stdout)
#endif

#ifndef OUTERR
#define OUTERR ( (default_out_file!=NULL) ? default_out_file : stderr)
#endif

/*---------- LOG and PRINTF macros */

#if defined(WITH_FILE_IO) /* -------------------- */

#define LOG(base_state, should_log, ...) BEGIN_MACRO	\
  if ((should_log) && (base_state)->log != NULL) {	\
    fprintf((base_state)->log, __VA_ARGS__);		\
  }							\
  END_MACRO

#define PRINTF(...)  printf(__VA_ARGS__)
#define FPRINTF(...) fprintf(__VA_ARGS__)
typedef FILE* outstream_t;

#ifndef WITH_PRINTF
#error "WITH_FILE_IO defined, but WITH_PRINTF is not defined"
#endif /* WITH_PRINTF */

#elif defined(WITH_PRINTF) /* -------------------- */

#define LOG(base_state, should_log, ...) BEGIN_MACRO	\
  if (should_log) {					\
    printf(__VA_ARGS__);				\
  }							\
  END_MACRO
#define PRINTF(...)         printf(__VA_ARGS__)
#define FPRINTF(unused,...) printf(__VA_ARGS__)
typedef unsigned char outstream_t; /* not used */

#else 

#define LOG(base_state, should_log, ...) BEGIN_MACRO END_MACRO
#define PRINTF(...) BEGIN_MACRO END_MACRO
#define FPRINTF(...) BEGIN_MACRO END_MACRO
typedef unsigned char outstream_t; /* not used */

#endif /* WITH_FILE_IO */ /* -------------------- */

/*--------------------------------------------------*/

#ifdef WITH_OPERA_INPACKET_MSG
#define SET_WARNING(state) base_state_set_info(state, 0, __FILE__, __LINE__);
#else /* --- */
#define SET_WARNING(state) 
#endif /* WITH_OPERA_SYSTEM_INFO */

#define WARN(base_state, ...) BEGIN_MACRO			\
  (base_state)->warning_count ++;				\
  SET_WARNING(base_state)                                       \
  FPRINTF(OUTERR, "warning (%s:%d): ", __func__, __LINE__);	\
  FPRINTF(OUTERR, __VA_ARGS__);					\
  END_MACRO

#define FATAL(base_state, ...) BEGIN_MACRO		        \
  (base_state)->error_count ++;				        \
  FPRINTF(OUTERR, "fatal (%s:%d): ", __func__, __LINE__);	\
  FPRINTF(OUTERR, __VA_ARGS__);				        \
  base_state_abort(base_state); \
  END_MACRO


#define HIPSENS_WARN(...) BEGIN_MACRO		\
  FPRINTF(OUTERR, "warning (%s:%d): ", __func__, __LINE__);	\
  FPRINTF(OUTERR, __VA_ARGS__);					\
  END_MACRO

#define HIPSENS_FATAL(...) BEGIN_MACRO		\
  FPRINTF(OUTERR, "fatal (%s:%d): ", __func__, __LINE__);	\
  FPRINTF(OUTERR, __VA_ARGS__);				        \
  hipsens_abort();					\
  END_MACRO


#ifdef WITH_DBG
#define DBG(x) BEGIN_MACRO_DBG x; END_MACRO_DBG

#ifndef CC2530 /* XXX: not only */
#define ASSERT(x) assert(x)
#else /* !defined(CC2530) */
#define ASSERT(x) BEGIN_MACRO_DBG if (!(x)) { \
  FPRINTF(OUTLOG, "FATAL: (" __FILE__ ":%d) assertion failed: %s\n", \
  __LINE__, #x); hipsens_abort(); } END_MACRO_DBG
#endif /* CC2530 */

#else /* !WITH_DBG */
#define DBG(x)
#define ASSERT(x)
#endif /* WITH_DBG */


/* ----- DBG ----- */

#ifdef WITH_DBG 

#define BEGIN_MACRO_DBG BEGIN_MACRO
#define END_MACRO_DBG END_MACRO

#else /* !defined(WITH_DBG) */

#define BEGIN_MACRO_DBG BEGIN_MACRO if (0) {
#define END_MACRO_DBG   } END_MACRO

#endif /* WITH_DBG */


/* ----- LOG ----- */

#ifdef WITH_LOG

#define BEGIN_MACRO_LOG BEGIN_MACRO
#define END_MACRO_LOG END_MACRO

#else /* !defined(WITH_DBG) */

#define BEGIN_MACRO_LOG BEGIN_MACRO if (0) {
#define END_MACRO_LOG   } END_MACRO

#endif /* WITH_DBG */


/*--------------------------------------------------*/

#ifdef WITH_STAT
#define IFSTAT(x) BEGIN_MACRO x END_MACRO
#else /* !WITH_STAT */
#define IFSTAT(x)
#endif /* WITH_STAT */

/*---------------------------------------------------------------------------*/

/* XXX: check where used */
#define STDBG(should_log,...) BEGIN_MACRO	  \
  if (should_log) { __VA_ARGS__ ; }		  \
  END_MACRO

#define STLOG(should_log, ...)			\
    BEGIN_MACRO_LOG LOG(state->base, should_log, __VA_ARGS__); END_MACRO_LOG

#define STFATAL(...) FATAL(state->base, __VA_ARGS__)
#define STWARN(...)  WARN(state->base, __VA_ARGS__)

#if defined(WITH_FILE_IO)   /* ---------- */

#define STLOGA(should_log,...)						\
  BEGIN_MACRO_LOG							\
  if ((should_log) && state->base->log != NULL) {			\
    FPRINTF(state->base->log, FMT_HST " ", state->base->current_time);	\
    address_t _my_address;						\
    hipsens_api_get_my_address(state->base->opaque_extra_info, _my_address); \
    address_write(state->base->log, _my_address);			\
    FPRINTF(state->base->log, " ");					\
    LOG(state->base, HIPSENS_TRUE, __VA_ARGS__);			\
  } END_MACRO_LOG

#define STWRITE(should_log, write_func, ...)		\
  BEGIN_MACRO_LOG					\
  if ((should_log) && state->base->log != NULL) {	\
    (write_func)(state->base->log, __VA_ARGS__);	\
  }							\
  END_MACRO_LOG

#elif defined(WITH_PRINTF) /* ---------- */

#define STLOGA(should_log,...)						\
  BEGIN_MACRO_LOG							\
  if (should_log) {							\
    PRINTF("%d ", state->base->current_time);				\
    address_t _my_address;						\
    hipsens_api_get_my_address(state->base->opaque_extra_info, _my_address); \
    address_write(0/*unused*/, _my_address);			\
    LOG(state->base, HIPSENS_TRUE, __VA_ARGS__);			\
  } END_MACRO_LOG

#define STWRITE(should_log, write_func, ...)		\
  BEGIN_MACRO_LOG					\
  if (should_log) {					\
    (write_func)(0 /*unused*/, __VA_ARGS__);		\
  }							\
  END_MACRO_LOG 


#else /*!WITH_PRINTF*/      /* ---------- */

#define STLOGA(...) STLOG(__VA_ARGS__)
#define STWRITE(...) 

#endif /*WITH_PRINTF*/      /* ---------- */

/*---------------------------------------------------------------------------*/

#endif /* _HIPSENS_MACRO_H */
