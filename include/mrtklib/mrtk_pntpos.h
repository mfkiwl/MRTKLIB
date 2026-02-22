/**
 * @file mrtk_pntpos.h
 * @brief MRTKLIB Single-Point Positioning Module — SPP functions.
 *
 * This header provides the standard single-point positioning functions
 * (pntpos, ionocorr, tropcorr), extracted from pntpos.c with zero
 * algorithmic changes.
 *
 * @note Functions declared here are implemented in mrtk_pntpos.c.
 */
#ifndef MRTK_PNTPOS_H
#define MRTK_PNTPOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_nav.h"
#include "mrtklib/mrtk_obs.h"
#include "mrtklib/mrtk_opt.h"
#include "mrtklib/mrtk_sol.h"

/*============================================================================
 * Single-Point Positioning Functions
 *===========================================================================*/

/**
 * @brief Compute ionospheric correction.
 * @param[in]  time     Time
 * @param[in]  nav      Navigation data
 * @param[in]  sat      Satellite number
 * @param[in]  pos      Receiver position {lat,lon,h} (rad|m)
 * @param[in]  azel     Azimuth/elevation angle {az,el} (rad)
 * @param[in]  ionoopt  Ionospheric correction option (IONOOPT_???)
 * @param[out] ion      Ionospheric delay (L1) (m)
 * @param[out] var      Ionospheric delay (L1) variance (m^2)
 * @return Status (1:ok, 0:error)
 */
int ionocorr(gtime_t time, const nav_t *nav, int sat, const double *pos,
             const double *azel, int ionoopt, double *ion, double *var);

/**
 * @brief Compute tropospheric correction.
 * @param[in]  time     Time
 * @param[in]  nav      Navigation data
 * @param[in]  pos      Receiver position {lat,lon,h} (rad|m)
 * @param[in]  azel     Azimuth/elevation angle {az,el} (rad)
 * @param[in]  tropopt  Tropospheric correction option (TROPOPT_???)
 * @param[out] trp      Tropospheric delay (m)
 * @param[out] var      Tropospheric delay variance (m^2)
 * @return Status (1:ok, 0:error)
 */
int tropcorr(gtime_t time, const nav_t *nav, const double *pos,
             const double *azel, int tropopt, double *trp, double *var);

/**
 * @brief Compute receiver position, velocity, clock bias by single-point
 *        positioning with pseudorange and doppler observables.
 * @param[in]    obs   Observation data
 * @param[in]    n     Number of observation data
 * @param[in]    nav   Navigation data
 * @param[in]    opt   Processing options
 * @param[in,out] sol  Solution
 * @param[in,out] azel Azimuth/elevation angle (rad) (NULL: no output)
 * @param[in,out] ssat Satellite status (NULL: no output)
 * @param[out]   msg   Error message for error exit
 * @return Status (1:ok, 0:error)
 */
int pntpos(const obsd_t *obs, int n, const nav_t *nav, const prcopt_t *opt,
           sol_t *sol, double *azel, ssat_t *ssat, char *msg);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_PNTPOS_H */
