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

#include "gc_thread.h"
#include "../gen/gen.h"
#include "../mark_sweep/gc_ms.h"
#include "../move_compact/gc_mc.h"
#include "../finalizer_weakref/finalizer_weakref.h"

#ifdef GC_GEN_STATS
#include "../gen/gen_stats.h"
#endif

#include <pthread.h>

#define ORDER_VERIFY

//#define GC_OBJ_SIZE_STATISTIC
#ifdef ORDER

#include "String_Pool.h"
#include "open/vm_util.h"

enum Class_State_Dummy {
    ST_Start,                   /// the initial state
    ST_LoadingAncestors,        /// the loading super class and super interfaces
    ST_Loaded,                  /// successfully loaded
    ST_BytecodesVerified,       /// bytecodes for methods verified for the class
    ST_InstanceSizeComputed,    /// preparing the class; instance size known
    ST_Prepared,                /// successfully prepared
    ST_ConstraintsVerified,     /// constraints verified for the class
    ST_Initializing,            /// initializing the class
    ST_Initialized,             /// the class initialized
    ST_Error                    /// bad class or the class initializer failed
};

struct ConstantPoolDummy {
private:
    static const unsigned char TAG_MASK = 0x0F;
    static const unsigned char ERROR_MASK = 0x40;
    static const unsigned char RESOLVED_MASK = 0x80;
    uint16 m_size;
    void* m_entries;
    void* m_failedResolution;
};

struct ClassDummy {
public:
    typedef struct {
        union {
            void* name;
            void* clss;
        };
        unsigned cp_index;
    } Class_Super;
    Class_Super m_super_class;
    const String* m_name;
    void* m_java_name;
    void* m_signature;
    void* m_simple_name;
    void* m_package;
    U_32 m_depth;
    int m_is_suitable_for_fast_instanceof;
    const char* m_class_file_name;
    void* m_src_file_name;
    unsigned m_id;
    void* m_class_loader;
    void** m_class_handle;
    uint16 m_version;
    uint16 m_access_flags;
    Class_State_Dummy m_state;
    bool m_deprecated;
    unsigned m_is_primitive : 1;
    unsigned m_is_array : 1;
    unsigned m_is_array_of_primitives : 1;
    unsigned m_has_finalizer : 1;
    unsigned m_can_access_all : 1;
    unsigned char m_is_fast_allocation_possible;
    unsigned int m_allocated_size;
    unsigned m_unpadded_instance_data_size;
    int m_alignment;
    unsigned m_instance_data_size;
    void* m_vtable;
    Allocation_Handle m_allocation_handle;
    unsigned m_num_virtual_method_entries;
    unsigned m_num_intfc_method_entries;
    void** m_vtable_descriptors;
    unsigned char m_num_dimensions;
    void* m_array_base_class;
    void* m_array_element_class;
    unsigned int m_array_element_size;
    unsigned int m_array_element_shift;
    uint16 m_num_superinterfaces;
    void* m_superinterfaces;
    ConstantPoolDummy m_const_pool;
    uint16 m_num_fields;
    uint16 m_num_static_fields;
    unsigned m_num_instance_refs;
    void* m_fields;
    unsigned m_static_data_size;
    void* m_static_data_block;
    uint16 m_num_methods;
    void* m_methods;
    void* m_finalize_method;
    void* m_static_initializer;
    void* m_default_constructor;
    uint16 m_declaring_class_index;
    uint16 m_enclosing_class_index;
    uint16 m_enclosing_method_index;
    uint16 m_num_innerclasses;

    struct InnerClass {
        uint16 index;
        uint16 access_flags;
    };
    void* m_innerclasses;
    void* m_annotations;
    void* m_invisible_annotations;
    void* m_initializing_thread;
    void* m_cha_first_child;
    void* m_cha_next_sibling;
    void* m_sourceDebugExtension;
    void* m_verify_data;
    void* m_lock;
    uint64 m_num_allocations;
    uint64 m_num_bytes_allocated;
    uint64 m_num_instanceof_slow;
    uint64 m_num_throws;
    uint64 m_num_class_init_checks;
    U_32 m_num_field_padding_bytes;
};

#define GC_BYTES_IN_VTABLE (sizeof(void*))
#define MAX_FAST_INSTOF_DEPTH 5

