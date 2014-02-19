#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif


#include "glusterfs.h"
#include "xlator.h"
#include "dht-common.h"
#include "byte-order.h"
    
int
dht_hash_compute (xlator_t *this, int type, const char *name, uint32_t *hash_p)
{
    return 0;
}

int
dht_inode_ctx_layout_get (inode_t *inode, xlator_t *this, dht_layout_t **layout)
{
    return 0;
}

int
dht_inode_ctx_layout_set (inode_t *inode, xlator_t *this,
                          dht_layout_t *layout_int)
{
    return 0;
}

int
dict_get_ptr (dict_t *this, char *key, void **ptr)
{
    return 0;
}

int
dict_get_ptr_and_len (dict_t *this, char *key, void **ptr, int *len)
{
    return 0;
}
int _gf_log (const char *domain, const char *file,
             const char *function, int32_t line, gf_loglevel_t level,
             const char *fmt, ...) 
{ 
    return 0; 
}
int _gf_log_callingfn (const char *domain, const char *file,
                       const char *function, int32_t line, gf_loglevel_t level,
                       const char *fmt, ...)
{ 
    return 0; 
}
