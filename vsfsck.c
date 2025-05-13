#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

// ? ############################## Defining Constants and Global Variables ##############################

#define BLOCKSIZE 4096
#define TOTALBLOCKS 64
#define SUPERBLOCKNUM 0
#define INODEBIMBLOCKNUM 1
#define DATABIMBLOCKNUM 2
#define INODETABSBLOCKNUM 3
#define INODETABNUMBLOCKS 5
#define FIRSTDATABLOCKNUM 8
#define LASTDATABLOCKNUM 63
#define NUMDATABLOCKSFS (LASTDATABLOCKNUM - FIRSTDATABLOCKNUM + 1)
#define INODESIZE 256
#define INODECOUNT ((INODETABNUMBLOCKS * BLOCKSIZE) / INODESIZE)
#define MAGICNUM 0xD34D
#define POINTERSPBLOCK (BLOCKSIZE / sizeof(uint32_t))

int referencedByAnyInode[TOTALBLOCKS];
int referencedByValidInode[TOTALBLOCKS];

/*
 ! PROJECT INFORMATION
 * Very Simple File System Checker (vsfsck)
 * Implemented features listed below by Farhan Zarif (23301692)
 *
 * Functions implemented:
 * - readBlock: Reads a block from the file system image
 * - writeBlock: Writes a block to the file system image
 * - bitCheck: Checks if a bit is set in a bitmap. Using bitwise operations.
 * - setBit: Sets a bit in a bitmap. Using bitwise operations.
 * - removeBit: Clears a bit in a bitmap. Using bitwise operations.
 * - validateSuperblock: Checks superblock values against expected constants
 * - fixSuperBlock: Fixes errors in the superblock
 * - markDataBlockReference: Records a data block as referenced
 * - processIndirectBPointers: Traverses indirect block pointers
 * - collectBlocksForInode: Collects all blocks referenced by an inode
 * - validateDataBitmap: Validates data bitmap consistency
 * - fixDataBitmap: Fixes errors in the data bitmap
 */

// ? ############################## Defining Structs ##############################

typedef struct
{
	uint16_t magicByte;
	uint32_t blockSize;
	uint32_t totalBlocks;
	uint32_t ibimBlock;		 // inode bitmap block number
	uint32_t dbimBlock;		 // data bitmap block number
	uint32_t itabStartBlock; // inode table start block number
	uint32_t firstDataBlock; // first data block number
	uint32_t inodeSize;
	uint32_t inodeCount;
	unsigned char reserved[4058];
} Superblock;

typedef struct
{
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
	uint32_t sizeBytes;
	uint32_t lastAccessTime;
	uint32_t createionTime;
	uint32_t lastModificationTime;
	uint32_t deletionTime;
	uint32_t numHardLinks;
	uint32_t numDataBlocksAllocated;
	uint32_t directPointer[12];
	uint32_t singleIndirectPointer;
	uint32_t doubleIndirectPointer;
	uint32_t tripleIndirectPointer;
	unsigned char reserved[156];
} Inode;

// ? ############################## Helper Functions References ##############################

void readBlock(int fd, uint32_t blockNum, unsigned char *buffer);
void writeBlock(int fd, uint32_t blockNum, unsigned char *buffer);
int bitCheck(const unsigned char *bitMap, int bitIndex);
void setBit(unsigned char *bitMap, int bitIndex);
void removeBit(unsigned char *bitMap, int bitIndex);
int validateSuperblock(char *image);
void fixSuperBlock(char *image);
void markDataBlockReference(uint32_t inodeNum, uint32_t dataBlockAddress, int isCurrentInodeValid);
void processIndirectBPointers(int fd, uint32_t inodeNum, uint32_t indirectBlockAddress, int level, int isCurrentInodeValid);
void collectBlocksForInode(int fd, uint32_t inodeNum, Inode *currentInode);
int validateDataBitmap(char *image);
void fixDataBitmap(char *image);
int validateInodeBitmap(char *image);
void fixInodeBitmap(char *image);
int validateAndFixBlockPointers(char *image);
int detectDuplicateBlocks(char *image);
void processIndirectReferences(int fd, uint32_t indirectBlockAddress, int level, int *blockRefCount);

