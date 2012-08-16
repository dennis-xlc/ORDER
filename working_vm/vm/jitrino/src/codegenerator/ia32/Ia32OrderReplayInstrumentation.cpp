
#include "Ia32IRManager.h"
#include "Ia32CodeGenerator.h"
#include "Ia32Printer.h"


#include <pthread.h>
#include "Ia32OrderReplayInstrumentation.h"


namespace Jitrino
{
namespace Ia32{

#ifdef REPLAY_OPTIMIZE
#define OBJ_RW_BIT 0x800
#endif


static ActionFactory<ORDERRep> _order_rep("order_rep");

//const static char* format1 = "obj access : (%d, %d), %s, %s\n";
//const static char* format2 = "method entry[%d] : (%s), (%s, %s)\n";
//const static char* format3 = "method exit : (%s), (%s, %s)\n";
//const static char* format4 = "interleaving : (%s), (%s, %s)\n";
//const static char* format5 = "lock old : 0x%hx\n";
//const static char* format6 = "lock new : 0x%hx\n";
const static char* format7 = "[INSTANCE_OF]: object (%d, %d) in (%s, %s, %s) ==> %ld by thread %d\n";

static uint16 last_bc_offset = 0;

static void printObjectAccess(const char * format, POINTER_SIZE_INT opnd1, POINTER_SIZE_INT opnd2,
	POINTER_SIZE_INT opnd3, POINTER_SIZE_INT opnd4,  POINTER_SIZE_INT opnd5, POINTER_SIZE_INT opnd6)
{


        printf(format, opnd1, opnd2, opnd3, opnd4, opnd5, opnd6, pthread_self());

}


Inst* ORDERRep::genObjectPrintInst(const char * format, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3, Opnd * opnd4, Opnd * opnd5, Opnd *opnd6)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd1, opnd2, opnd3, opnd4, opnd5, opnd6};

    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)printObjectAccess));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}

void ORDERRep::appendCnt(Node* follow_node)
{
    assert(parentObj);
    Opnd* offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::counter_offset());
    Opnd* obj = parentObj;
    Opnd* cnt_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_addr, obj, offset);

    Opnd* cnt_orig = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr); 
    Opnd* cnt_res = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr);
    Opnd* one = irm->newImmOpnd(typeInt32, 1);
    Opnd* cnt_new_val = irm->newOpnd(typeInt32);
    
    Inst* increase_cnt_inst = irm->newInstEx(Mnemonic_SUB, 1, cnt_new_val, cnt_orig, one);
    Inst* save_cnt_res_inst = irm->newCopyPseudoInst(Mnemonic_MOV, cnt_res, cnt_new_val);

    //yzm
    //1 for debug
//    Inst* debug_print_inst_val = genPrintOpndInst(format1, cnt_new_val);


    follow_node->appendInst(get_cnt_addr_inst);
    follow_node->appendInst(increase_cnt_inst);
    follow_node->appendInst(save_cnt_res_inst);

    //yzm
    //1 for debug
//    follow_node->appendInst(debug_print_inst_val);


}


Inst* ORDERRep::genPrintOpndInst(const char* format, Opnd* opnd)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd};
    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)VMInterface::printfAddr()));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}

Inst* ORDERRep::genPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd1, opnd2};
    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)VMInterface::printfAddr()));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}

Inst* ORDERRep::genPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd1, opnd2, opnd3};
    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)VMInterface::printfAddr()));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}

Inst* ORDERRep::genPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3, Opnd* opnd4)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd1, opnd2, opnd3, opnd4};
    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)VMInterface::printfAddr()));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}


static void conditional_print3(const char * format, POINTER_SIZE_INT opnd1, POINTER_SIZE_INT opnd2, POINTER_SIZE_INT opnd3)
{
    if (VMInterface::isInitializing())
    {
    }
    else
    {
        printf(format, pthread_self(), opnd1, opnd2, opnd3);
    }
}


Inst* ORDERRep::genConditionalPrintOpndInst(const char * format, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd1, opnd2, opnd3};
    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)conditional_print3));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}

//#ifdef ORDER_DEBUG_ALLOC_INFO
Inst* ORDERRep::genConditionalPrintOpndInst(Opnd * opnd1, Opnd * opnd2, Opnd * opnd3)
{
//    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { opnd1, opnd2, opnd3};
    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)VMInterface::vmEnterMethod()));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}
//#endif

static void conditional_print4(const char * format, POINTER_SIZE_INT opnd1, POINTER_SIZE_INT opnd2, POINTER_SIZE_INT opnd3, POINTER_SIZE_INT opnd4)
{
    if (VMInterface::isInitializing())
    {
    }
    else
    {
        printf(format, opnd1, opnd2, opnd3, opnd4);
    }
}

Inst* ORDERRep::genConditionalPrintOpndInst(const char * format, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3, Opnd * opnd4)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd1, opnd2, opnd3, opnd4};
    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)conditional_print4));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}


typedef struct VM_VectorDummy
{
    ManagedObjectDummy object;
    I_32 length;
} VM_VectorDummy;

#define VM_VECTOR_RT_OVERHEAD ((unsigned)(ManagedObject::get_size() + sizeof(I_32)))


// The offset to the first element of a vector with 8-byte elements.
#define VM_VECTOR_FIRST_ELEM_OFFSET_8 ((VM_VECTOR_RT_OVERHEAD + 7) & ~7)


#ifdef POINTER64
#define VM_VECTOR_FIRST_ELEM_OFFSET_1_2_4 VM_VECTOR_FIRST_ELEM_OFFSET_8
#else
#define VM_VECTOR_FIRST_ELEM_OFFSET_1_2_4 VM_VECTOR_RT_OVERHEAD
#endif



static void conditional_print5(const char * format, POINTER_SIZE_INT opnd1, POINTER_SIZE_INT opnd2, POINTER_SIZE_INT opnd3, POINTER_SIZE_INT opnd4, POINTER_SIZE_INT opnd5)
{
    ManagedObjectDummy* obj = (ManagedObjectDummy*)opnd5;

    if (opnd1 % 65536 == 22 && obj->alloc_count_for_hashcode == 405)
    {
        printf(format, opnd1, obj->alloc_count_for_hashcode, opnd3, opnd4);
    }

    if (opnd1 % 65536 == 22 && opnd2 == 405)
    {
//        assert(0);
        printf(format, opnd1, opnd2, opnd3, opnd4);

/*
        printf("length : %d\n", ((VM_VectorDummy*)opnd5)->length);

        uint16* start = (uint16*)((char*)opnd5 + VM_VECTOR_FIRST_ELEM_OFFSET_1_2_4);
        for (int i = 0 ; i < ((VM_VectorDummy*)opnd5)->length ; i ++)
        {
             printf("%c", ((char)*start) & 0xFF );
             start++;
        }
        printf("\n");
*/
    }
}



Inst* ORDERRep::genConditionalPrintOpndInst(const char * format, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3, Opnd * opnd4, Opnd* opnd5)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd1, opnd2, opnd3, opnd4, opnd5};
    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)conditional_print5));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}



void ORDERRep::appendCmpJmp(Node* compare_node, Node* ap_update_node, Node* follow_node, Opnd* tls_current)
{
    assert(parentObj);

    Opnd* offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::counter_offset());
    Opnd* cnt_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_addr, parentObj, offset);

    Opnd* cnt = irm->newMemOpnd(typeInt32, cnt_addr);
    Opnd* zero = irm->newImmOpnd(typeInt32, 0);
    Inst*com_tls_inst = irm->newInst(Mnemonic_CMP, zero, cnt);


    Inst* jmp_com_node = irm->newBranchInst(Mnemonic_JE, ap_update_node, follow_node);

    compare_node->appendInst(get_cnt_addr_inst);
    compare_node->appendInst(com_tls_inst);
    compare_node->appendInst(jmp_com_node);



}



void ORDERRep::appendUpdateInst(Node * ap_update_node, Opnd* obj, Node* unlock_sleep_node, Node* follow_node)
{

/*
#ifdef ORDER_DEBUG
    struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
    Opnd* c_name = irm->newImmOpnd(typeInt32, 
          (POINTER_SIZE_INT)classHandle->m_name->bytes);

    Opnd* m_name = irm->newImmOpnd(typeInt32, 
          (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

    Opnd* m_sig = irm->newImmOpnd(typeInt32, 
          (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());



    Inst* print_inst = genPrintOpndInst(format4, c_name, m_name, m_sig);
    ap_update_node->appendInst(print_inst);
#endif
*/





    void * replay_access_info_method = VMInterface::getReplayAccessInfoMethod();
    assert(replay_access_info_method);
    Opnd* args[] = {obj};
    Opnd* ret = irm->newOpnd(typeUInt8);
    Opnd* update_ap_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)replay_access_info_method));
    Inst* inst = irm->newCallInst(update_ap_method, &CallingConvention_CDECL, lengthof(args), args, ret);

    inst->setBCOffset(last_bc_offset);

    ap_update_node->appendInst(inst);


    Opnd* zero = irm->newImmOpnd(typeUInt8, 0);
    Inst*com_ret_inst = irm->newInst(Mnemonic_CMP, zero, ret);
    com_ret_inst->setBCOffset(last_bc_offset);

    Inst* jmp_com_node = irm->newBranchInst(Mnemonic_JE, unlock_sleep_node, follow_node);
    jmp_com_node->setBCOffset(last_bc_offset);

    ap_update_node->appendInst(com_ret_inst);
    ap_update_node->appendInst(jmp_com_node);



}



void ORDERRep::appendResetTLSCntInst(Node * ap_update_node, Opnd* obj, Opnd* tls_current)
{
    //set TLS to current thread id

    Opnd* tls_offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::tls_offset());
    Opnd* tls_record_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_tls_record_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tls_record_addr, obj, tls_offset);

    Opnd* tls_record = irm->newMemOpnd(typeInt32, tls_record_addr);
    Inst* set_tls_record_inst = irm->newCopyPseudoInst(Mnemonic_MOV, tls_record, tls_current);

    //set count to zero
    Opnd* cnt_offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::counter_offset());
    Opnd* cnt_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_addr, obj, cnt_offset);

    Opnd* cnt = irm->newMemOpnd(typeInt32, cnt_addr);
    Opnd* zero = irm->newImmOpnd(typeInt32, 0);
    Inst* set_cnt_inst = irm->newCopyPseudoInst(Mnemonic_MOV, cnt, zero);

/*
    //yzm
    //1 for debug
    Opnd* tls_before = irm->newMemOpnd(typeInt32, tls_record_addr);
    Inst* debug_print_tls_old = genPrintOpndInst(format2, tls_before);
    Opnd* cnt_before = irm->newMemOpnd(typeInt32, cnt_addr);
    Inst* debug_print_cnt_old = genPrintOpndInst(format3, cnt_before);
    Inst* debug_print_tls_new = genPrintOpndInst(format4, tls_current);
*/

    //append instructions
    ap_update_node->appendInst(get_tls_record_addr_inst);

/*
    //yzm
    //1 for debug
    node->appendInst(debug_print_tls_old);
*/

    ap_update_node->appendInst(set_tls_record_inst);

    ap_update_node->appendInst(get_cnt_addr_inst);

/*
    //yzm
    //1 for debug
    node->appendInst(debug_print_cnt_old);
    node->appendInst(debug_print_tls_new);
*/

    ap_update_node->appendInst(set_cnt_inst);

#ifdef ORDER_DEBUG
#ifdef ORDER_PER_EVA
    {
        MethodDesc* desc = getCompilationContext()->getVMCompilationInterface()->getMethodToCompile();
        MethodDummy *method = (MethodDummy*)desc->getMethodHandle();
        int64 ptr = (int64)&method->interleaving;

        Opnd* cnt_addr = irm->newImmOpnd(typeInt32Ptr, ptr);

        Opnd* cnt_orig = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr); 
        Opnd* cnt_res = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr);
        Opnd* one = irm->newImmOpnd(typeInt32, 1);
        Opnd* cnt_new_val = irm->newOpnd(typeInt32);
    
        Inst* increase_cnt_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_new_val, one, cnt_orig);
        Inst* save_cnt_res_inst = irm->newCopyPseudoInst(Mnemonic_MOV, cnt_res, cnt_new_val);

        ap_update_node->appendInst(increase_cnt_inst);
        ap_update_node->appendInst(save_cnt_res_inst);
    }
