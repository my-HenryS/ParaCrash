/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of h5check. The full h5check copyright notice,          *
 * including terms governing use, modification, and redistribution, is       *
 * contained in the file COPYING, which can be found at the root of the      *
 * source code distribution tree.  If you do not have access to this file,   *
 * you may request a copy from help@hdfgroup.org.                            *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "h5_check.h"
#include "h5_error.h"
#include "h5_pline.h"

#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif
#ifdef HAVE_SZLIB_H
#include <szlib.h>
#endif

#include <math.h>

/*
 * The filter code is copied mainly from the library
 */
ck_size_t Z_table_alloc_g = 0;
ck_size_t Z_table_used_g = 0;
Z_class_t *Z_table_g = NULL;

static ck_err_t pline_register(const Z_class_t *);
static int filter_find_idx(Z_filter_t);

static ck_err_t
pline_register(const Z_class_t *cls)
{
    ck_size_t i;
    ck_err_t ret_value = SUCCEED;

    assert(cls);

    if((cls->id < 0) || (cls->id > Z_FILTER_MAX)) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC,
	    "Registering filter:Invalid filter id", -1, NULL);
	CK_SET_RET_DONE(FAIL)
    }

    for(i = 0; i < Z_table_used_g; i++)
        if(Z_table_g[i].id == cls->id)
            break;

    /* Filter not already registered */
    if(i >= Z_table_used_g) {
        if(Z_table_used_g >= Z_table_alloc_g) {
            ck_size_t n = MAX(Z_MAX_NFILTERS, 2*Z_table_alloc_g);
            Z_class_t *table = realloc(Z_table_g, n*sizeof(Z_class_t));
            if(!table) {
		error_push(ERR_INTERNAL, ERR_NONE_SEC,
		    "Registering filter:Unable to extend filter table", -1, NULL);
		    CK_SET_RET_DONE(FAIL)
	    }
            Z_table_g = table;
            Z_table_alloc_g = n;
        } 

        i = Z_table_used_g++;
        memcpy(Z_table_g+i, cls, sizeof(Z_class_t));
    } else { /* Filter already registered */
        memcpy(Z_table_g+i, cls, sizeof(Z_class_t));
    }

done:
    return(ret_value);
}



ck_err_t
pline_init_interface(void)
{
    ck_err_t ret_value = SUCCEED;    

    if(debug_verbose())
        printf("INITIALIZING filters ...\n");

#ifdef HAVE_FILTER_DEFLATE
    if(pline_register(Z_DEFLATE) < SUCCEED) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Unable to register deflate filter", -1, NULL);
	CK_SET_RET(FAIL)
    }
#endif

#ifdef HAVE_FILTER_SHUFFLE
    if(pline_register(Z_SHUFFLE) < 0) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Unable to register shuffle filter", -1, NULL);
	CK_SET_RET(FAIL)
    }
#endif

#ifdef HAVE_FILTER_FLETCHER32
    if(pline_register(Z_FLETCHER32) < 0) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Unable to register fletcher32 filter", -1, NULL);
	CK_SET_RET(FAIL)
    }
#endif

#ifdef HAVE_FILTER_SZIP
    if(pline_register(Z_SZIP) < 0) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Unable to register szip filter", -1, NULL);
	CK_SET_RET(FAIL)
    }
#endif

#ifdef HAVE_FILTER_NBIT
    if(pline_register(Z_NBIT) < 0) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Unable to register nbit filter", -1, NULL);
	CK_SET_RET(FAIL)
    }
#endif

#ifdef HAVE_FILTER_SCALEOFFSET
    if(pline_register(Z_SCALEOFFSET) < 0) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Unable to register scaleoffset filter", -1, NULL);
	CK_SET_RET(FAIL)
    };
#endif

done:
    return(ret_value);
} /* pline_init_interface() */

void
pline_free(void)
{
    if(Z_table_g) free(Z_table_g);
}

#ifdef HAVE_FILTER_DEFLATE
/* 
 * Deflate filter
 */
static ck_size_t Z_filter_deflate(unsigned flags, ck_size_t cd_nelmts, const unsigned cd_values[], 
	ck_size_t nbytes, ck_size_t *buf_size, void **buf);

Z_class_t Z_DEFLATE[1] = {{
    Z_CLASS_T_VERS,     /* Z_class_t version */
    Z_FILTER_DEFLATE,  	/* Filter id number             */
    Z_filter_deflate,   /* The actual filter function   */
}};

