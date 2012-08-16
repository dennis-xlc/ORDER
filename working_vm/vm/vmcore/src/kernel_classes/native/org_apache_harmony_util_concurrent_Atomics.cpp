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
 * @author Andrey Chernyshev
 */  

#include "jni.h"
#include "jni_direct.h"
#include "atomics.h"
#include "port_barriers.h"
#include "org_apache_harmony_util_concurrent_Atomics.h"

#include "object_handles.h"

JNIEXPORT jint JNICALL Java_org_apache_harmony_util_concurrent_Atomics_arrayBaseOffset
  (JNIEnv * env, jclass self, jclass array)
{
    jlong array_element_size = Java_org_apache_harmony_util_concurrent_Atomics_arrayIndexScale(env, self, array);
    if(array_element_size < 8) {
        return VM_VECTOR_FIRST_ELEM_OFFSET_1_2_4;
    } else {
        return VM_VECTOR_FIRST_ELEM_OFFSET_8;
    }
}

JNIEXPORT jint JNICALL Java_org_apache_harmony_util_concurrent_Atomics_arrayIndexScale
  (JNIEnv * env, jclass self, jclass array)
{
    Class * clz = jclass_to_struct_Class(array);
    return clz->get_array_element_size();
}

JNIEXPORT void JNICALL 
Java_org_apache_harmony_util_concurrent_Atomics_setIntVolatile__Ljava_lang_Object_2JI(JNIEnv * env, jclass self, 
    jobject obj, jlong offset, jint value)
{
    SetIntFieldOffset(env, obj, (jint)offset, value);
    port_rw_barrier();
}


JNIEXPORT jint JNICALL 
Java_org_apache_harmony_util_concurrent_Atomics_getIntVolatile__Ljava_lang_Object_2J(JNIEnv * env, jclass self, 
    jobject obj, jlong offset)
{
    port_rw_barrier();
    return GetIntFieldOffset(env, obj, (jint)offset);
}

JNIEXPORT void JNICALL 
Java_org_apache_harmony_util_concurrent_Atomics_setLongVolatile__Ljava_lang_Object_2JJ(JNIEnv * env, jclass self, 
    jobject obj, jlong offset, jlong value)
{
    SetLongFieldOffset(env, obj, (jint)offset, value);
    port_rw_barrier();
}


JNIEXPORT jlong JNICALL 
Java_org_apache_harmony_util_concurrent_Atomics_getLongVolatile__Ljava_lang_Object_2J(JNIEnv * env, jclass self, 
    jobject obj, jlong offset)
{
    port_rw_barrier();
    return GetLongFieldOffset(env, obj, (jint)offset);
}

JNIEXPORT void JNICALL 
Java_org_apache_harmony_util_concurrent_Atomics_setObjectVolatile__Ljava_lang_Object_2JLjava_lang_Object_2(JNIEnv * env, jclass self, 
    jobject obj, jlong offset, jobject value)
{
    SetObjectFieldOffset(env, obj, (jint)offset, value);
    port_rw_barrier();
}


JNIEXPORT jobject JNICALL 
Java_org_apache_harmony_util_concurrent_Atomics_getObjectVolatile__Ljava_lang_Object_2J(JNIEnv * env, jclass self, 
    jobject obj, jlong offset)
{
    port_rw_barrier();
    return GetObjectFieldOffset(env, obj, (jint)offset);
}

JNIEXPORT jlong JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_getFieldOffset(JNIEnv * env, jclass self, 
    jobject field)
{
    return getFieldOffset(env, field);
}

#ifdef ORDER
#include "port_atomic.h"

#define RECORD_AS_ACCESS

extern bool vm_order_record;
extern bool vm_is_initializing;
extern FILE *order_system_call[];

#include "open/hythread_ext.h"

#ifdef REPLAY_OPTIMIZE
#define OBJ_RW_BIT 0x800
#endif

#endif

JNIEXPORT jboolean JNICALL 
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetObject__Ljava_lang_Object_2JLjava_lang_Object_2Ljava_lang_Object_2
(JNIEnv * env, jobject self, jobject obj, jlong offset, jobject expected, jobject value)