#endif
#endif


}

void ORDERRep::appendLockAndJmp(Node* cas_node, Node* follow_node, Node* fat_lock_node, Opnd* lock_addr)
{

//1CMPXCHG instruction can be further optimized!!!!!!!!!!!!
    Opnd* zero = irm->newImmOpnd(typeInt8, 0);
    Opnd* one = irm->newImmOpnd(typeInt8, 1);
    Opnd* new_val = irm->newOpnd(typeInt8);
    Opnd* al = irm->newOpnd(typeInt8, Constraint(RegName_AL));

    Inst* set_al_inst = irManager->newCopyPseudoInst(Mnemonic_MOV, al, zero);
    Inst* set_new_val_inst = irManager->newCopyPseudoInst(Mnemonic_MOV, new_val, one);

    Opnd* lock_old = irm->newMemOpnd(typeInt8, lock_addr);
    Inst* cmpxchg = irManager->newInst(Mnemonic_CMPXCHG, lock_old, new_val, al);
    cmpxchg->setPrefix(InstPrefix_LOCK);

    Inst* jmp_inst = irm->newBranchInst(Mnemonic_JZ, follow_node, fat_lock_node);

    cas_node->appendInst(set_al_inst);
    cas_node->appendInst(set_new_val_inst);
    cas_node->appendInst(cmpxchg);
    cas_node->appendInst(jmp_inst);
}


void ORDERRep::appendUnlock(Opnd* lock_addr, Node* follow_node)
{
    Opnd* zero = irm->newImmOpnd(typeInt8, 0);
    Opnd* obj_lock = irm->newMemOpnd(typeInt8, lock_addr);
    Inst* clear_inst = irManager->newCopyPseudoInst(Mnemonic_MOV, obj_lock, zero);

    follow_node->appendInst(clear_inst);
}


/*
//1Used for debug!!!!!!!!!!!!!!!!!!!!!!!
void ORDERRep::appendTlsTidMappingInst(Node* ap_update_node)
{
    Opnd* tls_current = irm->newOpnd(typeInt32Ptr);

    //2Calculate TLS
    Opnd* tm_self_tls_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)VMInterface::getTIDAddress()));
    Inst* get_tls_inst = irm->newCallInst(tm_self_tls_addr, &CallingConvention_CDECL, 0, NULL, tls_current);
    get_tls_inst->setBCOffset(last_bc_offset);


    Opnd* offset = irm->newImmOpnd(typeInt32, VMInterface::getTIDOffset());
    Opnd* tid_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_tid_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tid_addr, tls_current, offset);
    get_tid_addr_inst->setBCOffset(last_bc_offset);

    Opnd* tid = irm->newMemOpnd(typeInt32, tid_addr);
    Inst* printTLSTIDInst = genPrintOpndInst(format1, tls_current, tid);
    printTLSTIDInst->setBCOffset(last_bc_offset);

    ap_update_node->appendInst(get_tls_inst);
    ap_update_node->appendInst(get_tid_addr_inst);
    ap_update_node->appendInst(printTLSTIDInst);

}
*/

#include "port_atomic.h"
#include <unistd.h>
void ORDERRep::try_lock_obj(ManagedObjectDummy* p_obj)
{
    for (int i = 0 ; i < 5 ; i ++)
    {
        if (port_atomic_cas8(&p_obj->access_lock, 1, 0) == 0)
        {
            return;
        }
    }
//    for (int i = 0 ; i < 1000 ; i ++)
    while(true)
    {
        usleep(100);
#ifdef ORDER_DEBUG
        printf("usleep in try lock obj\n");
#endif
        if (port_atomic_cas8(&p_obj->access_lock, 1, 0) == 0)
        {
            return;
        }
    }
}


#include <open/hythread_ext.h>
void ORDERRep::usleep_debug(ManagedObjectDummy* p_obj, char* m_class_name, char* m_name, char* m_sig)
{

#ifdef ORDER_DEBUG
      printf("usleep in Ia32OrderReplayInstrumentation.cpp : (%d, %d, %d, t:%d), %d, <%s, %s, %s>!!!\n", 
		p_obj->alloc_tid, p_obj->alloc_count, p_obj->access_count, (int)p_obj->access_tid, (int)hythread_self()->thread_id, 
		m_class_name, m_name, m_sig);
#endif

    if (hythread_self()->request != 0) printf("request to lock thread : %d\n", (int)hythread_self()->thread_id);
//    printf("suspend enter, thread: %d, suspend_count: %d, request: %d, disable_count L %d\n", hythread_self()->thread_id,
//          hythread_self()->suspend_count, hythread_self()->request, hythread_self()->disable_count);
//    hythread_safe_point();
//    hythread_exception_safe_point();
    usleep(1000);
}

void ORDERRep::assert_order(ManagedObjectDummy* p_obj, char* m_class_name, char* m_name, char* m_sig)
{
    assert(0);
}


void ORDERRep::usleep_try_lock_obj_dummy(ManagedObjectDummy* p_obj)
{
#ifdef ORDER_DEBUG
    printf("usleep in Ia32OrderReplayInstrumentation.cpp: [alloc_tid: %d, alloc_count: %d], %d\n",
    	p_obj->alloc_tid, p_obj->alloc_count, (int)hythread_self()->thread_id);
#endif
//    hythread_safe_point();
//    hythread_exception_safe_point();
    usleep(100);
}

U_32 ORDERRep::special_lock(ManagedObjectDummy* p_obj)
{

#ifdef ORDER_DEBUG
    if (p_obj->alloc_tid == 0 || p_obj->alloc_tid % 0xffff >= ORDER_THREAD_NUM || p_obj->alloc_count == 0)
    {
        assert(0);
    }
#endif

        if (p_obj->alloc_count != (U_32)-1)
        {

            if (port_atomic_cas8(&p_obj->access_lock, 1, 0) != 0)
            {
#ifdef ORDER_DEBUG
                printf("usleep in monitor enter, (%d, %d), %d\n", p_obj->alloc_tid, p_obj->alloc_count, (int)hythread_self()->thread_id);
                if (hythread_self()->request != 0) printf("request to lock thread : %d\n", (int)hythread_self()->thread_id);
#endif

                return -1;
            }

            if (p_obj->access_count == 0){
                typedef U_8(*func_type)(ManagedObjectDummy *); 
                func_type func_val = (func_type)VMInterface::getReplayAccessInfoMethod();
                U_8 result = func_val(p_obj);
	       if (result == 0){
                    p_obj->access_lock = 0;
                    return -1;
	       }
            }
            else{
                if(p_obj->access_tid != (U_32)pthread_self()){
#ifdef ORDER_DEBUG
                    printf("usleep in monitor enter, tid not match, (%d, %d), current : %d, expected : %d, remaining : %d\n", p_obj->alloc_tid, p_obj->alloc_count, (int)hythread_self()->thread_id, p_obj->access_tid, p_obj->access_count);
                    assert(p_obj->alloc_tid != 0);
                    if (hythread_self()->request != 0) printf("request to lock thread : %d\n", (int)hythread_self()->thread_id);
#endif
                    p_obj->access_lock = 0;

                    return -1;
                }
            }    

            p_obj->access_count --;
#ifdef REPLAY_OPTIMIZE
			p_obj->order_padding |= OBJ_RW_BIT; //write
#endif
        }
        else
        {
            return 0;
            //assert(0);
        }

    return 0;
}



void ORDERRep::appendWait(Node* wait_node, Node* true_branch, Node* false_branch, Opnd* tls_current, Opnd* obj)
{
    Opnd* tls_offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::tls_offset());
    Opnd* tls_record_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_tls_record_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tls_record_addr, obj, tls_offset);


//1CMPXCHG instruction can be further optimized!!!!!!!!!!!!
    Opnd* new_val = irm->newOpnd(typeInt32);
    Opnd* al = irm->newOpnd(typeInt32, Constraint(RegName_EAX));

    Inst* set_al_inst = irManager->newCopyPseudoInst(Mnemonic_MOV, al, tls_current);
    Inst* set_new_val_inst = irManager->newCopyPseudoInst(Mnemonic_MOV, new_val, tls_current);

    Opnd* tls = irm->newMemOpnd(typeInt32, tls_record_addr);
    Inst* cmpxchg = irManager->newInst(Mnemonic_CMPXCHG, tls, new_val, al);
    cmpxchg->setPrefix(InstPrefix_LOCK);

    Inst* jmp_inst = irm->newBranchInst(Mnemonic_JZ, true_branch, false_branch);

    wait_node->appendInst(get_tls_record_addr_inst);
    wait_node->appendInst(set_al_inst);
    wait_node->appendInst(set_new_val_inst);
    wait_node->appendInst(cmpxchg);
    wait_node->appendInst(jmp_inst);
}

#ifdef BYPASS_INSTRUMENT

Opnd *ORDERRep::getNewOpnd(Opnd *original_opnd)
{
	Opnd *new_opnd = NULL;
	if(original_opnd->getMemOpndKind() != MemOpndKind_Null){

		//assert(original_opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Base));
		new_opnd = irm->newMemOpnd(original_opnd->getType(), 
					original_opnd->getMemOpndKind(),
					original_opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Base), 
					original_opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Index), 
					original_opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Scale), 
					original_opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement),
					original_opnd->getSegReg());
	}
	else{
		new_opnd = original_opnd;
	}

	return new_opnd;
}

