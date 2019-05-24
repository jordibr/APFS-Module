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
#include <linux/slab.h>

#include "apfs.h"
#include "apfs/volume.h"

/*
 * This is a recursive function. The records for an object can be distributed
 * in different nodes in the B-Tree. For this reason, the function starts in a
 * node branch. There can be two possibilities:
 *  - It is a non-leaf node: for each record, the function reads the disk 
 *    block and then calls itself.
 *  - It's a leaf node: for each record, the function checks that the record 
 *    type is APFS_TYPE_FILE_EXTENT. Next, we verify if the EXTENT containts
 *    the data requested by the user.
 */
static size_t read_data (struct apfs_btree_node_phys_t* node,
        struct super_block* sb, struct file* filp, char __user* buf,
        size_t len, loff_t* ppos)
{
    struct buffer_head* bh;
    struct buffer_head* bh_data;
    struct inode* inode;
    struct apfs_glb_info* glb_info;
    struct apfs_record_file_extent_key_t* ext_key;
    struct apfs_record_file_extent_val_t* ext_val;
    struct apfs_btree_node_phys_t* node_chl;
    struct apfs_kvloc_t* kvloc;
    size_t file_len, rst_data, bytes_to_read, ret;
    u_int8_t* key_zone;
    u_int8_t* val_zone;
    u_int8_t* toc_zone;
    int ckeys, ext_len, block_size;
    
    glb_info = (struct apfs_glb_info*) sb->s_fs_info;
    block_size = sb->s_blocksize;

    inode = filp->f_path.dentry->d_inode;
    file_len = inode->i_size;
    rst_data = file_len-(*ppos);
    bytes_to_read = min(rst_data, len);
        
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
            ret = read_data(node_chl, sb, filp, buf, len, ppos); 
            brelse(bh);
                        
            if (ret) {
                return ret;
            }
            continue;
        }

        /*
         * It's a leaf node, we look for the extent entries.
         */
        ext_key = (struct apfs_record_file_extent_key_t*) (key_zone + le16_to_cpu(kvloc->k.off));
        ext_val = (struct apfs_record_file_extent_val_t*) (val_zone - le16_to_cpu(kvloc->v.off));

        if (get_fs_obj_id(&(ext_key->hdr)) == inode->i_ino
                && get_fs_obj_type(&(ext_key->hdr)) == APFS_TYPE_FILE_EXTENT) {
            ext_len = ext_val->len_and_flags & APFS_RECORD_FILE_EXTENT_LEN_MASK;
            if (*ppos >= ext_key->logical_addr
                   && *ppos < ext_key->logical_addr + ext_len) {
                bytes_to_read = min(bytes_to_read, (size_t)(block_size-(*ppos)%(block_size)));
                bh_data = sb_bread(inode->i_sb, ext_val->phys_block_num+(*ppos)/(block_size));
                if (!bh_data) {
                    printk(KERN_ERR "apfs: unable to read block number\n");
                    return 0;
                }
                if (copy_to_user(buf, (bh_data->b_data)+(*ppos)%(block_size), bytes_to_read)) {
                    brelse(bh_data);
                    printk(KERN_ERR
                            "apfs: error copying data to the userspace buffer\n");
                    return 0;
                }
                
                *ppos += bytes_to_read;
                brelse(bh_data);
                return bytes_to_read;
            }
        }
    }
    
    return 0;
}

ssize_t apfs_read(struct file* filp, char __user* buf, size_t len,
              loff_t* ppos)
{
    struct inode* inode;
    struct buffer_head *bh;
    struct apfs_btree_node_phys_t* node;
    size_t read_b;
    
    inode = filp->f_path.dentry->d_inode;
    
    if (inode->i_size <= *ppos)
        return 0;
    
    bh = get_inode_branch(inode->i_sb, inode->i_ino);
    if (!bh)
        return 0;
    
    node = (struct apfs_btree_node_phys_t*) bh->b_data;
    read_b = read_data(node, inode->i_sb, filp, buf, len, ppos);
    
    brelse(bh);

    return read_b;        
}

struct file_operations apfs_file_operations = {
    .owner = THIS_MODULE,
    .read = apfs_read,
    .llseek = generic_file_llseek
};
