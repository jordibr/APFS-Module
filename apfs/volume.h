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
 
#ifndef _APFS_VOLUME_H
#define _APFS_VOLUME_H

#include "types.h"

/*
 * Static values for volumes
 */
#define APFS_VOL_MAGIC          'APSB'
#define APFS_MAX_HIST           8
#define APFS_VOLNAME_LEN        256
#define APFS_MODIFIED_NAMELEN   32

#define ROOT_DIR_INO_NUM        2

/*
 * The types of a file-system records.
 */
#define APFS_TYPE_ANY           0
#define APFS_TYPE_SNAP_METADATA 1
#define APFS_TYPE_EXTENT        2
#define APFS_TYPE_INODE         3
#define APFS_TYPE_XATTR         4
#define APFS_TYPE_SIBLING_LINK  5
#define APFS_TYPE_DSTREAM_ID    6
#define APFS_TYPE_CRYPTO_STATE  7
#define APFS_TYPE_FILE_EXTENT   8
#define APFS_TYPE_DIR_REC       9
#define APFS_TYPE_DIR_STATS     10
#define APFS_TYPE_SNAP_NAME     11

/*
 * Directory entry types
 */
#define APFS_DT_UNKNOWN     0
#define APFS_DT_FIFO        1
#define APFS_DT_CHR         2
#define APFS_DT_DIR         4
#define APFS_DT_BLK         6
#define APFS_DT_REG         8
#define APFS_DT_LNK         10
#define APFS_DT_SOCK        12
#define APFS_DT_WHT         14

/*
 * Mask to use in apfs_record_drec_val_t.flags to
 * compare with the directory entry type constants.
 */
#define APFS_DREC_TYPE_MASK 0x000F

struct apfs_wrapped_meta_crypto_state_t {
    u_int16_t major_version;
    u_int16_t minor_version;
    u_int32_t cpflags;
    u_int32_t persistent_class;
    u_int32_t key_os_version;
    u_int16_t key_revision;
    u_int16_t unused;
}__attribute__((aligned(2), packed));

struct apfs_modified_by_t {
    u_int8_t id[APFS_MODIFIED_NAMELEN];
    u_int64_t timestamp;
    xid_t last_xid;
};

/*
 * A superblock of a volume. With APFS, you can have several volumes
 * within a partition.
 */
struct apfs_vol_superblock_t {
    apfs_obj_header_t obj_h;
    u_int32_t apfs_magic;           /* It's always APFS_VOL_MAGIC */
    u_int32_t apfs_fs_index;
    
    u_int64_t apfs_features;
    u_int64_t apfs_readonly_compatible_features;
    u_int64_t apfs_incompatible_features;
    u_int64_t apfs_unmount_time;
    u_int64_t apfs_fs_reserve_block_count;
    u_int64_t apfs_fs_quota_block_count;
    u_int64_t apfs_fs_alloc_count;
    
    struct apfs_wrapped_meta_crypto_state_t apfs_meta_crypto;
    
    u_int32_t apfs_root_tree_type;      
    u_int32_t apfs_extentref_tree_type;
    u_int32_t apfs_snap_meta_tree_type;

    /* 
     * You have to look for the apfs_root_tree_oid in the apfs_omap_oid B-Tree 
     * to get the physical location of the root directory of this volume.
     */
    oid_t apfs_omap_oid;            
    oid_t apfs_root_tree_oid;
    oid_t apfs_extentref_tree_oid;
    oid_t apfs_snap_meta_tree_oid;

    xid_t apfs_revert_to_xid;
    oid_t apfs_revert_to_sblock_oid;

    u_int64_t apfs_next_obj_id;

    u_int64_t apfs_num_files;
    u_int64_t apfs_num_directories;
    u_int64_t apfs_num_symlinks;
    u_int64_t apfs_num_other_fsobjects;
    u_int64_t apfs_num_snapshots;

    u_int64_t apfs_total_blocks_alloced;
    u_int64_t apfs_total_blocks_freed;
    apfs_uuid_t apfs_vol_uuid;
    u_int64_t apfs_last_mod_time;
    u_int64_t apfs_fs_flags;
    struct apfs_modified_by_t apfs_formatted_by;
    struct apfs_modified_by_t apfs_modified_by[APFS_MAX_HIST];
    u_int8_t apfs_volname[APFS_VOLNAME_LEN];
    u_int32_t apfs_next_doc_id;
    u_int16_t apfs_role;
    u_int16_t reserved; 
    xid_t apfs_root_to_xid;
    oid_t apfs_er_state_oid;
};

/*
 * A header used at the beginning of all keys in a file-system
 * B-Tree.
 * Use these masks with apfs_record_key_t.obj_id_and_type to get
 * the object-id or the record type.
 */
#define APFS_OBJ_ID_MASK    0x0fffffffffffffffULL   
#define APFS_OBJ_TYPE_MASK  0xf000000000000000ULL
#define APFS_OBJ_TYPE_SHIFT 60

struct apfs_record_key_t {
    u_int64_t  obj_id_and_type;
} __attribute__((packed));

/*
 * Each type has its own key/value structure.
 * However, some types use the apfs_record_key_t directly and don't
 * need their own structure.
 */

/* APFS_TYPE_DIR_REC */
struct apfs_record_drec_key_t {
    struct apfs_record_key_t hdr;
    u_int16_t name_len;
    u_int8_t name[0];
} __attribute__((packed));

struct apfs_record_drec_val_t {
    u_int64_t file_id;
    u_int64_t date_added;
    u_int16_t flags;
    u_int8_t  xfields[];
} __attribute__((packed));


/* APFS_TYPE_INODE */
struct apfs_record_inode_val_t {
    u_int64_t parent_id;
    u_int64_t private_id;
    
    u_int64_t create_time;
    u_int64_t mod_time;
    u_int64_t change_time;
    u_int64_t access_time;
    
    u_int64_t internal_flags;
    
    union {
        int32_t nchildren;
        int32_t nlink;
    };  
    
    u_int32_t default_protection_class;
    u_int32_t write_generation_counter;
    u_int32_t bsd_flags;
    u_int32_t owner;
    u_int32_t group;
    u_int16_t mode;
    u_int16_t pad1;
    u_int64_t pad2;
    u_int8_t xfields[];
} __attribute__((packed));

/* APFS_TYPE_FILE_EXTENT */
#define APFS_RECORD_FILE_EXTENT_LEN_MASK        0x00ffffffffffffffULL
#define APFS_RECORD_FILE_EXTENT_FLAG_MASK       0xff00000000000000ULL
#define APFS_RECORD_FILE_EXTENT_FLAG_SHIFT      56

struct apfs_record_file_extent_val_t {
    u_int64_t len_and_flags;
    u_int64_t phys_block_num;
    u_int64_t crypto_id;
} __attribute__((packed));

struct apfs_record_file_extent_key_t {
    struct apfs_record_key_t hdr;
    u_int64_t logical_addr;
} __attribute__((packed));

/*
 * Extended Fields
 */
#define APFS_INO_EXT_TYPE_DSTREAM       8

struct apfs_x_field_t {
        u_int8_t x_type;
        u_int8_t x_flags;
        u_int16_t x_size;
};

struct apfs_xf_blob_t {
        u_int16_t xf_num_exts;
        u_int16_t xf_used_data;
        struct apfs_x_field_t xf_data[];
};

struct apfs_dstream_t {
        u_int64_t size;
        u_int64_t alloced_size;
        u_int64_t default_crypto_id;
        u_int64_t total_bytes_written;
        u_int64_t total_bytes_read;
} __attribute__((aligned(8),packed));

#endif /* _APFS_VOLUME_H */
