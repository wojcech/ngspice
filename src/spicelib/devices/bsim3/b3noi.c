/**** BSIM3v3.3.0, Released by Xuemei Xi 07/29/2005 ****/

/**********
 * Copyright 2004 Regents of the University of California. All rights reserved.
 * File: b3noi.c of BSIM3v3.3.0
 * Author: 1995 Gary W. Ng and Min-Chie Jeng.
 * Author: 1997-1999 Weidong Liu.
 * Author: 2001 Xuemei Xi
 * Modified by Xuemei Xi, 10/05/2001.
 **********/

#include "ngspice.h"
#include "bsim3def.h"
#include "cktdefs.h"
#include "iferrmsg.h"
#include "noisedef.h"
#include "suffix.h"
#include "const.h"  /* jwan */

/*
 * BSIM3noise (mode, operation, firstModel, ckt, data, OnDens)
 *    This routine names and evaluates all of the noise sources
 *    associated with MOSFET's.  It starts with the model *firstModel and
 *    traverses all of its insts.  It then proceeds to any other models
 *    on the linked list.  The total output noise density generated by
 *    all of the MOSFET's is summed with the variable "OnDens".
 */

/*
 Channel thermal and flicker noises are calculated based on the value
 of model->BSIM3noiMod.
 If model->BSIM3noiMod = 1,
    Channel thermal noise = SPICE2 model
    Flicker noise         = SPICE2 model
 If model->BSIM3noiMod = 2,
    Channel thermal noise = BSIM3 model
    Flicker noise         = BSIM3 model
 If model->BSIM3noiMod = 3,
    Channel thermal noise = SPICE2 model
    Flicker noise         = BSIM3 model
 If model->BSIM3noiMod = 4,
    Channel thermal noise = BSIM3 model
    Flicker noise         = SPICE2 model
 If model->BSIM3noiMod = 5,
    Channel thermal noise = SPICE2 model with linear/sat fix
    Flicker noise         = SPICE2 model
 If model->BSIM3noiMod = 6,
    Channel thermal noise = SPICE2 model with linear/sat fix
    Flicker noise         = BSIM3 model
 */


/*
 * JX: 1/f noise model is smoothed out 12/18/01.
 */


static double
StrongInversionNoiseEval(
double Vds,
BSIM3model *model,
BSIM3instance *here,
double freq, double temp)
{
struct bsim3SizeDependParam *pParam;
double cd, esat, DelClm, EffFreq, N0, Nl, Leff, Leffsq;
double T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, Ssi;

    pParam = here->pParam;
    cd = fabs(here->BSIM3cd);
    Leff = pParam->BSIM3leff - 2.0 * model->BSIM3lintnoi;
    Leffsq = Leff * Leff;
    esat = 2.0 * pParam->BSIM3vsattemp / here->BSIM3ueff;
    if(model->BSIM3em<=0.0) DelClm = 0.0; 
    else { 
	    T0 = ((((Vds - here->BSIM3Vdseff) / pParam->BSIM3litl) 
	    	+ model->BSIM3em) / esat);
            DelClm = pParam->BSIM3litl * log (MAX(T0, N_MINLOG));
            if (DelClm < 0.0)       DelClm = 0.0;  /* bugfix */
    }
    EffFreq = pow(freq, model->BSIM3ef);
    T1 = CHARGE * CHARGE * 8.62e-5 * cd * temp * here->BSIM3ueff;
    T2 = 1.0e8 * EffFreq * here->BSIM3Abulk * model->BSIM3cox * Leffsq;
    N0 = model->BSIM3cox * here->BSIM3Vgsteff / CHARGE;
    Nl = model->BSIM3cox * here->BSIM3Vgsteff
    	  * (1.0 - here->BSIM3AbovVgst2Vtm * here->BSIM3Vdseff) / CHARGE;

    T3 = model->BSIM3oxideTrapDensityA
       * log(MAX(((N0 + 2.0e14) / (Nl + 2.0e14)), N_MINLOG));
    T4 = model->BSIM3oxideTrapDensityB * (N0 - Nl);
    T5 = model->BSIM3oxideTrapDensityC * 0.5 * (N0 * N0 - Nl * Nl);

    T6 = 8.62e-5 * temp * cd * cd;
    T7 = 1.0e8 * EffFreq * Leffsq * pParam->BSIM3weff;
    T8 = model->BSIM3oxideTrapDensityA + model->BSIM3oxideTrapDensityB * Nl
       + model->BSIM3oxideTrapDensityC * Nl * Nl;
    T9 = (Nl + 2.0e14) * (Nl + 2.0e14);

    Ssi = T1 / T2 * (T3 + T4 + T5) + T6 / T7 * DelClm * T8 / T9;
    return Ssi;
}

