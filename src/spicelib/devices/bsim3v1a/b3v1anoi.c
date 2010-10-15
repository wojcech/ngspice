/**********
Copyright 1990 Regents of the University of California.  All rights reserved.
Author: 1995 Gary W. Ng and Min-Chie Jeng.
Modified by Paolo Nenzi 2002
File:  b3v1anoi.c
**********/

#include "ngspice.h"
#include "bsim3v1adef.h"
#include "cktdefs.h"
#include "iferrmsg.h"
#include "noisedef.h"
#include "suffix.h"
#include "const.h"  /* jwan */

/*
 * BSIM3v1Anoise (mode, operation, firstModel, ckt, data, OnDens)
 *    This routine names and evaluates all of the noise sources
 *    associated with MOSFET's.  It starts with the model *firstModel and
 *    traverses all of its insts.  It then proceeds to any other models
 *    on the linked list.  The total output noise density generated by
 *    all of the MOSFET's is summed with the variable "OnDens".
 */


static double
StrongInversionNoiseEval_b3v1a(double vgs, double vds, BSIM3v1Amodel *model, 
                         BSIM3v1Ainstance *here, double freq, double temp)
{
struct bsim3v1aSizeDependParam *pParam;
double cd, esat, DelClm, EffFreq, N0, Nl, Vgst;
double T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, Ssi;

    pParam = here->pParam;
    cd = fabs(here->BSIM3v1Acd) * here->BSIM3v1Am;
    if (vds > here->BSIM3v1Avdsat)
    {   esat = 2.0 * pParam->BSIM3v1Avsattemp / here->BSIM3v1Aueff;
	T0 = ((((vds - here->BSIM3v1Avdsat) / pParam->BSIM3v1Alitl) + model->BSIM3v1Aem)
	   / esat);
        DelClm = pParam->BSIM3v1Alitl * log (MAX(T0, N_MINLOG));
    }
    else 
        DelClm = 0.0;
    EffFreq = pow(freq, model->BSIM3v1Aef);
    T1 = CHARGE * CHARGE * 8.62e-5 * cd * (temp + CONSTCtoK) * here->BSIM3v1Aueff;
    T2 = 1.0e8 * EffFreq * model->BSIM3v1Acox
       * pParam->BSIM3v1Aleff * pParam->BSIM3v1Aleff;
    Vgst = vgs - here->BSIM3v1Avon;
    N0 = model->BSIM3v1Acox * Vgst / CHARGE;
    if (N0 < 0.0)
	N0 = 0.0;
    Nl = model->BSIM3v1Acox * (Vgst - MIN(vds, here->BSIM3v1Avdsat)) / CHARGE;
    if (Nl < 0.0)
	Nl = 0.0;

    T3 = model->BSIM3v1AoxideTrapDensityA
       * log(MAX(((N0 + 2.0e14) / (Nl + 2.0e14)), N_MINLOG));
    T4 = model->BSIM3v1AoxideTrapDensityB * (N0 - Nl);
    T5 = model->BSIM3v1AoxideTrapDensityC * 0.5 * (N0 * N0 - Nl * Nl);

    T6 = 8.62e-5 * (temp + CONSTCtoK) * cd * cd;
    T7 = 1.0e8 * EffFreq * pParam->BSIM3v1Aleff
       * pParam->BSIM3v1Aleff * pParam->BSIM3v1Aweff * here->BSIM3v1Am;
    T8 = model->BSIM3v1AoxideTrapDensityA + model->BSIM3v1AoxideTrapDensityB * Nl
       + model->BSIM3v1AoxideTrapDensityC * Nl * Nl;
    T9 = (Nl + 2.0e14) * (Nl + 2.0e14);

    Ssi = T1 / T2 * (T3 + T4 + T5) + T6 / T7 * DelClm * T8 / T9;
    return Ssi;
}

