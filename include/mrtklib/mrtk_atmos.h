/**
 * @file mrtk_atmos.h
 * @brief MRTKLIB Atmosphere Module — Ionospheric and tropospheric models.
 *
 * This header provides broadcast ionosphere (Klobuchar) model, ionospheric
 * mapping function, ionospheric pierce point computation, tropospheric
 * (Saastamoinen) model, and tropospheric mapping function (NMF/GMF).
 *
 * @note All functions in this module are pure cut-and-paste extractions
 *       from rtkcmn.c with zero algorithmic changes.
 */
#ifndef MRTK_ATMOS_H
#define MRTK_ATMOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mrtklib/mrtk_foundation.h"
#include "mrtklib/mrtk_time.h"
#include "mrtklib/mrtk_coords.h"

/*============================================================================
 * Ionosphere Models
 *===========================================================================*/

/**
 * @brief Compute ionospheric delay by broadcast ionosphere model (Klobuchar).
 * @param[in] t     Time (GPST)
 * @param[in] ion   Iono model parameters {a0,a1,a2,a3,b0,b1,b2,b3}
 * @param[in] pos   Receiver position {lat,lon,h} (rad,m)
 * @param[in] azel  Azimuth/elevation angle {az,el} (rad)
 * @return Ionospheric delay (L1) (m)
 */
double ionmodel(gtime_t t, const double *ion, const double *pos,
                const double *azel);

/**
 * @brief Compute ionospheric delay mapping function by single layer model.
 * @param[in] pos   Receiver position {lat,lon,h} (rad,m)
 * @param[in] azel  Azimuth/elevation angle {az,el} (rad)
 * @return Ionospheric mapping function
 */
double ionmapf(const double *pos, const double *azel);

/**
 * @brief Compute ionospheric pierce point (IPP) position and slant factor.
 * @param[in]  pos   Receiver position {lat,lon,h} (rad,m)
 * @param[in]  azel  Azimuth/elevation angle {az,el} (rad)
 * @param[in]  re    Earth radius (km)
 * @param[in]  hion  Altitude of ionosphere (km)
 * @param[out] posp  Pierce point position {lat,lon,h} (rad,m)
 * @return Slant factor
 * @note Only valid on the earth surface.
 */
double ionppp(const double *pos, const double *azel, double re, double hion,
              double *posp);

/*============================================================================
 * Troposphere Models
 *===========================================================================*/

/**
 * @brief Compute tropospheric delay by standard atmosphere and Saastamoinen.
 * @param[in] time  Time
 * @param[in] pos   Receiver position {lat,lon,h} (rad,m)
 * @param[in] azel  Azimuth/elevation angle {az,el} (rad)
 * @param[in] humi  Relative humidity
 * @return Tropospheric delay (m)
 */
double tropmodel(gtime_t time, const double *pos, const double *azel,
                 double humi);

/**
 * @brief Compute tropospheric mapping function by NMF.
 * @param[in]     time   Time
 * @param[in]     pos    Receiver position {lat,lon,h} (rad,m)
 * @param[in]     azel   Azimuth/elevation angle {az,el} (rad)
 * @param[in,out] mapfw  Wet mapping function (NULL: not output)
 * @return Dry mapping function
 * @note See ref [5] (NMF) and [9] (GMF).
 */
double tropmapf(gtime_t time, const double *pos, const double *azel,
                double *mapfw);

#ifdef __cplusplus
}
#endif

#endif /* MRTK_ATMOS_H */
