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
#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "apfs.h"
#include "apfs/volume.h"

/*
 * Create and return a new structure with the information of the inode 'i_no'.
 */
struct inode* get_apfs_inode(struct super_block* sb, struct inode* parent,
        uint64_t i_no, int inode_type)
{
    struct apfs_record_inode_val_t* apfs_inode;
    struct inode* inode;
    
    /*
     * Get the inode information from the disk.
     */
    apfs_inode = get_inode_from_disk(sb, i_no);
    if (!apfs_inode) {
        printk(KERN_ERR "apfs: inode not found [%llu]\n",
                i_no);
        return NULL;
    }
    
    /*
     * Create a new inode structure.
     */
    inode = new_inode(sb);
    if (!inode) {
        kfree(apfs_inode);
        printk(KERN_ERR "apfs: inode allocation failed\n");
        return NULL;
    }

    /*
     * Fill the inode structure.
     * TODO:
     * - Set the permissions correctly from the disk data.
     * - Set the uid/gid from the disk data.
     * - Currently, only files and directories are allowed.
     */
    inode->i_ino = i_no;
    inode_init_owner(inode, parent, inode_type);
    inc_nlink(inode);
    
    inode->i_ctime.tv_sec = le64_to_cpu(apfs_inode->create_time) / NSEC_TO_SEC;
    inode->i_atime.tv_sec = le64_to_cpu(apfs_inode->access_time) / NSEC_TO_SEC;
    inode->i_mtime.tv_sec = le64_to_cpu(apfs_inode->mod_time) / NSEC_TO_SEC;
    inode->i_sb = sb;
    inode->i_op = &apfs_inode_operations;
    inode->i_size = get_inode_size(apfs_inode);
    
    if (inode_type == S_IFDIR)
        inode->i_fop = &apfs_dir_operations;
    else
        inode->i_fop = &apfs_file_operations;
    
    inode->i_mode |= S_IWUGO | S_IRUGO | S_IXUGO;

    kfree(apfs_inode);
        
    return inode;
}

/*
 * This is a recursive function. The records for an object can be distributed
 * in different nodes in the B-Tree. For this reason, the function starts in a
 * node branch. There can be two possibilities:
 *  - It is a non-leaf node: for each record, the function reads the disk 
 *    block and then calls itself.
 *  - It's a leaf node: for each record, the function checks that the record 
 *    type is APFS_TYPE_DIR_REC. Next, check if it's the inode that we searching
 *    and register it.
 */
static int search_in_dir(struct apfs_btree_node_phys_t* node,
        struct super_block* sb, struct inode *parent_inode,
        struct dentry *child_dentry)
{
    struct buffer_head* bh;
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
    int rtn_val;

    glb_info = (struct apfs_glb_info*) sb->s_fs_info;

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
        if (le16_to_cpu(node->btn_level) != 0) {
            bh = get_fstree_child(sb, node, kvloc);
            if (!bh)
                continue;
            node_chl = (struct apfs_btree_node_phys_t*) bh->b_data;        
            rtn_val = search_in_dir(node_chl, sb, parent_inode, child_dentry); 
            
            brelse(bh);
            
            if (rtn_val)
                return rtn_val;
            else
                continue;
        }
        
        /*
         * It's a leaf node, we look for the directory entries.
         */
        drec_key = (struct apfs_record_drec_key_t*) (key_zone + le16_to_cpu(kvloc->k.off));
        drec_val = (struct apfs_record_drec_val_t*) (val_zone - le16_to_cpu(kvloc->v.off));
        
        if (get_fs_obj_id(&(drec_key->hdr)) == parent_inode->i_ino 
                && get_fs_obj_type(&(drec_key->hdr)) == APFS_TYPE_DIR_REC) {
            if (!strcmp(child_dentry->d_name.name, 
                        normalize_string(drec_key->name))){
                if (le16_to_cpu(drec_val->flags) & APFS_DT_DIR)
                    entry_type = S_IFDIR;
                else if (le16_to_cpu(drec_val->flags) & APFS_DT_REG)
                    entry_type = S_IFREG;
                else 
                    continue;
                d_add(child_dentry, get_apfs_inode(sb, parent_inode, 
                            le64_to_cpu(drec_val->file_id), entry_type));
                return 1;
            } 
        }
    }
    
    return 0;
} 

static struct dentry *apfs_lookup(struct inode *parent_inode,
        struct dentry *child_dentry, unsigned int flags)
{
    struct super_block* sb;
    struct buffer_head* bh;
    struct apfs_btree_node_phys_t* node;
    
    sb = parent_inode->i_sb;
    bh = get_inode_branch(sb, parent_inode->i_ino);
    if (!bh) {
        printk(KERN_INFO "apfs: inode not found[%lu]\n",
                parent_inode->i_ino);
        return NULL;
    }

    node = (struct apfs_btree_node_phys_t*) bh->b_data;
    search_in_dir(node, sb, parent_inode, child_dentry);
    
    brelse(bh);

    return NULL;
}

struct inode_operations apfs_inode_operations = {
    .lookup = apfs_lookup
};

