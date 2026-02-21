/**
 * @file mrtk_eph.c
 * @brief MRTKLIB Broadcast Ephemeris Module — Kepler/GLONASS/SBAS orbit and
 *        clock computation functions.
 *
 * This file contains all broadcast ephemeris orbit/clock computation functions
 * extracted from ephemeris.c with zero algorithmic changes (pure cut-and-paste).
 */
#include "mrtklib/mrtk_eph.h"
#include "mrtklib/mrtk_mat.h"

#include <math.h>

/*============================================================================
 * Constants (duplicated from rtklib.h / ephemeris.c to avoid header dependency)
 *===========================================================================*/

#define PI          3.1415926535897932  /* pi */
#define CLIGHT      299792458.0        /* speed of light (m/s) */
#define OMGE        7.2921151467E-5    /* earth angular velocity (IS-GPS) (rad/s) */

#define SQR(x)      ((x)*(x))

#define RE_GLO      6378136.0          /* radius of earth (m)            ref [2] */
#define MU_GPS      3.9860050E14       /* gravitational constant         ref [1] */
#define MU_GLO      3.9860044E14       /* gravitational constant         ref [2] */
#define MU_GAL      3.986004418E14     /* earth gravitational constant   ref [7] */
#define MU_CMP      3.986004418E14     /* earth gravitational constant   ref [9] */
#define J2_GLO      1.0826257E-3       /* 2nd zonal harmonic of geopot   ref [2] */

#define OMGE_GLO    7.292115E-5        /* earth angular velocity (rad/s) ref [2] */
#define OMGE_GAL    7.2921151467E-5    /* earth angular velocity (rad/s) ref [7] */
#define OMGE_CMP    7.292115E-5        /* earth angular velocity (rad/s) ref [9] */

#define SIN_5      -0.0871557427476582 /* sin(-5.0 deg) */
#define COS_5       0.9961946980917456 /* cos(-5.0 deg) */

#define ERREPH_GLO  5.0                /* error of glonass ephemeris (m) */
#define TSTEP       60.0               /* integration step glonass ephemeris (s) */
#define RTOL_KEPLER 1E-13              /* relative tolerance for Kepler equation */
#define MAX_ITER_KEPLER 30             /* max number of iteration of Kelpler */

#define STD_GAL_NAPA 500.0             /* error of galileo ephemeris for NAPA (m) */

/* navigation system identifiers */
#define SYS_NONE    0x00
#define SYS_GPS     0x01
#define SYS_SBS     0x02
#define SYS_GLO     0x04
#define SYS_GAL     0x08
#define SYS_QZS     0x10
#define SYS_CMP     0x20
#define SYS_IRN     0x40

/*============================================================================
 * Forward declarations for legacy functions (resolved at link time)
 *===========================================================================*/

extern void trace(int level, const char *format, ...);
extern int satsys(int sat, int *prn);

/*============================================================================
 * Static Helpers
 *===========================================================================*/