Inst *ORDERRep::cloneInst(Inst *original_inst)
{
	Inst *new_inst = NULL;

	switch(original_inst->getKind()){
		case Inst::Kind_CopyPseudoInst:
		{
			
			if(original_inst->getMnemonic() == Mnemonic_MOV){
				assert(original_inst->getOpndCount() == 2);
				Opnd *opnd0 = getNewOpnd(original_inst->getOpnd(0));
				Opnd *opnd1 = getNewOpnd(original_inst->getOpnd(1));
				new_inst = irm->newCopyPseudoInst(original_inst->getMnemonic(), opnd0, opnd1);
			}
			else{
				assert(0);
			}
			break;
		}
		
		case Inst::Kind_BranchInst:
		{
			assert(original_inst->getOpnd(1) == irm->getRegOpnd(RegName_EFLAGS));
			BranchInst *branch_inst = (BranchInst *)original_inst;
			new_inst = irm->newBranchInst(original_inst->getMnemonic(), branch_inst->getTrueTarget(), 
											branch_inst->getFalseTarget());

			break;
		}
		
		case Inst::Kind_Inst:
		{
			if(original_inst->getMnemonic() == Mnemonic_CMP || original_inst->getMnemonic() == Mnemonic_UCOMISD ||
				original_inst->getMnemonic() == Mnemonic_UCOMISS){
				assert(original_inst->getOpnd(0) == irm->getRegOpnd(RegName_EFLAGS));
				new_inst = irm->newInst(original_inst->getMnemonic(), getNewOpnd(original_inst->getOpnd(1)),
					getNewOpnd(original_inst->getOpnd(2)));
			}
			else if(original_inst->getMnemonic() == Mnemonic_ADD || original_inst->getMnemonic() == Mnemonic_SUB ||
				original_inst->getMnemonic() == Mnemonic_AND || original_inst->getMnemonic() == Mnemonic_XOR ||
				original_inst->getMnemonic() == Mnemonic_OR || original_inst->getMnemonic() == Mnemonic_IMUL){
				assert(original_inst->getForm() == Inst::Form_Extended);
				assert(original_inst->getOpnd(1) == irm->getRegOpnd(RegName_EFLAGS));
				new_inst = irm->newInstEx(original_inst->getMnemonic(), 1, 
						getNewOpnd(original_inst->getOpnd(0)), 
						getNewOpnd(original_inst->getOpnd(2)), getNewOpnd(original_inst->getOpnd(3)));
			}
			else if(original_inst->getMnemonic() == Mnemonic_NEG){
				assert(original_inst->getForm() == Inst::Form_Extended);
				assert(original_inst->getOpnd(1) == irm->getRegOpnd(RegName_EFLAGS));
				new_inst = irm->newInstEx(original_inst->getMnemonic(), 1, 
						getNewOpnd(original_inst->getOpnd(0)), 
						getNewOpnd(original_inst->getOpnd(2)));
			}
			else if(original_inst->getMnemonic() == Mnemonic_MOVZX || original_inst->getMnemonic() == Mnemonic_MOVSX){
				new_inst = irm->newInstEx(original_inst->getMnemonic(), 1, 
						getNewOpnd(original_inst->getOpnd(0)), 
						getNewOpnd(original_inst->getOpnd(1)));
				
			}
			else if(original_inst->getMnemonic() == Mnemonic_MOV ||original_inst->getMnemonic() == Mnemonic_CVTSI2SD || 
				original_inst->getMnemonic() == Mnemonic_CVTSI2SS || original_inst->getMnemonic() == Mnemonic_FLD){
				assert(original_inst->getOpndCount() == 2);
				Opnd *opnd0 = getNewOpnd(original_inst->getOpnd(0));
				Opnd *opnd1 = getNewOpnd(original_inst->getOpnd(1));
				new_inst = irm->newInst(original_inst->getMnemonic(), opnd0, opnd1);
			}
			else if(original_inst->getMnemonic() == Mnemonic_MULSS ||original_inst->getMnemonic() == Mnemonic_MULSD 
				|| original_inst->getMnemonic() == Mnemonic_DIVSD  || original_inst->getMnemonic() == Mnemonic_DIVSS
				|| original_inst->getMnemonic() == Mnemonic_SUBSD || original_inst->getMnemonic() == Mnemonic_SUBSS
				|| original_inst->getMnemonic() == Mnemonic_ADDSD || original_inst->getMnemonic() == Mnemonic_ADDSS){
				new_inst = irm->newInstEx(original_inst->getMnemonic(), 1, 
						getNewOpnd(original_inst->getOpnd(0)), 
						getNewOpnd(original_inst->getOpnd(1)), getNewOpnd(original_inst->getOpnd(2)));
			}
			else{
				assert(0);
			}
			
			
			break;
		}

		case Inst::Kind_CallInst:
		{
			CallInst *call_inst =(CallInst *)original_inst;
			if(call_inst->isDirect()){
				Opnd * targetOpnd=call_inst->getOpnd(call_inst->getTargetOpndIndex());
                                    assert(targetOpnd->isPlacedIn(OpndKind_Imm));
                                    Opnd::RuntimeInfo * ri=targetOpnd->getRuntimeInfo();
                                    if( ri && ri->getKind() == Opnd::RuntimeInfo::Kind_HelperAddress ){
                                        if((void *)VM_RT_INSTANCEOF == ri->getValue(0) ){
                                            Opnd * args[]={ getNewOpnd(call_inst->getOpnd(2)), getNewOpnd(call_inst->getOpnd(3)) };
                                            new_inst = irm->newCallInst(targetOpnd, call_inst->getCallingConventionClient().getCallingConvention(), 
												2, args, getNewOpnd(call_inst->getOpnd(0)));
                                        }
                                    }
			}
			else{
				assert(0);
			}

			break;
		}
		
		default:
			assert(0);
	}

	return new_inst;
}
#endif

bool ORDERRep::inst_is_InstnceOf(Inst *inst)
{
    //Inst::Kind inst_kind = inst->getKind();
    if(inst->hasKind(Inst::Kind_CallInst) && ((CallInst*)inst)->isDirect() ){
        CallInst* callInst = (CallInst*)inst;
        Opnd * targetOpnd=callInst->getOpnd(callInst->getTargetOpndIndex());
        assert(targetOpnd->isPlacedIn(OpndKind_Imm));
        Opnd::RuntimeInfo * ri=targetOpnd->getRuntimeInfo();
        if( ri && ri->getKind() == Opnd::RuntimeInfo::Kind_HelperAddress ){
            if((void *)VM_RT_INSTANCEOF == ri->getValue(0)){
                return TRUE;
            }
        }
    }

    return FALSE;
}

bool ORDERRep::inst_is_InstnceOfWithResolve(Inst *inst)
{
    //Inst::Kind inst_kind = inst->getKind();
    if(inst->hasKind(Inst::Kind_CallInst) && ((CallInst*)inst)->isDirect() ){
        CallInst* callInst = (CallInst*)inst;
        Opnd * targetOpnd=callInst->getOpnd(callInst->getTargetOpndIndex());
        assert(targetOpnd->isPlacedIn(OpndKind_Imm));
        Opnd::RuntimeInfo * ri=targetOpnd->getRuntimeInfo();
        if( ri && ri->getKind() == Opnd::RuntimeInfo::Kind_HelperAddress ){
            if((void *)VM_RT_INSTANCEOF_WITHRESOLVE== ri->getValue(0)){
                return TRUE;
            }
        }
    }

    return FALSE;
}

bool ORDERRep::inst_is_CheckCast(Inst *inst)
{
    //Inst::Kind inst_kind = inst->getKind();
    if(inst->hasKind(Inst::Kind_CallInst) && ((CallInst*)inst)->isDirect() ){
        CallInst* callInst = (CallInst*)inst;
        Opnd * targetOpnd=callInst->getOpnd(callInst->getTargetOpndIndex());
        assert(targetOpnd->isPlacedIn(OpndKind_Imm));
        Opnd::RuntimeInfo * ri=targetOpnd->getRuntimeInfo();
        if( ri && ri->getKind() == Opnd::RuntimeInfo::Kind_HelperAddress ){
            if((void *)VM_RT_CHECKCAST == ri->getValue(0)){
                return TRUE;
            }
        }
    }

    return FALSE;
}

//_________________________________________________________________________________________________
void ORDERRep::runImpl()
{
    struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
#ifdef ORDER_DEBUG
//	struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
	printf("Class Name : %s\n", (char*)classHandle->m_name->bytes);
    printf("Method Name : %s\n", getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());
    printf("Method Sig : %s\n", getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());
#endif
//    struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
//    printf("Method Sig : %s\n", classHandle->m_name->bytes);

    bool use_cas = true;
    if (strcmp(getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(),
		"writeOptionalDataToBuffer") == 0)
    {
		use_cas = false;
		printf("method bypass use_cas.\n");
    }

//		bool use_cas = false;






    irm = getCompilationContext()->getLIRManager();
    typeUInt8 = irm->getTypeManager().getPrimitiveType(Type::UInt8);
    typeInt8 = irm->getTypeManager().getPrimitiveType(Type::Int8);
    typeInt16 = irm->getTypeManager().getPrimitiveType(Type::Int16);
    typeInt32 = irm->getTypeManager().getPrimitiveType(Type::Int32);
    typeUInt32 = irm->getTypeManager().getPrimitiveType(Type::UInt32);
    typeInt8Ptr = irm->getManagedPtrType(typeInt8);
    typeInt16Ptr = irm->getManagedPtrType(typeInt16);
    typeInt32Ptr = irm->getManagedPtrType(typeInt32);

#ifdef _EM64T_
    typeIntPort = irm->getTypeManager().getPrimitiveType(Type::Int64);
#else
    typeIntPort = irm->getTypeManager().getPrimitiveType(Type::Int32);
#endif
    typeIntPtrPort = irm->getManagedPtrType(typeIntPort);

    irm->updateLivenessInfo();
    irm->calculateOpndStatistics();

#if !defined(DISABLE_PTHREAD_OPT)
    Opnd* tid_global = NULL;
    if (irm->pthread_opt_enable)
    {
        printf("pthread opt enable in instrument.\n");
        Node* entry = irm->getFlowGraph()->getEntryNode();

        Node* pthread_node = irm->getFlowGraph()->createBlockNode();

        Edges out_edges = entry->getOutEdges();
        for (unsigned int i = 0 ; i < out_edges.size() ; i ++)
        {
             Edge* old_edge = out_edges[i];
             irm->getFlowGraph()->addEdge(pthread_node, old_edge->getTargetNode(), old_edge->getEdgeProb());

             irm->getFlowGraph()->removeEdge(entry, old_edge->getTargetNode());
        }
        irm->getFlowGraph()->addEdge(entry, pthread_node);

        tid_global = irm->newOpnd(typeInt32);
        Opnd* tm_self_tls_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)pthread_self));
        Inst* get_tls_inst = irm->newCallInst(tm_self_tls_addr, &CallingConvention_CDECL, 0, NULL, tid_global);

        get_tls_inst->setBCOffset(last_bc_offset);
        pthread_node->appendInst(get_tls_inst);
    }
