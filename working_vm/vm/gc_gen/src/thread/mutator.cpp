/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * @author Xiao-Feng Li, 2006/10/05
 */

#include "mutator.h"
#include "../trace_forward/fspace.h"
#include "../mark_sweep/gc_ms.h"
#include "../mark_sweep/wspace.h"
#include "../finalizer_weakref/finalizer_weakref.h"

struct GC_Gen;
Space* gc_get_nos(GC_Gen* gc);
void mutator_initialize(GC* gc, void *unused_gc_information) 
{
  /* FIXME:: make sure gc_info is cleared */
  Mutator *mutator = (Mutator *)STD_MALLOC(sizeof(Mutator));
  memset(mutator, 0, sizeof(Mutator));
  mutator->alloc_space = gc_get_nos((GC_Gen*)gc);
  mutator->gc = gc;
    
  if(gc_is_gen_mode()){
    mutator->rem_set = free_set_pool_get_entry(gc->metadata);
    assert(vector_block_is_empty(mutator->rem_set));
  }
  mutator->dirty_set = free_set_pool_get_entry(gc->metadata);
  
  if(!IGNORE_FINREF )
    mutator->obj_with_fin = finref_get_free_block(gc);
  else
    mutator->obj_with_fin = NULL;

#ifdef USE_UNIQUE_MARK_SWEEP_GC
  allocator_init_local_chunks((Allocator*)mutator);
#endif

  
  lock(gc->mutator_list_lock);     // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

  mutator->next = (Mutator *)gc->mutator_list;
  gc->mutator_list = mutator;
  gc->num_mutators++;

  /*Begin to measure the mutator thread execution time. */
  mutator->time_measurement_start = time_now();
  unlock(gc->mutator_list_lock); // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  
  gc_set_tls(mutator);
  
  return;
}

void mutator_register_new_obj_size(Mutator * mutator);

void mutator_destruct(GC* gc, void *unused_gc_information)
{

  Mutator *mutator = (Mutator *)gc_get_tls();

  alloc_context_reset((Allocator*)mutator);


  lock(gc->mutator_list_lock);     // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

#ifdef USE_UNIQUE_MARK_SWEEP_GC
  allocactor_destruct_local_chunks((Allocator*)mutator);
#endif

  mutator_register_new_obj_size(mutator);

  volatile Mutator *temp = gc->mutator_list;
  if (temp == mutator) {  /* it is at the head of the list */
    gc->mutator_list = temp->next;
  } else {
    while (temp->next != mutator) {
      temp = temp->next;
      assert(temp);
    }
    temp->next = mutator->next;
  }
  gc->num_mutators--;

  unlock(gc->mutator_list_lock); // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  
  if(gc_is_gen_mode()){ /* put back the remset when a mutator exits */
    pool_put_entry(gc->metadata->mutator_remset_pool, mutator->rem_set);
    mutator->rem_set = NULL;
  }
  
  if(mutator->obj_with_fin){
    pool_put_entry(gc->finref_metadata->obj_with_fin_pool, mutator->obj_with_fin);
    mutator->obj_with_fin = NULL;
  }

  lock(mutator->dirty_set_lock);
  if( mutator->dirty_set != NULL){
    if(vector_block_is_empty(mutator->dirty_set))
      pool_put_entry(gc->metadata->free_set_pool, mutator->dirty_set);
    else{ /* FIXME:: this condition may be released. */
      pool_put_entry(gc->metadata->gc_dirty_set_pool, mutator->dirty_set);
      mutator->dirty_set = NULL;
    }
  }
  unlock(mutator->dirty_set_lock);
  STD_FREE(mutator);
  gc_set_tls(NULL);
  
  return;
}

void gc_reset_mutator_context(GC* gc)
{
  TRACE2("gc.process", "GC: reset mutator context  ...\n");
  Mutator *mutator = gc->mutator_list;
  while (mutator) {
    alloc_context_reset((Allocator*)mutator);    
    mutator = mutator->next;
  }  
  return;
}

void gc_prepare_mutator_remset(GC* gc)
{
  Mutator *mutator = gc->mutator_list;
  while (mutator) {
    mutator->rem_set = free_set_pool_get_entry(gc->metadata);
    mutator = mutator->next;
  }  
  return;
}

Boolean gc_local_dirtyset_is_empty(GC* gc)
{
  lock(gc->mutator_list_lock);

  Mutator *mutator = gc->mutator_list;
  while (mutator) {
    Vector_Block* local_dirty_set = mutator->dirty_set;
    if(!vector_block_is_empty(local_dirty_set)){
      unlock(gc->mutator_list_lock); 
      return FALSE;
    }
    mutator = mutator->next;
  }  

  unlock(gc->mutator_list_lock); 
  return TRUE;
}

Vector_Block* gc_get_local_dirty_set(GC* gc, unsigned int shared_id)
{
  lock(gc->mutator_list_lock);

  Mutator *mutator = gc->mutator_list;
  while (mutator) {
    Vector_Block* local_dirty_set = mutator->dirty_set;
    if(!vector_block_is_empty(local_dirty_set) && vector_block_set_shared(local_dirty_set,shared_id)){
      unlock(gc->mutator_list_lock); 
      return local_dirty_set;
    }
    mutator = mutator->next;
  }  

  unlock(gc->mutator_list_lock); 
  return NULL;
}

void gc_start_mutator_time_measure(GC* gc)
{
  lock(gc->mutator_list_lock);
  Mutator* mutator = gc->mutator_list;
  while (mutator) {
    mutator->time_measurement_start = time_now();
    mutator = mutator->next;
  }  
  unlock(gc->mutator_list_lock);
}

int64 gc_get_mutator_time(GC* gc)
{
  int64 time_mutator = 0;
  lock(gc->mutator_list_lock);
  Mutator* mutator = gc->mutator_list;
  while (mutator) {
#ifdef _DEBUG
    mutator->time_measurement_end = time_now();
#endif
    int64 time_measured = time_now() - mutator->time_measurement_start;
    if(time_measured > time_mutator){
      time_mutator = time_measured;
    }
    mutator = mutator->next;
  }  
  unlock(gc->mutator_list_lock);
  return time_mutator;
}