// ! ############################## MAIN FUNCTION ##############################
// * ############################## MAIN FUNCTION ##############################
// ? ############################## MAIN FUNCTION ##############################

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Incorrect Usage.\nCorrect Format :   %s <FILE.img>\n", argv[0]);
		printf("Try Running    :   cp vsfs-\\(backup\\).img vsfs.img && gcc -o checker vsfsck.c && ./checker vsfs.img\n");
		return 1;
	}

	// ! FARHAN ZARIF
	if (validateSuperblock(argv[1]) > 0)
	{
		printf("Superblock validation failed. Fixing errors...\n");
		fixSuperBlock(argv[1]);
		printf("---------------------------------\n");
		printf("\n");
	}
	else
	{
		printf("Superblock validation successful. No errors found.\n");
		printf("---------------------------------\n");
		printf("\n");
	}

	// ! Al- Saihan Tajvi
	if (validateInodeBitmap(argv[1]) > 0)
	{
		printf("Inode bitmap validation failed. Fixing errors...\n");
		fixInodeBitmap(argv[1]);
		printf("---------------------------------\n");
		printf("\n");
	}
	else
	{
		printf("Inode bitmap validation successful. No errors found.\n");
		printf("---------------------------------\n");
		printf("\n");
	}

	// ! FARHAN ZARIF
	if (validateDataBitmap(argv[1]) > 0)
	{
		printf("Data bitmap validation failed. Fixing errors...\n");
		fixDataBitmap(argv[1]);
		printf("---------------------------------\n");
		printf("\n");
	}
	else
	{
		printf("Data bitmap validation successful. No errors found.\n");
		printf("---------------------------------\n");
		printf("\n");
	}

	// ! Al- Saihan Tajvi
	if (validateAndFixBlockPointers(argv[1]) > 0)
	{
		printf("Bad block pointer validation failed.\n");
	}
	else
	{
		printf("Bad block pointer validation successful. No errors found.\n");
		printf("---------------------------------\n");
		printf("\n");
	}

	// ! SADIK
	if (detectDuplicateBlocks(argv[1]) > 0)
	{
		printf("Duplicate blocks were found (potential corruption)\n");
	}
	else
	{
		printf("No duplicate blocks found\n");
	}

	return 0;
}

// ! ############################## Farhan Zarif ##############################

// ? ############################## READ BLOCK ##############################

void readBlock(int fd, uint32_t blockNum, unsigned char *buffer)
{
	lseek(fd, blockNum * BLOCKSIZE, SEEK_SET);
	read(fd, buffer, BLOCKSIZE);
}

// ? ############################## WRITE BLOCK ##############################

void writeBlock(int fd, uint32_t blockNum, unsigned char *buffer)
{
	lseek(fd, blockNum * BLOCKSIZE, SEEK_SET);
	write(fd, buffer, BLOCKSIZE);
}

// ? ############################## BIT CHECK ##############################

int bitCheck(const unsigned char *bitMap, int bitIndex)
{
	int byteIndex = bitIndex / 8;
	int bitOffset = bitIndex % 8;

	int isSet = 0;
	if ((bitMap[byteIndex] >> bitOffset) & 1)
	{
		isSet = 1;
		return isSet;
	}
	return isSet;
}

// ? ############################## SET BIT ##############################

void setBit(unsigned char *bitMap, int bitIndex)
{
	int byteIndex = bitIndex / 8;
	int bitOffset = bitIndex % 8;
	bitMap[byteIndex] |= (1 << bitOffset);
}

// ? ############################## REMOVE BIT ##############################

void removeBit(unsigned char *bitMap, int bitIndex)
{
	int byteIndex = bitIndex / 8;
	int bitOffset = bitIndex % 8;
	bitMap[byteIndex] &= ~(1 << bitOffset);
}

// ? ############################## VALIDATE SUPERBLOCK ##############################