typedef struct GCVTableDummy {
    U_8 _gc_private_information[GC_BYTES_IN_VTABLE];
    void*             jlC; 
    unsigned int             vtmark; 
    ClassDummy* clss;

    // See the masks in vm_for_gc.h.
    U_32 class_properties;

    // Offset from the top by CLASS_ALLOCATED_SIZE_OFFSET
    // The number of bytes allocated for this object. It is the same as
    // instance_data_size with the constraint bit cleared. This includes
    // the OBJECT_HEADER_SIZE as well as the OBJECT_VTABLE_POINTER_SIZE
    unsigned int allocated_size;

    unsigned short array_element_size;
    unsigned short array_element_shift;
    
    // cached values, used for helper inlining to avoid extra memory access
    unsigned char** intfc_table_0;
    ClassDummy*          intfc_class_0;
    unsigned char** intfc_table_1;
    ClassDummy*          intfc_class_1;
    unsigned char** intfc_table_2;
    ClassDummy*          intfc_class_2;

    void* intfc_table;   // interface table; NULL if no intfc table
    ClassDummy *superclasses[MAX_FAST_INSTOF_DEPTH];
    unsigned char* methods[1];  // code for methods
} GCVTableDummy;

extern void* gc_class_class;
extern bool is_initializing;

extern char* method_names[32];
extern char* method_sigs[32];
extern char* class_names[32];

extern bool startTracing;