static POINTER_SIZE_INT desturcted_mutator_alloced_size;
static POINTER_SIZE_INT desturcted_mutator_alloced_num;
static POINTER_SIZE_INT desturcted_mutator_alloced_occupied_size;
static POINTER_SIZE_INT desturcted_mutator_write_barrier_marked_size;

void mutator_register_new_obj_size(Mutator * mutator)
{
  desturcted_mutator_alloced_size += mutator->new_obj_size;
  desturcted_mutator_alloced_num += mutator->new_obj_num;
  desturcted_mutator_alloced_occupied_size += mutator->new_obj_occupied_size;
  desturcted_mutator_write_barrier_marked_size += mutator->write_barrier_marked_size;
}


unsigned int gc_get_mutator_write_barrier_marked_size(GC* gc)
{
  POINTER_SIZE_INT write_barrier_marked_size = 0;
  lock(gc->mutator_list_lock);
  Mutator* mutator = gc->mutator_list;
  while (mutator) {
    write_barrier_marked_size += mutator->write_barrier_marked_size;
    mutator = mutator->next;
  }  
  unlock(gc->mutator_list_lock);
  return write_barrier_marked_size;
}
unsigned int gc_get_mutator_dirty_obj_num(GC *gc)
{
  POINTER_SIZE_INT dirty_obj_num = 0;
  lock(gc->mutator_list_lock);
  Mutator* mutator = gc->mutator_list;
  while (mutator) {
    dirty_obj_num += mutator->dirty_obj_num;
    mutator = mutator->next;
  }  
  unlock(gc->mutator_list_lock);
  return dirty_obj_num;
}

unsigned int gc_get_mutator_new_obj_size(GC* gc) 
{
  POINTER_SIZE_INT new_obj_occupied_size = 0;
  lock(gc->mutator_list_lock);
  Mutator* mutator = gc->mutator_list;
  while (mutator) {
    new_obj_occupied_size += mutator->new_obj_occupied_size;
    mutator = mutator->next;
  }  
  unlock(gc->mutator_list_lock);

  return new_obj_occupied_size + desturcted_mutator_alloced_occupied_size;
  
}

unsigned int gc_reset_mutator_new_obj_size(GC * gc)
{
  POINTER_SIZE_INT new_obj_occupied_size = 0;
  lock(gc->mutator_list_lock);
  Mutator* mutator = gc->mutator_list;
  while (mutator) {
    new_obj_occupied_size += mutator->new_obj_occupied_size;
    mutator->new_obj_size = 0;
    mutator->new_obj_num = 0;
    mutator->new_obj_occupied_size = 0;
    mutator->write_barrier_marked_size = 0;
    mutator->dirty_obj_num = 0;
    mutator = mutator->next;
  }
  new_obj_occupied_size += desturcted_mutator_alloced_occupied_size;
  desturcted_mutator_alloced_size = 0;
  desturcted_mutator_alloced_num = 0;
  desturcted_mutator_alloced_occupied_size = 0;
  unlock(gc->mutator_list_lock);
  
  return new_obj_occupied_size;
}

unsigned int gc_get_mutator_number(GC *gc)
{
  return gc->num_mutators;
}


#ifdef ORDER
/***************************  record mode code start ****************************/
bool binary_mode = TRUE;
//bool binary_mode = FALSE;
unsigned int swap_count = 0;
FILE *logFile = NULL;

/**************** Single Pool ************************/
static Access_Pool  global_access_pool;
Access_Pool *get_Global_Access_Pool() {
	return &global_access_pool;
}


void *alloc_access_pool()
{
    unsigned int access_pool_size = ACCESS_INFO_SIZE * ACCESS_INFO_NUM;
    void * access_pool = STD_MALLOC(access_pool_size);
#ifdef ORDER_DEBUG
    if(access_pool == NULL){
        fprintf(stderr,"[xlc]: Could NOT malloc local access pool in mutator_initialize!\n");
        assert(0);
    }
#endif
    memset(access_pool, 0, access_pool_size);
    //fprintf(stderr,"[xlc]: init a pool\n");
    return access_pool;
}

void reset_access_pool(Access_Info *access_pool)
{
    memset(access_pool, 0, ACCESS_INFO_SIZE * ACCESS_INFO_NUM);
}

void free_access_pool(Access_Info *access_pool)
{
#ifdef ORDER_DEBUG
    assert(access_pool != NULL);
#endif
    STD_FREE(access_pool);
}

void init_global_access_pool()
{
#ifdef ORDER_DEBUG
    fprintf(stderr, "[xlc]: GC init global access pool\n");
#endif
    Access_Pool *pool = get_Global_Access_Pool();
    pool->pool = (Access_Info *)alloc_access_pool();
    pool->used_num = 0;
    pool->lock = FREE_LOCK;

    logFile = fopen64("ORDER_RECORD.log", "w+");
#ifdef ORDER_DEBUG
    if(logFile == NULL){
        fprintf(stderr, "[xlc]: could NOT open file ORDER_RECORD.log\n");
        assert(0);
    }
#endif
}

#ifdef REPLAY_OPTIMIZE
#define OBJ_RW_BIT 			0x800
#define OBJ_CLEAN_RW_BIT 	0xF7FF
#endif

