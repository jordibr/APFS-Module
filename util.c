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

#include <linux/buffer_head.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "apfs.h"
#include "apfs/btree.h"
#include "apfs/volume.h"
#include "apfs/omap.h"

#define CMP_NODE_NONLEAF    0
#define CMP_NODE_LEAF       1

/*
 * Returns the id of a file-system object. 
 */
inline int get_fs_obj_id (struct apfs_record_key_t* hdr)
{
        return le64_to_cpu(hdr->obj_id_and_type) & APFS_OBJ_ID_MASK;
}

/*
 * Returns the type of a file-system object. 
 */
inline int get_fs_obj_type (struct apfs_record_key_t* hdr)
{
        return (le64_to_cpu(hdr->obj_id_and_type) & APFS_OBJ_TYPE_MASK) 
            >> APFS_OBJ_TYPE_SHIFT;
}

/*
 * Returns a pointer to the value zone of the indicated node.
 */
inline u_int8_t* get_val_zone(struct super_block* sb, 
                struct apfs_btree_node_phys_t* node)
{
        u_int8_t end_off;
        
        end_off = 0;
        if (le16_to_cpu(node->btn_flags) & APFS_BTNODE_ROOT) {
                end_off = sizeof(struct apfs_btree_info_t);
        }
        
        return ((u_int8_t*)node) + sb->s_blocksize - end_off;
} 

/*
 * Returns a pointer to the TOC zone of the indicated node.
 */
inline u_int8_t* get_toc_zone(struct apfs_btree_node_phys_t* node)
{
        return node->btn_data + le16_to_cpu(node->btn_table_space.off);
} 

/*
 * Returns a pointer to the key zone of the indicated node.
 */
inline u_int8_t* get_key_zone(struct apfs_btree_node_phys_t* node)
{
        return get_toc_zone(node) + le16_to_cpu(node->btn_table_space.len);
} 

/*
 * Used to compare two objects in an omap.
 */
int cmp_omap_toc_keys(u_int64_t oid, u_int64_t xid, 
        u_int64_t oid_c, u_int64_t xid_c, u_int8_t node_type)
{
    if (oid == oid_c && xid == xid_c)
        return 0;
        
    if ((oid == oid_c && xid > xid_c)
            || (oid >= oid_c && node_type == CMP_NODE_NONLEAF))
        return 2;
    
    if (oid > oid_c)
        return 1;
    
    return -1;
}

/*
 * Fill oid and xid parameters with the information of the key.
 */
int get_omap_key(struct apfs_btree_node_phys_t* node, int pos, 
                        u_int64_t* oid, u_int64_t* xid)
{
    u_int8_t* toc_zone;
    u_int8_t* key_zone;
    struct apfs_kvoff_t* kvoff;
    struct apfs_omap_key_t* k_val;
    
    if (le32_to_cpu(node->btn_nkeys) <= pos) {
        return 0;
    }
    
    toc_zone = get_toc_zone(node);
    key_zone = get_key_zone(node);
        
    kvoff = (struct apfs_kvoff_t*)(toc_zone + pos * sizeof(*kvoff));
    k_val = (struct apfs_omap_key_t*)(key_zone + le16_to_cpu(kvoff->k));
        
    *oid = le64_to_cpu(k_val->ok_oid);
    *xid = le64_to_cpu(k_val->ok_xid);
    
    return 1;
}

/*
 * Return the value (i.e. the block number) of omap.
 */
u_int64_t get_omap_value(struct super_block* sb,
        struct apfs_btree_node_phys_t* node, struct apfs_kvoff_t* toc)
{
    u_int8_t* ptr;
    oid_t* oid;

    if (le16_to_cpu(node->btn_level) != 0) {
        oid = (oid_t*)(get_val_zone(sb, node) - le16_to_cpu(toc->v));
        return le64_to_cpu(*oid);
    } else {
        ptr = get_val_zone(sb, node) - le16_to_cpu(toc->v);
        return le64_to_cpu(((struct apfs_omap_val_t*)ptr)->ov_paddr);
    }
}

