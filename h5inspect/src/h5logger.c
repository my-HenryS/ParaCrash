#include "h5_check.h"
#include "h5_error.h"
#include "h5_logger.h"

logger_ctx_t logger;

const char *type2s(enum TYPE type);
char *range2s(range_t range);
char *range_arr2s(range_t ranges[], int size);
static void logger_obj_dump(logger_obj_t *obj, char *basename);

int logger_init(FILE *file)
{
    logger.file = file;
}

void logger_dump()
{
    fprintf(logger.file, "{\"SUPERBLOCK\": %s\n", range2s(logger.superblock));
    if(logger.global_heap.start)
        fprintf(logger.file, ",\"GLOBAL_HEAP\": %s\n", range2s(logger.global_heap));
    logger_obj_dump(logger.root_grp, "");
    fprintf(logger.file, "}\n");
    fclose(logger.file);
}

static void logger_obj_dump(logger_obj_t *obj, char *basename)
{
    char new_basename[100];
    sprintf(new_basename, "%s%s/", basename, obj->name);
    fprintf(logger.file, ",\"%s %s\":{\n", type2s(obj->type), new_basename);
    fprintf(logger.file, "\t\"BASE\": %lld\n", obj->base_addr);
    fprintf(logger.file, "\t,\"OBJ_HEADER\": %s\n", range2s(obj->obj_header));
    fprintf(logger.file, "\t,\"BTREE_NODES\": %s\n", range_arr2s(obj->btree_nodes, obj->num_btree_nodes));

    if (obj->type == GROUP)
    {
        if (obj->num_subgrps > 0)
            fprintf(logger.file, "\t,\"SYMBOL_TABLE\": %s\n", range_arr2s(obj->sym_nodes, obj->num_sym_nodes));
        fprintf(logger.file, "\t,\"LOCAL_HEAP\": %s\n", range2s(obj->local_heap));
        fprintf(logger.file, "\t,\"DATA_SEGMENT\": %s\n", range2s(obj->data_segment));
        fprintf(logger.file, "}\n");

        int i = 0;
        for (; i < obj->num_subgrps; i++)
        {
            logger_obj_dump(obj->subgroups[i], new_basename);
        }
    }

    else if (obj->type == DATASET)
    {
        fprintf(logger.file, "\t,\"DATA_CHUNKS\": %s\n", range_arr2s(obj->data_chunks, obj->num_data_chunks));
        fprintf(logger.file, "}\n");
    }
}

logger_obj_t *logger_new_obj(char *name)
{
    logger_obj_t *new_obj = malloc(sizeof(logger_obj_t));
    memset(new_obj, 0, sizeof(logger_obj_t));
    new_obj->num_subgrps = 0;
    new_obj->num_sym_nodes = 0;
    new_obj->num_data_chunks = 0;
    new_obj->num_btree_nodes = 0;
    new_obj->parent_grp = NULL;
    new_obj->type = DATASET;
    strcpy(new_obj->name, name);
    return new_obj;
}

int logger_set_current_obj(logger_obj_t *curr)
{
    logger.prev_obj = logger.current_obj;
    logger.current_obj = curr;
    //printf("Current object: %s\n", logger.current_obj->name);
}
// revert to the previous object
// OBSOLETE
int logger_switch_back_obj()
{
    if (logger.prev_obj)
    {
        logger.current_obj = logger.prev_obj;
        //printf("Current object: %s\n", logger.current_obj->name);
    }
    else
    {
        return 0;
    }
}

int logger_add_subgroup(logger_obj_t *parent, logger_obj_t *child)
{
    //printf("%s:%s\n", child->name, parent->name);
    child->parent_grp = parent;
    parent->subgroups[parent->num_subgrps++] = child;
}

int logger_add_sym_node(logger_obj_t *obj, ck_addr_t start, ck_addr_t end)
{
    obj->sym_nodes[obj->num_sym_nodes++] = (range_t){start, end};
}

int logger_add_btree_node(logger_obj_t *obj, ck_addr_t start, ck_addr_t end)
{
    obj->btree_nodes[obj->num_btree_nodes++] = (range_t){start, end};
}

int logger_add_raw_data_chunk(logger_obj_t *obj, ck_addr_t start, ck_addr_t end)
{
    obj->data_chunks[obj->num_data_chunks++] = (range_t){start, end};
}

int logger_set_global_heap(ck_addr_t start, ck_addr_t end)
{
    logger.global_heap = (range_t){start, end};
}

int logger_set_superblock(ck_addr_t start, ck_addr_t end)
{
    logger.superblock = (range_t){start, end};
}

void print_range(range_t range)
{
    printf("%lld %lld\n", range.start, range.end);
}

char *range2s(range_t range)
{
    char *range_str = malloc(sizeof(char) * 30);
    sprintf(range_str, "[%lld, %lld]", range.start, range.end);
    return range_str;
}

char *range_arr2s(range_t ranges[], int size)
{
    char *arr_str = malloc(sizeof(char) * 30 * size);
    arr_str[0] = '[';
    int ptr = 1;
    int i = 0;
    for (; i < size; i++)
    {
        int bytes = snprintf(arr_str + ptr, 30, ", [%lld, %lld]" + (i ? 0 : 2), ranges[i].start, ranges[i].end);
        ptr += bytes;
    }
    arr_str[ptr++] = ']';
    arr_str[ptr++] = '\0';
    return arr_str;
}

const char *type2s(enum TYPE type)
{
    if (type == GROUP)
        return "_GROUP";
    else if (type == DATASET)
        return "_DATASET";
}