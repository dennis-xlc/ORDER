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
 * @author Sergey L. Ivashin
 */

#include "Ia32RegAllocCheck.h"
#include "Log.h"
#include "XTimer.h"


namespace Jitrino
{

namespace Ia32
{

using ::std::endl;
using ::std::ostream;


//========================================================================================
// utility routines
//========================================================================================


static ostream& operator << (ostream& os, const Inst& x)
{
    return os << "I#" << x.getId();
}


static ostream& operator << (ostream& os, const Opnd& x)
{
    return os << "O#" << x.getFirstId();
}


static ostream& operator << (ostream& os, RegName x)
{
    return os << getRegNameString(x);
}


//========================================================================================
// class RegAllocCheck
//========================================================================================

bool RegAllocCheck::run (bool checkloc)
{
    static CountTime regAllocCheckTimer("ia32::regAllocCheck");
    AutoTimer tm(regAllocCheckTimer);

    opandcount = irm.getOpndCount();
    errors = 0;
    headprinted = false;

    if (checkloc)
        checkLocations();

    const Nodes& nodes = irm.getFlowGraph()->getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        if (node->isBlockNode())
        {
            bblock = node;
            checkLiveness();
            checkConstraints();
        }
    }

    if (errors != 0)
        header() << endl << "ERRORS DETECTED: " << errors << endl;   
    else if (Log::isEnabled())
        header() << endl << "No errors detected" << endl;    

