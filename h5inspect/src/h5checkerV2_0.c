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
#include "h5_logger.h"

/*
 * This file mainly consists of routines for version 2 specific format
 */

extern logger_ctx_t logger; /* object logger */

/*  
 * version 2 : Btree2 class methods
 */

static ck_err_t HF_huge_btree2_indir_decode(driver_t *file, const uint8_t *raw, void *native, void *_ck_udata);
static ck_err_t HF_huge_btree2_indir_compare(const void *rec1, const void *rec2);

/* v2 B-tree class for indirectly accessed 'huge' objects */
const B2_class_t HF_BT2_INDIR[1] = {{
    B2_FHEAP_HUGE_INDIR_ID, /* Type of B-tree */
    sizeof(HF_huge_bt2_indir_rec_t),
    HF_huge_btree2_indir_decode,  /* Record decoding callback */
    HF_huge_btree2_indir_compare, /* Record comparison callback */
}};

static ck_err_t HF_huge_btree2_filt_indir_decode(driver_t *file, const uint8_t *raw, void *native, void *_ck_udata);
static ck_err_t HF_huge_btree2_filt_indir_compare(const void *rec1, const void *rec2);

/* v2 B-tree class for indirectly accessed, filtered 'huge' objects */
const B2_class_t HF_BT2_FILT_INDIR[1] = {{
    B2_FHEAP_HUGE_FILT_INDIR_ID, /* Type of B-tree */
    sizeof(HF_huge_bt2_filt_indir_rec_t),
    HF_huge_btree2_filt_indir_decode,  /* Record decoding callback */
    HF_huge_btree2_filt_indir_compare, /* Record comparison callback */
}};

static ck_err_t HF_huge_btree2_dir_decode(driver_t *file, const uint8_t *raw, void *native, void *_ck_udata);
static ck_err_t HF_huge_btree2_dir_compare(const void *rec1, const void *rec2);

/* v2 B-tree class for directly accessed 'huge' objects */
const B2_class_t HF_BT2_DIR[1] = {{
    /* B-tree class information */
    B2_FHEAP_HUGE_DIR_ID,          /* Type of B-tree */
    sizeof(HF_huge_bt2_dir_rec_t), /* Size of native record */
    HF_huge_btree2_dir_decode,     /* Record decoding callback */
    HF_huge_btree2_dir_compare,    /* Record comparison callback */
}};

static ck_err_t HF_huge_btree2_filt_dir_decode(driver_t *file, const uint8_t *raw, void *native, void *_ck_udata);
static ck_err_t HF_huge_btree2_filt_dir_compare(const void *rec1, const void *rec2);

/* v2 B-tree class for directly accessed, filtered 'huge' objects */
const B2_class_t HF_BT2_FILT_DIR[1] = {{
    /* B-tree class information */
    B2_FHEAP_HUGE_FILT_DIR_ID,          /* Type of B-tree */
    sizeof(HF_huge_bt2_filt_dir_rec_t), /* Size of native record */
    HF_huge_btree2_filt_dir_decode,     /* Record decoding callback */
    HF_huge_btree2_filt_dir_compare,    /* Record comparison callback */
}};

static ck_err_t G_dense_btree2_name_decode(driver_t *file, const uint8_t *raw, void *native, void *_ck_udata);
static ck_err_t G_dense_btree2_name_compare(const void *rec1, const void *rec2);

/* v2 B-tree class for indexing 'name' field of links */
const B2_class_t G_BT2_NAME[1] = {{
    /* B-tree class information */
    B2_GRP_DENSE_NAME_ID,           /* Type of B-tree */
    sizeof(G_dense_bt2_name_rec_t), /* Size of native record */
    G_dense_btree2_name_decode,     /* Record decoding callback */
    G_dense_btree2_name_compare,    /* Record comparison callback */
}};

static ck_err_t G_dense_btree2_corder_decode(driver_t *file, const uint8_t *raw, void *native, void *_ck_udata);
static ck_err_t G_dense_btree2_corder_compare(const void *rec1, const void *rec2);

/* v2 B-tree class for indexing 'creation order' field of links */
const B2_class_t G_BT2_CORDER[1] = {{
    /* B-tree class information */
    B2_GRP_DENSE_CORDER_ID,           /* Type of B-tree */
    sizeof(G_dense_bt2_corder_rec_t), /* Size of native record */
    G_dense_btree2_corder_decode,     /* Record decoding callback */
    G_dense_btree2_corder_compare,    /* Record comparison callback */

}};

static ck_err_t SM_message_decode(driver_t *file, const uint8_t *raw, void *native, void *_ck_udata);
static ck_err_t SM_message_compare(const void *rec1, const void *rec2);
static ck_err_t SOHM_ck_msg_cb(driver_t *file, const void *_record, void *_ck_udata);

/* v2 B-tree class for SOHM indexes*/
const B2_class_t SM_INDEX[1] = {{
    /* B-tree class information */
    B2_SOHM_INDEX_ID,   /* Type of B-tree */
    sizeof(SM_sohm_t),  /* Size of native record */
    SM_message_decode,  /* Record decoding callback */
    SM_message_compare, /* Record comparison callback */
}};

static ck_err_t A_dense_btree2_name_decode(driver_t *file, const uint8_t *raw, void *native, void *_ck_udata);
static ck_err_t A_dense_btree2_name_compare(const void *rec1, const void *rec2);

/* v2 B-tree class for indexing 'name' field of attributes */
const B2_class_t A_BT2_NAME[1] = {{
    /* B-tree class information */
    B2_ATTR_DENSE_NAME_ID,          /* Type of B-tree */
    sizeof(A_dense_bt2_name_rec_t), /* Size of native record */
    A_dense_btree2_name_decode,     /* Record decoding callback */
    A_dense_btree2_name_compare,    /* Record comparison callback */
}};

static ck_err_t A_dense_btree2_corder_decode(driver_t *file, const uint8_t *raw, void *native, void *_ck_udata);
static ck_err_t A_dense_btree2_corder_compare(const void *rec1, const void *rec2);

/* v2 B-tree class for indexing 'creation order' field of attributes */
const B2_class_t A_BT2_CORDER[1] = {{
    /* B-tree class information */
    B2_ATTR_DENSE_CORDER_ID,          /* Type of B-tree */
    sizeof(A_dense_bt2_corder_rec_t), /* Size of native record */
    A_dense_btree2_corder_decode,     /* Record decoding callback */
    A_dense_btree2_corder_compare,    /* Record comparison callback */
}};

static ck_err_t D_btree2_chunk_decode(driver_t *file, const uint8_t *raw, void *native, void *_ck_udata);
static ck_err_t D_btree2_chunk_compare(const void *rec1, const void *rec2);

/* v2 B-tree class for indexing non-filtered data chunks */
B2_class_t D_BT2_CHUNK[1] = {{
    B2_DATA_CHUNKS_ID,      /* Type of B-tree */
    NULL,                   /* Size of native record */
    D_btree2_chunk_decode,  /* Record decoding callback */
    D_btree2_chunk_compare, /* Record comparison callback */
}};

static ck_err_t D_btree2_filt_chunk_decode(driver_t *file, const uint8_t *raw, void *native, void *_ck_udata);
static ck_err_t D_btree2_filt_chunk_compare(const void *rec1, const void *rec2);

/* v2 B-tree class for indexing filtered data chunks */
B2_class_t D_BT2_FILT_CHUNK[1] = {{
    B2_DATA_FILT_CHUNKS_ID,      /* Type of B-tree */
    NULL,                        /* Size of native record */
    D_btree2_filt_chunk_decode,  /* Record decoding callback */
    D_btree2_filt_chunk_compare, /* Record comparison callback */
}};

static unsigned V_log2_of2(uint32_t);

static ck_err_t check_bt2_hdr(driver_t *, ck_addr_t, const B2_class_t *, B2_t **);
static void free_bt2_hdr(B2_t *hdr);

static ck_err_t check_bt2_leaf(driver_t *, ck_addr_t, B2_shared_t *, unsigned, unsigned UNUSED, B2_leaf_t **, ck_op_t, void *);
static void free_bt2_leaf(B2_leaf_t *leaf);

static ck_err_t check_bt2_internal(driver_t *, ck_addr_t, B2_shared_t *, unsigned, unsigned, B2_internal_t **, ck_op_t, void *);
static void free_bt2_internal(B2_internal_t *internal);

static ck_err_t check_bt2_real(driver_t *, ck_addr_t, B2_shared_t *, unsigned, unsigned, ck_op_t, void *);

static int B2_locate_record(const B2_class_t *, unsigned, ck_size_t *, const uint8_t *, const void *, unsigned *);
static ck_err_t B2_find(driver_t *, const B2_class_t *, ck_addr_t, void *, B2_found_t, void *);

static ck_err_t HF_dtable_init(HF_dtable_t *);
static ck_err_t HF_dtable_lookup(const HF_dtable_t *, ck_hsize_t, unsigned *, unsigned *);
static unsigned HF_dtable_size_to_rows(const HF_dtable_t *, ck_hsize_t);

static ck_err_t check_iblock(driver_t *, ck_addr_t, HF_hdr_t *, unsigned);
static ck_err_t check_iblock_real(driver_t *, ck_addr_t, HF_hdr_t *, unsigned, HF_indirect_t **);
static void free_fheap_iblock(HF_indirect_t *iblock);

static ck_err_t check_dblock(driver_t *, ck_addr_t, HF_hdr_t *, ck_hsize_t, HF_parent_t *, HF_direct_t **);
static void free_fheap_dblock(HF_direct_t *dblock);

static ck_err_t check_dtable(driver_t *, const uint8_t **, HF_dtable_t *, const uint8_t *, ck_addr_t);
static ck_err_t check_fheap_hdr(driver_t *, ck_addr_t, HF_hdr_t **);
static void free_fheap_hdr(HF_hdr_t *hdr);

static ck_err_t HF_tiny_init(HF_hdr_t *);
static ck_err_t HF_huge_init(driver_t *, HF_hdr_t *);
static ck_err_t HF_man_dblock_locate(driver_t *, HF_hdr_t *, ck_hsize_t, HF_indirect_t **, unsigned *);
static ck_err_t HF_huge_get_obj_info(driver_t *, HF_hdr_t *, const uint8_t *, obj_info_t *);
static ck_err_t HF_huge_read(driver_t *, HF_hdr_t *, void *, obj_info_t *);
static ck_err_t HF_man_read(driver_t *, HF_hdr_t *, void *, obj_info_t *);
static ck_err_t HF_tiny_read(driver_t *, HF_hdr_t *, const uint8_t *, void *);

static ck_err_t SM_type_to_flag(unsigned, unsigned *);
static ssize_t SM_get_index(const SM_master_table_t *, unsigned);

static ck_err_t check_fshdr(driver_t *, ck_addr_t, HF_hdr_t *);
static ck_err_t check_fssection(driver_t *, ck_addr_t, FS_hdr_t *);

static ck_err_t HF_sect_row_init_cls(FS_section_class_t *, HF_hdr_t *);
static ck_err_t HF_sect_indirect_init_cls(FS_section_class_t *, HF_hdr_t *);

FS_section_class_t HF_FSPACE_SECT_CLS_SINGLE[1] = {{
    HF_FSPACE_SECT_SINGLE, /* Section type                 */
    0,                     /* Extra serialized size        */
    NULL                   /* Initialize section class     */
}};

FS_section_class_t HF_FSPACE_SECT_CLS_FIRST_ROW[1] = {{
    HF_FSPACE_SECT_FIRST_ROW, /* Section type                 */
    0,                        /* Extra serialized size        */
    HF_sect_row_init_cls      /* Initialize section class     */
}};

FS_section_class_t HF_FSPACE_SECT_CLS_NORMAL_ROW[1] = {{
    HF_FSPACE_SECT_NORMAL_ROW, /* Section type                 */
    0,                         /* Extra serialized size        */
    HF_sect_row_init_cls       /* Initialize section class     */
}};

FS_section_class_t HF_FSPACE_SECT_CLS_INDIRECT[1] = {{
    HF_FSPACE_SECT_INDIRECT,  /* Section type                 */
    0,                        /* Extra serialized size        */
    HF_sect_indirect_init_cls /* Initialize section class     */
}};

const FS_section_class_t *classes[] = {
    HF_FSPACE_SECT_CLS_SINGLE,
    HF_FSPACE_SECT_CLS_FIRST_ROW,
    HF_FSPACE_SECT_CLS_NORMAL_ROW,
    HF_FSPACE_SECT_CLS_INDIRECT};

/*
 * End declarations
 */

/*
 * START version 2 btree Validation
 */

/* version 2 btree support routines */