int validateSuperblock(char *image)
{
	int fd = open(image, O_RDONLY);
	Superblock *sbPTR = (Superblock *)malloc(sizeof(Superblock));
	readBlock(fd, SUPERBLOCKNUM, (unsigned char *)sbPTR);

	printf("Validating superblock for image: %s\n", image);
	printf("---------------------------------\n");
	int error = 0;

	if (sbPTR->magicByte != MAGICNUM)
	{
		printf("Error: Superblock - Invalid magic number. Expected %X, GOT %X\n", MAGICNUM, sbPTR->magicByte);
		error++;
	}
	if (sbPTR->blockSize != BLOCKSIZE)
	{
		printf("Error: Superblock - Invalid block size. Expected %u, GOT %u\n", BLOCKSIZE, sbPTR->blockSize);
		error++;
	}
	if (sbPTR->totalBlocks != TOTALBLOCKS)
	{
		printf("Error: Superblock - Invalid total number of blocks. Expected %u, GOT %u\n", TOTALBLOCKS, sbPTR->totalBlocks);
		error++;
	}
	if (sbPTR->ibimBlock != INODEBIMBLOCKNUM)
	{
		printf("Error: Superblock - Invalid inode bitmap block number. Expected %u, GOT %u\n", INODEBIMBLOCKNUM, sbPTR->ibimBlock);
		error++;
	}
	if (sbPTR->dbimBlock != DATABIMBLOCKNUM)
	{
		printf("Error: Superblock - Invalid data bitmap block number. Expected %u, GOT %u\n", DATABIMBLOCKNUM, sbPTR->dbimBlock);
		error++;
	}
	if (sbPTR->itabStartBlock != INODETABSBLOCKNUM)
	{
		printf("Error: Superblock - Invalid inode start block number. Expected %u, GOT %u\n", INODETABSBLOCKNUM, sbPTR->itabStartBlock);
		error++;
	}
	if (sbPTR->firstDataBlock != FIRSTDATABLOCKNUM)
	{
		printf("Error: Superblock - Invalid inode table start block number. Expected %u, GOT %u\n", FIRSTDATABLOCKNUM, sbPTR->firstDataBlock);
		error++;
	}
	if (sbPTR->inodeSize != INODESIZE)
	{
		printf("Error: Superblock - Invalid inode size. Expected %u, GOT %u\n", INODESIZE, sbPTR->inodeSize);
		error++;
	}
	if (sbPTR->inodeCount != INODECOUNT)
	{
		printf("Error: Superblock - Invalid inode count. Expected %u, GOT %u\n", INODECOUNT, sbPTR->inodeCount);
		error++;
	}
	printf("---------------------------------\n");

	close(fd);
	free(sbPTR);
	return error;
}

// ? ############################## FIX SUPERBLOCK ##############################

void fixSuperBlock(char *image)
{
	int fd = open(image, O_WRONLY);
	Superblock *sbPTR = (Superblock *)malloc(sizeof(Superblock));
	sbPTR->magicByte = MAGICNUM;
	sbPTR->blockSize = BLOCKSIZE;
	sbPTR->totalBlocks = TOTALBLOCKS;
	sbPTR->ibimBlock = INODEBIMBLOCKNUM;
	sbPTR->dbimBlock = DATABIMBLOCKNUM;
	sbPTR->itabStartBlock = INODETABSBLOCKNUM;
	sbPTR->firstDataBlock = FIRSTDATABLOCKNUM;
	sbPTR->inodeSize = INODESIZE;
	sbPTR->inodeCount = INODECOUNT;

	writeBlock(fd, 0, (unsigned char *)sbPTR);
	close(fd);
	free(sbPTR);
	printf("Fixed all the errors regarding Superblock. Please rerun the checker to ensure!\n");
}

// ? ############################## MARK DATA BLOCK REFERENCE ##############################

void markDataBlockReference(uint32_t inodeNum, uint32_t dataBlockAddress, int isCurrentInodeValid)
{
	if (dataBlockAddress == 0)
	{
		return;
	}
	if (dataBlockAddress < FIRSTDATABLOCKNUM || dataBlockAddress > LASTDATABLOCKNUM)
	{
		printf("Error: Bad data block pointer. Address: %u. Out of valid data range.\n", dataBlockAddress);
		return;
	}
	referencedByAnyInode[dataBlockAddress] = 1;
	if (isCurrentInodeValid)
	{
		referencedByValidInode[dataBlockAddress] = 1;
	}
}

// ? ############################## PROCESS INDIRECT POINTERS ##############################

void processIndirectBPointers(int fd, uint32_t inodeNum, uint32_t indirectBlockAddress, int level, int isCurrentInodeValid)
{
	if (indirectBlockAddress == 0)
	{
		return;
	}
	if (indirectBlockAddress < FIRSTDATABLOCKNUM || indirectBlockAddress > LASTDATABLOCKNUM)
	{
		printf("Error: Bad data block pointer. Address: %u. Out of valid data range.\n", indirectBlockAddress);
		return;
	}

	uint32_t *pointers = (uint32_t *)malloc(BLOCKSIZE);
	readBlock(fd, indirectBlockAddress, (unsigned char *)pointers);
	for (int i = 0; i < POINTERSPBLOCK; i++)
	{
		uint32_t nextAddress = pointers[i];
		if (nextAddress == 0)
		{
			continue;
		}
		if (level == 1)
		{
			markDataBlockReference(inodeNum, nextAddress, isCurrentInodeValid);
		}
		else
		{
			processIndirectBPointers(fd, inodeNum, nextAddress, level - 1, isCurrentInodeValid);
		}
	}
	free(pointers);
}

