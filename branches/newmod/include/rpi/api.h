/*
 * This file is in the public domain.
 * Edward Brocklesby <ejb@lythe.org.uk>.
 * $Id$
 */

#ifndef RPI_H_INCLUDED
#define RPI_H_INCLUDED

#ifdef _WIN32
# define TPI_CALL __stdcall
# define TPI_EXPORT __declspec(dllexport)
#else
# define TPI_CALL
# define TPI_EXPORT
#endif

#ifdef __cplusplus
# include <cstdlib>
# include <cstdarg>
# include <ctime>
# include <cstdarg>
# define STD(x) std::x
# ifndef TPI_NO_CPP
#  define _tpi_want_cpp
#  include <vector>
#  include <string>
# endif
#else
# include <stdlib.h>
# include <stdarg.h>
# include <time.h>
# include <stdarg.h>
# define STD(x) x
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int rpi_bool;

#ifdef __cplusplus
static int const rpi_true = 1;
static int const rpi_false = 0;
#else
# define rpi_true 1
# define rpi_false 0
#endif

/* This is an implementation detail and shouldn't be here. */
typedef void (*rpi_bsfn) (void);
typedef rpi_bsfn (*rpi_lookup_fn) (char const *);

#define RPI_INHERIT(x) x _rpi_##x##_base

// Cast to absolute base (t = x's type)
#define RPI_BASECAST(x,t) ((_rpi_base*) (((char *)(x)) - t##_offset))

// Cast to direct base object (t = base's type)
#define RPI_UPCAST(x,t) ((t*) &((x)->_rpi_##t##_base))

// Cast to derived from direct base (only) (t = target's type)
#define RPI_DOWNCAST(x,t) ((t*) (((char *)(x)) + t##_offset))

// Cast from any X to any Y (does not check validity!)
#define RPI_ANYCAST(from_t,to_t,x) RPI_DOWNCAST(RPI_BASECAST(x,from_t),to_t)

typedef void *		(*rpi_new_t)		(char const *type, int offset, ...);
typedef void *		(*rpi_delete_t)		(void *);
typedef void *		(*rpi_lookup_t)		(char const *type, void *key);

typedef void *		 (*rpi_ctor)		(int, va_list);
typedef void 		 (*rpi_dtor) 		(void *);

/* hmm.. these are internal .. they should go away once everything is modularised */
void *new(char const*, int, ...);
void delete(void*);
rpi_bool register_class(char const*, rpi_ctor, rpi_dtor);


typedef struct _rpi_base_vtable {
	int		_rpi_rtti_id;

	/* These are internal functions, do not depend on them. */
	void			(*_rpi_dec_ref)		(void *);
	void		 	(*_rpi_inc_ref) 	(void *);
	void			(*_rpi_destroy)		(void *);
} _rpi_base_vtable;

typedef struct _rpi_base {
	_rpi_base_vtable	*v;
	int					 _rpi_ref_count;
} _rpi_base;

#define RPI_STATIC 0x1

#define RPI_INIT_LOADING 	0
#define RPI_INIT_UNLOADING 	1
#define RPI_DECLARE_FN(fn) extern fn ## _t fn

/*
 * Functions added here must also be added in rpi/rpi.c !!
 */
RPI_DECLARE_FN(rpi_new);
RPI_DECLARE_FN(rpi_delete);

#ifdef __cplusplus
}
#endif

#endif