{

#ifdef ORDER
#ifdef RECORD_AS_ACCESS
/****************************  RECORD AS ACCESS ***********************************/
    hythread_suspend_disable();
    ObjectHandle obj_handle = (ObjectHandle)obj;
    ManagedObject* p_obj = (obj_handle==NULL)?NULL:obj_handle->object;
    hythread_suspend_enable();
    assert(p_obj != NULL);
    if(vm_order_record){
        if (!vm_is_initializing && p_obj->alloc_count != -1)
        {

            while (port_atomic_cas8(&p_obj->access_lock, 1, 0) != 0)
            {
#ifdef ORDER_DEBUG
                printf("usleep in compareAndSetInt, %d, %d, thread : %d\n", p_obj->alloc_count, p_obj->access_lock, hythread_self()->thread_id);
                if (hythread_self()->request != 0) printf("request to lock thread : %d\n", hythread_self()->thread_id);
#endif
                usleep(1000);
            }

            if (p_obj->access_tid != (U_32)pthread_self())
            {
                gc_record_info(p_obj);
                p_obj->access_tid = (U_32)pthread_self();
                p_obj->access_count = 0;
            }

            p_obj->access_count ++;
#ifdef REPLAY_OPTIMIZE
            p_obj->order_padding |= OBJ_RW_BIT; //write
#endif


        }
    }
    else{
check_lock:
        if (!vm_is_initializing && p_obj->alloc_count != -1)
        {
            while (port_atomic_cas8(&p_obj->access_lock, 1, 0) != 0)
            {
#ifdef ORDER_DEBUG
                printf("usleep in monitor enter, (%d, %d), %d\n", p_obj->alloc_tid, p_obj->alloc_count, hythread_self()->thread_id);
                if (hythread_self()->request != 0) printf("request to lock thread : %d\n", hythread_self()->thread_id);
#endif
                usleep(1000);
                hythread_yield();
                hythread_safe_point();
                hythread_exception_safe_point();
            }

            if (p_obj->access_count == 0){
                U_8 result = gc_get_interleaving_info(p_obj);
	       if (result == 0){
                    p_obj->access_lock = 0;
                    usleep(1000);
                    hythread_yield();
                    hythread_safe_point();
                    hythread_exception_safe_point();
                    goto check_lock;
	       }
            }
            else{
                while(p_obj->access_tid != (U_32)pthread_self()){
#ifdef ORDER_DEBUG
                    printf("usleep in monitor enter, tid not match, (%d, %d), current : %d, expected : %d, remaining : %d\n", p_obj->alloc_tid, p_obj->alloc_count, hythread_self()->thread_id, p_obj->access_tid, p_obj->access_count);
                    assert(p_obj->alloc_tid != 0);
                    if (hythread_self()->request != 0) printf("request to lock thread : %d\n", hythread_self()->thread_id);
#endif
                    p_obj->access_lock = 0;
                    usleep(1000);
                    hythread_yield();
                    hythread_safe_point();
                    hythread_exception_safe_point();
                    goto check_lock;
                }
            }    

            p_obj->access_count --;
#ifdef REPLAY_OPTIMIZE
            p_obj->order_padding |= OBJ_RW_BIT; //write
#endif
        }
    }
#endif //#ifdef RECORD_AS_ACCESS

/***************************** RECORD AS SYSTEM CALL *************************/
#ifdef ORDER_DEBUG
    hythread_suspend_disable();
    ObjectHandle v = (ObjectHandle)obj;
    ManagedObject* val = (v==NULL)?NULL:v->object;
    U_32 alloc_count = 0;
    U_32 alloc_tid = 0;
    if (val) {
        alloc_count = val->alloc_count;
        alloc_tid = val->alloc_tid;
        if(alloc_count == -1){
            alloc_count = val->alloc_count_for_hashcode;
        }
    }
    hythread_suspend_enable();
#endif

    jboolean result = compareAndSetObjectField(env, self, obj, offset, expected, value);

    if(vm_order_record){

        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
            assert(order_system_call[tid] != NULL);
        }
		
#ifdef ORDER_DEBUG

        fprintf(order_system_call[tid], "[%d] ", 25);

//        fprintf(order_system_call[tid], "%d ", alloc_count);
#endif


#ifdef ORDER_DEBUG
        fprintf(order_system_call[tid], "%d %d %d\n", alloc_count, alloc_tid, result);
#else
        fprintf(order_system_call[tid], "%d\n", result);
#endif

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
            assert(order_system_call[tid] != NULL);
#endif
        }
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 25);

/*
        U_32 alloc_count_temp = 0;
        fscanf(order_system_call[tid], "%d ", &alloc_count_temp);
        assert(alloc_count_temp == alloc_count);
*/
#endif
        jboolean result_log = JNI_FALSE;
#ifdef ORDER_DEBUG
        U_32 alloc_count_log=0;
        U_32 alloc_tid_log = 0;
        fscanf(order_system_call[tid], "%d %d %d\n", &alloc_count_log, &alloc_tid_log, &result_log);
        assert(alloc_count_log == alloc_count);
        assert(alloc_tid_log == alloc_tid);

#else
        fscanf(order_system_call[tid], "%d\n", &result_log);
#endif
        assert(result_log == result);

     }

#ifdef RECORD_AS_ACCESS
     p_obj->access_lock = 0;
#endif
     return result;

#else  //#ifdef ORDER
    return compareAndSetObjectField(env, self, obj, offset, expected, value);
#endif



#if 0
#ifdef ORDER
/*
    hythread_suspend_disable();
    ObjectHandle v = (ObjectHandle)value;
    ManagedObject* val = (v==NULL)?NULL:v->object;
    U_32 alloc_count = 0;
    if (val) {alloc_count = val->alloc_count;}
    hythread_suspend_enable();
*/
    if(vm_order_record){

        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
            assert(order_system_call[tid] != NULL);
        }
		
#ifdef ORDER_DEBUG

        fprintf(order_system_call[tid], "[%d] ", 25);

//        fprintf(order_system_call[tid], "%d ", alloc_count);
#endif
        jboolean result = compareAndSetObjectField(env, self, obj, offset, expected, value);

        fprintf(order_system_call[tid], "%d\n", result);
        return result;
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
            assert(order_system_call[tid] != NULL);
#endif
        }
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 25);

