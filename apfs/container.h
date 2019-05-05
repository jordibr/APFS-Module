#ifndef _APFS_CONTAINER_H
#define _APFS_CONTAINER_H

#include "types.h"

/*
 * Superblock static values.
 */
#define APFS_MAGIC		"NXSB"
#define APFS_MAX_FILE_SYSTEMS 	100

/*
 * All object nodes start with a 32-byte header
 */
struct apfs_obj_header {
	u_int64_t checksum;		/* Checksum of the object */
	oid_t object_id;		/* Identify de object in the container */
	xid_t transaction_id;		/* Incremented when the object is changed */
	u_int16_t block_type;		/* Identify the type of object contained */
	u_int16_t flags;
	u_int16_t block_subtype;	/* Identify the subtype of object contained */
	u_int16_t padding;		/* A two-byte padding */
};
typedef struct apfs_obj_header apfs_obj_header_t;

/*
 * Definition of the superblook. This is a first structure in the partition.
 * Among other things, this structure saves information about the volumes 
 * and checkpoints.
 */
struct apfs_superblock_t {
	apfs_obj_header_t obj_h;
	u_int32_t magic_number;		/* It's always APFS_SP_MAGIC */
	u_int32_t block_size;		/* Standard block size is 0x1000 */
	u_int64_t block_count;		/* Total size of partition (number of blocks) */
	u_int64_t features;
	u_int64_t read_only_features;
	u_int64_t incompatible_features;

	apfs_uuid_t uuid;		/* Superblock id */

	oid_t next_oid;
	xid_t next_sid;
	
	/* Define the checkpoints regions */	
	u_int32_t xp_desc_blocks;
	u_int32_t xp_data_blocks;
	paddr_t xp_desc_base;
	paddr_t xp_data_base;
	u_int32_t xp_desc_next;
	u_int32_t xp_data_next;
	u_int32_t xp_desc_index;
	u_int32_t xp_desc_len;
	u_int32_t xp_data_index;
	u_int32_t xp_data_len;
	
	oid_t spaceman_oid;
	oid_t omap_oid;
	oid_t reaper_oid;

	u_int32_t test_type;

	u_int32_t max_file_systems;
	oid_t fs_oid[APFS_MAX_FILE_SYSTEMS];
};

/*
 * Structures that preserve checkpoints information. 
 */
struct apfs_checkpoint_mapping_t {
	u_int32_t cpm_type;
	u_int32_t cpm_subtype;
	u_int32_t cpm_size;
	u_int32_t cpm_pad;
	oid_t cpm_fs_oid;
	oid_t cpm_oid;
	oid_t cpm_paddr;
};

struct apfs_checkpoint_map_phys_t {
	apfs_obj_header_t obj_h;
	u_int32_t cpm_flags;
	u_int32_t cpm_count;
	struct apfs_checkpoint_mapping_t cpm_map[];
};

#endif /* _APFS_CONTAINER_H */
