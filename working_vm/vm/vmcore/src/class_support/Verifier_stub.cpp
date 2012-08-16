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
 * @author Pavel Pervov
 */


#define LOG_DOMAIN "verifier"
#include "cxxlog.h"

#include "verifier.h"
#include "Class.h"
#include "classloader.h"
#include "environment.h"
#include "open/vm.h"
#include "lock_manager.h"

bool Class::verify(const Global_Env * env)
{
    // fast path
    if (m_state >= ST_BytecodesVerified)
        return true;

    LMAutoUnlock aulock(m_lock);
    if (m_state >= ST_BytecodesVerified)
        return true;

    if (is_array()) {
        // no need do bytecode verification for arrays
        m_state = ST_BytecodesVerified;
        return true;
    }

    /**
     * Get verifier enable status
     */
    Boolean is_forced = env->verify_all;
    Boolean is_strict = env->verify_strict;
    Boolean is_bootstrap = m_class_loader->IsBootstrap();
    Boolean is_enabled = env->verify;

    /**
     * Verify class
     */
    if (is_enabled == 1 && !is_interface()
        && (is_bootstrap == FALSE || is_forced == TRUE)) {
        char *error;
        vf_Result result =
            vf_verify_class(this, is_strict, &error);
        if (VF_OK != result) {
            aulock.ForceUnlock();
            REPORT_FAILED_CLASS_CLASS(m_class_loader, this,
                                      "java/lang/VerifyError", error);
            vf_release_memory(error);
            return false;
        }
    }
    m_state = ST_BytecodesVerified;

    return true;
} // Class::verify



#ifdef ORDER
extern bool vm_order_record;
FILE *order_system_call_verify[ORDER_THREAD_NUM];

POINTER_SIZE_INT prefetch_hashcode_verify[ORDER_THREAD_NUM];
#include "open/hythread.h"
#endif


bool Class::verify_constraints(const Global_Env * env)
{
#ifdef ORDER

    if(m_hash_code == 0){
        m_hash_code = hash_function();
    }

    if(!vm_order_record){
        lock();

        if (m_state == ST_Prepared && is_array()) {
            // no need do constraint verification for arrays
            m_state = ST_ConstraintsVerified;
            unlock();
            return true;
        }
        unlock();


        U_32 tid = hythread_self()->thread_id;
        if(order_system_call_verify[tid] == NULL){
            char name[40];
            sprintf(name, "SYSTEM_CALL_VERIFY.%d.log", tid);

            order_system_call_verify[tid] = fopen64(name, "r");
            if(order_system_call_verify[tid] == NULL){
                goto wait_for_verify;
            }

#ifdef ORDER_DEBUG
            if (order_system_call_verify[tid] != NULL)
                fseek(order_system_call_verify[tid], 0, SEEK_SET);

            assert(order_system_call_verify[tid] != NULL);
            assert(ferror(order_system_call_verify[tid]) == 0);
            assert(order_system_call_verify[tid]->_IO_read_ptr != NULL);
#endif

        }

        if(0 == (U_32)prefetch_hashcode_verify[tid]){
            if(!feof(order_system_call_verify[tid])){
//                fread(&prefetch_hashcode_verify[tid], sizeof(prefetch_hashcode_verify[tid]), 1, order_system_call_prepare[tid]);
                fscanf(order_system_call_verify[tid], "%d ", &prefetch_hashcode_verify[tid]);
            }
        }

        if((U_32)m_hash_code != (U_32)prefetch_hashcode_verify[tid]){
			
wait_for_verify:
            while(m_state == ST_Prepared){
#ifdef ORDER_DEBUG
                printf("Thread[%d]: usleep in verify!!!\n", hythread_self()->thread_id);
#endif
                usleep(1000);
            }
            return true;

        }

        prefetch_hashcode_verify[tid] = 0;

    }
    else
    {
#endif
    // fast path
    switch (m_state) {
    case ST_ConstraintsVerified:
    case ST_Initializing:
    case ST_Initialized:
    case ST_Error:
        return true;
    }

    // lock class
    lock();

    // check verification stage again
    switch (m_state) {
    case ST_ConstraintsVerified:
    case ST_Initializing:
    case ST_Initialized:
    case ST_Error:
        unlock();
        return true;
    }
    assert(m_state == ST_Prepared);

    if (is_array()) {
        // no need do constraint verification for arrays
        m_state = ST_ConstraintsVerified;
        unlock();
        return true;
    }
    // unlock a class before calling to verifier
    unlock();

#ifdef ORDER
    }
#endif

    // get verifier enable status
    Boolean is_strict = env->verify_strict;

#ifdef ORDER
    if(vm_order_record){
        U_32 tid = hythread_self()->thread_id;
        if(order_system_call_verify[tid] == NULL){
            char name[40];
            sprintf(name, "SYSTEM_CALL_VERIFY.%d.log", tid);

            order_system_call_verify[tid] = fopen64(name, "a+");
#ifdef ORDER_DEBUG
            assert(order_system_call_verify[tid]);
#endif
        }

        fprintf(order_system_call_verify[tid], "%d \n", m_hash_code);
        
    }
#endif


    // check method constraints
    char *error;
    vf_Result result =
        vf_verify_class_constraints(this, is_strict, &error);

    // lock class and check result
    lock();
    switch (m_state) {
    case ST_ConstraintsVerified:
    case ST_Initializing:
    case ST_Initialized:
    case ST_Error:
        unlock();
        return true;
    }
    if (VF_OK != result) {
        unlock();
        if (result == VF_ErrorLoadClass) {
            // Exception is raised by class loading
            // and passed through verifier unchanged
            assert(exn_raised());
        } else {
            REPORT_FAILED_CLASS_CLASS(m_class_loader, this,
                                      "java/lang/VerifyError", error);
            vf_release_memory(error);
        }
        return false;
    }
    m_state = ST_ConstraintsVerified;

    // unlock class
    unlock();

    return true;
} // Class::verify_constraints
