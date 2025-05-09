# CSE321-Project---VSFSck
VSFSck: A Consistency Checker for Very Simple File System (VSFS) [EXT2]

A command-line tool for checking and repairing Very Simple File System (VSFS) images.

## Overview

`vsfsck` is a file system checker for VSFS (Very Simple File System). It validates the integrity of a VSFS image file and repairs any inconsistencies found in the superblock and data bitmap.

## Features

- Superblock Validation: Checks if superblock values match expected constants
- Data Bitmap Validation: Verifies consistency between data bitmap and inode references
- Automatic Repair: Fixes inconsistencies in both the superblock and data bitmap
- Indirect Block Processing: Supports single, double, and triple indirect block pointers

## File System Structure

The VSFS has the following layout:
- Block size: 4096 bytes
- Total blocks: 64
- Block 0: Superblock
- Block 1: Inode bitmap
- Block 2: Data bitmap
- Blocks 3-7: Inode table (5 blocks)
- Blocks 8-63: Data blocks

## Usage

```
./vsfsck <image_file_path>
```

Where `<image_file_path>` is the path to the VSFS image file to check and repair.

## Validation Rules

The checker implements these key validation rules:

1. Superblock Validation: Ensures that all superblock fields match the expected values (magic number, block size, etc.)

2. Data Bitmap Rules:
   - Rule A: Blocks marked as used in the bitmap should be referenced by valid inodes
   - Rule B: Blocks referenced by any inodes should be marked as used in the bitmap

## Build Instructions

Compile the program with:

```
gcc -o vsfsck vsfsck.c
```

## Example Output

```
Validating superblock for image: fsimage.img
---------------------------------
Superblock validation successful. No errors found.
---------------------------------

Validating Data Bitmap
---------------------------------
Checking Rule a: Bitmap used and referenced by valid inode
Checking Rule b: Referenced by any inode and bitmap used
---------------------------------
Data bitmap validation successful. No errors found.
---------------------------------
```

## Implementation Details

- Written in C for efficient low-level file system access
- Uses bitwise operations for bitmap manipulation
- Handles direct and indirect block pointers (single, double, and triple)
- Tracks blocks referenced by valid and invalid inodes separately
