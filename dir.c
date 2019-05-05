
#include <linux/fs.h>

static int apfs_iterate(struct file* filp, struct dir_context *ctx)
{
	struct inode* inode;	
	inode = filp->f_path.dentry->d_inode;

	printk(KERN_INFO "apfs: List inode [%lu]\n", inode->i_ino);

	/*
	 * TODO: Read inode from device and return the list of files.
	 */
	if(ctx->pos != 0)
		return 0;
	
	dir_emit(ctx, "AAAA\0", 1000,
			2, DT_REG);
	dir_emit(ctx, "BBBB\0", 1000,
			2, DT_REG);
	ctx->pos = 1;

	return 0;
}

struct file_operations apfs_dir_operations = {
	.owner = THIS_MODULE,
	.read = generic_read_dir,
	.llseek = generic_file_llseek,
	.iterate = apfs_iterate,
};