#endif

    const Nodes& nodes = irm->getFlowGraph()->getNodesPostOrder();
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
        Node* node = *it;

        if (node->isBlockNode()){

            for (Inst * inst=(Inst*)node->getLastInst(); inst!=NULL; inst=inst->getPrevInst()){

                int isWrite = 0;
                parentObj = NULL;

                if (inst->getParentClassLoad() || inst->getParentClassStore())
                {
#if (defined(ORDER) && defined(RECORD_STATIC))
                    ClassDummy* parentClz = NULL;
                    if (inst->getParentClassLoad())
                        parentClz = (ClassDummy*)inst->getParentClassLoad();
                    else
                    {
                        parentClz = (ClassDummy*)inst->getParentClassStore();
                        isWrite = 1;
                    }

                    U_32 class_handle_offset_val = ClassDummy::class_handle_offset(parentClz);

/*
                    ManagedObjectDummy* tempObj = **(ManagedObjectDummy***)((POINTER_SIZE_INT)parentClz + class_handle_offset_val);
                    printf("ORDER instrument static variable for class: %s\n", parentClz->m_name->bytes);
                    printf("class_handle_offset_val : %d\n", class_handle_offset_val);
                    printf("static, class obj : (%d, %d)\n", tempObj->alloc_tid, tempObj->alloc_count);
                    printf("real address : %d\n", (POINTER_SIZE_INT)&parentClz->m_class_handle);
                    printf("real obj : %d\n", (POINTER_SIZE_INT)*(ManagedObjectDummy**)parentClz->m_class_handle);
                    printf("parentClz : %d\n", (POINTER_SIZE_INT)parentClz);
                    printf("class_handle_offset_val : %d\n", class_handle_offset_val);
                    printf("tempObj : %d\n", (POINTER_SIZE_INT)tempObj);
*/

                    Opnd* parent_class = irm->newImmOpnd(typeIntPort, (POINTER_SIZE_INT)parentClz);
                    Opnd* class_handle_offset = irm->newImmOpnd(typeIntPort, class_handle_offset_val);
                    Opnd* parent_obj_address = irm->newOpnd(typeIntPtrPort);
                    Opnd* parent_obj_address_address = irm->newOpnd(typeIntPtrPort);
                    Inst* get_class_handle_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, parent_obj_address_address, parent_class, class_handle_offset);
                    get_class_handle_addr_inst->setBCOffset(last_bc_offset);
                    get_class_handle_addr_inst->insertBefore(inst);

                    Opnd* parent_obj_address_value = irm->newMemOpnd(typeIntPtrPort, parent_obj_address_address);
                    Inst* load_parent_obj_address_inst = irm->newCopyPseudoInst(Mnemonic_MOV, parent_obj_address, parent_obj_address_value);
                    load_parent_obj_address_inst->setBCOffset(last_bc_offset);
                    load_parent_obj_address_inst->insertBefore(inst);

                    Opnd* parent_obj_in_memory = irm->newMemOpnd(typeIntPtrPort, parent_obj_address);
                    parentObj = irm->newOpnd(typeIntPtrPort);
                    Inst* load_parent_obj_inst = irm->newCopyPseudoInst(Mnemonic_MOV, parentObj, parent_obj_in_memory);
                    load_parent_obj_inst->setBCOffset(last_bc_offset);
                    load_parent_obj_inst->insertBefore(inst);

                    assert(parentObj);
#endif
                }
/*
                else if (
                    (inst->getParentObjectLoad() || inst->getParentObjectStore()) &&
                    (inst->getMnemonic() == Mnemonic_CMP ||
                    inst->getMnemonic() == Mnemonic_CMPXCHG ||
                    inst->getMnemonic() == Mnemonic_CMPXCHG8B ||
                    inst->getMnemonic() == Mnemonic_CMPSB ||
                    inst->getMnemonic() == Mnemonic_CMPSW ||
                    inst->getMnemonic() == Mnemonic_CMPSD))
                {
                    printf("Unhandled: instuction has side effect!\n");
                }

*/
                else if (inst->getParentObjectLoad() || inst->getParentObjectStore())
                {
                       assert(!inst->getParentClassLoad() && !inst->getParentClassStore());
                       if (inst->getParentObjectLoad())
                           parentObj = inst->getParentObjectLoad();
                       else
                       {
                           parentObj = inst->getParentObjectStore();
                           isWrite = 1;
                       }

                       assert(parentObj);
                }
/*
                else if(inst_is_InstnceOf(inst)){
                    parentObj = inst->getOpnd(((CallInst *)inst)->getTargetOpndIndex() + 1);

                    printf("[INSTNCEOF]: Meeting an instanceof inst, parentObj %ld!!!\n", (long)parentObj);
                    assert(((CallInst *)inst)->getTargetOpndIndex() == 1);
                    assert(parentObj);

                }
                else if(inst_is_InstnceOfWithResolve(inst)){
                    parentObj = inst->getOpnd(((CallInst *)inst)->getTargetOpndIndex() + 3);

                    printf("[INSTANCEOF_WITHRESOLVE]: Meeting an INSTANCEOF_WITHRESOLVE inst  in (%s, %s, %s)!!!\n",
		  	getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(),
		  	getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString(),
		  	classHandle->m_name->bytes);
                    assert(((CallInst *)inst)->getTargetOpndIndex() == 1);
                    assert(parentObj);
                }

*/
                if (parentObj && !use_cas)
                {

                        Opnd* lock_addr = irm->newOpnd(typeInt8Ptr);
                        //1Preparation
                        {
                            //2Calculate Lock Address
                            Opnd* lock_offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::lock_offset());
                            Inst* get_lock_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, lock_addr, parentObj, lock_offset);
                            get_lock_addr_inst->insertBefore(inst);
                        }


                        //2Construct BBs
                        Node* follow_node;
                        Node* cas_node;
                        Node* fat_lock_node;
                        //  node     ->        cas_node     ->            follow_node
                        //               |                             |
                        //               |-> fat_lock_node -> 

                        {
                            cas_node = irm->getFlowGraph()->createBlockNode();
                            fat_lock_node = irm->getFlowGraph()->createBlockNode();
                            follow_node = irm->getFlowGraph()->createBlockNode();

                            follow_node->setExecCount(node->getExecCount());


                            //link follow_node to node's outs
                            //unlink node's out edges
                            Edges old_out_edges = node->getOutEdges();
                            for (unsigned int i = 0 ; i < old_out_edges.size() ; i ++)
                            {
                                Edge* old_edge = old_out_edges[i];
                                irm->getFlowGraph()->addEdge(follow_node, old_edge->getTargetNode(), old_edge->getEdgeProb());

                                irm->getFlowGraph()->removeEdge(node, old_edge->getTargetNode());
                            }

                            //link node to cas_node
                            irm->getFlowGraph()->addEdge(node, cas_node);

                            irm->getFlowGraph()->addEdge(cas_node, fat_lock_node);
                            irm->getFlowGraph()->addEdge(fat_lock_node, cas_node);

                            //1For debug : #10062801
                            irm->getFlowGraph()->addEdge(cas_node, follow_node);
                        }


                        //2Construct: Follow Node
                        {
                            Inst* inst_next = (Inst*)inst->next();

                                                    //2Original Access Statement
                            inst->unlink();
                            follow_node->appendInst(inst);

                            //2Synchronize : Unlock
                            appendUnlock(lock_addr, follow_node);

                            //2Original Statements after Access Operation
                            while(inst_next)
                            {
                                Inst* inst = inst_next;
                                inst_next = (Inst*)inst_next->next();
                                inst->unlink();
                                follow_node->appendInst(inst);
                            }
                        }


                        //1Fat Lock Node
                        {
                            fat_lock_node->appendInst(irManager->newRuntimeHelperCallInst(
                               VM_RT_GC_SAFE_POINT, 0, NULL, NULL)
                               );

						
                            Opnd* args[] = { parentObj };

                            Opnd* fat_lock_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)usleep_try_lock_obj_dummy));
//                         Opnd* fat_lock_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)try_lock_obj));
                            Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, lengthof(args), args, NULL);
//                         Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, 1, &parentObj, NULL);
                            fat_lock_inst->setBCOffset(last_bc_offset);
                            fat_lock_node->appendInst(fat_lock_inst);
                        }
					


                        //1Cas Node
                        {
                            Opnd* args[] = { parentObj };
                            Opnd* ret_val = irm->newOpnd(typeUInt32);

                            Opnd* lock_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)special_lock));
//                         Opnd* fat_lock_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)try_lock_obj));
                            Inst* lock_inst = irm->newCallInst(lock_method_addr, &CallingConvention_CDECL, lengthof(args), args, ret_val);
//                         Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, 1, &parentObj, NULL);

                            Opnd* zero = irm->newImmOpnd(typeUInt32, 0);
                            Inst* cmp = irManager->newInst(Mnemonic_CMP, ret_val, zero);
                            Inst* jmp_inst = irm->newBranchInst(Mnemonic_JZ, follow_node, fat_lock_node);

                            lock_inst->setBCOffset(last_bc_offset);
                            cas_node->appendInst(lock_inst);
                            cmp->setBCOffset(last_bc_offset);
                            cas_node->appendInst(cmp);
                            jmp_inst->setBCOffset(last_bc_offset);
                            cas_node->appendInst(jmp_inst);
                        }

                    if (inst->getParentObjectLoad())
                        inst->clearParentObjectLoad(); //Mark this instruction as processed
                    else if (inst->getParentObjectStore())
                        inst->clearParentObjectStore(); //Mark this instruction as processed
                    else if (inst->getParentClassLoad())
                        inst->clearParentClassLoad(); //Mark this instruction as processed
                    else if (inst->getParentClassStore())
                        inst->clearParentClassStore(); //Mark this instruction as processed
                    else
                        assert(0);

                    inst = (Inst*)node->getLastInst();
                    end = nodes.end();


                }

                else if (parentObj)
                {
                
#ifdef BYPASS_INSTRUMENT
/********************************* bypass the instrumentation code if the object is thread-local / assigned-once ***********************************************/
		bool merge_follow_node = FALSE;
		if(inst->getMnemonic() == Mnemonic_CMP || inst->getMnemonic() == Mnemonic_UCOMISD || 
				inst->getMnemonic() == Mnemonic_UCOMISS)
		{
		    //assert(inst->getNextInst()->hasKind(Inst::Kind_BranchInst));
		    Inst *next_inst = inst->getNextInst();
		    if(next_inst && next_inst->hasKind(Inst::Kind_BranchInst)){
		        merge_follow_node = TRUE;
		    }
		}

                       if (inst->getBCOffset() != ILLEGAL_BC_MAPPING_VALUE)
                            last_bc_offset = inst->getBCOffset();

                     Opnd* lock_addr = irm->newOpnd(typeInt8Ptr);

                     Opnd* current_tid = NULL;
#if !defined(DISABLE_PTHREAD_OPT)
                     if (tid_global)
                     {
                     }
                     else
                     {
#endif
                         current_tid = irm->newOpnd(typeInt32);
#if !defined(DISABLE_PTHREAD_OPT)
                     }
#endif

                     //2Preparation
                    {
#if !defined(DISABLE_PTHREAD_OPT)
                        if (tid_global)
                        {
                            assert(irm->pthread_opt_enable);
                            current_tid = tid_global;
                        }
                        else
#endif
                        {
                            assert(!irm->pthread_opt_enable);
                            //2Calculate TLS
//                        Opnd* tls_addr = irm->newOpnd(typeInt32);
                            Opnd* tm_self_tls_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)pthread_self));
                            Inst* get_tls_inst = irm->newCallInst(tm_self_tls_addr, &CallingConvention_CDECL, 0, NULL, current_tid);
/*
                        Opnd* tid_offset = irm->newImmOpnd(typeInt32, ((POINTER_SIZE_INT)HyThreadDummy::tid_offset()));
                        Opnd* tid_addr = irm->newOpnd(typeInt32Ptr);
                        Inst* get_tid_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tid_addr, tls_addr, tid_offset);

                        Opnd* tid = irm->newMemOpnd(typeInt32, tid_addr);
                        Inst* get_tid_inst = irm->newCopyPseudoInst(Mnemonic_MOV, current_tid, tid);
*/
                            get_tls_inst->setBCOffset(last_bc_offset);
                            get_tls_inst->insertBefore(inst);
//                        get_tid_addr_inst->setBCOffset(last_bc_offset);
//                        get_tid_addr_inst->insertBefore(inst);
//                        get_tid_inst->setBCOffset(last_bc_offset);
//                        get_tid_inst->insertBefore(inst);
                        }

                        //2Calculate Lock Address
                        Opnd* lock_offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::lock_offset());
                        Inst* get_lock_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, lock_addr, parentObj, lock_offset);
                        get_lock_addr_inst->insertBefore(inst);

                    }

