#include <stdio.h>

#define MAX_OBJ 500

enum TYPE
{
    DATASET = 1,
    GROUP = 2
};

typedef struct range
{
    ck_addr_t start;
    ck_addr_t end;
} range_t;

typedef struct logger_obj
{
    /* type and name of the object */
    enum TYPE type;
    char name[100];  

    /* base address of object */
    ck_addr_t base_addr; 

    /* object header [for non-continuation block] */
    range_t obj_header; 

    /* local heap */ 
    range_t local_heap;

    /* stores name of sub-objects */
    range_t data_segment; 

    /* list of b-tree nodes */
    range_t btree_nodes[MAX_OBJ]; 
    int num_btree_nodes;

    /* sym_node and data chunks are pointed out of leaf b-tree nodes */
    range_t sym_nodes[MAX_OBJ]; 
    int num_sym_nodes;
    /* list of data chunks */
    range_t data_chunks[MAX_OBJ]; 
    int num_data_chunks;

    /* list of subgroups or datasets */
    struct logger_obj *subgroups[50]; 
    int num_subgrps;

    /* parent group, if root then NULL */
    struct logger_obj *parent_grp; 

} logger_obj_t;

/* logger context */
typedef struct logger_ctx
{
    /* root group */
    logger_obj_t *root_grp; 

    /* output file */
    FILE *file;             

    /* address of superblock */
    range_t superblock;     

    /* address of global_heap */
    range_t global_heap;     

    /* context */
    logger_obj_t *current_obj;
    logger_obj_t *prev_obj;

} logger_ctx_t;

/* command line option */
int is_logging;

void logger_dump();

logger_obj_t *logger_new_obj(char* name);

void print_range(range_t range);

int logger_set_current_obj(logger_obj_t *curr);

// OBSOLETE: revert to the previous object
int logger_switch_back_obj();

int logger_add_subgroup(logger_obj_t *parent, logger_obj_t *child);

int logger_add_sym_node(logger_obj_t *obj, ck_addr_t start, ck_addr_t end);

int logger_add_btree_node(logger_obj_t *obj, ck_addr_t start, ck_addr_t end);

int logger_add_raw_data_chunk(logger_obj_t *obj, ck_addr_t start, ck_addr_t end);

int logger_set_global_heap(ck_addr_t start, ck_addr_t end);

int logger_set_superblock(ck_addr_t start, ck_addr_t end);