/*
        U_32 alloc_count_temp = 0;
        fscanf(order_system_call[tid], "%d ", &alloc_count_temp);
        assert(alloc_count_temp == alloc_count);
*/
#endif

        jboolean result = JNI_FALSE;
        fscanf(order_system_call[tid], "%d\n", &result);
        if (result == JNI_TRUE)
        {
           while(compareAndSetObjectField(env, self, obj, offset, expected, value) != JNI_TRUE)
           {
               usleep(1000);
           }
           return JNI_TRUE;
        }
        else
        {
#ifdef ORDER_DEBUG
            assert(JNI_FALSE == result);
#endif
            return JNI_FALSE;
        }
     }
#else
    return compareAndSetObjectField(env, self, obj, offset, expected, value);
#endif

#endif // #if 0 
}
 
 
JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetBoolean__Ljava_lang_Object_2JZZ 
(JNIEnv * env, jobject self, jobject obj, jlong offset, jboolean expected, jboolean value)
{
#ifdef ORDER
#ifdef RECORD_AS_ACCESS
/****************************  RECORD AS ACCESS ***********************************/
    hythread_suspend_disable();
    ObjectHandle obj_handle = (ObjectHandle)obj;
    ManagedObject* p_obj = (obj_handle==NULL)?NULL:obj_handle->object;
    hythread_suspend_enable();
    assert(p_obj != NULL);
    if(vm_order_record){
        if (!vm_is_initializing && p_obj->alloc_count != -1)
        {

            while (port_atomic_cas8(&p_obj->access_lock, 1, 0) != 0)
            {
#ifdef ORDER_DEBUG
                printf("usleep in compareAndSetInt, %d, %d, thread : %d\n", p_obj->alloc_count, p_obj->access_lock, hythread_self()->thread_id);
                if (hythread_self()->request != 0) printf("request to lock thread : %d\n", hythread_self()->thread_id);
#endif
                usleep(1000);
            }

            if (p_obj->access_tid != (U_32)pthread_self())
            {
                gc_record_info(p_obj);
                p_obj->access_tid = (U_32)pthread_self();
                p_obj->access_count = 0;
            }

            p_obj->access_count ++;
#ifdef REPLAY_OPTIMIZE
            p_obj->order_padding |= OBJ_RW_BIT; //write
#endif


        }
    }
    else{
check_lock:
        if (!vm_is_initializing && p_obj->alloc_count != -1)
        {
            while (port_atomic_cas8(&p_obj->access_lock, 1, 0) != 0)
            {
#ifdef ORDER_DEBUG
                printf("usleep in monitor enter, (%d, %d), %d\n", p_obj->alloc_tid, p_obj->alloc_count, hythread_self()->thread_id);
                if (hythread_self()->request != 0) printf("request to lock thread : %d\n", hythread_self()->thread_id);
#endif
                usleep(1000);
                hythread_yield();
                hythread_safe_point();
                hythread_exception_safe_point();
                p_obj = obj_handle->object;
#ifdef ORDER_DEBUG
                assert(p_obj != NULL);
#endif
            }

            if (p_obj->access_count == 0){
                U_8 result = gc_get_interleaving_info(p_obj);
	       if (result == 0){
                    p_obj->access_lock = 0;
                    usleep(1000);
                    hythread_yield();
                    hythread_safe_point();
                    hythread_exception_safe_point();
                    p_obj = obj_handle->object;
#ifdef ORDER_DEBUG
                    assert(p_obj != NULL);
#endif
                    goto check_lock;
	       }
            }
            else{
                while(p_obj->access_tid != (U_32)pthread_self()){
#ifdef ORDER_DEBUG
                    printf("usleep in monitor enter, tid not match, (%d, %d), current : %d, expected : %d, remaining : %d\n", p_obj->alloc_tid, p_obj->alloc_count, hythread_self()->thread_id, p_obj->access_tid, p_obj->access_count);
                    assert(p_obj->alloc_tid != 0);
                    if (hythread_self()->request != 0) printf("request to lock thread : %d\n", hythread_self()->thread_id);
#endif
                    p_obj->access_lock = 0;
                    usleep(1000);
                    hythread_yield();
                    hythread_safe_point();
                    hythread_exception_safe_point();
                    p_obj = obj_handle->object;
#ifdef ORDER_DEBUG
                    assert(p_obj != NULL);
#endif
                    goto check_lock;
                }
            }    

            p_obj->access_count --;
#ifdef REPLAY_OPTIMIZE
            p_obj->order_padding |= OBJ_RW_BIT; //write
#endif
        }
    }
#endif //#ifdef RECORD_AS_ACCESS

/***************************** RECORD AS SYSTEM CALL *************************/
#ifdef ORDER_DEBUG
    hythread_suspend_disable();
    ObjectHandle v = (ObjectHandle)obj;
    ManagedObject* val = (v==NULL)?NULL:v->object;
    U_32 alloc_count = 0;
    U_32 alloc_tid = 0;
    if (val) {
        alloc_count = val->alloc_count;
        alloc_tid = val->alloc_tid;
        if(alloc_count == -1){
            alloc_count = val->alloc_count_for_hashcode;
        }
    }
    hythread_suspend_enable();
#endif
    U_32 old_val = 0;
    jboolean result = compareAndSetBooleanField(env, self, obj, offset, expected, value, &old_val);

    if(vm_order_record){

        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
            assert(order_system_call[tid] != NULL);
        }
		
#ifdef ORDER_DEBUG

        fprintf(order_system_call[tid], "[%d] ", 26);

//        fprintf(order_system_call[tid], "%d ", alloc_count);
#endif


#ifdef ORDER_DEBUG
        fprintf(order_system_call[tid], "%d %d %d %d %d %d\n", alloc_count, alloc_tid, result, (U_32)expected, (U_32)value, (U_32)old_val);
        if(result == JNI_TRUE){
            assert(expected == old_val);
        }
        else{
            assert(expected != old_val);
        }
#else
        fprintf(order_system_call[tid], "%d\n", result);
#endif

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
            assert(order_system_call[tid] != NULL);
#endif
        }
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 26);