void init_obj_alloc_info(Partial_Reveal_Object*p_obj)
{
    if(order_record){
/***************************  record mode code start ****************************/
        U_16 alloc_tid = pthread_self();
        U_16 tid_in_harmony = (U_16)(hythread_self()->thread_id);

        p_obj->alloc_tid = tid_in_harmony;
        p_obj->access_tid = (U_32)pthread_self();
        p_obj->access_count = 1;
/*
        if (gc_class_class != NULL && gc_class_class == ((GCVTableDummy *)p_obj->vt_raw)->clss)
        {
            hythread_self()->alloc_count++;
            U_32 alloc_count = (U_32)(hythread_self()->alloc_count);
            p_obj->alloc_count = alloc_count;

            char * class_name = (char *)((GCVTableDummy *)p_obj->vt_raw)->clss->m_name->bytes;
            printf("alloc info : [clz](%d, %d) -> %d, %s\n", p_obj->alloc_tid,  p_obj->alloc_count, p_obj->access_count, class_name);
        }
        else if(hythread_self()->isInVMRegistry == 1){
            p_obj->alloc_count = -1;
            printf("Is In VM Registry!\n");
        }
        else if(is_initializing)
        {
            p_obj->alloc_count = -1;
        }
        else */ {
            hythread_self()->alloc_count++;
            U_32 alloc_count = (U_32)(hythread_self()->alloc_count);
            p_obj->alloc_count = alloc_count;
#if 0//#ifdef ORDER_DEBUG
            U_32 tid = hythread_self()->thread_id;

            if (!vm_is_vtable_compressed())
            {
                char * class_name = (char *)((GCVTableDummy *)p_obj->vt_raw)->clss->m_name->bytes;
                U_32 tid = hythread_self()->thread_id;
#ifdef ORDER_DEBUG
                printf("alloc info : (%d, %d) -> %d,  %s\n", p_obj->alloc_tid,  p_obj->alloc_count, p_obj->access_count, class_name);
#endif
            }

            else
            {
                GCVTableDummy* vt = (GCVTableDummy *) ((UDATA)p_obj->vt_raw + (UDATA)vm_get_vtable_base_address());
                char * class_name = (char *)vt->clss->m_name->bytes;
                U_32 tid = hythread_self()->thread_id;
#ifdef ORDER_DEBUG
                printf("alloc info : (%d, %d) -> %d,  %s\n", p_obj->alloc_tid,  p_obj->alloc_count, p_obj->access_count, class_name);
#endif
            }
 #endif      
        }
 
#ifdef ORDER_LOCAL_OPT
        set_obj_thread_local(p_obj);
#endif

#ifdef ORDER_DEBUG
        assert(p_obj->alloc_count > 0);
#endif
/***************************  record mode code end ****************************/
    }
    else{
/***************************  replay mode code start ****************************/
        U_16 alloc_tid = (U_16)(hythread_self()->thread_id);
    
        p_obj->alloc_tid = alloc_tid;
        p_obj->access_tid = (U_32)pthread_self();

/*
        if (gc_class_class != NULL && gc_class_class == ((GCVTableDummy *)p_obj->vt_raw)->clss)
        {
            hythread_self()->alloc_count++;
            U_32 alloc_count = (U_32)(hythread_self()->alloc_count);
            p_obj->alloc_count = alloc_count;

            char * class_name = (char *)((GCVTableDummy *)p_obj->vt_raw)->clss->m_name->bytes;
            printf("alloc info : [clz](%d, %d) -> %d, %s\n", p_obj->alloc_tid,  p_obj->alloc_count, p_obj->access_count, class_name);
        }
        else if(hythread_self()->isInVMRegistry == 1){
            p_obj->alloc_count = -1;
            p_obj->access_count = -1;
            printf("Is In VM Registry!\n");
        }
        else if(is_initializing)
        {
            p_obj->alloc_count = -1;
            p_obj->access_count = -1;
        }
        else */ {
            hythread_self()->alloc_count++;
            U_32 alloc_count = (U_32)(hythread_self()->alloc_count);
            p_obj->alloc_count = alloc_count;

#ifdef ORDER_DEBUG
            if(p_obj->alloc_count == 56 && p_obj->alloc_tid == 3){
		printf("here\n");
            }
#endif

            getAllocInfo(p_obj);

/*
            if (p_obj->alloc_tid==22)// &&  p_obj->alloc_count_for_hashcode== 405)
            {
                   GCVTableDummy* vt = (GCVTableDummy *) ((UDATA)p_obj->vt_raw + (UDATA)vm_get_vtable_base_address());
                   char * class_name = (char *)vt->clss->m_name->bytes;
                   U_32 tid = hythread_self()->thread_id;

                if (method_names[tid] && method_sigs[tid] && class_names[tid])
                    printf("alloc info : (%d, %d) -> %d,  %s, in (%s, %s, %s)\n", p_obj->alloc_tid,  p_obj->alloc_count, 
                        p_obj->access_count, class_name, method_names[tid], method_sigs[tid], class_names[tid]);
                else
                    printf("alloc info : (%d, %d) -> %d,  %s\n", p_obj->alloc_tid,  p_obj->alloc_count, 
                        p_obj->access_count, class_name);
            }
*/

#ifdef ORDER_DEBUG
 if (p_obj->alloc_tid==3&& p_obj->alloc_count == 67987)
 {
            if (!vm_is_vtable_compressed())
            {
                char * class_name = (char *)((GCVTableDummy *)p_obj->vt_raw)->clss->m_name->bytes;
                U_32 tid = hythread_self()->thread_id;

                if (method_names[tid] && method_sigs[tid] && class_names[tid])
                    printf("alloc info : (%d, %d) -> %d,  %s, in (%s, %s, %s)\n", p_obj->alloc_tid,  p_obj->alloc_count, 
                        p_obj->access_count, class_name, method_names[tid], method_sigs[tid], class_names[tid]);
                else
                    printf("alloc info : (%d, %d) -> %d,  %s\n", p_obj->alloc_tid,  p_obj->alloc_count, p_obj->access_count, class_name);
            }

            else
            {
                GCVTableDummy* vt = (GCVTableDummy *) ((UDATA)p_obj->vt_raw + (UDATA)vm_get_vtable_base_address());
                char * class_name = (char *)vt->clss->m_name->bytes;
                U_32 tid = hythread_self()->thread_id;

                 if (method_names[tid] && method_sigs[tid] && class_names[tid])
                    printf("alloc info : (%d, %d) -> %d,  %s, in (%s, %s, %s)\n", p_obj->alloc_tid,  p_obj->alloc_count, 
                        p_obj->access_count, class_name, method_names[tid], method_sigs[tid], class_names[tid]);
                else
                    printf("alloc info : (%d, %d) -> %d,  %s\n", p_obj->alloc_tid,  p_obj->alloc_count, p_obj->access_count, class_name);
            }
 }
//  if (p_obj->alloc_tid == 30 && p_obj->alloc_count == 134)
//  	assert(0);

#endif


        }
/***************************  replay mode code end ****************************/
    }

}

#endif




#ifdef GC_OBJ_SIZE_STATISTIC
#define GC_OBJ_SIZE_STA_MAX 256*KB
unsigned int obj_size_distribution_map[GC_OBJ_SIZE_STA_MAX>>10];
void gc_alloc_statistic_obj_distrubution(unsigned int size)
{
    unsigned int sta_precision = 16*KB;
    unsigned int max_sta_size = 128*KB;
    unsigned int sta_current = 0;    

    assert(!(GC_OBJ_SIZE_STA_MAX % sta_precision));
    assert(!(max_sta_size % sta_precision));    
    while( sta_current < max_sta_size ){
        if(size < sta_current){
            unsigned int index = sta_current >> 10;
            obj_size_distribution_map[index] ++;
            return;
        }
        sta_current += sta_precision;
    }
    unsigned int index = sta_current >> 10;
    obj_size_distribution_map[index]++;
    return;
}
#endif

