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
 * @author Intel, Pavel A. Ozhdikhin
 *
 */

#ifndef _LOOP_H_
#define _LOOP_H_

#include "BitSet.h"
#include "Stl.h"
#include "Tree.h"
#include "irmanager.h"
#include "LoopTree.h"

namespace Jitrino {

class OrderLoopIdentifier { 
    

public:
    OrderLoopIdentifier(MemoryManager& mm, IRManager& irm, DominatorTree& d, bool useProfile); 

    void computeLoops(bool normalize);
    void identifyPthreadInLoops();
    bool identifyMassivePthread();
private:
    
    class IdentifyByID {
    public:
        U_32 operator() (Node* node) { return node->getId(); }
    };
    class IdentifyByDFN {
    public:
        U_32 operator() (Node* node) { return node->getDfNum(); }
    };

    void        identifyPthreadInLoops(StlVector<Edge*>& backEdges);
    U_32      markNodesOfLoop(StlBitVector& nodesInLoop,Node* header,Node* tail);
    void       markPthreadOptimize();

    MemoryManager&  loopMemManager; // for creating loop hierarchy
    DominatorTree&  dom;           // dominator information
    IRManager&      irManager;
    InstFactory&    instFactory;  // create new label inst for blocks
    ControlFlowGraph& fg;
    LoopTree*       info;
    LoopNode*       root;
    bool            invert;
};

} //namespace Jitrino 

#endif // _LOOP_H_
