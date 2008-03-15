  /*
 				pca.c

*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*	Part of:	PSFEx
*
*	Author:		E.BERTIN (IAP)
*
*	Contents:	Stuff related to Principal Component Analysis (PCA).
*
*	Last modify:	20/02/2008
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*/

#ifdef HAVE_CONFIG_H
#include	"config.h"
#endif

#include	<math.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#include	"define.h"
#include	"fits/fitscat.h"
#include	"pca.h"
#include	"prefs.h"
#include	"psf.h"


/****** pca_onsnaps ***********************************************************
PROTO	float *pca_onsnaps(psfstruct **psf, int ncat, int npc)
PURPOSE	Make a Principal Component Analysis in pixel space of PSF models.
INPUT	Pointer to an array of PSF structures,
	Number of catalogues (PSFs),
	Number of principal components.
OUTPUT  Pointer to an array of principal component vectors.
NOTES   -.
AUTHOR  E. Bertin (IAP, Leiden observatory & ESO)
VERSION 20/02/2008
 ***/
float *pca_onsnaps(psfstruct **psfs, int ncat, int npc)
  {
   psfstruct	*psf;
   char		str[MAXCHAR];
   double	dpos[POLY_MAXDIM],
		*covmat,*covmatt,
		dstep,dstart, dval;
   float	*basis, *pix,*pix1,*pix2;
   int		c,d,i,j,p,n,w,h, ndim,npix,nt;

/* Build models of the PSF over a range of dependency parameters */
  ndim = psfs[0]->poly->ndim;
  w = psfs[0]->size[0];
  h = psfs[0]->size[1];
  npix = w*h;
  nt = 1;
  for (d=0; d<ndim; d++)
    nt *= PCA_NSNAP;
  dstep = 1.0/PCA_NSNAP;
  dstart = (1.0-dstep)/2.0;

  NFPRINTF(OUTPUT, "Setting-up the PCA covariance matrix");
  QCALLOC(covmat, double, npix*npix);
  for (c=0; c<ncat; c++)
    {
    psf = psfs[c];
    for (d=0; d<ndim; d++)
      dpos[d] = -dstart;
    for (n=0; n<nt; n++)
      {
      psf_build(psf, dpos);
      pix1 = pix = psf->loc;
/*---- Set-up the covariance/correlation matrix */
      covmatt = covmat;
      sprintf(str, "Setting-up the PCA covariance matrix (%.0f%%)...",
		100.0*((float)n/nt+c)/ncat);
      NFPRINTF(OUTPUT, str);
      for (j=npix; j--;)
        {
        pix2 = pix;
        dval = (double)*(pix1++);
        for (i=npix; i--;)
          *(covmatt++) += dval**(pix2++);
        }
      for (d=0; d<ndim; d++)
        if (dpos[d]<dstart-0.01)
          {
          dpos[d] += dstep;
          break;
          }
        else
          dpos[d] = -dstart;
      }
    }
/* Do recursive PCA */
  QMALLOC(basis, float, npc*npix);
  for (p=0; p<npc; p++)
    {
    sprintf(str, "Computing Principal Component vector #%d...", p);
    NFPRINTF(OUTPUT, str);
    pca_findpc(covmat, &basis[p*npix], npix);
    }

  free(covmat);

  return basis;
  }


/****** pca_oncomps ***********************************************************
PROTO	double *pca_oncomps(psfstruct **psfs, int ncat, int npc)
PURPOSE	Make a Principal Component Analysis in image space on PSF model
	components.
INPUT	Pointer to an array of PSF structures,
	Number of catalogues (PSFs),
	Number of principal components.
OUTPUT  Pointer to an array of principal component vectors.
NOTES   -.
AUTHOR  E. Bertin (IAP, Leiden observatory & ESO)
VERSION 15/03/2008
 ***/
