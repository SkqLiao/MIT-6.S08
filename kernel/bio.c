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

#define NBUCKET 7
struct {
  struct spinlock lock[NBUCKET], eviction_lock;
  struct buf buf[NBUCKET][NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
} bcache;

void binit(void) {
  struct buf *b;
  initlock(&bcache.eviction_lock, "eviction");
  for (int i = 0; i < NBUCKET; ++i) {
    initlock(&bcache.lock[i], "bcache");

    // Create linked list of buffers
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
    for (b = bcache.buf[i]; b < bcache.buf[i] + NBUF; b++) {
      b->next = bcache.head[i].next;
      b->prev = &bcache.head[i];
      b->id = i;
      initsleeplock(&b->lock, "buffer");
      bcache.head[i].next->prev = b;
      bcache.head[i].next = b;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {
  int i = (blockno << 10 | dev) % NBUCKET;
  struct buf *b;

  acquire(&bcache.lock[i]);

  // Is the block already cached?
  for (b = bcache.head[i].next; b != &bcache.head[i]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[i]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.head[i].prev; b != &bcache.head[i]; b = b->prev) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[i]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.lock[i]);
  acquire(&bcache.eviction_lock);

  for (b = bcache.head[i].next; b != &bcache.head[i]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[i]);
      release(&bcache.eviction_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  int bucket = -1;
  struct buf *b1 = 0;

  for (int j = 0; j < NBUCKET; ++j) {
    acquire(&bcache.lock[j]);
    for (b = bcache.head[j].prev; b != &bcache.head[j]; b = b->prev) {
      if (b->refcnt == 0) {
        b1 = b;
        break;
      }
    }
    if (!b1) {
      release(&bcache.lock[j]);
    } else {
      bucket = j;
      break;
    }
  }
  if (bucket == -1)
    panic("bget: no buffers");

  if (bucket != i) {
    b1->next->prev = b1->prev;
    b1->prev->next = b1->next;
    acquire(&bcache.lock[i]);
    b1->next = bcache.head[i].next;
    b1->prev = &bcache.head[i];
    bcache.head[i].next->prev = b1;
    bcache.head[i].next = b1;
    release(&bcache.lock[i]);
  }
  acquire(&bcache.lock[i]);
  b1->dev = dev;
  b1->blockno = blockno;
  b1->valid = 0;
  b1->refcnt = 1;
  b1->id = i;
  release(&bcache.lock[i]);
  release(&bcache.lock[bucket]);
  release(&bcache.eviction_lock);
  acquiresleep(&b1->lock);
  return b1;
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("brelse");

  int id = b->id;

  releasesleep(&b->lock);

  acquire(&bcache.lock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[id].next;
    b->prev = &bcache.head[id];
    bcache.head[id].next->prev = b;
    bcache.head[id].next = b;
  }

  release(&bcache.lock[id]);
}

void bpin(struct buf *b) {
  int id = b->id;
  acquire(&bcache.lock[id]);
  b->refcnt++;
  release(&bcache.lock[id]);
}

void bunpin(struct buf *b) {
  int id = b->id;
  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]);
}