void record_access_info(Partial_Reveal_Object* p_obj)
{

//    if(((Partial_Reveal_Object*)p_obj)->alloc_count == -1){
//        return;
//    }


#ifdef ORDER_LOCAL_OPT
    if(obj_is_thread_local(p_obj)){
        return;
    }
#endif

    //assert(threadMap.find(p_obj->access_tid) != threadMap.end());
    Access_Pool *pool = get_Global_Access_Pool();
    lock(pool->lock);
    Access_Info *access_pool = pool->pool;
#ifdef ORDER_DEBUG
    assert(access_pool != NULL);
    assert(pool->used_num < ACCESS_INFO_NUM);
#endif
    Access_Info *record_info = &access_pool[pool->used_num];
#ifdef ORDER_DEBUG
    assert(p_obj->alloc_count > 0);
    assert(p_obj->alloc_tid > 0);
#endif
//    assert(p_obj->alloc_tid == p_obj->access_tid);
    //these assignment may be replaced by memcpy operation
    record_info->access_count = p_obj->access_count;
    record_info->access_tid = p_obj->access_tid;
    record_info->alloc_count = p_obj->alloc_count;
    record_info->alloc_tid = p_obj->alloc_tid;

#ifdef REPLAY_OPTIMIZE
	record_info->rw_bit = ((p_obj->order_padding & OBJ_RW_BIT) == 0) ? 0 : 1;
	p_obj->order_padding &= OBJ_CLEAN_RW_BIT;
#endif

	/*if(record_info->alloc_count >= 16777215 && record_info->alloc_tid == 9){
		fprintf(stderr, "the record info access count is %d\n", record_info->alloc_count);
		assert(0);
	}*/
    //fprintf(logFile, "recode access info\n");

//    printf("record access info : count : %d\n", p_obj->access_count);
//    printf("record access info : tid : %d\n", p_obj->access_tid);

    pool->used_num++;

    if(pool->used_num == ACCESS_INFO_NUM){
        swap_access_pool_info(pool, POOL_IS_FULL);
        reset_access_pool(pool->pool);
        pool->used_num = 0;
    }
    unlock(pool->lock);
}

FORCE_INLINE void swap_access_pool_info(Access_Pool *pool, unsigned int swap_cause)
{
#ifdef ORDER_DEBUG
    assert(pool != NULL);
    assert(logFile != NULL);

    swap_count++;
	
    if(swap_cause == POOL_IS_FULL){
        assert(pool->used_num == ACCESS_INFO_NUM);
        printf("[xlc]: [%d] swap the access pool because of POOL_IS_FULL\n", swap_count);
    }
    else{
        assert(pool->used_num <= ACCESS_INFO_NUM);
        printf("[xlc]: [%d] swap the access pool because of POOL_DESTRUCT\n", swap_count);
    }

    if(binary_mode){
#endif
        unsigned int ret = fwrite((char *)pool->pool, ACCESS_INFO_SIZE, pool->used_num, logFile);
    
#ifdef ORDER_DEBUG
        assert(ret <= ACCESS_INFO_SIZE * pool->used_num);
    }
    else{
//        fprintf(logFile, "[%d]: Swap access pool info\n",swap_count);
        Access_Info *access_pool = pool->pool;
        for(unsigned int i = 0; i < pool->used_num; i++){
            Access_Info *record = &access_pool[i];
            //fprintf(logFile, "AllocTID %d AllocCount %d\n", 
		//	record->alloc_tid, record->alloc_count);
#ifdef REPLAY_OPTIMIZE
			fprintf(logFile, "AllocTID: %d,  AllocCount: %d,  AccessTID: %d,  AccessCount: %d,  RWbit: %d\n", 
			record->alloc_tid, record->alloc_count, record->access_tid, record->access_count, record->rw_bit);
#else
            fprintf(logFile, "AllocTID: %d,  AllocCount: %d,  AccessTID: %d,  AccessCount: %d \n", 
			record->alloc_tid, record->alloc_count, record->access_tid, record->access_count);
#endif
        }
    }
#endif
}

void destruct_global_access_pool()
{
#ifdef ORDER_DEBUG
    printf("[xlc]: GC destruct global access pool\n");
#endif
    Access_Pool *pool = get_Global_Access_Pool();
    swap_access_pool_info(pool, POOL_DESTRUCT);
    fflush(logFile);
    fclose(logFile);
#ifdef ORDER_DEBUG
    assert(pool->pool != NULL);
#endif
    free_access_pool(pool->pool);
    pool->pool = NULL;
    pool->used_num = 0;
    pool->lock = FREE_LOCK;
    
    
}
/***************************  record mode code end ****************************/


/***************************  replay mode code start ****************************/

//extern SpinLock thread_map_lock;
SpinLock thread_map_lock = FREE_LOCK;

U_32 mapping_from_thread_global_id_to_pthread_id(U_16 alloc_tid)
{
    U_16 i;
    U_32 temp = 0;

#ifdef ORDER_DEBUG
    assert(thread_map_lock == FREE_LOCK || thread_map_lock == LOCKED);
#endif

    lock(thread_map_lock);
    for(i=0; i < ORDER_THREAD_NUM; i++){
#ifdef ORDER_DEBUG
        printf("tid mapping : %u ==> %u\n", tid_pthreadid_mapping[i][1], tid_pthreadid_mapping[i][0]);
#endif
        if(tid_pthreadid_mapping[i][1] == alloc_tid){
            U_32 pid = tid_pthreadid_mapping[i][0];
            unlock(thread_map_lock);
            return pid;
        }
    }
    //should never reach here
    assert(0);
}

U_16 mapping_from_pthread_id_to_thread_assigned_id(U_32 pthread_id)
{
    U_16 i;
    U_32 temp = 0;
#ifdef ORDER_DEBUG
    assert(thread_map_lock == FREE_LOCK || thread_map_lock == LOCKED);
#endif
    lock(thread_map_lock);

    for(i=0; i < ORDER_THREAD_NUM; i++){
        
        if(tid_pthreadid_mapping[i][0] == pthread_id){
            unlock(thread_map_lock);
            return i;
        }
    }
    //should never reach here
    assert(0);
}

U_16 mapping_from_pthread_id_to_thread_assigned_id_no_assert(U_32 pthread_id)
{
    U_16 i;
    U_32 temp = 0;
#ifdef ORDER_DEBUG
    assert(thread_map_lock == FREE_LOCK || thread_map_lock == LOCKED);
#endif
    lock(thread_map_lock);

    for(i=0; i < ORDER_THREAD_NUM; i++){
        
        if(tid_pthreadid_mapping[i][0] == pthread_id){
            unlock(thread_map_lock);
            return i;
        }
    }
    unlock(thread_map_lock);
    return 0;
}

