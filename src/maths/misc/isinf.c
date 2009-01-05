#include "ngspice.h"

#if !HAVE_DECL_ISINF
#ifndef HAVE_ISINF
#ifdef HAVE_IEEEFP_H

int isinf(double x) { return !finite(x) && x==x; }

#else /* HAVE_IEEEFP_H */

/* this is really ugly - but it is a emergency case */

static int
isinf (const double x)
{
  double y = x - x;
  int s = (y != y);

  if (s && x > 0)
    return +1;
  else if (s && x < 0)
    return -1;
  else
    return 0;
}

#endif /* HAVE_IEEEFP_H */
#else /* HAVE_ISINF */
int Dummy_Symbol_5;
#endif /* HAVE_ISINF */
#endif /* HAVE_DECL_ISINF */
