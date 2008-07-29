OBJS = fstype.o main.o
tzbl: $(OBJS)
	$(CC) -o tzbl $(OBJS)
main.o: main.c fstype.h
fstype.o: fstype.c cramfs_fs.h ext2_fs.h ext3_fs.h iso9660_sb.h jfs_superblock.h luks_fs.h lvm2_sb.h minix_fs.h reiserfs_fs.h romfs_fs.h swap_fs.h xfs_sb.h
clean:
	-rm *.o tzbl