/* Lookup table for general log2(n) routine */
static const char LogTable256[] =
    {
        0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

unsigned
V_log2_gen(uint64_t n)
{
    unsigned r;                       /* r will be log2(n) */
    register unsigned int t, tt, ttt; /* temporaries */

    if ((ttt = (unsigned)(n >> 32)))
        if ((tt = (unsigned)(n >> 48)))
            r = (t = (unsigned)(n >> 56)) ? 56 + LogTable256[t] : 48 + LogTable256[tt & 0xFF];
        else
            r = (t = (unsigned)(n >> 40)) ? 40 + LogTable256[t] : 32 + LogTable256[ttt & 0xFF];
    else if ((tt = (unsigned)(n >> 16)))
        r = (t = (unsigned)(n >> 24)) ? 24 + LogTable256[t] : 16 + LogTable256[tt & 0xFF];
    else
        /* Added 'uint8_t' cast to pacify PGCC compiler */
        r = (t = (unsigned)(n >> 8)) ? 8 + LogTable256[t] : LogTable256[(uint8_t)n];

    return (r);
} /* V_log2_gen() */

/* 
 * version 2 btree class methods: decode() & compare()
 */
/*
 * decode()
 */
static ck_err_t
HF_huge_btree2_indir_decode(driver_t *file, const uint8_t *raw, void *_nrecord, void *_ck_udata)
{
    HF_huge_bt2_indir_rec_t *nrecord = (HF_huge_bt2_indir_rec_t *)_nrecord;

    addr_decode(file->shared, &raw, &nrecord->addr);
    DECODE_LENGTH(file->shared, raw, nrecord->len);
    DECODE_LENGTH(file->shared, raw, nrecord->id);

    return (SUCCEED);
} /* HF_huge_btree2_indir_decode() */

static ck_err_t
HF_huge_btree2_filt_indir_decode(driver_t *file, const uint8_t *raw, void *_nrecord, void *_ck_udata)
{
    HF_huge_bt2_filt_indir_rec_t *nrecord = (HF_huge_bt2_filt_indir_rec_t *)_nrecord;

    addr_decode(file->shared, &raw, &nrecord->addr);
    DECODE_LENGTH(file->shared, raw, nrecord->len);
    UINT32DECODE(raw, nrecord->filter_mask);
    DECODE_LENGTH(file->shared, raw, nrecord->obj_size);
    DECODE_LENGTH(file->shared, raw, nrecord->id);

    return (SUCCEED);
} /* HF_huge_btree2_filt_indir_decode() */

static ck_err_t
HF_huge_btree2_dir_decode(driver_t *file, const uint8_t *raw, void *_nrecord, void *_ck_udata)
{
    HF_huge_bt2_dir_rec_t *nrecord = (HF_huge_bt2_dir_rec_t *)_nrecord;

    addr_decode(file->shared, &raw, &nrecord->addr);
    DECODE_LENGTH(file->shared, raw, nrecord->len);

    return (SUCCEED);
} /* HF_huge_btree2_dir_decode() */

static ck_err_t
HF_huge_btree2_filt_dir_decode(driver_t *file, const uint8_t *raw, void *_nrecord, void *_ck_udata)
{
    HF_huge_bt2_filt_dir_rec_t *nrecord = (HF_huge_bt2_filt_dir_rec_t *)_nrecord;

    addr_decode(file->shared, &raw, &nrecord->addr);
    DECODE_LENGTH(file->shared, raw, nrecord->len);
    UINT32DECODE(raw, nrecord->filter_mask);
    DECODE_LENGTH(file->shared, raw, nrecord->obj_size);

    return (SUCCEED);
} /* HF_huge_btree2_filt_dir_decode() */

/* Indexed group on name */
static ck_err_t
G_dense_btree2_name_decode(driver_t *file, const uint8_t *raw, void *_nrecord, void *_ck_udata)
{
    G_dense_bt2_name_rec_t *nrecord = (G_dense_bt2_name_rec_t *)_nrecord;

    UINT32DECODE(raw, nrecord->hash)
    memcpy(nrecord->id, raw, (ck_size_t)G_DENSE_FHEAP_ID_LEN);

    return (SUCCEED);
} /* G_dense_btree2_name_decode() */

/* Indexed group on creation order */
static ck_err_t
G_dense_btree2_corder_decode(driver_t *file, const uint8_t *raw, void *_nrecord, void *_ck_udata)
{
    G_dense_bt2_corder_rec_t *nrecord = (G_dense_bt2_corder_rec_t *)_nrecord;

    INT64DECODE(raw, nrecord->corder)
    memcpy(nrecord->id, raw, (ck_size_t)G_DENSE_FHEAP_ID_LEN);

    return (SUCCEED);
} /* G_dense_btree2_corder_decode() */

static ck_err_t
SM_message_decode(driver_t *file, const uint8_t *raw, void *_nrecord, void *_ck_udata)
{
    SM_sohm_t *message = (SM_sohm_t *)_nrecord;

    message->location = *raw++;
    UINT32DECODE(raw, message->hash);

    if (message->location == SM_IN_HEAP)
    {
        UINT32DECODE(raw, message->u.heap_loc.ref_count);
        memcpy(message->u.heap_loc.fheap_id.id, raw, (size_t)OBJ_FHEAP_ID_LEN);
    }
    else
    {
        assert(message->location == SM_IN_OH);

        raw++; /* reserved */
        message->msg_type_id = *raw++;
        UINT16DECODE(raw, message->u.mesg_loc.index);
        addr_decode(file->shared, &raw, &message->u.mesg_loc.oh_addr);
    }

    return (SUCCEED);

} /* SM_message_decode() */

/* Indexed attribute on name */
static ck_err_t
A_dense_btree2_name_decode(driver_t *file, const uint8_t *raw, void *_nrecord, void *_ck_udata)
{
    A_dense_bt2_name_rec_t *nrecord = (A_dense_bt2_name_rec_t *)_nrecord;

    memcpy(nrecord->id.id, raw, (size_t)OBJ_FHEAP_ID_LEN);
    raw += OBJ_FHEAP_ID_LEN;
    nrecord->flags = *raw++;
    UINT32DECODE(raw, nrecord->corder)
    UINT32DECODE(raw, nrecord->hash)

    return (SUCCEED);
} /* A_dense_btree2_name_decode() */

/* Indexed attribute on creation order */
static ck_err_t
A_dense_btree2_corder_decode(driver_t *file, const uint8_t *raw, void *_nrecord, void *_ck_udata)
{
    A_dense_bt2_corder_rec_t *nrecord = (A_dense_bt2_corder_rec_t *)_nrecord;

    memcpy(nrecord->id.id, raw, (size_t)OBJ_FHEAP_ID_LEN);
    raw += OBJ_FHEAP_ID_LEN;
    nrecord->flags = *raw++;
    UINT32DECODE(raw, nrecord->corder)

    return (SUCCEED);
} /* A_dense_btree2_corder_decode() */

static ck_err_t D_btree2_chunk_decode(driver_t *file, const uint8_t *raw, void *_nrecord, void *_ck_udata)
{
    D_bt2_rec_t *nrecord = (D_bt2_rec_t *)_nrecord;
    OBJ_layout_chunk_t *layout_msg = (OBJ_layout_chunk_t *)_ck_udata;
    int ndims = layout_msg->ndims;

    addr_decode(file->shared, &raw, &(nrecord->addr));
    //printf("Hi %d\n", nrecord->addr);
    //printf("dims: %d\n", layout_msg->ndims);
    //printf("size: %d\n", layout_msg->size);
    D_BT2_CHUNK->nrec_size = sizeof(D_bt2_rec_t) - sizeof(unsigned long long) * (OBJ_LAYOUT_NDIMS - ndims);
    //printf("record size: %d\n", D_BT2_CHUNK->nrec_size);

    logger_add_raw_data_chunk(logger.current_obj, nrecord->addr, nrecord->addr + layout_msg->size);
    return (SUCCEED);
}

static ck_err_t D_btree2_filt_chunk_decode(driver_t *file, const uint8_t *raw, void *_nrecord, void *_ck_udata)
{
    D_bt2_filt_rec_t *nrecord = (D_bt2_filt_rec_t *)_nrecord;
    //printf("Hi\n");
    return (SUCCEED);
}

/* 
 * compare methods: only two are used here
 */
static ck_err_t
HF_huge_btree2_indir_compare(const void *_rec1, const void *_rec2)
{

    return ((ck_err_t)(((const HF_huge_bt2_indir_rec_t *)_rec1)->id - ((const HF_huge_bt2_indir_rec_t *)_rec2)->id));

} /* HF_huge_btree2_indir_compare() */

static ck_err_t
HF_huge_btree2_filt_indir_compare(const void *_rec1, const void *_rec2)
{

    return ((ck_err_t)(((const HF_huge_bt2_filt_indir_rec_t *)_rec1)->id - ((const HF_huge_bt2_filt_indir_rec_t *)_rec2)->id));
} /* HF_huge_btree2_filt_indir_compare() */

static ck_err_t
HF_huge_btree2_dir_compare(const void *_rec1, const void *_rec2)
{
    printf("HF_huge_btree2_dir_compare() Not implemented yet...shouldn't be called\n");
} /* HF_huge_btree2_dir_compare() */

static ck_err_t
HF_huge_btree2_filt_dir_compare(const void *_rec1, const void *_rec2)
{
    printf("HF_huge_btree2_filt_dir_compare() Not implemented yet...shouldn't be called\n");
} /* HF_huge_btree2_filt_dir_compare() */

static ck_err_t
G_dense_btree2_name_compare(const void *_bt2_udata, const void *_bt2_rec)
{
    printf("G_dense_btree2_name_compare() Not implemented yet...shouldn't be called\n");
} /* G_dense_btree2_name_compare() */

static ck_err_t
G_dense_btree2_corder_compare(const void *_bt2_udata, const void *_bt2_rec)
{
    printf("G_dense_btree2_corder_compare() Not implemented yet...shouldn't be called\n");
} /* G_dense_btree2_corder_compare() */

static ck_err_t
SM_message_compare(const void *rec1, const void *rec2)
{
    printf("SM_message_compare() Not implemented yet...shouldn't be called\n");
} /* SM_message_compare() */

static ck_err_t
A_dense_btree2_name_compare(const void *_bt2_udata, const void *_bt2_rec)
{
    printf("A_dense_btree2_name_compare() Not implemented yet...shouldn't be called\n");
} /* A_dense_btree2_name_compare() */

static ck_err_t
A_dense_btree2_corder_compare(const void *_bt2_udata, const void *_bt2_rec)
{
    printf("A_dense_btree2_corder_compare() Not implemented yet...shouldn't be called\n");
} /* A_dense_btree2_corder_compare() */

static ck_err_t
D_btree2_chunk_compare(const void *rec1, const void *rec2)
{
    printf("D_btree2_chunk_compare() Not implemented yet...shouldn't be called\n");
} /* D_btree2_chunk_compare() */

static ck_err_t
D_btree2_filt_chunk_compare(const void *rec1, const void *rec2)
{
    printf("D_btree2_filt_chunk_compare() Not implemented yet...shouldn't be called\n");
} /* D_btree2_filt_chunk_compare() */

/* 
 * version 2 btree found() callbacks: only two are used here
 */
static ck_err_t
HF_huge_bt2_indir_found(const void *nrecord, void *op_data)
{

    *(HF_huge_bt2_indir_rec_t *)op_data = *(const HF_huge_bt2_indir_rec_t *)nrecord;

    return (SUCCEED);
} /* HF_huge_bt2_indir_found() */

static ck_err_t
HF_huge_bt2_filt_indir_found(const void *nrecord, void *op_data)
{
    *(HF_huge_bt2_filt_indir_rec_t *)op_data = *(const HF_huge_bt2_filt_indir_rec_t *)nrecord;

    return (SUCCEED);
} /* HF_huge_bt2_filt_indir_found() */

/*
 * Validate version 2 btree header 
 */
static ck_err_t
check_bt2_hdr(driver_t *file, ck_addr_t bt_hdr_addr, const B2_class_t *type, B2_t **ret_hdr)
{
    int version, badinfo, u;
    uint8_t *buf = NULL, *p;
    ck_size_t hdr_size;
    uint32_t stored_chksum, computed_chksum;
    uint8_t split_percent, merge_percent;
    ck_hsize_t all_nrec;
    B2_t *hdr = NULL;
    B2_shared_t *bt2_shared = NULL;
    uint8_t *start_buf = NULL;
    ck_addr_t logical;
    int ret_value = SUCCEED;

    assert(file);
    assert(addr_defined(bt_hdr_addr));
    assert(type);

    if (debug_verbose())
        printf("VALIDATING version 2 btree header at address %llu...\n", bt_hdr_addr);

    if ((hdr = calloc(1, sizeof(B2_t))) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "Memory allocation error: v2 B-tree header", bt_hdr_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    if ((bt2_shared = calloc(1, sizeof(B2_shared_t))) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "Memory allocation error: v2 B-tree header", bt_hdr_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    hdr->shared = bt2_shared;

    hdr_size = B2_HEADER_SIZE(file->shared);
    if ((buf = malloc(hdr_size)) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "Memory allocation error: v2 B-tree header", bt_hdr_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    p = buf;
    start_buf = buf;
    logical = get_logical_addr(p, start_buf, bt_hdr_addr);

    if (FD_read(file, bt_hdr_addr, hdr_size, buf) < 0)
    {
        error_push(ERR_FILE, ERR_NONE_SEC, "Unable to read header: v2 B-tree header", bt_hdr_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* magic number */
    if (memcmp(p, B2_HDR_MAGIC, (size_t)B2_SIZEOF_MAGIC))
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Invalid signature: v2 B-tree header", bt_hdr_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }
    else if (debug_verbose())
        printf("FOUND version 2 btree header signature.\n");

    p += B2_SIZEOF_MAGIC;
    logical = get_logical_addr(p, start_buf, bt_hdr_addr);

    version = *p++;
    if (version != B2_HDR_VERSION)
    {
        badinfo = version;
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Bad version number: v2 B-tree header", bt_hdr_addr, &badinfo);
        CK_SET_RET(FAIL)
    }

    logical = get_logical_addr(p, start_buf, bt_hdr_addr);
    if ((uint8_t)type->id != *p++)
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Invalid tree type: v2 B-tree header", bt_hdr_addr, NULL);
        CK_SET_RET(FAIL)
    }

    /* NEED CHECK:HOW to validate all the following fields */
    UINT32DECODE(p, bt2_shared->node_size);
    UINT16DECODE(p, bt2_shared->rrec_size);
    UINT16DECODE(p, bt2_shared->depth);

    split_percent = *p++;
    merge_percent = *p++;

    addr_decode(file->shared, (const uint8_t **)&p, &(hdr->root.addr) /*out*/);
    UINT16DECODE(p, hdr->root.node_nrec /*out*/);

    DECODE_LENGTH(file->shared, p, all_nrec);

    logical = get_logical_addr(p, start_buf, bt_hdr_addr);
    UINT32DECODE(p, stored_chksum);

    computed_chksum = checksum_metadata(buf, (hdr_size - B2_SIZEOF_CHKSUM), 0);
    if (computed_chksum != stored_chksum)
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Incorrect checksum: v2 B-tree header", bt_hdr_addr, NULL);
        CK_SET_RET(FAIL)
    }

#ifdef DEBUG
    printf("hdr->root.addr=%llu, hdr->root.node_nrec=%u\n", hdr->root.addr, hdr->root.node_nrec);
    printf("node_size=%u, type->=%u, rrec_size=%u, depth=%u, node_nrec=%u, all_nrec=%u\n",
           bt2_shared->node_size, type->id, bt2_shared->rrec_size, bt2_shared->depth, hdr->root.node_nrec, all_nrec);
#endif

    /* initialize bt2_shared->node_info[] */
    if ((bt2_shared->node_info = calloc((ck_size_t)(bt2_shared->depth + 1), sizeof(B2_node_info_t))) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "Memory allocation error: v2 B-tree header", bt_hdr_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }
    /* Initialize leaf's node_info */
    bt2_shared->node_info[0].max_nrec = B2_NUM_LEAF_REC(bt2_shared->node_size, bt2_shared->rrec_size);
    bt2_shared->node_info[0].cum_max_nrec = bt2_shared->node_info[0].max_nrec;
    bt2_shared->node_info[0].cum_max_nrec_size = 0;
    bt2_shared->max_nrec_size = (V_log2_gen((uint64_t)bt2_shared->node_info[0].max_nrec) + 7) / 8;
    bt2_shared->type = type;

    if (bt2_shared->max_nrec_size > B2_SIZEOF_RECORDS_PER_NODE)
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Incorrect maximum possible # of records: v2 B-tree header", bt_hdr_addr, NULL);
        CK_SET_RET(FAIL)
    }

    /* Initialize internal node's node_info */
    if (bt2_shared->depth > 0)
    {
        for (u = 1; u < (bt2_shared->depth + 1); u++)
        {
            bt2_shared->node_info[u].max_nrec = B2_NUM_INT_REC(file->shared, bt2_shared, u);
            if (bt2_shared->node_info[u].max_nrec > bt2_shared->node_info[u - 1].max_nrec)
            {
                error_push(ERR_LEV_1, ERR_LEV_1A2, "Incorrect maximum # of records for this depth: v2 B-tree header", bt_hdr_addr, NULL);
                CK_SET_RET(FAIL)
            }

            bt2_shared->node_info[u].cum_max_nrec = ((bt2_shared->node_info[u].max_nrec + 1) *
                                                     bt2_shared->node_info[u - 1].cum_max_nrec) +
                                                    bt2_shared->node_info[u].max_nrec;
            bt2_shared->node_info[u].cum_max_nrec_size =
                (V_log2_gen((uint64_t)bt2_shared->node_info[u].cum_max_nrec) + 7) / 8;
        } /* end for */
    }     /* end if */

    /* (uses leaf # of records because its the largest) */
    if ((bt2_shared->nat_off =
             calloc((ck_size_t)bt2_shared->node_info[0].max_nrec, sizeof(ck_size_t))) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "Memory allocation error: v2 B-tree header", bt_hdr_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* Initialize offsets in native key block */
    /* (uses leaf # of records because its the largest) */
    for (u = 0; u < bt2_shared->node_info[0].max_nrec; u++)
        bt2_shared->nat_off[u] = type->nrec_size * u;

done:
    logger_add_btree_node(logger.current_obj, bt_hdr_addr, bt_hdr_addr + hdr_size);

    if (buf)
        free(buf);

    if (hdr)
    {
        if (ret_value == SUCCEED && ret_hdr)
            *ret_hdr = hdr;
        else
            free_bt2_hdr(hdr);
    }

    return (ret_value);
} /* check_bt2_hdr() */

/* Free memory for version 2 B-tree header */
static void
free_bt2_hdr(B2_t *hdr)
{
    assert(hdr);

    if (hdr->shared)
    {
        if (hdr->shared->nat_off)
            free(hdr->shared->nat_off);
        if (hdr->shared->node_info)
            free(hdr->shared->node_info);
        free(hdr->shared);
    }
    free(hdr);
} /* free_bt2_hdr() */

/* 
 * Validate version 2 btree leaf node
 *
 * Modifications:
 *	Add user callback to validate messages
 */
static ck_err_t
check_bt2_leaf(driver_t *file, ck_addr_t addr, B2_shared_t *bt2_shared, unsigned nrec, unsigned UNUSED depth, B2_leaf_t **ret_leaf, ck_op_t ck_op, void *ck_udata)
{
    int u;
    uint8_t *buf = NULL, *p;
    uint32_t stored_chksum, computed_chksum;
    uint8_t *native;
    B2_leaf_t *leaf = NULL;
    uint8_t *start_buf = NULL;
    ck_addr_t logical;
    int ret_value = SUCCEED;

    assert(file);
    assert(addr_defined(addr));
    assert(bt2_shared);

    if (debug_verbose())
        printf("VALIDATING version 2 btree leaf node at address %llu...\n", addr);

    if ((leaf = calloc(1, sizeof(B2_leaf_t))) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "Memory allocation error: v2 B-tree leaf node", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    leaf->shared = bt2_shared;
    leaf->nrec = nrec;

    if ((buf = malloc(bt2_shared->node_size)) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "Memory allocation error: v2 B-tree leaf node", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }
    p = buf;
    start_buf = buf;
    logical = get_logical_addr(p, start_buf, addr);

    if (FD_read(file, addr, bt2_shared->node_size, buf) == FAIL)
    {
        error_push(ERR_FILE, ERR_NONE_SEC, "Unable to read node: v2 B-tree leaf node", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* magic number */
    if (memcmp(p, B2_LEAF_MAGIC, (size_t)B2_SIZEOF_MAGIC))
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Invalid signature: v2 B-tree leaf node", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }
    else if (debug_verbose())
        printf("FOUND version 2 btree leaf signature.\n");

    p += B2_SIZEOF_MAGIC;

    logical = get_logical_addr(p, start_buf, addr);

    if (*p++ != B2_LEAF_VERSION)
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Invalid version: b2 B-tree leaf node", addr, NULL);
        CK_SET_RET(FAIL)
    }

    logical = get_logical_addr(p, start_buf, addr);
    if (*p++ != bt2_shared->type->id)
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Incorrect tree type: v2 B-tree leaf node", addr, NULL);
        CK_SET_RET(FAIL)
    }

    if ((leaf->leaf_native = calloc(leaf->nrec, bt2_shared->type->nrec_size)) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "Memory allocation error: v2 B-tree leaf node", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    native = leaf->leaf_native;
    for (u = 0; u < leaf->nrec; u++)
    {
        logical = get_logical_addr(p, start_buf, addr);
        if (bt2_shared->type->decode(file, p, native, ck_udata) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1A2, "Errors from decoding B-tree record: v2 B-tree leaf node", addr, NULL);
            CK_SET_RET(FAIL)
        }

        if (ck_op && ck_op(file, native, ck_udata) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1A2, "Errors from callback: v2 B-tree leaf node", addr, NULL);
            CK_SET_RET(FAIL)
        }

        p += bt2_shared->rrec_size;
        native += bt2_shared->type->nrec_size;
    }

    /* Compute checksum on internal node */
    computed_chksum = checksum_metadata(buf, (size_t)(p - buf), 0);

    logical = get_logical_addr(p, start_buf, addr);
    UINT32DECODE(p, stored_chksum);

    if (computed_chksum != stored_chksum)
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Incorrect checksum: v1 B-tree leaf node", addr, NULL);
        CK_SET_RET(FAIL)
    }

done:

    logger_add_btree_node(logger.current_obj, addr, addr + bt2_shared->node_size);
    
    if (buf)
        free(buf);

    if (leaf)
    {
        if (ret_value == SUCCEED && ret_leaf)
            *ret_leaf = leaf;
        else
            free_bt2_leaf(leaf);
    }

    return (ret_value);
} /* check_bt2_leaf(() */

/* Free memory for v2 B-tree leaf node */
static void
free_bt2_leaf(B2_leaf_t *leaf)
{
    assert(leaf);

    if (leaf->leaf_native)
        free(leaf->leaf_native);
    free(leaf);
} /* free_bt2_leaf() */

/*
 * Validate version 2 btree internal node
 * 
 * Modifications:
 *	Add user callback to validate messages
 */
static ck_err_t
check_bt2_internal(driver_t *file, ck_addr_t addr, B2_shared_t *bt2_shared, unsigned nrec, unsigned depth, B2_internal_t **ret_internal, ck_op_t ck_op, void *ck_udata)
{
    int version, badinfo;
    B2_subid_t type_id;
    uint32_t stored_chksum, computed_chksum;
    unsigned u;
    uint8_t *buf = NULL, *p;
    B2_internal_t *internal = NULL;
    uint8_t *native;
    B2_node_ptr_t *int_node_ptr;
    uint8_t *start_buf = NULL;
    ck_addr_t logical;
    int ret_value = SUCCEED;

    assert(file);
    assert(addr_defined(addr));
    assert(bt2_shared);

    if (debug_verbose())
        printf("VALIDATING version 2 btree internal node at address %llu...\n", addr);

    if ((internal = calloc(1, sizeof(B2_internal_t))) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "Memory allocation error: v2 B-tree internal node", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    internal->shared = bt2_shared;
    internal->nrec = nrec;
    internal->depth = depth;

    if ((buf = malloc(bt2_shared->node_size)) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "Memory allocation error: v2 B-tree internal node", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }
    p = buf;
    start_buf = buf;
    logical = get_logical_addr(p, start_buf, addr);

    if (FD_read(file, addr, bt2_shared->node_size, buf) == FAIL)
    {
        error_push(ERR_FILE, ERR_NONE_SEC, "Unable to read internal header: v2 B-tree internal node", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* magic number */
    if (memcmp(p, B2_INT_MAGIC, (size_t)B2_SIZEOF_MAGIC))
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2,
                   "Invalid signature: v2 B-tree internal node", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }
    else if (debug_verbose())
        printf("FOUND version 2 btree internal signature.\n");

    p += B2_SIZEOF_MAGIC;

    logical = get_logical_addr(p, start_buf, addr);
    version = *p++;
    if (version != B2_INT_VERSION)
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Invalid version: v2 B-tree internal node", addr, NULL);
        CK_SET_RET(FAIL)
    }

    logical = get_logical_addr(p, start_buf, addr);
    type_id = *p++;
    if (type_id != bt2_shared->type->id)
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Incorrect tree type: v2 B-tree internal node", addr, NULL);
        CK_SET_RET(FAIL)
    }

    /* records */
    if ((internal->int_native = calloc(internal->nrec, bt2_shared->type->nrec_size)) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "Memory allocation error: v2 B-tree internal node", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    native = internal->int_native;
    for (u = 0; u < internal->nrec; u++)
    {
        logical = get_logical_addr(p, start_buf, addr);
        if (bt2_shared->type->decode(file, p, native, ck_udata) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1A2, "Errors from decoding B-tree record: v2 B-tree internal node", addr, NULL);
            CK_SET_RET(FAIL)
        }

        if (ck_op && ck_op(file, native, ck_udata) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1A2, "Errors from callback: v2 B-tree internal node", addr, NULL);
            CK_SET_RET(FAIL)
        }
        p += bt2_shared->rrec_size;
        native += bt2_shared->type->nrec_size;
    }

    /* node pointers */
    if ((internal->node_ptrs = calloc(internal->nrec + 1, sizeof(B2_node_ptr_t))) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "Memory allocation error: v2 B-tree internal node", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    int_node_ptr = internal->node_ptrs;
    for (u = 0; u < internal->nrec + 1; u++)
    {
        addr_decode(file->shared, (const uint8_t **)&p, &int_node_ptr->addr);
        UINT64DECODE_VAR(p, int_node_ptr->node_nrec, bt2_shared->max_nrec_size);

#ifdef DEBUG
        printf("int_node_ptr->addr=%llu, int_node_ptr->node_nrec=%u\n",
               int_node_ptr->addr, int_node_ptr->node_nrec);
#endif

        if (depth > 1)
            UINT64DECODE_VAR(p, int_node_ptr->all_nrec, bt2_shared->node_info[depth - 1].cum_max_nrec_size)
        else
            int_node_ptr->all_nrec = int_node_ptr->node_nrec;
        int_node_ptr++;
    }

    computed_chksum = checksum_metadata(buf, (size_t)(p - buf), 0);

    logical = get_logical_addr(p, start_buf, addr);
    UINT32DECODE(p, stored_chksum);
    if (computed_chksum != stored_chksum)
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Incorrect checksum: v2 B-tree internal node", addr, NULL);
        CK_SET_RET(FAIL)
    }

done:

    logger_add_btree_node(logger.current_obj, addr, addr + bt2_shared->node_size);

    if (buf)
        free(buf);

    if (internal)
    {
        if (ret_value == SUCCEED && ret_internal)
            *ret_internal = internal;
        else
            free_bt2_internal(internal);
    }

    return (ret_value);
} /* check_bt2_internal() */

/* Free memory for v2 B-tree internal node */
static void
free_bt2_internal(B2_internal_t *internal)
{
    assert(internal);

    if (internal->int_native)
        free(internal->int_native);
    if (internal->node_ptrs)
        free(internal->node_ptrs);
    free(internal);
} /* free_bt2_internal() */

/*
 * Validate version 2 btree leaf or internal nodes
 */
static ck_err_t
check_bt2_real(driver_t *file, ck_addr_t addr, B2_shared_t *bt2_shared, unsigned nrec, unsigned depth, ck_op_t ck_op, void *ck_udata)
{
    int ret_value = SUCCEED;
    B2_internal_t *internal = NULL;
    B2_leaf_t *leaf = NULL;
    unsigned u;

    assert(file);
    assert(addr_defined(addr));
    assert(bt2_shared);

    if (depth > 0)
    { /* internal */
        if (check_bt2_internal(file, addr, bt2_shared, nrec, depth, &internal, ck_op, ck_udata) < 0)
            CK_SET_RET_DONE(FAIL)

        /* Iterate through node ptrs */
        for (u = 0; u < internal->nrec + 1; u++)
        {
            if (check_bt2_real(file, internal->node_ptrs[u].addr, bt2_shared, internal->node_ptrs[u].node_nrec, depth - 1, ck_op, ck_udata) < 0)
                CK_SET_RET_DONE(FAIL)
        }
    }
    else
    { /* leaf */
        if (check_bt2_leaf(file, addr, bt2_shared, nrec, depth, &leaf, ck_op, ck_udata) < 0)
            CK_SET_RET_DONE(FAIL)
    }

done:
    if (internal)
        free_bt2_internal(internal);

    if (leaf)
        free_bt2_leaf(leaf);

    return (ret_value);
} /* check_bt2_real() */

/*
 * ENTRY to validate version 2 btree 
 */
ck_err_t
check_btree2(driver_t *file, ck_addr_t btree_addr, const B2_class_t *type, ck_op_t ck_op, void *ck_udata)
{
    ck_err_t ret_value = SUCCEED; /* return value */
    ck_err_t ret_err = 0;         /* errors from the current routine */

    B2_t *hdr = NULL;

    assert(file);
    assert(addr_defined(btree_addr));
    assert(type);

    if (debug_verbose())
        printf("VALIDATING version 2 btree at logical address %llu...\n", btree_addr);

    if (check_bt2_hdr(file, btree_addr, type, &hdr) < 0)
        CK_INC_ERR_DONE

    if (addr_defined(hdr->root.addr))
    {
        if (check_bt2_real(file, hdr->root.addr, hdr->shared, hdr->root.node_nrec, hdr->shared->depth, ck_op, ck_udata) < 0)
            CK_INC_ERR_DONE
    }
    else
    { /* shouldn't happen */
        error_push(ERR_LEV_1, ERR_LEV_1A2, "Undefined v2 B-tree root node address", -1, NULL);
        CK_INC_ERR_DONE
    }

done:
    if (hdr)
        free_bt2_hdr(hdr);

    if (ret_err && !object_api())
    {
        error_print(stderr, file);
        error_clear();
    }

    if (ret_err)
        ret_value = FAIL;
    return (ret_value);
} /* check_btree2() */

/*
 * version 2 btree support routine to find a certain record
 */
static int
B2_locate_record(const B2_class_t *type, unsigned nrec, ck_size_t *rec_off,
                 const uint8_t *native, const void *udata, unsigned *idx)
{
    unsigned lo = 0, hi; /* Low & high index values */
    unsigned my_idx = 0; /* Final index value */
    int cmp = -1;        /* Key comparison value */

    hi = nrec;
    while (lo < hi && cmp)
    {
        my_idx = (lo + hi) / 2;
        if ((cmp = (type->compare)(udata, native + rec_off[my_idx])) < 0)
            hi = my_idx;
        else
            lo = my_idx + 1;
    }

    *idx = my_idx;

    return (cmp);
} /* B2_locate_record() */

/* 
 * version 2 btree support routine to finding retrieval info of huge objects
 */
static ck_err_t
B2_find(driver_t *file, const B2_class_t *type, ck_addr_t addr, void *udata, B2_found_t op, void *op_data)
{
    B2_t *bt2_hdr = NULL;
    B2_shared_t *bt2_shared = NULL;
    B2_node_ptr_t curr_node_ptr;    /* Node pointer info for current node */
    unsigned depth;                 /* Current depth of the tree */
    int cmp;                        /* Comparison value of records */
    unsigned idx;                   /* Location of record which matches key */
    B2_internal_t *internal = NULL; /* Pointer to internal node in B-tree */
    B2_leaf_t *leaf = NULL;
    ck_err_t ret_value = SUCCEED;

    assert(file);
    assert(type);
    assert(addr_defined(addr));

    /* Look up the B-tree header */
    if (check_bt2_hdr(file, addr, type, &bt2_hdr) < 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "v2 B-tree: Error found in validating btree header", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    bt2_shared = bt2_hdr->shared;

    /* Make copy of the root node pointer to start search with */
    curr_node_ptr = bt2_hdr->root;

    /* Current depth of the tree */
    depth = bt2_shared->depth;

    /* Check for empty tree */
    if (curr_node_ptr.node_nrec == 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2, "v2 B-tree:btree has no records", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* Walk down B-tree to find record or leaf node where record is located */
    cmp = -1;
    while (depth > 0 && cmp != 0)
    {
        B2_node_ptr_t next_node_ptr; /* Node pointer info for next node */

        if (check_bt2_internal(file, curr_node_ptr.addr, bt2_shared, curr_node_ptr.node_nrec, depth, &internal, NULL, NULL) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1A2,
                       "v2 B-tree: Error found in validating btree internal node", addr, NULL);
            CK_SET_RET_DONE(FAIL)
        }

        /* Locate node pointer for child */
        cmp = B2_locate_record(bt2_shared->type, internal->nrec, bt2_shared->nat_off, internal->int_native, udata, &idx);
        if (cmp > 0)
            idx++;
        if (cmp != 0)
        {
            /* Get node pointer for next node to search */
            curr_node_ptr = next_node_ptr = internal->node_ptrs[idx];

            /* Set pointer to next node to load */
            /* curr_node_ptr = next_node_ptr; */
        }
        else
        {
            /* Make callback: HF_huge_bt2_filt_indir_found() or HF_huge_bt2_indir_found() */
            if (op && (op)(B2_INT_NREC(internal, bt2_shared, idx), op_data) < 0)
            {
                error_push(ERR_LEV_1, ERR_LEV_1A2,
                           "v2 B-tree: Error found from callback of internal node record", addr, NULL);
                CK_SET_RET_DONE(FAIL)
            }
            CK_SET_RET_DONE(SUCCEED)
        }
        /* Decrement depth we're at in B-tree */
        depth--;
        if (internal)
        {
            free_bt2_internal(internal);
            internal = NULL;
        }
    } /* end while */

    if (check_bt2_leaf(file, curr_node_ptr.addr, bt2_shared, curr_node_ptr.node_nrec, depth, &leaf, NULL, NULL) < 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1A2,
                   "v2 B-tree: Error found in validating btree leaf node", addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* Locate record */
    cmp = B2_locate_record(bt2_shared->type, leaf->nrec, bt2_shared->nat_off, leaf->leaf_native, udata, &idx);

    if (cmp != 0)
        CK_SET_RET_DONE(FAIL)
    else
    {
        /* Make callback: HF_huge_bt2_filt_indir_found(), HF_huge_bt2_indir_found() */
        if (op && (op)(B2_LEAF_NREC(leaf, bt2_shared, idx), op_data) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1A2,
                       "v2 B-tree: Error found from callback of leaf node record", addr, NULL);
            CK_SET_RET_DONE(FAIL)
        } /* end if */
    }     /* end else */

done:
    if (bt2_hdr)
        free_bt2_hdr(bt2_hdr);

    if (internal)
        free_bt2_internal(internal);

    if (leaf)
        free_bt2_leaf(leaf);

    return (ret_value);
} /* B2_find() */

/*
 * END version 2 btree Validation
 */

/* 
 * START Free Space Manager validation 
 */

/* Size of serialized indirect section information */
#define HF_SECT_INDIRECT_SERIAL_SIZE(h) (                            \
    (h)->heap_off_size /* Indirect block's offset in "heap space" */ \
    + 2                /* Row */                                     \
    + 2                /* Column */                                  \
    + 2                /* # of entries */                            \
)

/* init callback */
static ck_err_t
HF_sect_row_init_cls(FS_section_class_t *cls, HF_hdr_t *fh_hdr)
{
    ck_err_t ret_value = SUCCEED; /* Return value */

    assert(cls);
    assert(fh_hdr);

    if (cls->type == HF_FSPACE_SECT_FIRST_ROW)
        cls->serial_size = HF_SECT_INDIRECT_SERIAL_SIZE(fh_hdr);
    else
        cls->serial_size = 0;

done:
    return (ret_value);
} /* HF_sect_row_init_cls() */

/* init callback */
static ck_err_t
HF_sect_indirect_init_cls(FS_section_class_t *cls, HF_hdr_t *fh_hdr)
{
    ck_err_t ret_value = SUCCEED;

    assert(cls);
    assert(fh_hdr);

    cls->serial_size = HF_SECT_INDIRECT_SERIAL_SIZE(fh_hdr);

done:
    return (ret_value);
} /* HF_sect_indirect_init_cls() */

/* Validate Free Space Section List */
static ck_err_t
check_fssection(driver_t *file, ck_addr_t fssect_addr, FS_hdr_t *fs_hdr)
{
    uint8_t *buf = NULL;
    ck_addr_t fshdr_addr;
    int ret_value = SUCCEED;
    size_t old_sect_size;
    const uint8_t *p;
    uint32_t stored_chksum, computed_chksum;
    uint8_t *start_buf = NULL;
    ck_addr_t logical;
    int version, badinfo;

    assert(file);
    assert(addr_defined(fssect_addr));
    assert(fs_hdr);

    if (debug_verbose())
        printf("VALIDATING the Free Space Section List %llu...\n", fssect_addr);

    if (fs_hdr->sect_addr != fssect_addr)
    {
        error_push(ERR_FILE, ERR_NONE_SEC,
                   "Free Space Section List:Incorrect address for free space sections", fssect_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    ASSIGN_OVERFLOW(/* To: */ old_sect_size, /* From: */ fs_hdr->sect_size, /* From: */ ck_hsize_t, /* To: */ ck_size_t);

    if (NULL == (buf = malloc((size_t)fs_hdr->sect_size)))
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC,
                   "Free Space Section List:Internal allocation error", fssect_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    if (FD_read(file, fssect_addr, fs_hdr->sect_size, buf) == FAIL)
    {
        error_push(ERR_FILE, ERR_NONE_SEC,
                   "Free Space Section List:Unable to read in free space section list", fssect_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    p = buf;
    start_buf = buf;
    logical = get_logical_addr(p, start_buf, fssect_addr);

    /* Magic number */
    if (memcmp(p, FS_SINFO_MAGIC, (ck_size_t)FS_SIZEOF_MAGIC))
    {
        error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Section List:Wrong signature", logical, NULL);
        CK_SET_RET_DONE(FAIL)
    }
    else if (debug_verbose())
        printf("FOUND Free Space Section List signature.\n");

    p += FS_SIZEOF_MAGIC;

    logical = get_logical_addr(p, start_buf, fssect_addr);
    version = *p++;
    if (version != FS_SINFO_VERSION)
    {
        error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Section List:Wrong version", logical, &badinfo);
        CK_SET_RET(FAIL)
    }

    /* Address of free space header for these sections */
    logical = get_logical_addr(p, start_buf, fssect_addr);
    addr_decode(file->shared, &p, &fshdr_addr);
    if (fshdr_addr != fs_hdr->addr)
    {
        error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Section List:Incorrect free space manager header address", logical, NULL);
        CK_SET_RET(FAIL)
    }

    /* Check for any serialized sections */
    if (fs_hdr->serial_sect_count > 0)
    {
        unsigned sect_cnt_size;
        unsigned sect_len_size;
        unsigned sect_off_size;

        sect_cnt_size = (V_log2_gen((uint64_t)fs_hdr->serial_sect_count) / 8) + 1;
        sect_len_size = (V_log2_gen((uint64_t)fs_hdr->max_sect_size) / 8) + 1;
        sect_off_size = (fs_hdr->max_sect_addr + 7) / 8;

        do
        {
            ck_hsize_t sect_size; /* Current section size */
            ck_size_t node_count; /* # of sections of this size */
            ck_size_t u;          /* Local index variable */

            /* The number of sections of this node's size */
            logical = get_logical_addr(p, start_buf, fssect_addr);
            UINT64DECODE_VAR(p, node_count, sect_cnt_size);
            if (node_count <= 0)
            {
                error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Section List:Incorrect # of sections", logical, NULL);
                CK_SET_RET(FAIL)
            }

            logical = get_logical_addr(p, start_buf, fssect_addr);
            UINT64DECODE_VAR(p, sect_size, sect_len_size);
            if (sect_size <= 0)
            {
                error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Section List:Incorrect size of the sections", logical, NULL);
                CK_SET_RET(FAIL)
            }

            /* Loop over nodes of this size */
            for (u = 0; u < node_count; u++)
            {
                ck_addr_t sect_addr;
                unsigned sect_type;
                unsigned des_flags;

                /* The address of the section */
                UINT64DECODE_VAR(p, sect_addr, sect_off_size);

                /* The type of this section */
                logical = get_logical_addr(p, start_buf, fssect_addr);
                sect_type = *p++;
                if (sect_type > fs_hdr->nclasses)
                {
                    sect_type = HF_FSPACE_SECT_SINGLE;
                    error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Section List:Incorrect section type", logical, NULL);
                    CK_SET_RET(FAIL)
                }

                p += fs_hdr->sect_cls[sect_type].serial_size;
            } /* end for */
        } while (p < ((buf + old_sect_size) - FS_SIZEOF_CHKSUM));
    } /* end if */

    logical = get_logical_addr(p, start_buf, fssect_addr);
    computed_chksum = checksum_metadata(buf, (size_t)(p - buf), 0);
    UINT32DECODE(p, stored_chksum);

    if (computed_chksum != stored_chksum)
    {
        error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Section List:Incorrect checksum", logical, NULL);
        CK_SET_RET(FAIL)
    }

done:
    if (buf)
        free(buf);

    return (ret_value);
} /* end check_fssection() */

/* validate free space manager header */
static ck_err_t
check_fshdr(driver_t *file, ck_addr_t fs_addr, HF_hdr_t *fh_hdr)
{
    FS_hdr_t *fs_hdr = NULL;
    uint8_t hdr_buf[FS_HDR_BUF_SIZE];
    ck_size_t size;
    const uint8_t *p;
    uint32_t stored_chksum, computed_chksum;
    int ret_value = SUCCEED;
    uint8_t *start_buf = NULL;
    ck_addr_t logical;
    ck_size_t u, nclasses;
    int version, badinfo;

    /* Check arguments */
    assert(file);
    assert(addr_defined(fs_addr));
    assert(fh_hdr);

    if (debug_verbose())
        printf("VALIDATING the free space manager header at %llu...\n", fs_addr);

    if (NULL == (fs_hdr = calloc(1, sizeof(FS_hdr_t))))
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC,
                   "Free Space Manager Header:Internal allocation error", fs_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    nclasses = NELMTS(classes);
    fs_hdr->nclasses = nclasses;
    if (nclasses > 0)
    {
        if ((fs_hdr->sect_cls = calloc(nclasses, sizeof(FS_section_class_t))) == NULL)
        {
            error_push(ERR_INTERNAL, ERR_NONE_SEC,
                       "Free Space Manager Header:Internal allocation error", fs_addr, NULL);
            CK_SET_RET_DONE(FAIL)
        }
        for (u = 0; u < nclasses; u++)
        {
            if (u != classes[u]->type)
            {
                error_push(ERR_INTERNAL, ERR_NONE_SEC,
                           "Free Space Manager Header:Internal class type error", fs_addr, NULL);
                CK_SET_RET_DONE(FAIL)
            }
            memcpy(&fs_hdr->sect_cls[u], classes[u], sizeof(FS_section_class_t));
            if (fs_hdr->sect_cls[u].init_cls)
                if ((fs_hdr->sect_cls[u].init_cls)(&fs_hdr->sect_cls[u], fh_hdr) < 0)
                {
                    error_push(ERR_INTERNAL, ERR_NONE_SEC,
                               "Free Space Manager Header:Internal initialization error of section class", fs_addr, NULL);
                    CK_SET_RET_DONE(FAIL)
                }
        }
    }
    fs_hdr->addr = CK_ADDR_UNDEF;
    fs_hdr->sect_addr = CK_ADDR_UNDEF;

    fs_hdr->addr = fs_addr;
    size = FS_HEADER_SIZE(file->shared);

    if (FD_read(file, fs_addr, size, hdr_buf) == FAIL)
    {
        error_push(ERR_FILE, ERR_NONE_SEC, "Free Space Manager Header:Unable to read in header", fs_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    p = hdr_buf;
    start_buf = hdr_buf;
    logical = get_logical_addr(p, start_buf, fs_addr);

    /* Magic number */
    if (memcmp(p, FS_HDR_MAGIC, (size_t)FS_SIZEOF_MAGIC))
    {
        error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Manager Header:Wrong header signature", logical, NULL);
        CK_SET_RET_DONE(FAIL)
    }
    else if (debug_verbose())
        printf("FOUND Free Space Manager Header signature.\n");

    p += FS_SIZEOF_MAGIC;
    logical = get_logical_addr(p, start_buf, fs_addr);

    /* Version */
    version = *p++;
    if (version != FS_HDR_VERSION)
    {
        badinfo = version;
        error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Manager Header:Wrong header version", logical, &badinfo);
        CK_SET_RET(FAIL)
    }

    /* Client ID */
    logical = get_logical_addr(p, start_buf, fs_addr);
    fs_hdr->client = *p++;
    if (fs_hdr->client >= FS_NUM_CLIENT_ID)
    {
        error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Manager Header:Unknown client ID", logical, NULL);
        CK_SET_RET(FAIL)
    }

    DECODE_LENGTH(file->shared, p, fs_hdr->tot_space);
    DECODE_LENGTH(file->shared, p, fs_hdr->tot_sect_count);
    DECODE_LENGTH(file->shared, p, fs_hdr->serial_sect_count);
    DECODE_LENGTH(file->shared, p, fs_hdr->ghost_sect_count);

    logical = get_logical_addr(p, start_buf, fs_addr);
    UINT16DECODE(p, nclasses);

    if (fs_hdr->nclasses > 0 && fs_hdr->nclasses != nclasses)
    {
        error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Manager Header:Section class count mismatch", logical, NULL);
        CK_SET_RET(FAIL)
    }

    UINT16DECODE(p, fs_hdr->shrink_percent);
    UINT16DECODE(p, fs_hdr->expand_percent);
    UINT16DECODE(p, fs_hdr->max_sect_addr);

    DECODE_LENGTH(file->shared, p, fs_hdr->max_sect_size);
    addr_decode(file->shared, &p, &fs_hdr->sect_addr);

    logical = get_logical_addr(p, start_buf, fs_addr);
    DECODE_LENGTH(file->shared, p, fs_hdr->sect_size);
    DECODE_LENGTH(file->shared, p, fs_hdr->alloc_sect_size);

    if (fs_hdr->sect_size > fs_hdr->alloc_sect_size)
    {
        error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Manager Header:Invalid section size", logical, NULL);
        CK_SET_RET(FAIL)
    }

    /* Metadata checksum */
    logical = get_logical_addr(p, start_buf, fs_addr);
    computed_chksum = checksum_metadata(hdr_buf, (size_t)(p - hdr_buf), 0);
    UINT32DECODE(p, stored_chksum);
    if (computed_chksum != stored_chksum)
    {
        error_push(ERR_LEV_1, ERR_LEV_1G, "Free Space Manager Header:Incorrect checksum", logical, NULL);
        CK_SET_RET(FAIL)
    }

    /* check free space section list */
    if (addr_defined(fs_hdr->sect_addr))
    {
        if (check_fssection(file, fs_hdr->sect_addr, fs_hdr) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F,
                       "Free Space Manager Header:Errors found when validating free space section list\n",
                       -1, NULL);
            CK_SET_RET(FAIL)
        }
    }

done:
    if (fs_hdr)
    {
        if (fs_hdr->sect_cls)
            free(fs_hdr->sect_cls);
        free(fs_hdr);
    }

    return (ret_value);
} /* check_fshdr() */

/*
 *  END Free Space Manager validation
 */

/* 
 * START fractal heap validation
 */

/* Lookup table for specialized log2(n) of power of two routine */
static const unsigned MultiplyDeBruijnBitPosition[32] =
    {
        0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
        31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9};

static unsigned
V_log2_of2(uint32_t n)
{
    return (MultiplyDeBruijnBitPosition[(n * (uint32_t)0x077CB531UL) >> 27]);
} /* V_log2_of2() */

/* 
 * Fractal Heap: doubling table initialization
 */
static ck_err_t
HF_dtable_init(HF_dtable_t *dtable)
{
    ck_hsize_t tmp_block_size;    /* Temporary block size */
    ck_hsize_t acc_block_off;     /* Accumulated block offset */
    ck_size_t u;                  /* Local index variable */
    ck_err_t ret_value = SUCCEED; /* Return value */

    assert(dtable);

    if (debug_verbose())
        printf("INITIALIZING the fractal heap doubling table ...\n");

    /* INITIALIZE header info for later use based on parameter values */
    dtable->start_bits = V_log2_of2((uint32_t)dtable->cparam.start_block_size);
    dtable->first_row_bits = dtable->start_bits + V_log2_of2(dtable->cparam.width);
    dtable->num_id_first_row = dtable->cparam.start_block_size * dtable->cparam.width;

    dtable->max_root_rows = (dtable->cparam.max_index - dtable->first_row_bits) + 1;
    dtable->max_direct_bits = V_log2_of2((uint32_t)dtable->cparam.max_direct_size);
    dtable->max_direct_rows = (dtable->max_direct_bits - dtable->start_bits) + 2;
    dtable->max_dir_blk_off_size = HF_SIZEOF_OFFSET_LEN(dtable->cparam.max_direct_size);

    /* Build table of block sizes for each row */
    if (NULL == (dtable->row_block_size = malloc(dtable->max_root_rows * sizeof(ck_hsize_t))))
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC,
                   "Doubling Table Initialization:Internal allocation error", -1, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    tmp_block_size = dtable->cparam.start_block_size;
    dtable->row_block_size[0] = dtable->cparam.start_block_size;

    for (u = 1; u < dtable->max_root_rows; u++)
    {
        dtable->row_block_size[u] = tmp_block_size;
        tmp_block_size *= 2;
    } /* end for */

done:
    return (ret_value);
} /* HF_dtable_init() */

/* 
 * Compute the row & col of an offset in a doubling-table
 */
static ck_err_t
HF_dtable_lookup(const HF_dtable_t *dtable, ck_hsize_t off, unsigned *row, unsigned *col)
{
    assert(dtable);
    assert(row);
    assert(col);

    /* Check for offset in first row */
    if (off < dtable->num_id_first_row)
    {
        *row = 0;
        ASSIGN_OVERFLOW(/* To: */ *col, /* From: */ (off / dtable->cparam.start_block_size), /* From: */ ck_hsize_t, /* To: */ unsigned);
    }
    else
    {
        unsigned high_bit = V_log2_gen(off);               /* Determine the high bit in the offset */
        ck_hsize_t off_mask = ((ck_hsize_t)1) << high_bit; /* Compute mask for determining column */

        *row = (high_bit - dtable->first_row_bits) + 1;
        ASSIGN_OVERFLOW(/* To: */ *col, /* From: */ ((off - off_mask) / dtable->row_block_size[*row]), /* From: */ ck_hsize_t, /* To: */ unsigned);
    }

    return (SUCCEED);
} /* HF_dtable_lookup() */

/* 
 * Fractal Heap: convert size to row
 */
static unsigned
HF_dtable_size_to_rows(const HF_dtable_t *dtable, ck_hsize_t size)
{
    unsigned rows;

    /*
     * Check arguments.
     */
    assert(dtable);

    rows = (V_log2_gen(size) - dtable->first_row_bits) + 1;

    return (rows);
} /* HF_dtable_size_to_rows() */

static ck_err_t
check_iblock_real(driver_t *file, ck_addr_t iblock_addr, HF_hdr_t *hdr, unsigned nrows, HF_indirect_t **ret_iblock)
{
    HF_indirect_t *iblock = NULL;
    uint8_t iblock_buf[HF_IBLOCK_BUF_SIZE];
    const uint8_t *p;
    ck_addr_t heap_addr;
    uint32_t stored_chksum = 0, computed_chksum = 0;
    size_t u;
    unsigned dir_rows;
    unsigned entry, row, col;
    int i;
    uint8_t *start_buf = NULL;
    ck_addr_t logical;
    int ret_value = SUCCEED;

    /* Allocate space for the fractal heap indirect block */
    if (NULL == (iblock = calloc(1, sizeof(HF_indirect_t))))
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC,
                   "Fractal Heap Indirect Block:Internal allocation error", iblock_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    iblock->nrows = nrows;

    iblock->addr = iblock_addr;
    iblock->nchildren = 0;

    /* Compute size of indirect block */
    iblock->size = HF_MAN_INDIRECT_SIZE(file->shared, hdr, iblock);

    if (FD_read(file, iblock_addr, iblock->size, iblock_buf) == FAIL)
    {
        error_push(ERR_FILE, ERR_NONE_SEC,
                   "Fractal Heap Indirect Block:Unable to read indirect block", iblock_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* Get temporary pointer to serialized indirect block */
    p = iblock_buf;
    start_buf = iblock_buf;
    logical = get_logical_addr(p, start_buf, iblock_addr);

    /* Magic number */
    if (memcmp(p, HF_IBLOCK_MAGIC, (size_t)HF_SIZEOF_MAGIC))
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Indirect Block:Wrong signature", logical, NULL);
        CK_SET_RET(FAIL)
    }

    p += HF_SIZEOF_MAGIC;

    logical = get_logical_addr(p, start_buf, iblock_addr);

    /* Version */
    if (*p++ != HF_IBLOCK_VERSION)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Indirect Block:Wrong version", logical, NULL);
        CK_SET_RET(FAIL)
    }

    logical = get_logical_addr(p, start_buf, iblock_addr);

    /* Address of heap that owns this block */
    addr_decode(file->shared, &p, &heap_addr);

    if (heap_addr != hdr->heap_addr)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Indirect Block:Wrong heap address",
                   logical, NULL);
        CK_SET_RET(FAIL)
    }

    /* Offset of heap within the heap's address space */
    UINT64DECODE_VAR(p, iblock->block_off, hdr->heap_off_size);

#ifdef DEBUG
    printf("Found iblock magic number and version number\n");
    printf("heap_addr found in indirect block=%llu\n", heap_addr);
    printf("iblock->block_off=%llu\n", iblock->block_off);
#endif

    if (NULL == (iblock->ents = malloc(sizeof(HF_indirect_ent_t) * (size_t)(iblock->nrows * hdr->man_dtable.cparam.width))))
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC,
                   "Fractal Heap Indirect Block:Internal allocation error for child block entries", iblock_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    if (hdr->filter_len > 0)
    {
        unsigned dir_rows; /* Number of direct rows in this indirect block */

        /* Compute the number of direct rows for this indirect block */
        dir_rows = MIN(iblock->nrows, hdr->man_dtable.max_direct_rows);

        /* Allocate indirect block filtered entry array */
        if (NULL == (iblock->filt_ents = calloc((ck_size_t)(dir_rows * hdr->man_dtable.cparam.width), sizeof(HF_indirect_filt_ent_t))))
        {

            error_push(ERR_INTERNAL, ERR_NONE_SEC,
                       "Fractal Heap Indirect Block:Internal allocation error", iblock_addr, NULL);
            CK_SET_RET_DONE(FAIL)
        }
    }
    else
        iblock->filt_ents = NULL;

    logical = get_logical_addr(p, start_buf, iblock_addr);
    for (u = 0; u < (iblock->nrows * hdr->man_dtable.cparam.width); u++)
    {
        logical = get_logical_addr(p, start_buf, iblock_addr);

        /* Decode child block address */
        addr_decode(file->shared, &p, &(iblock->ents[u].addr));
        /* Check for heap with I/O filters */
        if (hdr->filter_len > 0)
        {
            /* Sanity check */
            assert(iblock->filt_ents);

            /* Decode extra information for direct blocks */
            if (u < (hdr->man_dtable.max_direct_rows * hdr->man_dtable.cparam.width))
            {
                /* Size of filtered direct block */
                DECODE_LENGTH(file->shared, p, iblock->filt_ents[u].size);

                /* (either both the address & size are defined or both are
                 *  not defined)
                 */
                if (!((addr_defined(iblock->ents[u].addr) && iblock->filt_ents[u].size) || (!addr_defined(iblock->ents[u].addr) && iblock->filt_ents[u].size == 0)))
                {
                    error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Indirect Block:Inconsistent child direct block address v.s. size",
                               logical, NULL);
                    CK_SET_RET(FAIL)
                }

                /* I/O filter mask for filtered direct block */
                UINT32DECODE(p, iblock->filt_ents[u].filter_mask);
            } /* end if */
        }     /* end if */

        /* Count child blocks */
        if (addr_defined(iblock->ents[u].addr))
        {
#ifdef DEBUG
            printf("child block address=%llu\n", iblock->ents[u].addr);
#endif
            iblock->nchildren++;
            iblock->max_child = u;
        } /* end if */
    }     /* end for */

#ifdef DEBUG
    printf("iblock->nchildren=%u\n", iblock->nchildren);
#endif

    if (iblock->nchildren <= 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Indirect Block:should have nonzero # of child blocks",
                   logical, NULL);
        CK_SET_RET(FAIL)
    }

    logical = get_logical_addr(p, start_buf, iblock_addr);
    computed_chksum = checksum_metadata(iblock_buf, (size_t)(p - iblock_buf), 0);
    UINT32DECODE(p, stored_chksum);

    if (computed_chksum != stored_chksum)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Indirect Block:Incorrect checksum", iblock_addr, NULL);
        CK_SET_RET(FAIL)
    }

    assert((size_t)(p - iblock_buf) == iblock->size);

done:
    if (iblock)
    {
        if (ret_value == SUCCEED && ret_iblock)
            *ret_iblock = iblock;
        else
            free_fheap_iblock(iblock);
    }

    return (ret_value);
} /* check_iblock_real() */

/* Free memory for fractal heap indirect block */
static void
free_fheap_iblock(HF_indirect_t *iblock)
{
    assert(iblock);

    if (iblock->ents)
        free(iblock->ents);

    if (iblock->filt_ents)
        free(iblock->filt_ents);

    free(iblock);
} /* free_fheap_iblock() */

/*
 *  Locate a direct block in a managed heap
 */
static ck_err_t
HF_man_dblock_locate(driver_t *file, HF_hdr_t *fhdr, ck_hsize_t obj_off, HF_indirect_t **ret_iblock, unsigned *ret_entry)
{
    ck_addr_t iblock_addr;        /* Indirect block's address */
    HF_indirect_t *iblock;        /* Pointer to indirect block */
    unsigned row, col;            /* Row & column for object's block */
    unsigned entry;               /* Entry of block */
    ck_err_t ret_value = SUCCEED; /* Return value */

    assert(file);
    assert(fhdr);
    assert(fhdr->man_dtable.curr_root_rows); /* Only works for heaps with indirect root block */

#ifdef DEBUG
    printf("Entering HF_man_dblock_locate()\n");
#endif

    /* Look up row & column for object */
    if (HF_dtable_lookup(&fhdr->man_dtable, obj_off, &row, &col) < 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "HF_man_dblock_locate():Can't compute row & column of object", -1, NULL);
        CK_SET_RET(FAIL)
    }

    /* Set initial indirect block info */
    iblock_addr = fhdr->man_dtable.table_addr;

    if (check_iblock_real(file, iblock_addr, fhdr, fhdr->man_dtable.curr_root_rows, &iblock) < 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F,
                   "HF_man_dblock_locate():Errors found when validating Fractal Heap Indirect Block", -1, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* Check for indirect block row */
    while (row >= fhdr->man_dtable.max_direct_rows)
    {
        HF_indirect_t *new_iblock = NULL; /* Pointer to new indirect block */
        unsigned nrows;                   /* Number of rows in new indirect block */

        /* Compute # of rows in child indirect block */
        nrows = (V_log2_gen(fhdr->man_dtable.row_block_size[row]) - fhdr->man_dtable.first_row_bits) + 1;
        if (nrows >= iblock->nrows)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F,
                       "HF_man_dblock_locate():# of rows in child indirect block must be smaller than parent's", -1, NULL);
            CK_SET_RET_DONE(FAIL)
        }

        /* Compute indirect block's entry */
        entry = (row * fhdr->man_dtable.cparam.width) + col;

        /* Locate child indirect block */
        iblock_addr = iblock->ents[entry].addr;

        if (check_iblock_real(file, iblock_addr, fhdr, nrows, &new_iblock) < 0)
        {
            error_push(ERR_NONE_PRIM, ERR_NONE_SEC,
                       "HF_man_dblock_locate():Errors found when validating Fractal Heap Indirect Block", -1, NULL);
            CK_SET_RET_DONE(FAIL)
        }

        if (iblock)
            free_fheap_iblock(iblock);

        /* Switch variables to use new indirect block */
        iblock = new_iblock;

        /* Look up row & column in new indirect block for object */
        if (HF_dtable_lookup(&fhdr->man_dtable, (obj_off - iblock->block_off), &row, &col) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F,
                       "HF_man_dblock_locate():Can't compute row & column of object", -1, NULL);
            CK_SET_RET_DONE(FAIL)
        }
        if (row >= iblock->nrows)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F,
                       "HF_man_dblock_locate():Internal:Invalid # of rows", -1, NULL);
            CK_SET_RET_DONE(FAIL)
        }
    }

    /* Set return parameters */
    if (ret_entry)
        *ret_entry = (row * fhdr->man_dtable.cparam.width) + col;

