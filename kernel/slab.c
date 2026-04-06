#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "slab.h"

uint slab_total(struct kmem_cache *cache){
  return (MP2_SLAB_SIZE - SLAB_METADATA_SIZE) / cache->object_size;
}

uint list_cnt(struct list_head* head){
  struct slab *slab;
  uint cnt = 0;
  list_for_each_entry(slab, head, list){
    cnt++;
  }

  return cnt;
}

void print_kmem_cache(struct kmem_cache *cache, void (*slab_obj_printer)(void *))
{
  // TODO: Implement print_kmem_cache
  //printf("[SLAB] TODO: print_kmem_cache is not yet implemented \n");
  //slab_obj_printer((void *)cache);
  //printf("A size: %ld, B size: %ld\n", sizeof(struct kmem_cache), sizeof(struct spinlock));
  acquire(&cache->lock);
  printf("[SLAB] kmem_cache { name: %s, object_size: %u, at: %p, in_cache_obj: %u } \n", cache->name, cache->object_size, (void *)cache, cache->max_obj_cnt);
  // print cache slab
  printf("[SLAB] [ cache slabs ] \n");
  printf("[SLAB] [ slab %p ] { freelist: %p, nxt: %p, max_objs: %u } \n", (void *)cache, (void *)cache->freelist, (void *)0, cache->max_obj_cnt);
  char *base = (char *)(cache) + 568;
  void *obj;
  for(int i = 0; i < cache->max_obj_cnt; i++){
    obj = (void *)(base + i * cache->object_size);

    printf("[SLAB]  [ idx %d ] { addr: %p, as_ptr: %p, as_obj: { ", i, (void *)obj, *(void **)obj);
    slab_obj_printer(obj);
    printf(" } } \n");
  }
  // print partial slab
  if(!list_empty(&cache->partial)){
    struct slab *sl;
    printf("[SLAB] [ partial slabs ] \n");
    list_for_each_entry(sl, &cache->partial, list){
      printf("[SLAB] [ slab %p ] { freelist: %p, nxt: %p, max_objs: %u } \n", (void *)sl, (void *)sl->freelist, (void *)sl->list.next, sl->max_obj_cnt);
      base = (char *)(sl) + SLAB_METADATA_SIZE;
      for(int i =0; i < sl->max_obj_cnt; i++){
        obj = (void *)(base + i * cache->object_size);

        printf("[SLAB]  [ idx %d ] { addr: %p, as_ptr: %p, as_obj: { ", i, (void *)obj, *(void **)obj);
        slab_obj_printer(obj);
        printf(" } } \n");
      }
    }
  }
  
  printf("[SLAB] print_kmem_cache end\n");
  release(&cache->lock);
}

struct kmem_cache *kmem_cache_create(char *name, uint object_size)
{
  // TODO: Implement kmem_cache_create
  //printf("[SLAB] TODO: kmem_cache_create is not yet implemented \n");
  struct kmem_cache *cache = (struct kmem_cache*)kalloc();
  if(cache == 0)
    return 0;

  memset(cache, 0, MP2_SLAB_SIZE);
  initlock(&cache->lock, name);
  INIT_LIST_HEAD(&cache->full);
  INIT_LIST_HEAD(&cache->partial);
  strncpy(cache->name, name, MP2_CACHE_MAX_NAME);
  cache->object_size = object_size;
  cache->obj_cnt = 0;
  uint slab_max_objcnt = (MP2_SLAB_SIZE - SLAB_METADATA_SIZE) / cache->object_size;
  // first from 568
  char *start = (char *)(cache) + 568, *obj_addr;
  cache->max_obj_cnt = (MP2_SLAB_SIZE - 568) / cache->object_size;

  for(int i = 0; i < cache->max_obj_cnt; i++){
    obj_addr = start + i * cache->object_size;
    if(i != cache->max_obj_cnt - 1){
      *(void **)obj_addr = (void *)(obj_addr + cache->object_size); 
    }else{
      *(void **)obj_addr = 0;
    }
  }
  cache->freelist = (void **)start;

  printf("[SLAB] New kmem_cache (name: %s, object size: %u bytes, at: %p, max objects per slab: %u, support in cache obj: %u) is created \n", cache->name, object_size, (void *)cache, slab_max_objcnt, cache->max_obj_cnt);

  return cache;
  
}

struct slab *slab_create(struct kmem_cache *cache){
  
  struct slab* slab = (struct slab *)kalloc();
  if(slab == 0)
    return 0;

