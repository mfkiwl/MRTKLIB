/**
 * @file mrtk_peph.c
 * @brief MRTKLIB Precise Ephemeris Module — Earth rotation parameter I/O
 *        and interpolation functions.
 *
 * This file contains the ERP (Earth Rotation Parameter) functions extracted
 * from rtkcmn.c with zero algorithmic changes (pure cut-and-paste).
 *
 * @note Precise ephemeris functions (peph2pos, satantoff, readsp3, etc.)
 *       remain in preceph.c because they depend on nav_t which requires
 *       MAXSAT from the rtklib target.
 */
#include "mrtklib/mrtk_peph.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*============================================================================
 * Constants (duplicated from rtklib.h to avoid header dependency)
 *===========================================================================*/

#define PI          3.1415926535897932  /* pi */
#define D2R         (PI/180.0)         /* deg to rad */
#define AS2R        (D2R/3600.0)       /* arc sec to radian */

/*============================================================================
 * Forward declarations for legacy functions (resolved at link time)
 *===========================================================================*/

extern void trace(int level, const char *format, ...);

/*============================================================================
 * Public Functions
 *===========================================================================*/

/* read earth rotation parameters ----------------------------------------------
* read earth rotation parameters
* args   : char   *file       I   IGS ERP file (IGS ERP ver.2)
*          erp_t  *erp        O   earth rotation parameters
* return : status (1:ok,0:file open error)
*-----------------------------------------------------------------------------*/
extern int readerp(const char *file, erp_t *erp)
{
    FILE *fp;
    erpd_t *erp_data;
    double v[14]={0};
    char buff[256];

    trace(3,"readerp: file=%s\n",file);

    if (!(fp=fopen(file,"r"))) {
        trace(2,"erp file open error: file=%s\n",file);
        return 0;
    }
    while (fgets(buff,sizeof(buff),fp)) {
        if (sscanf(buff,"%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                   v,v+1,v+2,v+3,v+4,v+5,v+6,v+7,v+8,v+9,v+10,v+11,v+12,v+13)<5) {
            continue;
        }
        if (erp->n>=erp->nmax) {
            erp->nmax=erp->nmax<=0?128:erp->nmax*2;
            erp_data=(erpd_t *)realloc(erp->data,sizeof(erpd_t)*erp->nmax);
            if (!erp_data) {
                free(erp->data); erp->data=NULL; erp->n=erp->nmax=0;
                fclose(fp);
                return 0;
            }
            erp->data=erp_data;
        }
        erp->data[erp->n].mjd=v[0];
        erp->data[erp->n].xp=v[1]*1E-6*AS2R;
        erp->data[erp->n].yp=v[2]*1E-6*AS2R;
        erp->data[erp->n].ut1_utc=v[3]*1E-7;
        erp->data[erp->n].lod=v[4]*1E-7;
        erp->data[erp->n].xpr=v[12]*1E-6*AS2R;
        erp->data[erp->n++].ypr=v[13]*1E-6*AS2R;
    }
    fclose(fp);
    return 1;
}
/* get earth rotation parameter values -----------------------------------------
* get earth rotation parameter values
* args   : erp_t  *erp        I   earth rotation parameters
*          gtime_t time       I   time (gpst)
*          double *erpv       O   erp values {xp,yp,ut1_utc,lod} (rad,rad,s,s/d)
* return : status (1:ok,0:error)
*-----------------------------------------------------------------------------*/
extern int geterp(const erp_t *erp, gtime_t time, double *erpv)
{
    const double ep[]={2000,1,1,12,0,0};
    double mjd,day,a;
    int i,j,k;

    trace(4,"geterp:\n");

    if (erp->n<=0) return 0;

    mjd=51544.5+(timediff(gpst2utc(time),epoch2time(ep)))/86400.0;

    if (mjd<=erp->data[0].mjd) {
        day=mjd-erp->data[0].mjd;
        erpv[0]=erp->data[0].xp     +erp->data[0].xpr*day;
        erpv[1]=erp->data[0].yp     +erp->data[0].ypr*day;
        erpv[2]=erp->data[0].ut1_utc-erp->data[0].lod*day;
        erpv[3]=erp->data[0].lod;
        return 1;
    }
    if (mjd>=erp->data[erp->n-1].mjd) {
        day=mjd-erp->data[erp->n-1].mjd;
        erpv[0]=erp->data[erp->n-1].xp     +erp->data[erp->n-1].xpr*day;
        erpv[1]=erp->data[erp->n-1].yp     +erp->data[erp->n-1].ypr*day;
        erpv[2]=erp->data[erp->n-1].ut1_utc-erp->data[erp->n-1].lod*day;
        erpv[3]=erp->data[erp->n-1].lod;
        return 1;
    }
    for (j=0,k=erp->n-1;j<k-1;) {
        i=(j+k)/2;
        if (mjd<erp->data[i].mjd) k=i; else j=i;
    }
    if (erp->data[j].mjd==erp->data[j+1].mjd) {
        a=0.5;
    }
    else {
        a=(mjd-erp->data[j].mjd)/(erp->data[j+1].mjd-erp->data[j].mjd);
    }
    erpv[0]=(1.0-a)*erp->data[j].xp     +a*erp->data[j+1].xp;
    erpv[1]=(1.0-a)*erp->data[j].yp     +a*erp->data[j+1].yp;
    erpv[2]=(1.0-a)*erp->data[j].ut1_utc+a*erp->data[j+1].ut1_utc;
    erpv[3]=(1.0-a)*erp->data[j].lod    +a*erp->data[j+1].lod;
    return 1;
}