/*
 * Used to compare two objects in a file system tree.
 */
int cmp_fstree_toc_keys(u_int64_t oid, u_int64_t otype, char* name,
        u_int64_t oid_c, u_int64_t otype_c, char* name_c,
        u_int8_t type)
{
    u_int8_t strc;
    
    if (name == NULL || name_c == NULL) {
        strc = 0;
    } else {
        strc = strcmp(name, name_c);
    }
    
    if (oid == oid_c && otype == otype_c && strc==0)
        return 0;
    
    if ((oid > oid_c 
                || (oid == oid_c && otype > otype_c) 
                || (oid == oid_c && otype == otype_c && strc >= 0)) 
            && type == CMP_NODE_NONLEAF)
        return 2;
    
    if ((oid > oid_c)
            || (oid == oid_c && otype > otype_c) 
            || (oid == oid_c && otype == otype_c && strc > 0))
        return 1;
    
    return -1;
}

/*
 * Fill oid, type and name parameters with the information of the key.
 */
int get_fstree_key(struct apfs_btree_node_phys_t* node, int pos, 
                        u_int64_t* oid, u_int64_t* type, char** name)
{
    struct apfs_kvloc_t* kvloc;
    struct apfs_record_key_t* k_val;
    struct apfs_record_drec_key_t* drec;
    u_int8_t* toc_zone;
    u_int8_t* key_zone;
    
    if (le32_to_cpu(node->btn_nkeys) <= pos) {
        return 0;
    }

    toc_zone = get_toc_zone(node);
    key_zone = get_key_zone(node);
    
    kvloc = (struct apfs_kvloc_t*) (toc_zone + pos * sizeof(*kvloc));
    k_val = (struct apfs_record_key_t*) (key_zone + le16_to_cpu(kvloc->k.off));
        
    *oid = get_fs_obj_id(k_val);
    *type = get_fs_obj_type(k_val);
    
    if (*type == APFS_TYPE_DIR_REC && name != NULL) {
            drec = (struct apfs_record_drec_key_t*) k_val;
            *name = (char*) drec->name;
    } else {
            *name = NULL;
    }
    
    return 1;
}

/*
 * Return the value (i.e. the block number) of file-system tree. It's valid 
 * only in non-leaf nodes.
 */
u_int64_t get_fstree_value(struct super_block* sb, 
        struct apfs_btree_node_phys_t* node, struct apfs_kvloc_t* kvloc)
{
    u_int64_t* val;
    val = (u_int64_t*)(get_val_zone(sb, node) - le16_to_cpu(kvloc->v.off));
    return le64_to_cpu(*val);
}

/*
 * Performs a binary search in the B-Tree node.
 * It's return the kvloc/kvoff offset.
 */