int
BSIM3v1Anoise (int mode, int operation, GENmodel *inModel, CKTcircuit *ckt, 
               Ndata *data, double *OnDens)
{
BSIM3v1Amodel *model = (BSIM3v1Amodel *)inModel;
BSIM3v1Ainstance *here;
struct bsim3v1aSizeDependParam *pParam;
char name[N_MXVLNTH];
double tempOnoise;
double tempInoise;
double noizDens[BSIM3v1ANSRCS];
double lnNdens[BSIM3v1ANSRCS];

double vgs, vds, Slimit;
double T1, T10, T11;
double Ssi, Swi;

int i;

    /* define the names of the noise sources */
    static char *BSIM3v1AnNames[BSIM3v1ANSRCS] =
    {   /* Note that we have to keep the order */
	".rd",              /* noise due to rd */
			    /* consistent with the index definitions */
	".rs",              /* noise due to rs */
			    /* in BSIM3v1Adefs.h */
	".id",              /* noise due to id */
	".1overf",          /* flicker (1/f) noise */
	""                  /* total transistor noise */
    };

    for (; model != NULL; model = model->BSIM3v1AnextModel)
    {    for (here = model->BSIM3v1Ainstances; here != NULL;
	      here = here->BSIM3v1AnextInstance)
	 {    
	 
              if (here->BSIM3v1Aowner != ARCHme)
	              continue;

	      pParam = here->pParam;
	      switch (operation)
	      {  case N_OPEN:
		     /* see if we have to to produce a summary report */
		     /* if so, name all the noise generators */

		      if (((NOISEAN*)ckt->CKTcurJob)->NStpsSm != 0)
		      {   switch (mode)
			  {  case N_DENS:
			          for (i = 0; i < BSIM3v1ANSRCS; i++)
				  {    (void) sprintf(name, "onoise.%s%s",
					              here->BSIM3v1Aname,
						      BSIM3v1AnNames[i]);
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
			          for (i = 0; i < BSIM3v1ANSRCS; i++)
				  {    (void) sprintf(name, "onoise_total.%s%s",
						      here->BSIM3v1Aname,
						      BSIM3v1AnNames[i]);
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
						      here->BSIM3v1Aname,
						      BSIM3v1AnNames[i]);
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
		      switch (mode)
		      {  case N_DENS:
		              NevalSrc(&noizDens[BSIM3v1ARDNOIZ],
				       &lnNdens[BSIM3v1ARDNOIZ], ckt, THERMNOISE,
				       here->BSIM3v1AdNodePrime, here->BSIM3v1AdNode,
				       here->BSIM3v1AdrainConductance * here->BSIM3v1Am);

		              NevalSrc(&noizDens[BSIM3v1ARSNOIZ],
				       &lnNdens[BSIM3v1ARSNOIZ], ckt, THERMNOISE,
				       here->BSIM3v1AsNodePrime, here->BSIM3v1AsNode,
				       here->BSIM3v1AsourceConductance * here->BSIM3v1Am);

                              if (model->BSIM3v1AnoiMod == 2)
		              {   NevalSrc(&noizDens[BSIM3v1AIDNOIZ],
				         &lnNdens[BSIM3v1AIDNOIZ], ckt, THERMNOISE,
				         here->BSIM3v1AdNodePrime,
                                         here->BSIM3v1AsNodePrime, (here->BSIM3v1Aueff
					 * fabs((here->BSIM3v1Aqinv * here->BSIM3v1Am)
					 / (pParam->BSIM3v1Aleff
					 *  pParam->BSIM3v1Aleff))));
		              }
                              else
			      {   NevalSrc(&noizDens[BSIM3v1AIDNOIZ],
				       &lnNdens[BSIM3v1AIDNOIZ], ckt, THERMNOISE,
				       here->BSIM3v1AdNodePrime,
				       here->BSIM3v1AsNodePrime,
                                       (2.0 / 3.0 * fabs(here->BSIM3v1Agm
				       + here->BSIM3v1Agds)));

			      }
		              NevalSrc(&noizDens[BSIM3v1AFLNOIZ], (double*) NULL,
				       ckt, N_GAIN, here->BSIM3v1AdNodePrime,
				       here->BSIM3v1AsNodePrime, (double) 0.0);

                              if (model->BSIM3v1AnoiMod == 2)
			      {   vgs = *(ckt->CKTstates[0] + here->BSIM3v1Avgs);
		                  vds = *(ckt->CKTstates[0] + here->BSIM3v1Avds);
			          if (vds < 0.0)
			          {   vds = -vds;
				      vgs = vgs + vds;
			          }
                                  if (vgs >= here->BSIM3v1Avon + 0.1)
			          {   Ssi = StrongInversionNoiseEval_b3v1a(vgs, vds,
					    model, here, data->freq,
					    ckt->CKTtemp);
                                      noizDens[BSIM3v1AFLNOIZ] *= Ssi;
			          }
                                  else 
			          {   pParam = here->pParam;
				      T10 = model->BSIM3v1AoxideTrapDensityA
					  * 8.62e-5 * (ckt->CKTtemp + CONSTCtoK);
		                      T11 = pParam->BSIM3v1Aweff * pParam->BSIM3v1Aleff
				          * pow(data->freq, model->BSIM3v1Aef)
				          * 4.0e36;
		                      Swi = T10 / T11 * here->BSIM3v1Acd * here->BSIM3v1Am
				          * here->BSIM3v1Acd * here->BSIM3v1Am;
                                      Slimit = StrongInversionNoiseEval_b3v1a(
				           here->BSIM3v1Avon + 0.1,
				           vds, model, here,
				           data->freq, ckt->CKTtemp);
				      T1 = Swi + Slimit;
				      if (T1 > 0.0)
                                          noizDens[BSIM3v1AFLNOIZ] *= (Slimit * Swi)
							        / T1; 
				      else
                                          noizDens[BSIM3v1AFLNOIZ] *= 0.0;
			          }
		              }
                              else
			      {    noizDens[BSIM3v1AFLNOIZ] *= model->BSIM3v1Akf * 
				            exp(model->BSIM3v1Aaf
					    * log(MAX(fabs(here->BSIM3v1Acd * here->BSIM3v1Am),
					    N_MINLOG)))
					    / (pow(data->freq, model->BSIM3v1Aef)
					    * pParam->BSIM3v1Aleff
				            * pParam->BSIM3v1Aleff
					    * model->BSIM3v1Acox);
			      }

		              lnNdens[BSIM3v1AFLNOIZ] =
				     log(MAX(noizDens[BSIM3v1AFLNOIZ], N_MINLOG));

		              noizDens[BSIM3v1ATOTNOIZ] = noizDens[BSIM3v1ARDNOIZ]
						     + noizDens[BSIM3v1ARSNOIZ]
						     + noizDens[BSIM3v1AIDNOIZ]
						     + noizDens[BSIM3v1AFLNOIZ];
		              lnNdens[BSIM3v1ATOTNOIZ] = 
				     log(MAX(noizDens[BSIM3v1ATOTNOIZ], N_MINLOG));

		              *OnDens += noizDens[BSIM3v1ATOTNOIZ];

		              if (data->delFreq == 0.0)
			      {   /* if we haven't done any previous 
				     integration, we need to initialize our
				     "history" variables.
				    */

			          for (i = 0; i < BSIM3v1ANSRCS; i++)
				  {    here->BSIM3v1AnVar[LNLSTDENS][i] =
					     lnNdens[i];
			          }

			          /* clear out our integration variables
				     if it's the first pass
				   */
			          if (data->freq ==
				      ((NOISEAN*) ckt->CKTcurJob)->NstartFreq)
				  {   for (i = 0; i < BSIM3v1ANSRCS; i++)
				      {    here->BSIM3v1AnVar[OUTNOIZ][i] = 0.0;
				           here->BSIM3v1AnVar[INNOIZ][i] = 0.0;
			              }
			          }
		              }
			      else
			      {   /* data->delFreq != 0.0,
				     we have to integrate.
				   */
			          for (i = 0; i < BSIM3v1ANSRCS; i++)
				  {    if (i != BSIM3v1ATOTNOIZ)
				       {   tempOnoise = Nintegrate(noizDens[i],
						lnNdens[i],
				                here->BSIM3v1AnVar[LNLSTDENS][i],
						data);
				           tempInoise = Nintegrate(noizDens[i]
						* data->GainSqInv, lnNdens[i]
						+ data->lnGainInv,
				                here->BSIM3v1AnVar[LNLSTDENS][i]
						+ data->lnGainInv, data);
				           here->BSIM3v1AnVar[LNLSTDENS][i] =
						lnNdens[i];
				           data->outNoiz += tempOnoise;
				           data->inNoise += tempInoise;
				           if (((NOISEAN*)
					       ckt->CKTcurJob)->NStpsSm != 0)
					   {   here->BSIM3v1AnVar[OUTNOIZ][i]
						     += tempOnoise;
				               here->BSIM3v1AnVar[OUTNOIZ][BSIM3v1ATOTNOIZ]
						     += tempOnoise;
				               here->BSIM3v1AnVar[INNOIZ][i]
						     += tempInoise;
				               here->BSIM3v1AnVar[INNOIZ][BSIM3v1ATOTNOIZ]
						     += tempInoise;
                                           }
			               }
			          }
		              }
		              if (data->prtSummary)
			      {   for (i = 0; i < BSIM3v1ANSRCS; i++)
				  {    /* print a summary report */
			               data->outpVector[data->outNumber++]
					     = noizDens[i];
			          }
		              }
		              break;
		         case INT_NOIZ:
			      /* already calculated, just output */
		              if (((NOISEAN*)ckt->CKTcurJob)->NStpsSm != 0)
			      {   for (i = 0; i < BSIM3v1ANSRCS; i++)
				  {    data->outpVector[data->outNumber++]
					     = here->BSIM3v1AnVar[OUTNOIZ][i];
			               data->outpVector[data->outNumber++]
					     = here->BSIM3v1AnVar[INNOIZ][i];
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