double *pca_oncomps(psfstruct **psfs, int ncat, int npc)
  {
   char		str[MAXCHAR];
   double	*covmat, *covmatt, *meancomp, *pc,
		dval, mean1,mean2;
   float	*comp1, *comp2, *vector;
   int		i,c,c1,c2,p, npix;

  NFPRINTF(OUTPUT, "Setting-up the PCA covariance matrix");

  npix = psfs[0]->npix;
/* Compute the average pixel values */
  QCALLOC(meancomp, double, ncat);
  for (c=0; c<ncat; c++)
    {
    comp1=psfs[c]->comp;
    dval = 0.0;
    for (i=npix; i--;)
        dval += (double)*(comp1++);
    meancomp[c] = dval/npix;
    }

/* Set-up the covariance/correlation matrix */
  QCALLOC(covmat, double, ncat*ncat);
  covmatt = covmat;
  for (c1=0; c1<ncat; c1++)
    {
    sprintf(str, "Setting-up the PCA covariance matrix (%.0f%%)...",
		100.0*((float)c1)/ncat);
    NFPRINTF(OUTPUT, str);
    mean1 = meancomp[c1];
    for (c2=0; c2<ncat; c2++)
      {
      mean2 = meancomp[c2];
      comp1 = psfs[c1]->comp;
      comp2 = psfs[c2]->comp;
      dval = 0.0;
      for (i=npix; i--;)
        dval += ((double)*(comp1++)-mean1)*(*(comp2++)-mean2);
      *(covmatt++) = dval;
      }
    }

  free(meancomp);

/* Do recursive PCA */
  QMALLOC(vector, float, ncat);
  QMALLOC(pc, double, ncat*npc);
  for (p=0; p<npc; p++)
    {
    sprintf(str, "Computing Principal Component vector #%d...", p);
    NFPRINTF(OUTPUT, str);
    pca_findpc(covmat, vector, ncat);
    for (c=0; c<ncat; c++)
      pc[c*npc+p] = vector[c];
    }

  free(covmat);
  free(vector);

  return pc;
  }


/****** pca_findpc ************************************************************
PROTO	double pca_findpc(double *covmat, float *vec, int nmat)
PURPOSE	Find the principal component (the one with the highest eigenvalue) and
	subtract its contribution from the covariance matrix, using the
	iterative "power" method.
INPUT	Covariance matrix,
	output vector,
	Number of principal components.
OUTPUT  Eigenvalue (variance) of the PC.
NOTES   -.
AUTHOR  E. Bertin (IAP, Leiden observatory & ESO)
VERSION 14/11/2007
 ***/
double pca_findpc(double *covmat, float *vec, int nmat)
  {
   double	*tmat,*xmat, *c,*t,*x,
		dtval,dtnorm, xval,xnorm, lambda;
   float	*v;
   int		i,j,n;

  QMALLOC(xmat, double, nmat);

/* First initialize the eigenvector to an arbitrary direction */
  QMALLOC(tmat, double, nmat);
  for (t=tmat,i=nmat; i--;)
    *(t++) = 1.0;

  dtnorm = 1.0;
  for (n=PCA_NITER; n-- && dtnorm>PCA_CONVEPS;)    
    {
/*-- Compute |x> = C|t> */
    xnorm = 0.0;
    for (c=covmat,x=xmat,j=nmat; j--;)
      {
      for (xval=0.0,t=tmat,i=nmat; i--;)
        xval += *(c++)**(t++);
      xnorm += xval*xval;
      *(x++) = xval;
      }
/*-- Compute |t> = |x>/||x|| and ||Delta t|| (for testing convergence) */
    xnorm = sqrt(xnorm);
    dtnorm = 0.0;
    for (t=tmat,x=xmat,i=nmat; i--;)
      {
      dtval = *t;
      dtval -= (*(t++) = *(x++)/xnorm);
      dtnorm += dtval*dtval;
      }
    dtnorm = sqrt(dtnorm);
    }

  free(xmat);

/* Compute the eigenvalue lambda = <t|C|t> */
  lambda = 0.0;
  for (c=covmat,x=tmat,j=nmat; j--;)
    {
    for (xval=0.0,t=tmat,i=nmat; i--;)
      xval += *(c++)**(t++);
    lambda += xval**(x++);
    }

/* Finally subtract the contribution from the found PC: C -= lambda.|t><t| */
  for (c=covmat,x=tmat,j=nmat; j--;)
    for (xval=*(x++)*lambda,t=tmat,i=nmat; i--;)
      *(c++) -= xval**(t++);

/* Convert output vector to simple precision */
  for (v=vec,t=tmat,i=nmat; i--;)
    *(v++) = (float)*(t++);

  free(tmat);

  return lambda;
  }

