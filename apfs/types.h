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

#ifndef _APFS_TYPES_H
#define _APFS_TYPES_H

/*
 * Object type (and subtype) numbers
 */
#define APFS_OBJ_TYPE_CONTAINER         0x01    /* APFS container superblock */
#define APFS_OBJ_TYPE_ROOT_NODE         0x02    /* B-Tree root node */
#define APFS_OBJ_TYPE_NODE              0x03    /* B-Tree non-root node */
#define APFS_OBJ_TYPE_OMAP              0x0B
#define APFS_OBJ_TYPE_FS                0x0D
#define APFS_OBJ_TYPE_FSTREE            0x0E
#define APFS_OBJ_TYPE_CHECKPOINT_MAP    0x0C
#define APFS_OBJ_TYPE_VOL_SUPERBLOCK    0x0D    /* Volume Superblock */

#define APFS_OBJ_STYPE_NONE             0x00    /* No Subtype */
#define APFS_OBJ_STYPE_FILE             0x0E    /* File subtype */

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
