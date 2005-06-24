/*
 * This file is in the public domain.
 * Edward Brocklesby <ejb@lythe.org.uk>.
 *
 * $Id$
 */

#include "temple/tpilib/api.h"

#define IMPORT_FN(fn) fn = (fn ## _t)(lf(# fn))
#define DEFINE_FN(fn) fn ## _t fn

DEFINE_FN(rpi_new);
DEFINE_FN(rpi_delete);

/*
 * This is called by the TPI module loader when the module is loaded.
 */
extern "C" void rpi_register_api (rpi_lookup_fn lf)
{
	IMPORT_FN(rpi_new);
	IMPORT_FN(rpi_delete);
}
