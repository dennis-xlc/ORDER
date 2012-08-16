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

#ifndef _MUTATOR_H_
#define _MUTATOR_H_

#include "../common/gc_space.h"

#ifdef ORDER
/***************************  record mode code start ****************************/

#define _FILE_OFFSET_BITS 64

#include <stdio.h>
//#include <fcntl.h>
#include   <unistd.h>   
#include   <dirent.h>

#include <map>
using std::map;




typedef struct Access_Info {
    U_32 access_count;
    U_32 alloc_count;
    U_32 access_tid;
    U_16 alloc_tid;
#ifdef REPLAY_OPTIMIZE
	U_16 rw_bit;
#endif
} Access_Info;

typedef struct Access_Pool{
    Access_Info *pool;
    POINTER_SIZE_INT used_num;
    POINTER_SIZE_INT fill_num;
    SpinLock lock;
} Access_Pool;

enum SWAP_CAUSE{
  NIL,
  POOL_DESTRUCT,
  POOL_IS_FULL
};

#define ACCESS_INFO_NUM (1<<20) // the number of access info in every local access pool
#define MAX_POOL_NUM 10
#define ACCESS_INFO_SIZE  sizeof(Access_Info)

void *alloc_access_pool();
void reset_access_pool(Access_Info *access_pool);
void free_access_pool(Access_Info *access_pool);
void init_global_access_pool();
void record_access_info(Partial_Reveal_Object* p_obj);
void swap_access_pool_info(Access_Pool *pool, unsigned int swap_cause);
void destruct_global_access_pool();

/***************************  record mode code end ****************************/

/***************************  replay mode code start **************************/
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
//#include <fcntl.h>
#include   <unistd.h>   
#include   <dirent.h>
#include  <string.h>




/*  object alloc info */
typedef struct Alloc_Info {
    U_32 access_count;
    U_32 alloc_count;
} Alloc_Info;

/* the pool which store alloc info in meomry */
typedef struct AllocInfo_Pool{
    Alloc_Info *pool;
    U_32 entry_num;
    U_32 used_num;
} AllocInfo_Pool;

#define AllocInfo_SIZE sizeof(Alloc_Info)
#define AllocInfo_NUM 1000


/* object access interleaving info */
typedef struct ILInfo{
    U_32 alloc_count;
    U_32 last_tid;
    U_32 next_acc_count;
    U_32 next_tid;
    U_16 alloc_tid;   	
}ILInfo;

/* the pool which store interleaving info in meomry */
typedef struct ILInfo_Pool{
    ILInfo *pool;
    U_32 entry_num;
    U_32 used_num;
    SpinLock lock;
    U_32 require_tid;
    U_32 release_tid;
} ILInfo_Pool;

enum InterState{
  FOUND_IN_POOL,
  THREAD_ID_NOT_MATCH,
  NOT_FOUND_IN_POOL,
  NOT_EXIST_IN_DISK
};

#define ILInfo_SIZE sizeof(ILInfo)
#define ILInfo_NUM 10000
#define SMALLPOOL_CHUNK 20
#define SMALLPOOL_NUM 100


#include <map>
using std::map;

typedef map<U_16, AllocInfo_Pool *> allocInfoPoolMap;//alloc_tid ==> AllocInfo_Pool 

#define ALLOC_FILE_NUM ORDER_THREAD_NUM

extern U_32 tid_pthreadid_mapping[ORDER_THREAD_NUM][2];


void gc_init_fetch_log();
void gc_finalize_fetch_log();
void getAllocInfo(Partial_Reveal_Object *p_obj);
InterState getInterleavingInfo(Partial_Reveal_Object *p_obj);

/***************************  replay mode code end **************************/
#endif //#ifdef ORDER

struct Chunk_Header;

/* Mutator thread local information for GC */
typedef struct Mutator {
  /* <-- first couple of fields are overloaded as Allocator */
  void* free;
  void* ceiling;
  void* end;
  void* alloc_block;
  Chunk_Header ***local_chunks;
  Space* alloc_space;
  GC* gc;
  VmThreadHandle thread_handle;   /* This thread; */
  volatile unsigned int handshake_signal; /*Handshake is used in concurrent GC.*/
  unsigned int num_alloc_blocks; /* the number of allocated blocks since last collection. */
  int64 time_measurement_start;
  int64 time_measurement_end;
  /* END of Allocator --> */
  
  Vector_Block* rem_set;
  Vector_Block* obj_with_fin;
  Mutator* next;  /* The gc info area associated with the next active thread. */
  Vector_Block* dirty_set;
  SpinLock dirty_set_lock;
  unsigned int dirty_obj_slot_num; //only ON_THE_FLY
  unsigned int dirty_obj_num; //concurrent mark  
  
  /* obj alloc information */
  POINTER_SIZE_INT new_obj_size;
  /* accurate object number and total size*/
  POINTER_SIZE_INT new_obj_num;
  POINTER_SIZE_INT new_obj_occupied_size;
  POINTER_SIZE_INT write_barrier_marked_size;

} Mutator;

void mutator_initialize(GC* gc, void* tls_gc_info);
void mutator_destruct(GC* gc, void* tls_gc_info); 
void mutator_reset(GC *gc);

void gc_reset_mutator_context(GC* gc);
void gc_prepare_mutator_remset(GC* gc);

Boolean gc_local_dirtyset_is_empty(GC* gc);
Vector_Block* gc_get_local_dirty_set(GC* gc, unsigned int shared_id);
void gc_start_mutator_time_measure(GC* gc);
int64 gc_get_mutator_time(GC* gc);

unsigned int gc_get_mutator_write_barrier_marked_size( GC *gc );
unsigned int gc_get_mutator_dirty_obj_num(GC *gc);
unsigned int gc_get_mutator_new_obj_size( GC* gc );
unsigned int gc_reset_mutator_new_obj_size( GC* gc );

inline void mutator_post_signal(Mutator* mutator, unsigned int handshake_signal)
{ 
  mem_fence();
  mutator->handshake_signal = handshake_signal; 
  mem_fence();
}

inline void wait_mutator_signal(Mutator* mutator, unsigned int handshake_signal)
{ while(mutator->handshake_signal != handshake_signal); }


#endif /*ifndef _MUTATOR_H_ */