U_32 mapping_from_thread_assigned_id_to_pthread_id(U_16 thread_assigned_id)
{
#ifdef ORDER_DEBUG
    assert(thread_assigned_id < ORDER_THREAD_NUM);
#endif
    U_32 tid = tid_pthreadid_mapping[thread_assigned_id][0];
#ifdef ORDER_DEBUG
    printf("[Thread Mapping]: %d   ===>  %d\n", thread_assigned_id, tid);
#endif
    return tid;
}


#define ORDER_PRINT_DEBUG_INFO


#include  "list"
using std::list;

FILE *allocLog[ALLOC_FILE_NUM];
FILE *interLog;

#define PREFETCH_FORWARDING
#define INTERLEAVING_CACHE

//allocInfoPoolMap allocInfoPool;
AllocInfo_Pool *allocInfoPool[ORDER_THREAD_NUM];
ILInfo_Pool *interleavingInfo;
ILInfo_Pool *smallPool;

#ifdef PREFETCH_FORWARDING
FILE *forward_file;
U_64 fetch_start;
std::list<U_64> forwardList;
ILInfo *forwarding_pool;
#define FORWARD_POOL_SIZE  ILInfo_SIZE * ILInfo_NUM
#endif

#ifdef INTERLEAVING_CACHE
ILInfo_Pool *interCache;
#define CACHE_NUM 64
#endif


bool replay_verify = false; //assign true in gc_init_fetch_log


#ifdef INTERLEAVING_CACHE
bool addInterleavingInfo2Cache(ILInfo * interInfo)
{
    if(interCache->used_num >= CACHE_NUM){
        return FALSE;
    }

    ILInfo *blank_info = &(interCache->pool[interCache->used_num]);
#ifdef ORDER_DEBUG
    assert(blank_info->alloc_tid == 0 && blank_info->alloc_count == 0);
#endif
    memcpy((void *)blank_info, (void *)interInfo, ILInfo_SIZE);
    interCache->used_num++;
    return TRUE;
}

InterState findInterleavingInfoFromCache(Partial_Reveal_Object *p_obj){
#ifdef ORDER_DEBUG
    assert(interCache->used_num <= CACHE_NUM);
#endif

    int pool_size = interCache->used_num;
    int last_idx = pool_size - 1;
    ILInfo * interInfo = NULL;
    for(int i = 0; i < pool_size; i++){
        interInfo = &(interCache->pool[i]);
        if(interInfo->alloc_count == p_obj->alloc_count && interInfo->alloc_tid == p_obj->alloc_tid){
	   
            U_32 next_tid = mapping_from_thread_assigned_id_to_pthread_id(interInfo->next_tid);
            if(next_tid != (U_32)pthread_self()){
#ifdef ORDER_DEBUG
#ifdef ORDER_PRINT_DEBUG_INFO
                printf("[UNMATCH]: Object(%d, %d) is to be accessed by thread %d, current %d\n", p_obj->alloc_tid, p_obj->alloc_count, next_tid, pthread_self());
#endif
#endif
                return THREAD_ID_NOT_MATCH;
            }

            p_obj->access_tid = next_tid;

            p_obj->access_count = interInfo->next_acc_count;
//#ifdef ORDER_DEBUG
#ifdef ORDER_PRINT_DEBUG_INFO
            printf("[replay]: the inter info of object[%d, %d] is found in cache, n_t:[%d],  n_pthread_id:[%d]\n", p_obj->alloc_tid, p_obj->alloc_count,
			interInfo->next_tid, pthread_self());
#endif
//#endif
           U_16 last_tid =  mapping_from_pthread_id_to_thread_assigned_id_no_assert(p_obj->access_tid);
#ifdef ORDER_DEBUG
	   //assert(interInfo->last_tid == last_tid);
          if(last_tid == 0 || interInfo->last_tid != last_tid){
#ifdef ORDER_PRINT_DEBUG_INFO
	       printf("[WARNING]: this interleaving info MAY NOT correct since the last tid MAY be wrong!!!!!!\n");
#endif
	   }
#endif

          ILInfo * lastInfo = &(interCache->pool[last_idx]);
	   if(i != last_idx){
	       memcpy((void *)interInfo, (void *)lastInfo, ILInfo_SIZE);
	   }
	   memset(lastInfo, 0, ILInfo_SIZE);
	   interCache->used_num--;
	   return FOUND_IN_POOL;
        }
    }

    return NOT_FOUND_IN_POOL;
}
#endif


void getInterleavingInfoFromLog()
{
    //assert(interleavingInfo->used_num == interleavingInfo->entry_num);
#ifdef ORDER_DEBUG
    assert(interleavingInfo->pool != NULL);
#endif
    int num = fread((void *)interleavingInfo->pool, ILInfo_SIZE, ILInfo_NUM, interLog);
    interleavingInfo->entry_num = num;
    interleavingInfo->used_num = 0;

#ifdef PREFETCH_FORWARDING
    U_64 end_pos = fetch_start + num;
    std::list<U_64>::iterator itList;
    for(itList = forwardList.begin(); itList != forwardList.end(); ){
        U_64 pos = (U_64)*itList;
        if( pos >= fetch_start &&  pos < end_pos){
            ILInfo *interInfo = &(interleavingInfo->pool[pos - fetch_start]);
            memset(interInfo, 0, ILInfo_SIZE);
            interleavingInfo->used_num++;
            itList = forwardList.erase( itList);
        }
        else{
            itList++;
        }
    }

    fetch_start += num;
#endif
}