// ? ############################## COLLECT BLOCKS FOR INODE ##############################

void collectBlocksForInode(int fd, uint32_t inodeNum, Inode *currentInode)
{
	int isInodeValid = (currentInode->numHardLinks > 0 && currentInode->deletionTime == 0);
	if (currentInode->numDataBlocksAllocated == 0 && !isInodeValid)
	{
		return;
	}

	for (int i = 0; i < 12; i++)
	{
		markDataBlockReference(inodeNum, currentInode->directPointer[i], isInodeValid);
	}

	if (currentInode->singleIndirectPointer != 0)
	{
		processIndirectBPointers(fd, inodeNum, currentInode->singleIndirectPointer, 1, isInodeValid);
	}
	if (currentInode->doubleIndirectPointer != 0)
	{
		processIndirectBPointers(fd, inodeNum, currentInode->doubleIndirectPointer, 2, isInodeValid);
	}
	if (currentInode->tripleIndirectPointer != 0)
	{
		processIndirectBPointers(fd, inodeNum, currentInode->tripleIndirectPointer, 3, isInodeValid);
	}
}

// ? ############################## VALIDATE DATA BITMAP ##############################

int validateDataBitmap(char *image)
{
	int fd = open(image, O_RDONLY);
	Superblock *sbPTR = (Superblock *)malloc(sizeof(Superblock));
	readBlock(fd, SUPERBLOCKNUM, (unsigned char *)sbPTR);
	printf("Validating Data Bitmap\n");
	printf("---------------------------------\n");

	int error = 0;

	unsigned char dataBitmap[BLOCKSIZE];
	readBlock(fd, sbPTR->dbimBlock, dataBitmap);

	unsigned char blockBuffer[BLOCKSIZE]; // This is apparently the inode bitmap block
	readBlock(fd, sbPTR->ibimBlock, blockBuffer);
	Inode *currentInodePTR;
	uint32_t inodesPerBlock = sbPTR->blockSize / sbPTR->inodeSize;

	for (uint32_t i = 0; i < INODETABNUMBLOCKS; i++)
	{
		uint32_t currentInodeTableBlockNum = sbPTR->itabStartBlock + i;
		readBlock(fd, currentInodeTableBlockNum, blockBuffer);
		for (uint32_t j = 0; j < inodesPerBlock; j++)
		{
			uint32_t currentInodeNum = (i * inodesPerBlock) + j;
			currentInodePTR = (Inode *)((blockBuffer + (j * sbPTR->inodeSize)));
			collectBlocksForInode(fd, currentInodeNum, currentInodePTR);
		}
	}

	printf("Checking Rule A: Bitmap used and referenced by valid inode\n");
	for (uint32_t i = 0; i < NUMDATABLOCKSFS; i++)
	{
		uint32_t actualBlockNum = sbPTR->firstDataBlock + i;
		if (bitCheck(dataBitmap, i))
		{
			if (!referencedByValidInode[actualBlockNum])
			{
				printf("Error Rule a: Block %u (bitmap bit %u) is Used in bitmap, but not referenced by any valid inode.\n", actualBlockNum, i);
				error++;
			}
		}
	}

	printf("Checking Rule B: Referenced by any inode and bitmap used\n");
	for (uint32_t i = sbPTR->firstDataBlock; i <= LASTDATABLOCKNUM; i++)
	{
		if (referencedByAnyInode[i])
		{
			int bitmapBitIndex = i - sbPTR->firstDataBlock;
			if (!bitCheck(dataBitmap, bitmapBitIndex))
			{
				printf("Error Rule b: Block %u (bitmap bit %u) is referenced by an inode, but not marked used in data bitmap.\n", i, bitmapBitIndex);
				error++;
			}
		}
	}
	printf("---------------------------------\n");
	free(sbPTR);
	close(fd);
	return error;
}

// ? ############################## FIX DATA BITMAP ##############################

void fixDataBitmap(char *image)
{
	int fd = open(image, O_RDWR);
	Superblock *sbPTR = (Superblock *)malloc(sizeof(Superblock));
	readBlock(fd, SUPERBLOCKNUM, (unsigned char *)sbPTR);

	unsigned char dataBitmap[BLOCKSIZE];
	readBlock(fd, sbPTR->dbimBlock, dataBitmap);

	for (uint32_t i = 0; i < NUMDATABLOCKSFS; i++)
	{
		uint32_t actualBlockNum = sbPTR->firstDataBlock + i;
		if (bitCheck(dataBitmap, i))
		{
			if (!referencedByValidInode[actualBlockNum])
			{
				removeBit(dataBitmap, i);
			}
		}
		else
		{
			if (referencedByAnyInode[actualBlockNum])
			{
				setBit(dataBitmap, i);
			}
		}
	}

	writeBlock(fd, sbPTR->dbimBlock, dataBitmap);
	free(sbPTR);
	close(fd);
	printf("Fixed all the errors regarding Data Bitmap. Please rerun the checker to ensure!\n");
}