//1Remove Redundent Address Calculation!!!!!!!!!!!!!!!

                    //2Construct BBs
                    Node* ap_update_node;
                    Node* follow_node;
                    Node* cas_node;
                    Node* compare_node;
                    Node* fat_lock_node;
                    Node* wait_node;
                    Node* unlock_sleep_node;
                    Node* com_ignore_node;
		  //xlc
                    Node* access_only_node;
		  Node* access_unlock_node;
		  //end xlc
                    //Basic Blocks in ORDER record mode : 
                    //                                                                                                 -> ap_update_node ->       
                    //                                                                                               |                                |     
                    //  node -> com_ignore_node -> cas_node    ->     compare_node ------------------>  wait_node ->  follow_node
                    //                              |               |  |                                 |                                              |                        |
                    //                              |               |    ->   fat_lock_node  ->                                                |                        |
                    //                              |                <----------------unlock_sleep_node----------------                          |
                    //                                ----------------------------------------------------------------------->
		  {
                        ap_update_node = irm->getFlowGraph()->createBlockNode();
                        
                        cas_node = irm->getFlowGraph()->createBlockNode();
                        compare_node = irm->getFlowGraph()->createBlockNode();
                        fat_lock_node = irm->getFlowGraph()->createBlockNode();
                        wait_node = irm->getFlowGraph()->createBlockNode();
                        unlock_sleep_node = irm->getFlowGraph()->createBlockNode();
                        com_ignore_node = irm->getFlowGraph()->createBlockNode();
                        //xlc
                        access_only_node = irm->getFlowGraph()->createBlockNode();
                        access_unlock_node = irm->getFlowGraph()->createBlockNode();
                        //end xlc

                        compare_node->setExecCount(node->getExecCount());
                        
                        wait_node->setExecCount(node->getExecCount());

                        if(merge_follow_node)
                        {
                            Edges old_out_edges = node->getOutEdges();
                            for (unsigned int i = 0 ; i < old_out_edges.size() ; i ++)
                            {
                                Edge* old_edge = old_out_edges[i];
                                irm->getFlowGraph()->addEdge(access_unlock_node, old_edge->getTargetNode(), old_edge->getEdgeProb());
                                irm->getFlowGraph()->addEdge(access_only_node, old_edge->getTargetNode(), old_edge->getEdgeProb());
                                irm->getFlowGraph()->removeEdge(node, old_edge->getTargetNode());
                            }
                        }
                        else{
                            //link follow_node to node's outs
                            //unlink node's out edges
                            follow_node = irm->getFlowGraph()->createBlockNode();
                            follow_node->setExecCount(node->getExecCount());
							
                            Edges old_out_edges = node->getOutEdges();
                            for (unsigned int i = 0 ; i < old_out_edges.size() ; i ++)
                            {
                                Edge* old_edge = old_out_edges[i];
                                irm->getFlowGraph()->addEdge(follow_node, old_edge->getTargetNode(), old_edge->getEdgeProb());

                                irm->getFlowGraph()->removeEdge(node, old_edge->getTargetNode());
                            }
                            
                            irm->getFlowGraph()->addEdge(access_unlock_node, follow_node);
                            irm->getFlowGraph()->addEdge(access_only_node, follow_node);
                        }

                        //link node to cas_node
                        irm->getFlowGraph()->addEdge(node, com_ignore_node);

                        irm->getFlowGraph()->addEdge(com_ignore_node, cas_node);
                        irm->getFlowGraph()->addEdge(com_ignore_node, access_only_node);

                        irm->getFlowGraph()->addEdge(cas_node, fat_lock_node);
                        irm->getFlowGraph()->addEdge(cas_node, compare_node);

                        //1For debug : #10062801
                        irm->getFlowGraph()->addEdge(fat_lock_node, cas_node);
//                        irm->getFlowGraph()->addEdge(fat_lock_node, compare_node);

                        irm->getFlowGraph()->addEdge(compare_node, ap_update_node);
                        irm->getFlowGraph()->addEdge(compare_node, wait_node);

                        //link ap_update_node to follow_node
                        irm->getFlowGraph()->addEdge(ap_update_node, unlock_sleep_node);
                        irm->getFlowGraph()->addEdge(ap_update_node, access_unlock_node);

//                        irm->getFlowGraph()->addEdge(fat_lock_node, compare_node);

                        irm->getFlowGraph()->addEdge(wait_node, unlock_sleep_node);
                        irm->getFlowGraph()->addEdge(wait_node, access_unlock_node);

                        irm->getFlowGraph()->addEdge(unlock_sleep_node, com_ignore_node);

                    }



                    //2Construct: Follow Node
                    {
                        Inst* inst_next = (Inst*)inst->next();

#ifdef ORDER_DEBUG
#ifdef ORDER_PER_EVA
/*
                        {
                            MethodDesc* desc = getCompilationContext()->getVMCompilationInterface()->getMethodToCompile();
                            MethodDummy *method = (MethodDummy*)desc->getMethodHandle();
                            int64 ptr = (int64)&method->count;

                            Opnd* cnt_addr = irm->newImmOpnd(typeInt32Ptr, ptr);

                            Opnd* cnt_orig = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr); 
                            Opnd* cnt_res = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr);
                            Opnd* one = irm->newImmOpnd(typeInt32, 1);
                            Opnd* cnt_new_val = irm->newOpnd(typeInt32);
    
                            Inst* increase_cnt_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_new_val, one, cnt_orig);
                            Inst* save_cnt_res_inst = irm->newCopyPseudoInst(Mnemonic_MOV, cnt_res, cnt_new_val);

                            follow_node->appendInst(increase_cnt_inst);
                            follow_node->appendInst(save_cnt_res_inst);
                        }
 */
#endif
#endif

/*
#ifdef ORDER_DEBUG
//                        struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
//                        if (strcmp(getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(),"hashCode") == 0 && 
//					Type::isArray(parentObj->getType()->tag) &&
//					strcmp(classHandle->m_name->bytes,"java/lang/String") == 0 )
                        {
                            Opnd* offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_count_offset());
                            Opnd* cnt_addr = irm->newOpnd(typeInt32Ptr);
                            Inst* get_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_addr, parentObj, offset);
                            Opnd* cnt = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr); 

                            Opnd* offset_tid = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_tid_offset());
                            Opnd* tid_addr = irm->newOpnd(typeInt32Ptr);
                            Inst* get_tid_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tid_addr, parentObj, offset_tid);
                            Opnd* tid = irm->newMemOpnd(typeInt32, MemOpndKind_Any, tid_addr); 

                            Opnd* m_name = irm->newImmOpnd(typeIntPort, 
					(POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

//                            Opnd* m_sig = irm->newImmOpnd(typeIntPort, 
//					(POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());

				struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
				Opnd* m_class_name = irm->newImmOpnd(typeIntPort, (POINTER_SIZE_INT)classHandle->m_name->bytes);

                            Inst* print_inst = genConditionalPrintOpndInst(format1, tid, cnt, m_name, m_class_name, parentObj);

                            follow_node->appendInst(get_cnt_addr_inst);
                            follow_node->appendInst(get_tid_addr_inst);
                            follow_node->appendInst(print_inst);

                        }
#endif
*/

		      

                        //2Original Access Statement
                        inst->unlink();
						
                        //2clone the original access statement
                        Inst* access_inst_clone = cloneInst(inst);
                        //2end clone

/*
#ifdef REPLAY_OPTIMIZE
                        if(isWrite)
                        {
                            struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
                            Opnd* m_class_name = irm->newImmOpnd(typeIntPort, (POINTER_SIZE_INT)classHandle->m_name->bytes);

                            Opnd* m_name = irm->newImmOpnd(typeIntPort, 
                                (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

                            Opnd* m_sig = irm->newImmOpnd(typeIntPort, 
                                (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());

                            Opnd* args[] = { parentObj, m_class_name, m_name, m_sig};
                            Opnd* sleep_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)assert_order));
                            Inst* write_assert_inst = irm->newCallInst(sleep_method_addr, &CallingConvention_CDECL, lengthof(args), args, NULL);
                            write_assert_inst->setBCOffset(last_bc_offset);
                            access_only_node->appendInst(write_assert_inst);
                        }
#endif
*/
                        access_only_node->appendInst(access_inst_clone);
                        
	      

                         //2Count
                        //generate count instructions
                        appendCnt(access_unlock_node);
                        access_unlock_node->appendInst(inst);


                       if(inst_is_InstnceOf(inst)){

		         printf("[INSTNCEOF]: Meeting an instanceof inst in (%s, %s, %s)!!!\n",
		  	 getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(),
		  	 getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString(),
		  	 classHandle->m_name->bytes);
		  
                           assert(((CallInst *)inst)->getTargetOpndIndex() == 1);
                           //Opnd* arg1 = inst->getOpnd(((CallInst *)inst)->getTargetOpndIndex() + 1);


                           Opnd* offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_count_offset());
                           Opnd* cnt_addr = irm->newOpnd(typeInt32Ptr);
                           Inst* get_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_addr, parentObj, offset);
                           get_cnt_addr_inst->setBCOffset(last_bc_offset);
                           Opnd* cnt = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr); 

                           Opnd* offset_tid = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_tid_offset());
                           Opnd* tid_addr = irm->newOpnd(typeInt32Ptr);
                           Inst* get_tid_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tid_addr, parentObj, offset_tid);
                           get_tid_addr_inst->setBCOffset(last_bc_offset);
                           Opnd* tid = irm->newMemOpnd(typeInt32, MemOpndKind_Any, tid_addr); 

                           Opnd* m_name = irm->newImmOpnd(typeIntPort, 
					(POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

                           Opnd* m_sig = irm->newImmOpnd(typeIntPort, 
					(POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());
                           Opnd* clz_name = irm->newImmOpnd(typeIntPort, 
					(POINTER_SIZE_INT)classHandle->m_name->bytes);
		  

                           Inst* print_inst = genObjectPrintInst(format7, tid, cnt, m_name, m_sig, clz_name, parentObj);
	                  print_inst->setBCOffset(last_bc_offset);
  
                          get_cnt_addr_inst->insertBefore(inst);
                          get_tid_addr_inst->insertBefore(inst);
                          print_inst->insertBefore(inst);
                    

	               }
                        

                        

                        //2Synchronize : Unlock
                        appendUnlock(lock_addr, access_unlock_node);
                    

                        //2Original Statements after Access Operation
                        if(merge_follow_node)
                        {
                            while(inst_next)
                            {
                                Inst* inst = inst_next;
                                inst_next = (Inst*)inst_next->next();
                                inst->unlink();
                                access_unlock_node->appendInst(cloneInst(inst));
                                access_only_node->appendInst(inst);
                            }
                        }
                        else{
                            while(inst_next)
                            {
                                Inst* inst = inst_next;
                                inst_next = (Inst*)inst_next->next();
                                inst->unlink();
                                follow_node->appendInst(inst);
                            }
                        }
                        
                    }



                    {
                        Opnd* alloc_cnt_offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_count_offset());
                        Opnd* alloc_cnt_addr = irm->newOpnd(typeInt32Ptr);
                        Inst* get_alloc_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, alloc_cnt_addr, parentObj, alloc_cnt_offset);

                        Opnd* alloc_cnt = irm->newMemOpnd(typeInt32, alloc_cnt_addr);
                        Opnd* neg_one = irm->newImmOpnd(typeInt32, -1);
                        Inst*com_tls_inst = irm->newInst(Mnemonic_CMP, neg_one, alloc_cnt);

                        Inst* jmp_com_node = irm->newBranchInst(Mnemonic_JE, access_only_node, cas_node);

                        com_ignore_node->appendInst(get_alloc_cnt_addr_inst);
                        com_ignore_node->appendInst(com_tls_inst);
                        com_ignore_node->appendInst(jmp_com_node);
                    }



                    {
                        appendUnlock(lock_addr, unlock_sleep_node);


                        unlock_sleep_node->appendInst(irManager->newRuntimeHelperCallInst(
                            VM_RT_GC_SAFE_POINT, 0, NULL, NULL)
                            );

//                        Opnd* hundred = irm->newImmOpnd(typeInt32, 100);

						struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
						Opnd* m_class_name = irm->newImmOpnd(typeIntPort, (POINTER_SIZE_INT)classHandle->m_name->bytes);
						
                        Opnd* m_name = irm->newImmOpnd(typeIntPort, 
                            (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

                        Opnd* m_sig = irm->newImmOpnd(typeIntPort, 
                            (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());

                        Opnd* args[] = { parentObj, m_class_name, m_name, m_sig};

                        Opnd* sleep_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)usleep_debug));
                        Inst* sleep_inst = irm->newCallInst(sleep_method_addr, &CallingConvention_CDECL, lengthof(args), args, NULL);
                        sleep_inst->setBCOffset(last_bc_offset);
                        unlock_sleep_node->appendInst(sleep_inst);

#ifdef ORDER_DEBUG
/*
                        Opnd* arg_obj[] = { parentObj};
                        Opnd* enter_sleep_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)VMInterface::vmEnterSleep));
                        Inst* enter_sleep_inst = irm->newCallInst(enter_sleep_method_addr, &CallingConvention_CDECL, lengthof(arg_obj), arg_obj, NULL);
                        enter_sleep_inst->setBCOffset(last_bc_offset);
                        unlock_sleep_node->appendInst(enter_sleep_inst);
*/
#endif
                    }

                    //1Construct: fat_lock_node
                    {
                        //1For debug : #10062801

                         fat_lock_node->appendInst(irManager->newRuntimeHelperCallInst(
                            VM_RT_GC_SAFE_POINT, 0, NULL, NULL)
                            );

                         Opnd* args[] = { parentObj };

                         Opnd* fat_lock_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)usleep_try_lock_obj_dummy));
//                         Opnd* fat_lock_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)try_lock_obj));
                         Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, lengthof(args), args, NULL);
//                         Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, 1, &parentObj, NULL);
                         fat_lock_inst->setBCOffset(last_bc_offset);
                         fat_lock_node->appendInst(fat_lock_inst);

#ifdef ORDER_DEBUG
/*
                        Opnd* arg_obj[] = { parentObj};
                        Opnd* enter_sleep_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)VMInterface::vmEnterSleep));
                        Inst* enter_sleep_inst = irm->newCallInst(enter_sleep_method_addr, &CallingConvention_CDECL, lengthof(arg_obj), arg_obj, NULL);
                        enter_sleep_inst->setBCOffset(last_bc_offset);
                        fat_lock_node->appendInst(enter_sleep_inst);
*/
#endif
                    }

                    //2Construct: Wait Node
                    {
                        appendWait(wait_node, access_unlock_node, unlock_sleep_node, current_tid, parentObj);
                    }

                    //2Consturct: CAS Node
                    {
                        //2Synchronize : Lock
                        if (use_cas)
                        {
                            appendLockAndJmp(cas_node, compare_node, fat_lock_node, lock_addr);
                        }
                        else
                        {
                         Opnd* args[] = { parentObj };
                         Opnd* ret_val = irm->newOpnd(typeUInt32);

                         Opnd* lock_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)special_lock));