void init_interleaving_pool()
{
    interleavingInfo = (ILInfo_Pool *)STD_MALLOC(sizeof(ILInfo_Pool));
#ifdef ORDER_DEBUG
    if(interleavingInfo == NULL){
        fprintf(stderr,"[xlc]: Could NOT malloc allocPool!\n");
        assert(0);
    }
#endif
    memset(interleavingInfo, 0, sizeof(ILInfo_Pool));


    int pool_size = ILInfo_SIZE * ILInfo_NUM;
    ILInfo *pool = (ILInfo *)STD_MALLOC(pool_size);
#ifdef ORDER_DEBUG
    if(pool == NULL){
        fprintf(stderr,"[xlc]: Could NOT malloc pool !\n");
        assert(0);
    }
#endif
    memset(pool, 0, pool_size);

    interleavingInfo->pool = pool;
    interleavingInfo->used_num = 0;
    interleavingInfo->entry_num = 0;
    interleavingInfo->lock = FREE_LOCK;

    getInterleavingInfoFromLog();

    smallPool = (ILInfo_Pool *)STD_MALLOC(sizeof(ILInfo_Pool));
#ifdef ORDER_DEBUG
    if(smallPool == NULL){
        fprintf(stderr,"[xlc]: Could NOT malloc allocPool!\n");
        assert(0);
    }
#endif
    memset(smallPool, 0, sizeof(ILInfo_Pool));

    ILInfo *small_pool = (ILInfo *)STD_MALLOC(ILInfo_SIZE * SMALLPOOL_NUM);
#ifdef ORDER_DEBUG
    if(small_pool == NULL){
        fprintf(stderr,"[xlc]: Could NOT malloc pool !\n");
        assert(0);
    }
#endif
    memset(small_pool, 0, ILInfo_SIZE * SMALLPOOL_NUM);

    smallPool->pool = small_pool;
    smallPool->used_num = 0;
    smallPool->entry_num = 0;
    smallPool->lock = FREE_LOCK;

#ifdef INTERLEAVING_CACHE
    interCache = (ILInfo_Pool *)STD_MALLOC(sizeof(ILInfo_Pool));
#ifdef ORDER_DEBUG
    if(interCache == NULL){
        fprintf(stderr,"[xlc]: Could NOT malloc interCache!\n");
        assert(0);
    }
#endif
    memset(interCache, 0, sizeof(ILInfo_Pool));

    ILInfo *cache_pool = (ILInfo *)STD_MALLOC(ILInfo_SIZE * CACHE_NUM);
#ifdef ORDER_DEBUG
    if(cache_pool == NULL){
        fprintf(stderr,"[xlc]: Could NOT malloc pool !\n");
        assert(0);
    }
#endif
    memset(cache_pool, 0, ILInfo_SIZE * CACHE_NUM);

    interCache->pool = cache_pool;
    interCache->used_num = 0;
    interCache->entry_num = 0;
    interCache->lock = FREE_LOCK;
#endif

}

/***************************  Double Pool implement start *********************************************/
InterState find_in_small_pool(Partial_Reveal_Object *p_obj)
{
    if(smallPool->entry_num <= 0)  return NOT_FOUND_IN_POOL;

    int size = smallPool->entry_num;
    bool found = FALSE;
    ILInfo *interInfo = NULL;

    
    U_32 next_tid = 0;
    for(int i = 0; i < size; i++){
        interInfo = &(smallPool->pool[i]);
        if(interInfo->alloc_tid == 0 && interInfo->alloc_count == 0){
            continue;
        }

        if(interInfo->alloc_count == p_obj->alloc_count && interInfo->alloc_tid == p_obj->alloc_tid){
	   //the interleaving info is found
            found = TRUE;
            break;
        }

#ifdef ORDER_DEBUG
        assert (interInfo->alloc_count != p_obj->alloc_count || interInfo->alloc_tid != p_obj->alloc_tid);
#endif
    }

    if(found == TRUE){
        
#ifdef ORDER_DEBUG
        assert(interInfo != NULL);
#endif
        next_tid = mapping_from_thread_assigned_id_to_pthread_id(interInfo->next_tid);
        if(next_tid != (U_32)pthread_self()){
#ifdef INTERLEAVING_CACHE
            bool result = addInterleavingInfo2Cache(interInfo);
            if(result == TRUE){
#ifdef ORDER_DEBUG
//                printf("[replay]: add object interInfo (%d, %d, %d) in small pool!!!\n", interInfo->alloc_tid, interInfo->alloc_count, interInfo->next_tid);
#endif
                memset(interInfo, 0, ILInfo_SIZE);
                smallPool->used_num++;
                if(smallPool->used_num == smallPool->entry_num){
                    smallPool->used_num = 0;
                    smallPool->entry_num = 0;
                }
            }
#endif
#ifdef ORDER_DEBUG
#ifdef ORDER_PRINT_DEBUG_INFO
            printf("[UNMATCH]: Object(%d, %d) is to be accessed by thread %d, current %d\n", p_obj->alloc_tid, p_obj->alloc_count, next_tid, pthread_self());
#endif
#endif
            return THREAD_ID_NOT_MATCH;
        }
        p_obj->access_tid = next_tid;

        p_obj->access_count = interInfo->next_acc_count;
//#ifdef ORDER_DEBUG
//#ifdef ORDER_PRINT_DEBUG_INFO
        printf("[replay]: the inter info of object[%d, %d] is found in the small pool, n_t:[%d],  n_pthread_id:[%d]\n", p_obj->alloc_tid, p_obj->alloc_count,
			interInfo->next_tid, pthread_self());
//#endif
//#endif
        U_16 last_tid =  mapping_from_pthread_id_to_thread_assigned_id_no_assert(p_obj->access_tid);
#ifdef ORDER_DEBUG
        //assert(interInfo->last_tid == last_tid);
        if(last_tid == 0 || interInfo->last_tid != last_tid){
#ifdef ORDER_PRINT_DEBUG_INFO
	   printf("[WARNING]: this interleaving info MAY NOT correct since the last tid MAY be wrong!!!!!!\n");
#endif
        }
#endif
        memset(interInfo, 0, ILInfo_SIZE);

        smallPool->used_num++;
        if(smallPool->used_num == smallPool->entry_num){
            smallPool->used_num = 0;
            smallPool->entry_num = 0;
        }

        return FOUND_IN_POOL;
    }

    return NOT_FOUND_IN_POOL;

}

