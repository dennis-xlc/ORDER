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
#define LOG_DOMAIN "notify"
#include "cxxlog.h"

#include "platform_lowlevel.h"
#include "open/types.h"
#include "lock_manager.h"
#include "environment.h"
#include "exceptions.h"
#include "jni_utils.h"
#include "native_utils.h"
#include "vm_arrays.h"

#include "thread_generic.h"

#include "jthread.h"
#include "thread_manager.h"
#include "object.h"
#include "object_generic.h"
#include "mon_enter_exit.h"
#include "port_atomic.h"

#include "jit_runtime_support_common.h"

#include "vm_process.h"
#include "interpreter.h"
#include "vm_log.h"

#ifdef ORDER
extern bool vm_order_record;
extern FILE *order_system_call[];

typedef struct HashcodeInfo{
    U_32 alloc_count;
    U_32 hashCode;
    U_16 alloc_tid;   	
}HashcodeInfo;

#define HASHCODESIZE sizeof(HashcodeInfo)
#endif


void set_hash_bits(ManagedObject *p_obj)
    {
    U_8 hb = (U_8) (((POINTER_SIZE_INT)p_obj >> 3) & HASH_MASK)  ;
    // lowest 3 bits are not random enough so get rid of them

    if (hb == 0)
        hb = (23 & HASH_MASK);  // NO hash = zero allowed, thus hard map hb = 0 to a fixed prime number

    // don't care if the cmpxchg fails -- just means someone else already set the hash
    port_atomic_cas8(P_HASH_CONTENTION_BYTE(p_obj),hb, 0);
}

/* $$$ GMJ
long generic_hashcode(ManagedObject *obj) {
    return (long) gc_get_hashcode(obj);
}
*/

#ifdef ORDER
jint order_generic_hashcode(ManagedObject *pobj)
{
    if(vm_order_record){
        return (pobj->alloc_tid << 26 |pobj->alloc_count);
    }
    else
    {
        if (pobj->alloc_count == -1)
        {
            return (pobj->alloc_tid << 26 |pobj->alloc_count_for_hashcode);
        }
        else
        {
            return (pobj->alloc_tid << 26 |pobj->alloc_count);
        }
    }
}
#endif

jint default_hashcode(Managed_Object_Handle obj) {
    ManagedObject *p_obj = (ManagedObject*) obj;

    if (!p_obj) return 0L;

    if ( *P_HASH_CONTENTION_BYTE(p_obj) & HASH_MASK)
        return *P_HASH_CONTENTION_BYTE(p_obj) & HASH_MASK;

    set_hash_bits(p_obj);

    if ( *P_HASH_CONTENTION_BYTE(p_obj) & HASH_MASK)
        return *P_HASH_CONTENTION_BYTE(p_obj) & HASH_MASK;

    DIE(("All the possible cases are supposed to be covered before"));
    return 0xff;
}

long generic_hashcode(ManagedObject * p_obj)
{
    return (long) gc_get_hashcode0((Managed_Object_Handle) p_obj);
}


