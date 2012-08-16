
#include "Ia32IRManager.h"
#include "Ia32CodeGenerator.h"
#include "Ia32Printer.h"


#include <pthread.h>

enum Class_State_Dummy {
    ST_Start,                   /// the initial state
    ST_LoadingAncestors,        /// the loading super class and super interfaces
    ST_Loaded,                  /// successfully loaded
    ST_BytecodesVerified,       /// bytecodes for methods verified for the class
    ST_InstanceSizeComputed,    /// preparing the class; instance size known
    ST_Prepared,                /// successfully prepared
    ST_ConstraintsVerified,     /// constraints verified for the class
    ST_Initializing,            /// initializing the class
    ST_Initialized,             /// the class initialized
    ST_Error                    /// bad class or the class initializer failed
};

struct ConstantPoolDummy {
private:
    static const unsigned char TAG_MASK = 0x0F;
    static const unsigned char ERROR_MASK = 0x40;
    static const unsigned char RESOLVED_MASK = 0x80;
    uint16 m_size;
    void* m_entries;
    void* m_failedResolution;
};

#include "String_Pool.h"

typedef struct ClassDummy {
    typedef struct {
        union {
            void* name;
            void* clss;
        };
        unsigned cp_index;
    } Class_Super;
    Class_Super m_super_class;
    const String* m_name;
    void* m_java_name;
    void* m_signature;
    void* m_simple_name;
    void* m_package;
    U_32 m_depth;
    int m_is_suitable_for_fast_instanceof;
    const char* m_class_file_name;
    void* m_src_file_name;
    unsigned m_id;
    void* m_class_loader;
    void** m_class_handle;
    uint16 m_version;
    uint16 m_access_flags;
    Class_State_Dummy m_state;
    bool m_deprecated;
    unsigned m_is_primitive : 1;
    unsigned m_is_array : 1;
    unsigned m_is_array_of_primitives : 1;
    unsigned m_has_finalizer : 1;
    unsigned m_can_access_all : 1;
    unsigned char m_is_fast_allocation_possible;
    unsigned int m_allocated_size;
    unsigned m_unpadded_instance_data_size;
    int m_alignment;
    unsigned m_instance_data_size;
    void* m_vtable;
    Allocation_Handle m_allocation_handle;
    unsigned m_num_virtual_method_entries;
    unsigned m_num_intfc_method_entries;
    void** m_vtable_descriptors;
    unsigned char m_num_dimensions;
    void* m_array_base_class;
    void* m_array_element_class;
    unsigned int m_array_element_size;
    unsigned int m_array_element_shift;
    uint16 m_num_superinterfaces;
    void* m_superinterfaces;
    ConstantPoolDummy m_const_pool;
    uint16 m_num_fields;
    uint16 m_num_static_fields;
    unsigned m_num_instance_refs;
    void* m_fields;
    unsigned m_static_data_size;
    void* m_static_data_block;
    uint16 m_num_methods;
    void* m_methods;
    void* m_finalize_method;
    void* m_static_initializer;
    void* m_default_constructor;
    uint16 m_declaring_class_index;
    uint16 m_enclosing_class_index;
    uint16 m_enclosing_method_index;
    uint16 m_num_innerclasses;

    struct InnerClass {
        uint16 index;
        uint16 access_flags;
    };
    void* m_innerclasses;
    void* m_annotations;
    void* m_invisible_annotations;
    void* m_initializing_thread;
    void* m_cha_first_child;
    void* m_cha_next_sibling;
    void* m_sourceDebugExtension;
    void* m_verify_data;
    void* m_lock;
    uint64 m_num_allocations;
    uint64 m_num_bytes_allocated;
    uint64 m_num_instanceof_slow;
    uint64 m_num_throws;
    uint64 m_num_class_init_checks;
    U_32 m_num_field_padding_bytes;

    static unsigned class_handle_offset(ClassDummy* clz) 
    { 
        return (unsigned)(POINTER_SIZE_INT)(&((ClassDummy*)clz)->m_class_handle) - 
			(unsigned)(POINTER_SIZE_INT)clz; 
    }


}ClassDummy;




typedef struct HyThreadDummy {

#ifndef POSIX
    void* reserved;
#endif
    U_32 request;
    int16 disable_count;
    void * group;
    void *thread_local_storage[10];
    void *library;
    U_32 suspend_count;
    POINTER_SIZE_INT safepoint_callback;
    void *resume_event;
    void *next;
    void *prev;
    POINTER_SIZE_INT os_handle;
    POINTER_SIZE_INT mutex;
    void *monitor;
    void *current_condition;
    IDATA state;
    int priority;
    char java_status;
    char need_to_free;
    U_32 interrupted;
    void *waited_monitor;
    IDATA thread_id;
#ifdef ORDER
    U_32 alloc_count;
//    bool isInVMRegistry;
#endif

    static unsigned tid_offset() { return (unsigned)(POINTER_SIZE_INT)(&((HyThreadDummy*)NULL)->thread_id); }


} HyThreadDummy;



typedef struct ManagedObjectDummy {
#if defined USE_COMPRESSED_VTABLE_POINTERS
    union {
    U_32 vt_offset;
    POINTER_SIZE_INT padding;
    };
    union {
    U_32 obj_info;
    POINTER_SIZE_INT padding2;
    };
#else // USE_COMPRESSED_VTABLE_POINTERS
    VTable *vt_raw;
    union {
    U_32 obj_info;
    POINTER_SIZE_INT padding;
    };
#endif // USE_COMPRESSED_VTABLE_POINTERS

#ifdef ORDER  //xlc
    U_32 access_count;
    U_32 alloc_count;
    union{
        U_32 access_tid;
        U_32 alloc_count_for_hashcode;
    };
    U_16 alloc_tid;
    union{
        U_8 access_lock;
        U_16 order_padding;
    };

    //1 Notice: this is a dummy of ManagedObject 
    //1 We define it here because we cannot access this struction in OPT


    //yzm
    static POINTER_SIZE_INT counter_offset() { return (POINTER_SIZE_INT)(&((ManagedObjectDummy*)NULL)->access_count); }
    static POINTER_SIZE_INT tls_offset() { return (POINTER_SIZE_INT)(&((ManagedObjectDummy*)NULL)->access_tid); }
    static POINTER_SIZE_INT lock_offset() { return (POINTER_SIZE_INT)(&((ManagedObjectDummy*)NULL)->access_lock); }

    static POINTER_SIZE_INT alloc_tid_offset() { return (POINTER_SIZE_INT)(&((ManagedObjectDummy*)NULL)->alloc_tid); }
    static POINTER_SIZE_INT alloc_count_offset() { return (POINTER_SIZE_INT)(&((ManagedObjectDummy*)NULL)->alloc_count); }

#ifdef REPLAY_OPTIMIZE
	static POINTER_SIZE_INT order_padding_offset() { return (POINTER_SIZE_INT)(&((ManagedObjectDummy*)NULL)->order_padding); }
#endif

#endif

    VMEXPORT static bool _tag_pointer;
} ManagedObjectDummy;

