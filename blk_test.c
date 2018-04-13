/****************************************************************************************/
/* 											*/
/* Project	: Simple block device driver                                            */
/* File 	: blk_test.c                                                            */
/* Author	: Seungjae Baek (ibanez1383@gmail.com)                                  */
/* Company  	: Dankook Univ. Embedded System Lab.    				*/
/* Notes        : ~/drivers/block/rd.c의 내용을 바탕으로 				*/
/* 		만든 간단한 가상 블록 디바이스 드라이버					*/
/* Date         : 2008/07/06								*/
/*                                                                                      */
/****************************************************************************************/
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/atomic.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/buffer_head.h>
#include <linux/backing-dev.h>
#include <linux/blkpg.h>
#include <linux/writeback.h>

#include <asm/uaccess.h>

#define DEVICE_NAME 		"mydrv"
#define MYDRV_MAX_LENGTH	(8*1024*1024)
#define MYDRV_BLK_SIZE		512
#define MYDRV_TOTAL_BLK		(MYDRV_MAX_LENGTH/MYDRV_BLK_SIZE)

static int MYDRV_MAJOR = 0;
static char * mydrv_data;
struct request_queue *mydrv_queue;
struct gendisk       *mydrv_disk;
typedef struct request_queue request_queue_t;

static int mydrv_make_request(request_queue_t *q, struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	int i;
	char *data;
	char *buffer;
	sector_t sector = bio->bi_sector;
	unsigned long len = bio->bi_size >> 9;
	struct bio_vec *bvec;
	int rw = bio_data_dir(bio);

	printk("enter to mydrv_make_reauest function\n");
	if (sector + len > get_capacity(bdev->bd_disk))
		goto fail;

	if(rw == READA)
		rw = READ;

	data  = mydrv_data + (sector * MYDRV_BLK_SIZE);

	bio_for_each_segment(bvec, bio, i)
	{
		buffer = kmap(bvec->bv_page) + bvec->bv_offset;
		switch(rw) 
		{
			case READ  : memcpy(buffer, data, bvec->bv_len); 
					printk("READ\n");
				     break; 
			case WRITE : memcpy(data, buffer, bvec->bv_len); 
					printk("WRITE\n");
				     break;
			default    : kunmap(bvec->bv_page);
				     goto fail;
		}
		kunmap(bvec->bv_page);
		data += bvec->bv_len;
	}

	//bio_endio(bio, bio->bi_size, 0); //until 2.6.22
	bio_endio(bio, 0); //after 2.6.24
	return 0;
fail:
	//bio_io_error(bio, bio->bi_size); //until 2.6.22
	bio_io_error(bio); //after 2.6.24
	return 0;
} 

int mydrv_open(struct inode *inode, struct file *filp)
{
	printk("open\n");
	printk("<0>%s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

int mydrv_release (struct inode *inode, struct file *filp)
{
	printk("release\n");
	printk("<0>%s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

int mydrv_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	printk("ioctl\n");
	printk("<0>%s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
	printk("<0>CMD = %d\n", cmd);
	return 0;
}

static struct block_device_operations mydrv_fops =
{
	.owner   = THIS_MODULE,    
	.open    = mydrv_open,
	.release = mydrv_release,  
	.ioctl   = mydrv_ioctl,
};

int mydrv_init(void)
{
	int lp;

	if( (MYDRV_MAJOR = register_blkdev(MYDRV_MAJOR, DEVICE_NAME)) < 0 ){
		printk("<0> can't be registered\n");
		return -EIO;
	}
	printk("<0> major NO = %d\n", MYDRV_MAJOR);

	if( (mydrv_data = vmalloc(MYDRV_MAX_LENGTH)) == NULL ){
		unregister_chrdev(MYDRV_MAJOR, DEVICE_NAME);
		printk("<0> vmalloc failed\n");
		return -ENOMEM;
	}
	
	if( (mydrv_disk = alloc_disk(1)) == NULL ){
		printk("<0> alloc_dirk failed\n");
		unregister_chrdev(MYDRV_MAJOR, DEVICE_NAME);
		vfree(mydrv_data);
		return -EIO;
	}		

	if( (mydrv_queue = blk_alloc_queue(GFP_KERNEL)) == NULL ){
		printk("<0> blk_alloc_queue failed\n");
		unregister_chrdev(MYDRV_MAJOR, DEVICE_NAME);
		vfree(mydrv_data);
		put_disk(mydrv_disk);
		return -EIO;
	}

	blk_queue_make_request(mydrv_queue, &mydrv_make_request);
	printk("finish blk_queue_make_request function\n");
	//blk_queue_hardsect_size(mydrv_queue, MYDRV_BLK_SIZE);
	blk_queue_logical_block_size(mydrv_queue, MYDRV_BLK_SIZE);
	printk("finish blk_queue_logical_block_size function\n");

	mydrv_disk->major = MYDRV_MAJOR;
	mydrv_disk->first_minor = 0;
	mydrv_disk->fops = &mydrv_fops;
	mydrv_disk->queue = mydrv_queue;
	sprintf(mydrv_disk->disk_name,  "mydrv");
	set_capacity(mydrv_disk, MYDRV_TOTAL_BLK);
	printk("add_disk\n");
	add_disk(mydrv_disk);

	return 0;
}

void mydrv_exit(void)
{
	vfree(mydrv_data);
	del_gendisk(mydrv_disk);
	put_disk(mydrv_disk);

	unregister_blkdev(MYDRV_MAJOR, DEVICE_NAME );
}

module_init(mydrv_init);
module_exit(mydrv_exit);

MODULE_LICENSE("GPL");
