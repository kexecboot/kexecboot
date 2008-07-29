#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "fstype.h"
#include <sys/mount.h>
#include <linux/fs.h>

// lame search
int find(const char *what,const char **where, int size)
{
	int i;
	for(i=0; i<size; i++)
		if(!strcmp(what, where[i]))
			return i;
	return -1;
}

struct boot
{
	char *device;
	char *fstype;
	char *kernelpath;
	char *cmdline;
};

int main()
{
	char line[80];
	char *split;

	unsigned int fssize = 8;
	char * *fslist = malloc(fssize * sizeof(char*));
	unsigned int fscount = 0;
	
	unsigned int bootsize = 4;
	struct boot **bootlist = malloc(bootsize * sizeof(struct boot));
	unsigned int bootcount = 0;

	int i,ret;
	
	// make a list of all known filesystems
	FILE * f = fopen("/proc/filesystems","r");
	if(!f) {
		perror("/proc/filesystems");
		return -1;
	}
	while(fgets(line,80,f)){
		line[strlen(line)-1] = '\0';
		split = strchr(line,'\t');
		split++;
		fslist[fscount] = malloc(strlen(split)+1);
		strcpy(fslist[fscount], split);
		fscount++;
		if(fscount == fssize) {
			fssize *= 2;
			fslist = realloc(fslist, fssize*sizeof(char*));
		}
		printf("filesystem supported: %s\n",split);
	}
	fclose(f);
	// get list of bootable filesystems	
	f = fopen("/proc/partitions","r");
	if(!f) {
		perror("/proc/partitions");
		return -2;
	}
	// first two lines are bogus
	fgets(line,80,f);
	fgets(line,80,f);
	while(fgets(line,80,f)){
		char *device;
		const char *fstype;
		const char *kernelpath;
		char *cmdline;
		line[strlen(line)-1] = '\0';
		split = line + strspn(line," 01234567789");
		device = malloc( (strlen(split)+strlen("/dev/")+1) * sizeof(char));
		sprintf(device,"/dev/%s",split);
		printf("%s\n",device);
		int fd = open(device, O_RDONLY);
		if(fd < 0) {
			perror(device);
			free(device);
			continue;
		}
		if( -1 == identify_fs(fd, &fstype, NULL, 0)) {
			free(device);
			continue;
		}
		close(fd);
		if(!fstype){
			free(device);
			continue;
		 }
		// no unknown filesystems
		if(find(fstype, fslist, fscount) == -1) {
			free(device);
			continue;
		}
		printf("found %s (%s)\n",device, fstype);
		// mount fs
		if(mount(device, "/mnt", fstype, MS_RDONLY, NULL)){
			printf("mount failed\n");
			perror(device);
			free(device);
			continue;
		}
		if( fd = open("/mnt/zImage",O_RDONLY) >= 0) {
			kernelpath = "/mnt/zImage";
			close(fd);
		} else if( fd = open("/mnt/boot/zImage", O_RDONLY) >= 0) {
			kernelpath = "/mnt/boot/zImage";
			close(fd);
		} else {
			free(device);
			umount("/mnt");
			continue;
		}
		printf("found kernel\n");


		umount("/mnt");

	}
	fclose(f);
	return 0;
}