/*
        U_32 alloc_count_temp = 0;
        fscanf(order_system_call[tid], "%d ", &alloc_count_temp);
        assert(alloc_count_temp == alloc_count);
*/
#endif
        jboolean result_log = JNI_FALSE;
#ifdef ORDER_DEBUG
        U_32 alloc_count_log=0;
        U_32 alloc_tid_log = 0;
        U_32 expected_log = 0;
        U_32 value_log = 0;
        U_32 old_val_log = 0;
        fscanf(order_system_call[tid], "%d %d %d %d %d %d\n", &alloc_count_log, &alloc_tid_log, &result_log, &expected_log, &value_log, &old_val_log);
        assert(alloc_count_log == alloc_count);
        assert(alloc_tid_log == alloc_tid);
        assert(expected_log == expected);
        assert(value_log == value);
        assert((U_32)old_val == old_val_log);
#else
        fscanf(order_system_call[tid], "%d\n", &result_log);
#endif
        assert(result_log == result);
     }

#ifdef RECORD_AS_ACCESS
     p_obj->access_lock = 0;
#endif
     return result;

#else  //#ifdef ORDER
    return compareAndSetBooleanField(env, self, obj, offset, expected, value);
#endif




#if 0
#ifdef ORDER
/*
    hythread_suspend_disable();
    ObjectHandle v = (ObjectHandle)value;
    ManagedObject* val = (v==NULL)?NULL:v->object;
    U_32 alloc_count = 0;
    if (val) {alloc_count = val->alloc_count;}
    hythread_suspend_enable();
*/
    if(vm_order_record){

        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
            assert(order_system_call[tid] != NULL);
        }
		
#ifdef ORDER_DEBUG

        fprintf(order_system_call[tid], "[%d] ", 26);

//        fprintf(order_system_call[tid], "%d ", alloc_count);
#endif
        jboolean result = compareAndSetBooleanField(env, self, obj, offset, expected, value);

        fprintf(order_system_call[tid], "%d\n", result);
        return result;
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
            assert(order_system_call[tid] != NULL);
#endif
        }
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 26);

/*
        U_32 alloc_count_temp = 0;
        fscanf(order_system_call[tid], "%d ", &alloc_count_temp);
        assert(alloc_count_temp == alloc_count);
*/
#endif

        jboolean result = JNI_FALSE;
        fscanf(order_system_call[tid], "%d\n", &result);
        if (result == JNI_TRUE)
        {
           while(compareAndSetBooleanField(env, self, obj, offset, expected, value) != JNI_TRUE)
           {
               usleep(1000);
           }
           return JNI_TRUE;
        }
        else
        {
#ifdef ORDER_DEBUG
            assert(JNI_FALSE == result);
#endif
            return JNI_FALSE;
        }
     }
#else
    return compareAndSetBooleanField(env, self, obj, offset, expected, value);
