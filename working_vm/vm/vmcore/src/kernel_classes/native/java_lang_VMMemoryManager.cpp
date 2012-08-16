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
 * @author Euguene Ostrovsky
 */

/**
 * @file java_lang_VMMemoryManager.cpp
 *
 * This file is a part of kernel class natives VM core component.
 * It contains implementation for native methods of 
 * java.lang.VMMemoryManager class.
 */


#include "open/gc.h"
#include "jni_utils.h"
#include "object.h"
#include "finalize.h"
#include "cxxlog.h"
#include "vm_threads.h"

#include "java_lang_VMMemoryManager.h"

/*
 * Class:     java_lang_VMMemoryManager
 * Method:    arrayCopy
 * Signature: (Ljava/lang/Object;ILjava/lang/Object;II)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMMemoryManager_arrayCopy
  (JNIEnv *jenv, jclass, jobject src, jint srcPos, jobject dest, jint destPos, jint len)
{
    array_copy_jni(jenv, src, srcPos, dest, destPos, len);
}

/*
 * Class:     java_lang_VMMemoryManager
 * Method:    clone
 * Signature: (Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_java_lang_VMMemoryManager_clone
  (JNIEnv *jenv, jclass, jobject obj)
{
    return object_clone(jenv, obj);
}

#ifdef ORDER
extern bool vm_order_record;
extern FILE *order_system_call[];
#endif

/*
 * Class:     java_lang_VMMemoryManager
 * Method:    getFreeMemory
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_java_lang_VMMemoryManager_getFreeMemory
  (JNIEnv *, jclass)
{
    jlong free_memory = 0;

    free_memory = gc_free_memory();

#ifdef ORDER
    if(vm_order_record){
        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL){
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
#ifdef ORDER_DEBUG
            if(order_system_call[tid]== NULL){
                printf("[ORDER]: could NOT open  file %s\n", name);
            }
            fseek(order_system_call[tid], 0, SEEK_SET);
#endif
        }
#ifdef ORDER_DEBUG
        fprintf(order_system_call[tid], "[%d] ", 8);
#endif
        //fwrite(&free_memory, sizeof(jlong), 1, order_system_call[tid]);
        fprintf(order_system_call[tid], "%ld\n", free_memory);
//        fflush(order_system_call[tid]);
//        printf("Java_java_lang_VMMemoryManager_getFreeMemory : %d\n", free_memory);
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
            fseek(order_system_call[tid], 0, SEEK_SET);
#endif
        }
       
#ifdef ORDER_DEBUG
        //fread(&free_memory, sizeof(jlong), 1, order_system_call[tid]);
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 8);
#endif
        fscanf(order_system_call[tid], "%ld\n", &free_memory);
//        printf("Java_java_lang_VMMemoryManager_getFreeMemory : %d\n", free_memory);
    }
#endif

    return free_memory;
}

/*
 * Class:     java_lang_VMMemoryManager
 * Method:    getIdentityHashCode
 * Signature: (Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_java_lang_VMMemoryManager_getIdentityHashCode
  (JNIEnv *jenv, jclass, jobject obj)
{
    return object_get_generic_hashcode(jenv, obj);
}

/*
 * Class:     java_lang_VMMemoryManager
 * Method:    getMaxMemory
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_java_lang_VMMemoryManager_getMaxMemory
  (JNIEnv *, jclass)
{

    jlong max_memory = 0;
    max_memory = gc_max_memory();

#ifdef ORDER
    if(vm_order_record){
        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL){
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
#ifdef ORDER_DEBUG
            if(order_system_call[tid]== NULL){
                printf("[ORDER]: could NOT open  file %s\n", name);
            }
            fseek(order_system_call[tid], 0, SEEK_SET);
#endif
        }
#ifdef ORDER_DEBUG
        fprintf(order_system_call[tid], "[%d] ", 40);
#endif
        //fwrite(&free_memory, sizeof(jlong), 1, order_system_call[tid]);
        fprintf(order_system_call[tid], "%ld\n", max_memory);
//        fflush(order_system_call[tid]);
//        printf("Java_java_lang_VMMemoryManager_getFreeMemory : %d\n", free_memory);
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
            fseek(order_system_call[tid], 0, SEEK_SET);
#endif
        }
       
#ifdef ORDER_DEBUG
        //fread(&free_memory, sizeof(jlong), 1, order_system_call[tid]);
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 40);
#endif
        fscanf(order_system_call[tid], "%ld\n", &max_memory);
//        printf("Java_java_lang_VMMemoryManager_getFreeMemory : %d\n", free_memory);
    }
#endif

    return max_memory;

}

/*
 * Class:     java_lang_VMMemoryManager
 * Method:    getTotalMemory
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_java_lang_VMMemoryManager_getTotalMemory
  (JNIEnv *, jclass)
{

    jlong total_memory = 0;
    total_memory = gc_total_memory();

#ifdef ORDER
    if(vm_order_record){
        U_32 tid = hythread_self()->thread_id;
        if (order_system_call[tid] == NULL){
            char name[40];
            sprintf(name, "SYSTEM_CALL.%d.log", tid);

            order_system_call[tid] = fopen64(name, "a+");
#ifdef ORDER_DEBUG
            if(order_system_call[tid]== NULL){
                printf("[ORDER]: could NOT open  file %s\n", name);
            }
            fseek(order_system_call[tid], 0, SEEK_SET);
#endif
        }
#ifdef ORDER_DEBUG
        fprintf(order_system_call[tid], "[%d] ", 39);
#endif
        //fwrite(&free_memory, sizeof(jlong), 1, order_system_call[tid]);
        fprintf(order_system_call[tid], "%ld\n", total_memory);
//        fflush(order_system_call[tid]);
//        printf("Java_java_lang_VMMemoryManager_getTotalMemory : %d\n", free_memory);
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
            fseek(order_system_call[tid], 0, SEEK_SET);
#endif
        }
       
#ifdef ORDER_DEBUG
        //fread(&free_memory, sizeof(jlong), 1, order_system_call[tid]);
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 39);
#endif
        fscanf(order_system_call[tid], "%ld\n", &total_memory);
//        printf("Java_java_lang_VMMemoryManager_getTotalMemory : %d\n", free_memory);
    }
#endif

    return total_memory;

}

/*
 * Class:     java_lang_VMMemoryManager
 * Method:    runFinalzation
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_VMMemoryManager_runFinalization
  (JNIEnv *, jclass)
{
    TRACE2("ref", "Enqueueing references");
    vm_enqueue_references();
    
    // For now we run the finalizers immediately in the context of the thread which requested GC.
    // Eventually we may have a different scheme, e.g., a dedicated finalize thread.
    TRACE2("finalize", "Running pending finalizers");
    vm_run_pending_finalizers();
}

/*
 * Class:     java_lang_VMMemoryManager
 * Method:    runGC
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_java_lang_VMMemoryManager_runGC
  (JNIEnv *, jclass)
{
    assert(hythread_is_suspend_enabled());
    gc_force_gc();      
    return;
}