/* variance by ura ephemeris -------------------------------------------------*/
static double var_uraeph(int sys, int ura)
{
    const double ura_value[]={
        2.4,3.4,4.85,6.85,9.65,13.65,24.0,48.0,96.0,192.0,384.0,768.0,1536.0,
        3072.0,6144.0
    };
    if (sys==SYS_GAL) { /* galileo sisa (ref [7] 5.1.11) */
        if (ura<= 49) return SQR(ura*0.01);
        if (ura<= 74) return SQR(0.5+(ura- 50)*0.02);
        if (ura<= 99) return SQR(1.0+(ura- 75)*0.04);
        if (ura<=125) return SQR(2.0+(ura-100)*0.16);
        return SQR(STD_GAL_NAPA);
    }
    else { /* gps ura (ref [1] 20.3.3.3.1.1) */
        return ura<0||14<ura?SQR(6144.0):SQR(ura_value[ura]);
    }
}
/* glonass orbit differential equations --------------------------------------*/
static void deq(const double *x, double *xdot, const double *acc)
{
    double a,b,c,r2=dot(x,x,3),r3=r2*sqrt(r2),omg2=SQR(OMGE_GLO);

    if (r2<=0.0) {
        xdot[0]=xdot[1]=xdot[2]=xdot[3]=xdot[4]=xdot[5]=0.0;
        return;
    }
    /* ref [2] A.3.1.2 with bug fix for xdot[4],xdot[5] */
    a=1.5*J2_GLO*MU_GLO*SQR(RE_GLO)/r2/r3; /* 3/2*J2*mu*Ae^2/r^5 */
    b=5.0*x[2]*x[2]/r2;                    /* 5*z^2/r^2 */
    c=-MU_GLO/r3-a*(1.0-b);                /* -mu/r^3-a(1-b) */
    xdot[0]=x[3]; xdot[1]=x[4]; xdot[2]=x[5];
    xdot[3]=(c+omg2)*x[0]+2.0*OMGE_GLO*x[4]+acc[0];
    xdot[4]=(c+omg2)*x[1]-2.0*OMGE_GLO*x[3]+acc[1];
    xdot[5]=(c-2.0*a)*x[2]+acc[2];
}
/* glonass position and velocity by numerical integration --------------------*/
static void glorbit(double t, double *x, const double *acc)
{
    double k1[6],k2[6],k3[6],k4[6],w[6];
    int i;

    deq(x,k1,acc); for (i=0;i<6;i++) w[i]=x[i]+k1[i]*t/2.0;
    deq(w,k2,acc); for (i=0;i<6;i++) w[i]=x[i]+k2[i]*t/2.0;
    deq(w,k3,acc); for (i=0;i<6;i++) w[i]=x[i]+k3[i]*t;
    deq(w,k4,acc);
    for (i=0;i<6;i++) x[i]+=(k1[i]+2.0*k2[i]+2.0*k3[i]+k4[i])*t/6.0;
}

/*============================================================================
 * Public Functions
 *===========================================================================*/

