/******************************************************************************
FILE: compute_s2_refl.c

PURPOSE: Contains functions for handling the S2 TOA reflectance and surface
reflectance corrections.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
******************************************************************************/

#include "lasrc.h"
#include "time.h"
#include "aero_interp.h"
#include "poly_coeff.h"

/******************************************************************************
MODULE:  read_s2_toa_refl

PURPOSE:  Reads the input TOA S2 reflectance bands and converts all bands
to 10m resolution.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error reading the input TOA reflectance
SUCCESS         No errors encountered

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

NOTES:
******************************************************************************/
int read_s2_toa_refl
(
    Input_t *input,     /* I: input structure for the Sentinel product */
    Espa_internal_meta_t *xml_metadata,
                        /* I: XML metadata structure */
    uint16 **toaband    /* O: output TOA reflectance values (scaled) */
)
{
    char errmsg[STR_SIZE];                   /* error message */
    char FUNC_NAME[] = "read_s2_toa_refl";   /* function name */
    int ib;              /* looping variable for input bands */
    int nlines10 = -99;  /* number of lines in 10m reflectance bands */
    int nsamps10 = -99;  /* number of samps in 10m reflectance bands */
    int nlines20 = -99;  /* number of lines in 20m reflectance bands */
    int nsamps20 = -99;  /* number of samps in 20m reflectance bands */
    int nlines60 = -99;  /* number of lines in 20m reflectance bands */
    int nsamps60 = -99;  /* number of samps in 20m reflectance bands */
    uint16 *tmp20_band = NULL;  /* array for input 20m image data for a single
                                   band, nlines20 x nsamps20 */
    uint16 *tmp60_band = NULL;  /* array for input 60m image data for a single
                                   band, nlines60 x nsamps60 */

    /* Determine the 10m, 20m, and 60m number of lines and samples */
    for (ib = 0; ib < xml_metadata->nbands; ib++)
    {
        /* Use band 2 for the representative 10m band */
        if (!strcmp (xml_metadata->band[ib].name, "B02"))
        {
            nlines10 = xml_metadata->band[ib].nlines;
            nsamps10 = xml_metadata->band[ib].nsamps;
        }

        /* Use band 5 for the representative 20m band */
        else if (!strcmp (xml_metadata->band[ib].name, "B05"))
        {
            nlines20 = xml_metadata->band[ib].nlines;
            nsamps20 = xml_metadata->band[ib].nsamps;
        }

        /* Use band 1 for the representative 60m band */
        else if (!strcmp (xml_metadata->band[ib].name, "B01"))
        {
            nlines60 = xml_metadata->band[ib].nlines;
            nsamps60 = xml_metadata->band[ib].nsamps;
        }
    }

    /* Make sure they were found and are valid */
    if (nlines10 == -99 || nsamps10 == -99)
    {
        sprintf (errmsg, "Error obtaining the nlines/nsamps for 10m band");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    if (nlines20 == -99 || nsamps20 == -99)
    {
        sprintf (errmsg, "Error obtaining the nlines/nsamps for 20m band");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    if (nlines60 == -99 || nsamps60 == -99)
    {
        sprintf (errmsg, "Error obtaining the nlines/nsamps for 60m band");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Allocate memory for 10m, 20m 60m band data */
    tmp20_band = calloc (nlines20 * nsamps20, sizeof (uint16));
    if (tmp20_band == NULL)
    {
        sprintf (errmsg, "Error allocating memory for temporary 20m band");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    tmp60_band = calloc (nlines60 * nsamps60, sizeof (uint16));
    if (tmp60_band == NULL)
    {
        sprintf (errmsg, "Error allocating memory for temporary 60m band");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Loop through the Sentinel-2 bands */
    for (ib = DN_S2_BAND1; ib <= DN_S2_BAND12; ib++)
    {
        switch (ib)
        {
            /* 10m bands read as-is (4) */
            case DN_S2_BAND2:
            case DN_S2_BAND3:
            case DN_S2_BAND4:
            case DN_S2_BAND8:
                /* Read the input band data */
                if (get_input_refl_lines (input, ib, 0, nlines10, nsamps10,
                    &toaband[ib][0]) != SUCCESS)
                {
                    sprintf (errmsg, "Error reading S2 TOA 10m band %d", ib+1);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                break;

            /* 20m bands convert to 10m (6) */
            case DN_S2_BAND5:
            case DN_S2_BAND6:
            case DN_S2_BAND7:
            case DN_S2_BAND8A:
            case DN_S2_BAND11:
            case DN_S2_BAND12:
                /* Read the input band data */
                if (get_input_refl_lines (input, ib, 0, nlines20, nsamps20,
                    tmp20_band) != SUCCESS)
                {
                    sprintf (errmsg, "Error reading S2 TOA 20m band %d", ib+1);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                /* Convert to 10m */
                if (convert_to_10m (nlines20, nsamps20, nlines10, nsamps10,
                    tmp20_band, &toaband[ib][0]) != SUCCESS)
                {
                    sprintf (errmsg, "Error converting 20m band %d", ib+1);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                break;

            /* 60m bands convert to 10m (3, but skipping bands 9&10) */
            case DN_S2_BAND1:
                /* Read the input band data */
                if (get_input_refl_lines (input, ib, 0, nlines60, nsamps60,
                    tmp60_band) != SUCCESS)
                {
                    sprintf (errmsg, "Error reading S2 TOA 60m band %d", ib+1);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                /* Convert to 10m */
                if (convert_to_10m (nlines60, nsamps60, nlines10, nsamps10,
                    tmp60_band, &toaband[ib][0]) != SUCCESS)
                {
                    sprintf (errmsg, "Error converting 60m band %d", ib+1);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                break;
        }
    }

    /* Free the memory */
    free (tmp20_band);
    free (tmp60_band);

    return (SUCCESS);
}


/******************************************************************************
MODULE:  compute_s2_sr_refl

PURPOSE:  Computes the surface reflectance for all the Sentinel-2 reflectance
bands.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error computing the reflectance
SUCCESS         No errors encountered

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

NOTES:
1. Initializes the variables and data arrays from the lookup table and
   auxiliary files.
2. The tauray array was originally read in from a static ASCII file, but it is
   now hardcoded to save time from reading the file each time.  This file was
   generated (like many of the other auxiliary input tables) by running 6S and
   storing the coefficients.
4. Aerosols are retrieved for all non-fill pixels.  If the aerosol fails the
   model residual or NDVI test, then the pixel is flagged as water.  All water
   pixels are run through a water-specific aerosol retrieval.  If the model
   residual fails, then that pixel is marked as failed aerosol retrieval.  Any
   pixel that failed retrieval is then interpolated using an average of the
   clear (valid land pixel aerosols) and water (valid water pixel aerosols).
   Those final aerosol values are used for the surface reflectance corrections.
5. Cloud-based QA information is not processed in this algorithm.
******************************************************************************/
int compute_s2_sr_refl
(
    Input_t *input,     /* I: input structure for the Landsat product */
    Espa_internal_meta_t *xml_metadata,
                        /* I: XML metadata structure */
    char *xml_infile,   /* I: input XML filename */
    uint16 *qaband,     /* I: QA band for the input image, nlines x nsamps */
    int nlines,         /* I: number of lines in reflectance, thermal bands */
    int nsamps,         /* I: number of samps in reflectance, thermal bands */
    float pixsize,      /* I: pixel size for the reflectance bands */
    uint16 **toaband,   /* I: input TOA reflectance bands, nlines x nsamps */
    int16 **sband,      /* O: output SR bands, nlines x nsamps */
    float xts,          /* I: scene center solar zenith angle (deg) */
    float xmus,         /* I: cosine of solar zenith angle */
    char *anglehdf,     /* I: angle HDF filename */
    char *intrefnm,     /* I: intrinsic reflectance filename */
    char *transmnm,     /* I: transmission filename */
    char *spheranm,     /* I: spherical albedo filename */
    char *cmgdemnm,     /* I: climate modeling grid DEM filename */
    char *rationm,      /* I: ratio averages filename */
    char *auxnm         /* I: auxiliary filename for ozone and water vapor */
)
{
    char errmsg[STR_SIZE];                     /* error message */
    char FUNC_NAME[] = "compute_s2_sr_refl";   /* function name */
    Sat_t sat = input->meta.sat;               /* satellite */
    int retval;          /* return status */
    int i, j;            /* looping variable for pixels */
    int ib;              /* looping variable for input bands */
    int iband;           /* current band */
    int curr_pix;        /* current pixel in 1D arrays of nlines * nsamps */
    int iline;           /* current line in the 6x6 window for atm corr */
    int isamp;           /* current sample in the 6x6 window for atm corr */
    int curr_win_pix;    /* current pixel in the 6x6 window for atm corr */
    int pix_count;       /* count of valid pixels in the 5x5 window */
    float tmpf;          /* temporary floating point value */
    float rotoa;         /* top of atmosphere reflectance */
    float roslamb;       /* lambertian surface reflectance */
    float tgo;           /* other gaseous transmittance (tgog * tgoz) */
    float roatm;         /* intrinsic atmospheric reflectance */
    float ttatmg;        /* total atmospheric transmission */
    float satm;          /* atmosphere spherical albedo */
    float xrorayp;       /* reflectance of the atmosphere due to molecular
                            (Rayleigh) scattering */
    float next;
    float erelc[NSR_BANDS];   /* band ratio variable for refl bands */
    float troatm[NSR_BANDS];  /* atmospheric reflectance table for refl bands */

    int iband1, iband3; /* band indices (zero-based) */
    float raot;         /* AOT reflectance */
                        /* raot values for three different eps values */
    float residual;     /* model residual */
    float residual1, residual2, residual3;
                        /* residuals for 3 different eps values */
    float rsurf;        /* surface reflectance */
    float corf;         /* aerosol impact (higher values represent high
                           aerosol) */
    float ros4,ros5;    /* surface reflectance for bands 4 and 5 */
    int tmp_percent;    /* current percentage for printing status */
#ifndef _OPENMP
    int curr_tmp_percent; /* percentage for current line */
#endif

    float lat, lon;       /* pixel lat, long location */
    int lcmg, scmg;       /* line/sample index for the CMG */
    int lcmg1;            /* line+1 index for the CMG */
    float u, v;           /* line/sample index for the CMG */
    float one_minus_u;    /* 1.0 - u */
    float one_minus_v;    /* 1.0 - v */
    float one_minus_u_x_one_minus_v;  /* (1.0 - u) * (1.0 - v) */
    float one_minus_u_x_v;  /* (1.0 - u) * v */
    float u_x_one_minus_v;  /* u * (1.0 - v) */
    float u_x_v;            /* u * v */
    float ndwi_th1, ndwi_th2; /* values for NDWI calculations */
    float xcmg, ycmg;     /* x/y location for CMG */
    float xndwi;          /* calculated NDWI value */
    float median_aerosol; /* median aerosol value for clear pixels */
    uint8 *ipflag = NULL; /* QA flag to assist with aerosol interpolation,
                             nlines x nsamps */
    float *taero = NULL;  /* aerosol values for each pixel, nlines x nsamps */
    float *teps = NULL;   /* angstrom coeff for each pixel, nlines x nsamps */

    /* Vars for forward/inverse mapping space */
    Geoloc_t *space = NULL;       /* structure for geolocation information */
    Space_def_t space_def;        /* structure to define the space mapping */
    Img_coord_float_t img;        /* coordinate in line/sample space */
    Geo_coord_t geo;              /* coordinate in lat/long space */

    /* Lookup table variables */
    float eps;           /* angstrom coefficient */
    float eps1, eps2, eps3;  /* eps values for three runs */
    float xtv;           /* observation zenith angle (deg) */
    float xmuv;          /* cosine of observation zenith angle */
    float xfi;           /* azimuthal difference between the sun and
                            observation angle (deg) */
    float cosxfi;        /* cosine of azimuthal difference */
    float xtsstep;       /* solar zenith step value */
    float xtsmin;        /* minimum solar zenith value */
    float xtvstep;       /* observation step value */
    float xtvmin;        /* minimum observation value */
    float *rolutt = NULL;  /* intrinsic reflectance table
                          [NSR_BANDS x NPRES_VALS x NAOT_VALS x NSOLAR_VALS] */
    float *transt = NULL;  /* transmission table
                       [NSR_BANDS x NPRES_VALS x NAOT_VALS x NSUNANGLE_VALS] */
    float *sphalbt = NULL; /* spherical albedo table 
                              [NSR_BANDS x NPRES_VALS x NAOT_VALS] */
    float *normext = NULL; /* aerosol extinction coefficient at the current
                              wavelength (normalized at 550nm)
                              [NSR_BANDS x NPRES_VALS x NAOT_VALS] */
    float *tsmax = NULL;   /* maximum scattering angle table
                              [NVIEW_ZEN_VALS x NSOLAR_ZEN_VALS] */
    float *tsmin = NULL;   /* minimum scattering angle table
                              [NVIEW_ZEN_VALS x NSOLAR_ZEN_VALS] */
    float *nbfi = NULL;    /* number of azimuth angles
                              [NVIEW_ZEN_VALS x NSOLAR_ZEN_VALS] */
    float *nbfic = NULL;   /* communitive number of azimuth angles
                              [NVIEW_ZEN_VALS x NSOLAR_ZEN_VALS] */
    float *ttv = NULL;     /* view angle table
                              [NVIEW_ZEN_VALS x NSOLAR_ZEN_VALS] */
    float tts[22];         /* sun angle table */
    int32 indts[22];       /* index for sun angle table */
    int iaots;             /* index for AOTs */

    /* Atmospheric correction coefficient variables */
    float tgo_arr[NREFL_BANDS];     /* per-band other gaseous transmittance */
    float xrorayp_arr[NREFL_BANDS]; /* per-band reflectance of the atmosphere
                                       due to molecular (Rayleigh) scattering */
    float roatm_arr[NREFL_BANDS][NAOT_VALS];  /* per band AOT vals for roatm */
    float ttatmg_arr[NREFL_BANDS][NAOT_VALS]; /* per band AOT vals for ttatmg */
    float satm_arr[NREFL_BANDS][NAOT_VALS];   /* per band AOT vals for satm */
    float roatm_coef[NREFL_BANDS][NCOEF];  /* per band poly coeffs for roatm */
    float ttatmg_coef[NREFL_BANDS][NCOEF]; /* per band poly coeffs for ttatmg */
    float satm_coef[NREFL_BANDS][NCOEF];   /* per band poly coeffs for satm */
    float normext_p0a3_arr[NREFL_BANDS];   /* per band normext[iband][0][3] */
    int roatm_iaMax[NREFL_BANDS];
    int ia;                                /* looping variable for AOTs */
    int iaMaxTemp;                         /* max temp for current AOT level */
    float arr1[NAOT_VALS], coef1[NCOEF];   /* temporary arrays */

    /* Auxiliary file variables */
    int16 *dem = NULL;        /* CMG DEM data array [DEM_NBLAT x DEM_NBLON] */
    int16 *andwi = NULL;      /* avg NDWI [RATIO_NBLAT x RATIO_NBLON] */
    int16 *sndwi = NULL;      /* standard NDWI [RATIO_NBLAT x RATIO_NBLON] */
    int16 *ratiob1 = NULL;    /* mean band1 ratio [RATIO_NBLAT x RATIO_NBLON] */
    int16 *ratiob2 = NULL;    /* mean band2 ratio [RATIO_NBLAT x RATIO_NBLON] */
    int16 *ratiob7 = NULL;    /* mean band7 ratio [RATIO_NBLAT x RATIO_NBLON] */
    int16 *intratiob1 = NULL;   /* intercept band1 ratio,
                                   RATIO_NBLAT x RATIO_NBLON */
    int16 *intratiob2 = NULL;   /* intercept band2 ratio
                                   RATIO_NBLAT x RATIO_NBLON */
    int16 *intratiob7 = NULL;   /* intercept band7 ratio
                                   RATIO_NBLAT x RATIO_NBLON */
    int16 *slpratiob1 = NULL;   /* slope band1 ratio
                                   RATIO_NBLAT x RATIO_NBLON */
    int16 *slpratiob2 = NULL;   /* slope band2 ratio
                                   RATIO_NBLAT x RATIO_NBLON */
    int16 *slpratiob7 = NULL;   /* slope band7 ratio
                                   RATIO_NBLAT x RATIO_NBLON */
    uint16 *wv = NULL;       /* water vapor values [CMG_NBLAT x CMG_NBLON] */
    uint8 *oz = NULL;        /* ozone values [CMG_NBLAT x CMG_NBLON] */
    float raot550nm;    /* nearest input value of AOT */
    float uoz;          /* total column ozone */
    float uwv;          /* total column water vapor (precipital water vapor) */
    float pres;         /* surface pressure */
    float rb1;          /* band ratio 1 (unscaled) */
    float rb2;          /* band ratio 2 (unscaled) */
    float slpr11, slpr12, slpr21, slpr22;  /* band ratio slope at line,samp;
                           line, samp+1; line+1, samp; and line+1, samp+1 */
    float intr11, intr12, intr21, intr22;  /* band ratio intercept at line,samp;
                           line, samp+1; line+1, samp; and line+1, samp+1 */
    float slprb1, slprb2, slprb7;  /* interpolated band ratio slope values for
                                      band ratios 1, 2, 7 */
    float intrb1, intrb2, intrb7;  /* interpolated band ratio intercept values
                                      for band ratios 1, 2, 7 */
    int ratio_pix11;  /* pixel location for ratio products [lcmg][scmg] */
    int ratio_pix12;  /* pixel location for ratio products [lcmg][scmg+1] */
    int ratio_pix21;  /* pixel location for ratio products [lcmg+1][scmg] */
    int ratio_pix22;  /* pixel location for ratio products [lcmg+1][scmg+1] */

    /* Variables for finding the eps that minimizes the residual */
    double xa, xb, xc, xd, xe, xf;  /* coefficients */
    double coefa, coefb;            /* coefficients */
    float epsmin;                   /* eps which minimizes the residual */
    float resepsmin;                /* residual eps which minimizes residual */

    /* Output file info */
    time_t mytime;               /* timing variable */
    Output_t *sr_output = NULL;  /* output structure and metadata for the SR
                                    product */
    Envi_header_t envi_hdr;      /* output ENVI header information */
    char envi_file[STR_SIZE];    /* ENVI filename */
    char *cptr = NULL;           /* pointer to the file extension */

    /* Table constants */
    float aot550nm[NAOT_VALS] =  /* AOT look-up table */
        {0.01, 0.05, 0.10, 0.15, 0.20, 0.30, 0.40, 0.60, 0.80, 1.00, 1.20,
         1.40, 1.60, 1.80, 2.00, 2.30, 2.60, 3.00, 3.50, 4.00, 4.50, 5.00};
    float tpres[NPRES_VALS] =    /* surface pressure table */
        {1050.0, 1013.0, 900.0, 800.0, 700.0, 600.0, 500.0};

    /* Atmospheric correction variables */
    /* Look up table for atmospheric and geometric quantities. Taurary comes
       from tauray-ldcm/msi.ASC and the oz, wv, og variables come from
       gascoef-modis/msi.ASC. */
    /* NOTE: coefficients for bands 9 and 10 have been removed from these
       arrays since those bands are no longer processed */
    float tauray[NSR_S2_BANDS] =  /* molecular optical thickness
                                     coefficients -- produced by running 6S */
        {0.23432, 0.15106, 0.09102, 0.04535, 0.03584, 0.02924, 0.02338, 0.01847,
         0.01560, 0.00128, 0.00037};
    double oztransa[NSR_S2_BANDS] =   /* ozone transmission coeff */
        {-0.00264691, -0.0272572, -0.0986512, -0.0500348, -0.0204295,
         -0.0108641, 0.0001, 0.0001, 0.0001, 0.0001, 0.0001};
    double wvtransa[NSR_S2_BANDS] =   /* water vapor transmission coeff */
        {2.29849e-27, 2.29849e-27, 0.000777307, 0.00361051, 0.0141249,
         0.0137067, 0.00410217, 0.0285871, 0.000390755, 0.000640155, 0.018006};
    double wvtransb[NSR_S2_BANDS] =   /* water vapor transmission coeff */
        {0.999742, 0.999742, 0.891099, 0.754895, 0.75596, 0.763497, 0.74117,
         0.578722, 0.900899, 0.943712, 0.647517};
    double ogtransa1[NSR_S2_BANDS] =  /* other gases transmission coeff */
        {4.91586e-20, 4.91586e-20, 4.91586e-20, 4.91586e-20, 5.3367e-06,
         4.91586e-20, 9.03583e-05, 1.64109e-09, 1.90458e-05, 0.0212751,
         0.0243065};
    double ogtransb0[NSR_S2_BANDS] =  /* other gases transmission coeff */
        {0.000197019, 0.000197019, 0.000197019, 0.000197019, -0.980313,
         0.000197019, 0.0265393, 1.E-10, 0.0322844, 0.000197019, 0.000197019};
    double ogtransb1[NSR_S2_BANDS] =  /* other gases transmission coeff */
        {9.57011e-16, 9.57011e-16, 9.57011e-16, 9.57011e-16, 1.33639,
         9.57011e-16, 0.0532256, 1.E-10, -0.0219907, 0.0116062, 0.0604312};

#ifdef WRITE_TAERO
    FILE *aero_fptr=NULL;   /* file pointer for aerosol files */
#endif

    /* Start processing */
    mytime = time(NULL);
    printf ("Start surface reflectance corrections: %s", ctime(&mytime));

    /* Allocate memory for the many arrays needed to do the surface reflectance
       computations */
    retval = s2_memory_allocation_sr (nlines, nsamps, &ipflag, &taero, &teps,
        &dem, &andwi, &sndwi, &ratiob1, &ratiob2, &ratiob7, &intratiob1,
        &intratiob2, &intratiob7, &slpratiob1, &slpratiob2, &slpratiob7, &wv,
        &oz, &rolutt, &transt, &sphalbt, &normext, &tsmax, &tsmin, &nbfic,
        &nbfi, &ttv);
    if (retval != SUCCESS)
    {
        sprintf (errmsg, "Error allocating memory for the data arrays needed "
            "for S2 surface reflectance calculations.");
        error_handler (false, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Initialize the geolocation space applications */
    if (!get_geoloc_info (xml_metadata, &space_def))
    {
        sprintf (errmsg, "Getting the space definition from the XML file");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    space = setup_mapping (&space_def);
    if (space == NULL)
    {
        sprintf (errmsg, "Setting up the geolocation mapping");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Initialize the look up tables and atmospheric correction variables.
       view zenith initialized to 0.0 (xtv)
       azimuthal difference between sun and obs angle initialize to 0.0 (xfi)
       surface pressure is initialized to the pressure at the center of the
           scene (using the DEM) (pres)
       water vapor is initialized to the value at the center of the scene (uwv)
       ozone is initialized to the value at the center of the scene (uoz) */
    retval = init_sr_refl (nlines, nsamps, input, space, anglehdf, intrefnm,
        transmnm, spheranm, cmgdemnm, rationm, auxnm, &xtv, &xmuv, &xfi,
        &cosxfi, &pres, &uoz, &uwv, &xtsstep, &xtsmin, &xtvstep, &xtvmin,
        tsmax, tsmin, tts, ttv, indts, rolutt, transt, sphalbt, normext,
        nbfic, nbfi, dem, andwi, sndwi, ratiob1, ratiob2, ratiob7, intratiob1,
        intratiob2, intratiob7, slpratiob1, slpratiob2, slpratiob7, wv, oz);
    if (retval != SUCCESS)
    {
        sprintf (errmsg, "Error initializing the lookup tables and "
            "atmospheric correction variables.");
        error_handler (false, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Loop through all the reflectance bands and perform atmospheric
       corrections based on climatology */
    mytime = time(NULL);
    printf ("Performing atmospheric corrections for each S2 reflectance "
        "band ... %s", ctime(&mytime)); fflush(stdout);
    raot550nm = 0.05;
    eps = -1.0;
    for (ib = 0; ib <= SR_S2_BAND12; ib++)
    {
        printf ("  Band %s\n", S2_BANDNAME[ib]); fflush(stdout);

        /* Get the parameters for the atmospheric correction */
        /* rotoa is not defined for this call, which is ok, but the
           roslamb value is not valid upon output. Just set it to 0.0 to
           be consistent. */
        rotoa = 0.0;
        retval = atmcorlamb2 (input->meta.sat, xts, xtv, xmus, xmuv, xfi,
            cosxfi, raot550nm, ib, pres, tpres, aot550nm, rolutt, transt,
            xtsstep, xtsmin, xtvstep, xtvmin, sphalbt, normext, tsmax,
            tsmin, nbfic, nbfi, tts, indts, ttv, uoz, uwv, tauray,
            ogtransa1, ogtransb0, ogtransb1, wvtransa, wvtransb, oztransa,
            rotoa, &roslamb, &tgo, &roatm, &ttatmg, &satm, &xrorayp, &next,
            eps);
        if (retval != SUCCESS)
        {
            sprintf (errmsg, "Performing lambertian atmospheric correction "
                "type 2.");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Perform atmospheric corrections for reflectance bands */
#ifdef _OPENMP
        #pragma omp parallel for private (i, j, curr_pix, roslamb)
#endif
        for (i = 0; i < nlines; i++)
        {
            curr_pix = i * nsamps;
            for (j = 0; j < nsamps; j++, curr_pix++)
            {
                /* If this pixel is not fill then handle the atmospheric
                   correction */
                if (toaband[ib][curr_pix] > 0)
                {
                    /* Store the TOA scaled TOA reflectance values for later
                       use before completing atmospheric corrections */
                    roslamb = toaband[ib][curr_pix] * SCALE_FACTOR;
    
                    /* Apply the atmospheric corrections (ignoring the Rayleigh
                       scattering component and water vapor), and store the
                       scaled value for further corrections.  (NOTE: the full
                       computations are in atmcorlamb2) */
                    roslamb = roslamb / tgo;
                    roslamb = roslamb - roatm;
                    roslamb = roslamb / ttatmg;
                    roslamb = roslamb / (1.0 + satm * roslamb);
                    sband[ib][curr_pix] = (int) (roslamb * MULT_FACTOR);

                }
                else
                { /* fill value */
                    qaband[curr_pix] |= ESPA_L1_DESIGNATED_FILL_BIT;
                    sband[ib][curr_pix] = FILL_VALUE;
                }
            }  /* end for j */
        }  /* end for i */
    }  /* for ib */
    printf ("\n");

    /* Start the retrieval of atmospheric correction parameters for each band */
    mytime = time(NULL);
    printf ("Starting retrieval of atmospheric correction parameters ... %s",
        ctime(&mytime)); fflush(stdout);
    for (ib = 0; ib <= SR_S2_BAND12; ib++)
    {
        /* Get the parameters for the atmospheric correction */
        /* rotoa is not defined for this call, which is ok, but the
           roslamb value is not valid upon output. Just set it to 0.0 to
           be consistent. */
        normext_p0a3_arr[ib] = normext[ib * NPRES_VALS * NAOT_VALS + 0 + 3];
            /* normext[ib][0][3]; */
        rotoa = 0.0;
        eps = -1.0;
        for (ia = 0; ia < NAOT_VALS; ia++)
        {
            raot550nm = aot550nm[ia];
            retval = atmcorlamb2 (input->meta.sat, xts, xtv, xmus, xmuv, xfi,
                cosxfi, raot550nm, ib, pres, tpres, aot550nm, rolutt, transt,
                xtsstep, xtsmin, xtvstep, xtvmin, sphalbt, normext, tsmax,
                tsmin, nbfic, nbfi, tts, indts, ttv, uoz, uwv, tauray,
                ogtransa1, ogtransb0, ogtransb1, wvtransa, wvtransb, oztransa,
                rotoa, &roslamb, &tgo, &roatm, &ttatmg, &satm, &xrorayp, &next,
                eps);
            if (retval != SUCCESS)
            {
                sprintf (errmsg, "Performing lambertian atmospheric correction "
                    "type 2 for band %d.", ib);
                error_handler (true, FUNC_NAME, errmsg);
                exit (ERROR);
            }

            /* Store the AOT-related variables for use in the atmospheric
               corrections */
            roatm_arr[ib][ia] = roatm;
            ttatmg_arr[ib][ia] = ttatmg;
            satm_arr[ib][ia] = satm;
        }

        /* Store the band-related variables for use in the atmospheric
           corrections. tgo and xrorayp are the same for each AOT, so just
           save the last set for this band. */
        tgo_arr[ib] = tgo;
        xrorayp_arr[ib] = xrorayp;
    }

    for (ib = 0; ib <= SR_S2_BAND12; ib++)
    {
        /* Get the polynomial coefficients for roatm */
        for (ia = 0; ia < NAOT_VALS; ia++)
            arr1[ia] = roatm_arr[ib][ia];
        iaMaxTemp = 1;

        for (ia = 1; ia < NAOT_VALS; ia++)
        {
            if (ia == NAOT_VALS-1)
                iaMaxTemp = NAOT_VALS-1;

            if ((arr1[ia] - arr1[ia-1]) > ESPA_EPSILON)
                continue;
            else
            {
                iaMaxTemp = ia-1;
                break;
            }
        }

        roatm_iaMax[ib] = iaMaxTemp;
        get_3rd_order_poly_coeff (aot550nm, arr1, iaMaxTemp, coef1);
        for (ia = 0; ia < NCOEF; ia++)
            roatm_coef[ib][ia] = coef1[ia];

        /* Get the polynomial coefficients for ttatmg */
        for (ia = 0; ia < NAOT_VALS; ia++)
            arr1[ia] = ttatmg_arr[ib][ia];
        get_3rd_order_poly_coeff (aot550nm, arr1, NAOT_VALS, coef1);
        for (ia = 0; ia < NCOEF; ia++)
            ttatmg_coef[ib][ia] = coef1[ia];

        /* Get the polynomial coefficients for satm */
        for (ia = 0; ia < NAOT_VALS; ia++)
            arr1[ia] = satm_arr[ib][ia];
        get_3rd_order_poly_coeff (aot550nm, arr1, NAOT_VALS, coef1);
        for (ia = 0; ia < NCOEF; ia++)
            satm_coef[ib][ia] = coef1[ia];
    }

    /* Compute some EPS values */
    eps1 = LOW_EPS;
    eps2 = MOD_EPS;
    eps3 = HIGH_EPS;
    xa = (eps1 * eps1) - (eps3 * eps3);
    xd = (eps2 * eps2) - (eps3 * eps3);
    xb = eps1 - eps3;
    xe = eps2 - eps3;

    /* Start the aerosol inversion */
    mytime = time(NULL);
    printf ("Aerosol Inversion using %d x %d aerosol window ... %s",
        S2_AERO_WINDOW, S2_AERO_WINDOW, ctime(&mytime)); fflush(stdout);
    tmp_percent = 0;
#ifdef _OPENMP
    #pragma omp parallel for private (i, j, curr_pix, img, geo, lat, lon, xcmg, ycmg, lcmg, scmg, lcmg1, u, v, one_minus_u, one_minus_v, one_minus_u_x_one_minus_v, one_minus_u_x_v, u_x_one_minus_v, u_x_v, ratio_pix11, ratio_pix12, ratio_pix21, ratio_pix22, rb1, rb2, slpr11, slpr12, slpr21, slpr22, intr11, intr12, intr21, intr22, slprb1, slprb2, slprb7, intrb1, intrb2, intrb7, xndwi, ndwi_th1, ndwi_th2, iline, isamp, curr_win_pix, pix_count, iband, iband1, iband3, iaots, retval, eps, residual, residual1, residual2, residual3, raot, xc, xf, coefa, coefb, epsmin, resepsmin, corf, next, rotoa, raot550nm, roslamb, tgo, roatm, ttatmg, satm, xrorayp, ros4, ros5, erelc, troatm)
#endif
    for (i = 0; i < nlines; i+=S2_AERO_WINDOW)
    {
#ifndef _OPENMP
        /* update status, but not if multi-threaded */
        curr_tmp_percent = 100 * i / nlines;
        if (curr_tmp_percent > tmp_percent)
        {
            tmp_percent = curr_tmp_percent;
            if (tmp_percent % 10 == 0)
            {
                printf ("%d%% ", tmp_percent);
                fflush (stdout);
            }
        }
#endif

        curr_pix = i * nsamps;
        for (j = 0; j < nsamps; j+=S2_AERO_WINDOW, curr_pix+=S2_AERO_WINDOW)
        {
            /* If this pixel is fill */
            if (level1_qa_is_fill (qaband[curr_pix]))
            {
                ipflag[curr_pix] = (1 << IPFLAG_FILL);
                continue;
            }

            /* Get the lat/long for the current pixel (which may not be the
               center of the aerosol window), for the center of that pixel */
            img.l = i + 0.5;
            img.s = j + 0.5;
            img.is_fill = false;
            if (!from_space (space, &img, &geo))
            {
                sprintf (errmsg, "Mapping line/sample (%d, %d) to "
                    "geolocation coords", i, j);
                error_handler (true, FUNC_NAME, errmsg);
                exit (ERROR);
            }
            lat = geo.lat * RAD2DEG;
            lon = geo.lon * RAD2DEG;

            /* Use that lat/long to determine the line/sample in the
               CMG-related lookup tables, using the center of the UL
               pixel. Note, we are basically making sure the line/sample
               combination falls within -90, 90 and -180, 180 global climate
               data boundaries.  However, the source code below uses lcmg+1
               and scmg+1, which for some scenes may wrap around the
               dateline or the poles.  Thus we need to wrap the CMG data
               around to the beginning of the array. */
            /* Each CMG pixel is 0.05 x 0.05 degrees.  Use the center of the
               pixel for each calculation.  Negative latitude values should
               be the largest line values in the CMG grid.  Negative
               longitude values should be the smallest sample values in the
               CMG grid. */
            /* The line/sample calculation from the x/ycmg values are not
               rounded.  The interpolation of the value using line+1 and
               sample+1 are based on the truncated numbers, therefore
               rounding up is not appropriate. */
            ycmg = (89.975 - lat) * 20.0;   /* vs / 0.05 */
            xcmg = (179.975 + lon) * 20.0;  /* vs / 0.05 */
            lcmg = (int) ycmg;
            scmg = (int) xcmg;

            /* Handle the edges of the lat/long values in the CMG grid */
            if (lcmg < 0)
                lcmg = 0;
            else if (lcmg >= CMG_NBLAT)
                lcmg = CMG_NBLAT;

            if (scmg < 0)
                scmg = 0;
            else if (scmg >= CMG_NBLON)
                scmg = CMG_NBLON;

            /* If the current CMG pixel is at the edge of the CMG array, then
               allow the next pixel for interpolation to wrap around the
               array */
            if (lcmg >= CMG_NBLAT-1)  /* -90 degrees so wrap around */
                lcmg1 = 0;
            else
                lcmg1 = lcmg + 1;

            /* Determine the fractional difference between the integer location
               and floating point pixel location to be used for interpolation */
            u = (ycmg - lcmg);
            v = (xcmg - scmg);
            one_minus_u = 1.0 - u;
            one_minus_v = 1.0 - v;
            one_minus_u_x_one_minus_v = one_minus_u * one_minus_v;
            one_minus_u_x_v = one_minus_u * v;
            u_x_one_minus_v = u * one_minus_v;
            u_x_v = u * v;

            /* Determine the band ratios and slope/intercept */
            ratio_pix11 = lcmg * RATIO_NBLON + scmg;
            ratio_pix12 = ratio_pix11 + 1;
            ratio_pix21 = lcmg1 * RATIO_NBLON + scmg;
            ratio_pix22 = ratio_pix21 + 1;

            rb1 = ratiob1[ratio_pix11] * 0.001;  /* vs. / 1000. */
            rb2 = ratiob2[ratio_pix11] * 0.001;  /* vs. / 1000. */
            if (rb2 > 1.0 || rb1 > 1.0 || rb2 < 0.1 || rb1 < 0.1)
            {
                slpratiob1[ratio_pix11] = 0;
                slpratiob2[ratio_pix11] = 0;
                slpratiob7[ratio_pix11] = 0;
                intratiob1[ratio_pix11] = 550;
                intratiob2[ratio_pix11] = 600;
                intratiob7[ratio_pix11] = 2000;
            }
            else if (sndwi[ratio_pix11] < 200)
            {
                slpratiob1[ratio_pix11] = 0;
                slpratiob2[ratio_pix11] = 0;
                slpratiob7[ratio_pix11] = 0;
                intratiob1[ratio_pix11] = ratiob1[ratio_pix11];
                intratiob2[ratio_pix11] = ratiob2[ratio_pix11];
                intratiob7[ratio_pix11] = ratiob7[ratio_pix11];
            }

            rb1 = ratiob1[ratio_pix12] * 0.001;  /* vs. / 1000. */
            rb2 = ratiob2[ratio_pix12] * 0.001;  /* vs. / 1000. */
            if (rb2 > 1.0 || rb1 > 1.0 || rb2 < 0.1 || rb1 < 0.1)
            {
                slpratiob1[ratio_pix12] = 0;
                slpratiob2[ratio_pix12] = 0;
                slpratiob7[ratio_pix12] = 0;
                intratiob1[ratio_pix12] = 550;
                intratiob2[ratio_pix12] = 600;
                intratiob7[ratio_pix12] = 2000;
            }
            else if (sndwi[ratio_pix12] < 200)
            {
                slpratiob1[ratio_pix12] = 0;
                slpratiob2[ratio_pix12] = 0;
                slpratiob7[ratio_pix12] = 0;
                intratiob1[ratio_pix12] = ratiob1[ratio_pix12];
                intratiob2[ratio_pix12] = ratiob2[ratio_pix12];
                intratiob7[ratio_pix12] = ratiob7[ratio_pix12];
            }

            rb1 = ratiob1[ratio_pix21] * 0.001;  /* vs. / 1000. */
            rb2 = ratiob2[ratio_pix21] * 0.001;  /* vs. / 1000. */
            if (rb2 > 1.0 || rb1 > 1.0 || rb2 < 0.1 || rb1 < 0.1)
            {
                slpratiob1[ratio_pix21] = 0;
                slpratiob2[ratio_pix21] = 0;
                slpratiob7[ratio_pix21] = 0;
                intratiob1[ratio_pix21] = 550;
                intratiob2[ratio_pix21] = 600;
                intratiob7[ratio_pix21] = 2000;
            }
            else if (sndwi[ratio_pix21] < 200)
            {
                slpratiob1[ratio_pix21] = 0;
                slpratiob2[ratio_pix21] = 0;
                slpratiob7[ratio_pix21] = 0;
                intratiob1[ratio_pix21] = ratiob1[ratio_pix21];
                intratiob2[ratio_pix21] = ratiob2[ratio_pix21];
                intratiob7[ratio_pix21] = ratiob7[ratio_pix21];
            }

            rb1 = ratiob1[ratio_pix22] * 0.001;  /* vs. / 1000. */
            rb2 = ratiob2[ratio_pix22] * 0.001;  /* vs. / 1000. */
            if (rb2 > 1.0 || rb1 > 1.0 || rb2 < 0.1 || rb1 < 0.1)
            {
                slpratiob1[ratio_pix22] = 0;
                slpratiob2[ratio_pix22] = 0;
                slpratiob7[ratio_pix22] = 0;
                intratiob1[ratio_pix22] = 550;
                intratiob2[ratio_pix22] = 600;
                intratiob7[ratio_pix22] = 2000;
            }
            else if (sndwi[ratio_pix22] < 200)
            {
                slpratiob1[ratio_pix22] = 0;
                slpratiob2[ratio_pix22] = 0;
                slpratiob7[ratio_pix22] = 0;
                intratiob1[ratio_pix22] = ratiob1[ratio_pix22];
                intratiob2[ratio_pix22] = ratiob2[ratio_pix22];
                intratiob7[ratio_pix22] = ratiob7[ratio_pix22];
            }

            /* Compute the NDWI variables */
            ndwi_th1 = (andwi[ratio_pix11] + 2.0 *
                        sndwi[ratio_pix11]) * 0.001;
            ndwi_th2 = (andwi[ratio_pix11] - 2.0 *
                        sndwi[ratio_pix11]) * 0.001;

            /* Interpolate the slope/intercept for each band, and unscale */
            slpr11 = slpratiob1[ratio_pix11] * 0.001;  /* vs / 1000 */
            intr11 = intratiob1[ratio_pix11] * 0.001;  /* vs / 1000 */
            slpr12 = slpratiob1[ratio_pix12] * 0.001;  /* vs / 1000 */
            intr12 = intratiob1[ratio_pix12] * 0.001;  /* vs / 1000 */
            slpr21 = slpratiob1[ratio_pix21] * 0.001;  /* vs / 1000 */
            intr21 = intratiob1[ratio_pix21] * 0.001;  /* vs / 1000 */
            slpr22 = slpratiob1[ratio_pix22] * 0.001;  /* vs / 1000 */
            intr22 = intratiob1[ratio_pix22] * 0.001;  /* vs / 1000 */
            slprb1 = slpr11 * one_minus_u_x_one_minus_v +
                     slpr12 * one_minus_u_x_v +
                     slpr21 * u_x_one_minus_v +
                     slpr22 * u_x_v;
            intrb1 = intr11 * one_minus_u_x_one_minus_v +
                     intr12 * one_minus_u_x_v +
                     intr21 * u_x_one_minus_v +
                     intr22 * u_x_v;

            slpr11 = slpratiob2[ratio_pix11] * 0.001;  /* vs / 1000 */
            intr11 = intratiob2[ratio_pix11] * 0.001;  /* vs / 1000 */
            slpr12 = slpratiob2[ratio_pix12] * 0.001;  /* vs / 1000 */
            intr12 = intratiob2[ratio_pix12] * 0.001;  /* vs / 1000 */
            slpr21 = slpratiob2[ratio_pix21] * 0.001;  /* vs / 1000 */
            intr21 = intratiob2[ratio_pix21] * 0.001;  /* vs / 1000 */
            slpr22 = slpratiob2[ratio_pix22] * 0.001;  /* vs / 1000 */
            intr22 = intratiob2[ratio_pix22] * 0.001;  /* vs / 1000 */
            slprb2 = slpr11 * one_minus_u_x_one_minus_v +
                     slpr12 * one_minus_u_x_v +
                     slpr21 * u_x_one_minus_v +
                     slpr22 * u_x_v;
            intrb2 = intr11 * one_minus_u_x_one_minus_v +
                     intr12 * one_minus_u_x_v +
                     intr21 * u_x_one_minus_v +
                     intr22 * u_x_v;

            slpr11 = slpratiob7[ratio_pix11] * 0.001;  /* vs / 1000 */
            intr11 = intratiob7[ratio_pix11] * 0.001;  /* vs / 1000 */
            slpr12 = slpratiob7[ratio_pix12] * 0.001;  /* vs / 1000 */
            intr12 = intratiob7[ratio_pix12] * 0.001;  /* vs / 1000 */
            slpr21 = slpratiob7[ratio_pix21] * 0.001;  /* vs / 1000 */
            intr21 = intratiob7[ratio_pix21] * 0.001;  /* vs / 1000 */
            slpr22 = slpratiob7[ratio_pix22] * 0.001;  /* vs / 1000 */
            intr22 = intratiob7[ratio_pix22] * 0.001;  /* vs / 1000 */
            slprb7 = slpr11 * one_minus_u_x_one_minus_v +
                     slpr12 * one_minus_u_x_v +
                     slpr21 * u_x_one_minus_v +
                     slpr22 * u_x_v;
            intrb7 = intr11 * one_minus_u_x_one_minus_v +
                     intr12 * one_minus_u_x_v +
                     intr21 * u_x_one_minus_v +
                     intr22 * u_x_v;

            /* Calculate NDWI variables for the band ratios */
            xndwi = ((double) sband[SR_S2_BAND8A][curr_pix] -
                     (double) (sband[SR_S2_BAND12][curr_pix] * 0.5)) /
                    ((double) sband[SR_S2_BAND8A][curr_pix] +
                     (double) (sband[SR_S2_BAND12][curr_pix] * 0.5));
            if (xndwi > ndwi_th1)
                xndwi = ndwi_th1;
            if (xndwi < ndwi_th2)
                xndwi = ndwi_th2;

            /* Initialize the band ratios */
            for (ib = 0; ib < NSR_S2_BANDS; ib++)
            {
                erelc[ib] = -1.0;
                troatm[ib] = 0.0;
            }

            /* Compute the band ratio - coastal aerosol, blue, red, SWIR */
            erelc[DN_S2_BAND1] = (xndwi * slprb1 + intrb1);
            erelc[DN_S2_BAND2] = (xndwi * slprb2 + intrb2);
            erelc[DN_S2_BAND4] = 1.0;
            erelc[DN_S2_BAND12] = (xndwi * slprb7 + intrb7);

            /* Retrieve the TOA reflectance values for the current pixel; use
               a NxN average */
            pix_count = 0;
            for (iline = i; iline < i+S2_AERO_WINDOW; iline++)
            {
                if (iline >= nlines) continue;
                for (isamp = j; isamp < j+S2_AERO_WINDOW; isamp++)
                {
                    if (isamp >= nsamps) continue;
                    curr_win_pix = iline * nsamps + isamp;
                    troatm[DN_S2_BAND1] +=
                        toaband[DN_S2_BAND1][curr_win_pix] * SCALE_FACTOR;
                    troatm[DN_S2_BAND2] +=
                        toaband[DN_S2_BAND2][curr_win_pix] * SCALE_FACTOR;
                    troatm[DN_S2_BAND4] +=
                        toaband[DN_S2_BAND4][curr_win_pix] * SCALE_FACTOR;
                    troatm[DN_S2_BAND12] +=
                        toaband[DN_S2_BAND12][curr_win_pix] * SCALE_FACTOR;
                    pix_count++;
                }
            }

            troatm[DN_S2_BAND1] /= pix_count;
            troatm[DN_S2_BAND2] /= pix_count;
            troatm[DN_S2_BAND4] /= pix_count;
            troatm[DN_S2_BAND12] /= pix_count;

            /* Retrieve the aerosol information for low eps 1.0 */
            iband1 = DN_S2_BAND4;  /* red band */
            iband3 = DN_S2_BAND1;  /* coastal aerosol */
            eps = LOW_EPS;
            iaots = 0;
            subaeroret_new (input->meta.sat, iband1, iband3, erelc, troatm,
                tgo_arr, xrorayp_arr, roatm_iaMax, roatm_coef, ttatmg_coef,
                satm_coef, normext_p0a3_arr, &raot, &residual, &iaots, eps);

            /* Save the data */
            residual1 = residual;

            /* Retrieve the aerosol information for moderate eps 1.75 */
            eps = MOD_EPS;
            subaeroret_new (input->meta.sat, iband1, iband3, erelc, troatm,
                tgo_arr, xrorayp_arr, roatm_iaMax, roatm_coef, ttatmg_coef,
                satm_coef, normext_p0a3_arr, &raot, &residual, &iaots, eps);

            /* Save the data */
            residual2 = residual;

            /* Retrieve the aerosol information for high eps 2.5 */
            eps = HIGH_EPS;
            subaeroret_new (input->meta.sat, iband1, iband3, erelc, troatm,
                tgo_arr, xrorayp_arr, roatm_iaMax, roatm_coef, ttatmg_coef,
                satm_coef, normext_p0a3_arr, &raot, &residual, &iaots, eps);

            /* Save the data */
            residual3 = residual;

            /* Find the eps that minimizes the residual */
            xc = residual1 - residual3;
            xf = residual2 - residual3;
            coefa = (xc*xe - xb*xf) / (xa*xe - xb*xd);
            coefb = (xa*xf - xc*xd) / (xa*xe - xb*xd);

            /* Local extremum */
            epsmin = -coefb / (2.0 * coefa);
            resepsmin = xa*epsmin*epsmin + xb*epsmin + xc;
            if ((epsmin < LOW_EPS) || (epsmin > HIGH_EPS))
            {
                if (residual1 < residual3)
                    epsmin = eps1;
                else
                    epsmin = eps3;
            }
            else
            {
                if ((resepsmin > residual1) || (resepsmin > residual3))
                {
                    if (residual1 < residual3)
                        epsmin = eps1;
                    else
                        epsmin = eps3;
                }
            }
            eps = epsmin;

            subaeroret_new (input->meta.sat, iband1, iband3, erelc, troatm,
                tgo_arr, xrorayp_arr, roatm_iaMax, roatm_coef, ttatmg_coef,
                satm_coef, normext_p0a3_arr, &raot, &residual, &iaots, eps);
            corf = raot / xmus;

            /* Check the model residual.  Corf represents aerosol impact.
               Test the quality of the aerosol inversion. */
            if (residual < (0.015 + 0.005 * corf + 0.10 * troatm[DN_S2_BAND7]))
            {
                /* Test if NIR band 8a makes sense. Use a NxN window average. */
                iband = DN_S2_BAND8A;
                rotoa = 0.0;
                pix_count = 0;
                for (iline = i; iline < i+S2_AERO_WINDOW; iline++)
                {
                    if (iline >= nlines) continue;
                    curr_win_pix = iline * nsamps + j;
                    for (isamp = j; isamp < j+S2_AERO_WINDOW;
                         isamp++, curr_win_pix++)
                    {
                        if (isamp >= nsamps) continue;
                        rotoa += toaband[iband][curr_win_pix] * SCALE_FACTOR;
                        pix_count++;
                    }
                }
                rotoa /= pix_count;

                raot550nm = raot;
                atmcorlamb2_new (input->meta.sat, tgo_arr[iband],
                    xrorayp_arr[iband], aot550nm[roatm_iaMax[iband]],
                    &roatm_coef[iband][0], &ttatmg_coef[iband][0],
                    &satm_coef[iband][0], raot550nm, iband,
                    normext_p0a3_arr[iband], rotoa, &roslamb, eps);
                ros5 = roslamb;

                /* Test if red band 4 makes sense. Use a NxN window average. */
                iband = DN_S2_BAND4;
                rotoa = 0.0;
                pix_count = 0;
                for (iline = i; iline < i+S2_AERO_WINDOW; iline++)
                {
                    if (iline >= nlines) continue;
                    curr_win_pix = iline * nsamps + j;
                    for (isamp = j; isamp < j+S2_AERO_WINDOW;
                         isamp++, curr_win_pix++)
                    {
                        if (isamp >= nsamps) continue;
                        rotoa += toaband[iband][curr_win_pix] * SCALE_FACTOR;
                        pix_count++;
                    }
                }
                rotoa /= pix_count;

                raot550nm = raot;
                atmcorlamb2_new (input->meta.sat, tgo_arr[iband],
                    xrorayp_arr[iband], aot550nm[roatm_iaMax[iband]],
                    &roatm_coef[iband][0], &ttatmg_coef[iband][0],
                    &satm_coef[iband][0], raot550nm, iband,
                    normext_p0a3_arr[iband], rotoa, &roslamb, eps);
                ros4 = roslamb;

                /* Use the NDVI to validate the reflectance values or flag
                   as water */
                if ((ros5 > 0.1) && ((ros5 - ros4) / (ros5 + ros4) > 0))
                {
                    /* Clear pixel with valid aerosol retrieval */
                    taero[curr_pix] = raot;
                    teps[curr_pix] = eps;
                    ipflag[curr_pix] = (1 << IPFLAG_CLEAR);
                }
                else
                {
                    /* Flag as failed and fill will a default value */
/*                    taero[curr_pix] = raot;
                    teps[curr_pix] = eps;  */
                    taero[curr_pix] = DEFAULT_AERO;
                    teps[curr_pix] = DEFAULT_EPS;
                    ipflag[curr_pix] = (1 << IPFLAG_FAILED);
                }
            }
            else
            {
                /* Flag as failed and fill will a default value */
/*                taero[curr_pix] = raot;
                teps[curr_pix] = eps; */
                taero[curr_pix] = DEFAULT_AERO;
                teps[curr_pix] = DEFAULT_EPS;
                ipflag[curr_pix] = (1 << IPFLAG_FAILED);
            }

            /* Fill in the remaining taero and teps values for the window,
               using the current pixel */
            for (iline = i; iline < i+S2_AERO_WINDOW; iline++)
            {
                if (iline >= nlines) continue;
                curr_win_pix = iline * nsamps + j;
                for (isamp = j; isamp < j+S2_AERO_WINDOW;
                     isamp++, curr_win_pix++)
                {
                    if (isamp >= nsamps) continue;
                    teps[curr_win_pix] = teps[curr_pix];
                    taero[curr_win_pix] = taero[curr_pix];
                }
            }
        }  /* end for j */
    }  /* end for i */
    /* end aerosol inversion for the NxN window */

#ifndef _OPENMP
    /* update status */
    printf ("100%%\n");
    fflush (stdout);
#endif

    /* Done with the ratiob* arrays */
    free (andwi);  andwi = NULL;
    free (sndwi);  sndwi = NULL;
    free (ratiob1);  ratiob1 = NULL;
    free (ratiob2);  ratiob2 = NULL;
    free (ratiob7);  ratiob7 = NULL;
    free (intratiob1);  intratiob1 = NULL;
    free (intratiob2);  intratiob2 = NULL;
    free (intratiob7);  intratiob7 = NULL;
    free (slpratiob1);  slpratiob1 = NULL;
    free (slpratiob2);  slpratiob2 = NULL;
    free (slpratiob7);  slpratiob7 = NULL;

    /* Done with the DEM, water vapor, and ozone arrays */
    free (dem);  dem = NULL;
    free (wv);  wv = NULL;
    free (oz);  oz = NULL;

#ifdef WRITE_TAERO
    /* Write the ipflag values for comparison with other algorithms */
    aero_fptr = fopen ("ipflag.img", "w");
    fwrite (ipflag, nlines*nsamps, sizeof (uint8), aero_fptr);
    fclose (aero_fptr);

    /* Write the aerosol values for comparison with other algorithms */
    aero_fptr = fopen ("aerosols.img", "w");
    fwrite (taero, nlines*nsamps, sizeof (float), aero_fptr);
    fclose (aero_fptr);
#endif

    /* Find the median of the clear aerosols */
    mytime = time(NULL);
    printf ("Computing median of clear pixels in NxN windows %s",
        ctime(&mytime)); fflush(stdout);
    median_aerosol = find_median_aerosol_s2 (ipflag, taero, S2_AERO_WINDOW,
        nlines, nsamps);
    if (median_aerosol == 0.0)
    {   /* error message already printed */
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    printf ("Median aerosol value for clear aerosols is %f\n", median_aerosol);

    /* Fill the cloud, shadow, barren, urban, water pixels, etc. (any pixel
       which failed aerosol retrieval) with the median aerosol value instead
       of the current default aerosol value */
    mytime = time(NULL);
    printf ("Fill non-clear aerosol values in NxN windows with the median %s",
        ctime(&mytime)); fflush(stdout);
    aerosol_fill_median_s2 (ipflag, taero, S2_AERO_WINDOW, median_aerosol,
        nlines, nsamps);
    aerosol_fill_median_s2 (ipflag, teps, S2_AERO_WINDOW, DEFAULT_EPS,
        nlines, nsamps);

#ifdef WRITE_TAERO
    /* Write the ipflag values for comparison with other algorithms */
    aero_fptr = fopen ("ipflag2.img", "w");
    fwrite (ipflag, nlines*nsamps, sizeof (uint8), aero_fptr);
    fclose (aero_fptr);

    /* Write the aerosol values for comparison with other algorithms */
    aero_fptr = fopen ("aerosols2.img", "w");
    fwrite (taero, nlines*nsamps, sizeof (float), aero_fptr);
    fclose (aero_fptr);
#endif

    /* Use the UL corner of the aerosol windows to interpolate the remaining
       aerosol pixels in the window */
    mytime = time(NULL);
    printf ("Interpolating the aerosol values in the NxN windows %s\n",
        ctime(&mytime)); fflush(stdout);
    aerosol_interp_s2 (S2_AERO_WINDOW, ipflag, taero, nlines, nsamps);

    /* Use the UL corner of the aerosol windows to interpolate the teps values
       (angstrom coefficient) */
    mytime = time(NULL);
    printf ("Interpolating the teps values in the NxN windows %s\n",
        ctime(&mytime)); fflush(stdout);
    aerosol_interp_s2 (S2_AERO_WINDOW, ipflag, teps, nlines, nsamps);

#ifdef WRITE_TAERO
    /* Write the ipflag values for comparison with other algorithms */
    aero_fptr = fopen ("ipflag3.img", "w");
    fwrite (ipflag, nlines*nsamps, sizeof (uint8), aero_fptr);
    fclose (aero_fptr);

    /* Write the aerosol values for comparison with other algorithms */
    aero_fptr = fopen ("aerosols3.img", "w");
    fwrite (taero, nlines*nsamps, sizeof (float), aero_fptr);
    fclose (aero_fptr);
#endif

    /* Perform the second level of atmospheric correction using the aerosols */
    mytime = time(NULL);
    printf ("Performing atmospheric correction ... %s\n", ctime(&mytime));

    /* Loop through all the bands */
    for (ib = 0; ib <= DN_S2_BAND12; ib++)
    {
        printf ("  Band %s\n", S2_BANDNAME[ib]); fflush(stdout);
#ifdef _OPENMP
        #pragma omp parallel for private (i, j, curr_pix, rsurf, rotoa, raot550nm, eps, retval, tmpf, roslamb, tgo, roatm, ttatmg, satm, xrorayp, next)
#endif
        for (i = 0; i < nlines; i++)
        {
            curr_pix = i * nsamps;
            for (j = 0; j < nsamps; j++, curr_pix++)
            {
                /* If this pixel is fill, then don't process */
                if (level1_qa_is_fill (qaband[curr_pix]))
                    continue;

                /* Correct all pixels */
                rotoa = toaband[ib][curr_pix] * SCALE_FACTOR;
                raot550nm = taero[curr_pix];
                eps = teps[curr_pix];
                atmcorlamb2_new (input->meta.sat, tgo_arr[ib],
                    xrorayp_arr[ib], aot550nm[roatm_iaMax[ib]],
                    &roatm_coef[ib][0], &ttatmg_coef[ib][0],
                    &satm_coef[ib][0], raot550nm, ib, normext_p0a3_arr[ib],
                    rotoa, &roslamb, eps);

                /* If this is the coastal aerosol band then set the aerosol
                   bits in the QA band */
                if (ib == DN_S2_BAND1)
                {
                    /* Set up aerosol QA bits */
                    rsurf = sband[ib][curr_pix] * SCALE_FACTOR;
                    tmpf = fabs (rsurf - roslamb);
                    if (tmpf <= 0.015)
                    {  /* Set first aerosol bit (low aerosols) */
                        ipflag[curr_pix] |= (1 << AERO1_QA);
                    }
                    else
                    {
                        if (tmpf < 0.03)
                        {  /* Set second aerosol bit (average aerosols) */
                            ipflag[curr_pix] |= (1 << AERO2_QA);
                        }
                        else
                        {  /* Set both aerosol bits (high aerosols) */
                            ipflag[curr_pix] |= (1 << AERO1_QA);
                            ipflag[curr_pix] |= (1 << AERO2_QA);
                        }
                    }
                }  /* end if this is the coastal aerosol band */

                /* Save the scaled surface reflectance value, but make sure
                   it falls within the defined valid range. */
                roslamb = roslamb * MULT_FACTOR;  /* scale the value */
                if (roslamb < MIN_VALID)
                    sband[ib][curr_pix] = MIN_VALID;
                else if (roslamb > MAX_VALID)
                    sband[ib][curr_pix] = MAX_VALID;
                else
                    sband[ib][curr_pix] = (int) (roundf (roslamb));
            }  /* end for j */
        }  /* end for i */
    }  /* end for ib */

    /* Free memory for arrays no longer needed */
    free (taero);
    free (teps);
 
    /* Write the data to the output file */
    mytime = time(NULL);
    printf ("Writing surface reflectance corrected data to the output "
        "files ... %s", ctime(&mytime)); fflush(stdout);

    /* Open the output file */
    sr_output = open_output (xml_metadata, input, OUTPUT_SR);
    if (sr_output == NULL)
    {   /* error message already printed */
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Loop through the reflectance bands and write the data */
    for (ib = 0; ib <= DN_S2_BAND12; ib++)
    {
        printf ("  Band %s: %s\n", S2_BANDNAME[ib],
            sr_output->metadata.band[ib].file_name);
        if (put_output_lines (sr_output, sband[ib], ib, 0, nlines,
            sizeof (int16)) != SUCCESS)
        {
            sprintf (errmsg, "Writing output data for band %d", ib);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Create the ENVI header file this band */
        if (create_envi_struct (&sr_output->metadata.band[ib],
            &xml_metadata->global, &envi_hdr) != SUCCESS)
        {
            sprintf (errmsg, "Creating ENVI header structure.");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Write the ENVI header */
        strcpy (envi_file, sr_output->metadata.band[ib].file_name);
        cptr = strchr (envi_file, '.');
        strcpy (cptr, ".hdr");
        if (write_envi_hdr (envi_file, &envi_hdr) != SUCCESS)
        {
            sprintf (errmsg, "Writing ENVI header file.");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Append the surface reflectance bands to the XML file */
    if (append_metadata (NREFL_S2_BANDS, sr_output->metadata.band, xml_infile)
        != SUCCESS)
    {
        sprintf (errmsg, "Appending surface reflectance bands to the "
            "XML file.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Write the aerosol QA band */
    printf ("  Aerosol Band %d: %s\n", SR_S2_AEROSOL+1,
            sr_output->metadata.band[SR_S2_AEROSOL].file_name);
    if (put_output_lines (sr_output, ipflag, SR_S2_AEROSOL, 0, nlines,
        sizeof (uint8)) != SUCCESS)
    {
        sprintf (errmsg, "Writing aerosol QA output data");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Free memory for ipflag data */
    free (ipflag);

    /* Create the ENVI header for the aerosol QA band */
    if (create_envi_struct (&sr_output->metadata.band[SR_S2_AEROSOL],
        &xml_metadata->global, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Creating ENVI header structure.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Write the ENVI header */
    strcpy (envi_file, sr_output->metadata.band[SR_S2_AEROSOL].file_name);
    cptr = strchr (envi_file, '.');
    strcpy (cptr, ".hdr");
    if (write_envi_hdr (envi_file, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Writing ENVI header file.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Append the aerosol QA band to the XML file */
    if (append_metadata (1, &sr_output->metadata.band[SR_S2_AEROSOL],
        xml_infile) != SUCCESS)
    {
        sprintf (errmsg, "Appending aerosol QA band to XML file.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Close the output surface reflectance products */
    close_output (sat, sr_output, OUTPUT_SR);
    free_output (sr_output, OUTPUT_SR);

    /* Free the spatial mapping pointer */
    free (space);

    /* Free the data arrays */
    free (rolutt);
    free (transt);
    free (sphalbt);
    free (normext);
    free (tsmax);
    free (tsmin);
    free (nbfic);
    free (nbfi);
    free (ttv);

    /* Successful completion */
    mytime = time(NULL);
    printf ("Surface reflectance correction complete ... %s\n", ctime(&mytime));
    return (SUCCESS);
}

