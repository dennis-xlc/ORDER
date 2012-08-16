


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

struct ClassDummy {
public:
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
};




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
    static unsigned counter_offset() { return (unsigned)(POINTER_SIZE_INT)(&((ManagedObjectDummy*)NULL)->access_count); }
    static unsigned tls_offset() { return (unsigned)(POINTER_SIZE_INT)(&((ManagedObjectDummy*)NULL)->access_tid); }
    static unsigned lock_offset() { return (unsigned)(POINTER_SIZE_INT)(&((ManagedObjectDummy*)NULL)->access_lock); }

    static unsigned alloc_tid_offset() { return (unsigned)(POINTER_SIZE_INT)(&((ManagedObjectDummy*)NULL)->alloc_tid); }
    static unsigned alloc_count_offset() { return (unsigned)(POINTER_SIZE_INT)(&((ManagedObjectDummy*)NULL)->alloc_count); }


#endif

    VMEXPORT static bool _tag_pointer;
} ManagedObjectDummy;


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

#define BYPASS_INSTRUMENT

namespace Jitrino
{
namespace Ia32{

class ORDERRep : public SessionAction {
public:
    ORDERRep()
        :irm(0), typeInt16(0), typeInt32(0), typeInt16Ptr(0), typeInt32Ptr(0), typeIntPort(0), typeIntPtrPort(0),  parentObj(0) {}

    void runImpl();
    U_32 getSideEffects() const {return 0;}
    U_32 getNeedInfo()const {return 0;}
    Inst* genObjectPrintInst(const char * format, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3, Opnd * opnd4, Opnd * opnd5, Opnd * opnd6);

    static void try_lock_obj(ManagedObjectDummy* p_obj);
    static void usleep_debug(ManagedObjectDummy* p_obj, char* m_class_name, char* m_name, char* m_sig);
    static void usleep_try_lock_obj_dummy(ManagedObjectDummy* p_obj);
    static U_32 special_lock(ManagedObjectDummy* p_obj);
    static void assert_order(ManagedObjectDummy* p_obj, char* m_class_name, char* m_name, char* m_sig);

private:

    Inst* genPrintOpndInst(const char* format, Opnd* opnd);
    Inst* genPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2);
    Inst* genPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3);
    Inst* genPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3, Opnd* opnd4);

    
//#ifdef ORDER_DEBUG_ALLOC_INFO
    Inst* genConditionalPrintOpndInst(Opnd* opnd1, Opnd* opnd2, Opnd* opnd3);
//#endif
    Inst* genConditionalPrintOpndInst(const char * format, Opnd * opnd1, Opnd * opnd2, Opnd * opnd3);
    Inst* genConditionalPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3, Opnd* opnd4);
    Inst* genConditionalPrintOpndInst(const char* format, Opnd* opnd1, Opnd* opnd2, Opnd* opnd3, Opnd* opnd4, Opnd* opnd5);
    void appendCmpJmp(Node* compare_node, Node* ap_update_node, Node* follow_node, Opnd* tls_current);
    void appendResetTLSCntInst(Node * node, Opnd* obj, Opnd* tls_current);
    void appendCnt(Node* follow_node);
    void appendLockAndJmp(Node* cas_node, Node* follow_node, Node* fat_lock_node, Opnd* lock_addr);
    void appendUnlock(Opnd* lock_addr, Node* follow_node);
    void appendUpdateInst(Node * ap_update_node, Opnd* obj, Node* unlock_sleep_node, Node* follow_node);
//    void appendTlsTidMappingInst(Node* ap_update_node);
    void appendWait(Node* wait_node, Node* true_branch, Node* false_branch, Opnd* tls_current, Opnd* obj);
#ifdef BYPASS_INSTRUMENT
    Opnd *getNewOpnd(Opnd *original_opnd);
    Inst *cloneInst(Inst *original_inst);
#endif
    bool inst_is_InstnceOf(Inst *inst);
    bool inst_is_InstnceOfWithResolve(Inst *inst);
    bool inst_is_CheckCast(Inst *inst);

    IRManager* irm;
    Type* typeUInt8;
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
};
}}