done:
    if (iblock)
    {
        if (ret_value == SUCCEED && ret_iblock)
            *ret_iblock = iblock;
        else
            free_fheap_iblock(iblock);
    }

    return (ret_value);
} /* HF_man_dblock_locate() */

/* Validating fractal heap: direct block */
static ck_err_t
check_dblock(driver_t *file, ck_addr_t dblock_addr, HF_hdr_t *hdr, ck_hsize_t dblock_size, HF_parent_t *par_info, HF_direct_t **ret_dblock)
{
    HF_direct_t *dblock = NULL; /* Direct block info */
    const uint8_t *p;           /* Pointer into raw data buffer */
    ck_addr_t heap_addr;        /* Address of heap header in the file */
    int ret_value = SUCCEED;    /* Return value */
    uint8_t *start_buf = NULL;
    ck_addr_t logical;

    /* Check arguments */
    assert(file);
    assert(addr_defined(dblock_addr));

    if (debug_verbose())
        printf("VALIDATING the fractal heap direct block at %llu...\n", dblock_addr);

    /* Allocate space for the fractal heap direct block */
    if (NULL == (dblock = malloc(sizeof(HF_direct_t))))
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC,
                   "Fractal Heap Direct Block:Internal allocation error", dblock_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* Set block's internal information */
    dblock->size = dblock_size;
    dblock->blk_off_size = HF_SIZEOF_OFFSET_LEN(dblock->size);
    if ((dblock->blk = malloc((size_t)dblock->size)) == NULL)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC,
                   "Fractal Heap Direct Block:Internal allocation error", dblock_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* Check for I/O filters on this heap */
    if (hdr->filter_len > 0)
    {
        Z_cb_t filter_cb = {NULL, NULL}; /* Filter callback structure */
        ck_size_t nbytes;                /* Number of bytes used in buffer, after applying reverse filters */
        void *read_buf;                  /* Pointer to buffer to read in */
        ck_size_t read_size;             /* Size of filtered direct block to read */
        unsigned filter_mask;            /* Excluded filters for direct block */

        /* Check for root direct block */
        if (par_info->iblock == NULL)
        {
            assert(addr_eq(hdr->man_dtable.table_addr, dblock_addr));

            /* Set up parameters to read filtered direct block */
            read_size = hdr->pline_root_direct_size;
            filter_mask = hdr->pline_root_direct_filter_mask;
        }
        else
        {
            assert(addr_eq(par_info->iblock->ents[par_info->entry].addr, dblock_addr));

            /* Set up parameters to read filtered direct block */
            read_size = par_info->iblock->filt_ents[par_info->entry].size;
            filter_mask = par_info->iblock->filt_ents[par_info->entry].filter_mask;
        } /* end else */

        if (NULL == (read_buf = malloc(read_size)))
        {
            error_push(ERR_INTERNAL, ERR_NONE_SEC,
                       "Fractal Heap Direct Block:Internal allocation error for pipeline", dblock_addr, NULL);
            CK_SET_RET_DONE(FAIL)
        }

        /* Read filtered direct block from disk */
        if (FD_read(file, dblock_addr, read_size, read_buf) == FAIL)
        {
            error_push(ERR_FILE, ERR_NONE_SEC,
                       "Fractal Heap Direct Block:Unable to read filtered direct block", dblock_addr, NULL);
            CK_SET_RET_DONE(FAIL)
        }

        /* Push direct block data through I/O filter pipeline */
        nbytes = read_size;
        if (filter_pline(hdr->pline, Z_FLAG_REVERSE, &filter_mask, Z_ENABLE_EDC,
                         filter_cb, &nbytes, &read_size, &read_buf) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F,
                       "Fractal Heap Direct Block:Errors found in filter pipeline", dblock_addr, NULL);
            CK_SET_RET_DONE(FAIL)
        }

        if (nbytes != dblock->size)
        {
            error_push(ERR_FILE, ERR_NONE_SEC,
                       "Fractal Heap Direct Block:Unable to read direct block", dblock_addr, NULL);
            CK_SET_RET_DONE(FAIL)
        }

        /* Copy un-filtered data into block's buffer */
        memcpy(dblock->blk, read_buf, dblock->size);

        /* Release the read buffer */
        if (read_buf)
            free(read_buf);
    }
    else
    {
        /* Read direct block from disk */
        if (FD_read(file, dblock_addr, dblock->size, dblock->blk) == FAIL)
        {
            error_push(ERR_FILE, ERR_NONE_SEC,
                       "Fractal Heap Direct Block:Unable to read direct block", dblock_addr, NULL);
            CK_SET_RET_DONE(FAIL)
        }
    }

    /* Start decoding direct block */
    p = dblock->blk;
    start_buf = dblock->blk;
    logical = get_logical_addr(p, start_buf, dblock_addr);

    /* Magic number */
    if (memcmp(p, HF_DBLOCK_MAGIC, (size_t)HF_SIZEOF_MAGIC))
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Direct Block:Wrong signature", logical, NULL);
        CK_SET_RET(FAIL)
    }

    p += HF_SIZEOF_MAGIC;
    logical = get_logical_addr(p, start_buf, dblock_addr);

    /* Version */
    if (*p++ != HF_DBLOCK_VERSION)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Direct Block:Wrong version", logical, NULL);
        CK_SET_RET(FAIL)
    }

    logical = get_logical_addr(p, start_buf, dblock_addr);

    /* Address of heap that owns this block (just for file integrity checks) */
    addr_decode(file->shared, &p, &heap_addr);
    if (heap_addr != hdr->heap_addr)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Direct Block:Wrong heap address",
                   logical, NULL);
        CK_SET_RET(FAIL)
    }

    /* Offset of heap within the heap's address space */
    UINT64DECODE_VAR(p, dblock->block_off, hdr->heap_off_size);

    if (hdr->checksum_dblocks)
    {
        uint32_t stored_chksum, computed_chksum;

        logical = get_logical_addr(p, start_buf, dblock_addr);
        UINT32DECODE(p, stored_chksum);
        memset((uint8_t *)p - HF_SIZEOF_CHKSUM, 0, (size_t)HF_SIZEOF_CHKSUM);
        computed_chksum = checksum_metadata(dblock->blk, dblock->size, 0);
        if (computed_chksum != stored_chksum)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Direct Block:Incorrect checksum",
                       logical, NULL);
            CK_SET_RET(FAIL)
        }

    } /* end if */

    /* NEED CHECK: should this be a validation */
    assert((size_t)(p - dblock->blk) == HF_MAN_ABS_DIRECT_OVERHEAD(file->shared, hdr));

