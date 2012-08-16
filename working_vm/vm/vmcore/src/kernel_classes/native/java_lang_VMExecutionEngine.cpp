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
 * @file java_lang_VMExecutionEngine.cpp
 *
 * This file is a part of kernel class natives VM core component.
 * It contains implementation for native methods of 
 * java.lang.VMExecutionEngine class.
 */

#define LOG_DOMAIN "vm.kernel"
#include "cxxlog.h"

#include <apr_env.h>
#include <apr_file_io.h>
#include <apr_time.h>

#include "port_dso.h"
#include "port_sysinfo.h"
#include "port_timer.h"
#include "environment.h"
#include "jni_utils.h"
#include "properties.h"
#include "exceptions.h"
#include "java_lang_VMExecutionEngine.h"
#include "assertion_registry.h"
#include "init.h"

#ifdef ORDER
#ifdef RECORD_STATIC_IN_FIELD
#include "classloader.h"
#endif
extern FILE *order_system_call[];
extern FILE *systemCallCompile[];
extern FILE *order_system_call_prepare[];
extern FILE *order_system_call_verify[];
extern FILE *order_system_call_in_compile[];
extern FILE *systemCallMemmove[];
extern FILE *systemCallWaitFile[];
extern FILE *systemCallTimeFile[];
extern FILE *order_system_call_park[];
#endif

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    exit
 * Signature: (IZ[Ljava/lang/Runnable;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMExecutionEngine_exit
  (JNIEnv * jni_env, jclass, jint status, jboolean needFinalization, jobjectArray)
{
    exec_native_shutdown_sequence();

#ifdef ORDER_PER_EVA
    dump_method_count();
#endif

    // ToDo: process jobjectArray
    hythread_global_lock();

#ifdef ORDER
#ifdef RECORD_STATIC_IN_FIELD
    finalize_all_field_access_info();
#endif
    hythread_iterator_t iterator;
    hythread_suspend_all(&iterator, NULL);

	
    gc_finalize_order();
#ifdef ORDER_DEBUG
    printf("[ORDER]: Flush and Close all system call log!!!\n");
#endif
    for (int i = 0 ; i < ORDER_THREAD_NUM; i ++)
    {
        if (order_system_call[i] != NULL)
        {
            fflush(order_system_call[i]);
            fclose(order_system_call[i]);
            order_system_call[i] = NULL;
        }

        if (systemCallCompile[i] != NULL)
        {
            fflush(systemCallCompile[i]);
            fclose(systemCallCompile[i]);
            systemCallCompile[i] = NULL;
        }
		
        if(order_system_call_prepare[i] != NULL){
            fflush(order_system_call_prepare[i]);
            fclose(order_system_call_prepare[i]);
            order_system_call_prepare[i] = NULL;
        }

        if(order_system_call_verify[i] != NULL){
            fflush(order_system_call_verify[i]);
            fclose(order_system_call_verify[i]);
            order_system_call_verify[i] = NULL;
        }

        if(order_system_call_in_compile[i] != NULL){
            fflush(order_system_call_in_compile[i]);
            fclose(order_system_call_in_compile[i]);
            order_system_call_in_compile[i] = NULL;
        }

        if(systemCallMemmove[i] != NULL){
            fflush(systemCallMemmove[i]);
            fclose(systemCallMemmove[i]);
            systemCallMemmove[i] = NULL;
        }

        if(systemCallWaitFile[i] != NULL){
            fflush(systemCallWaitFile[i]);
            fclose(systemCallWaitFile[i]);
            systemCallWaitFile[i] = NULL;
        }

        if(systemCallTimeFile[i] != NULL){
            fflush(systemCallTimeFile[i]);
            fclose(systemCallTimeFile[i]);
            systemCallTimeFile[i] = NULL;
        }
		
        if(order_system_call_park[i] != NULL){
            fflush(order_system_call_park[i]);
            fclose(order_system_call_park[i]);
            order_system_call_park[i] = NULL;
        }
    }
#endif

    _exit(status);
    hythread_global_unlock();
}

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    getAssertionStatus
 * Signature: (Ljava/lang/Class;ZI)I
 */
JNIEXPORT jint JNICALL Java_java_lang_VMExecutionEngine_getAssertionStatus
  (JNIEnv * jenv, jclass, jclass jclss, jboolean recursive, jint defaultStatus)
{
#ifdef ORDER_DEBUG
    printf("[ENTER] : Java_java_lang_VMExecutionEngine_getAssertionStatus\n");
#endif

    Assertion_Status status = ASRT_UNSPECIFIED;
    Global_Env* genv = jni_get_vm_env(jenv);
    Assertion_Registry* reg = genv->assert_reg;
    if (!reg) {
        return status;
    }

    if(jclss) {
        Class* clss = jclass_to_struct_Class(jclss);
        while (clss->get_declaring_class_index()) {
            clss = class_get_declaring_class((Class_Handle)clss);
        }
        const char* name = clss->get_java_name()->bytes;
        bool system = (((void*)clss->get_class_loader()) == ((void*)genv->bootstrap_class_loader));
        TRACE("check assert status for " << name << " system=" << system);
        if (system || !recursive) {
            status = reg->get_class_status(name);
        } 
        TRACE("name checked: " << status);
        if (recursive || system) {
            if (status == ASRT_UNSPECIFIED) {
                status = reg->get_package_status(name);
            }
            TRACE("pkg checked: " << status);
            if (status == ASRT_UNSPECIFIED) {
                if (defaultStatus != ASRT_UNSPECIFIED) {
                    status = (Assertion_Status)defaultStatus;
                } else {
                    status = reg->is_enabled(system);
                }
            }
            TRACE("default checked: " << status);
        }
    } else {
        if (reg->classes || reg->packages || reg->enable_system) {
            status = ASRT_ENABLED;
        } else {
            status = reg->enable_all;
        }
    }
    TRACE("Resulting assertion status: " << status);
    return status;
}

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    getAvailableProcessors
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_java_lang_VMExecutionEngine_getAvailableProcessors
  (JNIEnv *, jclass)
{
#ifdef ORDER_DEBUG
    printf("[ENTER] : Java_java_lang_VMExecutionEngine_getAvailableProcessors\n");
#endif
    return port_CPUs_number();
}

/**
 * Adds property specified by key and val parameters to given Properties object.
 */
static bool PropPut(JNIEnv* jenv, jobject properties, const char* key, const char* val)
{
#ifdef ORDER_DEBUG
    printf("[ENTER] : PropPut in java_lang_VMExecutionEngine.cpp\n");
#endif

    jobject key_string = NewStringUTF(jenv, key);
    if (!key_string) return false;
    jobject val_string = val ? NewStringUTF(jenv, val) : NULL;
    if (val && !val_string) return false;

    static jmethodID put_method = NULL;
    if (!put_method) {
        Class* clss = VM_Global_State::loader_env->java_util_Properties_Class;
        String* name = VM_Global_State::loader_env->string_pool.lookup("put");
        String* sig = VM_Global_State::loader_env->string_pool.lookup(
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
        put_method = (jmethodID) class_lookup_method_recursive(clss, name, sig);
    }

    CallObjectMethod(jenv, properties, put_method, key_string, val_string);
    return !exn_raised();
}

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    getProperties
 * Signature: ()Ljava/util/Properties;
 */
JNIEXPORT jobject JNICALL Java_java_lang_VMExecutionEngine_getProperties
  (JNIEnv *jenv, jclass)
{
#ifdef ORDER_DEBUG
    printf("[ENTER] : Java_java_lang_VMExecutionEngine_getProperties\n");
#endif
    jobject jprops = create_default_instance(
        VM_Global_State::loader_env->java_util_Properties_Class);

    if (jprops) {
        Properties *pp = VM_Global_State::loader_env->JavaProperties();
        char** keys = pp->get_keys();
        for (int i = 0; keys[i] !=NULL; ++i) {
            char* value = pp->get(keys[i]);
/*
#ifdef ORDER
            printf("property key : %s\n", keys[i]);
            printf("property value : %s\n", value);
#endif
*/
            bool added = PropPut(jenv, jprops, keys[i], value);
            pp->destroy(value);
            if (!added) {
                break;
            }
        }
        pp->destroy(keys);
    }

    return jprops;
}

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    traceInstructions
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMExecutionEngine_traceInstructions
  (JNIEnv *jenv, jclass, jboolean)
{
    //ThrowNew_Quick(jenv, "java/lang/UnsupportedOperationException", NULL);
    return;
}

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    traceMethodCalls
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMExecutionEngine_traceMethodCalls
  (JNIEnv *jenv, jclass, jboolean)
{
    //ThrowNew_Quick(jenv, "java/lang/UnsupportedOperationException", NULL);
    return;
}

#ifdef ORDER
extern bool vm_order_record;
extern FILE *order_system_call[];
#endif

/*
* Class:     java_lang_VMExecutionEngine
* Method:    currentTimeMillis
* Signature: ()J
*/
JNIEXPORT jlong JNICALL Java_java_lang_VMExecutionEngine_currentTimeMillis
(JNIEnv *, jclass) {

    jlong result = apr_time_now()/1000;

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
        fprintf(order_system_call[tid], "[%d] ", 7);
#endif
        fprintf(order_system_call[tid], "%ld\n", result);
        //fwrite(&result, sizeof(jlong), 1, order_system_call[tid]);
//        fflush(order_system_call[tid]);
//        printf("Java_java_lang_VMExecutionEngine_currentTimeMillis : %d\n", result);
    }
    else
    {
        U_32 tid = hythread_self()->thread_id;
        jlong result_for_log = 0;
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
			
        //fread(&result, sizeof(jlong), 1, order_system_call[tid]);
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call[tid], "[%d] ", &bit_num);
        assert(bit_num == 7);
#endif

        fscanf(order_system_call[tid], "%ld\n", &result_for_log);

        printf("[CurrentTimeMillisMapping]: %ld ==>  %ld \n", result_for_log, result);
        result = result_for_log;
//        printf("Java_java_lang_VMExecutionEngine_currentTimeMillis : %d\n", result);
    }
#endif

/*
#ifdef ORDER
    if(vm_order_record){
        U_32 thread_id = hythread_self()->thread_id;
        printf("thread_id : %d\n", thread_id);

        char name[100];
        sprintf(name, "SYSTEM_CALL_TIME_%d.log", thread_id);

        if (systemCallTimeFile[thread_id] == NULL)
            systemCallTimeFile[thread_id] = fopen64(name, "w+");
        fwrite(&result, sizeof(jlong), 1, systemCallTimeFile[thread_id]);
        fflush(systemCallTimeFile[thread_id]);
        printf("Java_java_lang_VMExecutionEngine_currentTimeMillis : %d\n", result);
    }
    else
    {
        U_32 thread_assigned_id;
        thread_assigned_id = hythread_self()->thread_id;
        printf("thread_assigned_id : %d\n", thread_assigned_id);


        char name[100];
        sprintf(name, "SYSTEM_CALL_TIME_%d.log", thread_assigned_id);

        if (systemCallTimeFile[thread_assigned_id] == NULL)
            systemCallTimeFile[thread_assigned_id] = fopen64(name, "r");
        fread(&result, sizeof(jlong), 1, systemCallTimeFile[thread_assigned_id]);
        printf("Java_java_lang_VMExecutionEngine_currentTimeMillis : %d\n", result);
    }
#endif
*/

    return result;
}

/*
* Class:     java_lang_VMExecutionEngine
* Method:    nanoTime
* Signature: ()J
*/
JNIEXPORT jlong JNICALL Java_java_lang_VMExecutionEngine_nanoTime
(JNIEnv *, jclass) {

    jlong result = port_nanotimer();

#ifdef ORDER

#ifdef ORDER_DEBUG
    printf("[ENTER] : Java_java_lang_VMExecutionEngine_nanoTime\n");
#endif

    if(vm_order_record){
        U_32 tid = hythread_self()->thread_id;
        if (systemCallTimeFile[tid] == NULL){
            char name[40];
            sprintf(name, "SYSTEM_CALL_TIME.%d.log", tid);

            systemCallTimeFile[tid] = fopen64(name, "a+");
#ifdef ORDER_DEBUG
            if(systemCallTimeFile[tid]== NULL){
                printf("[ORDER]: could NOT open  file %s\n", name);
            }
            fseek(systemCallTimeFile[tid], 0, SEEK_SET);
#endif
        }
#ifdef ORDER_DEBUG
        fprintf(systemCallTimeFile[tid], "[%d] ", 36);
#endif
        fprintf(systemCallTimeFile[tid], "%ld\n", result);
    }
    else
    {
        U_32 tid = hythread_self()->thread_id;
        jlong result_for_log = 0;
        if (systemCallTimeFile[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL_TIME.%d.log", tid);
    
            systemCallTimeFile[tid] = fopen64(name, "r");
#ifdef ORDER_DEBUG
            if(systemCallTimeFile[tid]== NULL){
                printf("[ORDER]: could NOT open  file %s\n", name);
            }
            fseek(systemCallTimeFile[tid], 0, SEEK_SET);
#endif
        }
			
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(systemCallTimeFile[tid], "[%d] ", &bit_num);
        assert(bit_num == 36);
#endif

        fscanf(systemCallTimeFile[tid], "%ld\n", &result_for_log);
        result = result_for_log;

    }

#endif//#ifdef ORDER

    return result;

}

/*
* Class:     java_lang_VMExecutionEngine
* Method:    mapLibraryName
* Signature: (Ljava/lang/String;)Ljava/lang/String;
*/
JNIEXPORT jstring JNICALL Java_java_lang_VMExecutionEngine_mapLibraryName
(JNIEnv *jenv, jclass, jstring jlibname) {

#ifdef ORDER_DEBUG
    printf("[ENTER] : Java_java_lang_VMExecutionEngine_mapLibraryName\n");
#endif

    jstring res = NULL;
    if(jlibname) {
        const char* libname = GetStringUTFChars(jenv, jlibname, NULL);
        apr_pool_t *pp;
        if (APR_SUCCESS == apr_pool_create(&pp, 0)) {
            res = NewStringUTF(jenv, port_dso_name_decorate(libname, pp));
            apr_pool_destroy(pp);
        }
        ReleaseStringUTFChars(jenv, jlibname, libname);
    }
    return res;
}