static ck_size_t
Z_filter_deflate(unsigned flags, ck_size_t cd_nelmts, const unsigned cd_values[], ck_size_t nbytes, ck_size_t *buf_size, void **buf)
{
    void        	*outbuf = NULL;         /* Pointer to new buffer */
    int         	status;                 /* Status from zlib operation */
    ck_size_t      	ret_value;              /* Return value */

    if(debug_verbose())
        printf("Applying deflate filter ...\n");

    if(cd_nelmts != 1 || cd_values[0] > 9) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Deflate filter:Invalid level", -1, NULL);
	CK_SET_RET_DONE(FAIL)
    }

    if(flags & Z_FLAG_REVERSE) { /* Input, uncompress */
        z_stream        z_strm;                 /* zlib parameters */
        ck_size_t       nalloc = *buf_size;     /* Number of bytes for output (compressed) buffer */

        /* Allocate space for the compressed buffer */
        if (NULL == (outbuf = malloc(nalloc))) {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Deflate filter:Internal allocation error", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}

        /* Set the uncompression parameters */
        memset(&z_strm, 0, sizeof(z_strm));
        z_strm.next_in = *buf;
        ASSIGN_OVERFLOW(z_strm.avail_in,nbytes,size_t,uInt);
        z_strm.next_out = outbuf;
        ASSIGN_OVERFLOW(z_strm.avail_out,nalloc,size_t,uInt);

        /* Initialize the uncompression routines */
        if(Z_OK != inflateInit(&z_strm)) {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Deflate filter:Initialization failed", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}

        /* Loop to uncompress the buffer */
        do {
            /* Uncompress some data */
            status = inflate(&z_strm, Z_SYNC_FLUSH);

            /* Check if we are done uncompressing data */
            if(Z_STREAM_END==status)
                break;  /*done*/

            /* Check for error */
            if(Z_OK != status) {
                (void)inflateEnd(&z_strm);
		error_push(ERR_INTERNAL, ERR_NONE_SEC, "Deflate filter:Inflate failed", -1, NULL);
		CK_SET_RET_DONE(FAIL)
            }
            else {
                /* If we're not done and just ran out of buffer space, get more */
                if(0 == z_strm.avail_out) {
                    void        *new_outbuf;         /* Pointer to new output buffer */

                    /* Allocate a buffer twice as big */
                    nalloc *= 2;
                    if(NULL == (new_outbuf = realloc(outbuf, nalloc))) {
                        (void)inflateEnd(&z_strm);
			error_push(ERR_INTERNAL, ERR_NONE_SEC, "Deflate filter:Internal allocation error", -1, NULL);
			CK_SET_RET_DONE(FAIL)
                    } /* end if */
                    outbuf = new_outbuf;

                    /* Update pointers to buffer for next set of uncompressed data */
                    z_strm.next_out = (unsigned char*)outbuf + z_strm.total_out;
                    z_strm.avail_out = (uInt)(nalloc - z_strm.total_out);
                } /* end if */
            } /* end else */
        } while(status==Z_OK);

        /* Free the input buffer */
        free(*buf);

        /* Set return values */
        *buf = outbuf;
        outbuf = NULL;
        *buf_size = nalloc;
        ret_value = z_strm.total_out;

        /* Finish uncompressing the stream */
        (void)inflateEnd(&z_strm);
    } else {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Deflate filter:Invalid operation", -1, NULL);
	CK_SET_RET_DONE(FAIL)
    }


done:
    if(outbuf)
        free(outbuf);

    return(ret_value);
}
#endif

#ifdef HAVE_FILTER_SHUFFLE
/*
 * Shuffle filter
 */
static ck_size_t Z_filter_shuffle(unsigned flags, ck_size_t cd_nelmts, const unsigned cd_values[],
	ck_size_t nbytes, ck_size_t *buf_size, void **buf);

Z_class_t Z_SHUFFLE[1] = {{
    Z_CLASS_T_VERS,       /* Z_class_t version */
    Z_FILTER_SHUFFLE,     /* Filter id number             */
    Z_filter_shuffle,     /* The actual filter function   */
}};

static size_t
Z_filter_shuffle(unsigned flags, ck_size_t cd_nelmts, const unsigned cd_values[], ck_size_t nbytes, ck_size_t *buf_size, void **buf)
{
    void 		*dest = NULL;   /* Buffer to deposit [un]shuffled bytes into */
    unsigned char 	*_src=NULL;   	/* Alias for source buffer */
    unsigned char 	*_dest=NULL;  	/* Alias for destination buffer */
    unsigned 		bytesoftype;    /* Number of bytes per element */
    ck_size_t 		numofelements;       /* Number of elements in buffer */
    ck_size_t 		i;                   /* Local index variables */
#ifdef NO_DUFFS_DEVICE
    ck_size_t 		j;                   /* Local index variable */
#endif /* NO_DUFFS_DEVICE */
    ck_size_t 		leftover;            /* Extra bytes at end of buffer */
    ck_size_t 		ret_value;           /* Return value */


    if(debug_verbose())
        printf("Applying shuffle filter ...\n");

    /* Check arguments */
    if(cd_nelmts != Z_SHUFFLE_TOTAL_NPARMS || cd_values[Z_SHUFFLE_PARM_SIZE] == 0) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Shuffle filter:Invalid size", -1, NULL);
	CK_SET_RET_DONE(FAIL)
    }

    /* Get the number of bytes per element from the parameter block */
    bytesoftype = cd_values[Z_SHUFFLE_PARM_SIZE];

    /* Compute the number of elements in buffer */
    numofelements = nbytes/bytesoftype;

    /* Don't do anything for 1-byte elements, or "fractional" elements */
    if(bytesoftype > 1 && numofelements > 1) {
        /* Compute the leftover bytes if there are any */
        leftover = nbytes%bytesoftype;

        /* Allocate the destination buffer */
        if(NULL == (dest = malloc(nbytes))) {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Shuffle filter:Internal allocation error", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}

        if(flags & Z_FLAG_REVERSE) {
            /* Get the pointer to the source buffer */
            _src =(unsigned char *)(*buf);

            /* Input; unshuffle */
            for(i=0; i < bytesoftype; i++) {
                _dest=((unsigned char *)dest)+i;
#define DUFF_GUTS                                 \
    *_dest =* _src++;                             \
    _dest += bytesoftype;
#ifdef NO_DUFFS_DEVICE
                j = numofelements;
                while(j > 0) {
                    DUFF_GUTS;

                    j--;
                } /* end for */
#else /* NO_DUFFS_DEVICE */
            {
                ck_size_t duffs_index; /* Counting index for Duff's device */

                duffs_index = (numofelements + 7) / 8;
                switch (numofelements % 8) {
                    case 0:
                        do
                          {
                            DUFF_GUTS
                    case 7:
                            DUFF_GUTS
                    case 6:
                            DUFF_GUTS
                    case 5:
                            DUFF_GUTS
                    case 4:
                            DUFF_GUTS
                    case 3:
                            DUFF_GUTS
                    case 2:
                            DUFF_GUTS
                    case 1:
                            DUFF_GUTS
                      } while (--duffs_index > 0);
                } /* end switch */
            }
#endif /* NO_DUFFS_DEVICE */
#undef DUFF_GUTS
            } /* end for */

            /* Add leftover to the end of data */
            if(leftover > 0) {
                /* Adjust back to end of shuffled bytes */
                _dest -= (bytesoftype - 1);     /*lint !e794 _dest is initialized */
                memcpy((void*)_dest, (void*)_src, leftover);
            }
        } else {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Shuffle filter:Invalid operation", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}

        /* Free the input buffer */
        free(*buf);

        /* Set the buffer information to return */
        *buf = dest;
        *buf_size=nbytes;
    } /* end else */

    /* Set the return value */
    ret_value = nbytes;

done:
    return(ret_value);
}
#endif


#ifdef HAVE_FILTER_FLETCHER32

/* 
 * Fletcher32 filter 
 */
static ck_size_t Z_filter_fletcher32 (unsigned flags, ck_size_t cd_nelmts, const unsigned cd_values[], 
	ck_size_t nbytes, ck_size_t *buf_size, void **buf);

static uint32_t checksum_fletcher32(const void *, ck_size_t);

/* This message derives from H5Z */
Z_class_t Z_FLETCHER32[1] = {{
    Z_CLASS_T_VERS,       /* Z_class_t version */
    Z_FILTER_FLETCHER32,      /* Filter id number             */
    Z_filter_fletcher32,      /* The actual filter function   */
}};

/* suppport routine for Fletcher32 filter  */
static uint32_t
checksum_fletcher32(const void *_data, ck_size_t _len)
{
    const uint8_t 	*data = (const uint8_t *)_data;  /* Pointer to the data to be summed */
    ck_size_t 		len = _len / 2;      /* Length in 16-bit words */
    uint32_t 		sum1 = 0, sum2 = 0;

    assert(_data);
    assert(_len > 0);

    /* Compute checksum for pairs of bytes */
    /* (the magic "360" value is is the largest number of sums that can be
     *  performed without numeric overflow)
     */
    while(len) {
        unsigned tlen = len > 360 ? 360 : len;
        len -= tlen;
        do {
            sum1 += (((uint16_t)data[0]) << 8) | ((uint16_t)data[1]);
            data += 2;
            sum2 += sum1;
        } while (--tlen);
        sum1 = (sum1 & 0xffff) + (sum1 >> 16);
        sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    }

    /* Check for odd # of bytes */
    if(_len % 2) {
        sum1 += ((uint16_t)*data) << 8;
        sum2 += sum1;
        sum1 = (sum1 & 0xffff) + (sum1 >> 16);
        sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    } /* end if */

    /* Second reduction step to reduce sums to 16 bits */
    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);

    return((sum2 << 16) | sum1);
}


/* 
 * Fletcher32 filter 
 */
static ck_size_t
Z_filter_fletcher32(unsigned flags, ck_size_t UNUSED cd_nelmts, const unsigned UNUSED cd_values[],
    ck_size_t nbytes, size_t *buf_size, void **buf)
{
    void    	*outbuf = NULL;     /* Pointer to new buffer */
    unsigned 	char *src = (unsigned char*)(*buf);
    uint32_t 	fletcher;          /* Checksum value */
    uint32_t 	reversed_fletcher; /* Possible wrong checksum value */
    uint8_t  	c[4];
    uint8_t  	tmp;
    ck_size_t   ret_value;         /* Return value */

    assert(sizeof(uint32_t)>=4);

    if(debug_verbose())
        printf("Applying fletcher32 filter ...\n");

    if(flags & Z_FLAG_REVERSE) { /* Read */
        if(!(flags & Z_FLAG_SKIP_EDC)) {
            unsigned 	char *tmp_src;             /* Pointer to checksum in buffer */
            ck_size_t  	src_nbytes = nbytes;       /* Original number of bytes */
            uint32_t 	stored_fletcher;           /* Stored checksum value */

            /* Get the stored checksum */
            src_nbytes -= FLETCHER_LEN;
            tmp_src = src + src_nbytes;
            UINT32DECODE(tmp_src, stored_fletcher);

            /* Compute checksum (can't fail) */
            fletcher = checksum_fletcher32(src, src_nbytes);
            memcpy(c, &fletcher, (ck_size_t)4);

            tmp  = c[1];
            c[1] = c[0];
            c[0] = tmp;

            tmp  = c[3];
            c[3] = c[2];
            c[2] = tmp;

            memcpy(&reversed_fletcher, c, (ck_size_t)4);

            /* Verify computed checksum matches stored checksum */
            if(stored_fletcher != fletcher && stored_fletcher != reversed_fletcher) {
		error_push(ERR_INTERNAL, ERR_NONE_SEC, "Fletcher32 filterfilter:Data error", -1, NULL);
		CK_SET_RET_DONE(FAIL)
	    }
        }

        /* Set return values */
        /* (Re-use the input buffer, just note that the size is smaller by the size of the checksum) */
        ret_value = nbytes - FLETCHER_LEN;
    } else {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Fletcher32 filterfilter:Invalid operation", -1, NULL);
	CK_SET_RET_DONE(FAIL)
    }

done:
    if(outbuf)
        free(outbuf);
    return(ret_value);
}
#endif


#ifdef HAVE_FILTER_SZIP
/* 
 * szip filter 
 */
static ck_size_t Z_filter_szip (unsigned flags, ck_size_t cd_nelmts, const unsigned cd_values[], 
	ck_size_t nbytes, ck_size_t *buf_size, void **buf);

Z_class_t Z_SZIP[1] = {{
    Z_CLASS_T_VERS,       /* Z_class_t version */
    Z_FILTER_SZIP,        /* Filter id number             */
    Z_filter_szip,        /* The actual filter function   */
}};

static ck_size_t
Z_filter_szip (unsigned flags, ck_size_t cd_nelmts, const unsigned cd_values[],
    ck_size_t nbytes, ck_size_t *buf_size, void **buf)
{
    ck_size_t 	ret_value = 0;       /* Return value */
    ck_size_t 	size_out  = 0;       /* Size of output buffer */
    unsigned 	char *outbuf = NULL;    /* Pointer to new output buffer */
    unsigned 	char *newbuf = NULL;    /* Pointer to input buffer */
    SZ_com_t 	sz_param;          /* szip parameter block */

    if(debug_verbose())
        printf("Applying szip filter ...\n");

    if(cd_nelmts != 4) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Szip filter:Invalid level", -1, NULL);
	CK_SET_RET_DONE(FAIL)
    }

    /* Copy the filter parameters into the szip parameter block */
    ASSIGN_OVERFLOW(sz_param.options_mask,cd_values[Z_SZIP_PARM_MASK],unsigned,int);
    ASSIGN_OVERFLOW(sz_param.bits_per_pixel,cd_values[Z_SZIP_PARM_BPP],unsigned,int);
    ASSIGN_OVERFLOW(sz_param.pixels_per_block,cd_values[Z_SZIP_PARM_PPB],unsigned,int);
    ASSIGN_OVERFLOW(sz_param.pixels_per_scanline,cd_values[Z_SZIP_PARM_PPS],unsigned,int);

    /* Input; uncompress */
    if(flags & Z_FLAG_REVERSE) {
        uint32_t stored_nalloc;  /* Number of bytes the compressed block will expand into */
        ck_size_t nalloc;  /* Number of bytes the compressed block will expand into */

        /* Get the size of the uncompressed buffer */
        newbuf = *buf;
        UINT32DECODE(newbuf,stored_nalloc);
        ASSIGN_OVERFLOW(nalloc,stored_nalloc,uint32_t,size_t);

        /* Allocate space for the uncompressed buffer */
        if(NULL==(outbuf = malloc(nalloc))) {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Szip filter:Internal allocation error", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}

        /* Decompress the buffer */
        size_out = nalloc;
        if(SZ_BufftoBuffDecompress(outbuf, &size_out, newbuf, nbytes-4, &sz_param) != SZ_OK) {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Szip filter:Szip failed", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}
        assert(size_out==nalloc);

        /* Free the input buffer */
        free(*buf);

        /* Set return values */
        *buf = outbuf;
        outbuf = NULL;
        *buf_size = nalloc;
        ret_value = nalloc;
    } else {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Szip filter:Invalid operation", -1, NULL);
	CK_SET_RET_DONE(FAIL)
    }

done:
    if(outbuf)
        free(outbuf);
    return(ret_value);
}
#endif


#ifdef HAVE_FILTER_NBIT

/* 
 * nbit filter 
 */
static size_t Z_filter_nbit(unsigned flags, ck_size_t cd_nelmts, const unsigned cd_values[],
    ck_size_t nbytes, ck_size_t *buf_size, void **buf);

Z_class_t Z_NBIT[1] = {{
    Z_CLASS_T_VERS,       /* Z_class_t version */
    Z_FILTER_NBIT,        /* Filter id number             */
    Z_filter_nbit,        /* The actual filter function   */
}};

/* 
 * Struct of parameters needed for compressing/decompressing
 * one nbit atomic datatype: integer or floating-point
 */
typedef struct {
   size_t size;   /* size of datatype */
   int order;     /* datatype endianness order */
   int precision; /* datatype precision */
   int offset;    /* datatype offset */
} nbit_parms_atomic;

static unsigned parms_index = 0;

static void Z_nbit_next_byte(ck_size_t *, int *);
static void Z_nbit_decompress_one_byte(unsigned char *, ck_size_t, int, int, int, unsigned char *, ck_size_t *, int *, nbit_parms_atomic, int);
static void Z_nbit_decompress_one_nooptype(unsigned char *, ck_size_t, unsigned char *, ck_size_t *, int *, unsigned);
static void Z_nbit_decompress_one_atomic(unsigned char *, ck_size_t, unsigned char *, ck_size_t *, int *, nbit_parms_atomic);
static void Z_nbit_decompress_one_compound(unsigned char *data, size_t data_offset, unsigned char *buffer, size_t *j, int *buf_len, const unsigned parms[]);
static void Z_nbit_decompress_one_array(unsigned char *data, size_t data_offset, unsigned char *buffer, size_t *j, int *buf_len, const unsigned parms[]);
static void Z_nbit_decompress(unsigned char *, unsigned, unsigned char *, const unsigned parms[]);


/* support routine for nbit filter */
static void
Z_nbit_next_byte(ck_size_t *j, int *buf_len)
{
   ++(*j);
   *buf_len = 8 * sizeof(unsigned char);
}

/* support routine for nbit filter */
static void 
Z_nbit_decompress_one_byte(unsigned char *data, ck_size_t data_offset, int k, int begin_i, int end_i, unsigned char *buffer, ck_size_t *j, int *buf_len, nbit_parms_atomic p, int datatype_len)
{
   int dat_len; /* dat_len is the number of bits to be copied in each data byte */
   int uchar_offset;
   unsigned char val; /* value to be copied in each data byte */

   /* initialize value and bits of unsigned char to be copied */
   val = buffer[*j];
   uchar_offset = 0;

   if(begin_i != end_i) { /* significant bits occupy >1 unsigned char */
      if(k == begin_i)
         dat_len = 8 - (datatype_len - p.precision - p.offset) % 8;
      else if(k == end_i) {
         dat_len = 8 - p.offset %8;
         uchar_offset = 8 - dat_len;
      }
      else
         dat_len = 8;
   } else { /* all significant bits in one unsigned char */
      uchar_offset = p.offset % 8;
      dat_len = p.precision;
   }

   if(*buf_len > dat_len) {
      data[data_offset + k] =
      ((val >> (*buf_len - dat_len)) & ~(~0 << dat_len)) << uchar_offset;
      *buf_len -= dat_len;
   } else {
      data[data_offset + k] =
      ((val & ~(~0 << *buf_len)) << (dat_len - *buf_len)) << uchar_offset;
      dat_len -= *buf_len;
      Z_nbit_next_byte(j, buf_len);
      if(dat_len == 0) return;

      val = buffer[*j];
      data[data_offset + k] |=
      ((val >> (*buf_len - dat_len)) & ~(~0 << dat_len)) << uchar_offset;
      *buf_len -= dat_len;
   }
}

/* support routine for nbit filter */
static void 
Z_nbit_decompress_one_nooptype(unsigned char *data, ck_size_t data_offset,
    unsigned char *buffer, ck_size_t *j, int *buf_len, unsigned size)
{
   unsigned i;        /* index */
   unsigned dat_len;  /* dat_len is the number of bits to be copied in each data byte */
   unsigned char val; /* value to be copied in each data byte */

   for(i = 0; i < size; i++) {
      /* initialize value and bits of unsigned char to be copied */
      val = buffer[*j];
      dat_len = sizeof(unsigned char) * 8;

      data[data_offset + i] = ((val & ~(~0 << *buf_len)) << (dat_len - *buf_len));
      dat_len -= *buf_len;
      Z_nbit_next_byte(j, buf_len);
      if (dat_len == 0) continue;

      val = buffer[*j];
      data[data_offset + i] |= ((val >> (*buf_len - dat_len)) & ~(~0 << dat_len));
      *buf_len -= dat_len;
   }
}


/* support routine for nbit filter */
static void 
Z_nbit_decompress_one_atomic(unsigned char *data, ck_size_t data_offset,
	unsigned char *buffer, ck_size_t *j, int *buf_len, nbit_parms_atomic p)
{
   /* begin_i: the index of byte having first significant bit
      end_i: the index of byte having last significant bit */
   int k, begin_i, end_i, datatype_len;

   datatype_len = p.size * 8;

   if(p.order == Z_NBIT_ORDER_LE) { /* little endian */
      /* calculate begin_i and end_i */
      if((p.precision + p.offset) % 8 != 0)
         begin_i = (p.precision + p.offset) / 8;
      else
         begin_i = (p.precision + p.offset) / 8 - 1;
      end_i = p.offset / 8;

      for(k = begin_i; k >= end_i; k--)
         Z_nbit_decompress_one_byte(data, data_offset, k, begin_i, end_i, buffer, j, buf_len, p, datatype_len);
   }

   if(p.order == Z_NBIT_ORDER_BE) { /* big endian */
      /* calculate begin_i and end_i */
      begin_i = (datatype_len - p.precision - p.offset) / 8;
      if(p.offset % 8 != 0)
         end_i = (datatype_len - p.offset) / 8;
      else
         end_i = (datatype_len - p.offset) / 8 - 1;

      for(k = begin_i; k <= end_i; k++)
         Z_nbit_decompress_one_byte(data, data_offset, k, begin_i, end_i, buffer, j, buf_len, p, datatype_len);
   }
}


/* support routine for nbit filter */
static void 
Z_nbit_decompress_one_compound(unsigned char *data, ck_size_t data_offset,
    unsigned char *buffer, ck_size_t *j, int *buf_len, const unsigned parms[])
{
   unsigned i, nmembers, member_offset, member_class, size;
   nbit_parms_atomic p;

   parms_index++; /* skip total size of compound datatype */
   nmembers = parms[parms_index++];

   for(i = 0; i < nmembers; i++) {
      member_offset = parms[parms_index++];
      member_class = parms[parms_index++];
      switch(member_class) {
         case Z_NBIT_ATOMIC:
              p.size = parms[parms_index++];
              p.order = parms[parms_index++];
              p.precision = parms[parms_index++];
              p.offset = parms[parms_index++];
              Z_nbit_decompress_one_atomic(data, data_offset + member_offset,
                                             buffer, j, buf_len, p);
              break;
         case Z_NBIT_ARRAY:
              Z_nbit_decompress_one_array(data, data_offset + member_offset,
                                            buffer, j, buf_len, parms);
              break;
         case Z_NBIT_COMPOUND:
              Z_nbit_decompress_one_compound(data, data_offset+member_offset,
                                               buffer, j, buf_len, parms);
              break;
         case Z_NBIT_NOOPTYPE:
              size = parms[parms_index++];
              Z_nbit_decompress_one_nooptype(data, data_offset+member_offset,
                                               buffer, j, buf_len, size);
              break;
      } /* end switch */
   }
}

/* support routine for nbit filter */
static void 
Z_nbit_decompress_one_array(unsigned char *data, ck_size_t data_offset,
	unsigned char *buffer, ck_size_t *j, int *buf_len, const unsigned parms[])
{
   unsigned 	i, total_size, base_class, base_size, n, begin_index;
   nbit_parms_atomic p;

   total_size = parms[parms_index++];
   base_class = parms[parms_index++];

   switch(base_class) {
      case Z_NBIT_ATOMIC:
           p.size = parms[parms_index++];
           p.order = parms[parms_index++];
           p.precision = parms[parms_index++];
           p.offset = parms[parms_index++];
           n = total_size/p.size;
           for(i = 0; i < n; i++)
              Z_nbit_decompress_one_atomic(data, data_offset + i*p.size,
                                             buffer, j, buf_len, p);
           break;
      case Z_NBIT_ARRAY:
           base_size = parms[parms_index]; /* read in advance */
           n = total_size/base_size; /* number of base_type elements inside the array datatype */
           begin_index = parms_index;
           for(i = 0; i < n; i++) {
              Z_nbit_decompress_one_array(data, data_offset + i*base_size,
                                            buffer, j, buf_len, parms);
              parms_index = begin_index;
           }
           break;
      case Z_NBIT_COMPOUND:
           base_size = parms[parms_index]; /* read in advance */
           n = total_size/base_size; /* number of base_type elements inside the array datatype */
           begin_index = parms_index;
           for(i = 0; i < n; i++) {
              Z_nbit_decompress_one_compound(data, data_offset + i*base_size,
                                               buffer, j, buf_len, parms);
              parms_index = begin_index;
           }
           break;
      case Z_NBIT_NOOPTYPE:
           parms_index++; /* skip size of no-op type */
           Z_nbit_decompress_one_nooptype(data, data_offset, buffer, j, buf_len, total_size);
           break;
   } /* end switch */
}


/* support routine for nbit filter */
static void 
Z_nbit_decompress(unsigned char *data, unsigned d_nelmts, unsigned char *buffer, const unsigned parms[])
{
   /* i: index of data, j: index of buffer,
      buf_len: number of bits to be filled in current byte */
   ck_size_t 		i, j, size;
   int 			buf_len;
   nbit_parms_atomic 	p;

   /* may not have to initialize to zeros */
   for(i = 0; i < d_nelmts*parms[4]; i++) data[i] = 0;

   /* initialization before the loop */
   j = 0;
   buf_len = sizeof(unsigned char) * 8;

   switch(parms[3]) {
      case Z_NBIT_ATOMIC:
           /* set the index before goto function call */
           p.size = parms[4];
           p.order = parms[5];
           p.precision = parms[6];
           p.offset = parms[7];
           for(i = 0; i < d_nelmts; i++) {
              Z_nbit_decompress_one_atomic(data, i*p.size, buffer, &j, &buf_len, p);
           }
           break;
      case Z_NBIT_ARRAY:
           size = parms[4];
           parms_index = 4;
           for(i = 0; i < d_nelmts; i++) {
              Z_nbit_decompress_one_array(data, i*size, buffer, &j, &buf_len, parms);
              parms_index = 4;
           }
           break;
      case Z_NBIT_COMPOUND:
           size = parms[4];
           parms_index = 4;
           for(i = 0; i < d_nelmts; i++) {
              Z_nbit_decompress_one_compound(data, i*size, buffer, &j, &buf_len, parms);
              parms_index = 4;
           }
           break;
   } /* end switch */
}

/* 
 * nbit filter 
 */
static ck_size_t
Z_filter_nbit(unsigned flags, ck_size_t cd_nelmts, const unsigned cd_values[],
	    ck_size_t nbytes, ck_size_t *buf_size, void **buf)
{
    ck_size_t ret_value = 0;          /* return value */
    ck_size_t size_out  = 0;          /* size of output buffer */
    unsigned d_nelmts = 0;         /* number of elements in the chunk */
    unsigned char *outbuf = NULL;  /* pointer to new output buffer */

    /* check arguments
     * cd_values[0] stores actual number of parameters in cd_values[]
     */
    if(cd_nelmts != cd_values[0]) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Nbit filter:Invalid aggression level", -1, NULL);
	CK_SET_RET_DONE(FAIL)
    }

    /* check if need to do nbit compress or decompress
     * cd_values[1] stores the flag if true indicating no need to compress
     */
/* NEED to check into this */
    if(cd_values[1]) {
        ret_value = *buf_size;
        goto done;
    }

    /* copy a filter parameter to d_nelmts */
    d_nelmts = cd_values[2];

    /* input; decompress */
    if(flags & Z_FLAG_REVERSE) {
        size_out = d_nelmts * cd_values[4]; /* cd_values[4] stores datatype size */

        /* allocate memory space for decompressed buffer */
        if(NULL==(outbuf = malloc(size_out))) {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Nbit filter:Internal allocation error", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}

        /* decompress the buffer */
        Z_nbit_decompress(outbuf, d_nelmts, *buf, cd_values);
    }

    /* free the input buffer */
    free(*buf);

    /* set return values */
    *buf = outbuf;
    outbuf = NULL;
    *buf_size = size_out;
    ret_value = size_out;

done:
    if(outbuf)
        free(outbuf);
    return(ret_value);
}
#endif

#ifdef HAVE_FILTER_SCALEOFFSET
/* 
 * scaleoffset filter 
 */

static ck_size_t Z_filter_scaleoffset(unsigned flags, ck_size_t cd_nelmts,
    const unsigned cd_values[], ck_size_t nbytes, size_t *buf_size, void **buf);


Z_class_t Z_SCALEOFFSET[1] = {{
    Z_CLASS_T_VERS,       /* Z_class_t version */
    Z_FILTER_SCALEOFFSET, /* Filter id number         */
    Z_filter_scaleoffset,     /* The actual filter function   */
}};

/* Struct of parameters needed for compressing/decompressing one atomic datatype */
typedef struct {
   ck_size_t 	size;      /* datatype size */
   uint32_t 	minbits;   /* minimum bits to compress one value of such datatype */
   unsigned 	mem_order; /* current memory endianness order */
} so_parms_atomic;

static void Z_scaleoffset_convert(void *, unsigned, ck_size_t);
static void Z_scaleoffset_next_byte(ck_size_t *, int *);
static void Z_scaleoffset_decompress_one_byte(unsigned char *, ck_size_t, int, int, unsigned char *, ck_size_t *, int *, so_parms_atomic, int);
static void Z_scaleoffset_decompress_one_atomic(unsigned char *, ck_size_t, unsigned char *, ck_size_t *, int *, so_parms_atomic);
static void Z_scaleoffset_decompress(unsigned char *, unsigned, unsigned char *, so_parms_atomic);
static enum Z_scaleoffset_type Z_scaleoffset_get_type(unsigned, unsigned, unsigned);
static void Z_scaleoffset_postdecompress_i(void *, unsigned, enum Z_scaleoffset_type, unsigned, const void *, uint32_t, unsigned long_long);
static ck_err_t Z_scaleoffset_postdecompress_fd(void *, unsigned, enum Z_scaleoffset_type, unsigned, const void *, uint32_t, unsigned long_long, double);

/* Get the fill value for integer type */
#define Z_scaleoffset_get_filval_1(i, type, filval_buf, filval)               \
{                                                                             \
    type filval_mask;                                                         \
                                                                              \
    /* retrieve fill value from corresponding positions of cd_values[]        \
     * retrieve them corresponding to how they are stored                     \
     */                                                                       \
    for(i = 0; i < sizeof(type); i++) {                                       \
	filval_mask = ((const unsigned char *)filval_buf)[i];                 \
	filval_mask <<= i*8;                                                  \
	filval |= filval_mask;                                                \
    }                                                                         \
}

/* Postdecompress for unsigned integer type */
#define Z_scaleoffset_postdecompress_1(type, data, d_nelmts, filavail, filval_buf, minbits, minval)	\
{                                                                                 			\
    type *buf = data, filval = 0; unsigned i;                                      			\
                                                                                  			\
    if(filavail == Z_SCALEOFFSET_FILL_DEFINED) { /* fill value defined */        			\
	Z_scaleoffset_get_filval_1(i, type, filval_buf, filval)                   			\
	for(i = 0; i < d_nelmts; i++)                                               			\
	    buf[i] = (type)((buf[i] == (((type)1 << minbits) - 1))?filval:(buf[i] + minval));		\
    } else /* fill value undefined */                                              			\
	for(i = 0; i < d_nelmts; i++) 									\
	    buf[i] += (type)(minval);                     						\
}

/* Postdecompress for signed integer type */
#define Z_scaleoffset_postdecompress_2(type, data, d_nelmts, filavail, filval_buf, minbits, minval)	\
{                                                                                          		\
    type *buf = data, filval = 0; unsigned i;                                               		\
                                                                                           		\
    if(filavail == Z_SCALEOFFSET_FILL_DEFINED) { /* fill value defined */                 		\
	Z_scaleoffset_get_filval_1(i, type, filval_buf, filval)                            		\
	for(i = 0; i < d_nelmts; i++)                                                        		\
	    buf[i] = (type)(((unsigned type)buf[i] == (((unsigned type)1 << minbits) - 1)) ? filval : (buf[i] + minval));\
    } else /* fill value undefined */                                                      		\
	for(i = 0; i < d_nelmts; i++) 									\
	    buf[i] += (type)(minval);                              					\
}

/* Get the fill value for floating-point type */
#define Z_scaleoffset_get_filval_2(i, type, filval_buf, filval)                     	\
{                                                                                   	\
    if(sizeof(type)==sizeof(int))                                                   	\
	Z_scaleoffset_get_filval_1(i, int, filval_buf, *(int *)&filval)            	\
    else if(sizeof(type)==sizeof(long))                                              	\
	Z_scaleoffset_get_filval_1(i, long, filval_buf, *(long *)&filval)            	\
    else if(sizeof(type)==sizeof(long_long))                                           	\
	Z_scaleoffset_get_filval_1(i, long_long, filval_buf, *(long_long *)&filval)   	\
    else {                                                                              \
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Cannot find matched memory datatype", -1, NULL); \
	CK_SET_RET_DONE(FAIL)											    \
    }											\
}

/* Modify values of data in postdecompression if fill value defined for floating-point type */
#define Z_scaleoffset_modify_3(i, type, buf, d_nelmts, filval, minbits, min, D_val)     \
{                                                                                       \
    if (sizeof(type)==sizeof(int))                                                      \
	for(i = 0; i < d_nelmts; i++)                                                   \
	    buf[i] = (*(int *)&buf[i]==(((unsigned int)1 << minbits) - 1))?             \
		filval:(*(int *)&buf[i])/pow(10.0, D_val) + min;                        \
    else if(sizeof(type)==sizeof(long))                                                 \
	for(i = 0; i < d_nelmts; i++)                                                   \
	    buf[i] = (*(long *)&buf[i]==(((unsigned long)1 << minbits) - 1))?           \
		filval:(*(long *)&buf[i])/pow(10.0, D_val) + min;                       \
    else if(sizeof(type)==sizeof(long_long))                                            \
	for(i = 0; i < d_nelmts; i++)                                                   \
	    buf[i] = (*(long_long *)&buf[i]==(((unsigned long_long)1 << minbits) - 1))? \
		filval:(*(long_long *)&buf[i])/pow(10.0, D_val) + min;                  \
    else {                                                                              \
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Cannot find matched memory datatype", -1, NULL); \
	CK_SET_RET_DONE(FAIL)											    \
    }											\
}

/* Modify values of data in postdecompression if fill value undefined for floating-point type */
#define Z_scaleoffset_modify_4(i, type, buf, d_nelmts, min, D_val)                   	\
{                                                                                      	\
    if (sizeof(type) == sizeof(int))                                                    \
	for(i = 0; i < d_nelmts; i++)                                                   \
	    buf[i] = (*(int *)&buf[i])/pow(10.0, D_val) + min;                          \
    else if(sizeof(type) == sizeof(long))                                               \
	for(i = 0; i < d_nelmts; i++)                                                   \
	    buf[i] = (*(long *)&buf[i])/pow(10.0, D_val) + min;                         \
    else if(sizeof(type)==sizeof(long_long))                                            \
	for(i = 0; i < d_nelmts; i++)                                                   \
	    buf[i] = (*(long_long *)&buf[i])/pow(10.0, D_val) + min;                    \
    else {                                                                              \
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Cannot find matched memory datatype", -1, NULL); \
	CK_SET_RET_DONE(FAIL)											    \
    }											\
}



/* Retrive minimum value of floating-point type */
#define Z_scaleoffset_get_min(i, type, minval, min)                                	\
{                                                                                     	\
    if(sizeof(type)==sizeof(int)) {                                                    \
	int 	mask;                                                                   \
	for (i = 0; i < sizeof(int); i++) {                                             \
	    mask = ((unsigned char *)&minval)[i]; 					\
	    mask <<= i*8; 								\
	    *(int *)&min |= mask;    							\
	}                                                                               \
    } else if(sizeof(type)==sizeof(long)) {                                            	\
	long 	mask;                                                                   \
	for (i = 0; i < sizeof(long); i++) {                                            \
	    mask = ((unsigned char *)&minval)[i]; 					\
	    mask <<= i*8; 								\
	    *(long *)&min |= mask;   							\
	}                                                                               \
    } else if(sizeof(type) == sizeof(long_long)) {                                      \
	long_long 	mask;                                                           \
	for (i = 0; i < sizeof(long_long); i++) {                                      	\
	    mask = ((unsigned char *)&minval)[i]; 					\
	    mask <<= i*8; 								\
	    *(long_long *)&min |= mask;							\
	}                                                                               \
    } else {                                                                    	\
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Cannot find matched memory datatype", -1, NULL); \
	CK_SET_RET_DONE(FAIL)											    \
    }											\
}

/* Postdecompress for floating-point type using variable-minimum-bits method */
#define Z_scaleoffset_postdecompress_3(type, data, d_nelmts, filavail, filval_buf,   \
                                         minbits, minval, D_val)                     \
{                                                                                    \
    type *buf = data, filval = 0, min = 0; unsigned i;                               \
                                                                                     \
    Z_scaleoffset_get_min(i, type, minval, min)                                      \
                                                                                     \
    if(filavail == Z_SCALEOFFSET_FILL_DEFINED) { /* fill value defined */           \
	Z_scaleoffset_get_filval_2(i, type, filval_buf, filval)                      \
	Z_scaleoffset_modify_3(i, type, buf, d_nelmts, filval, minbits, min, D_val)  \
    } else /* fill value undefined */                                                \
	Z_scaleoffset_modify_4(i, type, buf, d_nelmts, min, D_val)                   \
}

/* 
 * Support routine for scaleoffset filter:
 * Change byte order of input buffer either from little-endian to big-endian
 * or from big-endian to little-endian.
 */
static void
Z_scaleoffset_convert(void *buf, unsigned d_nelmts, ck_size_t dtype_size)
{
    if(dtype_size > 1) {

	unsigned 	i, j;
	unsigned char 	*buffer, temp;

	buffer = buf;
	for(i = 0; i < d_nelmts * dtype_size; i += dtype_size)
	    for(j = 0; j < dtype_size/2; j++) {
		/* swap pair of bytes */
		temp = buffer[i+j];
		buffer[i+j] = buffer[i+dtype_size-1-j];
		buffer[i+dtype_size-1-j] = temp;
	    }
    }
}

/* Support routine for scaleoffset filter */
static void 
Z_scaleoffset_next_byte(ck_size_t *j, int *buf_len)
{
    ++(*j); *buf_len = 8 * sizeof(unsigned char);
}


/* Support routine for scaleoffset filter */
static void 
Z_scaleoffset_decompress_one_byte(unsigned char *data, ck_size_t data_offset, int k, int begin_i, 
    unsigned char *buffer, ck_size_t *j, int *buf_len, so_parms_atomic p, int dtype_len)
{
    int 		dat_len; /* dat_len is the number of bits to be copied in each data byte */
    unsigned 	char val; /* value to be copied in each data byte */

    /* initialize value and bits of unsigned char to be copied */
    val = buffer[*j];
    if(k == begin_i)
	dat_len = 8 - (dtype_len - p.minbits) % 8;
    else
	dat_len = 8;

    if(*buf_len > dat_len) {
	data[data_offset + k] = ((val >> (*buf_len - dat_len)) & ~(~0 << dat_len));
	*buf_len -= dat_len;
    } else {
	data[data_offset + k] = ((val & ~(~0 << *buf_len)) << (dat_len - *buf_len));
	dat_len -= *buf_len;
	Z_scaleoffset_next_byte(j, buf_len);
	if (dat_len == 0) 
	    return;
	val = buffer[*j];
	data[data_offset + k] |= ((val >> (*buf_len - dat_len)) & ~(~0 << dat_len));
	*buf_len -= dat_len;
    }
}

/* Support routine for scaleoffset filter */
static void 
Z_scaleoffset_decompress_one_atomic(unsigned char *data, ck_size_t data_offset, unsigned char *buffer, 
    ck_size_t *j, int *buf_len, so_parms_atomic p)
{
   /* begin_i: the index of byte having first significant bit */
    int 	k, begin_i, dtype_len;

    assert(p.minbits > 0);

    dtype_len = p.size * 8;

    if(p.mem_order == Z_SCALEOFFSET_ORDER_LE) { /* little endian */
	begin_i = p.size - 1 - (dtype_len - p.minbits) / 8;

	for(k = begin_i; k >= 0; k--)
	    Z_scaleoffset_decompress_one_byte(data, data_offset, k, begin_i, buffer, j, buf_len, p, dtype_len);
    }

    if(p.mem_order == Z_SCALEOFFSET_ORDER_BE) { /* big endian */
	begin_i = (dtype_len - p.minbits) / 8;

	for(k = begin_i; k <= p.size - 1; k++)
	    Z_scaleoffset_decompress_one_byte(data, data_offset, k, begin_i, buffer, j, buf_len, p, dtype_len);
    }
}

/* Support routine for scaleoffset filter */
static void 
Z_scaleoffset_decompress(unsigned char *data, unsigned d_nelmts, unsigned char *buffer, so_parms_atomic p)
{
   /* 
    * i: index of data
    * j: index of buffer
    * buf_len: number of bits to be filled in current byte 
    */
    ck_size_t 	i, j;
    int 	buf_len;

    /* must initialize to zeros */
    for(i = 0; i < d_nelmts*p.size; i++) 
	data[i] = 0;

    /* initialization before the loop */
    j = 0;
    buf_len = sizeof(unsigned char) * 8;

    /* decompress */
    for(i = 0; i < d_nelmts; i++)
	Z_scaleoffset_decompress_one_atomic(data, i*p.size, buffer, &j, &buf_len, p);
}


/* Support routine for scaleoffset filter */
static enum Z_scaleoffset_type
Z_scaleoffset_get_type(unsigned dtype_class, unsigned dtype_size, unsigned dtype_sign)
{
    enum Z_scaleoffset_type 	type = t_bad; /* integer type */
    enum Z_scaleoffset_type 	ret_value;           

    if(dtype_class == Z_SCALEOFFSET_CLS_INTEGER) {
	if(dtype_sign == Z_SCALEOFFSET_SGN_NONE) { /* unsigned integer */
	    if(dtype_size == sizeof(unsigned char))      
		type = t_uchar;
            else if(dtype_size == sizeof(unsigned short))     
		type = t_ushort;
            else if(dtype_size == sizeof(unsigned int))       
		type = t_uint;
            else if(dtype_size == sizeof(unsigned long))      
		type = t_ulong;
            else if(dtype_size == sizeof(unsigned long_long)) 
		type = t_ulong_long;
            else {
		error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Cannot find matched memory datatype", -1, NULL);
		CK_SET_RET_DONE(FAIL)
	    }
        }

        if(dtype_sign == Z_SCALEOFFSET_SGN_2) { /* signed integer */
            if(dtype_size == sizeof(signed char)) 
		type = t_schar;
            else if(dtype_size == sizeof(short))       
		type = t_short;
            else if(dtype_size == sizeof(int))         
		type = t_int;
            else if(dtype_size == sizeof(long))        
		type = t_long;
            else if(dtype_size == sizeof(long_long))   
		type = t_long_long;
            else {
		error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Cannot find matched memory datatype", -1, NULL);
		CK_SET_RET_DONE(FAIL)
	    }
        }
    }

    if(dtype_class == Z_SCALEOFFSET_CLS_FLOAT) {
        if (dtype_size == sizeof(float))       
	    type = t_float;
        else if(dtype_size == sizeof(double)) 
	    type = t_double;
        else {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Cannot find matched memory datatype", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}
    }

    /* Set return value */
    ret_value = type;

done:
    return(ret_value);
}



/* Support routine for scaleoffset filter */
/* postdecompress for integer type */
static void
Z_scaleoffset_postdecompress_i(void *data, unsigned d_nelmts, enum Z_scaleoffset_type type,
    unsigned filavail, const void *filval_buf, uint32_t minbits, unsigned long_long minval)
{
    long_long sminval = *(long_long*)&minval; /* for signed integer types */

    if(type == t_uchar)
	Z_scaleoffset_postdecompress_1(unsigned char, data, d_nelmts, filavail, filval_buf, minbits, minval)
    else if(type == t_ushort)
	Z_scaleoffset_postdecompress_1(unsigned short, data, d_nelmts, filavail, filval_buf, minbits, minval)
   else if(type == t_uint)
	Z_scaleoffset_postdecompress_1(unsigned int, data, d_nelmts, filavail, filval_buf, minbits, minval)
   else if(type == t_ulong)
	Z_scaleoffset_postdecompress_1(unsigned long, data, d_nelmts, filavail, filval_buf, minbits, minval)
   else if(type == t_ulong_long)
	Z_scaleoffset_postdecompress_1(unsigned long_long, data, d_nelmts, filavail, filval_buf, minbits, minval)
   else if(type == t_schar) {
	signed char *buf = data, filval = 0; unsigned i;

	if(filavail == Z_SCALEOFFSET_FILL_DEFINED) { /* fill value defined */
	    Z_scaleoffset_get_filval_1(i, signed char, filval_buf, filval)
	    for(i = 0; i < d_nelmts; i++)
		buf[i] = (buf[i] == (((unsigned char)1 << minbits) - 1))?filval:(buf[i] + sminval);
	} else /* fill value undefined */
	    for(i = 0; i < d_nelmts; i++) 
		buf[i] += sminval;
   } else if(type == t_short)
	Z_scaleoffset_postdecompress_2(short, data, d_nelmts, filavail, filval_buf, minbits, sminval)
   else if (type == t_int)
      Z_scaleoffset_postdecompress_2(int, data, d_nelmts, filavail, filval_buf, minbits, sminval)
   else if(type == t_long)
      Z_scaleoffset_postdecompress_2(long, data, d_nelmts, filavail, filval_buf, minbits, sminval)
   else if(type == t_long_long)
      Z_scaleoffset_postdecompress_2(long_long, data, d_nelmts, filavail, filval_buf, minbits, sminval)
}

/* 
 * Support routine for scaleoffset filter:
 * postdecompress for floating-point type, variable-minimum-bits method
 * success: non-negative
 * failure: negative
 */
static ck_err_t
Z_scaleoffset_postdecompress_fd(void *data, unsigned d_nelmts, enum Z_scaleoffset_type type,
    unsigned filavail, const void *filval_buf, uint32_t minbits, unsigned long_long minval, double D_val)
{
   long_long 	sminval = *(long_long*)&minval; /* for signed integer types */
   ck_err_t 	ret_value=SUCCEED;

   if(type == t_float)
      Z_scaleoffset_postdecompress_3(float, data, d_nelmts, filavail, filval_buf, minbits, sminval, D_val)
   else if(type == t_double)
      Z_scaleoffset_postdecompress_3(double, data, d_nelmts, filavail, filval_buf, minbits, sminval, D_val)

done:
   return(ret_value);
}


/* 
 * scaleoffset filter 
 */
static size_t
Z_filter_scaleoffset (unsigned flags, ck_size_t cd_nelmts, const unsigned cd_values[],
	ck_size_t nbytes, ck_size_t *buf_size, void **buf)
{
    ck_size_t 	ret_value=0;           	/* return value */
    ck_size_t 	size_out=0;           	/* size of output buffer */
    unsigned 	d_nelmts=0;          	/* number of data elements in the chunk */
    unsigned 	dtype_class;           	/* datatype class */
    unsigned 	dtype_sign;            	/* integer datatype sign */
    unsigned 	filavail;              	/* flag indicating if fill value is defined or not */
    int 	scale_factor=0;         /* scale factor */
    double 	D_val=0.0;             	/* decimal scale factor */
    uint32_t 	minbits=0;           	/* minimum number of bits to store values */
    int 	need_convert=FALSE;     /* flag indicating convertion of byte order */
    unsigned 	char *outbuf=NULL;   	/* pointer to new output buffer */
    unsigned 	buf_offset=21;       	/* buffer offset because of parameters stored in file */
    unsigned 	i;                     	/* index */
    DT_order_t 	native_order;
    unsigned 	long_long 	minval= 0;   	/* minimum value of input buffer */
    enum Z_scaleoffset_type	type;   	/* memory type corresponding to dataset datatype */
    Z_SO_scale_type_t 		scale_type = 0;	/* scale type */
    so_parms_atomic 		p;      	/* paramters needed for compress/decompress functions */

    if(debug_verbose())
        printf("Applying scaleoffset filter ...\n");

    if(cd_nelmts != Z_SCALEOFFSET_TOTAL_NPARMS) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Invalid # of parameters", -1, NULL);
	CK_SET_RET_DONE(FAIL)
    }

#ifdef HAVE_BIG_ENDIAN
    native_order = DT_ORDER_BE;
#else
    native_order = DT_ORDER_LE;
#endif

    /* Check if memory byte order matches dataset datatype byte order */
    switch(native_order) {
	case DT_ORDER_LE:      /* memory is little-endian byte order */
            if (cd_values[Z_SCALEOFFSET_PARM_ORDER] == Z_SCALEOFFSET_ORDER_BE)
                need_convert = TRUE;
            break;

        case DT_ORDER_BE:      /* memory is big-endian byte order */
            if(cd_values[Z_SCALEOFFSET_PARM_ORDER] == Z_SCALEOFFSET_ORDER_LE)
                need_convert = TRUE;
            break;

        default:
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Invalid endianness order", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
    } /* end switch */

    /* copy filter parameters to local variables */
    d_nelmts     = cd_values[Z_SCALEOFFSET_PARM_NELMTS];
    dtype_class  = cd_values[Z_SCALEOFFSET_PARM_CLASS];
    dtype_sign   = cd_values[Z_SCALEOFFSET_PARM_SIGN];
    filavail     = cd_values[Z_SCALEOFFSET_PARM_FILAVAIL];
    scale_factor = (int)cd_values[Z_SCALEOFFSET_PARM_SCALEFACTOR];
    scale_type   = cd_values[Z_SCALEOFFSET_PARM_SCALETYPE];

    /* 
     * Check and assign proper values set by user to related parameters
     * scale type can be H5Z_SO_FLOAT_DSCALE (0), H5Z_SO_FLOAT_ESCALE (1) or H5Z_SO_INT (other)
     * H5Z_SO_FLOAT_DSCALE : floating-point type, variable-minimum-bits method,
     *                      scale factor is decimal scale factor
     * H5Z_SO_FLOAT_ESCALE : floating-point type, fixed-minimum-bits method,
     *                      scale factor is the fixed minimum number of bits
     * H5Z_SO_INT          : integer type, scale_factor is minimum number of bits
     */
    if(dtype_class == Z_SCALEOFFSET_CLS_FLOAT) { /* floating-point type */
        if(scale_type != Z_SO_FLOAT_DSCALE && scale_type != Z_SO_FLOAT_ESCALE) {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Invalid scale type", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}
    }

    if(dtype_class==Z_SCALEOFFSET_CLS_INTEGER) { /* integer type */
        if(scale_type != Z_SO_INT) {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Invalid scale type", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}

        /* 
	 * If scale_factor is less than 0 for integer, library will reset it to 0.
         * In this case, library will calculate the minimum-bits
         */
        if (scale_factor < 0) 
	    scale_factor = 0;
    }

    /* fixed-minimum-bits method is not implemented and is forbidden */
    if(scale_type == Z_SO_FLOAT_ESCALE) {
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Unsupported E-scaling method", -1, NULL);
	CK_SET_RET_DONE(FAIL)
    }

    if(scale_type == Z_SO_FLOAT_DSCALE) { /* floating-point type, variable-minimum-bits */
        D_val = (double)scale_factor;
    } else { /* integer type, or floating-point type with fixed-minimum-bits method */
        if (scale_factor > (int)(cd_values[Z_SCALEOFFSET_PARM_SIZE] * 8)) {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Minimum # of bits exceeds maximum", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}

        /* no need to process data */
        if (scale_factor == (int)(cd_values[Z_SCALEOFFSET_PARM_SIZE] * 8)) {
            ret_value = *buf_size;
            goto done;
        }
        minbits = scale_factor;
    }

    /* prepare paramters to pass to compress/decompress functions */
    p.size = cd_values[Z_SCALEOFFSET_PARM_SIZE];
    p.mem_order = native_order;
    /* input; decompress */
    if(flags & Z_FLAG_REVERSE) {
        /* 
	 * Retrieve values of minbits and minval from input compressed buffer.
         * Retrieve them corresponding to how they are stored during compression.
         */
        uint32_t 	minbits_mask = 0;
        unsigned 	long_long minval_mask = 0;
        unsigned 	minval_size = 0;

        minbits = 0;
        for(i = 0; i < 4; i++) {
	    minbits_mask = ((unsigned char *)*buf)[i];
            minbits_mask <<= i*8;
            minbits |= minbits_mask;
        }

        /* 
	 * Retrieval of minval takes into consideration situation where sizeof
         * unsigned long_long (datatype of minval) may change from compression
         * to decompression, only smaller size is used
         */
        minval_size = sizeof(unsigned long_long) <= ((unsigned char *)*buf)[4] ?
			sizeof(unsigned long_long) : ((unsigned char *)*buf)[4];
        minval = 0;
        for(i = 0; i < minval_size; i++) {
            minval_mask = ((unsigned char *)*buf)[5+i];
            minval_mask <<= i*8;
            minval |= minval_mask;
        }

        assert(minbits <= p.size * 8);
        p.minbits = minbits;

        /* calculate size of output buffer after decompression */
        size_out = d_nelmts * p.size;

        /* allocate memory space for decompressed buffer */
        if(NULL == (outbuf = malloc(size_out))) {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Internal allocation error", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}

        /* special case: minbits equal to full precision */
        if(minbits == p.size * 8) {
	    memcpy(outbuf, (unsigned char*)(*buf)+buf_offset, size_out);

            /* convert to dataset datatype endianness order if needed */
            if(need_convert)
                Z_scaleoffset_convert(outbuf, d_nelmts, p.size);

            *buf = outbuf;
            outbuf = NULL;
            *buf_size = size_out;
            ret_value = size_out;
            goto done;
        }

        /* decompress the buffer if minbits not equal to zero */
        if(minbits != 0)
            Z_scaleoffset_decompress(outbuf, d_nelmts, (unsigned char*)(*buf)+buf_offset, p);
        else {
            /* fill value is not defined and all data elements have the same value */
            for (i = 0; i < size_out; i++) 
		outbuf[i] = 0;
        }

        /* before postprocess, get memory type */
        if((type = Z_scaleoffset_get_type(dtype_class, (unsigned)p.size, dtype_sign)) == 0) {
	    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Cannot get memory type", -1, NULL);
	    CK_SET_RET_DONE(FAIL)
	}

        /* postprocess after decompression */
        if(dtype_class==Z_SCALEOFFSET_CLS_INTEGER)
            Z_scaleoffset_postdecompress_i(outbuf, d_nelmts, type, filavail,
                                             &cd_values[Z_SCALEOFFSET_PARM_FILVAL], minbits, minval);

        if(dtype_class==Z_SCALEOFFSET_CLS_FLOAT)
            if (scale_type==0) { /* variable-minimum-bits method */
                if (Z_scaleoffset_postdecompress_fd(outbuf, d_nelmts, type, filavail,
			&cd_values[Z_SCALEOFFSET_PARM_FILVAL], minbits, minval, D_val) == FAIL) {
		    error_push(ERR_INTERNAL, ERR_NONE_SEC, "Scaleoffset filter:Internal post-decompression failed", -1, NULL);
		    CK_SET_RET_DONE(FAIL)
		}
            }

        /* after postprocess, convert to dataset datatype endianness order if needed */
        if(need_convert)
            Z_scaleoffset_convert(outbuf, d_nelmts, p.size);
    }

    /* free the input buffer */
    free(*buf);

    /* set return values */
    *buf = outbuf;
    outbuf = NULL;
    *buf_size = size_out;
    ret_value = size_out;

done:
    if(outbuf)
        free(outbuf);
    return(ret_value);
}

#endif

static int
filter_find_idx(Z_filter_t id)
{
    ck_size_t 	i;
    int 	ret_value=FAIL;

    for(i = 0; i < Z_table_used_g; i++)
        if(Z_table_g[i].id == id)
            CK_SET_RET_DONE((int)i)
done:
    return(ret_value);
} 

ck_err_t
filter_pline(const OBJ_filter_t *pline, unsigned flags, unsigned *filter_mask/*in,out*/,
	    Z_EDC_t edc_read, Z_cb_t cb_struct, ck_size_t *nbytes/*in,out*/,
	    ck_size_t *buf_size/*in,out*/, void **buf/*in,out*/)
{
    ck_size_t   i, idx, new_nbytes;
    int 	fclass_idx;     /* Index of filter class in global table */
    Z_class_t 	*fclass=NULL;   /* Filter class pointer */
    unsigned    failed = 0;
    unsigned    tmp_flags;
    ck_err_t    ret_value=SUCCEED;

    assert(0==(flags & ~((unsigned)Z_FLAG_INVMASK)));
    assert(filter_mask);
    assert(nbytes && *nbytes>0);
    assert(buf_size && *buf_size>0);
    assert(buf && *buf);
    assert(!pline || pline->nused<Z_MAX_NFILTERS);

    if(pline && (flags & Z_FLAG_REVERSE)) { /* Read */
	for(i = pline->nused; i > 0; --i) {
            idx = i-1;

#if 0
            if(*filter_mask & ((unsigned)1<<idx)) {
printf("I am in filter exclued\n");
                failed |= (unsigned)1 << idx;
                continue;/*filter excluded*/
            }
printf("I am done with filter mask\n");
printf("new_nbytes=%u\n", new_nbytes);
#endif
            if((fclass_idx = filter_find_idx(pline->filter[idx].id)) < 0) {
		error_push(ERR_INTERNAL, ERR_NONE_SEC, "Filter pipeline:Filter not registered", 
		    -1, NULL);
		CK_SET_RET_DONE(FAIL)
            }
            fclass = &Z_table_g[fclass_idx];
            tmp_flags = flags|(pline->filter[idx].flags);
            tmp_flags |= (edc_read == Z_DISABLE_EDC) ? Z_FLAG_SKIP_EDC : 0;
            new_nbytes = (fclass->filter)(tmp_flags, pline->filter[idx].cd_nelmts,
                                        pline->filter[idx].cd_values, *nbytes, buf_size, buf);

            if(new_nbytes == 0) {
                if((cb_struct.func && (Z_CB_FAIL==cb_struct.func(pline->filter[idx].id, *buf, *buf_size, cb_struct.op_data)))
                    || !cb_struct.func) {
			error_push(ERR_INTERNAL, ERR_NONE_SEC, "Filter pipeline:Read failed", -1, NULL);
			CK_SET_RET_DONE(FAIL)
		}

                *nbytes = *buf_size;
                failed |= (unsigned)1 << idx;
            } else {
                *nbytes = new_nbytes;
            }
        }
    } else { /* Write */
	error_push(ERR_INTERNAL, ERR_NONE_SEC, "pipeline:Illegal operation", -1, NULL);
	CK_SET_RET_DONE(FAIL)
    }

    *filter_mask = failed;

done:
    return(ret_value);
} /* filter_pline() */
