// Test file for calculating the conservative collision operator Cc
//
//	Copyright (c) 2014, Christian B. Mendl
//	All rights reserved.
//	http://christian.mendl.net
//
//	This program is free software; you can redistribute it and/or
//	modify it under the terms of the Simplified BSD License
//	http://www.opensource.org/licenses/bsd-license.php
//
//	References:
//	- Jianfeng Lu, Christian B. Mendl
//	  Numerical scheme for a spatially inhomogeneous matrix-valued quantum Boltzmann equation
//	  Journal of Computational Physics 291, 303-316 (2015)
//	  (arXiv:1408.1782)
//
//	- Martin L.R. F"urst, Christian B. Mendl, Herbert Spohn
//	  Matrix-valued Boltzmann equation for the Hubbard chain
//	  Physical Review E 86, 031122 (2012)
//	  (arXiv:1207.6926)
//_______________________________________________________________________________________________________________________
//

#include "collision.h"
#include "util.h"
#include <memory.h>
#include <time.h>


#if defined(_WIN32) & (defined(DEBUG) | defined(_DEBUG))
#include <crtdbg.h>
#endif

#define V_RETURN(x)  { hr = (x); if (hr < 0) { fprintf(stderr, "%s, line %d: command '%s' failed, return value: %d\n", __FILE__, __LINE__, #x, hr); return hr; } }


//_______________________________________________________________________________________________________________________
//
// difference z1 - z2
//
static inline void Complex_Subtract(const fftw_complex z1, const fftw_complex z2, fftw_complex ret)
{
	ret[0] = z1[0] - z2[0];
	ret[1] = z1[1] - z2[1];
}


//_______________________________________________________________________________________________________________________
//
// absolute value |z|
//
static inline double Complex_Abs(const fftw_complex z)
{
	if (fabs(z[0]) < fabs(z[1]))
	{
		return fabs(z[1])*sqrt(1 + square(z[0]/z[1]));
	}
	else	// fabs(z[1]) <= fabs(z[0])
	{
		if (z[0] != 0)
		{
			return fabs(z[0])*sqrt(1 + square(z[1]/z[0]));
		}
		else
		{
			return 0;
		}
	}
}



int main()
{
	const unsigned int J = 48;	// must not be too small
	const double L = 12;
	const double R = 7.5;

	unsigned int i, j, k;
	int hr;

	// enable run-time memory check for debug builds
	#if defined(_WIN32) & (defined(DEBUG) | defined(_DEBUG))
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	#endif

	// load data from disk
	wignerV_t *W = fftw_malloc(sizeof(wignerV_t));
	for (i = 0; i < 4; i++)
	{
		char fnmame[1024];
		sprintf(fnmame, "../test/data/W%d.dat", i);
		V_RETURN(ReadData(fnmame, &W->comp[i], sizeof(double), sizeof(W->comp[i]) / sizeof(double)));
	}

	// Fourier transform
	wignerF_t *WF = fftw_malloc(sizeof(wignerF_t));

	// FFT of real data fills half the resulting complex array only
	cgrid_t *WFh = fftw_malloc(sizeof(cgrid_t));
	const double scale = 1.0 / (N_GRID * N_GRID);
	for (i = 0; i < 4; i++)
	{
		fftw_plan plan = fftw_plan_dft_r2c_2d(N_GRID, N_GRID, W->comp[i].data, WFh->data, FFTW_ESTIMATE);
		fftw_execute(plan);
		fftw_destroy_plan(plan);

		for (k = 0; k < N_GRID*(N_GRID/2+1); k++)
		{
			WFh->data[k][0] *= scale;
			WFh->data[k][1] *= scale;
		}

		// copy entries to fill complete complex array
		// FFTW uses row-major ordering
		for (j = 0; j < N_GRID; j++)
		{
			// complementary index
			unsigned int jc = (N_GRID - j) & N_MASK;

			for (k = 0; k <= N_GRID/2; k++)
			{
				WF->comp[i].data[N_GRID*j+k][0] = WFh->data[(N_GRID/2+1)*j+k][0];
				WF->comp[i].data[N_GRID*j+k][1] = WFh->data[(N_GRID/2+1)*j+k][1];
			}
			for (k = N_GRID/2 + 1; k < N_GRID; k++)
			{
				// complementary index
				unsigned int kc = N_GRID - k;
				// copy conjugate value
				WF->comp[i].data[N_GRID*j+k][0] =  WFh->data[(N_GRID/2+1)*jc+kc][0];
				WF->comp[i].data[N_GRID*j+k][1] = -WFh->data[(N_GRID/2+1)*jc+kc][1];
			}
		}
	}
	fftw_free(WFh);

	// quadrature rules
	quadI1_t quadI1;
	FourierI1(J, L, R, &quadI1);

	// intermediate data
	CcInterm_t interm;
	CcInterm_Create(&interm);

	wignerF_t *Cc     = fftw_malloc(sizeof(wignerF_t));
	wignerF_t *Cc_ref = fftw_malloc(sizeof(wignerF_t));

	printf("calculating conservative collision operator Cc...\n");

	// start timer
	clock_t t_start = clock();

	// calculate Cc
	CcInt(WF, &quadI1, &interm, Cc);
	
	clock_t t_end = clock();
	double cpu_time = (double)(t_end - t_start) / CLOCKS_PER_SEC;
	printf("finished calculation, CPU time: %g\n\n", cpu_time);

	// example
	printf("example:\n");
	printf("Cc->comp[0].data[5]: %g + %gi\n", Cc->comp[0].data[5][0], Cc->comp[0].data[5][1]);
	printf("Cc->comp[1].data[5]: %g + %gi\n", Cc->comp[1].data[5][0], Cc->comp[1].data[5][1]);

	// load reference data from disk
	for (i = 0; i < 4; i++)
	{
		char fnmame[1024];
		sprintf(fnmame, "../test/data/Cc%d_ref.dat", i);
		V_RETURN(ReadData(fnmame, &Cc_ref->comp[i], sizeof(fftw_complex), sizeof(Cc_ref->comp[i]) / sizeof(fftw_complex)));
	}
	printf("Cc_ref->comp[0].data[5]: %g + %gi\n", Cc_ref->comp[0].data[5][0], Cc_ref->comp[0].data[5][1]);
	printf("Cc_ref->comp[1].data[5]: %g + %gi\n", Cc_ref->comp[1].data[5][0], Cc_ref->comp[1].data[5][1]);

	// compare with reference
	double err = 0;
	double nrf = 0;
	for (i = 0; i < 4; i++)
	{
		for (k = 0; k < N_GRID*N_GRID; k++)
		{
			fftw_complex diff;
			Complex_Subtract(Cc->comp[i].data[k], Cc_ref->comp[i].data[k], diff);
			err += Complex_Abs(diff);
			nrf += Complex_Abs(Cc_ref->comp[i].data[k]);
		}
	}
	printf("\ncumulative error: %g, relative error: %g\n", err, err / nrf);

	// clean up
	fftw_free(Cc_ref);
	fftw_free(Cc);
	CcInterm_Delete(&interm);
	QuadI1_Delete(&quadI1);
	fftw_free(WF);
	fftw_free(W);

	fftw_cleanup();

	return 0;
}
