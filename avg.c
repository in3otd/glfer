/* avg.c
 * 
 * Copyright (C) 2006 Edouard Griffiths (F4EXB)
 * Copyright (C) 2006-2008 Claudio Girardi
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "avg.h"
#include "util.h"

void init_avg(avg_data_t *avgdata)
{
  avgdata->avgwidth = 0;
  avgdata->avgdepth = 0;
  avgdata->effdepth = 0;
}

void alloc_avg(avg_data_t *avgdata, int width, int depth)
{
  int i,j;
  avgdata->avgwidth = width;
  avgdata->avgdepth = depth;
  
  avgdata->avgarray = malloc(width*sizeof(double));

  for (i=0; i<width; i++)
  {
    avgdata->avgarray[i] = (double *) malloc(depth*sizeof(double));
    for (j=0; j<depth; j++)
      avgdata->avgarray[i][j] = 0.0;
  }
  avgdata->avg = (double *) malloc(width*sizeof(double));
  avgdata->cum = (double *) malloc(width*sizeof(double));
  for (i=0; i<width; i++)
  {
    avgdata->avg[i] = 0.0;
    avgdata->cum[i] = 0.0;
  }
  avgdata->effdepth = 0; 
}

void delete_avg(avg_data_t *avgdata)
{
  int i;
  
  if (avgdata->avgwidth != 0)
  {
    for (i=0; i<avgdata->avgwidth; i++)
      free(avgdata->avgarray[i]);
    free(avgdata->avgarray);
    free(avgdata->avg);
    free(avgdata->cum);
  }
  
  avgdata->avgwidth = 0;
  avgdata->avgdepth = 0; 
  avgdata->effdepth = 0; 
}

/* first version
double update_avg(avg_data_t *avgdata, int index, double newval)
{
  int i;
  double oldval;
  
  if (avgdata->effdepth < avgdata->avgdepth) // depth not filled yet
  {
    avgdata->avgarray[index][avgdata->effdepth] = newval;
    avgdata->avg[index] = (avgdata->effdepth * avgdata->avg[index] + newval) / (avgdata->effdepth + 1); // accumulate average
    //avgdata->effdepth++;
  }
  else
  {
    oldval = avgdata->avgarray[index][0];
    avgdata->avg[index] = avgdata->avg[index] + ((newval - oldval)/avgdata->effdepth); // calculate new sliding average
    for (i=0; i<(avgdata->avgdepth-1); i++) // shift register
      memcpy(&(avgdata->avgarray[index][i]), &(avgdata->avgarray[index][i+1]), sizeof(double));
    avgdata->avgarray[index][avgdata->avgdepth-1] = newval; // put new value
  }

  avgdata->cum[index] += newval - avgdata->avg[index];    
  if (avgdata->cum[index] < 0)
    avgdata->cum[index] = 1e-15;
  return avgdata->cum[index];
}
*/

double update_avg_plain(avg_data_t *avgdata, int N, float *psd, int minbin, int maxbin, int *peakbin)
{
  double avgspec = 0.0;
  double max=psd[minbin];
  int index,i;
  
  for (index=minbin; index<maxbin; index++)
  {
    if (avgdata->effdepth < avgdata->avgdepth) // depth not filled yet
    {
      avgdata->avgarray[index][avgdata->effdepth] = psd[index];
      avgdata->cum[index] += psd[index];      
    }
    else
    {
      avgdata->cum[index] += psd[index] - avgdata->avgarray[index][0];
      for (i=0; i<(avgdata->avgdepth-1); i++) // shift register
        memcpy(&(avgdata->avgarray[index][i]), &(avgdata->avgarray[index][i+1]), sizeof(double));
      avgdata->avgarray[index][avgdata->avgdepth-1] = psd[index]; // put new value
    }

    if (avgdata->cum[index] > max)
    {
      max = avgdata->cum[index];
      *peakbin = index;
    }

    avgspec += avgdata->cum[index];
  }

  if (avgdata->effdepth < avgdata->avgdepth) // depth not filled yet
    avgdata->effdepth++;

  /*
  printf("max=%le min=%le\n", max, min);
  printf("avgdata->effdepth=%i\tavgdata->avgdepth=%i\n", avgdata->effdepth, avgdata->avgdepth);
  */
  
  // calculate average over spectrum and over memory
  avgspec = (avgspec-max) / ((double) (maxbin-minbin-1) * (double) (avgdata->effdepth+1)); 
  
  // calculate average over memory
  for (index=0; index<N; index++)
  {
    if ((index < minbin) || (index>=maxbin))
      avgdata->avg[index] = 1e-15;
    else
      avgdata->avg[index] = avgdata->cum[index] / (double) (avgdata->effdepth+1);   
  }
  
  return avgspec;
}