//                         Opnd* fat_lock_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)try_lock_obj));
                         Inst* lock_inst = irm->newCallInst(lock_method_addr, &CallingConvention_CDECL, lengthof(args), args, ret_val);
//                         Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, 1, &parentObj, NULL);

                         Opnd* zero = irm->newImmOpnd(typeUInt32, 0);
                         Inst* cmp = irManager->newInst(Mnemonic_CMP, ret_val, zero);
                         Inst* jmp_inst = irm->newBranchInst(Mnemonic_JZ, compare_node, fat_lock_node);

                         lock_inst->setBCOffset(last_bc_offset);
                         cas_node->appendInst(lock_inst);
                         cmp->setBCOffset(last_bc_offset);
                         cas_node->appendInst(cmp);
                         jmp_inst->setBCOffset(last_bc_offset);
                         cas_node->appendInst(jmp_inst);


                        }
                    }


                    //2Consturct: Compare Node
                    {
                        //2Compare : Part One
                        //generate compare instructions
                        appendCmpJmp(compare_node, ap_update_node, wait_node, current_tid);

                    }

                    //2Consturct: AP Update Node
                    {
                        //2Compare : Part Two
                        //generate instructions to update access pool
                        appendUpdateInst(ap_update_node, parentObj, unlock_sleep_node, access_unlock_node);

//                        appendTlsTidMappingInst(ap_update_node);

//                        appendResetTLSCntInst(ap_update_node, parentObj, current_tid);
                    }

                    if (inst->getParentObjectLoad())
                        inst->clearParentObjectLoad(); //Mark this instruction as processed
                    else if (inst->getParentObjectStore())
                        inst->clearParentObjectStore(); //Mark this instruction as processed
                    else if (inst->getParentClassLoad())
                        inst->clearParentClassLoad(); //Mark this instruction as processed
                    else if (inst->getParentClassStore())
                        inst->clearParentClassStore(); //Mark this instruction as processed
                    else if(inst_is_InstnceOf(inst))
                        ;
                    else
                        assert(0);

                    inst = (Inst*)node->getLastInst();
                    end = nodes.end();
/********************************************************************************************/
#else
/*
                       //xlc : try to ignore the "java.lang.String" class
                       struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
                       if (strcmp(classHandle->m_name->bytes,"java/lang/String") == 0){
                           return;
                       }
		     //end xlc
*/

                       if (inst->getBCOffset() != ILLEGAL_BC_MAPPING_VALUE)
                            last_bc_offset = inst->getBCOffset();

                     Opnd* lock_addr = irm->newOpnd(typeInt8Ptr);

                     Opnd* current_tid = NULL;
#if !defined(DISABLE_PTHREAD_OPT)
                     if (tid_global)
                     {
                     }
                     else
                     {
#endif
                         current_tid = irm->newOpnd(typeInt32);
#if !defined(DISABLE_PTHREAD_OPT)
                     }
#endif

                     //2Preparation
                    {
#if !defined(DISABLE_PTHREAD_OPT)
                        if (tid_global)
                        {
                            assert(irm->pthread_opt_enable);
                            current_tid = tid_global;
                        }
                        else
#endif
                        {
                            assert(!irm->pthread_opt_enable);
                            //2Calculate TLS
//                        Opnd* tls_addr = irm->newOpnd(typeInt32);
                            Opnd* tm_self_tls_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)pthread_self));
                            Inst* get_tls_inst = irm->newCallInst(tm_self_tls_addr, &CallingConvention_CDECL, 0, NULL, current_tid);
/*
                        Opnd* tid_offset = irm->newImmOpnd(typeInt32, ((POINTER_SIZE_INT)HyThreadDummy::tid_offset()));
                        Opnd* tid_addr = irm->newOpnd(typeInt32Ptr);
                        Inst* get_tid_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tid_addr, tls_addr, tid_offset);

                        Opnd* tid = irm->newMemOpnd(typeInt32, tid_addr);
                        Inst* get_tid_inst = irm->newCopyPseudoInst(Mnemonic_MOV, current_tid, tid);
*/
                            get_tls_inst->setBCOffset(last_bc_offset);
                            get_tls_inst->insertBefore(inst);
//                        get_tid_addr_inst->setBCOffset(last_bc_offset);
//                        get_tid_addr_inst->insertBefore(inst);
//                        get_tid_inst->setBCOffset(last_bc_offset);
//                        get_tid_inst->insertBefore(inst);
                        }

                        //2Calculate Lock Address
                        Opnd* lock_offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::lock_offset());
                        Inst* get_lock_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, lock_addr, parentObj, lock_offset);
                        get_lock_addr_inst->insertBefore(inst);

                    }

//1Remove Redundent Address Calculation!!!!!!!!!!!!!!!

                    //2Construct BBs
                    Node* ap_update_node;
                    Node* follow_node;
                    Node* cas_node;
                    Node* compare_node;
                    Node* fat_lock_node;
                    Node* wait_node;
                    Node* unlock_sleep_node;
                    Node* com_ignore_node;
                    //Basic Blocks in ORDER record mode : 
                    //                                                                                                 -> ap_update_node ->       
                    //                                                                                               |                                |     
                    //  node -> com_ignore_node -> cas_node    ->     compare_node ------------------>  wait_node ->  follow_node
                    //                              |               |  |                                 |                                              |                        |
                    //                              |               |    ->   fat_lock_node  ->                                                |                        |
                    //                              |                <----------------unlock_sleep_node----------------                          |
                    //                                ----------------------------------------------------------------------->
                    {
                        ap_update_node = irm->getFlowGraph()->createBlockNode();
                        follow_node = irm->getFlowGraph()->createBlockNode();
                        cas_node = irm->getFlowGraph()->createBlockNode();
                        compare_node = irm->getFlowGraph()->createBlockNode();
                        fat_lock_node = irm->getFlowGraph()->createBlockNode();
                        wait_node = irm->getFlowGraph()->createBlockNode();
                        unlock_sleep_node = irm->getFlowGraph()->createBlockNode();
                        com_ignore_node = irm->getFlowGraph()->createBlockNode();

                        compare_node->setExecCount(node->getExecCount());
                        follow_node->setExecCount(node->getExecCount());
                        wait_node->setExecCount(node->getExecCount());

                        //link follow_node to node's outs
                        //unlink node's out edges
                        Edges old_out_edges = node->getOutEdges();
                        for (unsigned int i = 0 ; i < old_out_edges.size() ; i ++)
                        {
                            Edge* old_edge = old_out_edges[i];
                            irm->getFlowGraph()->addEdge(follow_node, old_edge->getTargetNode(), old_edge->getEdgeProb());

                            irm->getFlowGraph()->removeEdge(node, old_edge->getTargetNode());
                        }

                        //link node to cas_node
                        irm->getFlowGraph()->addEdge(node, cas_node);

                        irm->getFlowGraph()->addEdge(cas_node, com_ignore_node);
                        irm->getFlowGraph()->addEdge(com_ignore_node, follow_node);

                        irm->getFlowGraph()->addEdge(cas_node, fat_lock_node);
                        irm->getFlowGraph()->addEdge(com_ignore_node, compare_node);

                        //1For debug : #10062801
                        irm->getFlowGraph()->addEdge(fat_lock_node, cas_node);
//                        irm->getFlowGraph()->addEdge(fat_lock_node, compare_node);

                        irm->getFlowGraph()->addEdge(compare_node, ap_update_node);
                        irm->getFlowGraph()->addEdge(compare_node, wait_node);

                        //link ap_update_node to follow_node
                        irm->getFlowGraph()->addEdge(ap_update_node, unlock_sleep_node);
                        irm->getFlowGraph()->addEdge(ap_update_node, follow_node);

//                        irm->getFlowGraph()->addEdge(fat_lock_node, compare_node);

                        irm->getFlowGraph()->addEdge(wait_node, unlock_sleep_node);
                        irm->getFlowGraph()->addEdge(wait_node, follow_node);

                        irm->getFlowGraph()->addEdge(unlock_sleep_node, cas_node);

                    }


                    //2Construct: Follow Node
                    {
                        Inst* inst_next = (Inst*)inst->next();

#ifdef ORDER_DEBUG
#ifdef ORDER_PER_EVA
                        {
                            MethodDesc* desc = getCompilationContext()->getVMCompilationInterface()->getMethodToCompile();
                            MethodDummy *method = (MethodDummy*)desc->getMethodHandle();
                            int64 ptr = (int64)&method->count;

                            Opnd* cnt_addr = irm->newImmOpnd(typeInt32Ptr, ptr);

                            Opnd* cnt_orig = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr); 
                            Opnd* cnt_res = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr);
                            Opnd* one = irm->newImmOpnd(typeInt32, 1);
                            Opnd* cnt_new_val = irm->newOpnd(typeInt32);
    
                            Inst* increase_cnt_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_new_val, one, cnt_orig);
                            Inst* save_cnt_res_inst = irm->newCopyPseudoInst(Mnemonic_MOV, cnt_res, cnt_new_val);

                            follow_node->appendInst(increase_cnt_inst);
                            follow_node->appendInst(save_cnt_res_inst);
                        }
#endif
#endif

/*
#ifdef ORDER_DEBUG
//                        struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
//                        if (strcmp(getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(),"hashCode") == 0 && 
//					Type::isArray(parentObj->getType()->tag) &&
//					strcmp(classHandle->m_name->bytes,"java/lang/String") == 0 )
                        {
                            Opnd* offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_count_offset());
                            Opnd* cnt_addr = irm->newOpnd(typeInt32Ptr);
                            Inst* get_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_addr, parentObj, offset);
                            Opnd* cnt = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr); 

                            Opnd* offset_tid = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_tid_offset());
                            Opnd* tid_addr = irm->newOpnd(typeInt32Ptr);
                            Inst* get_tid_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tid_addr, parentObj, offset_tid);
                            Opnd* tid = irm->newMemOpnd(typeInt32, MemOpndKind_Any, tid_addr); 

                            Opnd* m_name = irm->newImmOpnd(typeIntPort, 
					(POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

//                            Opnd* m_sig = irm->newImmOpnd(typeIntPort, 
//					(POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());


				struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
				Opnd* m_class_name = irm->newImmOpnd(typeIntPort, (POINTER_SIZE_INT)classHandle->m_name->bytes);

                            Inst* print_inst = genConditionalPrintOpndInst(format1, tid, cnt, m_name, m_class_name, parentObj);

                            follow_node->appendInst(get_cnt_addr_inst);
                            follow_node->appendInst(get_tid_addr_inst);
                            follow_node->appendInst(print_inst);

                        }
#endif
*/

                        //2Count
                        //generate count instructions
                        appendCnt(follow_node);

                        //2Original Access Statement
                        inst->unlink();
                        follow_node->appendInst(inst);

                        //2Synchronize : Unlock
                        appendUnlock(lock_addr, follow_node);

                        //2Original Statements after Access Operation
                        while(inst_next)
                        {
                            Inst* inst = inst_next;
                            inst_next = (Inst*)inst_next->next();
                            inst->unlink();
                            follow_node->appendInst(inst);
                        }
                    }



                    {
                        Opnd* alloc_cnt_offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_count_offset());
                        Opnd* alloc_cnt_addr = irm->newOpnd(typeInt32Ptr);
                        Inst* get_alloc_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, alloc_cnt_addr, parentObj, alloc_cnt_offset);

                        Opnd* alloc_cnt = irm->newMemOpnd(typeInt32, alloc_cnt_addr);
                        Opnd* neg_one = irm->newImmOpnd(typeInt32, -1);
                        Inst*com_tls_inst = irm->newInst(Mnemonic_CMP, neg_one, alloc_cnt);

                        Inst* jmp_com_node = irm->newBranchInst(Mnemonic_JE, follow_node, compare_node);

                        com_ignore_node->appendInst(get_alloc_cnt_addr_inst);
                        com_ignore_node->appendInst(com_tls_inst);
                        com_ignore_node->appendInst(jmp_com_node);
                    }



                    {
                        appendUnlock(lock_addr, unlock_sleep_node);


                        unlock_sleep_node->appendInst(irManager->newRuntimeHelperCallInst(
                            VM_RT_GC_SAFE_POINT, 0, NULL, NULL)
                            );

//                        Opnd* hundred = irm->newImmOpnd(typeInt32, 100);

						struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
						Opnd* m_class_name = irm->newImmOpnd(typeIntPort, (POINTER_SIZE_INT)classHandle->m_name->bytes);
						
                        Opnd* m_name = irm->newImmOpnd(typeIntPort, 
                            (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

                        Opnd* m_sig = irm->newImmOpnd(typeIntPort, 
                            (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());

                        Opnd* args[] = { parentObj, m_class_name, m_name, m_sig};

                        Opnd* sleep_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)usleep_debug));
                        Inst* sleep_inst = irm->newCallInst(sleep_method_addr, &CallingConvention_CDECL, lengthof(args), args, NULL);
                        sleep_inst->setBCOffset(last_bc_offset);
                        unlock_sleep_node->appendInst(sleep_inst);

#ifdef ORDER_DEBUG
/*
                        Opnd* arg_obj[] = { parentObj};
                        Opnd* enter_sleep_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)VMInterface::vmEnterSleep));
                        Inst* enter_sleep_inst = irm->newCallInst(enter_sleep_method_addr, &CallingConvention_CDECL, lengthof(arg_obj), arg_obj, NULL);
                        enter_sleep_inst->setBCOffset(last_bc_offset);
                        unlock_sleep_node->appendInst(enter_sleep_inst);
*/
#endif
                    }

                    //1Construct: fat_lock_node
                    {
                        //1For debug : #10062801

                         fat_lock_node->appendInst(irManager->newRuntimeHelperCallInst(
                            VM_RT_GC_SAFE_POINT, 0, NULL, NULL)
                            );

						
                         Opnd* args[] = { parentObj };

                         Opnd* fat_lock_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)usleep_try_lock_obj_dummy));
