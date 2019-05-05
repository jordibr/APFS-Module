
#include <linux/fs.h>

/*
 * dir.c
 */
extern struct file_operations apfs_dir_operations;

/*
 * inode.c
 */
struct inode* apfs_get_inode(struct super_block* sb, struct inode* parent,
			     uint64_t i_no);
extern struct inode_operations apfs_inode_operations;