void swap_rest_to_small_pool()
{
#ifdef ORDER_DEBUG
    assert(SMALLPOOL_NUM - smallPool->entry_num >= SMALLPOOL_CHUNK);
#endif
    //4 step 1:compress the small pool if there are objects left
    ILInfo *small_info = NULL;
    int num = smallPool->entry_num;
    int last_blank = 0;
    for(int j = 0; j < num; j++){
        small_info = &(smallPool->pool[j]);
        if(small_info->alloc_tid == 0 && small_info->alloc_count == 0){
            continue;
        }

        if(j > last_blank){
            ILInfo *blank_info = &(smallPool->pool[last_blank]);
#ifdef ORDER_DEBUG
            assert(blank_info->alloc_tid == 0 && blank_info->alloc_count == 0);
#endif
            memcpy((void *)blank_info, (void *)small_info, ILInfo_SIZE);
            memset(small_info, 0, ILInfo_SIZE);
        }

        last_blank++;
    }

    //4 step 2:move the last 20 objects in current pool to small pool
    int size = interleavingInfo->entry_num;
    ILInfo *interInfo = NULL;
    for(int i = 0; i < size; i++){
        interInfo = &(interleavingInfo->pool[i]);
        if(interInfo->alloc_tid == 0 && interInfo->alloc_count == 0){
            continue;
        }
        small_info = &(smallPool->pool[last_blank]);
        memcpy((void *)small_info, (void *)interInfo, ILInfo_SIZE);
        last_blank++;
    }
#ifdef ORDER_DEBUG
    assert(last_blank <= SMALLPOOL_NUM);
#endif
    smallPool->entry_num = last_blank;
    smallPool->used_num = 0;

}

#ifdef PREFETCH_FORWARDING

bool findInforwardingList(U_64 pos)
{
    std::list<U_64>::iterator itList;
    for(itList = forwardList.begin(); itList != forwardList.end(); ++itList){
        U_64 forwarded_pos = (U_64)*itList;
        if( pos == forwarded_pos){
            return TRUE;
        }
    }

    return FALSE;
}

InterState getInterleavingInfoForwarding(Partial_Reveal_Object *p_obj)
{
#ifdef ORDER_DEBUG
    assert(forwarding_pool != NULL);
#endif
    memset(forwarding_pool, 0, FORWARD_POOL_SIZE);

    bool result = false;
    U_16 last_tid = mapping_from_pthread_id_to_thread_assigned_id(p_obj->access_tid);
    U_64 cur_pos = fetch_start * ILInfo_SIZE;
    fseeko64(forward_file, (U_64)cur_pos, SEEK_SET);
    U_64 trunk_offset = 0;
    while(!feof(forward_file)){	
	int num = fread((void *)forwarding_pool, ILInfo_SIZE, ILInfo_NUM, forward_file);
         for(int i = 0; i < num; i++){
	    ILInfo *record = &forwarding_pool[i];
             if(record->alloc_tid == p_obj->alloc_tid && record->alloc_count == p_obj->alloc_count){
	 
                  U_64 pos = fetch_start + trunk_offset + i;
 
                  if (findInforwardingList(pos)) {
                      continue;
                  }

                 U_32 next_tid = mapping_from_thread_assigned_id_to_pthread_id(record->next_tid);
                 if(next_tid != (U_32)pthread_self()){
#ifdef INTERLEAVING_CACHE
                     bool result = addInterleavingInfo2Cache(record);
                     if(result == TRUE){
#ifdef ORDER_DEBUG
//                         printf("[replay]: add object interInfo (%d, %d, %d) in forwarding!!!\n", record->alloc_tid, record->alloc_count, record->next_tid);
#endif
                         forwardList.push_back(pos);
                     }
#endif
#ifdef ORDER_DEBUG
#ifdef ORDER_PRINT_DEBUG_INFO
                     printf("[UNMATCH]: Object(%d, %d) is to be accessed by thread %d, current %d\n", p_obj->alloc_tid, p_obj->alloc_count, next_tid, pthread_self());
#endif
#endif
                     return THREAD_ID_NOT_MATCH;
                 }
                 p_obj->access_tid = next_tid;
                 p_obj->access_count = record->next_acc_count;
                 //put it into list
                forwardList.push_back(pos);
//#ifdef ORDER_DEBUG
//#ifdef ORDER_PRINT_DEBUG_INFO
                 printf("[replay]: the inter info of object[%d, %d] is found forward, index:%d, n_t:[%d], n_pthread_id:[%d]\n", p_obj->alloc_tid, 
			p_obj->alloc_count, fetch_start + trunk_offset + i, record->next_tid, pthread_self());
//#endif
//#endif
                 U_16 last_tid =  mapping_from_pthread_id_to_thread_assigned_id_no_assert(p_obj->access_tid);
#ifdef ORDER_DEBUG
	         //assert(interInfo->last_tid == last_tid);
	         if(last_tid == 0 || record->last_tid != last_tid){
#ifdef ORDER_PRINT_DEBUG_INFO
	             printf("[WARNING]: this interleaving info MAY NOT correct since the last tid MAY be wrong!!!!!!\n");
#endif
	         }
#endif
                 return FOUND_IN_POOL;
             }

//            assert (record->alloc_count != p_obj->alloc_count || record->alloc_tid != p_obj->alloc_tid);

	}
       trunk_offset += num;
    }

    return NOT_EXIST_IN_DISK;
  
}
#endif