#ifdef DEBUG
    printf("dblock->size=%u, dblock->blk_off_size=%u\n", dblock->size, dblock->blk_off_size);
    printf("Found Direct block signature\n");
    printf("heap address found in direct block=%llu\n", heap_addr);
    printf("dblock->block_off=%llu\n", dblock->block_off);
#endif

done:
    if (dblock)
    {
        if (ret_value == SUCCEED && ret_dblock)
            *ret_dblock = dblock;
        else
            free_fheap_dblock(dblock);
    }

    return (ret_value);
} /* check_dblock() */

/* Free memory for fractal heap direct block */
static void
free_fheap_dblock(HF_direct_t *dblock)
{
    assert(dblock);

    if (dblock->blk)
        free(dblock->blk);

    free(dblock);
} /* free_fheap_dblock() */

/* Validating fractal heap: indirect block */
static ck_err_t
check_iblock(driver_t *file, ck_addr_t iblock_addr, HF_hdr_t *hdr, unsigned nrows)
{
    HF_indirect_t *iblock = NULL; /* Indirect block info */
    unsigned entry, row, col;
    int ret_value = SUCCEED; /* Return value */
    HF_parent_t par_info;

    /* Check arguments */
    assert(file);
    assert(addr_defined(iblock_addr));

    if (debug_verbose())
        printf("VALIDATING the fractal heap indirect block at %llu...\n", iblock_addr);

    if (check_iblock_real(file, iblock_addr, hdr, nrows, &iblock) < 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F,
                   "Fractal Heap Indirect Block:Error found when checking indirect block", iblock_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    entry = 0;
    for (row = 0; row < iblock->nrows; row++)
    {
        /* Iterate over entries in this row */
        for (col = 0; col < hdr->man_dtable.cparam.width; col++, entry++)
        {
            /* Check for child entry at this position */
            if (addr_defined(iblock->ents[entry].addr))
            {
                ck_hsize_t row_block_size; /* The size of blocks in this row */

                /* Get the row's block size */
                row_block_size = (ck_hsize_t)hdr->man_dtable.row_block_size[row];

                /* Are we in a direct or indirect block row */
                if (row < hdr->man_dtable.max_direct_rows)
                {
                    ck_hsize_t dblock_size; /* Size of direct block on disk */

                    dblock_size = row_block_size;
                    par_info.iblock = iblock;
                    par_info.entry = entry;
                    if (check_dblock(file, iblock->ents[entry].addr, hdr, dblock_size, &par_info, NULL) < 0)
                    {
                        error_push(ERR_LEV_1, ERR_LEV_1F,
                                   "Fractal Heap Indirect Block:Errors found when checking direct block", iblock->ents[entry].addr, NULL);
                        CK_SET_RET_DONE(FAIL)
                    }

                } /* end if */
                else
                {
                    unsigned child_nrows; /* Number of rows in new indirect block */

                    /* Compute # of rows in next child indirect block to use */
                    child_nrows = HF_dtable_size_to_rows(&hdr->man_dtable, row_block_size);
                    if (check_iblock(file, iblock->ents[entry].addr, hdr, child_nrows) < 0)
                    {
                        error_push(ERR_LEV_1, ERR_LEV_1F,
                                   "Fractal Heap Indirect Block:Errors found when checking indirect block (recursive)",
                                   iblock->ents[entry].addr, NULL);
                        CK_SET_RET_DONE(FAIL)
                    }
                } /* end else */
            }     /* end if */
        }         /* end for */
    }             /* end row */

done:
    if (iblock)
        free_fheap_iblock(iblock);

    return (ret_value);
} /* check_iblock() */

/* Validating fractal heap : doubling table */
static ck_err_t
check_dtable(driver_t *file, const uint8_t **pp, HF_dtable_t *dtable, const uint8_t *start_buf, ck_addr_t logi_base)
{
    int ret_value = SUCCEED;
    ck_addr_t logical;

    /* Check arguments */
    assert(file);
    assert(pp && *pp);
    assert(dtable);

    logical = get_logical_addr(*pp, start_buf, logi_base);

    /* Table width */
    UINT16DECODE(*pp, dtable->cparam.width);
    if (dtable->cparam.width == 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Doubling Table:width must be greater than 0", logical, NULL);
        CK_SET_RET(FAIL)
    }
    if (dtable->cparam.width > HF_WIDTH_LIMIT)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Doubling Table:width is too large", logical, NULL);
        CK_SET_RET(FAIL)
    }
    if (!POWER_OF_TWO(dtable->cparam.width))
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Doubling Table:width is not a power of 2", logical, NULL);
        CK_SET_RET(FAIL)
    }

    logical = get_logical_addr(*pp, start_buf, logi_base);

    /* Starting block size */
    DECODE_LENGTH(file->shared, *pp, dtable->cparam.start_block_size);
    if (dtable->cparam.start_block_size == 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Doubling Table:starting block size must be > 0", logical, NULL);
        CK_SET_RET(FAIL)
    }

    if (!POWER_OF_TWO(dtable->cparam.start_block_size))
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Doubling Table:starting block size is not a power of 2", logical, NULL);
        CK_SET_RET(FAIL)
    }

    logical = get_logical_addr(*pp, start_buf, logi_base);

    /* Maximum direct block size */
    DECODE_LENGTH(file->shared, *pp, dtable->cparam.max_direct_size);
    if (dtable->cparam.max_direct_size == 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Doubling Table:max. direct block size must be > 0", logical, NULL);
        CK_SET_RET(FAIL)
    }

    if (dtable->cparam.max_direct_size > HF_MAX_DIRECT_SIZE_LIMIT)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Doubling Table:max. direct block size is too large", logical, NULL);
        CK_SET_RET(FAIL)
    }

    if (!POWER_OF_TWO(dtable->cparam.max_direct_size))
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Doubling Table:max. direct block size is not a power of 2", logical, NULL);
        CK_SET_RET(FAIL)
    }

    logical = get_logical_addr(*pp, start_buf, logi_base);

    /* Maximum heap size (as # of bits) */
    UINT16DECODE(*pp, dtable->cparam.max_index);
    if (dtable->cparam.max_index == 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Doubling Table:max. heap size must be > 0", logical, NULL);
        CK_SET_RET(FAIL)
    }

    /* Starting # of rows in root indirect block */
    UINT16DECODE(*pp, dtable->cparam.start_root_rows);

    /* Address of table */
    addr_decode(file->shared, pp, &(dtable->table_addr));

    /* Current # of rows in root indirect block */
    UINT16DECODE(*pp, dtable->curr_root_rows);