// ! ############################## Al- Saihan Tajvi ##############################

// ? ############################## VALIDATE INODE BITMAP ##############################

int validateInodeBitmap(char *image)
{
	int fd = open(image, O_RDONLY);
	Superblock *sbPTR = (Superblock *)malloc(sizeof(Superblock));
	readBlock(fd, SUPERBLOCKNUM, (unsigned char *)sbPTR);
	printf("Validating Inode Bitmap\n");
	printf("---------------------------------\n");

	int error = 0;

	unsigned char inodeBitmap[BLOCKSIZE];
	readBlock(fd, sbPTR->ibimBlock, inodeBitmap);

	unsigned char blockBuffer[BLOCKSIZE];
	Inode *currentInodePTR;
	uint32_t inodesPerBlock = sbPTR->blockSize / sbPTR->inodeSize;

	printf("Check Rule A: Each bit set in the inode bitmap corresponds to a valid inode\n");
	printf("Check Rule B: Every such inode is marked as used in the bitmap\n");
	for (uint32_t i = 0; i < INODETABNUMBLOCKS; i++)
	{
		uint32_t currentInodeTableBlockNum = sbPTR->itabStartBlock + i;
		readBlock(fd, currentInodeTableBlockNum, blockBuffer);

		for (uint32_t j = 0; j < inodesPerBlock; j++)
		{
			uint32_t currentInodeNum = (i * inodesPerBlock) + j;
			currentInodePTR = (Inode *)((blockBuffer + (j * sbPTR->inodeSize)));

			int isInodeValid = (currentInodePTR->numHardLinks > 0 && currentInodePTR->deletionTime == 0);
			int isMarkedInBitmap = bitCheck(inodeBitmap, currentInodeNum);

			if (isMarkedInBitmap && !isInodeValid)
			{
				printf("Error: Inode %u is marked in bitmap but invalid (links=%u, del_time=%u)\n",
					   currentInodeNum, currentInodePTR->numHardLinks, currentInodePTR->deletionTime);
				error++;
			}

			if (isInodeValid && !isMarkedInBitmap)
			{
				printf("Error: Valid inode %u (links=%u) not marked in bitmap\n",
					   currentInodeNum, currentInodePTR->numHardLinks);
				error++;
			}
		}
	}

	printf("---------------------------------\n");
	free(sbPTR);
	close(fd);
	return error;
}

// ? ############################## FIX INODE BITMAP ##############################

void fixInodeBitmap(char *image)
{
	int fd = open(image, O_RDWR);
	Superblock *sbPTR = (Superblock *)malloc(sizeof(Superblock));
	readBlock(fd, SUPERBLOCKNUM, (unsigned char *)sbPTR);

	unsigned char inodeBitmap[BLOCKSIZE];
	readBlock(fd, sbPTR->ibimBlock, inodeBitmap);

	// Scan all inodes to fix both types of errors
	unsigned char blockBuffer[BLOCKSIZE];
	Inode *currentInodePTR;
	uint32_t inodesPerBlock = sbPTR->blockSize / sbPTR->inodeSize;

	for (uint32_t i = 0; i < INODETABNUMBLOCKS; i++)
	{
		uint32_t currentInodeTableBlockNum = sbPTR->itabStartBlock + i;
		readBlock(fd, currentInodeTableBlockNum, blockBuffer);

		for (uint32_t j = 0; j < inodesPerBlock; j++)
		{
			uint32_t currentInodeNum = (i * inodesPerBlock) + j;
			currentInodePTR = (Inode *)((blockBuffer + (j * sbPTR->inodeSize)));

			int isInodeValid = (currentInodePTR->numHardLinks > 0 && currentInodePTR->deletionTime == 0);
			int isMarkedInBitmap = bitCheck(inodeBitmap, currentInodeNum);

			// Fix Rule a:
			if (isMarkedInBitmap && !isInodeValid)
			{
				removeBit(inodeBitmap, currentInodeNum);
			}

			// Fix Rule b:
			if (isInodeValid && !isMarkedInBitmap)
			{
				setBit(inodeBitmap, currentInodeNum);
			}
		}
	}

	// final writing of the inode bitmap
	writeBlock(fd, sbPTR->ibimBlock, inodeBitmap);

	free(sbPTR);
	close(fd);
	printf("Fixed all inode bitmap errors. Please rerun the checker to verify.\n");
}