InterState getInterleavingInfo(Partial_Reveal_Object *p_obj)
{
#ifdef ORDER_DEBUG
    assert(p_obj->access_count == 0);
#endif
    lock(interleavingInfo->lock);
    interleavingInfo->require_tid = pthread_self();

#ifdef INTERLEAVING_CACHE
    InterState cache_state = findInterleavingInfoFromCache(p_obj);
    if(cache_state == FOUND_IN_POOL || cache_state == THREAD_ID_NOT_MATCH){
        interleavingInfo->release_tid= pthread_self();
        unlock(interleavingInfo->lock);
        return cache_state;
    }
#endif

    InterState state = find_in_small_pool(p_obj);

    if(state == FOUND_IN_POOL || state == THREAD_ID_NOT_MATCH){
        interleavingInfo->release_tid= pthread_self();
        unlock(interleavingInfo->lock);
        return state;
    }
    else if(state == NOT_FOUND_IN_POOL){
        int size = interleavingInfo->entry_num;
        ILInfo *interInfo = NULL;
        bool found = FALSE;
        U_32 next_tid = 0;
        int i = 0;
        for( i = 0; i < size; i++){
            interInfo = &(interleavingInfo->pool[i]);
            if(interInfo->alloc_tid == 0 && interInfo->alloc_count == 0){
                continue;
            }

            if(interInfo->alloc_count == p_obj->alloc_count && interInfo->alloc_tid == p_obj->alloc_tid){
	       //the interleaving info is found
                found = TRUE;
                break;
            }
#ifdef ORDER_DEBUG
            assert (interInfo->alloc_count != p_obj->alloc_count || interInfo->alloc_tid != p_obj->alloc_tid);
#endif
        }

        if(found == TRUE){
#ifdef ORDER_DEBUG
            assert(interInfo != NULL);
#endif
            next_tid = mapping_from_thread_assigned_id_to_pthread_id(interInfo->next_tid);
            if(next_tid != (U_32)pthread_self()){
#ifdef INTERLEAVING_CACHE
                bool result = addInterleavingInfo2Cache(interInfo);
                if(result == TRUE){
#ifdef ORDER_DEBUG
//                    printf("[replay]: add object interInfo (%d, %d, %d) in current pool!!!\n", interInfo->alloc_tid, interInfo->alloc_count, interInfo->next_tid);
#endif
                    memset(interInfo, 0, ILInfo_SIZE);
                    interleavingInfo->used_num++;
                    if(interleavingInfo->entry_num - interleavingInfo->used_num == SMALLPOOL_CHUNK){
                        swap_rest_to_small_pool();
                        getInterleavingInfoFromLog();
                    }
                }
#endif
#ifdef ORDER_DEBUG
#ifdef ORDER_PRINT_DEBUG_INFO
                printf("[UNMATCH]: Object(%d, %d) is to be accessed by thread %d, current %d\n", p_obj->alloc_tid, p_obj->alloc_count, next_tid, pthread_self());
#endif
#endif
                interleavingInfo->release_tid= pthread_self();
                unlock(interleavingInfo->lock);
                return THREAD_ID_NOT_MATCH;
            }
            p_obj->access_tid = next_tid;
            p_obj->access_count = interInfo->next_acc_count;
//#ifdef ORDER_DEBUG
//#ifdef ORDER_PRINT_DEBUG_INFO
            printf("[replay]: the inter info of object[%d, %d] is found in the current pool, index:%d, n_t:[%d], n_pthread_id:[%d]\n", p_obj->alloc_tid, 
				p_obj->alloc_count, fetch_start - ILInfo_NUM + i, interInfo->next_tid, pthread_self());
//#endif
//#endif
            U_16 last_tid =  mapping_from_pthread_id_to_thread_assigned_id_no_assert(p_obj->access_tid);
#ifdef ORDER_DEBUG
	   //assert(interInfo->last_tid == last_tid);
	   if(last_tid == 0 ||interInfo->last_tid != last_tid){
#ifdef ORDER_PRINT_DEBUG_INFO
	       printf("[WARNING]: this interleaving info MAY NOT correct since the last tid MAY be wrong!!!!!!\n");
#endif
	   }
#endif
            memset(interInfo, 0, ILInfo_SIZE);

            interleavingInfo->used_num++;
            if(interleavingInfo->entry_num - interleavingInfo->used_num == SMALLPOOL_CHUNK){
                swap_rest_to_small_pool();
                getInterleavingInfoFromLog();
            }
            interleavingInfo->release_tid= pthread_self();
            unlock(interleavingInfo->lock);
            return FOUND_IN_POOL;
        }
        else{
#ifndef PREFETCH_FORWARDING
#ifdef ORDER_DEBUG
#ifdef ORDER_PRINT_DEBUG_INFO
            printf("[replay]: the inter info of object[%d, %d] is NOT found in the current pool\n", p_obj->alloc_tid, p_obj->alloc_count);
#endif
#endif
#else
            state = getInterleavingInfoForwarding(p_obj);
            if(state == FOUND_IN_POOL || state == THREAD_ID_NOT_MATCH){
                interleavingInfo->release_tid= pthread_self();
                unlock(interleavingInfo->lock);
                return state;
            }
            else{
#if 0//#ifdef ORDER_DEBUG
                printf("[replay]: the inter info of object[%d, %d] does NOT exist!!!!\n", p_obj->alloc_tid, p_obj->alloc_count);
                assert(0);
#else
                ((Partial_Reveal_Object*)p_obj)->alloc_count_for_hashcode = ((Partial_Reveal_Object*)p_obj)->alloc_count;
                ((Partial_Reveal_Object*)p_obj)->alloc_count = -1;
                ((Partial_Reveal_Object*)p_obj)->access_count = -1;
                ((Partial_Reveal_Object*)p_obj)->order_padding &= (~OBJ_INIT_ONLY);
                interleavingInfo->release_tid= pthread_self();
                unlock(interleavingInfo->lock);
                return THREAD_ID_NOT_MATCH;
#endif
            }
#endif
        }
		
    }
    else{ assert(0); }

    interleavingInfo->release_tid= pthread_self();
    unlock(interleavingInfo->lock);
	
    return NOT_EXIST_IN_DISK;
}
/***************************  Double Pool implement end *********************************************/


void gc_init_fetch_log()
{
    char name[40];
    for(int i = 0; i < ALLOC_FILE_NUM; i++){
        sprintf(name, "record_%d_alloc.log", i);
        allocLog[i] = fopen64(name, "r");
#ifdef ORDER_DEBUG
        if(allocLog[i] == NULL){
            printf("[replay]: could NOT open alloc log file %s\n", name);
        }
#endif
        fseeko64(allocLog[i], (U_64)0, SEEK_SET);
    }

    interLog = fopen64("orderInter.log", "r");
#ifdef ORDER_DEBUG
    if(interLog == NULL){
        printf("[replay]: could NOT open alloc log file orderInter.log\n");
    }
#endif
    fseeko64(interLog, (U_64)0, SEEK_SET);

#ifdef PREFETCH_FORWARDING
    forward_file =  fopen64("orderInter.log", "r");
#ifdef ORDER_DEBUG
    if(forward_file == NULL){
        printf("[replay]: could NOT open alloc log file orderInter.log\n");
    }
#endif
    fseeko64(forward_file, (U_64)0, SEEK_SET);

    fetch_start = 0;

    forwarding_pool = (ILInfo *)STD_MALLOC(FORWARD_POOL_SIZE);
#ifdef ORDER_DEBUG
    if(forwarding_pool == NULL){
        fprintf(stderr,"[xlc]: Could NOT malloc pool !\n");
        assert(0);
    }
#endif
#endif

    init_interleaving_pool();

    replay_verify = true;

}