int
BSIM3noise (
int mode, int operation,
GENmodel *inModel,
CKTcircuit *ckt,
Ndata *data,
double *OnDens)
{
BSIM3model *model = (BSIM3model *)inModel;
BSIM3instance *here;
struct bsim3SizeDependParam *pParam;
char name[N_MXVLNTH];
double tempOnoise;
double tempInoise;
double noizDens[BSIM3NSRCS];
double lnNdens[BSIM3NSRCS];

double vds;

double T1, T10, T11;
double Ssi, Swi;

double m;

int i;

    /* define the names of the noise sources */
    static char *BSIM3nNames[BSIM3NSRCS] =
    {   /* Note that we have to keep the order */
	".rd",              /* noise due to rd */
			    /* consistent with the index definitions */
	".rs",              /* noise due to rs */
			    /* in BSIM3defs.h */
	".id",              /* noise due to id */
	".1overf",          /* flicker (1/f) noise */
	""                  /* total transistor noise */
    };

    for (; model != NULL; model = model->BSIM3nextModel)
    {    for (here = model->BSIM3instances; here != NULL;
	      here = here->BSIM3nextInstance)
	 {    pParam = here->pParam;
	      switch (operation)
	      {  case N_OPEN:
		     /* see if we have to to produce a summary report */
		     /* if so, name all the noise generators */

		      if (((NOISEAN*)ckt->CKTcurJob)->NStpsSm != 0)
		      {   switch (mode)
			  {  case N_DENS:
			          for (i = 0; i < BSIM3NSRCS; i++)
				  {    (void) sprintf(name, "onoise.%s%s",
					              here->BSIM3name,
						      BSIM3nNames[i]);
                                       data->namelist = (IFuid *) trealloc(
					     (char *) data->namelist,
					     (data->numPlots + 1)
					     * sizeof(IFuid));
                                       if (!data->namelist)
					   return(E_NOMEM);
		                       (*(SPfrontEnd->IFnewUid)) (ckt,
			                  &(data->namelist[data->numPlots++]),
			                  (IFuid) NULL, name, UID_OTHER,
					  NULL);
				       /* we've added one more plot */
			          }
			          break;
		             case INT_NOIZ:
			          for (i = 0; i < BSIM3NSRCS; i++)
				  {    (void) sprintf(name, "onoise_total.%s%s",
						      here->BSIM3name,
						      BSIM3nNames[i]);
                                       data->namelist = (IFuid *) trealloc(
					     (char *) data->namelist,
					     (data->numPlots + 1)
					     * sizeof(IFuid));
                                       if (!data->namelist)
					   return(E_NOMEM);
		                       (*(SPfrontEnd->IFnewUid)) (ckt,
			                  &(data->namelist[data->numPlots++]),
			                  (IFuid) NULL, name, UID_OTHER,
					  NULL);
				       /* we've added one more plot */

			               (void) sprintf(name, "inoise_total.%s%s",
						      here->BSIM3name,
						      BSIM3nNames[i]);
                                       data->namelist = (IFuid *) trealloc(
					     (char *) data->namelist,
					     (data->numPlots + 1)
					     * sizeof(IFuid));
                                       if (!data->namelist)
					   return(E_NOMEM);
		                       (*(SPfrontEnd->IFnewUid)) (ckt,
			                  &(data->namelist[data->numPlots++]),
			                  (IFuid) NULL, name, UID_OTHER,
					  NULL);
				       /* we've added one more plot */
			          }
			          break;
		          }
		      }
		      break;
	         case N_CALC:
		      m = here->BSIM3m;
		      switch (mode)
		      {  case N_DENS:
		              NevalSrc(&noizDens[BSIM3RDNOIZ],
				       &lnNdens[BSIM3RDNOIZ], ckt, THERMNOISE,
				       here->BSIM3dNodePrime, here->BSIM3dNode,
				       here->BSIM3drainConductance * m);

		              NevalSrc(&noizDens[BSIM3RSNOIZ],
				       &lnNdens[BSIM3RSNOIZ], ckt, THERMNOISE,
				       here->BSIM3sNodePrime, here->BSIM3sNode,
				       here->BSIM3sourceConductance * m);

                              switch( model->BSIM3noiMod )
			      {  case 1:
			         case 3:
                                     NevalSrc(&noizDens[BSIM3IDNOIZ],
                                              &lnNdens[BSIM3IDNOIZ], ckt, 
                                              THERMNOISE, here->BSIM3dNodePrime,
                                              here->BSIM3sNodePrime,
                                              2.0 * fabs(here->BSIM3gm
				              + here->BSIM3gds
                                              + here->BSIM3gmbs) / 3.0 * m);

				      break;
			         case 5:
			         case 6:
                                     vds = MIN(*(ckt->CKTstates[0] + here->BSIM3vds), here->BSIM3vdsat);
                                     NevalSrc(&noizDens[BSIM3IDNOIZ],
                                              &lnNdens[BSIM3IDNOIZ], ckt, 
                                              THERMNOISE, here->BSIM3dNodePrime,
                                              here->BSIM3sNodePrime,
                                              (3.0 - vds / here->BSIM3vdsat) 
                                              * fabs(here->BSIM3gm
				              + here->BSIM3gds
					      + here->BSIM3gmbs) / 3.0 * m);

				      break;
			         case 2:
			         case 4:
		                      NevalSrc(&noizDens[BSIM3IDNOIZ],
				               &lnNdens[BSIM3IDNOIZ], ckt,
					       THERMNOISE, here->BSIM3dNodePrime,
                                               here->BSIM3sNodePrime,
					       (m * here->BSIM3ueff
					       * fabs(here->BSIM3qinv)
					       / (pParam->BSIM3leff * pParam->BSIM3leff 
						+ here->BSIM3ueff *fabs(here->BSIM3qinv) 
						* here->BSIM3rds)));    /* bugfix */
				      break;
			      }
		              NevalSrc(&noizDens[BSIM3FLNOIZ], (double*) NULL,
				       ckt, N_GAIN, here->BSIM3dNodePrime,
				       here->BSIM3sNodePrime, (double) 0.0);

                              switch( model->BSIM3noiMod )
			      {  case 1:
			         case 4:
			         case 5:
			              noizDens[BSIM3FLNOIZ] *= m * model->BSIM3kf
					    * exp(model->BSIM3af
					    * log(MAX(fabs(here->BSIM3cd),
					    N_MINLOG)))
					    / (pow(data->freq, model->BSIM3ef)
					    * pParam->BSIM3leff
				            * pParam->BSIM3leff
					    * model->BSIM3cox);
				      break;
			         case 2:
			         case 3:
			         case 6:
		                      vds = *(ckt->CKTstates[0] + here->BSIM3vds);
			              if (vds < 0.0)
			              {   vds = -vds;
			              }
			              Ssi = StrongInversionNoiseEval(vds, model, 
			              		here, data->freq, ckt->CKTtemp);
				          T10 = model->BSIM3oxideTrapDensityA
					      * 8.62e-5 * ckt->CKTtemp;
		                          T11 = pParam->BSIM3weff
					      * pParam->BSIM3leff
				              * pow(data->freq, model->BSIM3ef)
				              * 4.0e36;
		                          Swi = T10 / T11 * here->BSIM3cd
				              * here->BSIM3cd;
				          T1 = Swi + Ssi;
				          if (T1 > 0.0)
                                              noizDens[BSIM3FLNOIZ] *= m * (Ssi * Swi) / T1; 
				          else
                                              noizDens[BSIM3FLNOIZ] *= 0.0;
				      break;
			      }

		              lnNdens[BSIM3FLNOIZ] =
				     log(MAX(noizDens[BSIM3FLNOIZ], N_MINLOG));

		              noizDens[BSIM3TOTNOIZ] = noizDens[BSIM3RDNOIZ]
						     + noizDens[BSIM3RSNOIZ]
						     + noizDens[BSIM3IDNOIZ]
						     + noizDens[BSIM3FLNOIZ];
		              lnNdens[BSIM3TOTNOIZ] = 
				     log(MAX(noizDens[BSIM3TOTNOIZ], N_MINLOG));

		              *OnDens += noizDens[BSIM3TOTNOIZ];

		              if (data->delFreq == 0.0)
			      {   /* if we haven't done any previous 
				     integration, we need to initialize our
				     "history" variables.
				    */

			          for (i = 0; i < BSIM3NSRCS; i++)
				  {    here->BSIM3nVar[LNLSTDENS][i] =
					     lnNdens[i];
			          }

			          /* clear out our integration variables
				     if it's the first pass
				   */
			          if (data->freq ==
				      ((NOISEAN*) ckt->CKTcurJob)->NstartFreq)
				  {   for (i = 0; i < BSIM3NSRCS; i++)
				      {    here->BSIM3nVar[OUTNOIZ][i] = 0.0;
				           here->BSIM3nVar[INNOIZ][i] = 0.0;
			              }
			          }
		              }
			      else
			      {   /* data->delFreq != 0.0,
				     we have to integrate.
				   */
			          for (i = 0; i < BSIM3NSRCS; i++)
				  {    if (i != BSIM3TOTNOIZ)
				       {   tempOnoise = Nintegrate(noizDens[i],
						lnNdens[i],
				                here->BSIM3nVar[LNLSTDENS][i],
						data);
				           tempInoise = Nintegrate(noizDens[i]
						* data->GainSqInv, lnNdens[i]
						+ data->lnGainInv,
				                here->BSIM3nVar[LNLSTDENS][i]
						+ data->lnGainInv, data);
				           here->BSIM3nVar[LNLSTDENS][i] =
						lnNdens[i];
				           data->outNoiz += tempOnoise;
				           data->inNoise += tempInoise;
				           if (((NOISEAN*)
					       ckt->CKTcurJob)->NStpsSm != 0)
					   {   here->BSIM3nVar[OUTNOIZ][i]
						     += tempOnoise;
				               here->BSIM3nVar[OUTNOIZ][BSIM3TOTNOIZ]
						     += tempOnoise;
				               here->BSIM3nVar[INNOIZ][i]
						     += tempInoise;
				               here->BSIM3nVar[INNOIZ][BSIM3TOTNOIZ]
						     += tempInoise;
                                           }
			               }
			          }
		              }
		              if (data->prtSummary)
			      {   for (i = 0; i < BSIM3NSRCS; i++)
				  {    /* print a summary report */
			               data->outpVector[data->outNumber++]
					     = noizDens[i];
			          }
		              }
		              break;
		         case INT_NOIZ:
			      /* already calculated, just output */
		              if (((NOISEAN*)ckt->CKTcurJob)->NStpsSm != 0)
			      {   for (i = 0; i < BSIM3NSRCS; i++)
				  {    data->outpVector[data->outNumber++]
					     = here->BSIM3nVar[OUTNOIZ][i];
			               data->outpVector[data->outNumber++]
					     = here->BSIM3nVar[INNOIZ][i];
			          }
		              }
		              break;
		      }
		      break;
	         case N_CLOSE:
		      /* do nothing, the main calling routine will close */
		      return (OK);
		      break;   /* the plots */
	      }       /* switch (operation) */
	 }    /* for here */
    }    /* for model */

    return(OK);
}