#ifdef REPLAY_OPTIMIZE
#define OBJ_RW_BIT 0x800
#endif

#ifdef ORDER_PER_EVA

class Class_Member_Dummy {
public:
    unsigned _offset;

    bool _synthetic;
    bool _deprecated;
    void* _annotations;
    void* _invisible_annotations;

    uint16 _access_flags;
    void* _name;
    void* _descriptor;
    void* _signature;
    void* _class;

#ifdef VM_STATS
    uint64 num_accesses;
    uint64 num_slow_accesses;
#endif
}; // Class_Member




struct MethodDummy : public Class_Member_Dummy {
public:
    struct LocalVarOffset{
        int value;
        LocalVarOffset* next;
    }; 
    enum State {
        ST_NotCompiled,                 // initial state
        ST_NotLinked = ST_NotCompiled,  // native not linked to implementation
        ST_Compiled,                    // compiled by JIT
        ST_Linked = ST_Compiled         // native linked to implementation
    };
    State _state;
    void *_code;
    void *_vtable_patch;

    void *_counting_stub;

    void *_jits;

    Method_Side_Effects _side_effects;
    void *_method_sig;

    /** set by JNI RegisterNatives() funcs */
    void *_registered_native_func;
    U_8 _num_param_annotations;
    void **_param_annotations;
    U_8 _num_invisible_param_annotations;
    void **_invisible_param_annotations; 
    
    void * _default_value;

    unsigned _index;                // index in method table
    unsigned _arguments_slot_num;   // number of slots for method arguments
    uint16 _max_stack;
    uint16 _max_locals;
    uint16 _n_exceptions;           // num exceptions method can throw
    uint16 _n_handlers;             // num exception handlers in byte codes
    void** _exceptions;          // array of exceptions method can throw
    U_32 _byte_code_length;       // num bytes of byte code
    U_8*   _byte_codes;           // method's byte codes
    void *_handlers;             // array of exception handlers in code
    void *_intf_method_for_fake_method;
    struct {
        unsigned is_init        : 1;
        unsigned is_clinit      : 1;
        unsigned is_finalize    : 1;    // is finalize() method
        unsigned is_overridden  : 1;    // has this virtual method been overridden by a loaded subclass?
        unsigned is_nop         : 1;
    } _flags;

    //
    // debugging info
    //
    void *_line_number_table;
    void *_local_vars_table;

    // This is the number of breakpoints which should be set in the
    // method when it is compiled. This number does not reflect
    // multiple breakpoints that are set in the same location by
    // different environments, it counts only unique locations
    U_32 pending_breakpoints;

    /** Information about methods inlined to this. */
    void* _inline_info;
    

    void* _recompilation_callbacks;

    // Records JITs to be notified when a method is recompiled or initially compiled.
    void *_notify_recompiled_records;

    U_8* m_stackmap;
#ifdef ORDER_PER_EVA
    U_32 count;
    U_32 interleaving;
#endif
    U_32 compiling_thread;
}; // Method
#endif


#ifdef RECORD_STATIC_IN_FIELD
#include "annotation.h"
#include "vm_java_support.h"

///////////////////////////////////////////////////////////////////////////////
// A class' members are its fields and methods.  Class_Member is the base
// class for Field and Method, and factors out the commonalities in these
// two classes.
///////////////////////////////////////////////////////////////////////////////
class Class_Member_dummy {
public:
    //
    // access modifiers
    //
    ClassDummy *get_class() const    {return _class;}
    String *get_name() const    {return _name;}


protected:
    Class_Member_dummy()
    {
        _access_flags = 0;
        _class = NULL;
        _offset = 0;
#ifdef VM_STATS
        num_accesses = 0;
        num_slow_accesses = 0;
#endif
        _synthetic = _deprecated = false;
        _annotations = NULL;
        _invisible_annotations = NULL;
        _signature = NULL;
    }

    // offset of class member; 
    //   for virtual  methods, the method's offset within the vtable
    //   for static   methods, not used, always zero
    //   for instance data,    offset within the instance's data block
    //   for static   data,    offset within the class' static data block
    unsigned _offset;

    bool _synthetic;
    bool _deprecated;
    AnnotationTable* _annotations;
    AnnotationTable* _invisible_annotations;

    uint16 _access_flags;
    String* _name;
    String* _descriptor;
    String* _signature;
    ClassDummy* _class;

public:
#ifdef VM_STATS
    uint64 num_accesses;
    uint64 num_slow_accesses;
#endif
}; // Class_Member


///////////////////////////////////////////////////////////////////////////////
// Fields within Class structures.
///////////////////////////////////////////////////////////////////////////////
struct Field_dummy : public Class_Member_dummy{  

private:
    //
    // The initial values of static fields.  This is defined by the
    // ConstantValue attribute in the class file.
    //
    // If there was not ConstantValue attribute for that field then _const_value_index==0
    //
    uint16 _const_value_index;
    Const_Java_Value const_value;
    TypeDesc* _field_type_desc;
    unsigned _is_injected : 1;
    unsigned _is_magic_type : 1;
    unsigned _offset_computed : 1;

    /** Turns on sending FieldAccess events on access to this field */
    char track_access;
    const static char TRACK_ACCESS_MASK = 1;

    /** Turns on sending FieldModification events on modification of this field */
    char track_modification;
    const static char TRACK_MODIFICATION_MASK = 1;

    //union {
    //    char bit_flags;
    //    struct {

    //        /** Turns on sending FieldAccess events on access to this field */
    //        char track_access : 1;
    //        const static char TRACK_ACCESS_MASK = 4;

