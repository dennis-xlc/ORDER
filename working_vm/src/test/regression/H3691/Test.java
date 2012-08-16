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

package org.apache.harmony.drlvm.tests.regression.h3691;

import junit.framework.TestCase;

public class Test extends TestCase {

    public void test() {
        assertEquals(50, foo()); 
    } 

    static int foo() { 
        int a = 10; 
        boolean b = false; 
        if ( (a > 10 ? 20 : 30) == 30 ) { 
            b = true; 
        } 

        if (!b) { 
            return 40; 
        } 
        return 50; 
    }
}