done:
    return (ret_value);
} /* check_dtable() */

static ck_err_t
HF_tiny_init(HF_hdr_t *fhdr)
{

    assert(fhdr);

    /* Compute information about 'tiny' objects for the heap */

    /* Check if tiny objects need an extra byte for their length */
    /* (account for boundary condition when length of an object would need an
     *  extra byte, but using that byte means that the extra length byte is
     *  unneccessary)
     */
    if ((fhdr->id_len - 1) <= HF_TINY_LEN_SHORT)
    {
        fhdr->tiny_max_len = fhdr->id_len - 1;
        fhdr->tiny_len_extended = FALSE;
    }
    else if ((fhdr->id_len - 1) == (HF_TINY_LEN_SHORT + 1))
    {
        fhdr->tiny_max_len = HF_TINY_LEN_SHORT;
        fhdr->tiny_len_extended = FALSE;
    }
    else
    {
        fhdr->tiny_max_len = fhdr->id_len - 2;
        fhdr->tiny_len_extended = TRUE;
    }

    return (SUCCEED);
} /* HF_tiny_init() */

static ck_err_t
HF_huge_init(driver_t *file, HF_hdr_t *hdr)
{
    assert(file);
    assert(hdr);
    /*  
     * Check if we can completely hold the 'huge' object's offset & length in
     * the file in the heap ID (which will speed up accessing it) and we don't
     * have any I/O pipeline filters.
     */
    if (hdr->filter_len > 0)
    {
        if ((hdr->id_len - 1) >=
            (SIZEOF_ADDR(file->shared) + SIZEOF_SIZE(file->shared) + 4 + SIZEOF_SIZE(file->shared)))
        {
            /* Indicate that v2 B-tree doesn't have to be used to locate object */
            hdr->huge_ids_direct = TRUE;

            /* Set the size of 'huge' object IDs */
            hdr->huge_id_size = SIZEOF_ADDR(file->shared) + SIZEOF_SIZE(file->shared) +
                                SIZEOF_SIZE(file->shared);
        }
        else
            /* Indicate that v2 B-tree must be used to access object */
            hdr->huge_ids_direct = FALSE;
    }
    else
    {
        if ((SIZEOF_ADDR(file->shared) + SIZEOF_SIZE(file->shared)) <= (hdr->id_len - 1))
        {
            /* Indicate that v2 B-tree doesn't have to be used to locate object */
            hdr->huge_ids_direct = TRUE;

            /* Set the size of 'huge' object IDs */
            hdr->huge_id_size = SIZEOF_ADDR(file->shared) + SIZEOF_SIZE(file->shared);
        } /* end if */
        else
            /* Indicate that v2 B-tree must be used to locate object */
            hdr->huge_ids_direct = FALSE;
    }

    if (!hdr->huge_ids_direct)
    {
        if ((hdr->id_len - 1) < sizeof(ck_hsize_t))
        {
            hdr->huge_id_size = hdr->id_len - 1;
        }
        else
        {
            hdr->huge_id_size = sizeof(ck_hsize_t);
        }
    }

    return (SUCCEED);
} /* HF_huge_init() */

