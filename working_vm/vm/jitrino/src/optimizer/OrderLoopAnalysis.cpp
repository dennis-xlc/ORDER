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


#include "escapeanalyzer.h"
#include "Log.h"
#include "Inst.h"
#include "Dominator.h"
#include "OrderLoopAnalysis.h"
#include "globalopndanalyzer.h"
#include "optimizer.h"
#include "FlowGraph.h"

#include <algorithm>

namespace Jitrino {

DEFINE_SESSION_ACTION(PthreadOptAnalysisPass, order_pthread_anal, "Order Pthread Analysis")

void
PthreadOptAnalysisPass::_run(IRManager& irm) {
    computeDominators(irm);
    DominatorTree* dominatorTree = irm.getDominatorTree();
    assert(dominatorTree->isValid());
    OrderLoopIdentifier lb(irm.getNestedMemoryManager(), irm, *dominatorTree, irm.getFlowGraph().hasEdgeProfile());
    bool marked = lb.identifyMassivePthread();
    if (marked)
        return;
    lb.computeLoops(true);
    lb.identifyPthreadInLoops();
}

OrderLoopIdentifier::OrderLoopIdentifier(MemoryManager& mm, IRManager& irm, 
                         DominatorTree& d, bool useProfile) 
    : loopMemManager(mm), dom(d), irManager(irm), 
      instFactory(irm.getInstFactory()), fg(irm.getFlowGraph()), info(NULL), root(NULL)
{
}


template <class IDFun>
class OrderLoopMarker {
public:
    static U_32 markNodesOfLoop(StlBitVector& nodesInLoop, Node* header, Node* tail);
private:
    static U_32 backwardMarkNode(StlBitVector& nodesInLoop, Node* node, StlBitVector& visited);
    static U_32 countInsts(Node* node);
};

template <class IDFun>
U_32 OrderLoopMarker<IDFun>::countInsts(Node* node) {
    U_32 count = 0;
    Inst* first = (Inst*)node->getFirstInst();
    if (first != NULL) {
        for(Inst* inst = first->getNextInst(); inst != NULL; inst = inst->getNextInst())
            ++count;
    }
    return count;
}

template <class IDFun>
U_32 OrderLoopMarker<IDFun>::backwardMarkNode(StlBitVector& nodesInLoop, 
                                Node* node, 
                                StlBitVector& visited) {
    static IDFun getId;
    if(visited.setBit(node->getId()))
        return 0;

    U_32 count = countInsts(node);
    nodesInLoop.setBit(getId(node));

    const Edges& inEdges = node->getInEdges();
    Edges::const_iterator eiter;
    for(eiter = inEdges.begin(); eiter != inEdges.end(); ++eiter) {
        Edge* e = *eiter;
        count += backwardMarkNode(nodesInLoop, e->getSourceNode(), visited);
    }
    return count;
}

// 
// traverse graph backward starting from tail until header is reached
//
template <class IDFun>
U_32 OrderLoopMarker<IDFun>::markNodesOfLoop(StlBitVector& nodesInLoop,
                               Node* header,
                               Node* tail) {
    static IDFun getId;
    U_32 maxSize = ::std::max(getId(header), getId(tail));
    MemoryManager tmm("OrderLoopIdentifier::markNodesOfLoop.tmm");
    StlBitVector visited(tmm, maxSize);

    // mark header is visited
    U_32 count = countInsts(header);
    nodesInLoop.setBit(getId(header));
    visited.setBit(header->getId());

    // starting backward traversal
    return count + backwardMarkNode(nodesInLoop, tail, visited);
}

void OrderLoopIdentifier::markPthreadOptimize()
{
    irManager.pthread_opt_enable = true;

//    printf("pthread optimization candidate identified : Method Name : %s, Method Sig : %s\n",
//		irManager.getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(), 
//		irManager.getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());
}

bool OrderLoopIdentifier::identifyMassivePthread()
{
    U_32 count = 0;

    const Nodes& nodes = irManager.getFlowGraph().getNodesPostOrder();
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) 
    {
        Node* node = *it;
        if (node->isBlockNode())
        {
            for (Inst * inst=(Inst*)node->getLastInst(); inst!=NULL; inst=inst->getPrevInst())
            {
                if (inst->isMultiSrcInst())
                {
                    if (((MultiSrcInst*)inst)->getParentObject() ||  ((MultiSrcInst*)inst)->getParentClass())
                    {
                        count++;
                        if (count >= 4)
                        {
                            markPthreadOptimize();
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}






// Perform loop inversion for each recorded loop.
void OrderLoopIdentifier::identifyPthreadInLoops(StlVector<Edge*>& loopEdgesIn) {





    // Mark the temporaries that are local to a given basic block
    GlobalOpndAnalyzer globalOpndAnalyzer(irManager);
    globalOpndAnalyzer.doAnalysis();

    // Set def-use chains
    MemoryManager peelmem("IdentifyPthreadOptimize.peelmem");

    StlVector<Edge*> loopEdges(peelmem);

    StlVector<Edge*>::iterator i = loopEdgesIn.begin();
    while (i != loopEdgesIn.end()) {
        loopEdges.insert(loopEdges.end(), *i);
        i++;
    }

    
    for(i = loopEdges.begin(); i != loopEdges.end(); ++i) {
        // The current back-edge
        Edge* backEdge = *i;

        // The initial header
        Node* header = backEdge->getTargetNode();
        if (header->isDispatchNode()) {
            continue;
        }
        assert(header->getInDegree() == 2);

        // The initial loop end
        Node* tail = backEdge->getSourceNode();

        // Temporary memory for peeling
        U_32 maxSize = fg.getMaxNodeId();
        MemoryManager tmm("IdentifyPthreadOptimize.tmm");
        
        // Compute nodes in loop
        StlBitVector nodesInLoop(tmm, maxSize);
        OrderLoopMarker<IdentifyByID>::markNodesOfLoop(nodesInLoop, header, tail);


        const Nodes& nodes = irManager.getFlowGraph().getNodesPostOrder();
        for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
            Node* node = *it;
            if (nodesInLoop.getBit(node->getId()))
            {
                if (node->isBlockNode())
                {
                    for (Inst * inst=(Inst*)node->getLastInst(); inst!=NULL; inst=inst->getPrevInst())
                    {
                        if (inst->isMultiSrcInst())
                        {
                            if (((MultiSrcInst*)inst)->getParentObject() ||  ((MultiSrcInst*)inst)->getParentClass())
                            {
                                markPthreadOptimize();
                                return;
                            }
                        }
                    }
                }
            }
        }
    }


}

class EdgeCoalescerCallbackImpl : public EdgeCoalescerCallback {
friend class OrderLoopIdentifier;
public:
    EdgeCoalescerCallbackImpl(IRManager& _irm) : ssaAffected(false), irm(_irm){}
    
    virtual void coalesce(Node* header, Node* newPreHeader, U_32 numEdges) {
        InstFactory& instFactory = irm.getInstFactory();
        OpndManager& opndManager = irm.getOpndManager();
        Inst* labelInst = (Inst*)header->getFirstInst();
        assert(labelInst->isLabel() && !labelInst->isCatchLabel());
        newPreHeader->appendInst(instFactory.makeLabel());
        newPreHeader->getFirstInst()->setBCOffset(labelInst->getBCOffset());
        if (numEdges > 1 ) {
            for (Inst* phi = labelInst->getNextInst();phi!=NULL && phi->isPhi(); phi = phi->getNextInst()) {
                Opnd *orgDst = phi->getDst();
                SsaVarOpnd *ssaOrgDst = orgDst->asSsaVarOpnd();
                assert(ssaOrgDst);
                VarOpnd *theVar = ssaOrgDst->getVar();
                assert(theVar);
                SsaVarOpnd* newDst = opndManager.createSsaVarOpnd(theVar);
                Inst *newInst = instFactory.makePhi(newDst, 0, 0);
                PhiInst *newPhiInst = newInst->asPhiInst();
                assert(newPhiInst);
                U_32 n = phi->getNumSrcOperands();
                for (U_32 i=0; i<n; i++) {
                    instFactory.appendSrc(newPhiInst, phi->getSrc(i));
                }
                PhiInst *phiInst = phi->asPhiInst();
                assert(phiInst);
                instFactory.appendSrc(phiInst, newDst);
                newPreHeader->appendInst(newInst);
                ssaAffected = true;
            }
        }
    }

private:
    bool ssaAffected;
    IRManager& irm;
};

void OrderLoopIdentifier::computeLoops(bool normalize) {
    // find all loop headers
    LoopTree* lt = irManager.getLoopTree();
    if (lt == NULL) {
        MemoryManager& mm = irManager.getMemoryManager();
        EdgeCoalescerCallbackImpl* c = new (mm) EdgeCoalescerCallbackImpl(irManager);
        lt = new LoopTree(irManager.getMemoryManager(), &irManager.getFlowGraph(),c);
        irManager.setLoopTree(lt);
    }
    EdgeCoalescerCallbackImpl* callback = (EdgeCoalescerCallbackImpl*)lt->getCoalesceCallback();
    callback->ssaAffected = false;
    lt->rebuild(normalize);
}

void OrderLoopIdentifier::identifyPthreadInLoops() {
//    printf("enter analysis : Method Name : %s, Method Sig : %s\n",
//		irManager.getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(), 
//		irManager.getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());

    LoopTree*lt = irManager.getLoopTree();
    if (!lt->hasLoops())  {
        return;
    }

    MemoryManager tmm("OrderLoopIdentifier::peelLoops");
    Edges loopEdges(tmm);
    const Nodes& nodes = irManager.getFlowGraph().getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        const Edges& edges = node->getOutEdges();
        for (Edges::const_iterator ite = edges.begin(), ende = edges.end(); ite!=ende; ++ite) {
            Edge* edge = *ite;
            if (lt->isBackEdge(edge)) {
                loopEdges.push_back(edge);
            }
        }
    }
    identifyPthreadInLoops(loopEdges);
}
}//namespace Jitrino 