#endif
#endif // #if 0 
}
 
                   
JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetInt__Ljava_lang_Object_2JII 
(JNIEnv * env, jobject self, jobject obj, jlong offset, jint expected, jint value)
{     
#ifdef ORDER
#ifdef RECORD_AS_ACCESS
/****************************  RECORD AS ACCESS ***********************************/
    hythread_suspend_disable();
    ObjectHandle obj_handle = (ObjectHandle)obj;
    ManagedObject* p_obj = (obj_handle==NULL)?NULL:obj_handle->object;
    hythread_suspend_enable();
    assert(p_obj != NULL);
    if(vm_order_record){
        if (!vm_is_initializing && p_obj->alloc_count != -1)
        {

            while (port_atomic_cas8(&p_obj->access_lock, 1, 0) != 0)
            {
#ifdef ORDER_DEBUG
                printf("usleep in compareAndSetInt, %d, %d, thread : %d\n", p_obj->alloc_count, p_obj->access_lock, hythread_self()->thread_id);
                if (hythread_self()->request != 0) printf("request to lock thread : %d\n", hythread_self()->thread_id);
#endif
                usleep(1000);
            }

            if (p_obj->access_tid != (U_32)pthread_self())
            {
                gc_record_info(p_obj);
                p_obj->access_tid = (U_32)pthread_self();
                p_obj->access_count = 0;
            }

            p_obj->access_count ++;
#ifdef REPLAY_OPTIMIZE
            p_obj->order_padding |= OBJ_RW_BIT; //write
#endif


        }
    }
    else{
check_lock:
        if (!vm_is_initializing && p_obj->alloc_count != -1)
        {
            while (port_atomic_cas8(&p_obj->access_lock, 1, 0) != 0)
            {
#ifdef ORDER_DEBUG
                printf("usleep in monitor enter, (%d, %d), %d\n", p_obj->alloc_tid, p_obj->alloc_count, hythread_self()->thread_id);
                if (hythread_self()->request != 0) printf("request to lock thread : %d\n", hythread_self()->thread_id);
#endif
                usleep(1000);
                hythread_yield();
                hythread_safe_point();
                hythread_exception_safe_point();
                p_obj = obj_handle->object;
#ifdef ORDER_DEBUG
                assert(p_obj != NULL);
#endif
            }

            if (p_obj->access_count == 0){
                U_8 result = gc_get_interleaving_info(p_obj);
	       if (result == 0){
                    p_obj->access_lock = 0;
                    usleep(1000);
                    hythread_yield();
                    hythread_safe_point();
                    hythread_exception_safe_point();
                    p_obj = obj_handle->object;
#ifdef ORDER_DEBUG
                    assert(p_obj != NULL);
#endif
                    goto check_lock;
	       }
            }
            else{
                while(p_obj->access_tid != (U_32)pthread_self()){
#ifdef ORDER_DEBUG
                    printf("usleep in monitor enter, tid not match, (%d, %d), current : %d, expected : %d, remaining : %d\n", p_obj->alloc_tid, p_obj->alloc_count, hythread_self()->thread_id, p_obj->access_tid, p_obj->access_count);
                    assert(p_obj->alloc_tid != 0);
                    if (hythread_self()->request != 0) printf("request to lock thread : %d\n", hythread_self()->thread_id);
#endif
                    p_obj->access_lock = 0;
                    usleep(1000);
                    hythread_yield();
                    hythread_safe_point();
                    hythread_exception_safe_point();
                    p_obj = obj_handle->object;
#ifdef ORDER_DEBUG
                    assert(p_obj != NULL);
#endif
                    goto check_lock;
                }
            }    

            p_obj->access_count --;
#ifdef REPLAY_OPTIMIZE
            p_obj->order_padding |= OBJ_RW_BIT; //write
#endif
        }
    }
#endif //#ifdef RECORD_AS_ACCESS

/***************************** RECORD AS SYSTEM CALL *************************/
#ifdef ORDER_DEBUG
    hythread_suspend_disable();
    ObjectHandle v = (ObjectHandle)obj;
    ManagedObject* val = (v==NULL)?NULL:v->object;
    U_32 alloc_count = 0;
    U_32 alloc_tid = 0;
    if (val) {
        alloc_count = val->alloc_count;
        alloc_tid = val->alloc_tid;
        if(alloc_count == -1){
            alloc_count = val->alloc_count_for_hashcode;
        }
    }
    hythread_suspend_enable();
#endif
    jint old_val = 0;
    jboolean result = compareAndSetIntField(env, self, obj, offset, expected, value, &old_val);

    if(vm_order_record){

        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
            assert(order_system_call[tid] != NULL);
        }
		
#ifdef ORDER_DEBUG

        fprintf(order_system_call[tid], "[%d] ", 27);

//        fprintf(order_system_call[tid], "%d ", alloc_count);
#endif


#ifdef ORDER_DEBUG
        fprintf(order_system_call[tid], "%d %d %d %d %d %d\n", alloc_count, alloc_tid, result, (U_32)expected, (U_32)value, (U_32)old_val);
        if(result == JNI_TRUE){
            assert(expected == old_val);
        }
        else{
            assert(expected != old_val);
        }
#else
        fprintf(order_system_call[tid], "%d\n", result);
#endif

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
            assert(order_system_call[tid] != NULL);
#endif
        }
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 27);

/*
        U_32 alloc_count_temp = 0;
        fscanf(order_system_call[tid], "%d ", &alloc_count_temp);
        assert(alloc_count_temp == alloc_count);
*/
#endif
        jboolean result_log = JNI_FALSE;
#ifdef ORDER_DEBUG
        U_32 alloc_count_log=0;
        U_32 alloc_tid_log = 0;
        U_32 expected_log = 0;
        U_32 value_log = 0;
        U_32 old_val_log = 0;
        fscanf(order_system_call[tid], "%d %d %d %d %d %d\n", &alloc_count_log, &alloc_tid_log, &result_log, &expected_log, &value_log, &old_val_log);
        assert(alloc_count_log == alloc_count);
        assert(alloc_tid_log == alloc_tid);
        assert(expected_log == expected);
        assert(value_log == value);
        assert((U_32)old_val == old_val_log);
#else
        fscanf(order_system_call[tid], "%d\n", &result_log);
#endif
        assert(result_log == result);
/*
        if (result == JNI_TRUE)
        {
           while(compareAndSetIntField(env, self, obj, offset, expected, value) != JNI_TRUE)
           {
               usleep(1000);
           }
           return JNI_TRUE;
        }
        else
        {
#ifdef ORDER_DEBUG
            assert(JNI_FALSE == result);
#endif
            OrderCompareAndSetIntField(env, self, obj, offset, expected, value, (jint)old_val_log);
            return JNI_FALSE;
        }
*/
     }

#ifdef RECORD_AS_ACCESS
     p_obj->access_lock = 0;
#endif
     return result;

#else  //#ifdef ORDER
    return compareAndSetIntField(env, self, obj, offset, expected, value);