    return errors == 0;
}                                                                               

//This is the Harmony bug
#ifdef ORDER //yzm

const U_32 IRMaxRegSize = 8;

const size_t MaxRegs = IRMaxRegNamesSameKind*IRMaxRegKinds*IRMaxRegSize;
#else
const size_t MaxRegs = IRMaxRegNamesSameKind*IRMaxRegKinds;
#endif

#ifdef ORDER //yzm
static RegName regName (int x)
{
    U_32 sidx = x / (IRMaxRegNamesSameKind * IRMaxRegKinds);

    U_32 old_idx = x % (IRMaxRegNamesSameKind * IRMaxRegKinds);
    U_32 k = old_idx / IRMaxRegNamesSameKind;
    U_32 i = old_idx % IRMaxRegNamesSameKind;

    U_32 s;
    switch(sidx)
    {
    case(0) : s = OpndSize_Null;	break;
    case(1) : s = OpndSize_8;	break;
    case(2) : s = OpndSize_16;	break;
    case(3) : s = OpndSize_32;	break;
    case(4) : s = OpndSize_64;	break;
#if !defined(TESTING_ENCODER)
    case(5) : s = OpndSize_80;	break;
    case(6) : s = OpndSize_128;	break;
#endif
    default : s = OpndSize_Any;
    }

//    printf("s : %d\n",s);
//    printf("k : %d\n",k);
//    printf("i : %d\n",i);

    return getRegName((OpndKind)k, (OpndSize)s, i);
}
#else
static RegName regName (int x)
{
    U_32 k = x / IRMaxRegNamesSameKind;
    return getRegName((OpndKind)k, Constraint::getDefaultSize(k), x % IRMaxRegNamesSameKind);
}
#endif

#ifdef ORDER //yzm
static size_t regIdx (const Opnd* opnd)
{
    RegName rn;
    if ((rn = opnd->getRegName()) == RegName_Null)
        return MaxRegs;

    size_t x = getRegIndex(rn),
           k = getRegKind(rn),
           s = getRegSize(rn); 

    size_t sidx;
    switch(s)
    {
    case(OpndSize_Null) : sidx = 0;	break;
    case(OpndSize_8) : sidx = 1;		break;
    case(OpndSize_16) : sidx = 2;	break;
    case(OpndSize_32) : sidx = 3;	break;
    case(OpndSize_64) : sidx = 4;	break;
#if !defined(TESTING_ENCODER)
    case(OpndSize_80) : sidx = 5;	break;
    case(OpndSize_128) : sidx = 6;	break;
#endif
    default : sidx = 7;
    }

    assert(x < IRMaxRegNamesSameKind && k < IRMaxRegKinds && sidx < IRMaxRegSize); 

    return sidx*IRMaxRegNamesSameKind*IRMaxRegKinds + k*IRMaxRegNamesSameKind + x;
}
#else
static size_t regIdx (const Opnd* opnd)
{
    RegName rn;
    if ((rn = opnd->getRegName()) == RegName_Null)
        return MaxRegs;

    size_t x = getRegIndex(rn),
           k = getRegKind(rn); 

    assert(x < IRMaxRegNamesSameKind && k < IRMaxRegKinds); 

    return k*IRMaxRegNamesSameKind + x;
}
#endif

void RegAllocCheck::checkLiveness ()
{
    lastbb = 0;

    BitSet lives(mm, (U_32)opandcount);
    irm.getLiveAtExit(bblock, lives);

    Opnd* regdefs[MaxRegs],
        * reguses[MaxRegs],
        * regnxts[MaxRegs];

    size_t ridx;

    for (ridx = 0; ridx != MaxRegs; ++ridx)
        regdefs[ridx] = 0,
        reguses[ridx] = 0,
        regnxts[ridx] = 0;

    Opnd* opnd;
    BitSet::IterB ls(lives);
    for (int i = ls.getNext(); i != -1; i = ls.getNext())
    {
        opnd = irm.getOpnd(i);
        if ((ridx = regIdx(opnd)) < MaxRegs)
        {
            if (regnxts[ridx] == 0)
                regnxts[ridx] = opnd;
            else
                if (regnxts[ridx] != opnd)
                {
                    error() << "at end of block," << regName((int)ridx) 
                            << " assigned to " << *regnxts[ridx] << " and " << *opnd << endl;
                }
        }
    }

//  iterate over instructions towards the top of the block

    for (Inst* inst = (Inst*)bblock->getLastInst(); inst != 0; inst = inst->getPrevInst())
    {
        const U_32 props = inst->getProperties();

    //  In general, operands can be stored in arbitrary order
        Inst::Opnds opnds(inst, Inst::OpndRole_All);
        for (Inst::Opnds::iterator it = opnds.begin(); it != opnds.end(); it = opnds.next(it)){
            opnd = opnds.getOpnd(it);
            if ((ridx = regIdx(opnd)) < MaxRegs)
            {
                const U_32 roles = inst->getOpndRoles(it);

                if ((roles & Inst::OpndRole_Def) != 0)
                {
                    if (regdefs[ridx] == 0)
                        regdefs[ridx] = opnd;
                    else
                        if (regdefs[ridx] != opnd)
                        {
                            error() << "  Invalid definitions at " << *inst << " " << regName((int)ridx) 
                                    << " of " << *regdefs[ridx] << " or " << *opnd << endl;
                        }
                }

                if ((roles & Inst::OpndRole_Use) != 0)
                {
                    if (reguses[ridx] == 0)
                        reguses[ridx] = opnd;
                    else
                        if (reguses[ridx] != opnd)
                        {
                            error() << "  Invalid usages at " << *inst << " " << regName((int)ridx) 
                                    << " of " << *reguses[ridx] << " or " << *opnd << endl;
                        }
                }
            }
        }
        for (ridx = 0; ridx != MaxRegs; ++ridx)
        {
            if (regdefs[ridx] != 0)
            {
                //  conditional definitions are ignored
                if ((props & Inst::Properties_Conditional) == 0)
                {
                    if (regdefs[ridx] != regnxts[ridx]) {
                        if (regnxts[ridx] != 0)
                        {
                            error() << "  " << *inst << " " << regName((int)ridx) 
                                    << " invalid usage of " << *regnxts[ridx]
                                    << " instead of " << *regdefs[ridx] << endl;

                            printf("Block : %d\n", bblock->getId());
                            printf("Id : %d\n", regnxts[ridx]->getId());
                            printf("First Id : %d\n", regnxts[ridx]->getFirstId());
                            {
                                RegName rn = regnxts[ridx]->getRegName();
                                unsigned char regIndex = getRegIndex(rn);
                                unsigned char regKind = getRegKind(rn);
                                unsigned char regSize = getRegSize(rn); 
                                size_t idx = (int)regIdx(regnxts[ridx]);
                                const char * regNameString=getRegNameString(rn);
                                std::cout << "regIndex : " << (int)regIndex << std::endl;
                                std::cout << "regKind : " << (int)regKind << std::endl;
                                std::cout << "regSize : " << (int)regSize << std::endl;
                                std::cout << "regIdx : " << (int)idx << std::endl;
                                std::cout << "regNameString : " << (regNameString?regNameString:"???") << std::endl;
                                std::cout<< "Reg Name : " << regName((int)regIdx(regnxts[ridx])) << std::endl;
                            }

							
                            printf("Def Id : %d\n", regdefs[ridx]->getId());
                            printf("Def First Id : %d\n", regdefs[ridx]->getFirstId());
                            {
                                RegName rn = regdefs[ridx]->getRegName();
                                unsigned char regIndex = getRegIndex(rn);
                                unsigned char regKind = getRegKind(rn);
                                unsigned char regSize = getRegSize(rn); 
                                size_t idx = (int)regIdx(regdefs[ridx]);
                                const char * regNameString=getRegNameString(rn);
                                std::cout << "Def regIndex : " << (int)regIndex << std::endl;
                                std::cout << "Def regKind : " << (int)regKind << std::endl;
                                std::cout << "Def regSize : " << (int)regSize << std::endl;
                                std::cout << "Def regIdx : " << (int)idx << std::endl;
                                std::cout << "Def regNameString : " << (regNameString?regNameString:"???") << std::endl;
                                std::cout<< "Def Reg Name : " << regName((int)regIdx(regdefs[ridx])) << std::endl;
                            }
                        }
                        else
                        {
                            //  dead def
                        }
                    }
                    regnxts[ridx] = 0;  // this register can used in the instruction
                }
            }

            else if (reguses[ridx] != 0)
            {
                if (reguses[ridx] != regnxts[ridx] && regnxts[ridx] != 0)
                {
                    error() << "  " << *inst << " " << regName((int)ridx) 
                            << " invalid usage of " << *reguses[ridx]
                            << " or " << *regnxts[ridx] << endl;

                            printf("Block : %d\n", bblock->getId());
                            printf("Id : %d\n", regnxts[ridx]->getId());
                            printf("First Id : %d\n", regnxts[ridx]->getFirstId());
                            {
                                RegName rn = regnxts[ridx]->getRegName();
                                unsigned char regIndex = getRegIndex(rn);
                                unsigned char regKind = getRegKind(rn);
                                unsigned char regSize = getRegSize(rn); 
                                size_t idx = (int)regIdx(regnxts[ridx]);
                                const char * regNameString=getRegNameString(rn);
                                std::cout << "regIndex : " << (int)regIndex << std::endl;
                                std::cout << "regKind : " << (int)regKind << std::endl;
                                std::cout << "regSize : " << (int)regSize << std::endl;
                                std::cout << "regIdx : " << (int)idx << std::endl;
                                std::cout << "regNameString : " << (regNameString?regNameString:"???") << std::endl;
                                std::cout<< "Reg Name : " << regName((int)regIdx(regnxts[ridx])) << std::endl;
                            }


                            printf("Use Id : %d\n", reguses[ridx]->getId());
                            printf("Use First Id : %d\n", reguses[ridx]->getFirstId());
                            {
                                RegName rn = reguses[ridx]->getRegName();
                                unsigned char regIndex = getRegIndex(rn);
                                unsigned char regKind = getRegKind(rn);
                                unsigned char regSize = getRegSize(rn); 
                                size_t idx = (int)regIdx(reguses[ridx]);
                                const char * regNameString=getRegNameString(rn);
                                std::cout << "Def regIndex : " << (int)regIndex << std::endl;
                                std::cout << "Def regKind : " << (int)regKind << std::endl;
                                std::cout << "Def regSize : " << (int)regSize << std::endl;
                                std::cout << "Def regIdx : " << (int)idx << std::endl;
                                std::cout << "Def regNameString : " << (regNameString?regNameString:"???") << std::endl;
                                std::cout<< "Use Reg Name : " << regName((int)regIdx(reguses[ridx])) << std::endl;
                            }

                }

                regnxts[ridx] = reguses[ridx];
            }

        // clear for next instruction to be processed
            regdefs[ridx] = 0;
            reguses[ridx] = 0;
        }
    }
}


void RegAllocCheck::checkLocations ()
{
    for (size_t i = 0; i != opandcount; ++i)
    {
        Opnd* opnd = irm.getOpnd((U_32)i);
        if (!opnd->hasAssignedPhysicalLocation())
        {
            header() << "Not assigned opand " << *opnd << endl;
            errors++;
        }
    }
}


void RegAllocCheck::checkConstraints ()
{
    lastbb = 0;
    for (const Inst* inst = (Inst*)bblock->getFirstInst(); inst != 0; inst = inst->getNextInst())
    {
        Opnd* opnd;
        Inst::Opnds opnds(inst, Inst::OpndRole_AllDefs);
        U_32 mask = 0;
        for (Inst::Opnds::iterator it = opnds.begin(); it != opnds.end(); it = opnds.next(it))
        {
            opnd = opnds.getOpnd(it);
            Constraint cloc = opnd->getConstraint(Opnd::ConstraintKind_Location);
            mask |= 1<<it;
            Constraint c = ((Inst *)inst)->getConstraint(it, mask, cloc.getSize());    
            if (!cloc.isNull())
                if (c.getSize() == cloc.getSize() && !c.contains(cloc))
                    error() << "Constraint error " << *inst << " " << *opnd << endl;
        }
    }
}


ostream& RegAllocCheck::error ()
{
    if (lastbb != bblock)
    {
        lastbb = bblock;
        header() << "BasicBlock " << bblock->getId() 
                 << "[" << ((Inst*)bblock->getFirstInst())->getId() << " .. " 
                 << ((Inst*)bblock->getLastInst())->getId() << "]" 
                 << endl ;
    }
    errors++;
    return Log::out();
}


ostream& RegAllocCheck::header ()
{
    if (!headprinted)
    {
        headprinted = true;

        MethodDesc& md=irm.getMethodDesc();
        Log::out() << endl << "RegAllocCheck for " 
                   << md.getParentType()->getName() 
                   << "." << md.getName() << "(" << md.getSignatureString() << ")"
                   << endl;  
    }
    return Log::out();
}


} //namespace Ia32
} //namespace Jitrino
