#ifndef _APFS_BTREE_H
#define _APFS_BTREE_H

#include "types.h"

/*
 * Flags to be used in apfs_btree_node_phys_t.btn_flags field.
 */
#define APFS_BTNODE_ROOT		0x0001
#define APFS_BTNODE_LEAF		0x0002
#define APFS_BTNODE_FIXED_KV_SIZE	0x0004

/*
 * These structures are used for location intormation within a B-tree node.
 * If APFS_BTNODE_FIXED_KV_SIZE flag is set, read the keys with
 * apfs_kvloc_t struc, otherwise, you must use apfs_kvoff_t.
 */
struct apfs_nloc_t {
	u_int16_t off;
	u_int16_t len;
};

struct apfs_kvloc_t {
	struct apfs_nloc_t k;
	struct apfs_nloc_t v;
};

struct apfs_kvoff_t {
	u_int16_t k;
	u_int16_t v;
};

/*
 * B-Tree node header. 
 * Each node in the B-Tree has this structure at the beginning.
 */
struct apfs_btree_node_phys_t {
	apfs_obj_header_t obj_h;
	u_int16_t btn_flags;
	u_int16_t btn_level;
	u_int32_t btn_nkeys;
	struct apfs_nloc_t btn_table_space;
	struct apfs_nloc_t btn_free_space;
	struct apfs_nloc_t btn_key_free_list;
	struct apfs_nloc_t btn_val_free_list;
	u_int64_t btn_data[];
};

/*
 * Save the information of the B-Tree. This structure is at the end 
 * of the root node.
 * The information in apfs_btree_info_fixed_t, is never changed and
 * is easy to be cache.
 */
struct apfs_btree_info_fixed_t {
	u_int32_t bt_flags;
	u_int32_t bt_node_size;
	u_int32_t bt_key_size;
	u_int32_t bt_val_size;
};

struct apfs_btree_info_t {
	struct apfs_btree_info_fixed_t bt_fixed;
	u_int32_t bt_longest_key;
	u_int32_t bt_longest_val;
	u_int64_t bt_key_count;
	u_int64_t bt_node_count;
};

#endif /* _APFS_BTREE_H */