/* 
 * Validate Fractal Heap Header
 */
static ck_err_t
check_fheap_hdr(driver_t *file, ck_addr_t fhdr_addr, HF_hdr_t **ret_hdr)
{
    HF_hdr_t *hdr = NULL;             /* Fractal heap info */
    uint8_t hdr_buf[HF_HDR_BUF_SIZE]; /* Buffer for header */
    ck_size_t size;                   /* Header size */
    const uint8_t *p;
    uint8_t heap_flags; /* Status flags for heap */
    uint32_t stored_chksum, computed_chksum;
    OBJ_filter_t *pline = NULL;
    uint8_t *start_buf = NULL;
    ck_addr_t logical;
    int ret_value = SUCCEED;

    /* Check arguments */
    assert(file);
    assert(addr_defined(fhdr_addr));

    if (debug_verbose())
        printf("VALIDATING the fractal heap header at %llu...\n", fhdr_addr);

    if (NULL == (hdr = calloc(1, sizeof(HF_hdr_t))))
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC,
                   "Fractal Heap Header:Internal allocation error", fhdr_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* Set the heap header's address */
    hdr->heap_addr = fhdr_addr;

    /* Compute the 'base' size of the fractal heap header on disk */
    size = HF_HEADER_SIZE(file->shared);

    if (FD_read(file, fhdr_addr, size, hdr_buf) == FAIL)
    {
        error_push(ERR_FILE, ERR_NONE_SEC,
                   "Fractal Heap Header:Unable to read in header", fhdr_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    p = hdr_buf;
    start_buf = hdr_buf;
    logical = get_logical_addr(p, start_buf, fhdr_addr);

    /* Magic number */
    if (memcmp(p, HF_HDR_MAGIC, (size_t)HF_SIZEOF_MAGIC))
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Header:Wrong header signature", logical, NULL);
        CK_SET_RET_DONE(FAIL)
    }
    else if (debug_verbose())
        printf("FOUND fractal header signature.\n");

    p += HF_SIZEOF_MAGIC;
    logical = get_logical_addr(p, start_buf, fhdr_addr);

    /* Version */
    if (*p++ != HF_HDR_VERSION)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Header:Wrong header version", logical, NULL);
        CK_SET_RET(FAIL)
    }

    logical = get_logical_addr(p, start_buf, fhdr_addr);
    UINT16DECODE(p, hdr->id_len); /* Heap ID length */
    if (hdr->id_len > HF_MAX_ID_LEN)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F,
                   "Fractal Heap Header:ID length is too large to store tiny object lengths", logical, NULL);
        CK_SET_RET(FAIL)
    }

    logical = get_logical_addr(p, start_buf, fhdr_addr);
    UINT16DECODE(p, hdr->filter_len); /* I/O filters' encoded length */

    /* Heap status flags */
    /* (bit 0: "huge" object IDs have wrapped) */
    /* (bit 1: checksum direct blocks) */
    logical = get_logical_addr(p, start_buf, fhdr_addr);
    heap_flags = *p++;
    if (heap_flags & ~(HF_HDR_FLAGS_HUGE_ID_WRAPPED | HF_HDR_FLAGS_CHECKSUM_DBLOCKS))
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Header:Only bits 0 & 1 should be set in Flags", logical, NULL);
        CK_SET_RET(FAIL)
    }

    hdr->huge_ids_wrapped = heap_flags & HF_HDR_FLAGS_HUGE_ID_WRAPPED;
    hdr->checksum_dblocks = heap_flags & HF_HDR_FLAGS_CHECKSUM_DBLOCKS;

    /* "Huge" object information */
    UINT32DECODE(p, hdr->max_man_size);                 /* Max. size of "managed" objects */
    DECODE_LENGTH(file->shared, p, hdr->huge_next_id);  /* Next ID to use for "huge" object */
    addr_decode(file->shared, &p, &hdr->huge_bt2_addr); /* B-tree Address of "huge" object */

    /* "Managed" object free space information */
    DECODE_LENGTH(file->shared, p, hdr->total_man_free);
    addr_decode(file->shared, &p, &hdr->fs_addr); /* Address of free section header */

    /* Heap statistics */
    DECODE_LENGTH(file->shared, p, hdr->man_size);
    DECODE_LENGTH(file->shared, p, hdr->man_alloc_size);
    DECODE_LENGTH(file->shared, p, hdr->man_iter_off);
    DECODE_LENGTH(file->shared, p, hdr->man_nobjs);
    DECODE_LENGTH(file->shared, p, hdr->huge_size);
    DECODE_LENGTH(file->shared, p, hdr->huge_nobjs);
    DECODE_LENGTH(file->shared, p, hdr->tiny_size);
    DECODE_LENGTH(file->shared, p, hdr->tiny_nobjs);

    /* Managed objects' doubling-table info */
    logical = get_logical_addr(p, start_buf, fhdr_addr);
    if (check_dtable(file, &p, &(hdr->man_dtable), start_buf, fhdr_addr) < 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Headers:Errors found when validating doubling table info", logical, NULL);
        CK_SET_RET(FAIL)
    }

    if (hdr->man_dtable.cparam.max_direct_size < hdr->max_man_size)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F,
                   "Fractal Heap Header:max. direct size is not large enough to hold all managed blocks", logical, NULL);
        CK_SET_RET(FAIL)
    }
    if (hdr->man_dtable.cparam.max_index > (8 * SIZEOF_SIZE(file->shared)))
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Header:max. heap size is too large for file", logical, NULL);
        CK_SET_RET(FAIL)
    }

    /* Sanity check */
    assert((size_t)(p - hdr_buf) == (size - HF_SIZEOF_CHKSUM));

    /* Check for I/O filter information to decode */
    if (hdr->filter_len > 0)
    {
        size_t filter_info_size; /* Size of filter information */
        size_t filter_info_off;  /* Offset in header of filter information */

        filter_info_off = p - hdr_buf;

        filter_info_size = file->shared->size_lengths /* Size of size for filtered root direct block */
                           + 4                        /* Size of filter mask for filtered root direct block */
                           + hdr->filter_len;         /* Size of encoded I/O filter info */

        hdr->heap_size = size + filter_info_size;

        if (FD_read(file, fhdr_addr + filter_info_off, filter_info_size + HF_SIZEOF_CHKSUM, hdr_buf + filter_info_off) == FAIL)
        {
            error_push(ERR_FILE, ERR_NONE_SEC,
                       "Fractal Heap Header:Unable to read filter info", fhdr_addr + size, NULL);
            CK_SET_RET_DONE(FAIL)
        }

        p = hdr_buf + filter_info_off;
        logical = get_logical_addr(p, start_buf, fhdr_addr);

        /* Decode the size of a filtered root direct block */
        DECODE_LENGTH(file->shared, p, hdr->pline_root_direct_size);

#if 0 /* BUG here : couldn't check for 0 because managed heap might be empty */
	if(hdr->pline_root_direct_size <= 0) {
	    error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Header:filtered root direct block size should be > 0", -1, NULL);
	    CK_SET_RET(FAIL)
	}
#endif

        /* Decode the filter mask for a filtered root direct block */
        UINT32DECODE(p, hdr->pline_root_direct_filter_mask);

        logical = get_logical_addr(p, start_buf, fhdr_addr);

        /* Decode I/O filter information */
        pline = message_type_g[OBJ_FILTER_ID]->decode(file, p, hdr_buf, fhdr_addr);
        if (pline == NULL)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Header:Errors found when decoding I/O filter info",
                       logical, NULL);
            CK_SET_RET_DONE(FAIL)
        }

        hdr->pline = pline;
        p += hdr->filter_len;
    } /* end if */
    else
        /* Set the heap header's size */
        hdr->heap_size = size;

    logical = get_logical_addr(p, start_buf, fhdr_addr);
    computed_chksum = checksum_metadata(hdr_buf, (size_t)(p - hdr_buf), 0);

    UINT32DECODE(p, stored_chksum);
    if (computed_chksum != stored_chksum)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Header:Incorrect checksum\n", logical, NULL);
        CK_SET_RET(FAIL)
    }

    if (HF_dtable_init(&hdr->man_dtable) < 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Header:Errors found when initializing doubling table\n",
                   -1, NULL);
        CK_SET_RET(FAIL)
    }

    hdr->heap_off_size = HF_SIZEOF_OFFSET_BITS(hdr->man_dtable.cparam.max_index);

    /* Set the size of heap IDs */
    hdr->heap_len_size = MIN(hdr->man_dtable.max_dir_blk_off_size, ((V_log2_gen((uint64_t)hdr->max_man_size) + 7) / 8));

    HF_tiny_init(hdr);
    HF_huge_init(file, hdr);

    /* check free space manager */
    if (addr_defined(hdr->fs_addr))
    {
        if (check_fshdr(file, hdr->fs_addr, hdr) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F, "Fractal Heap Header:Errors found when validating free space manager\n",
                       -1, NULL);
            CK_SET_RET(FAIL)
        }
    }