    //        /** Turns on sending FieldModification events on modification of this field */
    //        char track_modification : 1;
    //        const static char TRACK_MODIFICATION_MASK = 8;
    //    };
    //};
#ifdef RECORD_STATIC_IN_FIELD
public:
//copy from ORDER, may contain some unnessary fields
    U_32 access_count;
    U_32 alloc_count;
    union{
        U_32 access_tid;
        U_32 alloc_count_for_hashcode;
    };
    U_16 alloc_tid;
    union{
        U_8 access_lock;
        U_16 order_padding;
    };

    static POINTER_SIZE_INT counter_offset(Field_dummy* field) 
    { 
        return (POINTER_SIZE_INT)(&((Field_dummy*)field)->access_count) - 
			(POINTER_SIZE_INT)field; 
    }
    static POINTER_SIZE_INT tls_offset(Field_dummy* field) 
    { 
        return (POINTER_SIZE_INT)(&((Field_dummy*)field)->access_tid) - 
			(POINTER_SIZE_INT)field; 
    }

    static POINTER_SIZE_INT lock_offset(Field_dummy* field) 
    { 
        return (POINTER_SIZE_INT)(&((Field_dummy*)field)->access_lock) - 
			(POINTER_SIZE_INT)field; 
    }

    static POINTER_SIZE_INT alloc_tid_offset(Field_dummy* field) 
    { 
        return (POINTER_SIZE_INT)(&((Field_dummy*)field)->alloc_tid) - 
			(POINTER_SIZE_INT)field; 
    }

    static POINTER_SIZE_INT alloc_count_offset(Field_dummy* field) 
    { 
        return (POINTER_SIZE_INT)(&((Field_dummy*)field)->alloc_count) - 
			(POINTER_SIZE_INT)field; 
    }

    static POINTER_SIZE_INT order_padding_offset(Field_dummy* field) 
    { 
        return (POINTER_SIZE_INT)(&((Field_dummy*)field)->order_padding) - 
			(POINTER_SIZE_INT)field; 
    }
	
#endif
}; // Field

#endif

namespace Jitrino
{
namespace Ia32{

class ORDERRec : public SessionAction {
public:
    ORDERRec()
        :irm(0), typeInt16(0), typeInt32(0), typeInt16Ptr(0), typeInt32Ptr(0), typeIntPort(0), typeIntPtrPort(0),  parentObj(0) {}

    void runImpl();
    U_32 getSideEffects() const {return 0;}
    U_32 getNeedInfo()const {return 0;}

private:
    Inst* genObjectPrintInst(const char * format, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3, Opnd * opnd4, Opnd * opnd5, Opnd * opnd6);
    bool inst_is_InstnceOf(Inst *inst);
    bool inst_is_CheckCast(Inst *inst);
    bool inst_is_InstnceOfWithResolve(Inst *inst);
    Inst* genPrintOpndInst(const char* format, Opnd* opnd);
    Inst* genPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2);
    Inst* genPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3);
    Inst* genPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3, Opnd* opnd4);
    Inst* genConditionalPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3);
    Inst* genConditionalPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3, Opnd* opnd4);
    Inst* genConditionalPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3, Opnd* opnd4, Opnd* opnd5);
    Inst* genConditionalPrintOpndInst(Opnd * opnd1, Opnd * opnd2, Opnd * opnd3);
    void appendCmpJmp(Node* compare_node, Node* ap_update_node, Node* follow_node, Opnd* tls_current);
    void appendResetTLSCntInst(Node * node, Opnd* obj, Opnd* tls_current);
    void appendCnt(Node* follow_node);
#ifdef REPLAY_OPTIMIZE
	void appendWRbit(Node* follow_node);
#endif
    void appendLockAndJmp(Node* cas_node, Node* follow_node, Node* fat_lock_node, Opnd* lock_addr);
    void appendUnlock(Opnd* lock_addr, Node* follow_node);
    void appendUpdateInst(Node * ap_update_node, Opnd* obj, Opnd* tls_current);
//    void appendTlsTidMappingInst(Node* ap_update_node);
    Inst* order_newI8PseudoMoveInst(Opnd * dst, Opnd * src);
    Inst* order_newI8ADDInst(Opnd * dst, Opnd * src1, Opnd* src2);

#ifdef RECORD_STATIC_IN_FIELD
    void do_field_access_instrumentation(Node* node, Inst * inst, Opnd* tid_global,  int isWrite);
    void appendCnt_InField(Node* follow_node);
    void appendUnlock_InField(Opnd* lock_addr, Node* follow_node);
    void appendLockAndJmp_InField(Node* cas_node, Node* follow_node, Node* fat_lock_node, Opnd* lock_addr);
    void appendCmpJmp_InField(Node* compare_node, Node* ap_update_node, Node* follow_node, Opnd* tls_current);
    void appendUpdateInst_InField(Node * ap_update_node, Opnd* field, Opnd* tls_current);
    void appendResetTLSCntInst_InField(Node * ap_update_node, Opnd* field, Opnd* tls_current);
#ifdef REPLAY_OPTIMIZE
    void appendWRbit_InField(Node* follow_node);
#endif

#endif

    Ia32::IRManager* irm;
    Type* typeInt8;
    Type* typeInt16;
    Type* typeInt32;
    Type* typeUInt32;
    Type* typeInt8Ptr;
    Type* typeInt16Ptr;
    Type* typeInt32Ptr;
    Type* typeIntPort;
    Type* typeIntPtrPort;
    Opnd* parentObj;
#ifdef RECORD_STATIC_IN_FIELD
    Field_dummy *accessField;
    Opnd* accessFieldOpnd;
#endif

};

static ActionFactory<ORDERRec> _order_rec("order_rec");

//const static char* format1 = "obj access : (%d, %d), %s, %s\n";
//const static char* format2 = "method entry : (%s), (%s, %s)\n";
//const static char* format3 = "method exit : (%s), (%s, %s)\n";
//const static char* format3 = "count old : 0x%hx\n";
//const static char* format4 = "tls new : 0x%hx\n";
//const static char* format5 = "lock old : 0x%hx\n";
//const static char* format6 = "lock new : 0x%hx\n";
//const static char* format7 = "[INSTANCE_OF]: object (%d, %d) in (%s, %s, %s) ==> %ld by thread %d\n";

static uint16 last_bc_offset = 0;


static void printObjectAccess(const char * format, POINTER_SIZE_INT opnd1, POINTER_SIZE_INT opnd2,
	POINTER_SIZE_INT opnd3, POINTER_SIZE_INT opnd4,  POINTER_SIZE_INT opnd5, POINTER_SIZE_INT opnd6)
{


        printf(format, opnd1, opnd2, opnd3, opnd4, opnd5, opnd6, pthread_self());

}


