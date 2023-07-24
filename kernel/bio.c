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

#define NBUCKET 13
#define HASH_FUNCTION(X) (X % NBUCKET)

struct
{
  // struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;

  struct buf hash_buf[NBUCKET];
  struct spinlock hash_lock[NBUCKET];
} bcache;

void binit(void)
{
  struct buf *b;

  // initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKET; i++)
  {
    initlock(&bcache.hash_lock[i], "hash_cache");
    bcache.hash_buf[i].next = 0;
  }

  for (int i = 0; i < NBUF; i++)
  {
    b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->last_used = 0;
    b->refcnt = 0;
    b->next = bcache.hash_buf[0].next;
    bcache.hash_buf[0].next = b;
  }
}

struct buf *find_lru(int *ret_id)
{

  struct buf *b;
  struct buf *tmp_buf_ans = 0;

  for (int i = 0; i < NBUCKET; i++)
  {
    acquire(&bcache.hash_lock[i]);
    int find_new = 0;
    for (b = &bcache.hash_buf[i]; b->next; b = b->next)
    {
      if (b->next->refcnt == 0 && (!tmp_buf_ans || tmp_buf_ans->next->last_used > b->next->last_used))
      {
        find_new = 1;
        tmp_buf_ans = b;
      }
    }

    // 需要释放掉之前那把锁，而不是当前这把
    if (find_new)
    {
      if ((*ret_id) != -1)
        release(&bcache.hash_lock[*ret_id]);

      *ret_id = i;
    }
    // 需要释放掉当前这把锁
    else
    {
      release(&bcache.hash_lock[i]);
    }
  }

  return tmp_buf_ans;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;

  int key = HASH_FUNCTION(blockno);
  acquire(&bcache.hash_lock[key]);

  // Is the block already cached?
  for (b = bcache.hash_buf[key].next; b; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.hash_lock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.hash_lock[key]);

  // Not cached.
  // 这个函数返回时应该还获取着 bcache.hash_buf[lru_id] 的锁
  int id_ret = -1;
  struct buf *b_ret = find_lru(&id_ret);

  if (b_ret == 0 || id_ret == -1)
  {
    panic("bget: no buffers");
  }

  // 设置 b_ret,如果不存在bcache.hash_buf[id]数组中,要新加一个引用
  struct buf *lru = b_ret->next;
  b_ret->next = lru->next;

  // 可以释放 bcache.hash_buf[id_ret]的锁了，因为已经把这个 buf 给删除了
  release(&bcache.hash_lock[id_ret]);

  acquire(&bcache.hash_lock[key]);

  for (b = bcache.hash_buf[key].next; b; b = b->next)
  {
    if (b->blockno == blockno && b->dev == dev)
    {
      b->refcnt++;
      release(&bcache.hash_lock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  lru->next = bcache.hash_buf[key].next;
  bcache.hash_buf[key].next = lru;

  lru->refcnt = 1;
  lru->dev = dev;
  lru->blockno = blockno;
  lru->valid = 0;

  // 释放这把锁
  release(&bcache.hash_lock[key]);
  acquiresleep(&lru->lock);
  return lru;
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int key = HASH_FUNCTION(b->blockno);
  acquire(&bcache.hash_lock[key]);

  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    b->last_used = ticks;
  }

  release(&bcache.hash_lock[key]);
}

void bpin(struct buf *b)
{
  int key = HASH_FUNCTION(b->blockno);
  acquire(&bcache.hash_lock[key]);
  b->refcnt++;
  release(&bcache.hash_lock[key]);
}

void bunpin(struct buf *b)
{
  int key = HASH_FUNCTION(b->blockno);
  acquire(&bcache.hash_lock[key]);
  b->refcnt--;
  release(&bcache.hash_lock[key]);
}