#ifdef DEBUG
    printf("hdr->heap_size=%u;hdr->heap_off_size=%u\n", hdr->heap_size, hdr->heap_off_size);
    printf("hdr->man_alloc_size=%llu; hdr->huge_size=%llu\n",
           hdr->man_alloc_size, hdr->huge_size);

    printf("hdr->man_nobjs=%lu, hdr->huge_nobjs=%lu, hdr->tiny_nobjs=%lu\n",
           hdr->man_nobjs, hdr->huge_nobjs, hdr->tiny_nobjs);

    printf("double:cparam.start_root_rows=%u; cparam.max_index=%u, cparam.width=%u\n",
           hdr->man_dtable.cparam.start_root_rows,
           hdr->man_dtable.cparam.max_index, hdr->man_dtable.cparam.width);

    printf("double:curr_root_rows=%u;", hdr->man_dtable.curr_root_rows);
    if (addr_defined(hdr->man_dtable.table_addr))
        printf("table_addr=%llu\n", hdr->man_dtable.table_addr);
    else
        printf("table_addr is undefined\n");
#endif

done:
    if (hdr)
    {
        if (ret_value == SUCCEED && ret_hdr)
            *ret_hdr = hdr;
        else
            free_fheap_hdr(hdr);
    }

    return (ret_value);
} /* check_fheap_hdr() */

/* Free memory for fractal heap header */
static void
free_fheap_hdr(HF_hdr_t *hdr)
{
    assert(hdr);

    if (hdr->man_dtable.row_block_size)
        free(hdr->man_dtable.row_block_size);
    free(hdr);
} /* free_fheap_hdr() */

/*
 * ENTRY to validation of fractal heap 
 */
ck_err_t
check_fheap(driver_t *file, ck_addr_t fheap_addr)
{
    HF_hdr_t *fhdr = NULL;
    HF_parent_t par_info;
    ck_err_t ret_value = SUCCEED; /* return value */
    ck_err_t ret_err = 0;         /* errors from the current routine */
    ck_err_t ret_other_err = 0;   /* track errors from other routines */

    if (debug_verbose())
        printf("VALIDATING the fractal heap at logical address %llu...\n", fheap_addr);

    if (check_fheap_hdr(file, fheap_addr, &fhdr) < 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Errors found when validating Fractal Heap Header", fheap_addr, NULL);
        CK_INC_ERR_DONE
    }

    if (addr_defined(fhdr->man_dtable.table_addr))
    {
        if (fhdr->man_dtable.curr_root_rows == 0)
        { /* table_addr points to root direct block */
            par_info.iblock = NULL;
            par_info.entry = 0;
            if (check_dblock(file, fhdr->man_dtable.table_addr, fhdr, fhdr->man_dtable.cparam.start_block_size, &par_info, NULL) < 0)
            {
                error_push(ERR_LEV_1, ERR_LEV_1F,
                           "Errors found when validating Fractal Heap Direct Block", fhdr->man_dtable.table_addr, NULL);
                CK_INC_ERR_DONE
            }
        }
        else
        { /* table_addr points to root indirect block */
            if (check_iblock(file, fhdr->man_dtable.table_addr, fhdr, fhdr->man_dtable.curr_root_rows) < 0)
            {
                error_push(ERR_LEV_1, ERR_LEV_1F,
                           "Errors found when validating Fractal Heap Indirect Block", fhdr->man_dtable.table_addr, NULL);
                CK_INC_ERR_DONE
            }
        }
    }
    else
    {
        if (debug_verbose())
            printf("Empty managed heap ...\n");
    }

    if (addr_defined(fhdr->huge_bt2_addr))
    {
        if (debug_verbose())
            printf("Going to validate version 2 btree for fractal heap's huge objects at logical address %llu...\n",
                   fhdr->huge_bt2_addr);
        if (fhdr->huge_ids_direct)
        { /* directly accessed */
            if (fhdr->filter_len > 0)
            {
                if (check_btree2(file, fhdr->huge_bt2_addr, HF_BT2_FILT_DIR, NULL, NULL) < 0)
                    ++ret_other_err;
            }
            else
            {
                if (check_btree2(file, fhdr->huge_bt2_addr, HF_BT2_DIR, NULL, NULL) < 0)
                    ++ret_other_err;
            }
        }
        else
        { /* indirectly accessed */
            if (fhdr->filter_len > 0)
            {
                if (check_btree2(file, fhdr->huge_bt2_addr, HF_BT2_FILT_INDIR, NULL, NULL) < 0)
                    ++ret_other_err;
            }
            else
            {
                if (check_btree2(file, fhdr->huge_bt2_addr, HF_BT2_INDIR, NULL, NULL) < 0)
                    ++ret_other_err;
            }
        }
    }

done:
    if (fhdr)
        free_fheap_hdr(fhdr);

    if (ret_err && !object_api())
    {
        error_print(stderr, file);
        error_clear();
    }

    if (ret_err || ret_other_err)
        ret_value = FAIL;

    return (ret_value);
} /* check_fheap() */

/* ENTRY */
ck_err_t
HF_close(HF_hdr_t *fhdr)
{
    assert(fhdr);

    if (fhdr->man_dtable.row_block_size)
        free(fhdr->man_dtable.row_block_size);
    free(fhdr);

} /* HF_close() */

/* ENTRY */
HF_hdr_t *
HF_open(driver_t *file, ck_addr_t fh_addr)
{
    HF_hdr_t *ret_value = NULL;
    HF_hdr_t *fhdr = NULL;

    assert(file);
    assert(addr_defined(fh_addr));

    if (check_fheap_hdr(file, fh_addr, &fhdr) < 0)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "Errors found when validating Fractal Heap Header", -1, NULL);
        CK_SET_RET_DONE(NULL)
    }

    ret_value = fhdr;

done:
    if (ret_value == NULL && fhdr)
        (void)HF_close(fhdr);

    return (ret_value);
} /* HF_open() */

static ck_err_t
HF_huge_get_obj_info(driver_t *file, HF_hdr_t *fhdr, const uint8_t *id, obj_info_t *objinfo)
{
    ck_err_t ret_value = SUCCEED; /* Return value */

    assert(file);
    assert(fhdr);
    assert(addr_defined(fhdr->huge_bt2_addr));
    assert(id);
    assert(objinfo);

    /* Skip over the flag byte */
    id++;

    /* Check if 'huge' object ID encodes address & length directly */
    if (fhdr->huge_ids_direct)
    {
        addr_decode(file->shared, &id, &(objinfo->u.addr));
        DECODE_LENGTH(file->shared, id, objinfo->size);

        /* Retrieve extra information needed for filtered objects */
        if (fhdr->filter_len > 0)
        {
            UINT32DECODE(id, objinfo->mask);
            DECODE_LENGTH(file->shared, id, objinfo->filt_size);
        }
    }
    else
    {
        if (fhdr->filter_len > 0)
        {
            HF_huge_bt2_filt_indir_rec_t search_rec; /* Record for searching for object */
            HF_huge_bt2_filt_indir_rec_t found_rec;  /* Record found from tracking object */

            /* Get ID for looking up 'huge' object in v2 B-tree */
            UINT64DECODE_VAR(id, search_rec.id, fhdr->huge_id_size)

            /* Look up object in v2 B-tree */
            if (B2_find(file, HF_BT2_FILT_INDIR, fhdr->huge_bt2_addr,
                        &search_rec, HF_huge_bt2_filt_indir_found, &found_rec) < 0)
            {
                error_push(ERR_LEV_1, ERR_LEV_1F,
                           "HF_huge_get_obj_info:Cannot find object's info in version 2 B-tree", -1, NULL);
                CK_SET_RET(FAIL)
            }
            objinfo->u.addr = found_rec.addr;
            objinfo->size = (ck_size_t)found_rec.len;
        } /* end if */
        else
        {
            HF_huge_bt2_filt_indir_rec_t search_rec; /* Record for searching for object */
            HF_huge_bt2_filt_indir_rec_t found_rec;  /* Record found from tracking object */

            /* Get ID for looking up 'huge' object in v2 B-tree */
            UINT64DECODE_VAR(id, search_rec.id, fhdr->huge_id_size)

            /* Look up object in v2 B-tree */
            if (B2_find(file, HF_BT2_INDIR, fhdr->huge_bt2_addr,
                        &search_rec, HF_huge_bt2_indir_found, &found_rec) < 0)
            {
                error_push(ERR_LEV_1, ERR_LEV_1F,
                           "HF_huge_get_obj_info:Cannot find object's info in version 2 B-tree", -1, NULL);
                CK_SET_RET(FAIL)
            }

            /* Retrieve the object's length */
            objinfo->u.addr = found_rec.addr;
            objinfo->size = (ck_size_t)found_rec.len;
        } /* end else */
    }     /* end else */

done:
    return (ret_value);
} /* HF_huge_get_obj_info() */

/* ENTRY */
ck_err_t
HF_get_obj_info(driver_t *file, HF_hdr_t *fhdr, const void *_id, obj_info_t *objinfo)
{
    const uint8_t *id = (const uint8_t *)_id; /* Object ID */
    uint8_t id_flags;                         /* Heap ID flag bits */
    ck_err_t ret_value = SUCCEED;             /* Return value */
    ck_size_t enc_obj_size;                   /* Encoded object size */

    assert(file);
    assert(fhdr);
    assert(id);
    assert(objinfo);

    /* Get the ID flags */
    id_flags = *id;

    /* Check for correct heap ID version */
    if ((id_flags & HF_ID_VERS_MASK) != HF_ID_VERS_CURR)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "HF_get_obj_info:Incorrect version for heap ID", -1, NULL);
        CK_SET_RET(FAIL)
    }

    if ((id_flags & HF_ID_TYPE_MASK) == HF_ID_TYPE_MAN)
    {
#ifdef DEBUG
        printf("TYPE_MAN is encountered\n");
#endif
        id++; /* skip over the flag byte */
        UINT64DECODE_VAR(id, objinfo->u.off, fhdr->heap_off_size);
        UINT64DECODE_VAR(id, objinfo->size, fhdr->heap_len_size);
    }
    else if ((id_flags & HF_ID_TYPE_MASK) == HF_ID_TYPE_HUGE)
    {
#ifdef DEBUG
        printf("huge' object's length\n");
#endif
        if (HF_huge_get_obj_info(file, fhdr, id, objinfo) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F, "HF_get_obj_info:Cannot get huge object's info", -1, NULL);
            CK_SET_RET(FAIL)
        }
    }
    else if ((id_flags & HF_ID_TYPE_MASK) == HF_ID_TYPE_TINY)
    {
#ifdef DEBUG
        printf("'tiny' object's length\n");
#endif
        if (!fhdr->tiny_len_extended)
            enc_obj_size = *id & HF_TINY_MASK_SHORT;
        else
            enc_obj_size = *(id + 1) | ((*id & HF_TINY_MASK_EXT_1) << 8);
        objinfo->size = enc_obj_size + 1;
        /* no need to get obj_addr because the data is embedded in the ID */
        /* length is used to allocate message storage for message before passing to HF_read() */
        /* HF_read() will retrieve length (again) and data */
    }
    else
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "HF_get_obj_info:Unsupported type of heap ID", -1, NULL);
        CK_SET_RET(FAIL)
    }

done:
    return (ret_value);
} /* HF_get_obj_info() */

static ck_err_t
HF_huge_read(driver_t *file, HF_hdr_t *fhdr, void *op_data, obj_info_t *objinfo)
{
    void *read_buf = NULL;        /* Pointer to buffer for reading */
    ck_addr_t obj_addr;           /* Object's address in the file */
    ck_size_t obj_size = 0;       /* Object's size in the file */
    unsigned filter_mask = 0;     /* Filter mask for object (only used for filtered objects) */
    ck_err_t ret_value = SUCCEED; /* Return value */

    assert(file);
    assert(fhdr);
    assert(op_data);
    assert(objinfo);
    assert(addr_defined(objinfo->u.addr));
    assert(objinfo->size > 0);

    read_buf = op_data;
    obj_addr = objinfo->u.addr;
    obj_size = objinfo->size;

    /* NEED to handle filtered object: see H5HF_huge_op_real() for details */
    /* read the data from file */
    if (FD_read(file, obj_addr, obj_size, read_buf) == FAIL)
    {
        error_push(ERR_FILE, ERR_NONE_SEC,
                   "HF_huge_read():Unable to read huge object from file", obj_addr, NULL);
        CK_SET_RET_DONE(FAIL)
    }
done:
    return (ret_value);
} /* HF_huge_read() */

/* 
 * Modifications: 
 *	Turn offset assert into a validation check
 */
static ck_err_t
HF_man_read(driver_t *file, HF_hdr_t *fhdr, void *op_data, obj_info_t *objinfo)
{
    HF_direct_t *dblock = NULL;   /* Pointer to direct block to query */
    HF_indirect_t *iblock = NULL; /* Pointer to indirect block */
    HF_parent_t par_info;
    ck_addr_t dblock_addr;        /* Direct block address */
    ck_size_t dblock_size;        /* Direct block size */
    ck_size_t blk_off;            /* Offset of object in block */
    uint8_t *p;                   /* Temporary pointer to obj info in block */
    ck_err_t ret_value = SUCCEED; /* Return value */

    assert(file);
    assert(fhdr);
    assert(op_data);

    assert(objinfo);
    assert(objinfo->u.off > 0);
    assert(objinfo->size > 0);

#ifdef DEBUG
    printf("HF_man_read():obj_off=%llu, obj_len=%u\n", objinfo->u.off, objinfo->size);
#endif

    if (objinfo->u.off > fhdr->man_size)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "HF_man_read:Fractal heap object offset too large", -1, NULL);
        CK_SET_RET(FAIL)
    }
    if (objinfo->size > fhdr->man_dtable.cparam.max_direct_size)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F,
                   "HF_man_read:Fractal heap object size too large for direct block", -1, NULL);
        CK_SET_RET(FAIL)
    }
    if (objinfo->size > fhdr->max_man_size)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F,
                   "HF_man_read:Fractal heap object should be standalone", -1, NULL);
        CK_SET_RET(FAIL)
    }

    /* Check for root direct block */
    if (fhdr->man_dtable.curr_root_rows == 0)
    {
        /* Set direct block info */
        dblock_addr = fhdr->man_dtable.table_addr;
        dblock_size = fhdr->man_dtable.cparam.start_block_size;

        par_info.iblock = NULL;
        par_info.entry = 0;
        if (check_dblock(file, dblock_addr, fhdr, dblock_size, &par_info, &dblock) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F,
                       "HF_man_read:Errors found when checking direct block", -1, NULL);
            CK_SET_RET_DONE(FAIL)
        }
    }
    else
    {
        unsigned entry; /* Entry of block */

        if (HF_man_dblock_locate(file, fhdr, objinfo->u.off, &iblock, &entry) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F,
                       "HF_man_read:Errors found when locating direct block", -1, NULL);
            CK_SET_RET_DONE(FAIL)
        }

        /* Set direct block info */
        dblock_addr = iblock->ents[entry].addr;
        dblock_size = fhdr->man_dtable.row_block_size[entry / fhdr->man_dtable.cparam.width];

        /* Check for offset of invalid direct block */
        if (!addr_defined(dblock_addr))
        {
            error_push(ERR_LEV_1, ERR_LEV_1F, "HF_man_read:Invalid direct block address", -1, NULL);
            CK_SET_RET_DONE(FAIL)
        }

        par_info.iblock = iblock;
        par_info.entry = entry;
        if (check_dblock(file, dblock_addr, fhdr, dblock_size, &par_info, &dblock) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F,
                       "HF_man_read:Errors found when checking direct block", -1, NULL);
            CK_SET_RET_DONE(FAIL)
        }
    }

