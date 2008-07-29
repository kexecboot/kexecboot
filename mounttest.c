#include <stdio.h>
#include <sys/mount.h>
#include <linux/fs.h>
int main() {
	mount("/dev/sdd1", "/mnt", "ext2", 0, NULL);
	return 0;
}