Inst* ORDERRec::genObjectPrintInst(const char * format, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3, Opnd * opnd4, Opnd * opnd5, Opnd *opnd6)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd1, opnd2, opnd3, opnd4, opnd5, opnd6};

    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)printObjectAccess));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}


bool ORDERRec::inst_is_InstnceOf(Inst *inst)
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

bool ORDERRec::inst_is_InstnceOfWithResolve(Inst *inst)
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

bool ORDERRec::inst_is_CheckCast(Inst *inst)
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

Inst* ORDERRec::order_newI8PseudoMoveInst(Opnd * dst, Opnd * src)
{
    Inst* inst = irm->newI8PseudoInst(Mnemonic_MOV, 1, dst, src);

    {
        Constraint* opndConstraints = inst->getConstraints();
        opndConstraints[0] = Constraint(OpndKind_Any, dst->getSize());
        opndConstraints[1] = Constraint(OpndKind_Any, src->getSize());
    }

    return inst;
}

Inst* ORDERRec::order_newI8ADDInst(Opnd * dst, Opnd * src1, Opnd* src2)
{
    Inst* inst = irm->newI8PseudoInst(Mnemonic_ADD, 1, dst, src1, src2);

    {
        Constraint* opndConstraints = inst->getConstraints();
        opndConstraints[0] = Constraint(OpndKind_Any, dst->getSize());
        opndConstraints[1] = Constraint(OpndKind_Any, src1->getSize());
        opndConstraints[2] = Constraint(OpndKind_Any, src2->getSize());
    }

    return inst;
}


void ORDERRec::appendCnt(Node* follow_node)
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

    Inst* increase_cnt_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_new_val, one, cnt_orig);
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

#ifdef REPLAY_OPTIMIZE
void ORDERRec::appendWRbit(Node* follow_node)
{
    assert(parentObj);
    Opnd* offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::order_padding_offset());
    Opnd* obj = parentObj;
    Opnd* rw_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_rw_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, rw_addr, obj, offset);

    Opnd* rw_bit_orig = irm->newMemOpnd(typeInt32, MemOpndKind_Any, rw_addr); 
	Opnd* rw_bit_res = irm->newMemOpnd(typeInt32, MemOpndKind_Any, rw_addr);
    Opnd* rw_bit_num = irm->newImmOpnd(typeInt32, OBJ_RW_BIT);
	Opnd* rw_new_val = irm->newOpnd(typeInt32);

	Inst* or_inst = irm->newInstEx(Mnemonic_OR, 1, rw_new_val, rw_bit_num, rw_bit_orig);
    Inst* save_rw_inst = irm->newCopyPseudoInst(Mnemonic_MOV, rw_bit_res, rw_new_val);
	
    follow_node->appendInst(get_rw_addr_inst);
	follow_node->appendInst(or_inst);
    follow_node->appendInst(save_rw_inst);

}	
#endif


Inst* ORDERRec::genPrintOpndInst(const char* format, Opnd* opnd)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd};

    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)VMInterface::printfAddr()));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}

Inst* ORDERRec::genPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd1, opnd2};

    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)VMInterface::printfAddr()));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}

Inst* ORDERRec::genPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd1, opnd2, opnd3};

    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)VMInterface::printfAddr()));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}

Inst* ORDERRec::genPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3, Opnd* opnd4)
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
        printf(format, opnd1, opnd2, opnd3);
    }
}


Inst* ORDERRec::genConditionalPrintOpndInst(const char * format, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd1, opnd2, opnd3};

    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)conditional_print3));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}

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

Inst* ORDERRec::genConditionalPrintOpndInst(const char * format, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3, Opnd * opnd4)
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
    if (opnd1 % 65536 == 3 && opnd2 == 576)
    {
        printf(format, opnd1, opnd2, opnd3, opnd4);
//        printf("length : %d\n", ((VM_VectorDummy*)opnd5)->length);

//        char* start = (char*)opnd5 + VM_VECTOR_FIRST_ELEM_OFFSET_1_2_4;
//        for (int i = 0 ; i < ((VM_VectorDummy*)opnd5)->length ; i ++)
//        {
//             printf("%c", *start);
//             start+=2;
//        }
//        printf("\n");
    }
}


Inst* ORDERRec::genConditionalPrintOpndInst(const char * format, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3, Opnd * opnd4, Opnd* opnd5)
{
    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { regular_exp, opnd1, opnd2, opnd3, opnd4, opnd5};

    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)conditional_print5));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}

//#ifdef ORDER_DEBUG_ALLOC_INFO
Inst* ORDERRec::genConditionalPrintOpndInst(Opnd * opnd1, Opnd * opnd2, Opnd * opnd3)
{
//    Opnd* regular_exp = irm->newImmOpnd(typeIntPort,  (int64)format);
    Opnd* args[] = { opnd1, opnd2, opnd3};

    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)VMInterface::vmEnterMethod()));
    return irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);
}
//#endif



void ORDERRec::appendCmpJmp(Node* compare_node, Node* ap_update_node, Node* follow_node, Opnd* tls_current)
{
    assert(parentObj);

    Opnd* offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::tls_offset());
    Opnd* obj = parentObj;
    Opnd* tls_record_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_tls_record_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tls_record_addr, obj, offset);

    Opnd* tls_record = irm->newMemOpnd(typeInt32, tls_record_addr);
    Opnd* tls_record_val = irm->newOpnd(typeInt32);

    Inst* get_tls_record_inst = irm->newCopyPseudoInst(Mnemonic_MOV, tls_record_val, tls_record);
    Inst*com_tls_inst = irm->newInst(Mnemonic_CMP, tls_current, tls_record_val);


    Inst* jmp_com_node = irm->newBranchInst(Mnemonic_JNE, ap_update_node, follow_node);

    compare_node->appendInst(get_tls_record_addr_inst);
    compare_node->appendInst(get_tls_record_inst);
    compare_node->appendInst(com_tls_inst);
    compare_node->appendInst(jmp_com_node);
}


void ORDERRec::appendUpdateInst(Node * ap_update_node, Opnd* obj, Opnd* tls_current)
{
    void * record_access_info_method = VMInterface::getRecordAccessInfoMethod();

#ifdef ORDER_DEBUG
    assert(record_access_info_method);
#endif

    Opnd* args[] = {obj};
    Opnd* update_ap_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)record_access_info_method));
    Inst* inst = irm->newCallInst(update_ap_method, &CallingConvention_CDECL, lengthof(args), args, NULL);

    inst->setBCOffset(last_bc_offset);

    ap_update_node->appendInst(inst);
}



