#include <stdio.h>
#include "ext2_fs.h"
#include "read_ext2.h"
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("expected usage: ./runscan inputfile outputfile\n");
		exit(0);
	}
	if (opendir(argv[2]))
	{
		printf("ERROR - exited file\n");
		exit(1);
	}
	int input = open(argv[1], O_RDONLY);
	mkdir(argv[2], 448);
	ext2_read_init(input);
	struct ext2_super_block super;
	struct ext2_group_desc group;
	read_group_desc(input, 0, &group);
	read_super_block(input, 0, &super);
	int inode_is_jpg[inodes_per_group];
	uint q = 0;
	while (q < inodes_per_group)
	{
		inode_is_jpg[q] = 0; // set all to zero
		q++;
	}
	off_t start_inode_table = locate_inode_table(0, &group);
	for (unsigned int i = 0; i < inodes_per_group; i++)
	{
		struct ext2_inode *inode = malloc(128);
		read_inode(input, 0, start_inode_table, i, inode);
		/* the maximum index of the i_block array should be computed from i_blocks / ((1024<<s_log_block_size)/512)
		 * or once simplified, i_blocks/(2<<s_log_block_size)
		 * https://www.nongnu.org/ext2-doc/ext2.html#i-blocks
		 */
		char buf[block_size];
		lseek(input, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
		read(input, buf, block_size);
		int is_jpg = 0;
		if (buf[0] == (char)0xff &&
			buf[1] == (char)0xd8 &&
			buf[2] == (char)0xff &&
			(buf[3] == (char)0xe0 ||
			 buf[3] == (char)0xe1 ||
			 buf[3] == (char)0xe8))
		{
			is_jpg = 1;
		}

		if (is_jpg == 1)
		{
			int digitN = 1;
			int ct = i; // counter
			ct = ct / 10;
			while (ct != 0)
			{
				digitN++;
				ct = ct / 10;
			}
			size_t charsSize = (strlen(argv[2]) + strlen("/") + strlen("file-") + digitN + strlen(".jpg") + 1);
			// new file
			char *fn = malloc(charsSize);
			snprintf(fn, charsSize, "%s/file-%u.jpg", argv[2], i);
			int faddr = open(fn, O_WRONLY | O_TRUNC | O_CREAT, 438);
			size_t charsSize2 = (strlen(argv[2]) + strlen("/") + strlen("file-") + digitN + strlen("ex.jpg") + 1);
			char *fn2 = malloc(charsSize2);
			snprintf(fn2, charsSize2, "%s/file-%uex.jpg", argv[2], i);
			int new_faddr = open(fn2, O_WRONLY | O_TRUNC | O_CREAT, 438);

			uint size = inode->i_size; // size of file in inode

			uint block_count = 0;
			uint read_count = 0;
			while (1)
			{
				if (read_count >= size || read_count >= block_size * EXT2_NDIR_BLOCKS)
				{
					break;
				}
				uint j = 0;
				while (1)
				{ // calculate size
					if (j == block_size || j == size - read_count)
					{
						break;
					}
					else
					{
						j++;
					}
				}
				write(new_faddr, buf, j);
				write(faddr, buf, j);
				if (j < size - read_count)
				{
					block_count++;
					lseek(input, BLOCK_OFFSET(inode->i_block[block_count]), SEEK_SET);
					read(input, buf, block_size);
				}
				read_count += block_size;
			}
			if (size > read_count)
			{
				uint ind_buf[block_size / 4];
				lseek(input, BLOCK_OFFSET(inode->i_block[EXT2_IND_BLOCK]), SEEK_SET);
				read(input, ind_buf, block_size);
				lseek(input, BLOCK_OFFSET(ind_buf[0]), SEEK_SET);
				read(input, buf, block_size);
				block_count = 0;
				while (1)
				{
					if (read_count >= size || block_count >= (block_size / 4))
					{
						break;
					}
					uint j = 0;
					while (1)
					{ // calculate size
						if (j == block_size || j == size - read_count)
						{
							break;
						}
						else
						{
							j++;
						}
					}
					write(faddr, buf, j);
					write(new_faddr, buf, j);
					if (j < size - read_count)
					{
						block_count++;
						lseek(input, BLOCK_OFFSET(ind_buf[block_count]), SEEK_SET);
						read(input, buf, block_size);
					}
					read_count += block_size;
				}
			}
			if (size > read_count)
			{
				uint buf1[block_size / 4];
				uint buf2[block_size / 4];
				lseek(input, BLOCK_OFFSET(inode->i_block[EXT2_DIND_BLOCK]), SEEK_SET);
				read(input, buf1, block_size);
				uint n = 0; // the id of first layer
				while (1)
				{
					if (n >= (block_size / 4) || read_count >= size)
					{
						break;
					}
					lseek(input, BLOCK_OFFSET(buf1[n]), SEEK_SET);
					read(input, buf2, block_size);
					lseek(input, BLOCK_OFFSET(buf2[0]), SEEK_SET);
					read(input, buf, block_size);
					block_count = 0;
					while (1)
					{
						if (read_count >= size || block_count >= (block_size / 4))
						{
							break;
						}
						uint j = 0;
						while (1)
						{ // calculate size
							if (j == block_size || j == size - read_count)
							{
								break;
							}
							else
							{
								j++;
							}
						}
						write(new_faddr, buf, j);
						write(faddr, buf, j);
						if (j < size - read_count)
						{
							block_count++;
							lseek(input, BLOCK_OFFSET(buf2[block_count]), SEEK_SET);
							read(input, buf, block_size);
						}
						read_count += block_size;
					}
					n++;
				}
			}
			inode_is_jpg[i] = 64;
		}
		free(inode);
	}
	start_inode_table = locate_inode_table(0, &group);
	for (unsigned int i = 0; i < inodes_per_group; i++)
	{
		struct ext2_inode *inode = malloc(128);
		read_inode(input, 0, start_inode_table, i, inode);
		if (S_ISDIR(inode->i_mode)) // is directory
		{
			char dir_buf[block_size];
			lseek(input, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
			read(input, dir_buf, block_size);
			uint cur = 24;
			struct ext2_dir_entry_2 *dir_entry = (struct ext2_dir_entry_2 *)&(dir_buf[cur]);
			while (cur < block_size)
			{
				int charsSize = dir_entry->name_len & 0xFF;
				// double bytes
				if (charsSize <= 0)
				{
					break;
				}
				char name[255];
				strncpy(name, dir_entry->name, charsSize);
				name[charsSize] = '\0';
				struct ext2_inode *cur_i = malloc(128);
				read_inode(input, 0, start_inode_table, i, cur_i);
				if (inode_is_jpg[dir_entry->inode] == 64)
				{
					int digitN = 1; // rename
					int ct = dir_entry->inode;
					while (1)
					{
						if (ct < 10)
						{
							break;
						}
						digitN++;
						ct = ct / 10;
					}
					int dir_size = (strlen(argv[2]) + strlen("/"));
					size_t old_size = (dir_size + strlen("file-") + digitN + strlen("ex.jpg") + 1);
					char *former = malloc(old_size);
					snprintf(former, old_size, "%s/file-%uex.jpg", argv[2], dir_entry->inode);
					size_t now_chars = dir_size + dir_entry->name_len + 1;
					char *now = malloc(now_chars);
					snprintf(now, now_chars, "%s/%s", argv[2], dir_entry->name);
					rename(former, now);
				}
				cur += charsSize + 8;
				if (cur % 4 != 0)
				{
					cur += (4 - (cur % 4));
				}
				dir_entry = (struct ext2_dir_entry_2 *)&(dir_buf[cur]);
			}
		}
	}
}