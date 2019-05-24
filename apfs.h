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

#ifndef _APFS_MODULE_H
#define _APFS_MODULE_H

#include <linux/fs.h>

#include "apfs/types.h"
#include "apfs/container.h"
#include "apfs/btree.h"
#include "apfs/volume.h"

#define NSEC_TO_SEC     1000000000

/*
 * This structure is stored in the private data of the 
 * super_block structure.
 */
struct apfs_glb_info {
        oid_t cnt_oid;
        xid_t cnt_xid;
        
        oid_t vol_oid;
        xid_t vol_xid;

        paddr_t cnt_omap_tree;
        paddr_t vol_omap_tree;
        paddr_t vol_root_tree;
};

/*
 * dir.c
 */
extern struct file_operations apfs_dir_operations;

/*
 * file.c
 */
extern struct file_operations apfs_file_operations;

/*
 * inode.c
 */
extern struct inode_operations apfs_inode_operations;
 
struct inode* get_apfs_inode(struct super_block* sb, 
        struct inode* parent, uint64_t i_no, int inode_type);

/*
 * util.h
 */
int get_fs_obj_id (struct apfs_record_key_t* hdr);

int get_fs_obj_type (struct apfs_record_key_t* hdr);

inline u_int8_t* get_val_zone(struct super_block* sb,
        struct apfs_btree_node_phys_t* node);        

inline u_int8_t* get_toc_zone(struct apfs_btree_node_phys_t* node);

inline u_int8_t* get_key_zone(struct apfs_btree_node_phys_t* node);

u_int64_t get_phys_block(struct super_block* sb, paddr_t omap, 
        u_int64_t oid, u_int64_t xid);

u_int8_t* find_in_node(struct super_block* sb,
        struct apfs_btree_node_phys_t* node, u_int64_t f_val, u_int64_t s_val,
        char* t_val, u_int8_t tree_type);

struct apfs_record_inode_val_t* get_inode_from_disk(struct super_block* sb,
        u_int64_t i_no);

u_int64_t get_inode_size (struct apfs_record_inode_val_t* inode);

struct buffer_head* get_inode_branch(struct super_block* sb,
        u_int64_t i_no);

char* normalize_string(char* unicode_string);

u_int64_t get_fstree_value(struct super_block* sb, 
        struct apfs_btree_node_phys_t* node, struct apfs_kvloc_t* kvloc);

int get_fstree_key(struct apfs_btree_node_phys_t* node, int pos, 
        u_int64_t* oid, u_int64_t* type, char** name);
        
struct buffer_head* get_fstree_child (struct super_block* sb,
        struct apfs_btree_node_phys_t* node, struct apfs_kvloc_t* kvloc);

#endif /* _APFS_MODULE_H */