#ifdef TEMP
    /* Compute offset of object within block */
    assert((objinfo->u.off - dblock->block_off) < (ck_hsize_t)dblock_size);
#endif
    if ((objinfo->u.off - dblock->block_off) >= (ck_hsize_t)dblock_size)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F,
                   "HF_man_read:Object offset is not within direct block size", -1, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    blk_off = (size_t)(objinfo->u.off - dblock->block_off);

    /* Check for object's offset in the direct block prefix information */
    if (blk_off < HF_MAN_ABS_DIRECT_OVERHEAD(file->shared, fhdr))
    {
        error_push(ERR_LEV_1, ERR_LEV_1F,
                   "HF_man_read:Object located in prefix section of direct block", -1, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* Check for object's length overrunning the end of the direct block */
    if ((blk_off + objinfo->size) > dblock_size)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F,
                   "HF_man_read:Object overruns end of direct block", -1, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* Point to location for object */
    p = dblock->blk + blk_off;

    memcpy(op_data, p, objinfo->size);

done:
    if (dblock)
        free_fheap_dblock(dblock);
    if (iblock)
        free_fheap_iblock(iblock);

    return (ret_value);

} /* HF_man_read() */

static ck_err_t
HF_tiny_read(driver_t *file, HF_hdr_t *fhdr, const uint8_t *id, void *op_data)
{
    ck_size_t enc_obj_size;       /* Encoded object size */
    ck_err_t ret_value = SUCCEED; /* Return value */

    assert(file);
    assert(fhdr);
    assert(id);
    assert(op_data);

    /* Check if 'tiny' object ID is in extended form */
    if (!fhdr->tiny_len_extended)
    {
        /* Retrieve the object's encoded length */
        enc_obj_size = *id & HF_TINY_MASK_SHORT;

        /* Advance past flag byte(s) */
        id++;
    } /* end if */
    else
    {
        /* Retrieve the object's encoded length */
        /* (performed in this odd way to avoid compiler bug on tg-login3 with
         *  gcc 3.2.2 - QAK)
         */
        enc_obj_size = *(id + 1) | ((*id & HF_TINY_MASK_EXT_1) << 8);

        /* Advance past flag byte(s) */
        /* (performed in two steps to avoid compiler bug on tg-login3 with
         *  gcc 3.2.2 - QAK)
         */
        id++;
        id++;
    } /* end else */

    memcpy(op_data, id, enc_obj_size + 1);

done:
    return (SUCCEED);
} /* HF_tiny_read() */

/* ENTRY */
ck_err_t
HF_read(driver_t *file, HF_hdr_t *fhdr, const void *_id, void *obj /*out*/, obj_info_t *objinfo)
{
    const uint8_t *id = (const uint8_t *)_id; /* Object ID */
    uint8_t id_flags;                         /* Heap ID flag bits */
    ck_err_t ret_value = SUCCEED;             /* Return value */

    assert(fhdr);
    assert(id);
    assert(obj);
    assert(objinfo);

    /* Get the ID flags */
    id_flags = *id;

    /* Check for correct heap ID version */
    if ((id_flags & HF_ID_VERS_MASK) != HF_ID_VERS_CURR)
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "HF_read:Incorrect version for heap ID", -1, NULL);
        CK_SET_RET(FAIL)
    }

    /* Check type of object in heap */
    if ((id_flags & HF_ID_TYPE_MASK) == HF_ID_TYPE_MAN)
    {
        if (HF_man_read(file, fhdr, obj, objinfo) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F, "HF_read:Cannot read managed object", -1, NULL);
            CK_SET_RET(FAIL)
        }
    }
    else if ((id_flags & HF_ID_TYPE_MASK) == HF_ID_TYPE_HUGE)
    {
        /* Read 'huge' object from file */
        if (HF_huge_read(file, fhdr, obj, objinfo) < 0)
        {
            error_push(ERR_LEV_1, ERR_LEV_1F, "HF_read:Cannot read huge object", -1, NULL);
            CK_SET_RET(FAIL)
        }
    }
    else if ((id_flags & HF_ID_TYPE_MASK) == HF_ID_TYPE_TINY)
    {
        /* Read 'tiny' object from fractal heap ID */
        if (HF_tiny_read(file, fhdr, id, obj) < 0)
            error_push(ERR_LEV_1, ERR_LEV_1F, "HF_read:Cannot read tiny object", -1, NULL);
        CK_SET_RET(FAIL)
    }
    else
    {
        error_push(ERR_LEV_1, ERR_LEV_1F, "HF_read:Unsupported type of heap ID", -1, NULL);
        CK_SET_RET(FAIL)
    }

done:
    return (ret_value);
} /* end HF_read() */

/* 
 * END fractal heap validation
 */

/*
 * ENTRY to validate the Master Table of Shared Object Header Message Indexes
 *
 * Modifications: add warning for validation that is not implemented yet
 */
ck_err_t
check_SOHM(driver_t *file, ck_addr_t sohm_addr, unsigned nindexes)
{
    SM_master_table_t *table = NULL;
    uint8_t tbl_buf[SM_TBL_BUF_SIZE]; /* Buffer for table */
    uint32_t stored_chksum, computed_chksum;
    ck_size_t size, x;
    const uint8_t *p;
    uint8_t *start_buf = NULL;
    ck_err_t ret_value = SUCCEED; /* return value */
    ck_err_t ret_err = 0;         /* errors from the current routine */
    ck_err_t ret_other_err = 0;   /* track errors from other routines */
    ck_addr_t logical;

    assert(sohm_addr != CK_ADDR_UNDEF);
    assert(nindexes > 0);

    if (debug_verbose())
        printf("VALIDATING SOHM table at logical address %llu...\n", sohm_addr);

    if (NULL == (table = calloc(1, sizeof(SM_master_table_t))))
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC,
                   "SOHM: Internal allocation error", sohm_addr, NULL);
        CK_INC_ERR_DONE
    }

    /* num_indexes should be valid by the time it comes here */
    table->num_indexes = nindexes;

    /* 
     * Compute the size of the SOHM table header on disk.  This is the "table" itself
     * plus each index within the table
     */
    size = SM_TABLE_SIZE(file->shared) +
           (table->num_indexes * SM_INDEX_HEADER_SIZE(file->shared));

    /* Read header from disk */
    if (FD_read(file, sohm_addr, size, tbl_buf) == FAIL)
    {
        error_push(ERR_FILE, ERR_NONE_SEC, "SOHM:Unable to read in SOHM table", sohm_addr, NULL);
        CK_INC_ERR_DONE
    }

    /* Get temporary pointer to serialized table */
    p = tbl_buf;
    start_buf = tbl_buf;
    logical = get_logical_addr(p, start_buf, sohm_addr);

    /* Check magic number */
    if (memcmp(p, SM_TABLE_MAGIC, (size_t)SM_SIZEOF_MAGIC))
    {
        error_push(ERR_LEV_2, ERR_LEV_2A2p, "SOHM:Bad SOHM signature", logical, NULL);
        CK_INC_ERR
    }
    p += SM_SIZEOF_MAGIC;

    /* Don't count the checksum in the table size yet, since it comes after
     * all of the index headers
     */
    assert((size_t)(p - tbl_buf) == SM_TABLE_SIZE(f) - SM_SIZEOF_CHECKSUM);

    /* Allocate space for the index headers in memory*/
    if (NULL == (table->indexes = (SM_index_header_t *)malloc(sizeof(SM_index_header_t) * (size_t)table->num_indexes)))
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC,
                   "SOHM:Internal allocation error", sohm_addr, NULL);
        CK_INC_ERR_DONE
    }

    /* Read in the index headers */
    for (x = 0; x < table->num_indexes; ++x)
    {
        ck_op_t ck_msg_op = NULL;

        /* Verify correct version of index list */
        logical = get_logical_addr(p, start_buf, sohm_addr);
        if (SM_LIST_VERSION != *p++)
        {
            error_push(ERR_LEV_2, ERR_LEV_2A2p, "SOHM:Wrong SOHM index version ", logical, NULL);
            CK_INC_ERR
        }

        /* Type of the index (list or B-tree) */
        logical = get_logical_addr(p, start_buf, sohm_addr);
        table->indexes[x].index_type = *p++;
        if ((table->indexes[x].index_type != SM_LIST) && (table->indexes[x].index_type != SM_BTREE))
        {
            error_push(ERR_LEV_2, ERR_LEV_2A2p, "SOHM:Wrong SOHM index type", logical, NULL);
            CK_INC_ERR
        }

        /* Type of messages in the index */
        logical = get_logical_addr(p, start_buf, sohm_addr);
        UINT16DECODE(p, table->indexes[x].mesg_types);
        if (table->indexes[x].mesg_types & ~SHMESG_ALL_FLAG)
        {
            error_push(ERR_LEV_2, ERR_LEV_2A2p, "SOHM:Unknown message type flags", logical, NULL);
            CK_INC_ERR
        }

        /* Minimum size of message to share */
        logical = get_logical_addr(p, start_buf, sohm_addr);
        UINT32DECODE(p, table->indexes[x].min_mesg_size);
        if (table->indexes[x].min_mesg_size < 0)
        {
            error_push(ERR_LEV_2, ERR_LEV_2A2p, "SOHM:Incorrect minimum message size", logical, NULL);
            CK_INC_ERR
        }

        logical = get_logical_addr(p, start_buf, sohm_addr);

        /* List cutoff; fewer than this number and index becomes a list */
        UINT16DECODE(p, table->indexes[x].list_max);

        /* B-tree cutoff; more than this number and index becomes a B-tree */
        UINT16DECODE(p, table->indexes[x].btree_min);

        if (!((table->indexes[x].list_max + 1) >= table->indexes[x].btree_min))
        {
            error_push(ERR_LEV_2, ERR_LEV_2A2p, "SOHM:Incorrect list & btree cutoff", logical, NULL);
            CK_INC_ERR
        }

        logical = get_logical_addr(p, start_buf, sohm_addr);

        /* Number of messages shared */
        UINT16DECODE(p, table->indexes[x].num_messages);

        if ((table->indexes[x].index_type == SM_LIST) &&
            (table->indexes[x].num_messages >= table->indexes[x].list_max))
        {
            error_push(ERR_LEV_2, ERR_LEV_2A2p, "SOHM:Inconsistent type & list cutoff", logical, NULL);
            CK_INC_ERR
        }
        if ((table->indexes[x].index_type == SM_BTREE) &&
            (table->indexes[x].num_messages <= table->indexes[x].btree_min))
        {
            error_push(ERR_LEV_2, ERR_LEV_2A2p, "SOHM:Inconsistent type & btree cutoff", logical, NULL);
            CK_INC_ERR
        }

        logical = get_logical_addr(p, start_buf, sohm_addr);

        /* Address of the actual index */
        addr_decode(file->shared, &p, &(table->indexes[x].index_addr));

        logical = get_logical_addr(p, start_buf, sohm_addr);

        /* Address of the index's heap */
        addr_decode(file->shared, &p, &(table->indexes[x].heap_addr));
        if (addr_defined(table->indexes[x].heap_addr))
        {
            if (check_fheap(file, table->indexes[x].heap_addr) < 0)
                ++ret_other_err;
        }

        if (addr_defined(table->indexes[x].index_addr) && (table->indexes[x].index_type == SM_BTREE))
        {
            if (check_btree2(file, table->indexes[x].index_addr, SM_INDEX, NULL, NULL))
                ++ret_other_err;
        }

        if (addr_defined(table->indexes[x].index_addr) && (table->indexes[x].index_type == SM_LIST))
            if (debug_verbose())
                printf("Warning:validation of shared message record list is not implemented yet\n");

    } /* end for */

    /* Read in checksum */
    logical = get_logical_addr(p, start_buf, sohm_addr);
    UINT32DECODE(p, stored_chksum);

    computed_chksum = checksum_metadata(tbl_buf, (size - SM_SIZEOF_CHECKSUM), 0);
    if (computed_chksum != stored_chksum)
    {
        error_push(ERR_LEV_2, ERR_LEV_2A, "SOHM:Incorrect checksum", logical, NULL);
        CK_INC_ERR
    }

    assert((size_t)(p - tbl_buf) == size);

done:
    if (ret_err && !object_api())
    {
        error_print(stderr, file);
        error_clear();
    }

    if (!ret_err)
        file->shared->sohm_tbl = table;
    else
    { /* Release resources */
        if (table)
        {
            if (table->indexes)
                free(table->indexes);
            free(table);
        }
    }

    if (ret_err || ret_other_err)
        ret_value = FAIL;

    return (ret_value);
} /* check_SOHM() */

/* 
 * Routines to access shared messages in the SOHM heap 
 */
static ck_err_t
SM_type_to_flag(unsigned type_id, unsigned *type_flag)
{
    ck_err_t ret_value = SUCCEED;

    /* Translate the type_id into an SM type flag */
    switch (type_id)
    {
    case OBJ_SDS_ID:
        *type_flag = SHMESG_SDSPACE_FLAG;
        break;

    case OBJ_DT_ID:
        *type_flag = SHMESG_DTYPE_FLAG;
        break;

    case OBJ_FILL_ID:
    case OBJ_FILL_OLD_ID:
        *type_flag = SHMESG_FILL_FLAG;
        break;

    case OBJ_FILTER_ID:
        *type_flag = SHMESG_PLINE_FLAG;
        break;

    case OBJ_ATTR_ID:
        *type_flag = SHMESG_ATTR_FLAG;
        break;

    default:
        CK_SET_RET(FAIL)
    } /* end switch */

done:
    return (ret_value);
} /* SM_type_to_flag() */

static ssize_t
SM_get_index(const SM_master_table_t *table, unsigned type_id)
{
    ssize_t x;
    unsigned type_flag;
    ssize_t ret_value = FAIL;

    assert(table);

    /* Translate the type_id into a SM type flag */
    if (SM_type_to_flag(type_id, &type_flag) < 0)
    {
        error_push(ERR_INTERNAL, ERR_NONE_SEC, "SM_get_index:Cannot map message type to flag", -1, NULL);
        CK_SET_RET_DONE(FAIL)
    }

    /* Search the indexes until we find one that matches this flag or we've
     * searched them all.
     */
    for (x = 0; x < table->num_indexes; ++x)
        if (table->indexes[x].mesg_types & type_flag)
            CK_SET_RET_DONE(x)

    /* At this point, ret_value is either the location of the correct
     * index or it's still FAIL because we didn't find an index.
     */
done:
    return (ret_value);
} /* SM_get_index() */

/*
 * ENTRY: Find the fractal heap address for a shared message of a certain type
 */
ck_err_t
SM_get_fheap_addr(driver_t *f, unsigned type_id, ck_addr_t *fheap_addr)
{
    SM_master_table_t *table = NULL; /* Shared object master table */
    ssize_t index_num;               /* Which index */
    ck_err_t ret_value = SUCCEED;    /* Return value */

    assert(f);
    assert(fheap_addr);

    if ((table = f->shared->sohm_tbl) == NULL)
        CK_SET_RET_DONE(FAIL);

    /* Look up index for message type */
    if ((index_num = SM_get_index(table, type_id)) < 0)
        CK_SET_RET_DONE(FAIL);

    /* Retrieve heap address for index */
    *fheap_addr = table->indexes[index_num].heap_addr;

done:
    return (ret_value);
} /* SM_get_fheap_addr() */
