
#include "Ia32IRManager.h"
#include "Ia32CodeGenerator.h"
#include "Ia32Printer.h"



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
};



namespace Jitrino
{
namespace Ia32{


class ORDERDeb : public SessionAction {
public:
    ORDERDeb()
        :irm(0) {}

    void runImpl();
private:
    Ia32::IRManager* irm;
};

static ActionFactory<ORDERDeb> _order_deb("order_deb");

//_________________________________________________________________________________________________
void ORDERDeb::runImpl()
{
    uint16 last_bc_offset = 0;
    irm = getCompilationContext()->getLIRManager();

//    Type* typeInt32 = irm->getTypeManager().getPrimitiveType(Type::Int32);
//    Type* typeInt32Ptr = irm->getManagedPtrType(typeInt32);

#ifdef _EM64T_
    Type* typeIntPort = irm->getTypeManager().getPrimitiveType(Type::Int64);
#else
    Type* typeIntPort = irm->getTypeManager().getPrimitiveType(Type::Int32);
#endif
    Type* typeIntPtrPort = irm->getManagedPtrType(typeIntPort);


    irm->updateLivenessInfo();
    irm->calculateOpndStatistics();
    const Nodes& nodes = irm->getFlowGraph()->getNodesPostOrder();

    struct ClassDummy* classHandle = (struct ClassDummy*)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getParentHandle();
//  if(strncasecmp(classHandle->m_name->bytes, "org/jruby/", 10)  == 0||
//	strncasecmp(classHandle->m_name->bytes, "java/util/concurrent/", 21) == 0)
  {
    for (Nodes::const_iterator it = nodes.begin(),end = nodes.end();it!=end; ++it) {
        Node* node = *it;

        if (node->isBlockNode())
        {
            for (Inst * inst=(Inst*)node->getLastInst(); inst!=NULL; inst=inst->getPrevInst())
            {
                //if (inst->getKind() == Inst::Kind_CallInst)
                if (inst->getKind() == Inst::Kind_RetInst)
                {


                    if (inst->getBCOffset() != ILLEGAL_BC_MAPPING_VALUE)
                         last_bc_offset = inst->getBCOffset();

                    Opnd* c_name = irm->newImmOpnd(typeIntPort, 
                        (POINTER_SIZE_INT)classHandle->m_name->bytes);

                    Opnd* m_name = irm->newImmOpnd(typeIntPort, 
                        (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getName());

                    Opnd* m_sig = irm->newImmOpnd(typeIntPort, 
                        (POINTER_SIZE_INT)getCompilationContext()->getVMCompilationInterface()->getMethodToCompile()->getSignatureString());

                    Opnd* args[] = { m_name, m_sig, c_name};
                    Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)VMInterface::vmEnterMethod()));
                    //Opnd* debug_print_method = irm->newImmOpnd(typeIntPtrPort, ((POINTER_SIZE_INT)VMInterface::vmMethodReturn()));
                    Inst* debug_inst =irm->newCallInst(debug_print_method, &CallingConvention_CDECL, lengthof(args), args, NULL);

                    debug_inst->setBCOffset(last_bc_offset);
                    //debug_inst->insertAfter(inst);
                    debug_inst->insertBefore(inst);
                }
            }
        }
    }
  }

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

