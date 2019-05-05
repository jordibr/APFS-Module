
#include <linux/fs.h>
#include "apfs.h"

static struct dentry *apfs_lookup(struct inode *parent_inode,
			       struct dentry *child_dentry, unsigned int flags)
{
	
	printk(KERN_INFO "apfs: lookup in inode %lu for %s\n", 
		parent_inode->i_ino, child_dentry->d_name.name);

	return NULL;
}

struct inode* apfs_get_inode(struct super_block* sb, struct inode* parent,
			     uint64_t i_no)
{
	struct inode* inode;
	inode = new_inode(sb);
	
	if (!inode)
	{
		printk(KERN_ERR "apfs: Inode allocation failed\n");
		return NULL;
	}
	
	/*
	 * TODO: Read data from partition and fill inode correctly.
	 */

	inode->i_ino = i_no;
	inode_init_owner(inode, parent, S_IFDIR);
	inc_nlink(inode);
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
	
	inode->i_sb = sb;
	inode->i_op = &apfs_inode_operations;
	inode->i_fop = &apfs_dir_operations;

	return inode;
}

struct inode_operations apfs_inode_operations = {
	.lookup = apfs_lookup,
};