/* almanac to satellite position and clock bias --------------------------------
* compute satellite position and clock bias with almanac (gps, galileo, qzss)
* args   : gtime_t time     I   time (gpst)
*          alm_t *alm       I   almanac
*          double *rs       O   satellite position (ecef) {x,y,z} (m)
*          double *dts      O   satellite clock bias (s)
* return : none
* notes  : see ref [1],[7],[8]
*-----------------------------------------------------------------------------*/
extern void alm2pos(gtime_t time, const alm_t *alm, double *rs, double *dts)
{
    double tk,M,E,Ek,sinE,cosE,u,r,i,O,x,y,sinO,cosO,cosi,mu;
    int n;

    trace(4,"alm2pos : time=%s sat=%2d\n",time_str(time,3),alm->sat);

    tk=timediff(time,alm->toa);

    if (alm->A<=0.0) {
        rs[0]=rs[1]=rs[2]=*dts=0.0;
        return;
    }
    mu=satsys(alm->sat,NULL)==SYS_GAL?MU_GAL:MU_GPS;

    M=alm->M0+sqrt(mu/(alm->A*alm->A*alm->A))*tk;
    for (n=0,E=M,Ek=0.0;fabs(E-Ek)>RTOL_KEPLER&&n<MAX_ITER_KEPLER;n++) {
        Ek=E; E-=(E-alm->e*sin(E)-M)/(1.0-alm->e*cos(E));
    }
    if (n>=MAX_ITER_KEPLER) {
        trace(2,"alm2pos: kepler iteration overflow sat=%2d\n",alm->sat);
        return;
    }
    sinE=sin(E); cosE=cos(E);
    u=atan2(sqrt(1.0-alm->e*alm->e)*sinE,cosE-alm->e)+alm->omg;
    r=alm->A*(1.0-alm->e*cosE);
    i=alm->i0;
    O=alm->OMG0+(alm->OMGd-OMGE)*tk-OMGE*alm->toas;
    x=r*cos(u); y=r*sin(u); sinO=sin(O); cosO=cos(O); cosi=cos(i);
    rs[0]=x*cosO-y*cosi*sinO;
    rs[1]=x*sinO+y*cosi*cosO;
    rs[2]=y*sin(i);
    *dts=alm->f0+alm->f1*tk;
}
/* broadcast ephemeris to satellite clock bias ---------------------------------
* compute satellite clock bias with broadcast ephemeris (gps, galileo, qzss)
* args   : gtime_t time     I   time by satellite clock (gpst)
*          eph_t *eph       I   broadcast ephemeris
* return : satellite clock bias (s) without relativeity correction
* notes  : see ref [1],[7],[8]
*          satellite clock does not include relativity correction and tdg
*-----------------------------------------------------------------------------*/
extern double eph2clk(gtime_t time, const eph_t *eph)
{
    double t,ts;
    int i;

    trace(4,"eph2clk : time=%s sat=%2d\n",time_str(time,3),eph->sat);

    t=ts=timediff(time,eph->toc);

    for (i=0;i<2;i++) {
        t=ts-(eph->f0+eph->f1*t+eph->f2*t*t);
    }
    return eph->f0+eph->f1*t+eph->f2*t*t;
}
/* broadcast ephemeris to satellite position and clock bias --------------------
* compute satellite position and clock bias with broadcast ephemeris (gps,
* galileo, qzss)
* args   : gtime_t time     I   time (gpst)
*          eph_t *eph       I   broadcast ephemeris
*          double *rs       O   satellite position (ecef) {x,y,z} (m)
*          double *dts      O   satellite clock bias (s)
*          double *var      O   satellite position and clock variance (m^2)
* return : none
* notes  : see ref [1],[7],[8]
*          satellite clock includes relativity correction without code bias
*          (tgd or bgd)
*-----------------------------------------------------------------------------*/
extern void eph2pos(gtime_t time, const eph_t *eph, double *rs, double *dts,
                    double *var)
{
    double tk,M,E,Ek,sinE,cosE,u,r,i,O,sin2u,cos2u,x,y,sinO,cosO,cosi,mu,omge;
    double xg,yg,zg,sino,coso;
    int n,sys,prn;

    trace(4,"eph2pos : time=%s sat=%2d\n",time_str(time,3),eph->sat);

    if (eph->A<=0.0) {
        rs[0]=rs[1]=rs[2]=*dts=*var=0.0;
        return;
    }
    tk=timediff(time,eph->toe);

    switch ((sys=satsys(eph->sat,&prn))) {
        case SYS_GAL: mu=MU_GAL; omge=OMGE_GAL; break;
        case SYS_CMP: mu=MU_CMP; omge=OMGE_CMP; break;
        default:      mu=MU_GPS; omge=OMGE;     break;
    }
    M=eph->M0+(sqrt(mu/(eph->A*eph->A*eph->A))+eph->deln)*tk;

    for (n=0,E=M,Ek=0.0;fabs(E-Ek)>RTOL_KEPLER&&n<MAX_ITER_KEPLER;n++) {
        Ek=E; E-=(E-eph->e*sin(E)-M)/(1.0-eph->e*cos(E));
    }
    if (n>=MAX_ITER_KEPLER) {
        trace(2,"eph2pos: kepler iteration overflow sat=%2d\n",eph->sat);
        return;
    }
    sinE=sin(E); cosE=cos(E);

    trace(4,"kepler: sat=%2d e=%8.5f n=%2d del=%10.3e\n",eph->sat,eph->e,n,E-Ek);

    u=atan2(sqrt(1.0-eph->e*eph->e)*sinE,cosE-eph->e)+eph->omg;
    r=eph->A*(1.0-eph->e*cosE);
    i=eph->i0+eph->idot*tk;
    sin2u=sin(2.0*u); cos2u=cos(2.0*u);
    u+=eph->cus*sin2u+eph->cuc*cos2u;
    r+=eph->crs*sin2u+eph->crc*cos2u;
    i+=eph->cis*sin2u+eph->cic*cos2u;
    x=r*cos(u); y=r*sin(u); cosi=cos(i);

    /* beidou geo satellite */
    if (sys==SYS_CMP&&(prn<=5||prn>=59)) { /* ref [9] table 4-1 */
        O=eph->OMG0+eph->OMGd*tk-omge*eph->toes;
        sinO=sin(O); cosO=cos(O);
        xg=x*cosO-y*cosi*sinO;
        yg=x*sinO+y*cosi*cosO;
        zg=y*sin(i);
        sino=sin(omge*tk); coso=cos(omge*tk);
        rs[0]= xg*coso+yg*sino*COS_5+zg*sino*SIN_5;
        rs[1]=-xg*sino+yg*coso*COS_5+zg*coso*SIN_5;
        rs[2]=-yg*SIN_5+zg*COS_5;
    }
    else {
        O=eph->OMG0+(eph->OMGd-omge)*tk-omge*eph->toes;
        sinO=sin(O); cosO=cos(O);
        rs[0]=x*cosO-y*cosi*sinO;
        rs[1]=x*sinO+y*cosi*cosO;
        rs[2]=y*sin(i);
    }
    tk=timediff(time,eph->toc);
    *dts=eph->f0+eph->f1*tk+eph->f2*tk*tk;

    /* relativity correction */
    *dts-=2.0*sqrt(mu*eph->A)*eph->e*sinE/SQR(CLIGHT);

    /* position and clock error variance */
    *var=var_uraeph(sys,eph->sva);
}
/* glonass ephemeris to satellite clock bias -----------------------------------
* compute satellite clock bias with glonass ephemeris
* args   : gtime_t time     I   time by satellite clock (gpst)
*          geph_t *geph     I   glonass ephemeris
* return : satellite clock bias (s)
* notes  : see ref [2]
*-----------------------------------------------------------------------------*/
extern double geph2clk(gtime_t time, const geph_t *geph)
{
    double t,ts;
    int i;

    trace(4,"geph2clk: time=%s sat=%2d\n",time_str(time,3),geph->sat);

    t=ts=timediff(time,geph->toe);

    for (i=0;i<2;i++) {
        t=ts-(-geph->taun+geph->gamn*t);
    }
    return -geph->taun+geph->gamn*t;
}
/* glonass ephemeris to satellite position and clock bias ----------------------
* compute satellite position and clock bias with glonass ephemeris
* args   : gtime_t time     I   time (gpst)
*          geph_t *geph     I   glonass ephemeris
*          double *rs       O   satellite position {x,y,z} (ecef) (m)
*          double *dts      O   satellite clock bias (s)
*          double *var      O   satellite position and clock variance (m^2)
* return : none
* notes  : see ref [2]
*-----------------------------------------------------------------------------*/
extern void geph2pos(gtime_t time, const geph_t *geph, double *rs, double *dts,
                     double *var)
{
    double t,tt,x[6];
    int i;

    trace(4,"geph2pos: time=%s sat=%2d\n",time_str(time,3),geph->sat);

    t=timediff(time,geph->toe);

    *dts=-geph->taun+geph->gamn*t;

    for (i=0;i<3;i++) {
        x[i  ]=geph->pos[i];
        x[i+3]=geph->vel[i];
    }
    for (tt=t<0.0?-TSTEP:TSTEP;fabs(t)>1E-9;t-=tt) {
        if (fabs(t)<TSTEP) tt=t;
        glorbit(tt,x,geph->acc);
    }
    for (i=0;i<3;i++) rs[i]=x[i];

    *var=SQR(ERREPH_GLO);
}
/* sbas ephemeris to satellite clock bias --------------------------------------
* compute satellite clock bias with sbas ephemeris
* args   : gtime_t time     I   time by satellite clock (gpst)
*          seph_t *seph     I   sbas ephemeris
* return : satellite clock bias (s)
* notes  : see ref [3]
*-----------------------------------------------------------------------------*/
extern double seph2clk(gtime_t time, const seph_t *seph)
{
    double t,ts;
    int i;

    trace(4,"seph2clk: time=%s sat=%2d\n",time_str(time,3),seph->sat);

    t=ts=timediff(time,seph->t0);

    for (i=0;i<2;i++) {
        t=ts-seph->af0+seph->af1*t;
    }
    return seph->af0+seph->af1*t;
}
/* sbas ephemeris to satellite position and clock bias -------------------------
* compute satellite position and clock bias with sbas ephemeris
* args   : gtime_t time     I   time (gpst)
*          seph_t  *seph    I   sbas ephemeris
*          double  *rs      O   satellite position {x,y,z} (ecef) (m)
*          double  *dts     O   satellite clock bias (s)
*          double  *var     O   satellite position and clock variance (m^2)
* return : none
* notes  : see ref [3]
*-----------------------------------------------------------------------------*/
extern void seph2pos(gtime_t time, const seph_t *seph, double *rs, double *dts,
                     double *var)
{
    double t;
    int i;

    trace(4,"seph2pos: time=%s sat=%2d\n",time_str(time,3),seph->sat);

    t=timediff(time,seph->t0);

    for (i=0;i<3;i++) {
        rs[i]=seph->pos[i]+seph->vel[i]*t+seph->acc[i]*t*t/2.0;
    }
    *dts=seph->af0+seph->af1*t;

    *var=var_uraeph(SYS_SBS,seph->sva);
}