extern Boolean mutator_need_block;

Managed_Object_Handle gc_alloc(unsigned size, Allocation_Handle ah, void *unused_gc_tls) 
{
  Managed_Object_Handle p_obj = NULL;
 
  /* All requests for space should be multiples of 4 (IA32) or 8(IPF) */
  assert((size % GC_OBJECT_ALIGNMENT) == 0);
  assert(ah);

  size = (size & NEXT_TO_HIGH_BIT_CLEAR_MASK);
  
  Allocator* allocator = (Allocator*)gc_get_tls();
  Boolean type_has_fin = type_has_finalizer((Partial_Reveal_VTable*)decode_vt((VT)ah));
  
  if(type_has_fin && !IGNORE_FINREF && mutator_need_block)
    vm_heavy_finalizer_block_mutator();

#ifdef GC_OBJ_SIZE_STATISTIC
  gc_alloc_statistic_obj_distrubution(size);
#endif

#if defined(USE_UNIQUE_MARK_SWEEP_GC)

  p_obj = (Managed_Object_Handle)gc_ms_alloc(size, allocator);

#elif defined(USE_UNIQUE_MOVE_COMPACT_GC)

  p_obj = (Managed_Object_Handle)gc_mc_alloc(size, allocator);

#else

  if ( size > GC_LOS_OBJ_SIZE_THRESHOLD ){
    p_obj = (Managed_Object_Handle)los_alloc(size, allocator);

#ifdef GC_GEN_STATS
    if (p_obj != NULL){
      GC_Gen* gc = (GC_Gen*)allocator->gc;
      gc->stats->obj_num_los_alloc++;
      gc->stats->total_size_los_alloc += size;
    }
#endif /* #ifdef GC_GEN_STATS */

  }else{
      p_obj = (Managed_Object_Handle)nos_alloc(size, allocator);
  }

#endif /* defined(USE_UNIQUE_MARK_SWEEP_GC) else */

  if( p_obj == NULL )
    return NULL;

  assert((((POINTER_SIZE_INT)p_obj) % GC_OBJECT_ALIGNMENT) == 0);
    
  obj_set_vt((Partial_Reveal_Object*)p_obj, (VT)ah);
  
  if(type_has_fin && !IGNORE_FINREF)
    mutator_add_finalizer((Mutator*)allocator, (Partial_Reveal_Object*)p_obj);

#ifdef ORDER  
  init_obj_alloc_info((Partial_Reveal_Object*)p_obj);
#endif
    
  return (Managed_Object_Handle)p_obj;
}


Managed_Object_Handle gc_alloc_fast (unsigned size, Allocation_Handle ah, void *unused_gc_tls) 
{
  /* All requests for space should be multiples of 4 (IA32) or 8(IPF) */
  assert((size % GC_OBJECT_ALIGNMENT) == 0);
  assert(ah);
  
  if(type_has_finalizer((Partial_Reveal_VTable *) decode_vt((VT)ah)))
    return NULL;

#ifdef GC_OBJ_SIZE_STATISTIC
  gc_alloc_statistic_obj_distrubution(size);
#endif
   
  Allocator* allocator = (Allocator*)gc_get_tls();
 
  /* Try to allocate an object from the current Thread Local Block */
  Managed_Object_Handle p_obj;

#if defined(USE_UNIQUE_MARK_SWEEP_GC)

  p_obj = (Managed_Object_Handle)gc_ms_fast_alloc(size, allocator);

#elif defined(USE_UNIQUE_MOVE_COMPACT_GC)

  if ( size > GC_LARGE_OBJ_SIZE_THRESHOLD ) return NULL;
  p_obj = (Managed_Object_Handle)thread_local_alloc(size, allocator);

#else
  /* object shoud be handled specially */
  if ( size > GC_LOS_OBJ_SIZE_THRESHOLD ) return NULL;
  p_obj = (Managed_Object_Handle)thread_local_alloc(size, allocator);

#endif

  if(p_obj == NULL) return NULL;

  assert((((POINTER_SIZE_INT)p_obj) % GC_OBJECT_ALIGNMENT) == 0);
  obj_set_vt((Partial_Reveal_Object*)p_obj, (VT)ah);

#ifdef ORDER
  init_obj_alloc_info((Partial_Reveal_Object*)p_obj);
#endif

  return p_obj;
}