void ORDERRec::appendResetTLSCntInst(Node * ap_update_node, Opnd* obj, Opnd* tls_current)
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

/*#ifdef REPLAY_OPTIMIZE
	//set rw bit to zero
	Opnd* rw_bit_offset = irm->newImmOpnd(typeInt32, ManagedObjectDummy::rw_bit_offset());
    Opnd* rw_bit_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_rw_bit_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, rw_bit_addr, obj, rw_bit_offset);

	Opnd* bit = irm->newImmOpnd(typeInt32, 0);
    Opnd* rw_bit = irm->newMemOpnd(typeInt32, rw_bit_addr);
    Inst* set_rw_bit_inst = irm->newCopyPseudoInst(Mnemonic_MOV, rw_bit, _zero);
#endif*/

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
    ap_update_node->appendInst(set_cnt_inst);

//#ifdef ORDER_DEBUG
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
//#endif


}

void ORDERRec::appendLockAndJmp(Node* cas_node, Node* follow_node, Node* fat_lock_node, Opnd* lock_addr)
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


void ORDERRec::appendUnlock(Opnd* lock_addr, Node* follow_node)
{
    Opnd* zero = irm->newImmOpnd(typeInt8, 0);
    Opnd* obj_lock = irm->newMemOpnd(typeInt8, lock_addr);
    Inst* clear_inst = irManager->newCopyPseudoInst(Mnemonic_MOV, obj_lock, zero);

    follow_node->appendInst(clear_inst);
}