u_int8_t* find_in_node(struct super_block* sb,
        struct apfs_btree_node_phys_t* node, u_int64_t f_val,
        u_int64_t s_val, char* t_val, u_int8_t tree_type)
{
    int mid, left, right;
    int cmp, aux, found;
    u_int64_t f_val_tree;
    u_int64_t s_val_tree;
    char* t_val_tree = NULL;
    u_int8_t* toc;
    u_int8_t node_type;
    
    if (le16_to_cpu(node->btn_level) != 0)
        node_type = CMP_NODE_NONLEAF;
    else
        node_type = CMP_NODE_LEAF;
    
    left = 0;
    right = le32_to_cpu(node->btn_nkeys) - 1;
    found = 0;
        
    while (left <= right && found != 1)
    {
        mid = (right + left) / 2;

        /*
         * Calculate the key offset and get key value
         */
        if (tree_type == APFS_OBJ_TYPE_OMAP) {
            get_omap_key(node, mid, &f_val_tree, &s_val_tree);
            cmp = cmp_omap_toc_keys(f_val, s_val, f_val_tree, s_val_tree, 
                    node_type);
        } else {
            get_fstree_key(node, mid, &f_val_tree, &s_val_tree, &t_val_tree);
            cmp = cmp_fstree_toc_keys(f_val, s_val, t_val, f_val_tree, 
                    s_val_tree, t_val_tree, node_type);
        }
        
        if (cmp == 0) {
            found = 1;
        } else if (cmp == 1) {
            left = mid + 1;
        } else if (cmp == 2) {
            if (tree_type == APFS_OBJ_TYPE_OMAP) {
                aux = get_omap_key(node, mid + 1, &f_val_tree, &s_val_tree);
            } else {
                aux = get_fstree_key(node, mid + 1, &f_val_tree, &s_val_tree, 
                        &t_val_tree);
            }
            if (!aux) {
                found = 1;
            } else {
                if (tree_type == APFS_OBJ_TYPE_OMAP) {
                    cmp = cmp_omap_toc_keys(f_val, s_val, f_val_tree, 
                            s_val_tree, node_type);
                } else {
                    cmp = cmp_fstree_toc_keys(f_val, s_val, t_val, f_val_tree,
                            s_val_tree, t_val_tree, node_type);
                }
                if (cmp < 0)
                    found = 1;
                else
                    left = mid + 1;
            }
        } else {
            right = mid - 1;
        }
    }
     
    /*
     * Check if we have found the value. Next, will return the offset.
     */   
    if (found) {
        toc = get_toc_zone(node);                   
        if (tree_type == APFS_OBJ_TYPE_OMAP)
            toc += mid * sizeof(struct apfs_kvoff_t);
        else
            toc += mid * sizeof(struct apfs_kvloc_t);
        return toc;
    } else {
        return NULL;
    }
}

/*
 * Return a pyshical block of the specific object.
 */
u_int64_t get_phys_block(struct super_block* sb, paddr_t omap,
        u_int64_t oid, u_int64_t xid)
{
    struct buffer_head *bh;
    struct apfs_btree_node_phys_t* omap_nde;
    struct apfs_kvoff_t* kvoff;
    u_int64_t block_n;
    
    bh = sb_bread(sb, omap);
    if (!bh) {
        printk(KERN_ERR "apfs: unable to read block [%llu]\n",
                omap);
        return 0;
    }
    omap_nde = (struct apfs_btree_node_phys_t*) bh->b_data;
        
    while (le16_to_cpu(omap_nde->btn_level) >= 0)
    {
        kvoff = (struct apfs_kvoff_t*) find_in_node(sb, omap_nde, oid, xid, 
                NULL, APFS_OBJ_TYPE_OMAP);
        brelse(bh);
        
        if (!kvoff)
            return 0;
        
        block_n = get_omap_value(sb, omap_nde, kvoff);
        
        if (le16_to_cpu(omap_nde->btn_level) == 0)
            return block_n;
        
        bh = sb_bread(sb, block_n);
        if (!bh) {
            printk(KERN_ERR "apfs: unable to read block [%llu]\n",
                    block_n);
            return 0;
        }
        omap_nde = (struct apfs_btree_node_phys_t*) bh->b_data; 
    }
    
    brelse(bh);
    
    return 0;
}

/*
 * Allocate and return an inode structure from the disk.
 */
