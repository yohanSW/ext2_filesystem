/*
 * File 	: blk_test.c
 * Author	: Seungjae Baek (baeksj@dankook.ac.kr)
 * Company  	: Dankook Univ. Embedded System Lab.
 */
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
#define MYDRV_TOTAL_BLK		(MYDRV_MAX_LENGTH >> 9)

static int MYDRV_MAJOR = 0;
static char * mydrv_data;
struct request_queue *mydrv_queue;
struct gendisk       *mydrv_disk;

static void mydrv_make_request(struct request_queue *q, struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	int rw;
	struct bio_vec bvec;
	sector_t sector;
	struct bvec_iter iter;
	int err = -EIO;
	void * mem;
	char * data;

	sector = bio->bi_iter.bi_sector;
	if (bio_end_sector(bio) > get_capacity(bdev->bd_disk))
		goto out;
	if (unlikely(bio->bi_rw & REQ_DISCARD)) {
		err = 0;
		goto out;
	}

	rw = bio_rw(bio);
	if(rw == READA)
		rw = READ;

	bio_for_each_segment(bvec, bio, iter)
	{
		unsigned int len = bvec.bv_len;
		data  = mydrv_data + (sector * MYDRV_BLK_SIZE);
		mem = kmap_atomic(bvec.bv_page) + bvec.bv_offset;

		if (rw == READ) {
			memcpy(mem + bvec.bv_offset, data, len); 
			flush_dcache_page(page);
		} else {
			flush_dcache_page(page);
			memcpy(data, mem + bvec.bv_offset, len); 
		}
		kunmap_atomic(mem);
		data += len ;
		sector += len >> 9;
		err=0;
	}

out:
	bio_endio(bio, err);
} 

int mydrv_open(struct block_device *dev, fmode_t mode)
{
	return 0;
}

void mydrv_release (struct gendisk *gd, fmode_t mode)
{
	return;
}

int mydrv_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
	int err;
	if (cmd != BLKFLSBUF)
		return -ENOTTY;
	err = -EBUSY;
	if (bdev->bd_openers <= 1){
		kill_bdev(bdev);
		err = 0;
	}
	return err;
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
	if( (MYDRV_MAJOR = register_blkdev(MYDRV_MAJOR, DEVICE_NAME)) < 0 ){
		printk("<0> can't be registered\n");
		return -EIO;
	}
	printk("<0> major NO = %d\n", MYDRV_MAJOR);

	if( (mydrv_data = vmalloc(MYDRV_MAX_LENGTH)) == NULL ){
		unregister_blkdev(MYDRV_MAJOR, DEVICE_NAME);
		printk("<0> vmalloc failed\n");
		return -ENOMEM;
	}
	if( (mydrv_disk = alloc_disk(1)) == NULL ){
		printk("<0> alloc_disk failed\n");
		unregister_chrdev(MYDRV_MAJOR, DEVICE_NAME);
		vfree(mydrv_data);
		return -EIO;
	}		

	if( (mydrv_queue =  blk_alloc_queue(GFP_KERNEL)) == NULL ){
		printk("<0> blk_alloc_queue failed\n");
		put_disk(mydrv_disk);
		vfree(mydrv_data);
		unregister_chrdev(MYDRV_MAJOR, DEVICE_NAME);
		return -EIO;
	}

	blk_queue_make_request(mydrv_queue, &mydrv_make_request);
	blk_queue_max_hw_sectors(mydrv_queue, 512);
	mydrv_disk->major = MYDRV_MAJOR;
	mydrv_disk->first_minor = 0;
	mydrv_disk->fops = &mydrv_fops;
	mydrv_disk->queue = mydrv_queue;
	sprintf(mydrv_disk->disk_name,  "mydrv");
	set_capacity(mydrv_disk, MYDRV_TOTAL_BLK);
	add_disk(mydrv_disk);

	return 0;
}

void mydrv_exit(void)
{
	del_gendisk(mydrv_disk);
	put_disk(mydrv_disk);
	blk_cleanup_queue(mydrv_queue);
	vfree(mydrv_data);

	blk_unregister_region(MKDEV(MYDRV_MAJOR, 0), 1);
	unregister_blkdev(MYDRV_MAJOR, DEVICE_NAME );
}

module_init(mydrv_init);
module_exit(mydrv_exit);
MODULE_LICENSE("GPL");