double update_avg_sumextreme(avg_data_t *avgdata, int N, float *psd, int max0, int minbin, int maxbin, int *peakbin)
{
  double max = psd[minbin];
  double avgspec = 0.0;
  double min = 1.0;
  int index,i;
  int varsamples;
  
  for (index=minbin; index<maxbin; index++)
  {
    /* printf("psd[%i]=%f\n", index, psd[index]); */
    if (avgdata->effdepth < avgdata->avgdepth) // depth not filled yet
    {
      avgdata->avgarray[index][avgdata->effdepth] = psd[index];
      avgdata->cum[index] += psd[index];
    }
    else
    {
      avgdata->cum[index] += psd[index] - avgdata->avgarray[index][0];
      for (i=0; i<(avgdata->avgdepth-1); i++) // shift register
        memcpy(&(avgdata->avgarray[index][i]), &(avgdata->avgarray[index][i+1]), sizeof(double));
      avgdata->avgarray[index][avgdata->avgdepth-1] = psd[index]; // put new value
    }

    avgspec += avgdata->cum[index];

    if (avgdata->cum[index] > max)
    {
      max = avgdata->cum[index];
      *peakbin = index;
    }
    if (avgdata->cum[index] < min)
      min = avgdata->cum[index];
  }

  if (avgdata->effdepth < avgdata->avgdepth) // depth not filled yet
    avgdata->effdepth++;

  avgspec = (avgspec - max) / (double) (maxbin-minbin-1); // average over spectrum
  
  // rescale the values for output
  
  for (index=0; index<N; index++)
  {
    if ((index < minbin) || (index>=maxbin))
      avgdata->avg[index] = 1e-15;
    else
    {
      if (max0) // maximum fixed at 0 dB
        avgdata->avg[index] = (avgdata->cum[index] - min) / (max - min);
      else      // average fixed at 0 dB
        avgdata->avg[index] = avgdata->cum[index]/avgspec;
      /* printf("avgdata->cum[%i]=%le avgdata->avg[%i]=%le min=%le max-min=%le\n", index, avgdata->cum[index], index, avgdata->avg[index], min, max-min); */
    }    
  }
  
  /* printf("max=%le avgspec=%le m/a=%lf\n", max, avgspec, max / avgspec); */
  return max / avgspec;
}

// same as above but minimum is replaced by average over the spectrum to increase even more S+N/N
double update_avg_sumavg(avg_data_t *avgdata, int N, float *psd, int max0, int minbin, int maxbin, int *peakbin, double *variance)
{
  double max = psd[minbin];
  double avgspec = 0.0;
  double sum_avg;
  int index,i;
  int varsamples;
  
  /* printf("minbin=%i maxbin=%i\n", minbin, maxbin); */
  
  for (index=minbin; index<maxbin; index++)
  {
    /* printf("psd[%i]=%f\n", index, psd[index]); */
    if (avgdata->effdepth < avgdata->avgdepth) // depth not filled yet
    {
      avgdata->avgarray[index][avgdata->effdepth] = psd[index];
      avgdata->cum[index] += psd[index];
    }
    else
    {
      avgdata->cum[index] += psd[index] - avgdata->avgarray[index][0];
      for (i=0; i<(avgdata->avgdepth-1); i++) // shift register
        memcpy(&(avgdata->avgarray[index][i]), &(avgdata->avgarray[index][i+1]), sizeof(double));
      avgdata->avgarray[index][avgdata->avgdepth-1] = psd[index]; // put new value
    }
    
    avgspec += avgdata->cum[index];

    if (avgdata->cum[index] > max)
    {
      max = avgdata->cum[index];
      *peakbin = index;
    }
  }
  
  if (avgdata->effdepth < avgdata->avgdepth) // depth not filled yet
    avgdata->effdepth++;

  avgspec = (avgspec - max) / (double) (maxbin-minbin-1);       // average over spectrum excluding maximum
  *variance = 0.0;                                              // reset variance accumulator
  varsamples = 0;                                                 
  
  // rescale the values for output and calculate variance
  
  for (index=0; index<N; index++)
  {
    if ((index < minbin) || (index>=maxbin))
      avgdata->avg[index] = 1e-15;
    else
    {
      sum_avg = avgdata->cum[index] - avgspec; // sum minus average over spectrum gives (anticipated) signal delta
      if (sum_avg > 0)
      {
        if (max0)
          avgdata->avg[index] = (avgdata->cum[index] - avgspec) / (max - avgspec); // S+N/N clipped to maximum not scaled to time integration
        else
          avgdata->avg[index] = avgdata->cum[index]/avgspec;
        if (index != *peakbin)  // exclude maximum (to match what has been done for the average)
        {
          *variance += (avgdata->cum[index]/avgspec)*(avgdata->cum[index]/avgspec);  // accumulate
          varsamples++;
        }
      }
      else
      {
        avgdata->avg[index] = 1e-15;
      }
      /* printf("avgdata->cum[%i]=%le avgdata->avg[%i]=%le avgspec=%le max-avgspec=%le\n", index, avgdata->cum[index], index, avgdata->avg[index], avgspec, max-avgspec); */
    }    
  }
  
  // calculate variance
  *variance = *variance / (double) varsamples;

  /* printf("max=%le avgspec=%le m/a=%lf\n", max, avgspec, max / avgspec); */
  return max / avgspec; 
}