  memset(slab, 0, MP2_SLAB_SIZE);
  INIT_LIST_HEAD(&slab->list);
  char *start = (char *)(slab) + SLAB_METADATA_SIZE, *obj_addr;
  slab->max_obj_cnt = slab_total(cache);
  for(int i = 0; i < slab->max_obj_cnt; i++){
    obj_addr = start + i * cache->object_size;
    if(i != slab->max_obj_cnt - 1){
      *(void **)obj_addr = (void *)(obj_addr + cache->object_size); 
    }else{
      *(void **)obj_addr = 0;
    }
  }
  slab->freelist = (void **)start;
  slab->obj_cnt = 0;

  printf("[SLAB] A new slab %p (%s) is allocated \n", (void *)slab, cache->name);
  
  return slab;
  
}

void kmem_cache_destroy(struct kmem_cache *cache)
{
  // TODO: Implement kmem_cache_destroy (will not be tested)
  printf("[SLAB] TODO: kmem_cache_destroy is not yet implemented \n");
}

void *kmem_cache_alloc(struct kmem_cache *cache)
{ 
  // TODO: Implement kmem_cache_alloc
  //printf("[SLAB] TODO: kmem_cache_alloc is not yet implemented \n");
  
  // acquire(&cache->lock); // acquire the lock before modification
  // ... (modify kmem_cache)
  // release(&cache->lock); // release the lock before return
  acquire(&cache->lock);

  printf("[SLAB] Alloc request on cache %s \n", cache->name);
  void *obj;
  struct slab *sl = 0;
  if(cache->freelist == 0){   // no in cache space

    if(list_empty(&cache->partial)){  // no partial slab exists
      sl = slab_create(cache);
      if(sl == 0){
        release(&cache->lock);
        return 0;
      }
      list_add_tail(&sl->list, &cache->partial);
    }else{  // find the partial slab
      sl = list_first_entry(&cache->partial, struct slab, list);
    } 

    obj = (void *)sl->freelist;
    sl->freelist = (void **)(*sl->freelist);
    sl->obj_cnt++;
    if(sl->obj_cnt == sl->max_obj_cnt){
      list_del(&sl->list);
      list_add_tail(&sl->list, &cache->full);
    }
    printf("[SLAB] Object %p in slab %p (%s) is allocated and initialized \n", (void *)obj, (void *)sl, cache->name);

  }else{
    obj = (void *)cache->freelist;
    cache->freelist = (void **)(*cache->freelist);
    cache->obj_cnt++;
    printf("[SLAB] Object %p in slab %p (%s) is allocated and initialized \n", (void *)obj, (void *)cache, cache->name);
  }
  
  release(&cache->lock);

  return obj;
  
}

void kmem_cache_free(struct kmem_cache *cache, void *obj)
{ 
  // TODO: Implement kmem_cache_free
  //printf("[SLAB] TODO: kmem_cache_free is not yet implemented \n");

  // acquire(&cache->lock); // acquire the lock before modification
  // ... (modify kmem_cache)  char *start =printf("[SLAB] Object %p in slab %p (%s) is allocated and initialized \n", obj, sl, cache->name); 0;
  // release(&cache->lock); // release the lock before return
  acquire(&cache->lock);
  struct slab* sl = 0;
  // obj return back
  if(PGROUNDDOWN((uint64)obj) == (uint64)cache){
    *(void **)obj = (void *)cache->freelist;
    cache->freelist = (void **)obj;
    cache->obj_cnt--;
    printf("[SLAB] Free %p in slab %p (%s) \n", (void *)obj, (void *)cache, cache->name);
  }else{
    sl = (struct slab *)PGROUNDDOWN((uint64)obj);
    if(sl->obj_cnt == sl->max_obj_cnt ){  // move to partial from full
      list_del(&sl->list);
      list_add_tail(&sl->list, &cache->partial);
    }
    sl->obj_cnt--; 
    *(void **)obj = (void *)sl->freelist;
    sl->freelist = (void **)obj;
    printf("[SLAB] Free %p in slab %p (%s) \n", (void *)obj, (void *)sl, cache->name);
  }

  // find partial slab cnt
  uint partial_cnt = list_cnt(&cache->partial), find = 0;

  if(partial_cnt > MP2_MIN_AVAIL_SLAB)
  list_for_each_entry(sl, &cache->partial, list){
    if(sl->obj_cnt == 0){  // the slab is unused
      find = 1;
      break;
    }
  }

  if(find){
    list_del(&sl->list);
    printf("[SLAB] Slab %p (%s) is freed due to save memory\n", (void *)sl, cache->name);
    kfree((void *)sl);
  }

  printf("[SLAB] End of free \n");

  release(&cache->lock);
  
}