jint object_get_generic_hashcode(JNIEnv*, jobject jobj)
{
    jint hash;
    if (jobj != NULL) {
#ifdef ORDER
        hash = order_generic_hashcode((ManagedObject*)((ObjectHandle)jobj)->object);
#else
        hash = generic_hashcode(((ObjectHandle)jobj)->object);
#endif
/*
#ifdef ORDER
        ManagedObject *p_obj = (ManagedObject *)((ObjectHandle)jobj)->object;
        HashcodeInfo hashCodeInfo;
        if(vm_order_record){
            U_32 tid = hythread_self()->thread_id;
            if (order_system_call[tid] == NULL)
            {
                char name[40];
                sprintf(name, "SYSTEM_CALL.%d.log", tid);

                order_system_call[tid] = fopen64(name, "a+");
#ifdef ORDER_DEBUG
                if(order_system_call[tid]== NULL){
                    printf("[ORDER]: could NOT open  file %s\n", name);
                }
#endif
                fseek(order_system_call[tid], 0, SEEK_SET);
            }
#ifdef ORDER_DEBUG
            fprintf(order_system_call[tid], "[%d] %d %d ", 4, p_obj->alloc_tid, p_obj->alloc_count);
#endif
            fprintf(order_system_call[tid], "%d\n", hash);
//            fflush(order_system_call[tid]);
            //printf("[ORDER]: OBJECT HASHCODE : object[%d, %d] == > %d\n", p_obj->alloc_tid, p_obj->alloc_count, hash);
        }
        else
        {
            U_32 tid = hythread_self()->thread_id;
            if (order_system_call[tid] == NULL)
            {
                char name[40];
                sprintf(name, "SYSTEM_CALL.%d.log", tid);
    
                order_system_call[tid] = fopen64(name, "r");
#ifdef ORDER_DEBUG
                if(order_system_call[tid]== NULL){
                    printf("[ORDER]: could NOT open  file %s\n", name);
                }
#endif
                fseek(order_system_call[tid], 0, SEEK_SET);
            }

#ifdef ORDER_DEBUG
            int bit_num;
            fscanf(order_system_call[tid], "[%d]", &bit_num);
            assert(bit_num == 4);

            fscanf(order_system_call[tid], "%d ", &(hashCodeInfo.alloc_tid));
            fscanf(order_system_call[tid], "%d ", &(hashCodeInfo.alloc_count));
#endif
            fscanf(order_system_call[tid], "%d\n", &(hashCodeInfo.hashCode));

#ifdef ORDER_DEBUG
            assert(hashCodeInfo.alloc_tid == p_obj->alloc_tid);
            assert(hashCodeInfo.alloc_count == p_obj->alloc_count || p_obj->alloc_count == -1);
#endif
            hash = hashCodeInfo.hashCode;
            //printf("[ORDER]: OBJECT HASHCODE : object[%d, %d] == > %d\n", p_obj->alloc_tid, p_obj->alloc_count, hash);
         }
#endif
*/
    } else {
        hash = 0;
    }
    return hash;
}

jobject object_clone(JNIEnv *jenv, jobject jobj)
{
    ASSERT_RAISE_AREA;
    ManagedObject *result;
    assert(hythread_is_suspend_enabled());
    if(!jobj) {
        // Throw NullPointerException.
        throw_exception_from_jni(jenv, "java/lang/NullPointerException", 0);
        return NULL;
    }
    tmn_suspend_disable();
    ObjectHandle h = (ObjectHandle) jobj;
    VTable *vt = h->object->vt();
    unsigned size;
    if((vt->class_properties & CL_PROP_ARRAY_MASK) != 0)
    {
        // clone an array
        I_32 length = get_vector_length((Vector_Handle) h->object);
        size = vt->clss->calculate_array_size(length);
        assert(size > 0);
        result = (ManagedObject*)
            vm_new_vector_using_vtable_and_thread_pointer(length,
                vt->clss->get_allocation_handle(), vm_get_gc_thread_local());
    }
    else
    {
        // clone an object
        Global_Env *global_env = VM_Global_State::loader_env;
        if (!class_is_subtype_fast(h->object->vt(), global_env->java_lang_Cloneable_Class))
        {
            tmn_suspend_enable(); 
            throw_exception_from_jni(jenv, "java/lang/CloneNotSupportedException", 0);
            return NULL;
        }
        size = vt->allocated_size;
        result = vt->clss->allocate_instance();
    }
    if (result == NULL) {
        tmn_suspend_enable(); 
        exn_raise_object(VM_Global_State::loader_env->java_lang_OutOfMemoryError);
        return NULL;
    }
    if(gc_requires_barriers())
        gc_heap_wrote_object(result);

    // Gregory - Skip object header copying, it should be initialized
    // by GC already, and copying may erase some information that GC
    // wrote in it at allocation time
    const size_t skip = ManagedObject::get_constant_header_size();
    assert(skip <= size);
    jbyte *dest = (jbyte *)result + skip;
    jbyte *src = (jbyte *)h->object + skip;
    memcpy(dest, src, size - skip);

    ObjectHandle new_handle = oh_allocate_local_handle();
    new_handle->object = result;
    tmn_suspend_enable(); 
    return (jobject) new_handle;
}
