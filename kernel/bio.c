// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 31
struct bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket buckets[NBUCKET];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;

int
hash(uint dev, uint blockno)
{
  return (dev ^ blockno) % NBUCKET;
}

void
bremove(struct buf *b)
{
  b->next->prev = b->prev;
  b->prev->next = b->next;
  b->next = 0;
  b->prev = 0;
}

void
binsert(struct buf *head, struct buf *b)
{
  b->next = head->next;
  b->prev = head;
  head->next->prev = b;
  head->next = b;
}

void
binit(void)
{
  struct buf *b;
  int i = 0;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  for(int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.buckets[i].lock, "bcache.bucket");
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->refcnt = 0;
    b->valid = 0;
    b->dev = 0;
    b->blockno = 0;
    
    binsert(&bcache.buckets[i % NBUCKET].head, b);
    i++;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int h = hash(dev, blockno);

  acquire(&bcache.buckets[h].lock);

  // Is the block already cached?
  for(b = bcache.buckets[h].head.next; b != &bcache.buckets[h].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[h].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.buckets[h].lock);

  // Not cached.
  acquire(&bcache.lock);
  acquire(&bcache.buckets[h].lock);
  for(b = bcache.buckets[h].head.next; b != &bcache.buckets[h].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[h].lock);
      release(&bcache.lock);
      
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.buckets[h].lock);

  for(int k = 0; k < NBUCKET; k++) {
    int i = (h + k) % NBUCKET;
    acquire(&bcache.buckets[i].lock);
    for(b = bcache.buckets[i].head.prev; b != &bcache.buckets[i].head; b = b->prev){
      if(b->refcnt == 0) {
        if(i == h) {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;

          release(&bcache.buckets[i].lock);
          release(&bcache.lock);

          acquiresleep(&b->lock);
          return b;
        }else {
          // victim 在旧 bucket，需要移动到新 bucket
          bremove(b);
          release(&bcache.buckets[i].lock);

          acquire(&bcache.buckets[h].lock);

          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;

          binsert(&bcache.buckets[h].head, b);

          release(&bcache.buckets[h].lock);
          release(&bcache.lock);

          acquiresleep(&b->lock);
          return b;
        }
      }
    }
    release(&bcache.buckets[i].lock);
  }
  release(&bcache.lock);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int h = hash(b->dev, b->blockno);

  acquire(&bcache.buckets[h].lock);
  b->refcnt--;
  release(&bcache.buckets[h].lock);
}

void
bpin(struct buf *b) {
  int h = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[h].lock);
  b->refcnt++;
  release(&bcache.buckets[h].lock);
}

void
bunpin(struct buf *b) {
  int h = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[h].lock);
  b->refcnt--;
  release(&bcache.buckets[h].lock);
}


