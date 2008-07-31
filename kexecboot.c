/* 
 *  kexecboot - A kexec based bootloader 
 *
 *      Copyright (c) 2008 Thomas Kunze <thommycheck@gmx.de>
 *  small parts:
 *  	Copyright (c) 2006 Matthew Allum <mallum@o-hand.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "kexecboot.h"

void display_slot(FB *fb, struct boot * boot,int slot, int height, int iscurrent)
{
	int margin = fb->width/64;
	char text[100];
	if(!iscurrent)
		fb_draw_rect(fb, 0, slot*height, fb->width, height,
			0xec, 0xec, 0xe1);
	else { //draw red border
		fb_draw_rect(fb, 0, slot*height, fb->width, height,
			0xff, 0x00, 0x00);
		fb_draw_rect(fb, margin, slot*height+margin, fb->width-2*margin
			, height-2*margin, 0xec, 0xec, 0xe1);
	}
	if(!strncmp(boot->device,"/dev/hd",strlen("/dev/hd")))
		fb_draw_image(fb, 2*margin, slot*height+2*margin, CF_IMG_WIDTH, 
			CF_IMG_HEIGHT, CF_IMG_BYTES_PER_PIXEL, CF_IMG_RLE_PIXEL_DATA);
	else if(!strncmp(boot->device,"/dev/mmcblk",strlen("/dev/mmcblk")))
		fb_draw_image(fb, 2*margin, slot*height + 2*margin, MMC_IMG_WIDTH,
			MMC_IMG_HEIGHT, MMC_IMG_BYTES_PER_PIXEL, MMC_IMG_RLE_PIXEL_DATA);
	else if(!strncmp(boot->device,"/dev/mtdblock",strlen("/dev/mtdblock")))
		fb_draw_image(fb, 2*margin, slot*height + 2*margin, MEMORY_IMG_WIDTH,
			MEMORY_IMG_HEIGHT, MEMORY_IMG_BYTES_PER_PIXEL, MEMORY_IMG_RLE_PIXEL_DATA);
	sprintf(text,"%s (%s)",boot->device, boot->fstype);
	fb_draw_text (fb, CF_IMG_WIDTH+2*margin, slot*height+2*margin, 0, 0, 0, 
			&radeon_font, text);
			
}

void display_menu(FB *fb, struct bootlist *bl, int current)
{
	int i,j;
	int margin = fb->width/64;
	int slotheight = LOGO_IMG_HEIGHT;
	int slots = fb->height/slotheight -1;
	// struct boot that is in fist slot
	static int firstslot=0;
	char test[20];

	/* Clear the background with #ecece1 */
	fb_draw_rect(fb, 0, 0, fb->width, fb->height,0xec, 0xec, 0xe1);

	/* logo */
	fb_draw_image(fb,
		      0,
		      0,
		      LOGO_IMG_WIDTH,
		      LOGO_IMG_HEIGHT,
		      LOGO_IMG_BYTES_PER_PIXEL, LOGO_IMG_RLE_PIXEL_DATA);
	fb_draw_text (fb, LOGO_IMG_WIDTH + margin, margin, 0, 0, 0, &radeon_font,
			"Which device do\nyou want to boot\ntoday?");
	sprintf(test, "%d", current);
	fb_draw_text (fb, margin, margin, 0, 0, 0, &radeon_font,
			test);
	if(current < firstslot)
		firstslot=current;
	if(current > firstslot + slots -1)
		firstslot = current - (slots -1);
	for(i=1, j=firstslot; i <= slots; i++, j++){
		display_slot(fb, bl->list[j], i, slotheight, j == current);
	}
}

void start_kernel(struct boot *boot)
{
	//kexec --command-line="CMDLINE" -l /mnt/boot/zImage
	char command[COMMAND_LINE_SIZE + 50];
//	mount(boot->device, "/mnt", boot->fstype, MS_RDONLY, NULL);
	if( boot->cmdline )
		sprintf(command,"kexec --command-line=\"%s\" -l %s", 
			boot->cmdline, boot->kernelpath);
	else
		sprintf(command,"kexec -l %s", boot->kernelpath);
//	system(command);
	puts(command);
//	system("kexec -e");
//	umount("/mnt");
}