//                         Opnd* fat_lock_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)try_lock_obj));
                         Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, lengthof(args), args, NULL);
//                         Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, 1, &parentObj, NULL);
                         fat_lock_inst->setBCOffset(last_bc_offset);
                         fat_lock_node->appendInst(fat_lock_inst);

#ifdef ORDER_DEBUG
/*
                        Opnd* arg_obj[] = { parentObj};
                        Opnd* enter_sleep_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)VMInterface::vmEnterSleep));
                        Inst* enter_sleep_inst = irm->newCallInst(enter_sleep_method_addr, &CallingConvention_CDECL, lengthof(arg_obj), arg_obj, NULL);
                        enter_sleep_inst->setBCOffset(last_bc_offset);
                        fat_lock_node->appendInst(enter_sleep_inst);
*/
#endif
                    }

                    //2Construct: Wait Node
                    {
                        appendWait(wait_node, follow_node, unlock_sleep_node, current_tid, parentObj);
                    }

                    //2Consturct: CAS Node
                    {
                        //2Synchronize : Lock
                        if (use_cas)
                        {
                            appendLockAndJmp(cas_node, com_ignore_node, fat_lock_node, lock_addr);
                        }
                        else
                        {
                         Opnd* args[] = { parentObj };
                         Opnd* ret_val = irm->newOpnd(typeUInt32);

                         Opnd* lock_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)special_lock));
//                         Opnd* fat_lock_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)try_lock_obj));
                         Inst* lock_inst = irm->newCallInst(lock_method_addr, &CallingConvention_CDECL, lengthof(args), args, ret_val);
//                         Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, 1, &parentObj, NULL);

                         Opnd* zero = irm->newImmOpnd(typeUInt32, 0);
                         Inst* cmp = irManager->newInst(Mnemonic_CMP, ret_val, zero);
                         Inst* jmp_inst = irm->newBranchInst(Mnemonic_JZ, com_ignore_node, fat_lock_node);

                         lock_inst->setBCOffset(last_bc_offset);
                         cas_node->appendInst(lock_inst);
                         cmp->setBCOffset(last_bc_offset);
                         cas_node->appendInst(cmp);
                         jmp_inst->setBCOffset(last_bc_offset);
                         cas_node->appendInst(jmp_inst);


                        }

                    }


                    //2Consturct: Compare Node
                    {
                        //2Compare : Part One
                        //generate compare instructions
                        appendCmpJmp(compare_node, ap_update_node, wait_node, current_tid);

                    }

                    //2Consturct: AP Update Node
                    {
                        //2Compare : Part Two
                        //generate instructions to update access pool
                        appendUpdateInst(ap_update_node, parentObj, unlock_sleep_node, follow_node);

//                        appendTlsTidMappingInst(ap_update_node);

//                        appendResetTLSCntInst(ap_update_node, parentObj, current_tid);
                    }

                    if (inst->getParentObjectLoad())
                        inst->clearParentObjectLoad(); //Mark this instruction as processed
                    else if (inst->getParentObjectStore())
                        inst->clearParentObjectStore(); //Mark this instruction as processed
                    else if (inst->getParentClassLoad())
                        inst->clearParentClassLoad(); //Mark this instruction as processed
                    else if (inst->getParentClassStore())
                        inst->clearParentClassStore(); //Mark this instruction as processed
                    else
                        assert(0);

                    inst = (Inst*)node->getLastInst();
                    end = nodes.end();
#endif
                }

//#ifdef XLC_TRUE				
	       else if(inst_is_InstnceOf(inst) || inst_is_InstnceOfWithResolve(inst)){
                    if(inst_is_InstnceOf(inst)){
                        parentObj = inst->getOpnd(((CallInst *)inst)->getTargetOpndIndex() + 1);
#ifdef ORDER_DEBUG
                        printf("[INSTANCEOF]: Meeting an instanceof inst, parentObj %ld!!!\n", (long)parentObj);
#endif
                    }
                    else{
                        parentObj = inst->getOpnd(((CallInst *)inst)->getTargetOpndIndex() + 3);
#ifdef ORDER_DEBUG
                        printf("[INSTANCEOF_WITHRESOLVE]: Meeting an INSTANCEOF_WITHRESOLVE inst, parentObj %ld!!!\n", (long)parentObj);   
#endif
                    }

                    assert(((CallInst *)inst)->getTargetOpndIndex() == 1);
                    assert(parentObj);
					
                    if (inst->getBCOffset() != ILLEGAL_BC_MAPPING_VALUE)
                            last_bc_offset = inst->getBCOffset();

                     Opnd* lock_addr = irm->newOpnd(typeInt8Ptr);

                     Opnd* current_tid = NULL;
#if !defined(DISABLE_PTHREAD_OPT)
                     if (tid_global)
                     {
                     }
                     else
                     {
#endif
                         current_tid = irm->newOpnd(typeInt32);
#if !defined(DISABLE_PTHREAD_OPT)
                     }
#endif

                     //2Preparation
                    {
#if !defined(DISABLE_PTHREAD_OPT)
                        if (tid_global)
                        {
                            assert(irm->pthread_opt_enable);
                            current_tid = tid_global;
                        }
                        else
#endif
                        {
                            assert(!irm->pthread_opt_enable);
                            //2Calculate TLS
//                        Opnd* tls_addr = irm->newOpnd(typeInt32);
                            Opnd* tm_self_tls_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)pthread_self));
                            Inst* get_tls_inst = irm->newCallInst(tm_self_tls_addr, &CallingConvention_CDECL, 0, NULL, current_tid);
/*
                        Opnd* tid_offset = irm->newImmOpnd(typeInt32, ((POINTER_SIZE_INT)HyThreadDummy::tid_offset()));
                        Opnd* tid_addr = irm->newOpnd(typeInt32Ptr);
                        Inst* get_tid_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tid_addr, tls_addr, tid_offset);

                        Opnd* tid = irm->newMemOpnd(typeInt32, tid_addr);
                        Inst* get_tid_inst = irm->newCopyPseudoInst(Mnemonic_MOV, current_tid, tid);
*/
                            get_tls_inst->setBCOffset(last_bc_offset);
                            get_tls_inst->insertBefore(inst);
//                        get_tid_addr_inst->setBCOffset(last_bc_offset);
//                        get_tid_addr_inst->insertBefore(inst);
//                        get_tid_inst->setBCOffset(last_bc_offset);
//                        get_tid_inst->insertBefore(inst);
                        }

                        //2Calculate Lock Address
                        Opnd* lock_offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::lock_offset());
                        Inst* get_lock_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, lock_addr, parentObj, lock_offset);
                        get_lock_addr_inst->insertBefore(inst);

                    }

//1Remove Redundent Address Calculation!!!!!!!!!!!!!!!

                    //2Construct BBs
                    Node* ap_update_node;
                    Node* follow_node;
                    Node* cas_node;
                    Node* compare_node;
                    Node* fat_lock_node;
                    Node* wait_node;
                    Node* unlock_sleep_node;
                    Node* com_ignore_node;
                    //Basic Blocks in ORDER record mode : 
                    //                                                                                                 -> ap_update_node ->       
                    //                                                                                               |                                |     
                    //  node -> com_ignore_node -> cas_node    ->     compare_node ------------------>  wait_node ->  follow_node
                    //                              |               |  |                                 |                                              |                        |
                    //                              |               |    ->   fat_lock_node  ->                                                |                        |
                    //                              |                <----------------unlock_sleep_node----------------                          |
                    //                                ----------------------------------------------------------------------->
                    {
                        ap_update_node = irm->getFlowGraph()->createBlockNode();
                        follow_node = irm->getFlowGraph()->createBlockNode();
                        cas_node = irm->getFlowGraph()->createBlockNode();
                        compare_node = irm->getFlowGraph()->createBlockNode();
                        fat_lock_node = irm->getFlowGraph()->createBlockNode();
                        wait_node = irm->getFlowGraph()->createBlockNode();
                        unlock_sleep_node = irm->getFlowGraph()->createBlockNode();
                        com_ignore_node = irm->getFlowGraph()->createBlockNode();

                        compare_node->setExecCount(node->getExecCount());
                        follow_node->setExecCount(node->getExecCount());
                        wait_node->setExecCount(node->getExecCount());

                        //link follow_node to node's outs
                        //unlink node's out edges
                        Edges old_out_edges = node->getOutEdges();
                        for (unsigned int i = 0 ; i < old_out_edges.size() ; i ++)
                        {
                            Edge* old_edge = old_out_edges[i];
                            irm->getFlowGraph()->addEdge(follow_node, old_edge->getTargetNode(), old_edge->getEdgeProb());

                            irm->getFlowGraph()->removeEdge(node, old_edge->getTargetNode());
                        }

                        //link node to cas_node
                        irm->getFlowGraph()->addEdge(node, cas_node);

                        irm->getFlowGraph()->addEdge(cas_node, com_ignore_node);
                        irm->getFlowGraph()->addEdge(com_ignore_node, follow_node);

                        irm->getFlowGraph()->addEdge(cas_node, fat_lock_node);
                        irm->getFlowGraph()->addEdge(com_ignore_node, compare_node);

                        //1For debug : #10062801
                        irm->getFlowGraph()->addEdge(fat_lock_node, cas_node);
//                        irm->getFlowGraph()->addEdge(fat_lock_node, compare_node);

                        irm->getFlowGraph()->addEdge(compare_node, ap_update_node);
                        irm->getFlowGraph()->addEdge(compare_node, wait_node);

                        //link ap_update_node to follow_node
                        irm->getFlowGraph()->addEdge(ap_update_node, unlock_sleep_node);
                        irm->getFlowGraph()->addEdge(ap_update_node, follow_node);

//                        irm->getFlowGraph()->addEdge(fat_lock_node, compare_node);

                        irm->getFlowGraph()->addEdge(wait_node, unlock_sleep_node);
                        irm->getFlowGraph()->addEdge(wait_node, follow_node);

                        irm->getFlowGraph()->addEdge(unlock_sleep_node, cas_node);

                    }


                    //2Construct: Follow Node
                    {
                        Inst* inst_next = (Inst*)inst->next();

#ifdef ORDER_DEBUG
#ifdef ORDER_PER_EVA
                        {
                            MethodDesc* desc = getCompilationContext()->getVMCompilationInterface()->getMethodToCompile();
                            MethodDummy *method = (MethodDummy*)desc->getMethodHandle();
                            int64 ptr = (int64)&method->count;

                            Opnd* cnt_addr = irm->newImmOpnd(typeInt32Ptr, ptr);

                            Opnd* cnt_orig = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr); 
                            Opnd* cnt_res = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr);
                            Opnd* one = irm->newImmOpnd(typeInt32, 1);
                            Opnd* cnt_new_val = irm->newOpnd(typeInt32);
    
                            Inst* increase_cnt_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_new_val, one, cnt_orig);
                            Inst* save_cnt_res_inst = irm->newCopyPseudoInst(Mnemonic_MOV, cnt_res, cnt_new_val);

                            follow_node->appendInst(increase_cnt_inst);
                            follow_node->appendInst(save_cnt_res_inst);
                        }