/*
//1Used for debug!!!!!!!!!!!!!!!!!!!!!!!
void ORDERRec::appendTlsTidMappingInst(Node* ap_update_node)
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

static void try_lock_obj(ManagedObjectDummy* p_obj)
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
        if (port_atomic_cas8(&p_obj->access_lock, 1, 0) == 0)
        {
            return;
        }
    }
}

#ifdef RECORD_STATIC_IN_FIELD

#ifdef REPLAY_OPTIMIZE
void ORDERRec::appendWRbit_InField(Node* follow_node)
{
    assert(accessFieldOpnd);
    Opnd* offset = irm->newImmOpnd(typeIntPort, Field_dummy::order_padding_offset(accessField));
    Opnd* field = accessFieldOpnd;
    Opnd* rw_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_rw_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, rw_addr, field, offset);

    Opnd* rw_bit_orig = irm->newMemOpnd(typeInt32, MemOpndKind_Any, rw_addr); 
    Opnd* rw_bit_res = irm->newMemOpnd(typeInt32, MemOpndKind_Any, rw_addr);
    Opnd* rw_bit_num = irm->newImmOpnd(typeInt32, OBJ_RW_BIT);
    Opnd* rw_new_val = irm->newOpnd(typeInt32);

    Inst* or_inst = irm->newInstEx(Mnemonic_OR, 1, rw_new_val, rw_bit_num, rw_bit_orig);
    Inst* save_rw_inst = irm->newCopyPseudoInst(Mnemonic_MOV, rw_bit_res, rw_new_val);
	
    follow_node->appendInst(get_rw_addr_inst);
    follow_node->appendInst(or_inst);
    follow_node->appendInst(save_rw_inst);

}	
#endif


void ORDERRec::appendCnt_InField(Node* follow_node)
{
    assert(accessFieldOpnd);
    Opnd* offset = irm->newImmOpnd(typeIntPort, Field_dummy::counter_offset(accessField));

    Opnd* cnt_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_addr, accessFieldOpnd, offset);

    Opnd* cnt_orig = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr); 
    Opnd* cnt_res = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr);
    Opnd* one = irm->newImmOpnd(typeInt32, 1);
    Opnd* cnt_new_val = irm->newOpnd(typeInt32);

    Inst* increase_cnt_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_new_val, one, cnt_orig);
    Inst* save_cnt_res_inst = irm->newCopyPseudoInst(Mnemonic_MOV, cnt_res, cnt_new_val);


    follow_node->appendInst(get_cnt_addr_inst);

    follow_node->appendInst(increase_cnt_inst);
    follow_node->appendInst(save_cnt_res_inst);

}


void ORDERRec::appendUnlock_InField(Opnd* lock_addr, Node* follow_node)
{
    Opnd* zero = irm->newImmOpnd(typeInt8, 0);
    Opnd* obj_lock = irm->newMemOpnd(typeInt8, lock_addr);
    Inst* clear_inst = irManager->newCopyPseudoInst(Mnemonic_MOV, obj_lock, zero);

    follow_node->appendInst(clear_inst);
}


static void try_lock_obj_InField(Field_dummy* field)
{
    for (int i = 0 ; i < 5 ; i ++)
    {
        if (port_atomic_cas8(&field->access_lock, 1, 0) == 0)
        {
            return;
        }
    }
//    for (int i = 0 ; i < 1000 ; i ++)
    while(true)
    {
        usleep(100);
        if (port_atomic_cas8(&field->access_lock, 1, 0) == 0)
        {
            return;
        }
#ifdef LEAP_DEBUG
        printf("[LEAPRec]: sleeping when access field (%s) ...\n", field->get_name()->bytes);
#endif
    }
}


void ORDERRec::appendLockAndJmp_InField(Node* cas_node, Node* follow_node, Node* fat_lock_node, Opnd* lock_addr)
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

void ORDERRec::appendCmpJmp_InField(Node* compare_node, Node* ap_update_node, Node* follow_node, Opnd* tls_current)
{
    assert(accessFieldOpnd);

    Opnd* offset = irm->newImmOpnd(typeIntPort, Field_dummy::tls_offset(accessField));

    Opnd* tls_record_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_tls_record_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tls_record_addr, accessFieldOpnd, offset);

    Opnd* tls_record = irm->newMemOpnd(typeInt32, tls_record_addr);
    Opnd* tls_record_val = irm->newOpnd(typeInt32);

    Inst* get_tls_record_inst = irm->newCopyPseudoInst(Mnemonic_MOV, tls_record_val, tls_record);
    Inst*com_tls_inst = irm->newInst(Mnemonic_CMP, tls_current, tls_record_val);


    Inst* jmp_com_node = irm->newBranchInst(Mnemonic_JNE, ap_update_node, follow_node);

    compare_node->appendInst(get_tls_record_addr_inst);
    compare_node->appendInst(get_tls_record_inst);
    compare_node->appendInst(com_tls_inst);
    compare_node->appendInst(jmp_com_node);
}


void ORDERRec::appendUpdateInst_InField(Node * ap_update_node, Opnd* field, Opnd* tls_current)
{
    void * record_access_info_method = VMInterface::getRecordFieldAccessInfoMethod();

    Opnd* args[] = {field};
    Opnd* update_ap_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)record_access_info_method));
    Inst* inst = irm->newCallInst(update_ap_method, &CallingConvention_CDECL, lengthof(args), args, NULL);

    inst->setBCOffset(last_bc_offset);

    ap_update_node->appendInst(inst);
}


void ORDERRec::appendResetTLSCntInst_InField(Node * ap_update_node, Opnd* field, Opnd* tls_current)
{
    //set TLS to current thread id

    Opnd* tls_offset = irm->newImmOpnd(typeIntPort, Field_dummy::tls_offset(accessField));
    Opnd* tls_record_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_tls_record_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tls_record_addr, field, tls_offset);

    Opnd* tls_record = irm->newMemOpnd(typeInt32, tls_record_addr);
    Inst* set_tls_record_inst = irm->newCopyPseudoInst(Mnemonic_MOV, tls_record, tls_current);

    //set count to zero
    Opnd* cnt_offset = irm->newImmOpnd(typeIntPort, Field_dummy::counter_offset(accessField));
    Opnd* cnt_addr = irm->newOpnd(typeInt32Ptr);
    Inst* get_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_addr, field, cnt_offset);

    Opnd* cnt = irm->newMemOpnd(typeInt32, cnt_addr);
    Opnd* zero = irm->newImmOpnd(typeInt32, 0);
    Inst* set_cnt_inst = irm->newCopyPseudoInst(Mnemonic_MOV, cnt, zero);


    //append instructions
    ap_update_node->appendInst(get_tls_record_addr_inst);



    ap_update_node->appendInst(set_tls_record_inst);

    ap_update_node->appendInst(get_cnt_addr_inst);
    ap_update_node->appendInst(set_cnt_inst);


}



void ORDERRec::do_field_access_instrumentation(Node* node, Inst * inst, Opnd* tid_global, int isWrite)
{

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
            Opnd* tm_self_tls_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)pthread_self));
            Inst* get_tls_inst = irm->newCallInst(tm_self_tls_addr, &CallingConvention_CDECL, 0, NULL, current_tid);
            get_tls_inst->setBCOffset(last_bc_offset);
            get_tls_inst->insertBefore(inst);
         }
         //2Calculate Lock Address
         Opnd* lock_offset = irm->newImmOpnd(typeIntPort, Field_dummy::lock_offset(accessField));
         Inst* get_lock_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, lock_addr, accessFieldOpnd, lock_offset);
         get_lock_addr_inst->insertBefore(inst);
    }



    //2Construct BBs
    Node* ap_update_node;
    Node* follow_node;
    Node* cas_node;
    Node* compare_node;
    Node* fat_lock_node;
                    //Basic Blocks in ORDER record mode : 
                    //                                                       -> ap_update_node ->
                    //                                                       |                                |
                    //  node -> cas_node -> compare_node ------------------> follow_node
                    //                 |                                 |
                    //                   ->   fat_lock_node  ->
    {
        ap_update_node = irm->getFlowGraph()->createBlockNode();
        follow_node = irm->getFlowGraph()->createBlockNode();
        cas_node = irm->getFlowGraph()->createBlockNode();
        compare_node = irm->getFlowGraph()->createBlockNode();
        fat_lock_node = irm->getFlowGraph()->createBlockNode();

        compare_node->setExecCount(node->getExecCount());
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

         //link cas_node to cas_node
         irm->getFlowGraph()->addEdge(cas_node, fat_lock_node);

         //link cas_node to compare_node
         irm->getFlowGraph()->addEdge(cas_node, compare_node);

         //link compare_node to ap_update_node
         irm->getFlowGraph()->addEdge(compare_node, ap_update_node);

         //link compare_node to follow_node
         irm->getFlowGraph()->addEdge(compare_node, follow_node);

         //link ap_update_node to follow_node
         irm->getFlowGraph()->addEdge(ap_update_node, follow_node);

         irm->getFlowGraph()->addEdge(fat_lock_node, compare_node);
    }

    {
        Inst* inst_next = (Inst*)inst->next();
        //2Count
        //generate count instructions

        appendCnt_InField(follow_node);


#ifdef REPLAY_OPTIMIZE
        if(isWrite)
            appendWRbit_InField(follow_node);
#endif

        //2Original Access Statement
        inst->unlink();
#ifdef ORDER_DEBUG
        assert(inst->node == NULL);
#endif
        follow_node->appendInst(inst);

        //2Synchronize : Unlock
        appendUnlock_InField(lock_addr, follow_node);

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
        Opnd* fat_lock_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)try_lock_obj_InField));
        Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, 1, &accessFieldOpnd, NULL);
        fat_lock_inst->setBCOffset(last_bc_offset);
        fat_lock_node->appendInst(fat_lock_inst);
    }


    //2Consturct: CAS Node
    {
        //2Synchronize : Lock
        appendLockAndJmp_InField(cas_node, compare_node, fat_lock_node, lock_addr);
    }


    //2Consturct: Compare Node
    {
        //2Compare : Part One
        //generate compare instructions
        appendCmpJmp_InField(compare_node, ap_update_node, follow_node, current_tid);

    }

     //2Consturct: AP Update Node
    {
         //2Compare : Part Two
         //generate instructions to update access pool
         appendUpdateInst_InField(ap_update_node, accessFieldOpnd, current_tid);
         appendResetTLSCntInst_InField(ap_update_node, accessFieldOpnd, current_tid);
    }
}
#endif

//_________________________________________________________________________________________________
void ORDERRec::runImpl()
{
#ifdef ORDER_DEBUG
   struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();

    printf("Method Name : %s\n", getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());
    printf("Method Sig : %s\n", getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());
    //struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
    printf("Class Name : %s\n", classHandle->m_name->bytes);
#endif
/*
    bool ignore_instanceof = FALSE;
    if(strcmp(classHandle->m_name->bytes,"org/jruby/ast/CallNode") == 0 && 
		strcmp(getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(),"setArgsNode") == 0){
		ignore_instanceof = TRUE;
    }
*/
    irm = getCompilationContext()->getLIRManager();
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
//        printf("pthread opt enable in instrument.\n");
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
#ifdef REPLAY_OPTIMIZE
				int isWrite = 0;
