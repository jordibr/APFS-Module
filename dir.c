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

#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "apfs.h"
#include "apfs/volume.h"

/*
 * This is a recursive function. The records for an object can be distributed
 * in different nodes in the B-Tree. For this reason, the function starts in a
 * node branch. There can be two possibilities:
 *  - It is a non-leaf node: for each record, the function reads the disk 
 *    block and then calls itself.
 *  - It's a leaf node: for each record, the function checks that the record 
 *    type is APFS_TYPE_FILE_EXTENT. If it is, the function 'emit' the 
 *    directory. 
 */
static void list_dir(struct apfs_btree_node_phys_t* node,
        struct super_block* sb, struct file* filp, struct dir_context *ctx)
{
    struct buffer_head* bh;
    struct inode* inode;
    struct apfs_glb_info* glb_info;
    struct apfs_record_drec_key_t* drec_key;
    struct apfs_record_drec_val_t* drec_val;
    struct apfs_btree_node_phys_t* node_chl;
    struct apfs_kvloc_t* kvloc;
    u_int8_t* key_zone;
    u_int8_t* val_zone;
    u_int8_t* toc_zone;
    int ckeys;
    int entry_type;
    
    glb_info = (struct apfs_glb_info*) sb->s_fs_info;
    inode = filp->f_path.dentry->d_inode;
        
    toc_zone = get_toc_zone(node); 
    key_zone = get_key_zone(node);
    val_zone = get_val_zone(sb, node);
    kvloc = (struct apfs_kvloc_t*) toc_zone;
    
    /*
     * Iterate for over each record of the node
     */ 
    for (ckeys = 0; ckeys<le32_to_cpu(node->btn_nkeys); ckeys++, kvloc++) {
        /*
         * If it is not a leaf node, we go to the child node of the tree.
         */
        if (node->btn_level != 0) {
            bh = get_fstree_child(sb, node, kvloc);
            if (!bh)
                continue;
            node_chl = (struct apfs_btree_node_phys_t*) bh->b_data;        
            list_dir(node_chl, sb, filp, ctx); 
            brelse(bh);
            continue;
        }
        
        /*
         * It's a leaf node, we look for the directory entries.
         */        
        drec_key = (struct apfs_record_drec_key_t*) (key_zone + le16_to_cpu(kvloc->k.off));
        drec_val = (struct apfs_record_drec_val_t*) (val_zone - le16_to_cpu(kvloc->v.off));
        
        if (get_fs_obj_id(&(drec_key->hdr)) == inode->i_ino 
                && get_fs_obj_type(&(drec_key->hdr)) == APFS_TYPE_DIR_REC) {
            if (le16_to_cpu(drec_val->flags) & APFS_DT_DIR)
                entry_type = DT_DIR;
            else if (le16_to_cpu(drec_val->flags) & APFS_DT_REG)
                entry_type = DT_REG;
            else 
                continue;
            
            dir_emit(ctx, normalize_string(drec_key->name), 
                    strlen(normalize_string(drec_key->name)), 
                    le64_to_cpu(drec_val->file_id), entry_type);
            ctx->pos++;
        }
    }
} 

static int apfs_iterate(struct file* filp, struct dir_context *ctx)
{
    struct inode* inode;
    struct super_block* sb;
    struct buffer_head* bh;
    struct apfs_btree_node_phys_t* node;
    
    if(ctx->pos != 0)
        return 0;
    
    inode = filp->f_path.dentry->d_inode;
    sb = inode->i_sb;
        
    if (!dir_emit_dots(filp, ctx)) {
        return -ENOMEM;
    }
    ctx->pos = 2;
        
    bh = get_inode_branch(sb, inode->i_ino);
    if (!bh)
        return 0;
    
    node = (struct apfs_btree_node_phys_t*) bh->b_data;        
    list_dir(node, sb, filp, ctx); 
    
    brelse(bh);
        
    return 0;
}

struct file_operations apfs_dir_operations = {
    .owner = THIS_MODULE,
    .read = generic_read_dir,
    .llseek = generic_file_llseek,
    .iterate = apfs_iterate,
};