struct apfs_record_inode_val_t* get_inode_from_disk(struct super_block* sb,
        u_int64_t i_no)
{
    struct buffer_head *fs_tree_bh;
    struct buffer_head *aux_bh;
    struct apfs_btree_node_phys_t* fs_tree_node;
    struct apfs_glb_info* glb_info;
    struct apfs_record_inode_val_t* apfs_inode;
    struct apfs_xf_blob_t* xf;
    struct apfs_kvloc_t* kvloc;
    u_int8_t min_xfield_len;
    u_int8_t* ptr;
    
    glb_info = (struct apfs_glb_info*) sb->s_fs_info;
    
    /*
     * Search the inode structure in the device.
     */
    fs_tree_bh = sb_bread(sb, glb_info->vol_root_tree);
    if (!fs_tree_bh) {
        printk(KERN_ERR "apfs: unable to read block [%llu]\n", 
                glb_info->vol_root_tree);
        goto end;
    }
    fs_tree_node = (struct apfs_btree_node_phys_t*) fs_tree_bh->b_data;
    
    while (le16_to_cpu(fs_tree_node->btn_level) > 0) {
        kvloc = (struct apfs_kvloc_t*) find_in_node(sb, fs_tree_node, i_no, 
                APFS_TYPE_INODE, NULL, APFS_OBJ_TYPE_FSTREE);
        
        if (!kvloc) {
            printk(KERN_ERR "apfs: inode %llu not found", i_no);
            goto release_bh;
        }
        
        aux_bh = fs_tree_bh;
        fs_tree_bh = get_fstree_child(sb, fs_tree_node, kvloc);
        brelse(aux_bh);
        
        if (!fs_tree_bh) {
            printk(KERN_ERR "apfs: unable to read block\n"); 
            goto end;
        }
        fs_tree_node = (struct apfs_btree_node_phys_t*) fs_tree_bh->b_data;
    }
        
    if (le16_to_cpu(fs_tree_node->btn_level) == 0) {
        /*
         * We are in the leaf node. Now, we search the inode.
         * Finally, allocate memory for it.
         */
        kvloc = (struct apfs_kvloc_t*) find_in_node(sb, fs_tree_node, i_no, 
                APFS_TYPE_INODE, NULL, APFS_OBJ_TYPE_FSTREE);
        min_xfield_len = 0;
        
        if (sizeof(struct apfs_record_inode_val_t) == le16_to_cpu(kvloc->v.len))
            min_xfield_len = sizeof(struct apfs_xf_blob_t);
            
        ptr = kmalloc(le16_to_cpu(kvloc->v.len) + min_xfield_len, GFP_KERNEL);
        if (!ptr) {
            printk(KERN_ERR "apfs: not enought memory\n");
            goto release_bh;
        }
        memcpy(ptr, get_val_zone(sb, fs_tree_node) 
                - le16_to_cpu(kvloc->v.off), le16_to_cpu(kvloc->v.len));
        
        apfs_inode = (struct apfs_record_inode_val_t*) ptr;
        if (sizeof(struct apfs_record_inode_val_t) == le16_to_cpu(kvloc->v.len)) {
            xf = (struct apfs_xf_blob_t*) apfs_inode->xfields;
            xf->xf_num_exts = xf->xf_used_data = 0;
        }

        return apfs_inode;
    }
        
release_bh:
        brelse(fs_tree_bh);
end:
        return NULL;
}

/*
 * Returns the file size. Search in the extended fields.
 * You should use this function only by nodes getted with the
 * function: get_inode_by_disk.
 */
u_int64_t get_inode_size (struct apfs_record_inode_val_t* inode)
{
    struct apfs_xf_blob_t* xf_blob;
    struct apfs_dstream_t* dstream;
    struct apfs_x_field_t* x_field;
    int c, acum_xf_size;
    u_int8_t* p;
        
    xf_blob = (struct apfs_xf_blob_t*) inode->xfields;
    p = (u_int8_t*)xf_blob->xf_data;
    p += le16_to_cpu(xf_blob->xf_num_exts) * sizeof(struct apfs_x_field_t);
    acum_xf_size = 0;
    x_field = xf_blob->xf_data;
    
    for (c=0; c<le16_to_cpu(xf_blob->xf_num_exts); c++) {
        if (xf_blob->xf_data[c].x_type == APFS_INO_EXT_TYPE_DSTREAM) {
            p += acum_xf_size;
            dstream = (struct apfs_dstream_t*) p;
            return le64_to_cpu(dstream->size);
        }
        acum_xf_size += round_up(le16_to_cpu(xf_blob->xf_data[c].x_size), 8);
    }

    return 0;
} 

/*
 * Returns the node that has all the information of an inode. 
 * If the inode records are divided into two nodes, the 
 * function will return the parent of these nodes.
 */