#endif
                parentObj = NULL;

#ifdef RECORD_STATIC_IN_FIELD
                accessField = NULL;
                accessFieldOpnd = NULL;
                if(inst->getAccessFieldLoad() || inst->getAccessFieldStore()){
                    if(inst->getAccessFieldLoad()){
                        accessField = (Field_dummy *)inst->getAccessFieldLoad();
                        isWrite = 0;
                    }
                    else{
                        accessField = (Field_dummy *)inst->getAccessFieldStore();
                        isWrite = 1;
                    }

#ifdef ORDER_DEBUG
                    assert(accessField != NULL);
                    //printf("[FIELD INSTRUMENT]: Inst->getgetAccessField() == %p\n", accessField);
                    printf("[FIELD INSTRUMENT]: field_name is %s in Class ==> %s \n", accessField->get_name()->bytes, accessField->get_class()->m_name->bytes);
#endif
                    accessFieldOpnd = irm->newImmOpnd(typeIntPort, (POINTER_SIZE_INT)accessField);
                    do_field_access_instrumentation(node, inst, tid_global, isWrite);

                    if(inst->getAccessFieldLoad()){
                        inst->clearAccessFieldLoad();
                    }
					
                    if(inst->getAccessFieldStore()){
                        inst->clearAccessFieldStore();
                    }

                    inst = (Inst*)node->getLastInst();
                    end = nodes.end();
                    
                }
#else
                if (inst->getParentClassLoad() || inst->getParentClassStore())
                {

#if (defined(ORDER) && defined(RECORD_STATIC))
                    ClassDummy* parentClz = NULL;
                    if (inst->getParentClassLoad()){
                        parentClz = (ClassDummy*)inst->getParentClassLoad();		
                    }
                    else{
                        parentClz = (ClassDummy*)inst->getParentClassStore();	
#ifdef REPLAY_OPTIMIZE
						isWrite = 1;
#endif
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
                    Opnd* parent_obj_address_address = irm->newOpnd(typeIntPtrPort);
                    Inst* get_class_handle_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, parent_obj_address_address, parent_class, class_handle_offset);

                    get_class_handle_addr_inst->setBCOffset(last_bc_offset);
                    get_class_handle_addr_inst->insertBefore(inst);

                    Opnd* parent_obj_address = irm->newOpnd(typeIntPtrPort);
                    Opnd* parent_obj_address_value = irm->newMemOpnd(typeIntPort, parent_obj_address_address);
                    Inst* load_parent_obj_address_inst = irm->newCopyPseudoInst(Mnemonic_MOV, parent_obj_address, parent_obj_address_value);

                    load_parent_obj_address_inst->setBCOffset(last_bc_offset);
                    load_parent_obj_address_inst->insertBefore(inst);

                    Opnd* parent_obj_in_memory = irm->newMemOpnd(typeIntPort, parent_obj_address);
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
#endif
                else if (inst->getParentObjectLoad() || inst->getParentObjectStore())
                {
                       assert(!inst->getParentClassLoad() && !inst->getParentClassStore());
                       if (inst->getParentObjectLoad()){
                           	parentObj = inst->getParentObjectLoad();
                       }
                       else{
                           	parentObj = inst->getParentObjectStore();		
#ifdef REPLAY_OPTIMIZE
							isWrite = 1;
#endif
                    	}
#ifdef ORDER_DEBUG
                       assert(parentObj);
#endif
                }

                else if(inst_is_InstnceOf(inst)){
                    parentObj = inst->getOpnd(((CallInst *)inst)->getTargetOpndIndex() + 1);
#ifdef ORDER_DEBUG
                    printf("[INSTANCEOF]: Meeting an instanceof inst, parentObj %ld!!!\n", (long)parentObj);

                    assert(((CallInst *)inst)->getTargetOpndIndex() == 1);
                    assert(parentObj);
#endif

                }

                else if(inst_is_InstnceOfWithResolve(inst)){
                    parentObj = inst->getOpnd(((CallInst *)inst)->getTargetOpndIndex() + 3);
#ifdef ORDER_DEBUG
                    printf("[INSTANCEOF_WITHRESOLVE]: Meeting an INSTANCEOF_WITHRESOLVE inst  in (%s, %s, %s)!!!\n",
		  	getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(),
		  	getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString(),
		  	classHandle->m_name->bytes);
                    assert(((CallInst *)inst)->getTargetOpndIndex() == 1);
                    assert(parentObj);
#endif
                }


                if (parentObj)
                {
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
//                        Opnd* tls_addr = irm->newOpnd(typeIntPort);
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
                    //Basic Blocks in ORDER record mode : 
                    //                                                       -> ap_update_node ->
                    //                                                       |                                |
                    //  node -> cas_node -> compare_node ------------------> follow_node
                    //                 |                                 |
                    //                   ->   fat_lock_node  ->
                    {
                        ap_update_node = irm->getFlowGraph()->createBlockNode();
                        follow_node = irm->getFlowGraph()->createBlockNode();
                        cas_node = irm->getFlowGraph()->createBlockNode();
                        compare_node = irm->getFlowGraph()->createBlockNode();
                        fat_lock_node = irm->getFlowGraph()->createBlockNode();

                        compare_node->setExecCount(node->getExecCount());
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

                        //link cas_node to cas_node
                        irm->getFlowGraph()->addEdge(cas_node, fat_lock_node);

                        //link cas_node to compare_node
                        irm->getFlowGraph()->addEdge(cas_node, compare_node);

                        //link compare_node to ap_update_node
                        irm->getFlowGraph()->addEdge(compare_node, ap_update_node);

                        //link compare_node to follow_node
                        irm->getFlowGraph()->addEdge(compare_node, follow_node);

                        //link ap_update_node to follow_node
                        irm->getFlowGraph()->addEdge(ap_update_node, follow_node);

                        irm->getFlowGraph()->addEdge(fat_lock_node, compare_node);
                    }


                    //2Construct: Follow Node
                    {
                        Inst* inst_next = (Inst*)inst->next();

//#ifdef ORDER_DEBUG
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
//#endif
/*
#ifdef ORDER_DEBUG
//                        struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
//                        if (strcmp(classHandle->m_name->bytes,"java/util/Hashtable") == 0 && 
//			strcmp(getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(),"rehash") == 0)
                        {
                            Opnd* offset = irm->newImmOpnd(typeInt32, ManagedObjectDummy::alloc_count_offset());
                            Opnd* cnt_addr = irm->newOpnd(typeInt32Ptr);
                            Inst* get_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_addr, parentObj, offset);
                            Opnd* cnt = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr); 

                            Opnd* offset_tid = irm->newImmOpnd(typeInt32, ManagedObjectDummy::alloc_tid_offset());
                            Opnd* tid_addr = irm->newOpnd(typeInt32Ptr);
                            Inst* get_tid_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tid_addr, parentObj, offset_tid);
                            Opnd* tid = irm->newMemOpnd(typeInt32, MemOpndKind_Any, tid_addr); 

                            Opnd* m_name = irm->newImmOpnd(typeInt32, 
					(POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

                            Opnd* m_sig = irm->newImmOpnd(typeInt32, 
					(POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());


                            Inst* print_inst = genConditionalPrintOpndInst(format1, tid, cnt, m_name, m_sig, parentObj);

                            follow_node->appendInst(get_cnt_addr_inst);
                            follow_node->appendInst(get_tid_addr_inst);
                            follow_node->appendInst(print_inst);

                        }
#endif
*/



                        //2Count
                        //generate count instructions

                        appendCnt(follow_node);

#ifdef REPLAY_OPTIMIZE
						if(isWrite)
							appendWRbit(follow_node);
#endif

                        //2Original Access Statement
                        inst->unlink();
#ifdef ORDER_DEBUG
                        assert(inst->node == NULL);
#endif
                       follow_node->appendInst(inst);
/*
                       if(inst_is_InstnceOf(inst)){

		         printf("[INSTNCEOF]: Meeting an instanceof inst in (%s, %s, %s)!!!\n",
		  	 getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName(),
		  	 getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString(),
		  	 classHandle->m_name->bytes);
		  
                           assert(((CallInst *)inst)->getTargetOpndIndex() == 1);
                           Opnd* arg1 = inst->getOpnd(((CallInst *)inst)->getTargetOpndIndex() + 1);


                           Opnd* offset = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_count_offset());
                           Opnd* cnt_addr = irm->newOpnd(typeInt32Ptr);
                           Inst* get_cnt_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, cnt_addr, arg1, offset);
                           get_cnt_addr_inst->setBCOffset(last_bc_offset);
                           Opnd* cnt = irm->newMemOpnd(typeInt32, MemOpndKind_Any, cnt_addr); 

                           Opnd* offset_tid = irm->newImmOpnd(typeIntPort, ManagedObjectDummy::alloc_tid_offset());
                           Opnd* tid_addr = irm->newOpnd(typeInt32Ptr);
                           Inst* get_tid_addr_inst = irm->newInstEx(Mnemonic_ADD, 1, tid_addr, arg1, offset_tid);
                           get_tid_addr_inst->setBCOffset(last_bc_offset);
                           Opnd* tid = irm->newMemOpnd(typeInt32, MemOpndKind_Any, tid_addr); 

                           Opnd* m_name = irm->newImmOpnd(typeIntPort, 
					(POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

                           Opnd* m_sig = irm->newImmOpnd(typeIntPort, 
					(POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());
                           Opnd* clz_name = irm->newImmOpnd(typeIntPort, 
					(POINTER_SIZE_INT)classHandle->m_name->bytes);
		  

                           Inst* print_inst = genObjectPrintInst(format7, tid, cnt, m_name, m_sig, clz_name);
	                  print_inst->setBCOffset(last_bc_offset);
  
                          get_cnt_addr_inst->insertBefore(inst);
                          get_tid_addr_inst->insertBefore(inst);
                          print_inst->insertBefore(inst);
                    

	               }
                        
*/
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
                         Opnd* fat_lock_method_addr = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)try_lock_obj));
                         Inst* fat_lock_inst = irm->newCallInst(fat_lock_method_addr, &CallingConvention_CDECL, 1, &parentObj, NULL);
                         fat_lock_inst->setBCOffset(last_bc_offset);
                         fat_lock_node->appendInst(fat_lock_inst);
                    }

                    //2Consturct: CAS Node
                    {
                        //2Synchronize : Lock
                        appendLockAndJmp(cas_node, compare_node, fat_lock_node, lock_addr);
                    }


                    //2Consturct: Compare Node
                    {
                        //2Compare : Part One
                        //generate compare instructions
                        appendCmpJmp(compare_node, ap_update_node, follow_node, current_tid);

                    }

                    //2Consturct: AP Update Node
                    {
                        //2Compare : Part Two
                        //generate instructions to update access pool
                        appendUpdateInst(ap_update_node, parentObj, current_tid);

//                        appendTlsTidMappingInst(ap_update_node);

                        appendResetTLSCntInst(ap_update_node, parentObj, current_tid);
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
            }
        }
    }



