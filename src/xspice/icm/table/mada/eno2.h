/* This file is automatically generated. DO NOT EDIT! */

#ifndef _sf_eno2_h
#define _sf_eno2_h


#include "eno.h"


typedef struct Eno2 *sf_eno2;
/* abstract data type */


sf_eno2 sf_eno2_init (int order,     /* interpolation order */
                      int n1, int n2 /* data dimensions */);
/*< Initialize interpolation object >*/


void sf_eno2_set (sf_eno2 pnt, double **c /* data [n2][n1] */);
/*< Set the interpolation table. c can be changed or freed afterwords. >*/


void sf_eno2_set1 (sf_eno2 pnt, double *c /* data [n2*n1] */);
/*< Set the interpolation table. c can be changed or freed afterwords. >*/


void sf_eno2_set1_wstride (sf_eno2 pnt, double *c /* data [n2*n1] */, int stride);
/*< Set the interpolation table. c can be changed or freed afterwords. >*/


void sf_eno2_close (sf_eno2 pnt);
/*< Free internal storage >*/


void sf_eno2_apply (sf_eno2 pnt,
                    int i, int j,       /* grid location */
                    double x, double y, /* offset from grid */
                    double *f,          /* output data value */
                    double *f1,         /* output derivative [2] */
                    der what            /* what to compute [FUNC,DER,BOTH] */);
/*< Apply interpolation. >*/

#endif
