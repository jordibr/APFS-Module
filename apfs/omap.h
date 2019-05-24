/* 
 * This file is part of the APFS-Module.
 * Copyright (c) 2019 Jordi Barcons.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _APFS_OMAP_H
#define _APFS_OMAP_H

#include "types.h"
#include "btree.h"

/*
 * This structure represent an object map.
 * The most important field of this structure is the om_tree_oid. This field, 
 * point to a B-Tree. In this B-Tree, we can translate an logical object-id
 * to a physical object-id (location in the partition).
 */
struct apfs_omap_phys_t {
    apfs_obj_header_t obj_h;
    u_int32_t om_flags;
    u_int32_t om_snap_count;
    u_int32_t om_tree_type;
    u_int32_t om_snapshot_tree_type;
    oid_t om_tree_oid;
    oid_t om_snapshot_tree_oid;
    xid_t om_most_recent_snap;
    xid_t om_pending_revert_min;
    xid_t om_pending_revert_max;
};

/*
 * When you read a keys in the omap B-Tree, you need to use this structure.
 */
struct apfs_omap_key_t {
    oid_t ok_oid;
    xid_t ok_xid;
};

/*
 * A value in the object map B-Tree (paddr_t is the location of the object
 * in the partition).
 */
struct apfs_omap_val_t {
    u_int32_t ov_flags;
    u_int32_t ov_size;
    paddr_t ov_paddr;
};

#endif /* _APFS_OMAP_H */
