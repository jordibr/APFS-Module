#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "apfs.h"
#include "apfs/container.h"

static struct kmem_cache *apfs_inode_cache;

static void apfs_destroy_inode(struct inode* i)
{
	printk(KERN_INFO "apfs: inode destroyed!\n");
}

static void apfs_put_super(struct super_block* sb)
{
	printk(KERN_INFO "apfs: super putted!\n");
}

static struct super_operations const apfs_super_ops = {
	.destroy_inode = apfs_destroy_inode,
	.put_super = apfs_put_super,
};

static int apfs_fill_sb(struct super_block* sb, void* data, int silent)
{
	struct inode* root;

	sb->s_magic = 0x12;
	sb->s_op = &apfs_super_ops;	

	root = apfs_get_inode(sb, NULL, 0);

	if (!root)
	{
		return -ENOMEM;
	}

	sb->s_root = d_make_root(root);
	if (!sb->s_root)
	{
		printk(KERN_ERR "apfs: Root creation failed\n");
		return -ENOMEM;
	}

	return 0;
}

static struct dentry* apfs_mount (struct file_system_type* type, 
		int flags, char const* dev, void* data)
{
	struct dentry* entry;
	entry = mount_bdev(type, flags, dev, data, apfs_fill_sb);

	if (unlikely(IS_ERR(entry)))
		printk(KERN_ERR "apfs: Error mounting");
	else
		printk(KERN_INFO "apfs: Succesfully mounted on [%s]\n",
		       dev);

	return entry;
}

static void apfs_kill_sb(struct super_block* sb)
{
	kill_block_super(sb);
	printk(KERN_INFO
	       "apfs: Superblock is destroyed. Unmount succesful.\n");
	return;
}

static struct file_system_type apfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "apfs",
	.mount		= apfs_mount,
	.kill_sb	= apfs_kill_sb,
	.fs_flags	= FS_REQUIRES_DEV,
};
MODULE_ALIAS_FS("apfs");

static int __init init_apfs_fs(void)
{
	int err;

	/*
	 * Create inode cache
	 */
	apfs_inode_cache = kmem_cache_create("apfs_inode_cache",
			sizeof(struct inode),
			0,
			(SLAB_RECLAIM_ACCOUNT| SLAB_MEM_SPREAD),
			NULL);

	if (!apfs_inode_cache) {
		printk(KERN_ERR "apfs: cannot create inode cache\n");
		return -ENOMEM;
	}

	/*
	 * Register Filesystem
	 */
        err = register_filesystem(&apfs_fs_type);
	if (likely(!err)) {
		printk(KERN_INFO "apfs: Sucessfully registered\n");
	} else {
		kmem_cache_destroy(apfs_inode_cache);
		printk(KERN_ERR "apfs: Failed to register. Error[%d]\n", 
				err);
		return err;
	}

	return 0;
}

static void __exit exit_apfs_fs(void)
{
	int err;
	err = unregister_filesystem(&apfs_fs_type);
	kmem_cache_destroy(apfs_inode_cache);
	
	if (likely(!err))
		printk(KERN_INFO "apfs: Sucessfully unregistered\n");
	else
		printk(KERN_ERR "apfs: Failed to unregister. Error:[%d]\n", 
				err);
}

MODULE_AUTHOR("Jordi Barcons");
MODULE_DESCRIPTION("Apple Filesystem");
MODULE_LICENSE("GPL");
module_init(init_apfs_fs);
module_exit(exit_apfs_fs);