// ? ############################## BAD BLOCK CHECKER + FIXER ##############################

int validateAndFixBlockPointers(char *image)
{
	int fd = open(image, O_RDWR); // Need read-write for fixing
	Superblock *sbPTR = (Superblock *)malloc(sizeof(Superblock));
	readBlock(fd, SUPERBLOCKNUM, (unsigned char *)sbPTR);

	printf("Checking and fixing bad block pointers\n");
	printf("---------------------------------\n");

	int error = 0;
	int fixed = 0;
	unsigned char blockBuffer[BLOCKSIZE];
	Inode *currentInodePTR;
	uint32_t inodesPerBlock = sbPTR->blockSize / sbPTR->inodeSize;

	// Scan all inodes in the inode table
	for (uint32_t i = 0; i < INODETABNUMBLOCKS; i++)
	{
		uint32_t currentInodeTableBlockNum = sbPTR->itabStartBlock + i;
		readBlock(fd, currentInodeTableBlockNum, blockBuffer);

		for (uint32_t j = 0; j < inodesPerBlock; j++)
		{
			uint32_t currentInodeNum = (i * inodesPerBlock) + j;
			currentInodePTR = (Inode *)((blockBuffer + (j * sbPTR->inodeSize)));
			int inodeModified = 0;

			// Check direct pointers
			for (int k = 0; k < 12; k++)
			{
				uint32_t blockNum = currentInodePTR->directPointer[k];
				if (blockNum != 0 && (blockNum < FIRSTDATABLOCKNUM || blockNum > LASTDATABLOCKNUM))
				{
					printf("Error: Inode %u has bad direct pointer %u (block %u). Fixing by nulling pointer.\n",
						   currentInodeNum, k, blockNum);
					currentInodePTR->directPointer[k] = 0;
					inodeModified = 1;
					error++;
					fixed++;
				}
			}

			// Check single indirect pointer
			if (currentInodePTR->singleIndirectPointer != 0)
			{
				if (currentInodePTR->singleIndirectPointer < FIRSTDATABLOCKNUM ||
					currentInodePTR->singleIndirectPointer > LASTDATABLOCKNUM)
				{
					printf("Error: Inode %u has bad single indirect pointer (block %u). Fixing by nulling pointer.\n",
						   currentInodeNum, currentInodePTR->singleIndirectPointer);
					currentInodePTR->singleIndirectPointer = 0;
					inodeModified = 1;
					error++;
					fixed++;
				}
				else
				{
					// Check pointers in the indirect block
					uint32_t indirectBlock[POINTERSPBLOCK];
					readBlock(fd, currentInodePTR->singleIndirectPointer, (unsigned char *)indirectBlock);
					int blockModified = 0;

					for (int k = 0; k < POINTERSPBLOCK; k++)
					{
						if (indirectBlock[k] != 0 &&
							(indirectBlock[k] < FIRSTDATABLOCKNUM || indirectBlock[k] > LASTDATABLOCKNUM))
						{
							printf("Error: Inode %u has bad single-indirect pointer %u (block %u). Fixing by nulling pointer.\n",
								   currentInodeNum, k, indirectBlock[k]);
							indirectBlock[k] = 0;
							blockModified = 1;
							error++;
							fixed++;
						}
					}

					if (blockModified)
					{
						writeBlock(fd, currentInodePTR->singleIndirectPointer, (unsigned char *)indirectBlock);
					}
				}
			}

			// ? Check double indirect pointer
			if (currentInodePTR->doubleIndirectPointer != 0)
			{
				if (currentInodePTR->doubleIndirectPointer < FIRSTDATABLOCKNUM ||
					currentInodePTR->doubleIndirectPointer > LASTDATABLOCKNUM)
				{
					printf("Error: Inode %u has bad double indirect pointer (block %u). Fixing by nulling pointer.\n",
						   currentInodeNum, currentInodePTR->doubleIndirectPointer);
					currentInodePTR->doubleIndirectPointer = 0;
					inodeModified = 1;
					error++;
					fixed++;
				}
				else
				{
					// ? Check second level pointers
					uint32_t firstLevel[POINTERSPBLOCK];
					readBlock(fd, currentInodePTR->doubleIndirectPointer, (unsigned char *)firstLevel);
					int firstLevelModified = 0;

					for (int k = 0; k < POINTERSPBLOCK; k++)
					{
						if (firstLevel[k] != 0)
						{
							if (firstLevel[k] < FIRSTDATABLOCKNUM || firstLevel[k] > LASTDATABLOCKNUM)
							{
								printf("Error: Inode %u has bad double-indirect first-level pointer %u (block %u). Fixing by nulling pointer.\n",
									   currentInodeNum, k, firstLevel[k]);
								firstLevel[k] = 0;
								firstLevelModified = 1;
								error++;
								fixed++;
							}
							else
							{
								// ? Check third level pointers
								uint32_t secondLevel[POINTERSPBLOCK];
								readBlock(fd, firstLevel[k], (unsigned char *)secondLevel);
								int secondLevelModified = 0;

								for (int l = 0; l < POINTERSPBLOCK; l++)
								{
									if (secondLevel[l] != 0 &&
										(secondLevel[l] < FIRSTDATABLOCKNUM || secondLevel[l] > LASTDATABLOCKNUM))
									{
										printf("Error: Inode %u has bad double-indirect second-level pointer %u (block %u). Fixing by nulling pointer.\n",
											   currentInodeNum, l, secondLevel[l]);
										secondLevel[l] = 0;
										secondLevelModified = 1;
										error++;
										fixed++;
									}
								}

								if (secondLevelModified)
								{
									writeBlock(fd, firstLevel[k], (unsigned char *)secondLevel);
								}
							}
						}
					}

					if (firstLevelModified)
					{
						writeBlock(fd, currentInodePTR->doubleIndirectPointer, (unsigned char *)firstLevel);
					}
				}
			}

			// ? Check triple indirect pointer
			if (currentInodePTR->tripleIndirectPointer != 0)
			{
				if (currentInodePTR->tripleIndirectPointer < FIRSTDATABLOCKNUM ||
					currentInodePTR->tripleIndirectPointer > LASTDATABLOCKNUM)
				{
					printf("Error: Inode %u has bad triple indirect pointer (block %u). Fixing by nulling pointer.\n",
						   currentInodeNum, currentInodePTR->tripleIndirectPointer);
					currentInodePTR->tripleIndirectPointer = 0;
					inodeModified = 1;
					error++;
					fixed++;
				}
				else
				{
					// ? Check second level pointers
					uint32_t firstLevel[POINTERSPBLOCK];
					readBlock(fd, currentInodePTR->tripleIndirectPointer, (unsigned char *)firstLevel);
					int firstLevelModified = 0;

					for (int k = 0; k < POINTERSPBLOCK; k++)
					{
						if (firstLevel[k] != 0)
						{
							if (firstLevel[k] < FIRSTDATABLOCKNUM || firstLevel[k] > LASTDATABLOCKNUM)
							{
								printf("Error: Inode %u has bad triple-indirect first-level pointer %u (block %u). Fixing by nulling pointer.\n",
									   currentInodeNum, k, firstLevel[k]);
								firstLevel[k] = 0;
								firstLevelModified = 1;
								error++;
								fixed++;
							}
							else
							{
								// ? Check third level pointers
								uint32_t secondLevel[POINTERSPBLOCK];
								readBlock(fd, firstLevel[k], (unsigned char *)secondLevel);
								int secondLevelModified = 0;

								for (int l = 0; l < POINTERSPBLOCK; l++)
								{
									if (secondLevel[l] != 0)
									{
										if (secondLevel[l] < FIRSTDATABLOCKNUM || secondLevel[l] > LASTDATABLOCKNUM)
										{
											printf("Error: Inode %u has bad triple-indirect second-level pointer %u (block %u). Fixing by nulling pointer.\n",
												   currentInodeNum, l, secondLevel[l]);
											secondLevel[l] = 0;
											secondLevelModified = 1;
											error++;
											fixed++;
										}
										else
										{
											// ? Check fourth level pointers
											uint32_t thirdLevel[POINTERSPBLOCK];
											readBlock(fd, secondLevel[l], (unsigned char *)thirdLevel);
											int thirdLevelModified = 0;

											for (int m = 0; m < POINTERSPBLOCK; m++)
											{
												if (thirdLevel[m] != 0 &&
													(thirdLevel[m] < FIRSTDATABLOCKNUM || thirdLevel[m] > LASTDATABLOCKNUM))
												{
													printf("Error: Inode %u has bad triple-indirect third-level pointer %u (block %u). Fixing by nulling pointer.\n",
														   currentInodeNum, m, thirdLevel[m]);
													thirdLevel[m] = 0;
													thirdLevelModified = 1;
													error++;
													fixed++;
												}
											}

											if (thirdLevelModified)
											{
												writeBlock(fd, secondLevel[l], (unsigned char *)thirdLevel);
											}
										}
									}
								}

								if (secondLevelModified)
								{
									writeBlock(fd, firstLevel[k], (unsigned char *)secondLevel);
								}
							}
						}
					}

					if (firstLevelModified)
					{
						writeBlock(fd, currentInodePTR->tripleIndirectPointer, (unsigned char *)firstLevel);
					}
				}
			}

			if (inodeModified)
			{
				// ? Write back the modified inode
				writeBlock(fd, currentInodeTableBlockNum, blockBuffer);
			}
		}
	}

	printf("Found %d bad block pointers, fixed %d\n", error, fixed);
	printf("---------------------------------\n");
	free(sbPTR);
	close(fd);
	return error;
}