#endif
}
 
 
JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetLong__Ljava_lang_Object_2JJJ 
(JNIEnv * env, jobject self, jobject obj, jlong offset, jlong expected, jlong value)
{
#ifdef ORDER
#ifdef RECORD_AS_ACCESS
/****************************  RECORD AS ACCESS ***********************************/
    hythread_suspend_disable();
    ObjectHandle obj_handle = (ObjectHandle)obj;
    ManagedObject* p_obj = (obj_handle==NULL)?NULL:obj_handle->object;
    hythread_suspend_enable();
    assert(p_obj != NULL);
    if(vm_order_record){
        if (!vm_is_initializing && p_obj->alloc_count != -1)
        {

            while (port_atomic_cas8(&p_obj->access_lock, 1, 0) != 0)
            {
#ifdef ORDER_DEBUG
                printf("usleep in compareAndSetInt, %d, %d, thread : %d\n", p_obj->alloc_count, p_obj->access_lock, hythread_self()->thread_id);
                if (hythread_self()->request != 0) printf("request to lock thread : %d\n", hythread_self()->thread_id);
#endif
                usleep(1000);
            }

            if (p_obj->access_tid != (U_32)pthread_self())
            {
                gc_record_info(p_obj);
                p_obj->access_tid = (U_32)pthread_self();
                p_obj->access_count = 0;
            }

            p_obj->access_count ++;
#ifdef REPLAY_OPTIMIZE
            p_obj->order_padding |= OBJ_RW_BIT; //write
#endif


        }
    }
    else{
check_lock:
        if (!vm_is_initializing && p_obj->alloc_count != -1)
        {
            while (port_atomic_cas8(&p_obj->access_lock, 1, 0) != 0)
            {
#ifdef ORDER_DEBUG
                printf("usleep in monitor enter, (%d, %d), %d\n", p_obj->alloc_tid, p_obj->alloc_count, hythread_self()->thread_id);
                if (hythread_self()->request != 0) printf("request to lock thread : %d\n", hythread_self()->thread_id);
#endif
                usleep(1000);
                hythread_yield();
                hythread_safe_point();
                hythread_exception_safe_point();
                p_obj = obj_handle->object;
#ifdef ORDER_DEBUG
                assert(p_obj != NULL);
#endif
            }

            if (p_obj->access_count == 0){
                U_8 result = gc_get_interleaving_info(p_obj);
	       if (result == 0){
                    p_obj->access_lock = 0;
                    usleep(1000);
                    hythread_yield();
                    hythread_safe_point();
                    hythread_exception_safe_point();
                    p_obj = obj_handle->object;
#ifdef ORDER_DEBUG
                    assert(p_obj != NULL);
#endif
                    goto check_lock;
	       }
            }
            else{
                while(p_obj->access_tid != (U_32)pthread_self()){
#ifdef ORDER_DEBUG
                    printf("usleep in monitor enter, tid not match, (%d, %d), current : %d, expected : %d, remaining : %d\n", p_obj->alloc_tid, p_obj->alloc_count, hythread_self()->thread_id, p_obj->access_tid, p_obj->access_count);
                    assert(p_obj->alloc_tid != 0);
                    if (hythread_self()->request != 0) printf("request to lock thread : %d\n", hythread_self()->thread_id);
#endif
                    p_obj->access_lock = 0;
                    usleep(1000);
                    hythread_yield();
                    hythread_safe_point();
                    hythread_exception_safe_point();
                    p_obj = obj_handle->object;
#ifdef ORDER_DEBUG
                    assert(p_obj != NULL);
#endif
                    goto check_lock;
                }
            }    

            p_obj->access_count --;
#ifdef REPLAY_OPTIMIZE
            p_obj->order_padding |= OBJ_RW_BIT; //write
#endif
        }
    }
#endif //#ifdef RECORD_AS_ACCESS

/***************************** RECORD AS SYSTEM CALL *************************/
#ifdef ORDER_DEBUG
    hythread_suspend_disable();
    ObjectHandle v = (ObjectHandle)obj;
    ManagedObject* val = (v==NULL)?NULL:v->object;
    U_32 alloc_count = 0;
    U_32 alloc_tid = 0;
    if (val) {
        alloc_count = val->alloc_count;
        alloc_tid = val->alloc_tid;
        if(alloc_count == -1){
            alloc_count = val->alloc_count_for_hashcode;
        }
    }
    hythread_suspend_enable();
#endif
    jlong old_val = 0;
    jboolean result = compareAndSetLongField(env, self, obj, offset, expected, value, &old_val);

    if(vm_order_record){

        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
            assert(order_system_call[tid] != NULL);
        }
		
#ifdef ORDER_DEBUG

        fprintf(order_system_call[tid], "[%d] ", 28);

//        fprintf(order_system_call[tid], "%d ", alloc_count);
#endif


#ifdef ORDER_DEBUG
        fprintf(order_system_call[tid], "%ld %ld %ld %ld %ld %ld\n", alloc_count, alloc_tid, result, (long)expected, (long)value, (long)old_val);
        if(result == JNI_TRUE){
            assert(expected == old_val);
        }
        else{
            assert(expected != old_val);
        }
#else
        fprintf(order_system_call[tid], "%d\n", result);
#endif

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
            assert(order_system_call[tid] != NULL);
#endif
        }
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 28);

/*
        U_32 alloc_count_temp = 0;
        fscanf(order_system_call[tid], "%d ", &alloc_count_temp);
        assert(alloc_count_temp == alloc_count);
*/
#endif
        jboolean result_log = JNI_FALSE;
