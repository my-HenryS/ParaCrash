#define _GNU_SOURCE
#include <sys/time.h>   // for gettimeofday()
#include <stdarg.h>     // for va_list, va_start and va_end
#include "recorder.h"


/*
 * External global values defined in recorder.h
 */
bool __recording;
FilenameHashTable* __filename_hashtable;


/*
 * Some of functions are not made by the application
 * And they are operating on wierd files
 * We should not include them in the trace file
 *
 * return 1 if we should ignore the file
 */
inline int exclude_filename(const char *filename) {
    if (filename == NULL) return 0; // pass

    /* these are paths that we will not trace */
    // TODO put these in configuration file?
    static const char *exclusions[] = {"/dev/", "/proc", "/sys", "/etc", "/usr/tce/packages",
                        "pipe:[", "anon_inode:[", "socket:[", NULL};
    int i = 0;
    // Need to make sure both parameters for strncmp are not NULL, otherwise its gonna crash
    while(exclusions[i] != NULL) {
        int find = strncmp(exclusions[i], filename, strlen(exclusions[i]));
        if (find == 0)      // find it. should ignore this filename
            return 1;
        i++;
    }
    return 0;
}

inline long get_file_size(const char *filename) {
    struct stat sb;
    int res = stat(filename, &sb);          // careful here, make sure stat() is not intercepted
    if (res != 0 ) return 0;               // file not exist or some other error

    int is_regular_file = S_ISREG(sb.st_mode);
    if (!is_regular_file) return 0;        // is directory
    return sb.st_size;
}

inline double recorder_wtime(void) {
  struct timeval time;
  gettimeofday(&time, NULL);
  return (time.tv_sec + ((double)time.tv_usec / 1000000));
}

/* Integer to stirng */
inline char* itoa(long long val) {
    char *str = malloc(sizeof(char) * 16);
    sprintf(str, "%lld", val);
    return str;
}

/* Integer to stirng */
inline char* ftoa(double val) {
    char *str = malloc(sizeof(char) * 24);
    sprintf(str, "%f", val);
    return str;
}

/* Pointer to string */
inline char* ptoa(const void *ptr) {
    char *str = malloc(sizeof(char) * 16);
    sprintf(str, "%p", ptr);
    return str;
}

/* Array Pointer to string */
inline char* hsize_t_arr_p_2s(const hsize_t *arr, int size) {
    if(arr == NULL)
        return "NULL";
    hsize_t *myarr = (hsize_t *)arr;
    char *str = malloc(sizeof(char) * 5 * size);
    int index = 0;
    index += sprintf(&str[index], "%d", size);
    for(int i = 0; i < size; i++){
        //printf("%lld\n",myarr[i]);
        index += sprintf(&str[index], ",%lld", myarr[i]);
    }
    //sprintf(&str[index], "\0");
    return str;
}

/* Array Pointer to string */
inline char* multi_dimen_arr_p_2s(const hsize_t *arr, int ndims, const void * buf) {
    if(arr == NULL)
        return "NULL";
    int elem = ((int *)buf)[0];
    char *dim_str = hsize_t_arr_p_2s(arr, ndims);
    char *str = malloc(sizeof(char) * (strlen(dim_str)+20));
    strcpy(str, dim_str);
    int index = strlen(str);
    //index += sprintf(&str[index], ":");
    index += sprintf(&str[index], ",%d", elem);
    //sprintf(&str[index], "\0");
    return str;
}


//function to convert ascii char[] to hex-string (char[])
void string2hexString(char* input, char* output, int length)
{
    int loop;
    int i;
    unsigned char     hex_str[]= "0123456789abcdef";

    i=0;
    loop=0;

    output[i++] = '0';
    output[i++] = 'x';

    for(loop = 0; loop < length; loop++)
    {
        sprintf((char*)(output+i),"%02X", (unsigned char)input[loop]);
        //printf("%c\n", input[loop]);
        //printf("%s\n", output+i);
        //printf("%d %d\n", loop, length);
        //output[i++] = hex_str[(input[loop] >> 4) & 0x0F];
        //output[i++] = hex_str[(input[loop]) & 0x0F];
        i += 2;
    }
    //insert NULL at the end of the output string
    output[i++] = '\0';
}

inline char* ptos(const void *ptr, int length) {
    //char *str = malloc(sizeof(char) * 2* length);
    //sprintf(str, "%X", (char*)ptr);
    char *str = (char *)ptr;
    char *output = malloc(sizeof(char) * length * 2 + 3);
#ifdef OBYTES    
    string2hexString(str, output, length);
#endif
    return output;
}

/* Put many arguments (char *) in a list (char**) */
inline char** assemble_args_list(int arg_count, ...) {
    char** args = malloc(sizeof(char*) * arg_count);
    int i;
    va_list valist;
    va_start(valist, arg_count);
    for(i = 0; i < arg_count; i++)
        args[i] = va_arg(valist, char*);
    va_end(valist);
    return args;
}

/*
 * Convert between function name (char*) and Id (unsigned char)
 * func_list is a fixed string list defined in recorder-log-format.h
 */
inline const char* get_function_name_by_id(int id) {
    if (id < 0 || id > 255) return "WRONG_FUNCTION_ID";
    return func_list[id];
}

unsigned char get_function_id_by_name(const char* name) {
    size_t len = sizeof(func_list) / sizeof(char *);
    unsigned char i;
    for(i = 0; i < len; i++) {
        if (strcmp(func_list[i], name) == 0)
            return i;
    }
    printf("[Recorder ERROR] Missing function %s\n", name);
    return 255;
}

/*
 * My implementation to replace realpath() system call
 * return the filename id from the hashmap
 */
inline char* realrealpath(const char *path) {
    if(!__recording) return strdup(path);

    FilenameHashTable *entry = malloc(sizeof(FilenameHashTable));

    realpath(path, entry->name);      // we do not intercept realpath()
    if (entry->name == NULL)          // realpath() could return NULL on error
        strcpy(entry->name, path);

    FilenameHashTable *found;
    HASH_FIND_STR(__filename_hashtable, entry->name, found);

    // return duplicated name, because we need to free record.args later
    if(found) {
        free(entry);
        return strdup(found->name);
    } else {
        // insert to hashtable
        HASH_ADD_STR(__filename_hashtable, name, entry);
        return strdup(entry->name);
    }
}