struct buffer_head* get_inode_branch(struct super_block* sb,
        u_int64_t i_no) 
{
    struct apfs_glb_info* glb_info;
    struct buffer_head *fs_tree_bh;
    struct buffer_head *aux_bh;
    struct apfs_btree_node_phys_t* fs_tree_node;
    struct apfs_kvloc_t* kvloc;
    struct apfs_record_key_t* k_val;
    u_int8_t found;
    u_int64_t key_pos;
    u_int8_t* key_zone;
    oid_t oid_fnd;
    
    glb_info = (struct apfs_glb_info*) sb->s_fs_info;
    
    /*
     * Load the B-tree of the root dir.
     */
    fs_tree_bh = sb_bread(sb, glb_info->vol_root_tree);
    if (!fs_tree_bh) {
        printk(KERN_ERR "apfs: unable to read block [%llu]\n", 
                glb_info->vol_root_tree);
        goto end;
    }
    
    fs_tree_node = (struct apfs_btree_node_phys_t*) fs_tree_bh->b_data;
        
    found = 0;
    while (fs_tree_node->btn_level > 0 && !found) {
        /*
         * Search for the object in the node.
         */
        
        kvloc = (struct apfs_kvloc_t*) find_in_node(sb, fs_tree_node, i_no, 
                APFS_TYPE_INODE, NULL, APFS_OBJ_TYPE_FSTREE);
        
        if (!kvloc) {
            printk(KERN_ERR "apfs: inode %llu not found", i_no);
            goto release_bh;
        }
                
        /*
         * Find the next element. If it's the same object_id, the data
         * of this object is distributed among different nodes.
         */
        key_pos = (((u_int8_t *)kvloc) - get_toc_zone(fs_tree_node))/sizeof(*kvloc);
        
        if (key_pos < le32_to_cpu(fs_tree_node->btn_nkeys)) {
            kvloc++;
            key_zone = get_key_zone(fs_tree_node);
            k_val = (struct apfs_record_key_t*)(key_zone + le16_to_cpu(kvloc->k.off));
            oid_fnd = k_val->obj_id_and_type & APFS_OBJ_ID_MASK;
            if (oid_fnd == i_no)
                found = 1;
            kvloc--;
        }
                
        /*
         * Go to the next level!
         */
        if (!found) {
            aux_bh = fs_tree_bh;
            fs_tree_bh = get_fstree_child(sb, fs_tree_node, kvloc);
            brelse(aux_bh);
        
            if (!fs_tree_bh) {
                printk(KERN_ERR "apfs: unable to read block\n"); 
                goto end;
            }
            fs_tree_node = (struct apfs_btree_node_phys_t*) fs_tree_bh->b_data;
        }
    }
        
    return fs_tree_bh;
    
release_bh:
    brelse(fs_tree_bh);
end:
    return NULL;
}

/*
 * Returns the buffer_head of the next node indicated by kvloc
 * in the hierarchy
 */
struct buffer_head* get_fstree_child (struct super_block* sb,
        struct apfs_btree_node_phys_t* node, struct apfs_kvloc_t* kvloc)
{
    struct apfs_glb_info* glb_info;
    struct buffer_head* bh;
    oid_t oid;
    paddr_t block_n;
    
    glb_info = (struct apfs_glb_info*) sb->s_fs_info;
    
    oid = get_fstree_value(sb, node, kvloc);
    block_n = get_phys_block(sb, glb_info->vol_omap_tree, oid,
            glb_info->vol_xid);
    
    if (!block_n) {
        printk(KERN_ERR "apfs: object id not found [%llu]", oid);
        return NULL;
    }
    
    bh = sb_bread(sb, block_n);
    if (!bh) {
        printk(KERN_ERR "apfs: unable to read block [%llu]\n",
                glb_info->vol_root_tree);
        return NULL;
    }

    return bh;
}

/*
 * TODO: Implement a real unicode normalization function.
 */
char* normalize_string(char* unicode_string) 
{
    return unicode_string+2;
}