// ! ############################## Sadik Mina Dweep ##############################
// ? ############################## DUPLICATE BLOCK DETECTOR ##############################

int detectDuplicateBlocks(char *image)
{
	int fd = open(image, O_RDONLY);
	Superblock *sbPTR = (Superblock *)malloc(sizeof(Superblock));
	readBlock(fd, SUPERBLOCKNUM, (unsigned char *)sbPTR);

	printf("Checking for duplicate blocks\n");
	printf("---------------------------------\n");

	int error = 0;
	unsigned char blockBuffer[BLOCKSIZE];
	Inode *currentInodePTR;
	uint32_t inodesPerBlock = sbPTR->blockSize / sbPTR->inodeSize;

	// Create a reference count array for all blocks
	int *blockRefCount = calloc(TOTALBLOCKS, sizeof(int));

	// First pass: Count references to each block
	for (uint32_t i = 0; i < INODETABNUMBLOCKS; i++)
	{
		uint32_t currentInodeTableBlockNum = sbPTR->itabStartBlock + i;
		readBlock(fd, currentInodeTableBlockNum, blockBuffer);

		for (uint32_t j = 0; j < inodesPerBlock; j++)
		{
			currentInodePTR = (Inode *)((blockBuffer + (j * sbPTR->inodeSize)));

			// Skip invalid inodes
			if (currentInodePTR->numHardLinks == 0 || currentInodePTR->deletionTime != 0)
			{
				continue;
			}

			// Check direct pointers
			for (int k = 0; k < 12; k++)
			{
				uint32_t blockNum = currentInodePTR->directPointer[k];
				if (blockNum != 0 && blockNum < TOTALBLOCKS)
				{
					blockRefCount[blockNum]++;
				}
			}

			// Process indirect pointers (single, double, triple)
			processIndirectReferences(fd, currentInodePTR->singleIndirectPointer, 1, blockRefCount);
			processIndirectReferences(fd, currentInodePTR->doubleIndirectPointer, 2, blockRefCount);
			processIndirectReferences(fd, currentInodePTR->tripleIndirectPointer, 3, blockRefCount);
		}
	}

	// Second pass: Report duplicates
	for (uint32_t blockNum = FIRSTDATABLOCKNUM; blockNum <= LASTDATABLOCKNUM; blockNum++)
	{
		if (blockRefCount[blockNum] > 1)
		{
			printf("Error: Block %u is referenced %d times (duplicate)\n",
				   blockNum, blockRefCount[blockNum]);
			error++;
		}
	}

	printf("---------------------------------\n");
	printf("Found %d duplicate block references\n", error);
	printf("---------------------------------\n");

	free(blockRefCount);
	free(sbPTR);
	close(fd);
	return error;
}

// Helper function to process indirect references recursively
void processIndirectReferences(int fd, uint32_t indirectBlock, int level, int *blockRefCount)
{
	if (indirectBlock == 0 || indirectBlock >= TOTALBLOCKS)
	{
		return;
	}

	uint32_t pointers[POINTERSPBLOCK];
	readBlock(fd, indirectBlock, (unsigned char *)pointers);

	for (int i = 0; i < POINTERSPBLOCK; i++)
	{
		uint32_t blockNum = pointers[i];
		if (blockNum == 0)
		{
			continue;
		}

		if (level == 1)
		{
			// This is a data block
			if (blockNum < TOTALBLOCKS)
			{
				blockRefCount[blockNum]++;
			}
		}
		else
		{
			// This is another level of indirection
			if (blockNum < TOTALBLOCKS)
			{
				blockRefCount[blockNum]++; // Count the indirect block itself
				processIndirectReferences(fd, blockNum, level - 1, blockRefCount);
			}
		}
	}
}