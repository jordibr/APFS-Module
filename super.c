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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "apfs.h"
#include "apfs/container.h"
#include "apfs/volume.h"
#include "apfs/btree.h"
#include "apfs/omap.h"

static void apfs_put_super(struct super_block* sb)
{
    kfree(sb->s_fs_info);
    printk(KERN_INFO "apfs: super putted!\n");
}

static struct super_operations const apfs_super_ops = {
    .put_super = apfs_put_super
};

static int apfs_fill_sb(struct super_block* sb, void* data, int silent)
{
    struct buffer_head *bh_cnt;
    struct buffer_head *bh_vol;
    struct buffer_head *bh;
    
    struct apfs_superblock_t* apfs_cnt;
    struct apfs_vol_superblock_t* apfs_vol;

    struct apfs_omap_phys_t* omap_obj;
    struct apfs_glb_info* glb_info;
    struct inode* root_inode;

    oid_t vol_block;
    int ret = -ENOMEM;
    
    /*
     * Read the superblock of the partition and fill the structure.
     * TODO: Read the last valid superblock. Currently, the first partition 
     * is read. This is correct if the device was properly unmounted.
     */
    bh_cnt = sb_bread(sb, APFS_SUPERBLOCK_BLOCK);
    if (!bh_cnt) {
        printk(KERN_ERR "apfs: unable to read the superblock\n");
        goto end;
    }

    apfs_cnt = (struct apfs_superblock_t*) bh_cnt->b_data;
    
    sb->s_magic = le32_to_cpu(apfs_cnt->magic_number);
    sb->s_blocksize = le32_to_cpu(apfs_cnt->block_size);
    sb->s_op = &apfs_super_ops;
        
    if (sb->s_magic != APFS_MAGIC) {
        printk(KERN_ERR "apfs: it is not an APFS partition\n");
        goto release_sb;
    }

    if (sb->s_blocksize < APFS_DEFAULT_BLOCK_SIZE 
            || sb->s_blocksize > APFS_MAXIMUM_BLOCK_SIZE) {
        printk(KERN_ERR "apfs: does not have a valid block size\n");
        goto release_sb;
    }
    
    /*
     * It's a valid partition. We allocate memory for the structure stored in
     * sb->s_fs_info.
     */
    glb_info = kmalloc(sizeof(struct apfs_glb_info), GFP_KERNEL);
    if (!glb_info) {
        printk(KERN_ERR "apfs: not enought memory\n");
        goto release_sb;
    }
    sb->s_fs_info = glb_info;
    glb_info->cnt_oid = le64_to_cpu(apfs_cnt->obj_h.oid);
    glb_info->cnt_xid = le64_to_cpu(apfs_cnt->obj_h.xid);

    /*
     * Start getting data from partition. First, we look for the block number
     * of the container tree.
     */
    bh = sb_bread(sb, apfs_cnt->omap_oid);
    if (!bh) {
        printk(KERN_ERR "apfs: unable to read block [%llu]\n", 
            apfs_cnt->omap_oid);
        goto release_glb_info;
    }
    omap_obj = (struct apfs_omap_phys_t*) bh->b_data;
    glb_info->cnt_omap_tree = le64_to_cpu(omap_obj->om_tree_oid);
    brelse(bh);
    
    /*
     * We get the block number of the volume structure.
     * TODO: Allow to choose the volume. For now, we mount the first volume 
     * (apfs_cnt->fs_oid[0]).
     */
    vol_block = le64_to_cpu(get_phys_block(sb, glb_info->cnt_omap_tree,
                apfs_cnt->fs_oid[0], glb_info->cnt_xid));
    if (vol_block == 0) {
        printk(KERN_ERR "apfs: invalid object id [%llu]\n",
                apfs_cnt->fs_oid[0]);
        goto release_glb_info;
    }
     
    /*
     * Read the structure of the volume (i.e. apfs_vol_superblock_t).
     */
    bh_vol = sb_bread(sb, vol_block);
    if (!bh_vol){
        printk(KERN_ERR "apfs: unable to read block [%llu]\n", 
                vol_block);
        goto release_glb_info;
    }
    apfs_vol = (struct apfs_vol_superblock_t*) bh_vol->b_data;
    glb_info->vol_oid = le64_to_cpu(apfs_vol->obj_h.oid);
    glb_info->vol_xid = le64_to_cpu(apfs_vol->obj_h.xid);
     
    /*
     * Get the block number of the omap tree of the volume.
     */
    bh = sb_bread(sb, apfs_vol->apfs_omap_oid);
    if (!bh){
        printk(KERN_ERR "apfs: unable to read block [%llu]\n", 
                apfs_vol->apfs_omap_oid);
        goto release_vol;
    }
    omap_obj = (struct apfs_omap_phys_t*) bh->b_data;
    glb_info->vol_omap_tree = le64_to_cpu(omap_obj->om_tree_oid);
    brelse(bh);
     
    /*
     * Get the block number of the root dir.
     */
    glb_info->vol_root_tree = le64_to_cpu(get_phys_block(sb,
                glb_info->vol_omap_tree, apfs_vol->apfs_root_tree_oid,
                glb_info->vol_xid)); 
    if (glb_info->vol_root_tree == 0) {
        printk(KERN_ERR "apfs: invalid object id [%llu]\n",
                apfs_vol->apfs_root_tree_oid);
        goto release_vol;
    }
     
    /*
     * Create the dentry of the root directory.
     */
    root_inode = get_apfs_inode(sb, NULL, 2, S_IFDIR);
    if (!root_inode) {
        goto release_vol;
    }
    
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        printk(KERN_ERR "apfs: root creation failed\n");
        goto release_vol;
    }

    brelse(bh_vol);
    brelse(bh_cnt);
    
    return 0;

release_vol:
    brelse(bh_vol);   
release_glb_info:
    kfree(glb_info);
release_sb:
    brelse(bh_cnt);
end:
    return ret;
}

static struct dentry* apfs_mount (struct file_system_type* type, 
        int flags, char const* dev, void* data)
{
    struct dentry* entry;
    entry = mount_bdev(type, flags, dev, data, apfs_fill_sb);

    if (unlikely(IS_ERR(entry)))
        printk(KERN_ERR "apfs: error mounting\n");
    else
        printk(KERN_INFO "apfs: succesfully mounted on [%s]\n",
                dev);

    return entry;
}

static struct file_system_type apfs_fs_type = {
    .owner      = THIS_MODULE,
    .name       = "apfs",
    .mount      = apfs_mount,
    .kill_sb    = kill_block_super,
    .fs_flags   = FS_REQUIRES_DEV,
};
MODULE_ALIAS_FS("apfs");

static int __init init_apfs_fs(void)
{
    int err;

    err = register_filesystem(&apfs_fs_type);
    if (likely(!err)) {
        printk(KERN_INFO "apfs: sucessfully registered\n");
    } else {
        printk(KERN_ERR "apfs: failed to register. Error[%d]\n", 
                err);
        return err;
    }

    return 0;
}

static void __exit exit_apfs_fs(void)
{
    int err;

    err = unregister_filesystem(&apfs_fs_type);

    if (likely(!err))
        printk(KERN_INFO "apfs: sucessfully unregistered\n");
    else
        printk(KERN_ERR "apfs: failed to unregister. Error:[%d]\n", 
                err);
}

MODULE_AUTHOR("Jordi Barcons");
MODULE_DESCRIPTION("Apple Filesystem");
MODULE_LICENSE("GPL");
module_init(init_apfs_fs);
module_exit(exit_apfs_fs);