#endif
#endif

/*
#ifdef ORDER_DEBUG
//                        struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
//                        if (strcmp(getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(),"hashCode") == 0 && 
//					Type::isArray(parentObj->getType()->tag) &&
//					strcmp(classHandle->m_name->bytes,"java/lang/String") == 0 )
                        {
                            Opnd* offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_count_offset());
                            Opnd* cnt_addr = irm->newOpnd(typeInt32Ptr);
                            Inst* get_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_addr, parentObj, offset);
                            Opnd* cnt = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr); 

                            Opnd* offset_tid = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_tid_offset());
                            Opnd* tid_addr = irm->newOpnd(typeInt32Ptr);
                            Inst* get_tid_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tid_addr, parentObj, offset_tid);
                            Opnd* tid = irm->newMemOpnd(typeInt32, MemOpndKind_Any, tid_addr); 

                            Opnd* m_name = irm->newImmOpnd(typeIntPort, 
					(POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

//                            Opnd* m_sig = irm->newImmOpnd(typeIntPort, 
//					(POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());


				struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
				Opnd* m_class_name = irm->newImmOpnd(typeIntPort, (POINTER_SIZE_INT)classHandle->m_name->bytes);

                            Inst* print_inst = genConditionalPrintOpndInst(format1, tid, cnt, m_name, m_class_name, parentObj);

                            follow_node->appendInst(get_cnt_addr_inst);
                            follow_node->appendInst(get_tid_addr_inst);
                            follow_node->appendInst(print_inst);

                        }
#endif
*/

                        //2Count
                        //generate count instructions
                        appendCnt(follow_node);

                        //2Original Access Statement
                        inst->unlink();
                        follow_node->appendInst(inst);

                        //2Synchronize : Unlock
                        appendUnlock(lock_addr, follow_node);

                        //2Original Statements after Access Operation
                        while(inst_next)
                        {
                            Inst* inst = inst_next;
                            inst_next = (Inst*)inst_next->next();
                            inst->unlink();
                            follow_node->appendInst(inst);
                        }
                    }



                    {
                        Opnd* alloc_cnt_offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_count_offset());
                        Opnd* alloc_cnt_addr = irm->newOpnd(typeInt32Ptr);
                        Inst* get_alloc_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, alloc_cnt_addr, parentObj, alloc_cnt_offset);

                        Opnd* alloc_cnt = irm->newMemOpnd(typeInt32, alloc_cnt_addr);
                        Opnd* neg_one = irm->newImmOpnd(typeInt32, -1);
                        Inst*com_tls_inst = irm->newInst(Mnemonic_CMP, neg_one, alloc_cnt);

                        Inst* jmp_com_node = irm->newBranchInst(Mnemonic_JE, follow_node, compare_node);

                        com_ignore_node->appendInst(get_alloc_cnt_addr_inst);
                        com_ignore_node->appendInst(com_tls_inst);
                        com_ignore_node->appendInst(jmp_com_node);
                    }



                    {
                        appendUnlock(lock_addr, unlock_sleep_node);


                        unlock_sleep_node->appendInst(irManager->newRuntimeHelperCallInst(
                            VM_RT_GC_SAFE_POINT, 0, NULL, NULL)
                            );

//                        Opnd* hundred = irm->newImmOpnd(typeInt32, 100);

						struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
						Opnd* m_class_name = irm->newImmOpnd(typeIntPort, (POINTER_SIZE_INT)classHandle->m_name->bytes);
						
                        Opnd* m_name = irm->newImmOpnd(typeIntPort, 
                            (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

                        Opnd* m_sig = irm->newImmOpnd(typeIntPort, 
                            (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());

                        Opnd* args[] = { parentObj, m_class_name, m_name, m_sig};

                        Opnd* sleep_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)usleep_debug));
                        Inst* sleep_inst = irm->newCallInst(sleep_method_addr, &CallingConvention_CDECL, lengthof(args), args, NULL);
                        sleep_inst->setBCOffset(last_bc_offset);
                        unlock_sleep_node->appendInst(sleep_inst);

#ifdef ORDER_DEBUG
/*
                        Opnd* arg_obj[] = { parentObj};
                        Opnd* enter_sleep_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)VMInterface::vmEnterSleep));
                        Inst* enter_sleep_inst = irm->newCallInst(enter_sleep_method_addr, &CallingConvention_CDECL, lengthof(arg_obj), arg_obj, NULL);
                        enter_sleep_inst->setBCOffset(last_bc_offset);
                        unlock_sleep_node->appendInst(enter_sleep_inst);
*/
#endif
                    }

                    //1Construct: fat_lock_node
                    {
                        //1For debug : #10062801

                         fat_lock_node->appendInst(irManager->newRuntimeHelperCallInst(
                            VM_RT_GC_SAFE_POINT, 0, NULL, NULL)
                            );

						
                         Opnd* args[] = { parentObj };

                         Opnd* fat_lock_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)usleep_try_lock_obj_dummy));
//                         Opnd* fat_lock_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)try_lock_obj));
                         Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, lengthof(args), args, NULL);
//                         Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, 1, &parentObj, NULL);
                         fat_lock_inst->setBCOffset(last_bc_offset);
                         fat_lock_node->appendInst(fat_lock_inst);

#ifdef ORDER_DEBUG
/*
                        Opnd* arg_obj[] = { parentObj};
                        Opnd* enter_sleep_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)VMInterface::vmEnterSleep));
                        Inst* enter_sleep_inst = irm->newCallInst(enter_sleep_method_addr, &CallingConvention_CDECL, lengthof(arg_obj), arg_obj, NULL);
                        enter_sleep_inst->setBCOffset(last_bc_offset);
                        fat_lock_node->appendInst(enter_sleep_inst);
*/
#endif
                    }

                    //2Construct: Wait Node
                    {
                        appendWait(wait_node, follow_node, unlock_sleep_node, current_tid, parentObj);
                    }

                    //2Consturct: CAS Node
                    {
                        //2Synchronize : Lock
                        if (use_cas)
                        {
                            appendLockAndJmp(cas_node, com_ignore_node, fat_lock_node, lock_addr);
                        }
                        else
                        {
                         Opnd* args[] = { parentObj };
                         Opnd* ret_val = irm->newOpnd(typeUInt32);

                         Opnd* lock_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)special_lock));
//                         Opnd* fat_lock_method_addr = irm->newImmOpnd(typeInt32Ptr, ((POINTER_SIZE_INT)try_lock_obj));
                         Inst* lock_inst = irm->newCallInst(lock_method_addr, &CallingConvention_CDECL, lengthof(args), args, ret_val);
//                         Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, 1, &parentObj, NULL);

                         Opnd* zero = irm->newImmOpnd(typeUInt32, 0);
                         Inst* cmp = irManager->newInst(Mnemonic_CMP, ret_val, zero);
                         Inst* jmp_inst = irm->newBranchInst(Mnemonic_JZ, com_ignore_node, fat_lock_node);

                         lock_inst->setBCOffset(last_bc_offset);
                         cas_node->appendInst(lock_inst);
                         cmp->setBCOffset(last_bc_offset);
                         cas_node->appendInst(cmp);
                         jmp_inst->setBCOffset(last_bc_offset);
                         cas_node->appendInst(jmp_inst);


                        }

                    }


                    //2Consturct: Compare Node
                    {
                        //2Compare : Part One
                        //generate compare instructions
                        appendCmpJmp(compare_node, ap_update_node, wait_node, current_tid);

                    }

                    //2Consturct: AP Update Node
                    {
                        //2Compare : Part Two
                        //generate instructions to update access pool
                        appendUpdateInst(ap_update_node, parentObj, unlock_sleep_node, follow_node);

//                        appendTlsTidMappingInst(ap_update_node);

//                        appendResetTLSCntInst(ap_update_node, parentObj, current_tid);
                    }

                    if (inst->getParentObjectLoad())
                        inst->clearParentObjectLoad(); //Mark this instruction as processed
                    else if (inst->getParentObjectStore())
                        inst->clearParentObjectStore(); //Mark this instruction as processed
                    else if (inst->getParentClassLoad())
                        inst->clearParentClassLoad(); //Mark this instruction as processed
                    else if (inst->getParentClassStore())
                        inst->clearParentClassStore(); //Mark this instruction as processed
                    else if(inst_is_InstnceOf(inst) || inst_is_InstnceOfWithResolve(inst))
                        ;
                    else
                        assert(0);

                    inst = (Inst*)node->getLastInst();
                    end = nodes.end();
	       }
//#endif	
            }
        }
    }




//#ifdef ORDER_DEBUG

//#ifdef ORDER_DEBUG_ALLOC_INFO
//    if (strcmp(getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(),"hashCode") == 0 && 
//	strcmp(classHandle->m_name->bytes,"java/lang/String") == 0 )
//    if(strncasecmp(classHandle->m_name->bytes, "org/jruby/", 10)  == 0||
//	strncasecmp(classHandle->m_name->bytes, "java/util/concurrent/", 21) == 0)
    {
        Node* entry = irm->getFlowGraph()->getEntryNode();

        Edges out_edges = entry->getOutEdges();
        for (unsigned int i = 0 ; i < out_edges.size() ; i ++)
        {
            Edge* edge = out_edges[i];
            Node* node_after_entry = edge->getTargetNode();

            struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
            Opnd* c_name = irm->newImmOpnd(typeIntPort, 
                  (POINTER_SIZE_INT)classHandle->m_name->bytes);

            Opnd* m_name = irm->newImmOpnd(typeIntPort, 
                  (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

            Opnd* m_sig = irm->newImmOpnd(typeIntPort, 
                  (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());



            Inst* print_inst = genConditionalPrintOpndInst(m_name, m_sig, c_name);
//            Inst* print_inst = genConditionalPrintOpndInst(format2, c_name, m_name, m_sig);


            node_after_entry->prependInst(print_inst);

        }
    }

//#endif
/*
    {
        Node* exit = irm->getFlowGraph()->getReturnNode();
        //Node* exit = irm->getFlowGraph()->getExitNode();

        Edges in_edges = exit->getInEdges();
        for (unsigned int i = 0 ; i < in_edges.size() ; i ++)
        {
            Edge* edge = in_edges[i];
            Node* node_before_exit = edge->getSourceNode();

            struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
            Opnd* c_name = irm->newImmOpnd(typeIntPort, 
                  (POINTER_SIZE_INT)classHandle->m_name->bytes);

            Opnd* m_name = irm->newImmOpnd(typeIntPort, 
                  (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

            Opnd* m_sig = irm->newImmOpnd(typeIntPort, 
                  (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());



            //Inst* print_inst = genConditionalPrintOpndInst(format3, c_name, m_name, m_sig);
            Inst* print_inst = genConditionalPrintOpndInst(m_name, m_sig, c_name);


            node_before_exit->appendInst(print_inst);
        }
    }
*/
//#endif



    irm->getFlowGraph()->orderNodes();
    irm->eliminateSameOpndMoves();
    irm->getFlowGraph()->purgeEmptyNodes();
    irm->getFlowGraph()->mergeAdjacentNodes(true, false);
    irm->getFlowGraph()->purgeUnreachableNodes();

    irm->packOpnds();
    irm->invalidateLivenessInfo();
    irm->updateLoopInfo();
    irm->updateLivenessInfo();
    irm->calculateOpndStatistics();
    getCompilationContext()->setLIRManager(irm);
}

}}; //namespace Ia32


