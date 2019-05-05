#ifndef _APFS_TYPES_H
#define _APFS_TYPES_H

/*
 * Object type (and subtype) numbers
 */
#define APFS_OBJ_TYPE_CONTAINER 	0x01	/* APFS container superblock */
#define APFS_OBJ_TYPE_ROOT_NODE 	0x02	/* B-Tree root node */
#define APFS_OBJ_TYPE_NODE 		0x03	/* B-Tree non-root node */
#define APFS_OBJ_TYPE_CHECKPOINT_MAP	0x0C
#define APFS_OBJ_TYPE_VOL_SUPERBLOCK	0x0D	/* Volume Superblock */

#define APFS_OBJ_STYPE_NONE		0x00	/* No Subtype */
#define APFS_OBJ_STYPE_FILE		0x0E	/* File subtype */

/*
 * APFS uuid is an unsigned int of 128 bits
 */
typedef unsigned char apfs_uuid_t[16];

/*
 * A physical address of an on-disk block
 */
typedef u_int64_t paddr_t;

/*
 * An object identifier
 */
typedef u_int64_t oid_t;

/*
 * A transaction identifier
 */
typedef u_int64_t xid_t;

#endif /* _APFS_TYPES_H */