void gc_finalize_fetch_log()
{
    for(int i = 0; i < ALLOC_FILE_NUM; i++){
        fclose(allocLog[i]);
    }
    fclose(interLog);

/*
    allocInfoPoolMap::iterator iter;
    for(iter = allocInfoPool.begin(); iter != allocInfoPool.end(); iter++){
        AllocInfo_Pool *temp = (AllocInfo_Pool *)iter->second;
        STD_FREE(temp->pool);
        STD_FREE(temp);
    }
*/
    for(int i = 0; i < ORDER_THREAD_NUM; i++){
        AllocInfo_Pool *temp = allocInfoPool[i];
        if(temp){
            STD_FREE(temp->pool);
            STD_FREE(temp);
        }
    }
    STD_FREE(interleavingInfo->pool);
    STD_FREE(interleavingInfo);

    STD_FREE(smallPool->pool);
    STD_FREE(smallPool);

#ifdef PREFETCH_FORWARDING
    STD_FREE(forwarding_pool);
#endif

#ifdef INTERLEAVING_CACHE
    STD_FREE(interCache->pool);
    STD_FREE(interCache);
#endif
}

void getAllocInfoFromLog(AllocInfo_Pool* allocPool, int file_num)
{
#ifdef ORDER_DEBUG
    assert(allocPool->used_num == allocPool->entry_num);
    assert(allocPool->pool != NULL);
#endif
    int num = fread((void *)allocPool->pool, AllocInfo_SIZE, AllocInfo_NUM, allocLog[file_num]);
    allocPool->entry_num = num;
    allocPool->used_num = 0;
}


AllocInfo_Pool *createAllocInfoPool()
{
    AllocInfo_Pool *allocPool = (AllocInfo_Pool *)STD_MALLOC(sizeof(AllocInfo_Pool));
#ifdef ORDER_DEBUG
    if(allocPool == NULL){
        fprintf(stderr,"[xlc]: Could NOT malloc allocPool!\n");
        assert(0);
    }
#endif
    memset(allocPool, 0, sizeof(AllocInfo_Pool));


    unsigned int pool_size = AllocInfo_SIZE * AllocInfo_NUM;
    Alloc_Info * pool = (Alloc_Info *)STD_MALLOC(pool_size);
#ifdef ORDER_DEBUG
    if(pool == NULL){
        fprintf(stderr,"[xlc]: Could NOT malloc pool !\n");
        assert(0);
    }
#endif
    memset(pool, 0, pool_size);

    allocPool->pool = pool;
    allocPool->entry_num = 0;
    allocPool->used_num = 0;
	
    return allocPool;
}


AllocInfo_Pool *getAllocInfoPool(U_16 alloc_tid)
{
//    allocInfoPoolMap::iterator iter;
 //   iter = allocInfoPool.find(alloc_tid);
/*
    //if NOT found
    if(iter == allocInfoPool.end()){
        AllocInfo_Pool * allocPool = createAllocInfoPool();
        getAllocInfoFromLog(allocPool, (int)alloc_tid);
        allocInfoPool[alloc_tid]  = allocPool;
    }
*/
    if(allocInfoPool[alloc_tid] == NULL){
        AllocInfo_Pool * allocPool = createAllocInfoPool();
        getAllocInfoFromLog(allocPool, (int)alloc_tid);
        allocInfoPool[alloc_tid]  = allocPool;
    }

    return (AllocInfo_Pool *)allocInfoPool[alloc_tid];
}

void getAllocInfo(Partial_Reveal_Object *p_obj)
{
    U_16 alloc_tid = p_obj->alloc_tid;
    U_32 alloc_count = p_obj->alloc_count;
    AllocInfo_Pool * allocPool = getAllocInfoPool(alloc_tid);
#ifdef ORDER_DEBUG
    assert(allocPool != NULL);
#endif

    //assert(allocPool->entry_num > 0);
    if(allocPool->entry_num > 0){
		
        Alloc_Info * allocInfo = &(allocPool->pool[allocPool->used_num]);
#ifdef REPLAY_OPTIMIZE
		if(p_obj->alloc_count != allocInfo->alloc_count){
#ifdef ORDER_DEBUG
			assert(p_obj->alloc_count < allocInfo->alloc_count);
#endif
                     p_obj->alloc_count_for_hashcode = p_obj->alloc_count;
			p_obj->alloc_count = -1;
			return;
		}
#endif
#ifdef ORDER_DEBUG
        assert(p_obj->alloc_count == allocInfo->alloc_count);
#endif
		allocPool->used_num++;

        if ((allocInfo->access_count & 0x80000000) != 0)
        {
            allocInfo->access_count &= 0x7FFFFFFF;
            p_obj->order_padding |= OBJ_INIT_ONLY;
//            printf("init only object : %d, %d\n", p_obj->alloc_tid, p_obj->alloc_count);
        }

        p_obj->access_count = allocInfo->access_count - 1;

        if(allocPool->used_num == allocPool->entry_num){
            getAllocInfoFromLog(allocPool, (int)alloc_tid);
        }

    }

    else{
#ifdef REPLAY_OPTIMIZE
                  p_obj->alloc_count_for_hashcode = p_obj->alloc_count;
		p_obj->alloc_count = -1;
#else
		assert(0);
		printf("[xlc]:the alloc count in tid %d now is %d\n", alloc_tid,  p_obj->alloc_count);
#endif
    }

}

/***************************  replay mode code end ****************************/

#endif //#ifdef ORDER