int main(int argc, char **argv)
{
	int i = 0, angle = 0, choice = 0;
	FB *fb;
	struct bootlist * bl;
	char* eventif = "/dev/input/event0";
	struct input_event evt;
	struct termios old, new;

	while (++i < argc) {
		if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--angle")) {
			if (++i > argc)
				goto fail;
			angle = atoi(argv[i]);
			continue;
		}

		if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--input")) {
			if (++i > argc)
				goto fail;
			eventif = argv[i];
			continue;
		}

	      fail:
		fprintf(stderr,	"Usage: %s [-a|--angle <0|90|180|270>] \
			[-i|--input </dev/input/eventX>\n",
			argv[0]);
		exit(-1);
	}

	if ((fb = fb_new(angle)) == NULL)
		exit(-1);

	//bl = scan_devices();
	bl = malloc(sizeof(struct bootlist));
	bl->size = 8;
	bl->list = malloc(8 * sizeof(struct boot*));
	
	bl->list[0] = malloc(sizeof(struct boot));
	bl->list[0]->device = "/dev/mtdblock2";
	bl->list[0]->fstype = "jffs2";
	bl->list[0]->kernelpath = "/boot/zImage";
	bl->list[0]->cmdline = "cmdline0";
	
	bl->list[1] = malloc(sizeof(struct boot));
	bl->list[1]->device = "/dev/mmcblk0p1";
	bl->list[1]->fstype = "ext2";
	bl->list[1]->kernelpath = "/boot/zImage";
	bl->list[1]->cmdline = "cmdline1";
	
	bl->list[2] = malloc(sizeof(struct boot));
	bl->list[2]->device = "/dev/hda1";
	bl->list[2]->fstype = "ext3";
	bl->list[2]->kernelpath = "/boot/zImage";
	bl->list[2]->cmdline = "cmdline2";
	
	bl->list[3] = malloc(sizeof(struct boot));
	bl->list[3]->device = "/dev/mmcblk1p2";
	bl->list[3]->fstype = "ext2";
	bl->list[3]->kernelpath = "/boot/zImage";
	bl->list[3]->cmdline = "cmdline3";
	
	bl->list[4] = malloc(sizeof(struct boot));
	bl->list[4]->device = "/dev/mtdblock2";
	bl->list[4]->fstype = "jffs2";
	bl->list[4]->kernelpath = "/boot/zImage";
	bl->list[4]->cmdline = NULL;
	
	bl->list[5] = malloc(sizeof(struct boot));
	bl->list[5]->device = "/dev/mmcblk0p1";
	bl->list[5]->fstype = "ext2";
	bl->list[5]->kernelpath = "/boot/zImage";
	bl->list[5]->cmdline = NULL;
	
	bl->list[6] = malloc(sizeof(struct boot));
	bl->list[6]->device = "/dev/hda1";
	bl->list[6]->fstype = "ext3";
	bl->list[6]->kernelpath = "/boot/zImage";
	bl->list[6]->cmdline = NULL;
	
	bl->list[7] = malloc(sizeof(struct boot));
	bl->list[7]->device = "/dev/mmcblk1p2";
	bl->list[7]->fstype = "ext2";
	bl->list[7]->kernelpath = "/boot/zImage";
	bl->list[7]->cmdline = NULL;
	
	FILE *f = fopen(eventif,"r");
	if(!f){
	    perror(eventif);
	    exit(3);
	}
	

	// deactivate terminal input

/*	tcgetattr(fileno(stdin), &old);
	new = old;
	new.c_lflag &= ~ECHO;
	new.c_cflag &=~CREAD;
	tcsetattr(fileno(stdin), TCSANOW, &new);
*/
	do{
		display_menu(fb, bl, choice);
		do
			fread(&evt, sizeof(struct input_event), 1, f);
		while(evt.type != EV_KEY || evt.value != 0);
				    
		if(evt.code == KEY_UP && choice >0)
			choice--;
		if(evt.code == KEY_DOWN && choice < bl->size-1)
			choice++;
		//printf("%d %d\n",choice, evt.code);
	    
	}while(evt.code != 87 && evt.code != 63 );
	fclose(f);
	// reset terminal
//	tcsetattr(fileno(stdin), TCSANOW, &old);
	fb_destroy(fb);
	start_kernel(bl->list[choice]);
	return 0;
}