#ifdef ORDER_DEBUG
        long alloc_count_log=0;
        long alloc_tid_log = 0;
        long expected_log = 0;
        long value_log = 0;
        long old_val_log = 0;
        fscanf(order_system_call[tid], "%ld %ld %ld %ld %ld %ld\n", &alloc_count_log, &alloc_tid_log, &result_log, &expected_log, &value_log, &old_val_log);
        assert(alloc_count_log == alloc_count);
        assert(alloc_tid_log == alloc_tid);
        assert(expected_log == expected);
        assert(value_log == value);
        assert((long)old_val == old_val_log);
#else
        fscanf(order_system_call[tid], "%d\n", &result_log);
#endif
        assert(result_log == result);
/*
        if (result == JNI_TRUE)
        {
           while(compareAndSetIntField(env, self, obj, offset, expected, value) != JNI_TRUE)
           {
               usleep(1000);
           }
           return JNI_TRUE;
        }
        else
        {
#ifdef ORDER_DEBUG
            assert(JNI_FALSE == result);
#endif
            OrderCompareAndSetIntField(env, self, obj, offset, expected, value, (jint)old_val_log);
            return JNI_FALSE;
        }
*/
     }

#ifdef RECORD_AS_ACCESS
     p_obj->access_lock = 0;
#endif
     return result;

#else  //#ifdef ORDER
    return compareAndSetLongField(env, self, obj, offset, expected, value);
#endif



#if 0
#ifdef ORDER
/*
    hythread_suspend_disable();
    ObjectHandle v = (ObjectHandle)value;
    ManagedObject* val = (v==NULL)?NULL:v->object;
    U_32 alloc_count = 0;
    if (val) {alloc_count = val->alloc_count;}
    hythread_suspend_enable();
*/
    if(vm_order_record){

        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
            assert(order_system_call[tid] != NULL);
        }
		
#ifdef ORDER_DEBUG

        fprintf(order_system_call[tid], "[%d] ", 28);

//        fprintf(order_system_call[tid], "%d ", alloc_count);
#endif
        jboolean result = compareAndSetLongField(env, self, obj, offset, expected, value);

        fprintf(order_system_call[tid], "%d\n", result);
        return result;
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
            assert(order_system_call[tid] != NULL);
#endif
        }
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 28);

/*
        U_32 alloc_count_temp = 0;
        fscanf(order_system_call[tid], "%d ", &alloc_count_temp);
        assert(alloc_count_temp == alloc_count);
*/
#endif

        jboolean result = JNI_FALSE;
        fscanf(order_system_call[tid], "%d\n", &result);
        if (result == JNI_TRUE)
        {
           while(compareAndSetLongField(env, self, obj, offset, expected, value) != JNI_TRUE)
           {
               usleep(1000);
           }
           return JNI_TRUE;
        }
        else
        {
#ifdef ORDER_DEBUG
            assert(JNI_FALSE == result);
#endif
            return JNI_FALSE;
        }
     }
#else
    return compareAndSetLongField(env, self, obj, offset, expected, value);
#endif

#endif //#if 0
}


JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetInt___3IIII
(JNIEnv * env, jobject self, jintArray array, jint index, jint expected, jint value)
{
#ifdef ORDER_DEBUG
    assert(0);
#endif
#ifdef ORDER
/*
    hythread_suspend_disable();
    ObjectHandle v = (ObjectHandle)value;
    ManagedObject* val = (v==NULL)?NULL:v->object;
    U_32 alloc_count = 0;
    if (val) {alloc_count = val->alloc_count;}
    hythread_suspend_enable();
*/
    if(vm_order_record){

        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
            assert(order_system_call[tid] != NULL);
        }
		
#ifdef ORDER_DEBUG

        fprintf(order_system_call[tid], "[%d] ", 29);

//        fprintf(order_system_call[tid], "%d ", alloc_count);
#endif
        jboolean result = compareAndSetIntArray(env, self, array, index, expected, value);

        fprintf(order_system_call[tid], "%d\n", result);
        return result;
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
            assert(order_system_call[tid] != NULL);
#endif
        }
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 29);

/*
        U_32 alloc_count_temp = 0;
        fscanf(order_system_call[tid], "%d ", &alloc_count_temp);
        assert(alloc_count_temp == alloc_count);
*/
#endif

        jboolean result = JNI_FALSE;
        fscanf(order_system_call[tid], "%d\n", &result);
        if (result == JNI_TRUE)
        {
           while(compareAndSetIntArray(env, self, array, index, expected, value) != JNI_TRUE)
           {
               usleep(1000);
           }
           return JNI_TRUE;
        }
        else
        {
#ifdef ORDER_DEBUG
            assert(JNI_FALSE == result);
#endif
            return JNI_FALSE;
        }
     }
#else
    return compareAndSetIntArray(env, self, array, index, expected, value);
#endif
}


JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetBoolean___3ZIZZ
(JNIEnv * env, jobject self, jbooleanArray array, jint index, jboolean expected, jboolean value)
{
#ifdef ORDER_DEBUG
    assert(0);
#endif
#ifdef ORDER
/*
    hythread_suspend_disable();
    ObjectHandle v = (ObjectHandle)value;
    ManagedObject* val = (v==NULL)?NULL:v->object;
    U_32 alloc_count = 0;
    if (val) {alloc_count = val->alloc_count;}
    hythread_suspend_enable();
*/
    if(vm_order_record){

        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
            assert(order_system_call[tid] != NULL);
        }
		
#ifdef ORDER_DEBUG

        fprintf(order_system_call[tid], "[%d] ", 30);

//        fprintf(order_system_call[tid], "%d ", alloc_count);
#endif
        jboolean result = compareAndSetBooleanArray(env, self, array, index, expected, value);

        fprintf(order_system_call[tid], "%d\n", result);
        return result;
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
            assert(order_system_call[tid] != NULL);
#endif
        }
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 30);

/*
        U_32 alloc_count_temp = 0;
        fscanf(order_system_call[tid], "%d ", &alloc_count_temp);
        assert(alloc_count_temp == alloc_count);
*/
#endif

        jboolean result = JNI_FALSE;
        fscanf(order_system_call[tid], "%d\n", &result);
        if (result == JNI_TRUE)
        {
           while(compareAndSetBooleanArray(env, self, array, index, expected, value) != JNI_TRUE)
           {
               usleep(1000);
           }
           return JNI_TRUE;
        }
        else
        {
#ifdef ORDER_DEBUG
            assert(JNI_FALSE == result);
#endif
            return JNI_FALSE;
        }
     }
#else
    return compareAndSetBooleanArray(env, self, array, index, expected, value);
#endif
}


JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetLong___3JIJJ
(JNIEnv * env, jobject self, jlongArray array, jint index, jlong expected, jlong value)
{
#ifdef ORDER_DEBUG
    assert(0);
#endif
#ifdef ORDER
/*
    hythread_suspend_disable();
    ObjectHandle v = (ObjectHandle)value;
    ManagedObject* val = (v==NULL)?NULL:v->object;
    U_32 alloc_count = 0;
    if (val) {alloc_count = val->alloc_count;}
    hythread_suspend_enable();
*/
    if(vm_order_record){

        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
            assert(order_system_call[tid] != NULL);
        }
		
#ifdef ORDER_DEBUG

        fprintf(order_system_call[tid], "[%d] ", 31);

//        fprintf(order_system_call[tid], "%d ", alloc_count);
#endif
        jboolean result = compareAndSetLongArray(env, self, array, index, expected, value);

        fprintf(order_system_call[tid], "%d\n", result);
        return result;
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
            assert(order_system_call[tid] != NULL);
#endif
        }
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 31);

/*
        U_32 alloc_count_temp = 0;
        fscanf(order_system_call[tid], "%d ", &alloc_count_temp);
        assert(alloc_count_temp == alloc_count);
*/
#endif

        jboolean result = JNI_FALSE;
        fscanf(order_system_call[tid], "%d\n", &result);
        if (result == JNI_TRUE)
        {
           while(compareAndSetLongArray(env, self, array, index, expected, value) != JNI_TRUE)
           {
               usleep(1000);
           }
           return JNI_TRUE;
        }
        else
        {
#ifdef ORDER_DEBUG
            assert(JNI_FALSE == result);
#endif
            return JNI_FALSE;
        }
     }
#else

    return compareAndSetLongArray(env, self, array, index, expected, value);
#endif
}


JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetObject___3Ljava_lang_Object_2ILjava_lang_Object_2Ljava_lang_Object_2
(JNIEnv * env, jobject self, jobjectArray array, jint index, jobject expected, jobject value)
{
#ifdef ORDER_DEBUG
    assert(0);
#endif
#ifdef ORDER
/*
    hythread_suspend_disable();
    ObjectHandle v = (ObjectHandle)value;
    ManagedObject* val = (v==NULL)?NULL:v->object;
    U_32 alloc_count = 0;
    if (val) {alloc_count = val->alloc_count;}
    hythread_suspend_enable();
*/
    if(vm_order_record){

        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
            assert(order_system_call[tid] != NULL);
        }
		
#ifdef ORDER_DEBUG

        fprintf(order_system_call[tid], "[%d] ", 32);

//        fprintf(order_system_call[tid], "%d ", alloc_count);
#endif
        jboolean result = compareAndSetObjectArray(env, self, array, index, expected, value);

        fprintf(order_system_call[tid], "%d\n", result);
        return result;
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
            assert(order_system_call[tid] != NULL);
#endif
        }
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 32);

/*
        U_32 alloc_count_temp = 0;
        fscanf(order_system_call[tid], "%d ", &alloc_count_temp);
        assert(alloc_count_temp == alloc_count);
*/
#endif

        jboolean result = JNI_FALSE;
        fscanf(order_system_call[tid], "%d\n", &result);
        if (result == JNI_TRUE)
        {
           while(compareAndSetObjectArray(env, self, array, index, expected, value) != JNI_TRUE)
           {
               usleep(1000);
           }
           return JNI_TRUE;
        }
        else
        {
#ifdef ORDER_DEBUG
            assert(JNI_FALSE == result);
#endif
            return JNI_FALSE;
        }
     }
#else
    return compareAndSetObjectArray(env, self, array, index, expected, value);
#endif
}

JNIEXPORT jboolean JNICALL 
Java_java_util_concurrent_atomic_AtomicLong_VMSupportsCS8
(JNIEnv *, jclass) 
{
    return vmSupportsCAS8();
}
