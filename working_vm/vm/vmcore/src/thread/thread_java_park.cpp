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
 * @file thread_java_park.c
 * @brief Java thread park/unpark functions
 */

#include <apr_time.h>
#include "open/hythread_ext.h"
#include "jthread.h"
#include "vm_threads.h"


#ifdef ORDER
extern bool vm_order_record;
FILE *order_system_call_park[ORDER_THREAD_NUM];
#endif

/**
 * Parks the current thread.
 *
 * Stops the current thread from executing until it is unparked or interrupted.
 * Unlike wait or sleep, the interrupted flag is NOT cleared by this API.
 *
 * @sa java.util.concurrent.locks.LockSupport.park() 
 */
IDATA VMCALL jthread_park()
{
    return hythread_park(0, 0);
} // jthread_park

/**
 * Parks the current thread with the specified timeout.
 *
 * Stops the current thread from executing until it is unparked, interrupted,
 * or the specified timeout elapses.
 * Unlike wait or sleep, the interrupted flag is NOT cleared by this API.
 *
 * @param[in] millis timeout in milliseconds
 * @param[in] nanos timeout in nanoseconds
 * @sa java.util.concurrent.locks.LockSupport.park() 
 */
IDATA VMCALL jthread_timed_park(jlong millis, jint nanos)
{
    IDATA result = hythread_park((I_64) millis, (IDATA) nanos);
#ifdef ORDER
    if(vm_order_record){

        U_32 tid = hythread_self()->thread_id;
        if (order_system_call_park[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL_PARK.%d.log", tid);

            order_system_call_park[tid] = fopen64(name, "a+");
            assert(order_system_call_park[tid] != NULL);
        }
		
#ifdef ORDER_DEBUG

        fprintf(order_system_call_park[tid], "[%d] ", 37);

//        fprintf(order_system_call[tid], "%d ", alloc_count);
#endif
        fprintf(order_system_call_park[tid], "%d %d %d\n", (int)millis, (int)nanos, (int)result);
    }
    else
    {
        U_32 tid = hythread_self()->thread_id;
        if (order_system_call_park[tid] == NULL)
        {
            char name[40];
            sprintf(name, "SYSTEM_CALL_PARK.%d.log", tid);

            order_system_call_park[tid] = fopen64(name, "r");
#ifdef ORDER_DEBUG
            assert(order_system_call_park[tid] != NULL);
#endif
        }
#ifdef ORDER_DEBUG
        int bit_num;
        fscanf(order_system_call_park[tid], "[%d] ", &bit_num);
        assert(bit_num == 37);
#endif
        int millis_from_log = 0;
        int nanos_from_log = 0;
        int result_from_log = 0;
        fscanf(order_system_call_park[tid], "%d %d %d\n", &millis_from_log, &nanos_from_log, &result_from_log);
        assert(millis_from_log == (int)millis);
        assert(nanos_from_log == (int)nanos);
//        assert(result_from_log == (int)result);
        result = result_from_log;
        
     }
#endif
    return result;

} // jthread_timed_park

/**
 * Unparks the given thread.
 *
 * If the thread is parked, it will return from park.
 * If the thread is not parked, its 'UNPARKED' flag will be set, and it will
 * return immediately the next time it is parked.
 *
 * Note that unparks are not counted. Unparking a thread once is the same as
 * unparking it n times.
 *
 * @param[in] java_thread thread that needs to be unparked
 * @sa java.util.concurrent.locks.LockSupport.unpark() 
 */
IDATA VMCALL jthread_unpark(jthread java_thread)
{
    assert(java_thread);
    hythread_t native_thread = jthread_get_native_thread(java_thread);
    hythread_unpark(native_thread);
    return TM_ERROR_NONE;
} // jthread_unpark

/**
 * Parks the current thread until the specified deadline
 *
 * Stops the current thread from executing until it is unparked, interrupted,
 * or until the specified deadline.
 * Unlike wait or sleep, the interrupted flag is NOT cleared by this API.
 *
 * @param[in] millis absolute time in milliseconds to wait until
 * @sa java.util.concurrent.locks.LockSupport.parkUntil() 
 */
IDATA VMCALL jthread_park_until(jlong millis)
{
    jlong delta = millis - apr_time_now() / 1000;
    if (delta <= 0)
        return TM_ERROR_NONE;
    return hythread_park((I_64) delta, 0);
} // jthread_park_until