#ifdef ORDER_DEBUG
/*
    if(strncasecmp(classHandle->m_name->bytes, "org/jruby/", 10)  == 0 ||
	strncasecmp(classHandle->m_name->bytes, "java/util/concurrent/", 21) == 0)
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


    {
        Node* exit = irm->getFlowGraph()->getExitNode();

        Edges in_edges = exit->getInEdges();
        for (unsigned int i = 0 ; i < in_edges.size() ; i ++)
        {
            Edge* edge = in_edges[i];
            Node* node_before_exit = edge->getSourceNode();

            struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
            Opnd* c_name = irm->newImmOpnd(typeInt32, 
                  (POINTER_SIZE_INT)classHandle->m_name->bytes);

            Opnd* m_name = irm->newImmOpnd(typeInt32, 
                  (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

            Opnd* m_sig = irm->newImmOpnd(typeInt32, 
                  (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());



            Inst* print_inst = genConditionalPrintOpndInst(format3, c_name, m_name, m_sig);


            node_before_exit->appendInst(print_inst);
        }
    }
*/
#endif



    irm->getFlowGraph()->orderNodes();
    irm->eliminateSameOpndMoves();
    irm->getFlowGraph()->purgeEmptyNodes();
    irm->getFlowGraph()->mergeAdjacentNodes(true, false);
    irm->getFlowGraph()->purgeUnreachableNodes();

    irm->packOpnds();
    irm->invalidateLivenessInfo();
    irm->updateLoopInfo();
    irm->updateLivenessInfo();

    getCompilationContext()->setLIRManager(irm);
}

}}; //namespace Ia32

