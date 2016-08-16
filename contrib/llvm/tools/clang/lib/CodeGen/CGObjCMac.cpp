//===------- CGObjCMac.cpp - Interface to Apple Objective-C Runtime -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This provides Objective-C code generation targeting the Apple runtime.
//
//===----------------------------------------------------------------------===//

#include "CGObjCRuntime.h"
#include "CGBlocks.h"
#include "CGCleanup.h"
#include "CGRecordLayout.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/StmtObjC.h"
#include "clang/Basic/LangOptions.h"
#include "clang/CodeGen/CGFunctionInfo.h"
#include "clang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdio>

using namespace clang;
using namespace CodeGen;

namespace {

// FIXME: We should find a nicer way to make the labels for metadata, string
// concatenation is lame.

class ObjCCommonTypesHelper {
protected:
  llvm::LLVMContext &VMContext;

private:
  // The types of these functions don't really matter because we
  // should always bitcast before calling them.

  /// id objc_msgSend (id, SEL, ...)
  /// 
  /// The default messenger, used for sends whose ABI is unchanged from
  /// the all-integer/pointer case.
  llvm::Constant *getMessageSendFn() const {
    // Add the non-lazy-bind attribute, since objc_msgSend is likely to
    // be called a lot.
    llvm::Type *params[] = { ObjectPtrTy, SelectorPtrTy };
    return
      CGM.CreateRuntimeFunction(llvm::FunctionType::get(ObjectPtrTy,
                                                        params, true),
                                "objc_msgSend",
                                llvm::AttributeSet::get(CGM.getLLVMContext(),
                                              llvm::AttributeSet::FunctionIndex,
                                                 llvm::Attribute::NonLazyBind));
  }

  /// void objc_msgSend_stret (id, SEL, ...)
  ///
  /// The messenger used when the return value is an aggregate returned
  /// by indirect reference in the first argument, and therefore the
  /// self and selector parameters are shifted over by one.
  llvm::Constant *getMessageSendStretFn() const {
    llvm::Type *params[] = { ObjectPtrTy, SelectorPtrTy };
    return CGM.CreateRuntimeFunction(llvm::FunctionType::get(CGM.VoidTy,
                                                             params, true),
                                     "objc_msgSend_stret");

  }

  /// [double | long double] objc_msgSend_fpret(id self, SEL op, ...)
  ///
  /// The messenger used when the return value is returned on the x87
  /// floating-point stack; without a special entrypoint, the nil case
  /// would be unbalanced.
  llvm::Constant *getMessageSendFpretFn() const {
    llvm::Type *params[] = { ObjectPtrTy, SelectorPtrTy };
    return CGM.CreateRuntimeFunction(llvm::FunctionType::get(CGM.DoubleTy,
                                                             params, true),
                                     "objc_msgSend_fpret");

  }

  /// _Complex long double objc_msgSend_fp2ret(id self, SEL op, ...)
  ///
  /// The messenger used when the return value is returned in two values on the
  /// x87 floating point stack; without a special entrypoint, the nil case
  /// would be unbalanced. Only used on 64-bit X86.
  llvm::Constant *getMessageSendFp2retFn() const {
    llvm::Type *params[] = { ObjectPtrTy, SelectorPtrTy };
    llvm::Type *longDoubleType = llvm::Type::getX86_FP80Ty(VMContext);
    llvm::Type *resultType = 
      llvm::StructType::get(longDoubleType, longDoubleType, nullptr);

    return CGM.CreateRuntimeFunction(llvm::FunctionType::get(resultType,
                                                             params, true),
                                     "objc_msgSend_fp2ret");
  }

  /// id objc_msgSendSuper(struct objc_super *super, SEL op, ...)
  ///
  /// The messenger used for super calls, which have different dispatch
  /// semantics.  The class passed is the superclass of the current
  /// class.
  llvm::Constant *getMessageSendSuperFn() const {
    llvm::Type *params[] = { SuperPtrTy, SelectorPtrTy };
    return CGM.CreateRuntimeFunction(llvm::FunctionType::get(ObjectPtrTy,
                                                             params, true),
                                     "objc_msgSendSuper");
  }

  /// id objc_msgSendSuper2(struct objc_super *super, SEL op, ...)
  ///
  /// A slightly different messenger used for super calls.  The class
  /// passed is the current class.
  llvm::Constant *getMessageSendSuperFn2() const {
    llvm::Type *params[] = { SuperPtrTy, SelectorPtrTy };
    return CGM.CreateRuntimeFunction(llvm::FunctionType::get(ObjectPtrTy,
                                                             params, true),
                                     "objc_msgSendSuper2");
  }

  /// void objc_msgSendSuper_stret(void *stretAddr, struct objc_super *super,
  ///                              SEL op, ...)
  ///
  /// The messenger used for super calls which return an aggregate indirectly.
  llvm::Constant *getMessageSendSuperStretFn() const {
    llvm::Type *params[] = { Int8PtrTy, SuperPtrTy, SelectorPtrTy };
    return CGM.CreateRuntimeFunction(
      llvm::FunctionType::get(CGM.VoidTy, params, true),
      "objc_msgSendSuper_stret");
  }

  /// void objc_msgSendSuper2_stret(void * stretAddr, struct objc_super *super,
  ///                               SEL op, ...)
  ///
  /// objc_msgSendSuper_stret with the super2 semantics.
  llvm::Constant *getMessageSendSuperStretFn2() const {
    llvm::Type *params[] = { Int8PtrTy, SuperPtrTy, SelectorPtrTy };
    return CGM.CreateRuntimeFunction(
      llvm::FunctionType::get(CGM.VoidTy, params, true),
      "objc_msgSendSuper2_stret");
  }

  llvm::Constant *getMessageSendSuperFpretFn() const {
    // There is no objc_msgSendSuper_fpret? How can that work?
    return getMessageSendSuperFn();
  }

  llvm::Constant *getMessageSendSuperFpretFn2() const {
    // There is no objc_msgSendSuper_fpret? How can that work?
    return getMessageSendSuperFn2();
  }

protected:
  CodeGen::CodeGenModule &CGM;

public:
  llvm::Type *ShortTy, *IntTy, *LongTy, *LongLongTy;
  llvm::Type *Int8PtrTy, *Int8PtrPtrTy;
  llvm::Type *IvarOffsetVarTy;

  /// ObjectPtrTy - LLVM type for object handles (typeof(id))
  llvm::Type *ObjectPtrTy;

  /// PtrObjectPtrTy - LLVM type for id *
  llvm::Type *PtrObjectPtrTy;

  /// SelectorPtrTy - LLVM type for selector handles (typeof(SEL))
  llvm::Type *SelectorPtrTy;
  
private:
  /// ProtocolPtrTy - LLVM type for external protocol handles
  /// (typeof(Protocol))
  llvm::Type *ExternalProtocolPtrTy;
  
public:
  llvm::Type *getExternalProtocolPtrTy() {
    if (!ExternalProtocolPtrTy) {
      // FIXME: It would be nice to unify this with the opaque type, so that the
      // IR comes out a bit cleaner.
      CodeGen::CodeGenTypes &Types = CGM.getTypes();
      ASTContext &Ctx = CGM.getContext();
      llvm::Type *T = Types.ConvertType(Ctx.getObjCProtoType());
      ExternalProtocolPtrTy = llvm::PointerType::getUnqual(T);
    }
    
    return ExternalProtocolPtrTy;
  }
  
  // SuperCTy - clang type for struct objc_super.
  QualType SuperCTy;
  // SuperPtrCTy - clang type for struct objc_super *.
  QualType SuperPtrCTy;

  /// SuperTy - LLVM type for struct objc_super.
  llvm::StructType *SuperTy;
  /// SuperPtrTy - LLVM type for struct objc_super *.
  llvm::Type *SuperPtrTy;

  /// PropertyTy - LLVM type for struct objc_property (struct _prop_t
  /// in GCC parlance).
  llvm::StructType *PropertyTy;

  /// PropertyListTy - LLVM type for struct objc_property_list
  /// (_prop_list_t in GCC parlance).
  llvm::StructType *PropertyListTy;
  /// PropertyListPtrTy - LLVM type for struct objc_property_list*.
  llvm::Type *PropertyListPtrTy;

  // MethodTy - LLVM type for struct objc_method.
  llvm::StructType *MethodTy;

  /// CacheTy - LLVM type for struct objc_cache.
  llvm::Type *CacheTy;
  /// CachePtrTy - LLVM type for struct objc_cache *.
  llvm::Type *CachePtrTy;
  
  llvm::Constant *getGetPropertyFn() {
    CodeGen::CodeGenTypes &Types = CGM.getTypes();
    ASTContext &Ctx = CGM.getContext();
    // id objc_getProperty (id, SEL, ptrdiff_t, bool)
    CanQualType IdType = Ctx.getCanonicalParamType(Ctx.getObjCIdType());
    CanQualType SelType = Ctx.getCanonicalParamType(Ctx.getObjCSelType());
    CanQualType Params[] = {
        IdType, SelType,
        Ctx.getPointerDiffType()->getCanonicalTypeUnqualified(), Ctx.BoolTy};
    llvm::FunctionType *FTy =
        Types.GetFunctionType(
          Types.arrangeBuiltinFunctionDeclaration(IdType, Params));
    return CGM.CreateRuntimeFunction(FTy, "objc_getProperty");
  }

  llvm::Constant *getSetPropertyFn() {
    CodeGen::CodeGenTypes &Types = CGM.getTypes();
    ASTContext &Ctx = CGM.getContext();
    // void objc_setProperty (id, SEL, ptrdiff_t, id, bool, bool)
    CanQualType IdType = Ctx.getCanonicalParamType(Ctx.getObjCIdType());
    CanQualType SelType = Ctx.getCanonicalParamType(Ctx.getObjCSelType());
    CanQualType Params[] = {
        IdType,
        SelType,
        Ctx.getPointerDiffType()->getCanonicalTypeUnqualified(),
        IdType,
        Ctx.BoolTy,
        Ctx.BoolTy};
    llvm::FunctionType *FTy =
        Types.GetFunctionType(
          Types.arrangeBuiltinFunctionDeclaration(Ctx.VoidTy, Params));
    return CGM.CreateRuntimeFunction(FTy, "objc_setProperty");
  }

  llvm::Constant *getOptimizedSetPropertyFn(bool atomic, bool copy) {
    CodeGen::CodeGenTypes &Types = CGM.getTypes();
    ASTContext &Ctx = CGM.getContext();
    // void objc_setProperty_atomic(id self, SEL _cmd, 
    //                              id newValue, ptrdiff_t offset);
    // void objc_setProperty_nonatomic(id self, SEL _cmd, 
    //                                 id newValue, ptrdiff_t offset);
    // void objc_setProperty_atomic_copy(id self, SEL _cmd, 
    //                                   id newValue, ptrdiff_t offset);
    // void objc_setProperty_nonatomic_copy(id self, SEL _cmd, 
    //                                      id newValue, ptrdiff_t offset);
    
    SmallVector<CanQualType,4> Params;
    CanQualType IdType = Ctx.getCanonicalParamType(Ctx.getObjCIdType());
    CanQualType SelType = Ctx.getCanonicalParamType(Ctx.getObjCSelType());
    Params.push_back(IdType);
    Params.push_back(SelType);
    Params.push_back(IdType);
    Params.push_back(Ctx.getPointerDiffType()->getCanonicalTypeUnqualified());
    llvm::FunctionType *FTy =
        Types.GetFunctionType(
          Types.arrangeBuiltinFunctionDeclaration(Ctx.VoidTy, Params));
    const char *name;
    if (atomic && copy)
      name = "objc_setProperty_atomic_copy";
    else if (atomic && !copy)
      name = "objc_setProperty_atomic";
    else if (!atomic && copy)
      name = "objc_setProperty_nonatomic_copy";
    else
      name = "objc_setProperty_nonatomic";
      
    return CGM.CreateRuntimeFunction(FTy, name);
  }
  
  llvm::Constant *getCopyStructFn() {
    CodeGen::CodeGenTypes &Types = CGM.getTypes();
    ASTContext &Ctx = CGM.getContext();
    // void objc_copyStruct (void *, const void *, size_t, bool, bool)
    SmallVector<CanQualType,5> Params;
    Params.push_back(Ctx.VoidPtrTy);
    Params.push_back(Ctx.VoidPtrTy);
    Params.push_back(Ctx.LongTy);
    Params.push_back(Ctx.BoolTy);
    Params.push_back(Ctx.BoolTy);
    llvm::FunctionType *FTy =
        Types.GetFunctionType(
          Types.arrangeBuiltinFunctionDeclaration(Ctx.VoidTy, Params));
    return CGM.CreateRuntimeFunction(FTy, "objc_copyStruct");
  }
  
  /// This routine declares and returns address of:
  /// void objc_copyCppObjectAtomic(
  ///         void *dest, const void *src, 
  ///         void (*copyHelper) (void *dest, const void *source));
  llvm::Constant *getCppAtomicObjectFunction() {
    CodeGen::CodeGenTypes &Types = CGM.getTypes();
    ASTContext &Ctx = CGM.getContext();
    /// void objc_copyCppObjectAtomic(void *dest, const void *src, void *helper);
    SmallVector<CanQualType,3> Params;
    Params.push_back(Ctx.VoidPtrTy);
    Params.push_back(Ctx.VoidPtrTy);
    Params.push_back(Ctx.VoidPtrTy);
    llvm::FunctionType *FTy =
        Types.GetFunctionType(
          Types.arrangeBuiltinFunctionDeclaration(Ctx.VoidTy, Params));
    return CGM.CreateRuntimeFunction(FTy, "objc_copyCppObjectAtomic");
  }
  
  llvm::Constant *getEnumerationMutationFn() {
    CodeGen::CodeGenTypes &Types = CGM.getTypes();
    ASTContext &Ctx = CGM.getContext();
    // void objc_enumerationMutation (id)
    SmallVector<CanQualType,1> Params;
    Params.push_back(Ctx.getCanonicalParamType(Ctx.getObjCIdType()));
    llvm::FunctionType *FTy =
        Types.GetFunctionType(
          Types.arrangeBuiltinFunctionDeclaration(Ctx.VoidTy, Params));
    return CGM.CreateRuntimeFunction(FTy, "objc_enumerationMutation");
  }

  llvm::Constant *getLookUpClassFn() {
    CodeGen::CodeGenTypes &Types = CGM.getTypes();
    ASTContext &Ctx = CGM.getContext();
    // Class objc_lookUpClass (const char *)
    SmallVector<CanQualType,1> Params;
    Params.push_back(
      Ctx.getCanonicalType(Ctx.getPointerType(Ctx.CharTy.withConst())));
    llvm::FunctionType *FTy =
        Types.GetFunctionType(Types.arrangeBuiltinFunctionDeclaration(
                                Ctx.getCanonicalType(Ctx.getObjCClassType()),
                                Params));
    return CGM.CreateRuntimeFunction(FTy, "objc_lookUpClass");
  }

  /// GcReadWeakFn -- LLVM objc_read_weak (id *src) function.
  llvm::Constant *getGcReadWeakFn() {
    // id objc_read_weak (id *)
    llvm::Type *args[] = { ObjectPtrTy->getPointerTo() };
    llvm::FunctionType *FTy =
      llvm::FunctionType::get(ObjectPtrTy, args, false);
    return CGM.CreateRuntimeFunction(FTy, "objc_read_weak");
  }

  /// GcAssignWeakFn -- LLVM objc_assign_weak function.
  llvm::Constant *getGcAssignWeakFn() {
    // id objc_assign_weak (id, id *)
    llvm::Type *args[] = { ObjectPtrTy, ObjectPtrTy->getPointerTo() };
    llvm::FunctionType *FTy =
      llvm::FunctionType::get(ObjectPtrTy, args, false);
    return CGM.CreateRuntimeFunction(FTy, "objc_assign_weak");
  }

  /// GcAssignGlobalFn -- LLVM objc_assign_global function.
  llvm::Constant *getGcAssignGlobalFn() {
    // id objc_assign_global(id, id *)
    llvm::Type *args[] = { ObjectPtrTy, ObjectPtrTy->getPointerTo() };
    llvm::FunctionType *FTy =
      llvm::FunctionType::get(ObjectPtrTy, args, false);
    return CGM.CreateRuntimeFunction(FTy, "objc_assign_global");
  }

  /// GcAssignThreadLocalFn -- LLVM objc_assign_threadlocal function.
  llvm::Constant *getGcAssignThreadLocalFn() {
    // id objc_assign_threadlocal(id src, id * dest)
    llvm::Type *args[] = { ObjectPtrTy, ObjectPtrTy->getPointerTo() };
    llvm::FunctionType *FTy =
      llvm::FunctionType::get(ObjectPtrTy, args, false);
    return CGM.CreateRuntimeFunction(FTy, "objc_assign_threadlocal");
  }
  
  /// GcAssignIvarFn -- LLVM objc_assign_ivar function.
  llvm::Constant *getGcAssignIvarFn() {
    // id objc_assign_ivar(id, id *, ptrdiff_t)
    llvm::Type *args[] = { ObjectPtrTy, ObjectPtrTy->getPointerTo(),
                           CGM.PtrDiffTy };
    llvm::FunctionType *FTy =
      llvm::FunctionType::get(ObjectPtrTy, args, false);
    return CGM.CreateRuntimeFunction(FTy, "objc_assign_ivar");
  }

  /// GcMemmoveCollectableFn -- LLVM objc_memmove_collectable function.
  llvm::Constant *GcMemmoveCollectableFn() {
    // void *objc_memmove_collectable(void *dst, const void *src, size_t size)
    llvm::Type *args[] = { Int8PtrTy, Int8PtrTy, LongTy };
    llvm::FunctionType *FTy = llvm::FunctionType::get(Int8PtrTy, args, false);
    return CGM.CreateRuntimeFunction(FTy, "objc_memmove_collectable");
  }

  /// GcAssignStrongCastFn -- LLVM objc_assign_strongCast function.
  llvm::Constant *getGcAssignStrongCastFn() {
    // id objc_assign_strongCast(id, id *)
    llvm::Type *args[] = { ObjectPtrTy, ObjectPtrTy->getPointerTo() };
    llvm::FunctionType *FTy =
      llvm::FunctionType::get(ObjectPtrTy, args, false);
    return CGM.CreateRuntimeFunction(FTy, "objc_assign_strongCast");
  }

  /// ExceptionThrowFn - LLVM objc_exception_throw function.
  llvm::Constant *getExceptionThrowFn() {
    // void objc_exception_throw(id)
    llvm::Type *args[] = { ObjectPtrTy };
    llvm::FunctionType *FTy =
      llvm::FunctionType::get(CGM.VoidTy, args, false);
    return CGM.CreateRuntimeFunction(FTy, "objc_exception_throw");
  }

  /// ExceptionRethrowFn - LLVM objc_exception_rethrow function.
  llvm::Constant *getExceptionRethrowFn() {
    // void objc_exception_rethrow(void)
    llvm::FunctionType *FTy = llvm::FunctionType::get(CGM.VoidTy, false);
    return CGM.CreateRuntimeFunction(FTy, "objc_exception_rethrow");
  }
  
  /// SyncEnterFn - LLVM object_sync_enter function.
  llvm::Constant *getSyncEnterFn() {
    // int objc_sync_enter (id)
    llvm::Type *args[] = { ObjectPtrTy };
    llvm::FunctionType *FTy =
      llvm::FunctionType::get(CGM.IntTy, args, false);
    return CGM.CreateRuntimeFunction(FTy, "objc_sync_enter");
  }

  /// SyncExitFn - LLVM object_sync_exit function.
  llvm::Constant *getSyncExitFn() {
    // int objc_sync_exit (id)
    llvm::Type *args[] = { ObjectPtrTy };
    llvm::FunctionType *FTy =
      llvm::FunctionType::get(CGM.IntTy, args, false);
    return CGM.CreateRuntimeFunction(FTy, "objc_sync_exit");
  }

  llvm::Constant *getSendFn(bool IsSuper) const {
    return IsSuper ? getMessageSendSuperFn() : getMessageSendFn();
  }

  llvm::Constant *getSendFn2(bool IsSuper) const {
    return IsSuper ? getMessageSendSuperFn2() : getMessageSendFn();
  }

  llvm::Constant *getSendStretFn(bool IsSuper) const {
    return IsSuper ? getMessageSendSuperStretFn() : getMessageSendStretFn();
  }

  llvm::Constant *getSendStretFn2(bool IsSuper) const {
    return IsSuper ? getMessageSendSuperStretFn2() : getMessageSendStretFn();
  }

  llvm::Constant *getSendFpretFn(bool IsSuper) const {
    return IsSuper ? getMessageSendSuperFpretFn() : getMessageSendFpretFn();
  }

  llvm::Constant *getSendFpretFn2(bool IsSuper) const {
    return IsSuper ? getMessageSendSuperFpretFn2() : getMessageSendFpretFn();
  }

  llvm::Constant *getSendFp2retFn(bool IsSuper) const {
    return IsSuper ? getMessageSendSuperFn() : getMessageSendFp2retFn();
  }

  llvm::Constant *getSendFp2RetFn2(bool IsSuper) const {
    return IsSuper ? getMessageSendSuperFn2() : getMessageSendFp2retFn();
  }

  ObjCCommonTypesHelper(CodeGen::CodeGenModule &cgm);
};

/// ObjCTypesHelper - Helper class that encapsulates lazy
/// construction of varies types used during ObjC generation.
class ObjCTypesHelper : public ObjCCommonTypesHelper {
public:
  /// SymtabTy - LLVM type for struct objc_symtab.
  llvm::StructType *SymtabTy;
  /// SymtabPtrTy - LLVM type for struct objc_symtab *.
  llvm::Type *SymtabPtrTy;
  /// ModuleTy - LLVM type for struct objc_module.
  llvm::StructType *ModuleTy;

  /// ProtocolTy - LLVM type for struct objc_protocol.
  llvm::StructType *ProtocolTy;
  /// ProtocolPtrTy - LLVM type for struct objc_protocol *.
  llvm::Type *ProtocolPtrTy;
  /// ProtocolExtensionTy - LLVM type for struct
  /// objc_protocol_extension.
  llvm::StructType *ProtocolExtensionTy;
  /// ProtocolExtensionTy - LLVM type for struct
  /// objc_protocol_extension *.
  llvm::Type *ProtocolExtensionPtrTy;
  /// MethodDescriptionTy - LLVM type for struct
  /// objc_method_description.
  llvm::StructType *MethodDescriptionTy;
  /// MethodDescriptionListTy - LLVM type for struct
  /// objc_method_description_list.
  llvm::StructType *MethodDescriptionListTy;
  /// MethodDescriptionListPtrTy - LLVM type for struct
  /// objc_method_description_list *.
  llvm::Type *MethodDescriptionListPtrTy;
  /// ProtocolListTy - LLVM type for struct objc_property_list.
  llvm::StructType *ProtocolListTy;
  /// ProtocolListPtrTy - LLVM type for struct objc_property_list*.
  llvm::Type *ProtocolListPtrTy;
  /// CategoryTy - LLVM type for struct objc_category.
  llvm::StructType *CategoryTy;
  /// ClassTy - LLVM type for struct objc_class.
  llvm::StructType *ClassTy;
  /// ClassPtrTy - LLVM type for struct objc_class *.
  llvm::Type *ClassPtrTy;
  /// ClassExtensionTy - LLVM type for struct objc_class_ext.
  llvm::StructType *ClassExtensionTy;
  /// ClassExtensionPtrTy - LLVM type for struct objc_class_ext *.
  llvm::Type *ClassExtensionPtrTy;
  // IvarTy - LLVM type for struct objc_ivar.
  llvm::StructType *IvarTy;
  /// IvarListTy - LLVM type for struct objc_ivar_list.
  llvm::Type *IvarListTy;
  /// IvarListPtrTy - LLVM type for struct objc_ivar_list *.
  llvm::Type *IvarListPtrTy;
  /// MethodListTy - LLVM type for struct objc_method_list.
  llvm::Type *MethodListTy;
  /// MethodListPtrTy - LLVM type for struct objc_method_list *.
  llvm::Type *MethodListPtrTy;

  /// ExceptionDataTy - LLVM type for struct _objc_exception_data.
  llvm::Type *ExceptionDataTy;
  
  /// ExceptionTryEnterFn - LLVM objc_exception_try_enter function.
  llvm::Constant *getExceptionTryEnterFn() {
    llvm::Type *params[] = { ExceptionDataTy->getPointerTo() };
    return CGM.CreateRuntimeFunction(
      llvm::FunctionType::get(CGM.VoidTy, params, false),
      "objc_exception_try_enter");
  }

  /// ExceptionTryExitFn - LLVM objc_exception_try_exit function.
  llvm::Constant *getExceptionTryExitFn() {
    llvm::Type *params[] = { ExceptionDataTy->getPointerTo() };
    return CGM.CreateRuntimeFunction(
      llvm::FunctionType::get(CGM.VoidTy, params, false),
      "objc_exception_try_exit");
  }

  /// ExceptionExtractFn - LLVM objc_exception_extract function.
  llvm::Constant *getExceptionExtractFn() {
    llvm::Type *params[] = { ExceptionDataTy->getPointerTo() };
    return CGM.CreateRuntimeFunction(llvm::FunctionType::get(ObjectPtrTy,
                                                             params, false),
                                     "objc_exception_extract");
  }

  /// ExceptionMatchFn - LLVM objc_exception_match function.
  llvm::Constant *getExceptionMatchFn() {
    llvm::Type *params[] = { ClassPtrTy, ObjectPtrTy };
    return CGM.CreateRuntimeFunction(
      llvm::FunctionType::get(CGM.Int32Ty, params, false),
      "objc_exception_match");
  }

  /// SetJmpFn - LLVM _setjmp function.
  llvm::Constant *getSetJmpFn() {
    // This is specifically the prototype for x86.
    llvm::Type *params[] = { CGM.Int32Ty->getPointerTo() };
    return
      CGM.CreateRuntimeFunction(llvm::FunctionType::get(CGM.Int32Ty,
                                                        params, false),
                                "_setjmp",
                                llvm::AttributeSet::get(CGM.getLLVMContext(),
                                              llvm::AttributeSet::FunctionIndex,
                                                 llvm::Attribute::NonLazyBind));
  }

public:
  ObjCTypesHelper(CodeGen::CodeGenModule &cgm);
};

/// ObjCNonFragileABITypesHelper - will have all types needed by objective-c's
/// modern abi
class ObjCNonFragileABITypesHelper : public ObjCCommonTypesHelper {
public:
  // MethodListnfABITy - LLVM for struct _method_list_t
  llvm::StructType *MethodListnfABITy;

  // MethodListnfABIPtrTy - LLVM for struct _method_list_t*
  llvm::Type *MethodListnfABIPtrTy;

  // ProtocolnfABITy = LLVM for struct _protocol_t
  llvm::StructType *ProtocolnfABITy;

  // ProtocolnfABIPtrTy = LLVM for struct _protocol_t*
  llvm::Type *ProtocolnfABIPtrTy;

  // ProtocolListnfABITy - LLVM for struct _objc_protocol_list
  llvm::StructType *ProtocolListnfABITy;

  // ProtocolListnfABIPtrTy - LLVM for struct _objc_protocol_list*
  llvm::Type *ProtocolListnfABIPtrTy;

  // ClassnfABITy - LLVM for struct _class_t
  llvm::StructType *ClassnfABITy;

  // ClassnfABIPtrTy - LLVM for struct _class_t*
  llvm::Type *ClassnfABIPtrTy;

  // IvarnfABITy - LLVM for struct _ivar_t
  llvm::StructType *IvarnfABITy;

  // IvarListnfABITy - LLVM for struct _ivar_list_t
  llvm::StructType *IvarListnfABITy;

  // IvarListnfABIPtrTy = LLVM for struct _ivar_list_t*
  llvm::Type *IvarListnfABIPtrTy;

  // ClassRonfABITy - LLVM for struct _class_ro_t
  llvm::StructType *ClassRonfABITy;

  // ImpnfABITy - LLVM for id (*)(id, SEL, ...)
  llvm::Type *ImpnfABITy;

  // CategorynfABITy - LLVM for struct _category_t
  llvm::StructType *CategorynfABITy;

  // New types for nonfragile abi messaging.

  // MessageRefTy - LLVM for:
  // struct _message_ref_t {
  //   IMP messenger;
  //   SEL name;
  // };
  llvm::StructType *MessageRefTy;
  // MessageRefCTy - clang type for struct _message_ref_t
  QualType MessageRefCTy;

  // MessageRefPtrTy - LLVM for struct _message_ref_t*
  llvm::Type *MessageRefPtrTy;
  // MessageRefCPtrTy - clang type for struct _message_ref_t*
  QualType MessageRefCPtrTy;

  // SuperMessageRefTy - LLVM for:
  // struct _super_message_ref_t {
  //   SUPER_IMP messenger;
  //   SEL name;
  // };
  llvm::StructType *SuperMessageRefTy;

  // SuperMessageRefPtrTy - LLVM for struct _super_message_ref_t*
  llvm::Type *SuperMessageRefPtrTy;

  llvm::Constant *getMessageSendFixupFn() {
    // id objc_msgSend_fixup(id, struct message_ref_t*, ...)
    llvm::Type *params[] = { ObjectPtrTy, MessageRefPtrTy };
    return CGM.CreateRuntimeFunction(llvm::FunctionType::get(ObjectPtrTy,
                                                             params, true),
                                     "objc_msgSend_fixup");
  }

  llvm::Constant *getMessageSendFpretFixupFn() {
    // id objc_msgSend_fpret_fixup(id, struct message_ref_t*, ...)
    llvm::Type *params[] = { ObjectPtrTy, MessageRefPtrTy };
    return CGM.CreateRuntimeFunction(llvm::FunctionType::get(ObjectPtrTy,
                                                             params, true),
                                     "objc_msgSend_fpret_fixup");
  }

  llvm::Constant *getMessageSendStretFixupFn() {
    // id objc_msgSend_stret_fixup(id, struct message_ref_t*, ...)
    llvm::Type *params[] = { ObjectPtrTy, MessageRefPtrTy };
    return CGM.CreateRuntimeFunction(llvm::FunctionType::get(ObjectPtrTy,
                                                             params, true),
                                     "objc_msgSend_stret_fixup");
  }

  llvm::Constant *getMessageSendSuper2FixupFn() {
    // id objc_msgSendSuper2_fixup (struct objc_super *,
    //                              struct _super_message_ref_t*, ...)
    llvm::Type *params[] = { SuperPtrTy, SuperMessageRefPtrTy };
    return  CGM.CreateRuntimeFunction(llvm::FunctionType::get(ObjectPtrTy,
                                                              params, true),
                                      "objc_msgSendSuper2_fixup");
  }

  llvm::Constant *getMessageSendSuper2StretFixupFn() {
    // id objc_msgSendSuper2_stret_fixup(struct objc_super *,
    //                                   struct _super_message_ref_t*, ...)
    llvm::Type *params[] = { SuperPtrTy, SuperMessageRefPtrTy };
    return  CGM.CreateRuntimeFunction(llvm::FunctionType::get(ObjectPtrTy,
                                                              params, true),
                                      "objc_msgSendSuper2_stret_fixup");
  }

  llvm::Constant *getObjCEndCatchFn() {
    return CGM.CreateRuntimeFunction(llvm::FunctionType::get(CGM.VoidTy, false),
                                     "objc_end_catch");

  }

  llvm::Constant *getObjCBeginCatchFn() {
    llvm::Type *params[] = { Int8PtrTy };
    return CGM.CreateRuntimeFunction(llvm::FunctionType::get(Int8PtrTy,
                                                             params, false),
                                     "objc_begin_catch");
  }

  llvm::StructType *EHTypeTy;
  llvm::Type *EHTypePtrTy;
  
  ObjCNonFragileABITypesHelper(CodeGen::CodeGenModule &cgm);
};

class CGObjCCommonMac : public CodeGen::CGObjCRuntime {
public:
  class SKIP_SCAN {
  public:
    unsigned skip;
    unsigned scan;
    SKIP_SCAN(unsigned _skip = 0, unsigned _scan = 0)
      : skip(_skip), scan(_scan) {}
  };

  /// opcode for captured block variables layout 'instructions'.
  /// In the following descriptions, 'I' is the value of the immediate field.
  /// (field following the opcode).
  ///
  enum BLOCK_LAYOUT_OPCODE {
    /// An operator which affects how the following layout should be
    /// interpreted.
    ///   I == 0: Halt interpretation and treat everything else as
    ///           a non-pointer.  Note that this instruction is equal
    ///           to '\0'.
    ///   I != 0: Currently unused.
    BLOCK_LAYOUT_OPERATOR            = 0,
    
    /// The next I+1 bytes do not contain a value of object pointer type.
    /// Note that this can leave the stream unaligned, meaning that
    /// subsequent word-size instructions do not begin at a multiple of
    /// the pointer size.
    BLOCK_LAYOUT_NON_OBJECT_BYTES    = 1,
    
    /// The next I+1 words do not contain a value of object pointer type.
    /// This is simply an optimized version of BLOCK_LAYOUT_BYTES for
    /// when the required skip quantity is a multiple of the pointer size.
    BLOCK_LAYOUT_NON_OBJECT_WORDS    = 2,
    
    /// The next I+1 words are __strong pointers to Objective-C
    /// objects or blocks.
    BLOCK_LAYOUT_STRONG              = 3,
    
    /// The next I+1 words are pointers to __block variables.
    BLOCK_LAYOUT_BYREF               = 4,
    
    /// The next I+1 words are __weak pointers to Objective-C
    /// objects or blocks.
    BLOCK_LAYOUT_WEAK                = 5,
    
    /// The next I+1 words are __unsafe_unretained pointers to
    /// Objective-C objects or blocks.
    BLOCK_LAYOUT_UNRETAINED          = 6
    
    /// The next I+1 words are block or object pointers with some
    /// as-yet-unspecified ownership semantics.  If we add more
    /// flavors of ownership semantics, values will be taken from
    /// this range.
    ///
    /// This is included so that older tools can at least continue
    /// processing the layout past such things.
    //BLOCK_LAYOUT_OWNERSHIP_UNKNOWN = 7..10,
    
    /// All other opcodes are reserved.  Halt interpretation and
    /// treat everything else as opaque.
  };
 
  class RUN_SKIP {
  public:
    enum BLOCK_LAYOUT_OPCODE opcode;
    CharUnits block_var_bytepos;
    CharUnits block_var_size;
    RUN_SKIP(enum BLOCK_LAYOUT_OPCODE Opcode = BLOCK_LAYOUT_OPERATOR,
             CharUnits BytePos = CharUnits::Zero(),
             CharUnits Size = CharUnits::Zero())
    : opcode(Opcode), block_var_bytepos(BytePos),  block_var_size(Size) {}
    
    // Allow sorting based on byte pos.
    bool operator<(const RUN_SKIP &b) const {
      return block_var_bytepos < b.block_var_bytepos;
    }
  };
  
protected:
  llvm::LLVMContext &VMContext;
  // FIXME! May not be needing this after all.
  unsigned ObjCABI;

  // arc/mrr layout of captured block literal variables.
  SmallVector<RUN_SKIP, 16> RunSkipBlockVars;

  /// LazySymbols - Symbols to generate a lazy reference for. See
  /// DefinedSymbols and FinishModule().
  llvm::SetVector<IdentifierInfo*> LazySymbols;

  /// DefinedSymbols - External symbols which are defined by this
  /// module. The symbols in this list and LazySymbols are used to add
  /// special linker symbols which ensure that Objective-C modules are
  /// linked properly.
  llvm::SetVector<IdentifierInfo*> DefinedSymbols;

  /// ClassNames - uniqued class names.
  llvm::StringMap<llvm::GlobalVariable*> ClassNames;

  /// MethodVarNames - uniqued method variable names.
  llvm::DenseMap<Selector, llvm::GlobalVariable*> MethodVarNames;

  /// DefinedCategoryNames - list of category names in form Class_Category.
  llvm::SmallSetVector<std::string, 16> DefinedCategoryNames;

  /// MethodVarTypes - uniqued method type signatures. We have to use
  /// a StringMap here because have no other unique reference.
  llvm::StringMap<llvm::GlobalVariable*> MethodVarTypes;

  /// MethodDefinitions - map of methods which have been defined in
  /// this translation unit.
  llvm::DenseMap<const ObjCMethodDecl*, llvm::Function*> MethodDefinitions;

  /// PropertyNames - uniqued method variable names.
  llvm::DenseMap<IdentifierInfo*, llvm::GlobalVariable*> PropertyNames;

  /// ClassReferences - uniqued class references.
  llvm::DenseMap<IdentifierInfo*, llvm::GlobalVariable*> ClassReferences;

  /// SelectorReferences - uniqued selector references.
  llvm::DenseMap<Selector, llvm::GlobalVariable*> SelectorReferences;

  /// Protocols - Protocols for which an objc_protocol structure has
  /// been emitted. Forward declarations are handled by creating an
  /// empty structure whose initializer is filled in when/if defined.
  llvm::DenseMap<IdentifierInfo*, llvm::GlobalVariable*> Protocols;

  /// DefinedProtocols - Protocols which have actually been
  /// defined. We should not need this, see FIXME in GenerateProtocol.
  llvm::DenseSet<IdentifierInfo*> DefinedProtocols;

  /// DefinedClasses - List of defined classes.
  SmallVector<llvm::GlobalValue*, 16> DefinedClasses;
  
  /// ImplementedClasses - List of @implemented classes.
  SmallVector<const ObjCInterfaceDecl*, 16> ImplementedClasses;

  /// DefinedNonLazyClasses - List of defined "non-lazy" classes.
  SmallVector<llvm::GlobalValue*, 16> DefinedNonLazyClasses;

  /// DefinedCategories - List of defined categories.
  SmallVector<llvm::GlobalValue*, 16> DefinedCategories;

  /// DefinedNonLazyCategories - List of defined "non-lazy" categories.
  SmallVector<llvm::GlobalValue*, 16> DefinedNonLazyCategories;

  /// GetNameForMethod - Return a name for the given method.
  /// \param[out] NameOut - The return value.
  void GetNameForMethod(const ObjCMethodDecl *OMD,
                        const ObjCContainerDecl *CD,
                        SmallVectorImpl<char> &NameOut);

  /// GetMethodVarName - Return a unique constant for the given
  /// selector's name. The return value has type char *.
  llvm::Constant *GetMethodVarName(Selector Sel);
  llvm::Constant *GetMethodVarName(IdentifierInfo *Ident);

  /// GetMethodVarType - Return a unique constant for the given
  /// method's type encoding string. The return value has type char *.

  // FIXME: This is a horrible name.
  llvm::Constant *GetMethodVarType(const ObjCMethodDecl *D,
                                   bool Extended = false);
  llvm::Constant *GetMethodVarType(const FieldDecl *D);

  /// GetPropertyName - Return a unique constant for the given
  /// name. The return value has type char *.
  llvm::Constant *GetPropertyName(IdentifierInfo *Ident);

  // FIXME: This can be dropped once string functions are unified.
  llvm::Constant *GetPropertyTypeString(const ObjCPropertyDecl *PD,
                                        const Decl *Container);

  /// GetClassName - Return a unique constant for the given selector's
  /// runtime name (which may change via use of objc_runtime_name attribute on
  /// class or protocol definition. The return value has type char *.
  llvm::Constant *GetClassName(StringRef RuntimeName);

  llvm::Function *GetMethodDefinition(const ObjCMethodDecl *MD);

  /// BuildIvarLayout - Builds ivar layout bitmap for the class
  /// implementation for the __strong or __weak case.
  ///
  /// \param hasMRCWeakIvars - Whether we are compiling in MRC and there
  ///   are any weak ivars defined directly in the class.  Meaningless unless
  ///   building a weak layout.  Does not guarantee that the layout will
  ///   actually have any entries, because the ivar might be under-aligned.
  llvm::Constant *BuildIvarLayout(const ObjCImplementationDecl *OI,
                                  CharUnits beginOffset,
                                  CharUnits endOffset,
                                  bool forStrongLayout,
                                  bool hasMRCWeakIvars);

  llvm::Constant *BuildStrongIvarLayout(const ObjCImplementationDecl *OI,
                                        CharUnits beginOffset,
                                        CharUnits endOffset) {
    return BuildIvarLayout(OI, beginOffset, endOffset, true, false);
  }

  llvm::Constant *BuildWeakIvarLayout(const ObjCImplementationDecl *OI,
                                      CharUnits beginOffset,
                                      CharUnits endOffset,
                                      bool hasMRCWeakIvars) {
    return BuildIvarLayout(OI, beginOffset, endOffset, false, hasMRCWeakIvars);
  }
  
  Qualifiers::ObjCLifetime getBlockCaptureLifetime(QualType QT, bool ByrefLayout);
  
  void UpdateRunSkipBlockVars(bool IsByref,
                              Qualifiers::ObjCLifetime LifeTime,
                              CharUnits FieldOffset,
                              CharUnits FieldSize);
  
  void BuildRCBlockVarRecordLayout(const RecordType *RT,
                                   CharUnits BytePos, bool &HasUnion,
                                   bool ByrefLayout=false);
  
  void BuildRCRecordLayout(const llvm::StructLayout *RecLayout,
                           const RecordDecl *RD,
                           ArrayRef<const FieldDecl*> RecFields,
                           CharUnits BytePos, bool &HasUnion,
                           bool ByrefLayout);
  
  uint64_t InlineLayoutInstruction(SmallVectorImpl<unsigned char> &Layout);
  
  llvm::Constant *getBitmapBlockLayout(bool ComputeByrefLayout);
  
  /// GetIvarLayoutName - Returns a unique constant for the given
  /// ivar layout bitmap.
  llvm::Constant *GetIvarLayoutName(IdentifierInfo *Ident,
                                    const ObjCCommonTypesHelper &ObjCTypes);

  /// EmitPropertyList - Emit the given property list. The return
  /// value has type PropertyListPtrTy.
  llvm::Constant *EmitPropertyList(Twine Name,
                                   const Decl *Container,
                                   const ObjCContainerDecl *OCD,
                                   const ObjCCommonTypesHelper &ObjCTypes,
                                   bool IsClassProperty);

  /// EmitProtocolMethodTypes - Generate the array of extended method type 
  /// strings. The return value has type Int8PtrPtrTy.
  llvm::Constant *EmitProtocolMethodTypes(Twine Name, 
                                          ArrayRef<llvm::Constant*> MethodTypes,
                                       const ObjCCommonTypesHelper &ObjCTypes);

  /// PushProtocolProperties - Push protocol's property on the input stack.
  void PushProtocolProperties(
    llvm::SmallPtrSet<const IdentifierInfo*, 16> &PropertySet,
    SmallVectorImpl<llvm::Constant*> &Properties,
    const Decl *Container,
    const ObjCProtocolDecl *Proto,
    const ObjCCommonTypesHelper &ObjCTypes,
    bool IsClassProperty);

  /// GetProtocolRef - Return a reference to the internal protocol
  /// description, creating an empty one if it has not been
  /// defined. The return value has type ProtocolPtrTy.
  llvm::Constant *GetProtocolRef(const ObjCProtocolDecl *PD);

  /// Return a reference to the given Class using runtime calls rather than
  /// by a symbol reference.
  llvm::Value *EmitClassRefViaRuntime(CodeGenFunction &CGF,
                                      const ObjCInterfaceDecl *ID,
                                      ObjCCommonTypesHelper &ObjCTypes);

public:
  /// CreateMetadataVar - Create a global variable with internal
  /// linkage for use by the Objective-C runtime.
  ///
  /// This is a convenience wrapper which not only creates the
  /// variable, but also sets the section and alignment and adds the
  /// global to the "llvm.used" list.
  ///
  /// \param Name - The variable name.
  /// \param Init - The variable initializer; this is also used to
  /// define the type of the variable.
  /// \param Section - The section the variable should go into, or empty.
  /// \param Align - The alignment for the variable, or 0.
  /// \param AddToUsed - Whether the variable should be added to
  /// "llvm.used".
  llvm::GlobalVariable *CreateMetadataVar(Twine Name, llvm::Constant *Init,
                                          StringRef Section, CharUnits Align,
                                          bool AddToUsed);

protected:
  CodeGen::RValue EmitMessageSend(CodeGen::CodeGenFunction &CGF,
                                  ReturnValueSlot Return,
                                  QualType ResultType,
                                  llvm::Value *Sel,
                                  llvm::Value *Arg0,
                                  QualType Arg0Ty,
                                  bool IsSuper,
                                  const CallArgList &CallArgs,
                                  const ObjCMethodDecl *OMD,
                                  const ObjCInterfaceDecl *ClassReceiver,
                                  const ObjCCommonTypesHelper &ObjCTypes);

  /// EmitImageInfo - Emit the image info marker used to encode some module
  /// level information.
  void EmitImageInfo();

public:
  CGObjCCommonMac(CodeGen::CodeGenModule &cgm) :
    CGObjCRuntime(cgm), VMContext(cgm.getLLVMContext()) { }

  bool isNonFragileABI() const {
    return ObjCABI == 2;
  }

  ConstantAddress GenerateConstantString(const StringLiteral *SL) override;

  llvm::Function *GenerateMethod(const ObjCMethodDecl *OMD,
                                 const ObjCContainerDecl *CD=nullptr) override;

  void GenerateProtocol(const ObjCProtocolDecl *PD) override;

  /// GetOrEmitProtocol - Get the protocol object for the given
  /// declaration, emitting it if necessary. The return value has type
  /// ProtocolPtrTy.
  virtual llvm::Constant *GetOrEmitProtocol(const ObjCProtocolDecl *PD)=0;

  /// GetOrEmitProtocolRef - Get a forward reference to the protocol
  /// object for the given declaration, emitting it if needed. These
  /// forward references will be filled in with empty bodies if no
  /// definition is seen. The return value has type ProtocolPtrTy.
  virtual llvm::Constant *GetOrEmitProtocolRef(const ObjCProtocolDecl *PD)=0;
  llvm::Constant *BuildGCBlockLayout(CodeGen::CodeGenModule &CGM,
                                     const CGBlockInfo &blockInfo) override;
  llvm::Constant *BuildRCBlockLayout(CodeGen::CodeGenModule &CGM,
                                     const CGBlockInfo &blockInfo) override;

  llvm::Constant *BuildByrefLayout(CodeGen::CodeGenModule &CGM,
                                   QualType T) override;
};

class CGObjCMac : public CGObjCCommonMac {
private:
  ObjCTypesHelper ObjCTypes;

  /// EmitModuleInfo - Another marker encoding module level
  /// information.
  void EmitModuleInfo();

  /// EmitModuleSymols - Emit module symbols, the list of defined
  /// classes and categories. The result has type SymtabPtrTy.
  llvm::Constant *EmitModuleSymbols();

  /// FinishModule - Write out global data structures at the end of
  /// processing a translation unit.
  void FinishModule();

  /// EmitClassExtension - Generate the class extension structure used
  /// to store the weak ivar layout and properties. The return value
  /// has type ClassExtensionPtrTy.
  llvm::Constant *EmitClassExtension(const ObjCImplementationDecl *ID,
                                     CharUnits instanceSize,
                                     bool hasMRCWeakIvars,
                                     bool isClassProperty);

  /// EmitClassRef - Return a Value*, of type ObjCTypes.ClassPtrTy,
  /// for the given class.
  llvm::Value *EmitClassRef(CodeGenFunction &CGF,
                            const ObjCInterfaceDecl *ID);
  
  llvm::Value *EmitClassRefFromId(CodeGenFunction &CGF,
                                  IdentifierInfo *II);

  llvm::Value *EmitNSAutoreleasePoolClassRef(CodeGenFunction &CGF) override;

  /// EmitSuperClassRef - Emits reference to class's main metadata class.
  llvm::Value *EmitSuperClassRef(const ObjCInterfaceDecl *ID);

  /// EmitIvarList - Emit the ivar list for the given
  /// implementation. If ForClass is true the list of class ivars
  /// (i.e. metaclass ivars) is emitted, otherwise the list of
  /// interface ivars will be emitted. The return value has type
  /// IvarListPtrTy.
  llvm::Constant *EmitIvarList(const ObjCImplementationDecl *ID,
                               bool ForClass);

  /// EmitMetaClass - Emit a forward reference to the class structure
  /// for the metaclass of the given interface. The return value has
  /// type ClassPtrTy.
  llvm::Constant *EmitMetaClassRef(const ObjCInterfaceDecl *ID);

  /// EmitMetaClass - Emit a class structure for the metaclass of the
  /// given implementation. The return value has type ClassPtrTy.
  llvm::Constant *EmitMetaClass(const ObjCImplementationDecl *ID,
                                llvm::Constant *Protocols,
                                ArrayRef<llvm::Constant*> Methods);

  llvm::Constant *GetMethodConstant(const ObjCMethodDecl *MD);

  llvm::Constant *GetMethodDescriptionConstant(const ObjCMethodDecl *MD);

  /// EmitMethodList - Emit the method list for the given
  /// implementation. The return value has type MethodListPtrTy.
  llvm::Constant *EmitMethodList(Twine Name, StringRef Section,
                                 ArrayRef<llvm::Constant *> Methods);

  /// EmitMethodDescList - Emit a method description list for a list of
  /// method declarations.
  ///  - TypeName: The name for the type containing the methods.
  ///  - IsProtocol: True iff these methods are for a protocol.
  ///  - ClassMethds: True iff these are class methods.
  ///  - Required: When true, only "required" methods are
  ///    listed. Similarly, when false only "optional" methods are
  ///    listed. For classes this should always be true.
  ///  - begin, end: The method list to output.
  ///
  /// The return value has type MethodDescriptionListPtrTy.
  llvm::Constant *EmitMethodDescList(Twine Name, StringRef Section,
                                     ArrayRef<llvm::Constant *> Methods);

  /// GetOrEmitProtocol - Get the protocol object for the given
  /// declaration, emitting it if necessary. The return value has type
  /// ProtocolPtrTy.
  llvm::Constant *GetOrEmitProtocol(const ObjCProtocolDecl *PD) override;

  /// GetOrEmitProtocolRef - Get a forward reference to the protocol
  /// object for the given declaration, emitting it if needed. These
  /// forward references will be filled in with empty bodies if no
  /// definition is seen. The return value has type ProtocolPtrTy.
  llvm::Constant *GetOrEmitProtocolRef(const ObjCProtocolDecl *PD) override;

  /// EmitProtocolExtension - Generate the protocol extension
  /// structure used to store optional instance and class methods, and
  /// protocol properties. The return value has type
  /// ProtocolExtensionPtrTy.
  llvm::Constant *
  EmitProtocolExtension(const ObjCProtocolDecl *PD,
                        ArrayRef<llvm::Constant*> OptInstanceMethods,
                        ArrayRef<llvm::Constant*> OptClassMethods,
                        ArrayRef<llvm::Constant*> MethodTypesExt);

  /// EmitProtocolList - Generate the list of referenced
  /// protocols. The return value has type ProtocolListPtrTy.
  llvm::Constant *EmitProtocolList(Twine Name,
                                   ObjCProtocolDecl::protocol_iterator begin,
                                   ObjCProtocolDecl::protocol_iterator end);

  /// EmitSelector - Return a Value*, of type ObjCTypes.SelectorPtrTy,
  /// for the given selector.
  llvm::Value *EmitSelector(CodeGenFunction &CGF, Selector Sel);
  Address EmitSelectorAddr(CodeGenFunction &CGF, Selector Sel);

public:
  CGObjCMac(CodeGen::CodeGenModule &cgm);

  llvm::Function *ModuleInitFunction() override;

  CodeGen::RValue GenerateMessageSend(CodeGen::CodeGenFunction &CGF,
                                      ReturnValueSlot Return,
                                      QualType ResultType,
                                      Selector Sel, llvm::Value *Receiver,
                                      const CallArgList &CallArgs,
                                      const ObjCInterfaceDecl *Class,
                                      const ObjCMethodDecl *Method) override;

  CodeGen::RValue
  GenerateMessageSendSuper(CodeGen::CodeGenFunction &CGF,
                           ReturnValueSlot Return, QualType ResultType,
                           Selector Sel, const ObjCInterfaceDecl *Class,
                           bool isCategoryImpl, llvm::Value *Receiver,
                           bool IsClassMessage, const CallArgList &CallArgs,
                           const ObjCMethodDecl *Method) override;

  llvm::Value *GetClass(CodeGenFunction &CGF,
                        const ObjCInterfaceDecl *ID) override;

  llvm::Value *GetSelector(CodeGenFunction &CGF, Selector Sel) override;
  Address GetAddrOfSelector(CodeGenFunction &CGF, Selector Sel) override;

  /// The NeXT/Apple runtimes do not support typed selectors; just emit an
  /// untyped one.
  llvm::Value *GetSelector(CodeGenFunction &CGF,
                           const ObjCMethodDecl *Method) override;

  llvm::Constant *GetEHType(QualType T) override;

  void GenerateCategory(const ObjCCategoryImplDecl *CMD) override;

  void GenerateClass(const ObjCImplementationDecl *ClassDecl) override;

  void RegisterAlias(const ObjCCompatibleAliasDecl *OAD) override {}

  llvm::Value *GenerateProtocolRef(CodeGenFunction &CGF,
                                   const ObjCProtocolDecl *PD) override;

  llvm::Constant *GetPropertyGetFunction() override;
  llvm::Constant *GetPropertySetFunction() override;
  llvm::Constant *GetOptimizedPropertySetFunction(bool atomic,
                                                  bool copy) override;
  llvm::Constant *GetGetStructFunction() override;
  llvm::Constant *GetSetStructFunction() override;
  llvm::Constant *GetCppAtomicObjectGetFunction() override;
  llvm::Constant *GetCppAtomicObjectSetFunction() override;
  llvm::Constant *EnumerationMutationFunction() override;

  void EmitTryStmt(CodeGen::CodeGenFunction &CGF,
                   const ObjCAtTryStmt &S) override;
  void EmitSynchronizedStmt(CodeGen::CodeGenFunction &CGF,
                            const ObjCAtSynchronizedStmt &S) override;
  void EmitTryOrSynchronizedStmt(CodeGen::CodeGenFunction &CGF, const Stmt &S);
  void EmitThrowStmt(CodeGen::CodeGenFunction &CGF, const ObjCAtThrowStmt &S,
                     bool ClearInsertionPoint=true) override;
  llvm::Value * EmitObjCWeakRead(CodeGen::CodeGenFunction &CGF,
                                 Address AddrWeakObj) override;
  void EmitObjCWeakAssign(CodeGen::CodeGenFunction &CGF,
                          llvm::Value *src, Address dst) override;
  void EmitObjCGlobalAssign(CodeGen::CodeGenFunction &CGF,
                            llvm::Value *src, Address dest,
                            bool threadlocal = false) override;
  void EmitObjCIvarAssign(CodeGen::CodeGenFunction &CGF,
                          llvm::Value *src, Address dest,
                          llvm::Value *ivarOffset) override;
  void EmitObjCStrongCastAssign(CodeGen::CodeGenFunction &CGF,
                                llvm::Value *src, Address dest) override;
  void EmitGCMemmoveCollectable(CodeGen::CodeGenFunction &CGF,
                                Address dest, Address src,
                                llvm::Value *size) override;

  LValue EmitObjCValueForIvar(CodeGen::CodeGenFunction &CGF, QualType ObjectTy,
                              llvm::Value *BaseValue, const ObjCIvarDecl *Ivar,
                              unsigned CVRQualifiers) override;
  llvm::Value *EmitIvarOffset(CodeGen::CodeGenFunction &CGF,
                              const ObjCInterfaceDecl *Interface,
                              const ObjCIvarDecl *Ivar) override;

  /// GetClassGlobal - Return the global variable for the Objective-C
  /// class of the given name.
  llvm::GlobalVariable *GetClassGlobal(StringRef Name,
                                       bool Weak = false) override {
    llvm_unreachable("CGObjCMac::GetClassGlobal");
  }
};

class CGObjCNonFragileABIMac : public CGObjCCommonMac {
private:
  ObjCNonFragileABITypesHelper ObjCTypes;
  llvm::GlobalVariable* ObjCEmptyCacheVar;
  llvm::GlobalVariable* ObjCEmptyVtableVar;

  /// SuperClassReferences - uniqued super class references.
  llvm::DenseMap<IdentifierInfo*, llvm::GlobalVariable*> SuperClassReferences;

  /// MetaClassReferences - uniqued meta class references.
  llvm::DenseMap<IdentifierInfo*, llvm::GlobalVariable*> MetaClassReferences;

  /// EHTypeReferences - uniqued class ehtype references.
  llvm::DenseMap<IdentifierInfo*, llvm::GlobalVariable*> EHTypeReferences;

  /// VTableDispatchMethods - List of methods for which we generate
  /// vtable-based message dispatch.
  llvm::DenseSet<Selector> VTableDispatchMethods;

  /// DefinedMetaClasses - List of defined meta-classes.
  std::vector<llvm::GlobalValue*> DefinedMetaClasses;
  
  /// isVTableDispatchedSelector - Returns true if SEL is a
  /// vtable-based selector.
  bool isVTableDispatchedSelector(Selector Sel);

  /// FinishNonFragileABIModule - Write out global data structures at the end of
  /// processing a translation unit.
  void FinishNonFragileABIModule();

  /// AddModuleClassList - Add the given list of class pointers to the
  /// module with the provided symbol and section names.
  void AddModuleClassList(ArrayRef<llvm::GlobalValue *> Container,
                          StringRef SymbolName, StringRef SectionName);

  llvm::GlobalVariable * BuildClassRoTInitializer(unsigned flags,
                                              unsigned InstanceStart,
                                              unsigned InstanceSize,
                                              const ObjCImplementationDecl *ID);
  llvm::GlobalVariable * BuildClassMetaData(const std::string &ClassName,
                                            llvm::Constant *IsAGV,
                                            llvm::Constant *SuperClassGV,
                                            llvm::Constant *ClassRoGV,
                                            bool HiddenVisibility,
                                            bool Weak);

  llvm::Constant *GetMethodConstant(const ObjCMethodDecl *MD);

  llvm::Constant *GetMethodDescriptionConstant(const ObjCMethodDecl *MD);

  /// EmitMethodList - Emit the method list for the given
  /// implementation. The return value has type MethodListnfABITy.
  llvm::Constant *EmitMethodList(Twine Name, StringRef Section,
                                 ArrayRef<llvm::Constant *> Methods);
  /// EmitIvarList - Emit the ivar list for the given
  /// implementation. If ForClass is true the list of class ivars
  /// (i.e. metaclass ivars) is emitted, otherwise the list of
  /// interface ivars will be emitted. The return value has type
  /// IvarListnfABIPtrTy.
  llvm::Constant *EmitIvarList(const ObjCImplementationDecl *ID);

  llvm::Constant *EmitIvarOffsetVar(const ObjCInterfaceDecl *ID,
                                    const ObjCIvarDecl *Ivar,
                                    unsigned long int offset);

  /// GetOrEmitProtocol - Get the protocol object for the given
  /// declaration, emitting it if necessary. The return value has type
  /// ProtocolPtrTy.
  llvm::Constant *GetOrEmitProtocol(const ObjCProtocolDecl *PD) override;

  /// GetOrEmitProtocolRef - Get a forward reference to the protocol
  /// object for the given declaration, emitting it if needed. These
  /// forward references will be filled in with empty bodies if no
  /// definition is seen. The return value has type ProtocolPtrTy.
  llvm::Constant *GetOrEmitProtocolRef(const ObjCProtocolDecl *PD) override;

  /// EmitProtocolList - Generate the list of referenced
  /// protocols. The return value has type ProtocolListPtrTy.
  llvm::Constant *EmitProtocolList(Twine Name,
                                   ObjCProtocolDecl::protocol_iterator begin,
                                   ObjCProtocolDecl::protocol_iterator end);

  CodeGen::RValue EmitVTableMessageSend(CodeGen::CodeGenFunction &CGF,
                                        ReturnValueSlot Return,
                                        QualType ResultType,
                                        Selector Sel,
                                        llvm::Value *Receiver,
                                        QualType Arg0Ty,
                                        bool IsSuper,
                                        const CallArgList &CallArgs,
                                        const ObjCMethodDecl *Method);
  
  /// GetClassGlobal - Return the global variable for the Objective-C
  /// class of the given name.
  llvm::GlobalVariable *GetClassGlobal(StringRef Name,
                                       bool Weak = false) override;

  /// EmitClassRef - Return a Value*, of type ObjCTypes.ClassPtrTy,
  /// for the given class reference.
  llvm::Value *EmitClassRef(CodeGenFunction &CGF,
                            const ObjCInterfaceDecl *ID);
  
  llvm::Value *EmitClassRefFromId(CodeGenFunction &CGF,
                                  IdentifierInfo *II, bool Weak,
                                  const ObjCInterfaceDecl *ID);

  llvm::Value *EmitNSAutoreleasePoolClassRef(CodeGenFunction &CGF) override;

  /// EmitSuperClassRef - Return a Value*, of type ObjCTypes.ClassPtrTy,
  /// for the given super class reference.
  llvm::Value *EmitSuperClassRef(CodeGenFunction &CGF,
                                 const ObjCInterfaceDecl *ID);

  /// EmitMetaClassRef - Return a Value * of the address of _class_t
  /// meta-data
  llvm::Value *EmitMetaClassRef(CodeGenFunction &CGF,
                                const ObjCInterfaceDecl *ID, bool Weak);

  /// ObjCIvarOffsetVariable - Returns the ivar offset variable for
  /// the given ivar.
  ///
  llvm::GlobalVariable * ObjCIvarOffsetVariable(
    const ObjCInterfaceDecl *ID,
    const ObjCIvarDecl *Ivar);

  /// EmitSelector - Return a Value*, of type ObjCTypes.SelectorPtrTy,
  /// for the given selector.
  llvm::Value *EmitSelector(CodeGenFunction &CGF, Selector Sel);
  Address EmitSelectorAddr(CodeGenFunction &CGF, Selector Sel);

  /// GetInterfaceEHType - Get the cached ehtype for the given Objective-C
  /// interface. The return value has type EHTypePtrTy.
  llvm::Constant *GetInterfaceEHType(const ObjCInterfaceDecl *ID,
                                  bool ForDefinition);

  StringRef getMetaclassSymbolPrefix() const { return "OBJC_METACLASS_$_"; }

  StringRef getClassSymbolPrefix() const { return "OBJC_CLASS_$_"; }

  void GetClassSizeInfo(const ObjCImplementationDecl *OID,
                        uint32_t &InstanceStart,
                        uint32_t &InstanceSize);

  // Shamelessly stolen from Analysis/CFRefCount.cpp
  Selector GetNullarySelector(const char* name) const {
    IdentifierInfo* II = &CGM.getContext().Idents.get(name);
    return CGM.getContext().Selectors.getSelector(0, &II);
  }

  Selector GetUnarySelector(const char* name) const {
    IdentifierInfo* II = &CGM.getContext().Idents.get(name);
    return CGM.getContext().Selectors.getSelector(1, &II);
  }

  /// ImplementationIsNonLazy - Check whether the given category or
  /// class implementation is "non-lazy".
  bool ImplementationIsNonLazy(const ObjCImplDecl *OD) const;

  bool IsIvarOffsetKnownIdempotent(const CodeGen::CodeGenFunction &CGF,
                                   const ObjCIvarDecl *IV) {
    // Annotate the load as an invariant load iff inside an instance method
    // and ivar belongs to instance method's class and one of its super class.
    // This check is needed because the ivar offset is a lazily
    // initialised value that may depend on objc_msgSend to perform a fixup on
    // the first message dispatch.
    //
    // An additional opportunity to mark the load as invariant arises when the
    // base of the ivar access is a parameter to an Objective C method.
    // However, because the parameters are not available in the current
    // interface, we cannot perform this check.
    if (const ObjCMethodDecl *MD =
          dyn_cast_or_null<ObjCMethodDecl>(CGF.CurFuncDecl))
      if (MD->isInstanceMethod())
        if (const ObjCInterfaceDecl *ID = MD->getClassInterface())
          return IV->getContainingInterface()->isSuperClassOf(ID);
    return false;
  }

public:
  CGObjCNonFragileABIMac(CodeGen::CodeGenModule &cgm);
  // FIXME. All stubs for now!
  llvm::Function *ModuleInitFunction() override;

  CodeGen::RValue GenerateMessageSend(CodeGen::CodeGenFunction &CGF,
                                      ReturnValueSlot Return,
                                      QualType ResultType, Selector Sel,
                                      llvm::Value *Receiver,
                                      const CallArgList &CallArgs,
                                      const ObjCInterfaceDecl *Class,
                                      const ObjCMethodDecl *Method) override;

  CodeGen::RValue
  GenerateMessageSendSuper(CodeGen::CodeGenFunction &CGF,
                           ReturnValueSlot Return, QualType ResultType,
                           Selector Sel, const ObjCInterfaceDecl *Class,
                           bool isCategoryImpl, llvm::Value *Receiver,
                           bool IsClassMessage, const CallArgList &CallArgs,
                           const ObjCMethodDecl *Method) override;

  llvm::Value *GetClass(CodeGenFunction &CGF,
                        const ObjCInterfaceDecl *ID) override;

  llvm::Value *GetSelector(CodeGenFunction &CGF, Selector Sel) override
    { return EmitSelector(CGF, Sel); }
  Address GetAddrOfSelector(CodeGenFunction &CGF, Selector Sel) override
    { return EmitSelectorAddr(CGF, Sel); }

  /// The NeXT/Apple runtimes do not support typed selectors; just emit an
  /// untyped one.
  llvm::Value *GetSelector(CodeGenFunction &CGF,
                           const ObjCMethodDecl *Method) override
    { return EmitSelector(CGF, Method->getSelector()); }

  void GenerateCategory(const ObjCCategoryImplDecl *CMD) override;

  void GenerateClass(const ObjCImplementationDecl *ClassDecl) override;

  void RegisterAlias(const ObjCCompatibleAliasDecl *OAD) override {}

  llvm::Value *GenerateProtocolRef(CodeGenFunction &CGF,
                                   const ObjCProtocolDecl *PD) override;

  llvm::Constant *GetEHType(QualType T) override;

  llvm::Constant *GetPropertyGetFunction() override {
    return ObjCTypes.getGetPropertyFn();
  }
  llvm::Constant *GetPropertySetFunction() override {
    return ObjCTypes.getSetPropertyFn();
  }

  llvm::Constant *GetOptimizedPropertySetFunction(bool atomic,
                                                  bool copy) override {
    return ObjCTypes.getOptimizedSetPropertyFn(atomic, copy);
  }

  llvm::Constant *GetSetStructFunction() override {
    return ObjCTypes.getCopyStructFn();
  }

  llvm::Constant *GetGetStructFunction() override {
    return ObjCTypes.getCopyStructFn();
  }

  llvm::Constant *GetCppAtomicObjectSetFunction() override {
    return ObjCTypes.getCppAtomicObjectFunction();
  }

  llvm::Constant *GetCppAtomicObjectGetFunction() override {
    return ObjCTypes.getCppAtomicObjectFunction();
  }

  llvm::Constant *EnumerationMutationFunction() override {
    return ObjCTypes.getEnumerationMutationFn();
  }

  void EmitTryStmt(CodeGen::CodeGenFunction &CGF,
                   const ObjCAtTryStmt &S) override;
  void EmitSynchronizedStmt(CodeGen::CodeGenFunction &CGF,
                            const ObjCAtSynchronizedStmt &S) override;
  void EmitThrowStmt(CodeGen::CodeGenFunction &CGF, const ObjCAtThrowStmt &S,
                     bool ClearInsertionPoint=true) override;
  llvm::Value * EmitObjCWeakRead(CodeGen::CodeGenFunction &CGF,
                                 Address AddrWeakObj) override;
  void EmitObjCWeakAssign(CodeGen::CodeGenFunction &CGF,
                          llvm::Value *src, Address edst) override;
  void EmitObjCGlobalAssign(CodeGen::CodeGenFunction &CGF,
                            llvm::Value *src, Address dest,
                            bool threadlocal = false) override;
  void EmitObjCIvarAssign(CodeGen::CodeGenFunction &CGF,
                          llvm::Value *src, Address dest,
                          llvm::Value *ivarOffset) override;
  void EmitObjCStrongCastAssign(CodeGen::CodeGenFunction &CGF,
                                llvm::Value *src, Address dest) override;
  void EmitGCMemmoveCollectable(CodeGen::CodeGenFunction &CGF,
                                Address dest, Address src,
                                llvm::Value *size) override;
  LValue EmitObjCValueForIvar(CodeGen::CodeGenFunction &CGF, QualType ObjectTy,
                              llvm::Value *BaseValue, const ObjCIvarDecl *Ivar,
                              unsigned CVRQualifiers) override;
  llvm::Value *EmitIvarOffset(CodeGen::CodeGenFunction &CGF,
                              const ObjCInterfaceDecl *Interface,
                              const ObjCIvarDecl *Ivar) override;
};

/// A helper class for performing the null-initialization of a return
/// value.
struct NullReturnState {
  llvm::BasicBlock *NullBB;
  NullReturnState() : NullBB(nullptr) {}

  /// Perform a null-check of the given receiver.
  void init(CodeGenFunction &CGF, llvm::Value *receiver) {
    // Make blocks for the null-receiver and call edges.
    NullBB = CGF.createBasicBlock("msgSend.null-receiver");
    llvm::BasicBlock *callBB = CGF.createBasicBlock("msgSend.call");

    // Check for a null receiver and, if there is one, jump to the
    // null-receiver block.  There's no point in trying to avoid it:
    // we're always going to put *something* there, because otherwise
    // we shouldn't have done this null-check in the first place.
    llvm::Value *isNull = CGF.Builder.CreateIsNull(receiver);
    CGF.Builder.CreateCondBr(isNull, NullBB, callBB);

    // Otherwise, start performing the call.
    CGF.EmitBlock(callBB);
  }

  /// Complete the null-return operation.  It is valid to call this
  /// regardless of whether 'init' has been called.
  RValue complete(CodeGenFunction &CGF, RValue result, QualType resultType,
                  const CallArgList &CallArgs,
                  const ObjCMethodDecl *Method) {
    // If we never had to do a null-check, just use the raw result.
    if (!NullBB) return result;

    // The continuation block.  This will be left null if we don't have an
    // IP, which can happen if the method we're calling is marked noreturn.
    llvm::BasicBlock *contBB = nullptr;

    // Finish the call path.
    llvm::BasicBlock *callBB = CGF.Builder.GetInsertBlock();
    if (callBB) {
      contBB = CGF.createBasicBlock("msgSend.cont");
      CGF.Builder.CreateBr(contBB);
    }

    // Okay, start emitting the null-receiver block.
    CGF.EmitBlock(NullBB);
    
    // Release any consumed arguments we've got.
    if (Method) {
      CallArgList::const_iterator I = CallArgs.begin();
      for (ObjCMethodDecl::param_const_iterator i = Method->param_begin(),
           e = Method->param_end(); i != e; ++i, ++I) {
        const ParmVarDecl *ParamDecl = (*i);
        if (ParamDecl->hasAttr<NSConsumedAttr>()) {
          RValue RV = I->RV;
          assert(RV.isScalar() && 
                 "NullReturnState::complete - arg not on object");
          CGF.EmitARCRelease(RV.getScalarVal(), ARCImpreciseLifetime);
        }
      }
    }

    // The phi code below assumes that we haven't needed any control flow yet.
    assert(CGF.Builder.GetInsertBlock() == NullBB);

    // If we've got a void return, just jump to the continuation block.
    if (result.isScalar() && resultType->isVoidType()) {
      // No jumps required if the message-send was noreturn.
      if (contBB) CGF.EmitBlock(contBB);
      return result;
    }

    // If we've got a scalar return, build a phi.
    if (result.isScalar()) {
      // Derive the null-initialization value.
      llvm::Constant *null = CGF.CGM.EmitNullConstant(resultType);

      // If no join is necessary, just flow out.
      if (!contBB) return RValue::get(null);

      // Otherwise, build a phi.
      CGF.EmitBlock(contBB);
      llvm::PHINode *phi = CGF.Builder.CreatePHI(null->getType(), 2);
      phi->addIncoming(result.getScalarVal(), callBB);
      phi->addIncoming(null, NullBB);
      return RValue::get(phi);
    }

    // If we've got an aggregate return, null the buffer out.
    // FIXME: maybe we should be doing things differently for all the
    // cases where the ABI has us returning (1) non-agg values in
    // memory or (2) agg values in registers.
    if (result.isAggregate()) {
      assert(result.isAggregate() && "null init of non-aggregate result?");
      CGF.EmitNullInitialization(result.getAggregateAddress(), resultType);
      if (contBB) CGF.EmitBlock(contBB);
      return result;
    }

    // Complex types.
    CGF.EmitBlock(contBB);
    CodeGenFunction::ComplexPairTy callResult = result.getComplexVal();

    // Find the scalar type and its zero value.
    llvm::Type *scalarTy = callResult.first->getType();
    llvm::Constant *scalarZero = llvm::Constant::getNullValue(scalarTy);

    // Build phis for both coordinates.
    llvm::PHINode *real = CGF.Builder.CreatePHI(scalarTy, 2);
    real->addIncoming(callResult.first, callBB);
    real->addIncoming(scalarZero, NullBB);
    llvm::PHINode *imag = CGF.Builder.CreatePHI(scalarTy, 2);
    imag->addIncoming(callResult.second, callBB);
    imag->addIncoming(scalarZero, NullBB);
    return RValue::getComplex(real, imag);
  }
};

} // end anonymous namespace

/* *** Helper Functions *** */

/// getConstantGEP() - Help routine to construct simple GEPs.
static llvm::Constant *getConstantGEP(llvm::LLVMContext &VMContext,
                                      llvm::GlobalVariable *C, unsigned idx0,
                                      unsigned idx1) {
  llvm::Value *Idxs[] = {
    llvm::ConstantInt::get(llvm::Type::getInt32Ty(VMContext), idx0),
    llvm::ConstantInt::get(llvm::Type::getInt32Ty(VMContext), idx1)
  };
  return llvm::ConstantExpr::getGetElementPtr(C->getValueType(), C, Idxs);
}

/// hasObjCExceptionAttribute - Return true if this class or any super
/// class has the __objc_exception__ attribute.
static bool hasObjCExceptionAttribute(ASTContext &Context,
                                      const ObjCInterfaceDecl *OID) {
  if (OID->hasAttr<ObjCExceptionAttr>())
    return true;
  if (const ObjCInterfaceDecl *Super = OID->getSuperClass())
    return hasObjCExceptionAttribute(Context, Super);
  return false;
}

/* *** CGObjCMac Public Interface *** */

CGObjCMac::CGObjCMac(CodeGen::CodeGenModule &cgm) : CGObjCCommonMac(cgm),
                                                    ObjCTypes(cgm) {
  ObjCABI = 1;
  EmitImageInfo();
}

/// GetClass - Return a reference to the class for the given interface
/// decl.
llvm::Value *CGObjCMac::GetClass(CodeGenFunction &CGF,
                                 const ObjCInterfaceDecl *ID) {
  return EmitClassRef(CGF, ID);
}

/// GetSelector - Return the pointer to the unique'd string for this selector.
llvm::Value *CGObjCMac::GetSelector(CodeGenFunction &CGF, Selector Sel) {
  return EmitSelector(CGF, Sel);
}
Address CGObjCMac::GetAddrOfSelector(CodeGenFunction &CGF, Selector Sel) {
  return EmitSelectorAddr(CGF, Sel);
}
llvm::Value *CGObjCMac::GetSelector(CodeGenFunction &CGF, const ObjCMethodDecl
                                    *Method) {
  return EmitSelector(CGF, Method->getSelector());
}

llvm::Constant *CGObjCMac::GetEHType(QualType T) {
  if (T->isObjCIdType() ||
      T->isObjCQualifiedIdType()) {
    return CGM.GetAddrOfRTTIDescriptor(
              CGM.getContext().getObjCIdRedefinitionType(), /*ForEH=*/true);
  }
  if (T->isObjCClassType() ||
      T->isObjCQualifiedClassType()) {
    return CGM.GetAddrOfRTTIDescriptor(
             CGM.getContext().getObjCClassRedefinitionType(), /*ForEH=*/true);
  }
  if (T->isObjCObjectPointerType())
    return CGM.GetAddrOfRTTIDescriptor(T,  /*ForEH=*/true);
  
  llvm_unreachable("asking for catch type for ObjC type in fragile runtime");
}

/// Generate a constant CFString object.
/*
  struct __builtin_CFString {
  const int *isa; // point to __CFConstantStringClassReference
  int flags;
  const char *str;
  long length;
  };
*/

/// or Generate a constant NSString object.
/*
   struct __builtin_NSString {
     const int *isa; // point to __NSConstantStringClassReference
     const char *str;
     unsigned int length;
   };
*/

ConstantAddress CGObjCCommonMac::GenerateConstantString(
  const StringLiteral *SL) {
  return (CGM.getLangOpts().NoConstantCFStrings == 0 ? 
          CGM.GetAddrOfConstantCFString(SL) :
          CGM.GetAddrOfConstantString(SL));
}

enum {
  kCFTaggedObjectID_Integer = (1 << 1) + 1
};

/// Generates a message send where the super is the receiver.  This is
/// a message send to self with special delivery semantics indicating
/// which class's method should be called.
CodeGen::RValue
CGObjCMac::GenerateMessageSendSuper(CodeGen::CodeGenFunction &CGF,
                                    ReturnValueSlot Return,
                                    QualType ResultType,
                                    Selector Sel,
                                    const ObjCInterfaceDecl *Class,
                                    bool isCategoryImpl,
                                    llvm::Value *Receiver,
                                    bool IsClassMessage,
                                    const CodeGen::CallArgList &CallArgs,
                                    const ObjCMethodDecl *Method) {
  // Create and init a super structure; this is a (receiver, class)
  // pair we will pass to objc_msgSendSuper.
  Address ObjCSuper =
    CGF.CreateTempAlloca(ObjCTypes.SuperTy, CGF.getPointerAlign(),
                         "objc_super");
  llvm::Value *ReceiverAsObject =
    CGF.Builder.CreateBitCast(Receiver, ObjCTypes.ObjectPtrTy);
  CGF.Builder.CreateStore(
      ReceiverAsObject,
      CGF.Builder.CreateStructGEP(ObjCSuper, 0, CharUnits::Zero()));

  // If this is a class message the metaclass is passed as the target.
  llvm::Value *Target;
  if (IsClassMessage) {
    if (isCategoryImpl) {
      // Message sent to 'super' in a class method defined in a category
      // implementation requires an odd treatment.
      // If we are in a class method, we must retrieve the
      // _metaclass_ for the current class, pointed at by
      // the class's "isa" pointer.  The following assumes that
      // isa" is the first ivar in a class (which it must be).
      Target = EmitClassRef(CGF, Class->getSuperClass());
      Target = CGF.Builder.CreateStructGEP(ObjCTypes.ClassTy, Target, 0);
      Target = CGF.Builder.CreateAlignedLoad(Target, CGF.getPointerAlign());
    } else {
      llvm::Constant *MetaClassPtr = EmitMetaClassRef(Class);
      llvm::Value *SuperPtr =
          CGF.Builder.CreateStructGEP(ObjCTypes.ClassTy, MetaClassPtr, 1);
      llvm::Value *Super =
        CGF.Builder.CreateAlignedLoad(SuperPtr, CGF.getPointerAlign());
      Target = Super;
    }
  } else if (isCategoryImpl)
    Target = EmitClassRef(CGF, Class->getSuperClass());
  else {
    llvm::Value *ClassPtr = EmitSuperClassRef(Class);
    ClassPtr = CGF.Builder.CreateStructGEP(ObjCTypes.ClassTy, ClassPtr, 1);
    Target = CGF.Builder.CreateAlignedLoad(ClassPtr, CGF.getPointerAlign());
  }
  // FIXME: We shouldn't need to do this cast, rectify the ASTContext and
  // ObjCTypes types.
  llvm::Type *ClassTy =
    CGM.getTypes().ConvertType(CGF.getContext().getObjCClassType());
  Target = CGF.Builder.CreateBitCast(Target, ClassTy);
  CGF.Builder.CreateStore(Target,
          CGF.Builder.CreateStructGEP(ObjCSuper, 1, CGF.getPointerSize()));
  return EmitMessageSend(CGF, Return, ResultType,
                         EmitSelector(CGF, Sel),
                         ObjCSuper.getPointer(), ObjCTypes.SuperPtrCTy,
                         true, CallArgs, Method, Class, ObjCTypes);
}

/// Generate code for a message send expression.
CodeGen::RValue CGObjCMac::GenerateMessageSend(CodeGen::CodeGenFunction &CGF,
                                               ReturnValueSlot Return,
                                               QualType ResultType,
                                               Selector Sel,
                                               llvm::Value *Receiver,
                                               const CallArgList &CallArgs,
                                               const ObjCInterfaceDecl *Class,
                                               const ObjCMethodDecl *Method) {
  return EmitMessageSend(CGF, Return, ResultType,
                         EmitSelector(CGF, Sel),
                         Receiver, CGF.getContext().getObjCIdType(),
                         false, CallArgs, Method, Class, ObjCTypes);
}

static bool isWeakLinkedClass(const ObjCInterfaceDecl *ID) {
  do {
    if (ID->isWeakImported())
      return true;
  } while ((ID = ID->getSuperClass()));

  return false;
}

CodeGen::RValue
CGObjCCommonMac::EmitMessageSend(CodeGen::CodeGenFunction &CGF,
                                 ReturnValueSlot Return,
                                 QualType ResultType,
                                 llvm::Value *Sel,
                                 llvm::Value *Arg0,
                                 QualType Arg0Ty,
                                 bool IsSuper,
                                 const CallArgList &CallArgs,
                                 const ObjCMethodDecl *Method,
                                 const ObjCInterfaceDecl *ClassReceiver,
                                 const ObjCCommonTypesHelper &ObjCTypes) {
  CallArgList ActualArgs;
  if (!IsSuper)
    Arg0 = CGF.Builder.CreateBitCast(Arg0, ObjCTypes.ObjectPtrTy);
  ActualArgs.add(RValue::get(Arg0), Arg0Ty);
  ActualArgs.add(RValue::get(Sel), CGF.getContext().getObjCSelType());
  ActualArgs.addFrom(CallArgs);

  // If we're calling a method, use the formal signature.
  MessageSendInfo MSI = getMessageSendInfo(Method, ResultType, ActualArgs);

  if (Method)
    assert(CGM.getContext().getCanonicalType(Method->getReturnType()) ==
               CGM.getContext().getCanonicalType(ResultType) &&
           "Result type mismatch!");

  bool ReceiverCanBeNull = true;

  // Super dispatch assumes that self is non-null; even the messenger
  // doesn't have a null check internally.
  if (IsSuper) {
    ReceiverCanBeNull = false;

  // If this is a direct dispatch of a class method, check whether the class,
  // or anything in its hierarchy, was weak-linked.
  } else if (ClassReceiver && Method && Method->isClassMethod()) {
    ReceiverCanBeNull = isWeakLinkedClass(ClassReceiver);

  // If we're emitting a method, and self is const (meaning just ARC, for now),
  // and the receiver is a load of self, then self is a valid object.
  } else if (auto CurMethod =
               dyn_cast_or_null<ObjCMethodDecl>(CGF.CurCodeDecl)) {
    auto Self = CurMethod->getSelfDecl();
    if (Self->getType().isConstQualified()) {
      if (auto LI = dyn_cast<llvm::LoadInst>(Arg0->stripPointerCasts())) {
        llvm::Value *SelfAddr = CGF.GetAddrOfLocalVar(Self).getPointer();
        if (SelfAddr == LI->getPointerOperand()) {
          ReceiverCanBeNull = false;
        }
      }
    }
  }

  NullReturnState nullReturn;

  llvm::Constant *Fn = nullptr;
  if (CGM.ReturnSlotInterferesWithArgs(MSI.CallInfo)) {
    if (ReceiverCanBeNull) nullReturn.init(CGF, Arg0);
    Fn = (ObjCABI == 2) ?  ObjCTypes.getSendStretFn2(IsSuper)
      : ObjCTypes.getSendStretFn(IsSuper);
  } else if (CGM.ReturnTypeUsesFPRet(ResultType)) {
    Fn = (ObjCABI == 2) ? ObjCTypes.getSendFpretFn2(IsSuper)
      : ObjCTypes.getSendFpretFn(IsSuper);
  } else if (CGM.ReturnTypeUsesFP2Ret(ResultType)) {
    Fn = (ObjCABI == 2) ? ObjCTypes.getSendFp2RetFn2(IsSuper)
      : ObjCTypes.getSendFp2retFn(IsSuper);
  } else {
    // arm64 uses objc_msgSend for stret methods and yet null receiver check
    // must be made for it.
    if (ReceiverCanBeNull && CGM.ReturnTypeUsesSRet(MSI.CallInfo))
      nullReturn.init(CGF, Arg0);
    Fn = (ObjCABI == 2) ? ObjCTypes.getSendFn2(IsSuper)
      : ObjCTypes.getSendFn(IsSuper);
  }

  // Emit a null-check if there's a consumed argument other than the receiver.
  bool RequiresNullCheck = false;
  if (ReceiverCanBeNull && CGM.getLangOpts().ObjCAutoRefCount && Method) {
    for (const auto *ParamDecl : Method->parameters()) {
      if (ParamDecl->hasAttr<NSConsumedAttr>()) {
        if (!nullReturn.NullBB)
          nullReturn.init(CGF, Arg0);
        RequiresNullCheck = true;
        break;
      }
    }
  }
  
  llvm::Instruction *CallSite;
  Fn = llvm::ConstantExpr::getBitCast(Fn, MSI.MessengerType);
  RValue rvalue = CGF.EmitCall(MSI.CallInfo, Fn, Return, ActualArgs,
                               CGCalleeInfo(), &CallSite);

  // Mark the call as noreturn if the method is marked noreturn and the
  // receiver cannot be null.
  if (Method && Method->hasAttr<NoReturnAttr>() && !ReceiverCanBeNull) {
    llvm::CallSite(CallSite).setDoesNotReturn();
  }

  return nullReturn.complete(CGF, rvalue, ResultType, CallArgs,
                             RequiresNullCheck ? Method : nullptr);
}

static Qualifiers::GC GetGCAttrTypeForType(ASTContext &Ctx, QualType FQT,
                                           bool pointee = false) {
  // Note that GC qualification applies recursively to C pointer types
  // that aren't otherwise decorated.  This is weird, but it's probably
  // an intentional workaround to the unreliable placement of GC qualifiers.
  if (FQT.isObjCGCStrong())
    return Qualifiers::Strong;

  if (FQT.isObjCGCWeak())
    return Qualifiers::Weak;

  if (auto ownership = FQT.getObjCLifetime()) {
    // Ownership does not apply recursively to C pointer types.
    if (pointee) return Qualifiers::GCNone;
    switch (ownership) {
    case Qualifiers::OCL_Weak: return Qualifiers::Weak;
    case Qualifiers::OCL_Strong: return Qualifiers::Strong;
    case Qualifiers::OCL_ExplicitNone: return Qualifiers::GCNone;
    case Qualifiers::OCL_Autoreleasing: llvm_unreachable("autoreleasing ivar?");
    case Qualifiers::OCL_None: llvm_unreachable("known nonzero");
    }
    llvm_unreachable("bad objc ownership");
  }
  
  // Treat unqualified retainable pointers as strong.
  if (FQT->isObjCObjectPointerType() || FQT->isBlockPointerType())
    return Qualifiers::Strong;
  
  // Walk into C pointer types, but only in GC.
  if (Ctx.getLangOpts().getGC() != LangOptions::NonGC) {
    if (const PointerType *PT = FQT->getAs<PointerType>())
      return GetGCAttrTypeForType(Ctx, PT->getPointeeType(), /*pointee*/ true);
  }
  
  return Qualifiers::GCNone;
}

namespace {
  struct IvarInfo {
    CharUnits Offset;
    uint64_t SizeInWords;
    IvarInfo(CharUnits offset, uint64_t sizeInWords)
      : Offset(offset), SizeInWords(sizeInWords) {}

    // Allow sorting based on byte pos.
    bool operator<(const IvarInfo &other) const {
      return Offset < other.Offset;
    }
  };

  /// A helper class for building GC layout strings.
  class IvarLayoutBuilder {
    CodeGenModule &CGM;

    /// The start of the layout.  Offsets will be relative to this value,
    /// and entries less than this value will be silently discarded.
    CharUnits InstanceBegin;

    /// The end of the layout.  Offsets will never exceed this value.
    CharUnits InstanceEnd;

    /// Whether we're generating the strong layout or the weak layout.
    bool ForStrongLayout;

    /// Whether the offsets in IvarsInfo might be out-of-order.
    bool IsDisordered = false;

    llvm::SmallVector<IvarInfo, 8> IvarsInfo;

  public:
    IvarLayoutBuilder(CodeGenModule &CGM, CharUnits instanceBegin,
                      CharUnits instanceEnd, bool forStrongLayout)
      : CGM(CGM), InstanceBegin(instanceBegin), InstanceEnd(instanceEnd),
        ForStrongLayout(forStrongLayout) {
    }

    void visitRecord(const RecordType *RT, CharUnits offset);

    template <class Iterator, class GetOffsetFn>
    void visitAggregate(Iterator begin, Iterator end, 
                        CharUnits aggrOffset,
                        const GetOffsetFn &getOffset);

    void visitField(const FieldDecl *field, CharUnits offset);

    /// Add the layout of a block implementation.
    void visitBlock(const CGBlockInfo &blockInfo);

    /// Is there any information for an interesting bitmap?
    bool hasBitmapData() const { return !IvarsInfo.empty(); }

    llvm::Constant *buildBitmap(CGObjCCommonMac &CGObjC,
                                llvm::SmallVectorImpl<unsigned char> &buffer);

    static void dump(ArrayRef<unsigned char> buffer) {
      const unsigned char *s = buffer.data();
      for (unsigned i = 0, e = buffer.size(); i < e; i++)
        if (!(s[i] & 0xf0))
          printf("0x0%x%s", s[i], s[i] != 0 ? ", " : "");
        else
          printf("0x%x%s",  s[i], s[i] != 0 ? ", " : "");
      printf("\n");
    }
  };
} // end anonymous namespace

llvm::Constant *CGObjCCommonMac::BuildGCBlockLayout(CodeGenModule &CGM,
                                                const CGBlockInfo &blockInfo) {
  
  llvm::Constant *nullPtr = llvm::Constant::getNullValue(CGM.Int8PtrTy);
  if (CGM.getLangOpts().getGC() == LangOptions::NonGC)
    return nullPtr;

  IvarLayoutBuilder builder(CGM, CharUnits::Zero(), blockInfo.BlockSize,
                            /*for strong layout*/ true);

  builder.visitBlock(blockInfo);

  if (!builder.hasBitmapData())
    return nullPtr;

  llvm::SmallVector<unsigned char, 32> buffer;
  llvm::Constant *C = builder.buildBitmap(*this, buffer);
  if (CGM.getLangOpts().ObjCGCBitmapPrint && !buffer.empty()) {
    printf("\n block variable layout for block: ");
    builder.dump(buffer);
  }
  
  return C;
}

void IvarLayoutBuilder::visitBlock(const CGBlockInfo &blockInfo) {
  // __isa is the first field in block descriptor and must assume by runtime's
  // convention that it is GC'able.
  IvarsInfo.push_back(IvarInfo(CharUnits::Zero(), 1));

  const BlockDecl *blockDecl = blockInfo.getBlockDecl();

  // Ignore the optional 'this' capture: C++ objects are not assumed
  // to be GC'ed.

  CharUnits lastFieldOffset;

  // Walk the captured variables.
  for (const auto &CI : blockDecl->captures()) {
    const VarDecl *variable = CI.getVariable();
    QualType type = variable->getType();

    const CGBlockInfo::Capture &capture = blockInfo.getCapture(variable);

    // Ignore constant captures.
    if (capture.isConstant()) continue;

    CharUnits fieldOffset = capture.getOffset();

    // Block fields are not necessarily ordered; if we detect that we're
    // adding them out-of-order, make sure we sort later.
    if (fieldOffset < lastFieldOffset)
      IsDisordered = true;
    lastFieldOffset = fieldOffset;

    // __block variables are passed by their descriptor address.
    if (CI.isByRef()) {
      IvarsInfo.push_back(IvarInfo(fieldOffset, /*size in words*/ 1));
      continue;
    }

    assert(!type->isArrayType() && "array variable should not be caught");
    if (const RecordType *record = type->getAs<RecordType>()) {
      visitRecord(record, fieldOffset);
      continue;
    }
      
    Qualifiers::GC GCAttr = GetGCAttrTypeForType(CGM.getContext(), type);

    if (GCAttr == Qualifiers::Strong) {
      assert(CGM.getContext().getTypeSize(type)
                == CGM.getTarget().getPointerWidth(0));
      IvarsInfo.push_back(IvarInfo(fieldOffset, /*size in words*/ 1));
    }
  }
}

/// getBlockCaptureLifetime - This routine returns life time of the captured
/// block variable for the purpose of block layout meta-data generation. FQT is
/// the type of the variable captured in the block.
Qualifiers::ObjCLifetime CGObjCCommonMac::getBlockCaptureLifetime(QualType FQT,
                                                                  bool ByrefLayout) {
  // If it has an ownership qualifier, we're done.
  if (auto lifetime = FQT.getObjCLifetime())
    return lifetime;

  // If it doesn't, and this is ARC, it has no ownership.
  if (CGM.getLangOpts().ObjCAutoRefCount)
    return Qualifiers::OCL_None;
  
  // In MRC, retainable pointers are owned by non-__block variables.
  if (FQT->isObjCObjectPointerType() || FQT->isBlockPointerType())
    return ByrefLayout ? Qualifiers::OCL_ExplicitNone : Qualifiers::OCL_Strong;
  
  return Qualifiers::OCL_None;
}

void CGObjCCommonMac::UpdateRunSkipBlockVars(bool IsByref,
                                             Qualifiers::ObjCLifetime LifeTime,
                                             CharUnits FieldOffset,
                                             CharUnits FieldSize) {
  // __block variables are passed by their descriptor address.
  if (IsByref)
    RunSkipBlockVars.push_back(RUN_SKIP(BLOCK_LAYOUT_BYREF, FieldOffset,
                                        FieldSize));
  else if (LifeTime == Qualifiers::OCL_Strong)
    RunSkipBlockVars.push_back(RUN_SKIP(BLOCK_LAYOUT_STRONG, FieldOffset,
                                        FieldSize));
  else if (LifeTime == Qualifiers::OCL_Weak)
    RunSkipBlockVars.push_back(RUN_SKIP(BLOCK_LAYOUT_WEAK, FieldOffset,
                                        FieldSize));
  else if (LifeTime == Qualifiers::OCL_ExplicitNone)
    RunSkipBlockVars.push_back(RUN_SKIP(BLOCK_LAYOUT_UNRETAINED, FieldOffset,
                                        FieldSize));
  else
    RunSkipBlockVars.push_back(RUN_SKIP(BLOCK_LAYOUT_NON_OBJECT_BYTES,
                                        FieldOffset,
                                        FieldSize));
}

void CGObjCCommonMac::BuildRCRecordLayout(const llvm::StructLayout *RecLayout,
                                          const RecordDecl *RD,
                                          ArrayRef<const FieldDecl*> RecFields,
                                          CharUnits BytePos, bool &HasUnion,
                                          bool ByrefLayout) {
  bool IsUnion = (RD && RD->isUnion());
  CharUnits MaxUnionSize = CharUnits::Zero();
  const FieldDecl *MaxField = nullptr;
  const FieldDecl *LastFieldBitfieldOrUnnamed = nullptr;
  CharUnits MaxFieldOffset = CharUnits::Zero();
  CharUnits LastBitfieldOrUnnamedOffset = CharUnits::Zero();
  
  if (RecFields.empty())
    return;
  unsigned ByteSizeInBits = CGM.getTarget().getCharWidth();
  
  for (unsigned i = 0, e = RecFields.size(); i != e; ++i) {
    const FieldDecl *Field = RecFields[i];
    // Note that 'i' here is actually the field index inside RD of Field,
    // although this dependency is hidden.
    const ASTRecordLayout &RL = CGM.getContext().getASTRecordLayout(RD);
    CharUnits FieldOffset =
      CGM.getContext().toCharUnitsFromBits(RL.getFieldOffset(i));
    
    // Skip over unnamed or bitfields
    if (!Field->getIdentifier() || Field->isBitField()) {
      LastFieldBitfieldOrUnnamed = Field;
      LastBitfieldOrUnnamedOffset = FieldOffset;
      continue;
    }

    LastFieldBitfieldOrUnnamed = nullptr;
    QualType FQT = Field->getType();
    if (FQT->isRecordType() || FQT->isUnionType()) {
      if (FQT->isUnionType())
        HasUnion = true;
      
      BuildRCBlockVarRecordLayout(FQT->getAs<RecordType>(),
                                  BytePos + FieldOffset, HasUnion);
      continue;
    }
    
    if (const ArrayType *Array = CGM.getContext().getAsArrayType(FQT)) {
      const ConstantArrayType *CArray =
        dyn_cast_or_null<ConstantArrayType>(Array);
      uint64_t ElCount = CArray->getSize().getZExtValue();
      assert(CArray && "only array with known element size is supported");
      FQT = CArray->getElementType();
      while (const ArrayType *Array = CGM.getContext().getAsArrayType(FQT)) {
        const ConstantArrayType *CArray =
          dyn_cast_or_null<ConstantArrayType>(Array);
        ElCount *= CArray->getSize().getZExtValue();
        FQT = CArray->getElementType();
      }
      if (FQT->isRecordType() && ElCount) {
        int OldIndex = RunSkipBlockVars.size() - 1;
        const RecordType *RT = FQT->getAs<RecordType>();
        BuildRCBlockVarRecordLayout(RT, BytePos + FieldOffset,
                                    HasUnion);
        
        // Replicate layout information for each array element. Note that
        // one element is already done.
        uint64_t ElIx = 1;
        for (int FirstIndex = RunSkipBlockVars.size() - 1 ;ElIx < ElCount; ElIx++) {
          CharUnits Size = CGM.getContext().getTypeSizeInChars(RT);
          for (int i = OldIndex+1; i <= FirstIndex; ++i)
            RunSkipBlockVars.push_back(
              RUN_SKIP(RunSkipBlockVars[i].opcode,
              RunSkipBlockVars[i].block_var_bytepos + Size*ElIx,
              RunSkipBlockVars[i].block_var_size));
        }
        continue;
      }
    }
    CharUnits FieldSize = CGM.getContext().getTypeSizeInChars(Field->getType());
    if (IsUnion) {
      CharUnits UnionIvarSize = FieldSize;
      if (UnionIvarSize > MaxUnionSize) {
        MaxUnionSize = UnionIvarSize;
        MaxField = Field;
        MaxFieldOffset = FieldOffset;
      }
    } else {
      UpdateRunSkipBlockVars(false,
                             getBlockCaptureLifetime(FQT, ByrefLayout),
                             BytePos + FieldOffset,
                             FieldSize);
    }
  }
  
  if (LastFieldBitfieldOrUnnamed) {
    if (LastFieldBitfieldOrUnnamed->isBitField()) {
      // Last field was a bitfield. Must update the info.
      uint64_t BitFieldSize
        = LastFieldBitfieldOrUnnamed->getBitWidthValue(CGM.getContext());
      unsigned UnsSize = (BitFieldSize / ByteSizeInBits) +
                        ((BitFieldSize % ByteSizeInBits) != 0);
      CharUnits Size = CharUnits::fromQuantity(UnsSize);
      Size += LastBitfieldOrUnnamedOffset;
      UpdateRunSkipBlockVars(false,
                             getBlockCaptureLifetime(LastFieldBitfieldOrUnnamed->getType(),
                                                     ByrefLayout),
                             BytePos + LastBitfieldOrUnnamedOffset,
                             Size);
    } else {
      assert(!LastFieldBitfieldOrUnnamed->getIdentifier() &&"Expected unnamed");
      // Last field was unnamed. Must update skip info.
      CharUnits FieldSize
        = CGM.getContext().getTypeSizeInChars(LastFieldBitfieldOrUnnamed->getType());
      UpdateRunSkipBlockVars(false,
                             getBlockCaptureLifetime(LastFieldBitfieldOrUnnamed->getType(),
                                                     ByrefLayout),
                             BytePos + LastBitfieldOrUnnamedOffset,
                             FieldSize);
    }
  }
  
  if (MaxField)
    UpdateRunSkipBlockVars(false,
                           getBlockCaptureLifetime(MaxField->getType(), ByrefLayout),
                           BytePos + MaxFieldOffset,
                           MaxUnionSize);
}

void CGObjCCommonMac::BuildRCBlockVarRecordLayout(const RecordType *RT,
                                                  CharUnits BytePos,
                                                  bool &HasUnion,
                                                  bool ByrefLayout) {
  const RecordDecl *RD = RT->getDecl();
  SmallVector<const FieldDecl*, 16> Fields(RD->fields());
  llvm::Type *Ty = CGM.getTypes().ConvertType(QualType(RT, 0));
  const llvm::StructLayout *RecLayout =
    CGM.getDataLayout().getStructLayout(cast<llvm::StructType>(Ty));
  
  BuildRCRecordLayout(RecLayout, RD, Fields, BytePos, HasUnion, ByrefLayout);
}

/// InlineLayoutInstruction - This routine produce an inline instruction for the
/// block variable layout if it can. If not, it returns 0. Rules are as follow:
/// If ((uintptr_t) layout) < (1 << 12), the layout is inline. In the 64bit world,
/// an inline layout of value 0x0000000000000xyz is interpreted as follows:
/// x captured object pointers of BLOCK_LAYOUT_STRONG. Followed by
/// y captured object of BLOCK_LAYOUT_BYREF. Followed by
/// z captured object of BLOCK_LAYOUT_WEAK. If any of the above is missing, zero
/// replaces it. For example, 0x00000x00 means x BLOCK_LAYOUT_STRONG and no
/// BLOCK_LAYOUT_BYREF and no BLOCK_LAYOUT_WEAK objects are captured.
uint64_t CGObjCCommonMac::InlineLayoutInstruction(
                                    SmallVectorImpl<unsigned char> &Layout) {
  uint64_t Result = 0;
  if (Layout.size() <= 3) {
    unsigned size = Layout.size();
    unsigned strong_word_count = 0, byref_word_count=0, weak_word_count=0;
    unsigned char inst;
    enum BLOCK_LAYOUT_OPCODE opcode ;
    switch (size) {
      case 3:
        inst = Layout[0];
        opcode = (enum BLOCK_LAYOUT_OPCODE) (inst >> 4);
        if (opcode == BLOCK_LAYOUT_STRONG)
          strong_word_count = (inst & 0xF)+1;
        else
          return 0;
        inst = Layout[1];
        opcode = (enum BLOCK_LAYOUT_OPCODE) (inst >> 4);
        if (opcode == BLOCK_LAYOUT_BYREF)
          byref_word_count = (inst & 0xF)+1;
        else
          return 0;
        inst = Layout[2];
        opcode = (enum BLOCK_LAYOUT_OPCODE) (inst >> 4);
        if (opcode == BLOCK_LAYOUT_WEAK)
          weak_word_count = (inst & 0xF)+1;
        else
          return 0;
        break;
        
      case 2:
        inst = Layout[0];
        opcode = (enum BLOCK_LAYOUT_OPCODE) (inst >> 4);
        if (opcode == BLOCK_LAYOUT_STRONG) {
          strong_word_count = (inst & 0xF)+1;
          inst = Layout[1];
          opcode = (enum BLOCK_LAYOUT_OPCODE) (inst >> 4);
          if (opcode == BLOCK_LAYOUT_BYREF)
            byref_word_count = (inst & 0xF)+1;
          else if (opcode == BLOCK_LAYOUT_WEAK)
            weak_word_count = (inst & 0xF)+1;
          else
            return 0;
        }
        else if (opcode == BLOCK_LAYOUT_BYREF) {
          byref_word_count = (inst & 0xF)+1;
          inst = Layout[1];
          opcode = (enum BLOCK_LAYOUT_OPCODE) (inst >> 4);
          if (opcode == BLOCK_LAYOUT_WEAK)
            weak_word_count = (inst & 0xF)+1;
          else
            return 0;
        }
        else
          return 0;
        break;
        
      case 1:
        inst = Layout[0];
        opcode = (enum BLOCK_LAYOUT_OPCODE) (inst >> 4);
        if (opcode == BLOCK_LAYOUT_STRONG)
          strong_word_count = (inst & 0xF)+1;
        else if (opcode == BLOCK_LAYOUT_BYREF)
          byref_word_count = (inst & 0xF)+1;
        else if (opcode == BLOCK_LAYOUT_WEAK)
          weak_word_count = (inst & 0xF)+1;
        else
          return 0;
        break;
        
      default:
        return 0;
    }
    
    // Cannot inline when any of the word counts is 15. Because this is one less
    // than the actual work count (so 15 means 16 actual word counts),
    // and we can only display 0 thru 15 word counts.
    if (strong_word_count == 16 || byref_word_count == 16 || weak_word_count == 16)
      return 0;
    
    unsigned count =
      (strong_word_count != 0) + (byref_word_count != 0) + (weak_word_count != 0);
    
    if (size == count) {
      if (strong_word_count)
        Result = strong_word_count;
      Result <<= 4;
      if (byref_word_count)
        Result += byref_word_count;
      Result <<= 4;
      if (weak_word_count)
        Result += weak_word_count;
    }
  }
  return Result;
}

llvm::Constant *CGObjCCommonMac::getBitmapBlockLayout(bool ComputeByrefLayout) {
  llvm::Constant *nullPtr = llvm::Constant::getNullValue(CGM.Int8PtrTy);
  if (RunSkipBlockVars.empty())
    return nullPtr;
  unsigned WordSizeInBits = CGM.getTarget().getPointerWidth(0);
  unsigned ByteSizeInBits = CGM.getTarget().getCharWidth();
  unsigned WordSizeInBytes = WordSizeInBits/ByteSizeInBits;
  
  // Sort on byte position; captures might not be allocated in order,
  // and unions can do funny things.
  llvm::array_pod_sort(RunSkipBlockVars.begin(), RunSkipBlockVars.end());
  SmallVector<unsigned char, 16> Layout;
  
  unsigned size = RunSkipBlockVars.size();
  for (unsigned i = 0; i < size; i++) {
    enum BLOCK_LAYOUT_OPCODE opcode = RunSkipBlockVars[i].opcode;
    CharUnits start_byte_pos = RunSkipBlockVars[i].block_var_bytepos;
    CharUnits end_byte_pos = start_byte_pos;
    unsigned j = i+1;
    while (j < size) {
      if (opcode == RunSkipBlockVars[j].opcode) {
        end_byte_pos = RunSkipBlockVars[j++].block_var_bytepos;
        i++;
      }
      else
        break;
    }
    CharUnits size_in_bytes =
    end_byte_pos - start_byte_pos + RunSkipBlockVars[j-1].block_var_size;
    if (j < size) {
      CharUnits gap =
      RunSkipBlockVars[j].block_var_bytepos -
      RunSkipBlockVars[j-1].block_var_bytepos - RunSkipBlockVars[j-1].block_var_size;
      size_in_bytes += gap;
    }
    CharUnits residue_in_bytes = CharUnits::Zero();
    if (opcode == BLOCK_LAYOUT_NON_OBJECT_BYTES) {
      residue_in_bytes = size_in_bytes % WordSizeInBytes;
      size_in_bytes -= residue_in_bytes;
      opcode = BLOCK_LAYOUT_NON_OBJECT_WORDS;
    }
    
    unsigned size_in_words = size_in_bytes.getQuantity() / WordSizeInBytes;
    while (size_in_words >= 16) {
      // Note that value in imm. is one less that the actual
      // value. So, 0xf means 16 words follow!
      unsigned char inst = (opcode << 4) | 0xf;
      Layout.push_back(inst);
      size_in_words -= 16;
    }
    if (size_in_words > 0) {
      // Note that value in imm. is one less that the actual
      // value. So, we subtract 1 away!
      unsigned char inst = (opcode << 4) | (size_in_words-1);
      Layout.push_back(inst);
    }
    if (residue_in_bytes > CharUnits::Zero()) {
      unsigned char inst =
      (BLOCK_LAYOUT_NON_OBJECT_BYTES << 4) | (residue_in_bytes.getQuantity()-1);
      Layout.push_back(inst);
    }
  }
  
  while (!Layout.empty()) {
    unsigned char inst = Layout.back();
    enum BLOCK_LAYOUT_OPCODE opcode = (enum BLOCK_LAYOUT_OPCODE) (inst >> 4);
    if (opcode == BLOCK_LAYOUT_NON_OBJECT_BYTES || opcode == BLOCK_LAYOUT_NON_OBJECT_WORDS)
      Layout.pop_back();
    else
      break;
  }
  
  uint64_t Result = InlineLayoutInstruction(Layout);
  if (Result != 0) {
    // Block variable layout instruction has been inlined.
    if (CGM.getLangOpts().ObjCGCBitmapPrint) {
      if (ComputeByrefLayout)
        printf("\n Inline BYREF variable layout: ");
      else
        printf("\n Inline block variable layout: ");
      printf("0x0%" PRIx64 "", Result);
      if (auto numStrong = (Result & 0xF00) >> 8)
        printf(", BL_STRONG:%d", (int) numStrong);
      if (auto numByref = (Result & 0x0F0) >> 4)
        printf(", BL_BYREF:%d", (int) numByref);
      if (auto numWeak = (Result & 0x00F) >> 0)
        printf(", BL_WEAK:%d", (int) numWeak);
      printf(", BL_OPERATOR:0\n");
    }
    return llvm::ConstantInt::get(CGM.IntPtrTy, Result);
  }
  
  unsigned char inst = (BLOCK_LAYOUT_OPERATOR << 4) | 0;
  Layout.push_back(inst);
  std::string BitMap;
  for (unsigned i = 0, e = Layout.size(); i != e; i++)
    BitMap += Layout[i];
  
  if (CGM.getLangOpts().ObjCGCBitmapPrint) {
    if (ComputeByrefLayout)
      printf("\n Byref variable layout: ");
    else
      printf("\n Block variable layout: ");
    for (unsigned i = 0, e = BitMap.size(); i != e; i++) {
      unsigned char inst = BitMap[i];
      enum BLOCK_LAYOUT_OPCODE opcode = (enum BLOCK_LAYOUT_OPCODE) (inst >> 4);
      unsigned delta = 1;
      switch (opcode) {
        case BLOCK_LAYOUT_OPERATOR:
          printf("BL_OPERATOR:");
          delta = 0;
          break;
        case BLOCK_LAYOUT_NON_OBJECT_BYTES:
          printf("BL_NON_OBJECT_BYTES:");
          break;
        case BLOCK_LAYOUT_NON_OBJECT_WORDS:
          printf("BL_NON_OBJECT_WORD:");
          break;
        case BLOCK_LAYOUT_STRONG:
          printf("BL_STRONG:");
          break;
        case BLOCK_LAYOUT_BYREF:
          printf("BL_BYREF:");
          break;
        case BLOCK_LAYOUT_WEAK:
          printf("BL_WEAK:");
          break;
        case BLOCK_LAYOUT_UNRETAINED:
          printf("BL_UNRETAINED:");
          break;
      }
      // Actual value of word count is one more that what is in the imm.
      // field of the instruction
      printf("%d", (inst & 0xf) + delta);
      if (i < e-1)
        printf(", ");
      else
        printf("\n");
    }
  }

  llvm::GlobalVariable *Entry = CreateMetadataVar(
      "OBJC_CLASS_NAME_",
      llvm::ConstantDataArray::getString(VMContext, BitMap, false),
      "__TEXT,__objc_classname,cstring_literals", CharUnits::One(), true);
  return getConstantGEP(VMContext, Entry, 0, 0);
}

llvm::Constant *CGObjCCommonMac::BuildRCBlockLayout(CodeGenModule &CGM,
                                                    const CGBlockInfo &blockInfo) {
  assert(CGM.getLangOpts().getGC() == LangOptions::NonGC);
  
  RunSkipBlockVars.clear();
  bool hasUnion = false;
  
  unsigned WordSizeInBits = CGM.getTarget().getPointerWidth(0);
  unsigned ByteSizeInBits = CGM.getTarget().getCharWidth();
  unsigned WordSizeInBytes = WordSizeInBits/ByteSizeInBits;
  
  const BlockDecl *blockDecl = blockInfo.getBlockDecl();
  
  // Calculate the basic layout of the block structure.
  const llvm::StructLayout *layout =
  CGM.getDataLayout().getStructLayout(blockInfo.StructureType);
  
  // Ignore the optional 'this' capture: C++ objects are not assumed
  // to be GC'ed.
  if (blockInfo.BlockHeaderForcedGapSize != CharUnits::Zero())
    UpdateRunSkipBlockVars(false, Qualifiers::OCL_None,
                           blockInfo.BlockHeaderForcedGapOffset,
                           blockInfo.BlockHeaderForcedGapSize);
  // Walk the captured variables.
  for (const auto &CI : blockDecl->captures()) {
    const VarDecl *variable = CI.getVariable();
    QualType type = variable->getType();
    
    const CGBlockInfo::Capture &capture = blockInfo.getCapture(variable);
    
    // Ignore constant captures.
    if (capture.isConstant()) continue;
    
    CharUnits fieldOffset =
       CharUnits::fromQuantity(layout->getElementOffset(capture.getIndex()));
    
    assert(!type->isArrayType() && "array variable should not be caught");
    if (!CI.isByRef())
      if (const RecordType *record = type->getAs<RecordType>()) {
        BuildRCBlockVarRecordLayout(record, fieldOffset, hasUnion);
        continue;
      }
    CharUnits fieldSize;
    if (CI.isByRef())
      fieldSize = CharUnits::fromQuantity(WordSizeInBytes);
    else
      fieldSize = CGM.getContext().getTypeSizeInChars(type);
    UpdateRunSkipBlockVars(CI.isByRef(), getBlockCaptureLifetime(type, false),
                           fieldOffset, fieldSize);
  }
  return getBitmapBlockLayout(false);
}

llvm::Constant *CGObjCCommonMac::BuildByrefLayout(CodeGen::CodeGenModule &CGM,
                                                  QualType T) {
  assert(CGM.getLangOpts().getGC() == LangOptions::NonGC);
  assert(!T->isArrayType() && "__block array variable should not be caught");
  CharUnits fieldOffset;
  RunSkipBlockVars.clear();
  bool hasUnion = false;
  if (const RecordType *record = T->getAs<RecordType>()) {
    BuildRCBlockVarRecordLayout(record, fieldOffset, hasUnion, true /*ByrefLayout */);
    llvm::Constant *Result = getBitmapBlockLayout(true);
    if (isa<llvm::ConstantInt>(Result))
      Result = llvm::ConstantExpr::getIntToPtr(Result, CGM.Int8PtrTy);
    return Result;
  }
  llvm::Constant *nullPtr = llvm::Constant::getNullValue(CGM.Int8PtrTy);
  return nullPtr;
}

llvm::Value *CGObjCMac::GenerateProtocolRef(CodeGenFunction &CGF,
                                            const ObjCProtocolDecl *PD) {
  // FIXME: I don't understand why gcc generates this, or where it is
  // resolved. Investigate. Its also wasteful to look this up over and over.
  LazySymbols.insert(&CGM.getContext().Idents.get("Protocol"));

  return llvm::ConstantExpr::getBitCast(GetProtocolRef(PD),
                                        ObjCTypes.getExternalProtocolPtrTy());
}

void CGObjCCommonMac::GenerateProtocol(const ObjCProtocolDecl *PD) {
  // FIXME: We shouldn't need this, the protocol decl should contain enough
  // information to tell us whether this was a declaration or a definition.
  DefinedProtocols.insert(PD->getIdentifier());

  // If we have generated a forward reference to this protocol, emit
  // it now. Otherwise do nothing, the protocol objects are lazily
  // emitted.
  if (Protocols.count(PD->getIdentifier()))
    GetOrEmitProtocol(PD);
}

llvm::Constant *CGObjCCommonMac::GetProtocolRef(const ObjCProtocolDecl *PD) {
  if (DefinedProtocols.count(PD->getIdentifier()))
    return GetOrEmitProtocol(PD);
  
  return GetOrEmitProtocolRef(PD);
}

llvm::Value *CGObjCCommonMac::EmitClassRefViaRuntime(
               CodeGenFunction &CGF,
               const ObjCInterfaceDecl *ID,
               ObjCCommonTypesHelper &ObjCTypes) {
  llvm::Constant *lookUpClassFn = ObjCTypes.getLookUpClassFn();

  llvm::Value *className =
      CGF.CGM.GetAddrOfConstantCString(ID->getObjCRuntimeNameAsString())
        .getPointer();
  ASTContext &ctx = CGF.CGM.getContext();
  className =
      CGF.Builder.CreateBitCast(className,
                                CGF.ConvertType(
                                  ctx.getPointerType(ctx.CharTy.withConst())));
  llvm::CallInst *call = CGF.Builder.CreateCall(lookUpClassFn, className);
  call->setDoesNotThrow();
  return call;
}

/*
// Objective-C 1.0 extensions
struct _objc_protocol {
struct _objc_protocol_extension *isa;
char *protocol_name;
struct _objc_protocol_list *protocol_list;
struct _objc__method_prototype_list *instance_methods;
struct _objc__method_prototype_list *class_methods
};

See EmitProtocolExtension().
*/
llvm::Constant *CGObjCMac::GetOrEmitProtocol(const ObjCProtocolDecl *PD) {
  llvm::GlobalVariable *Entry = Protocols[PD->getIdentifier()];

  // Early exit if a defining object has already been generated.
  if (Entry && Entry->hasInitializer())
    return Entry;

  // Use the protocol definition, if there is one.
  if (const ObjCProtocolDecl *Def = PD->getDefinition())
    PD = Def;

  // FIXME: I don't understand why gcc generates this, or where it is
  // resolved. Investigate. Its also wasteful to look this up over and over.
  LazySymbols.insert(&CGM.getContext().Idents.get("Protocol"));

  // Construct method lists.
  std::vector<llvm::Constant*> InstanceMethods, ClassMethods;
  std::vector<llvm::Constant*> OptInstanceMethods, OptClassMethods;
  std::vector<llvm::Constant*> MethodTypesExt, OptMethodTypesExt;
  for (const auto *MD : PD->instance_methods()) {
    llvm::Constant *C = GetMethodDescriptionConstant(MD);
    if (!C)
      return GetOrEmitProtocolRef(PD);
    
    if (MD->getImplementationControl() == ObjCMethodDecl::Optional) {
      OptInstanceMethods.push_back(C);
      OptMethodTypesExt.push_back(GetMethodVarType(MD, true));
    } else {
      InstanceMethods.push_back(C);
      MethodTypesExt.push_back(GetMethodVarType(MD, true));
    }
  }

  for (const auto *MD : PD->class_methods()) {
    llvm::Constant *C = GetMethodDescriptionConstant(MD);
    if (!C)
      return GetOrEmitProtocolRef(PD);

    if (MD->getImplementationControl() == ObjCMethodDecl::Optional) {
      OptClassMethods.push_back(C);
      OptMethodTypesExt.push_back(GetMethodVarType(MD, true));
    } else {
      ClassMethods.push_back(C);
      MethodTypesExt.push_back(GetMethodVarType(MD, true));
    }
  }

  MethodTypesExt.insert(MethodTypesExt.end(),
                        OptMethodTypesExt.begin(), OptMethodTypesExt.end());

  llvm::Constant *Values[] = {
      EmitProtocolExtension(PD, OptInstanceMethods, OptClassMethods,
                            MethodTypesExt),
      GetClassName(PD->getObjCRuntimeNameAsString()),
      EmitProtocolList("OBJC_PROTOCOL_REFS_" + PD->getName(),
                       PD->protocol_begin(), PD->protocol_end()),
      EmitMethodDescList("OBJC_PROTOCOL_INSTANCE_METHODS_" + PD->getName(),
                         "__OBJC,__cat_inst_meth,regular,no_dead_strip",
                         InstanceMethods),
      EmitMethodDescList("OBJC_PROTOCOL_CLASS_METHODS_" + PD->getName(),
                         "__OBJC,__cat_cls_meth,regular,no_dead_strip",
                         ClassMethods)};
  llvm::Constant *Init = llvm::ConstantStruct::get(ObjCTypes.ProtocolTy,
                                                   Values);

  if (Entry) {
    // Already created, update the initializer.
    assert(Entry->hasPrivateLinkage());
    Entry->setInitializer(Init);
  } else {
    Entry = new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ProtocolTy,
                                     false, llvm::GlobalValue::PrivateLinkage,
                                     Init, "OBJC_PROTOCOL_" + PD->getName());
    Entry->setSection("__OBJC,__protocol,regular,no_dead_strip");
    // FIXME: Is this necessary? Why only for protocol?
    Entry->setAlignment(4);

    Protocols[PD->getIdentifier()] = Entry;
  }
  CGM.addCompilerUsedGlobal(Entry);

  return Entry;
}

llvm::Constant *CGObjCMac::GetOrEmitProtocolRef(const ObjCProtocolDecl *PD) {
  llvm::GlobalVariable *&Entry = Protocols[PD->getIdentifier()];

  if (!Entry) {
    // We use the initializer as a marker of whether this is a forward
    // reference or not. At module finalization we add the empty
    // contents for protocols which were referenced but never defined.
    Entry = new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ProtocolTy,
                                     false, llvm::GlobalValue::PrivateLinkage,
                                     nullptr, "OBJC_PROTOCOL_" + PD->getName());
    Entry->setSection("__OBJC,__protocol,regular,no_dead_strip");
    // FIXME: Is this necessary? Why only for protocol?
    Entry->setAlignment(4);
  }

  return Entry;
}

/*
  struct _objc_protocol_extension {
  uint32_t size;
  struct objc_method_description_list *optional_instance_methods;
  struct objc_method_description_list *optional_class_methods;
  struct objc_property_list *instance_properties;
  const char ** extendedMethodTypes;
  struct objc_property_list *class_properties;
  };
*/
llvm::Constant *
CGObjCMac::EmitProtocolExtension(const ObjCProtocolDecl *PD,
                                 ArrayRef<llvm::Constant*> OptInstanceMethods,
                                 ArrayRef<llvm::Constant*> OptClassMethods,
                                 ArrayRef<llvm::Constant*> MethodTypesExt) {
  uint64_t Size =
    CGM.getDataLayout().getTypeAllocSize(ObjCTypes.ProtocolExtensionTy);
  llvm::Constant *Values[] = {
      llvm::ConstantInt::get(ObjCTypes.IntTy, Size),
      EmitMethodDescList("OBJC_PROTOCOL_INSTANCE_METHODS_OPT_" + PD->getName(),
                         "__OBJC,__cat_inst_meth,regular,no_dead_strip",
                         OptInstanceMethods),
      EmitMethodDescList("OBJC_PROTOCOL_CLASS_METHODS_OPT_" + PD->getName(),
                         "__OBJC,__cat_cls_meth,regular,no_dead_strip",
                         OptClassMethods),
      EmitPropertyList("OBJC_$_PROP_PROTO_LIST_" + PD->getName(), nullptr, PD,
                       ObjCTypes, false),
      EmitProtocolMethodTypes("OBJC_PROTOCOL_METHOD_TYPES_" + PD->getName(),
                              MethodTypesExt, ObjCTypes),
      EmitPropertyList("OBJC_$_CLASS_PROP_PROTO_LIST_" + PD->getName(), nullptr,
                       PD, ObjCTypes, true)};

  // Return null if no extension bits are used.
  if (Values[1]->isNullValue() && Values[2]->isNullValue() &&
      Values[3]->isNullValue() && Values[4]->isNullValue() &&
      Values[5]->isNullValue())
    return llvm::Constant::getNullValue(ObjCTypes.ProtocolExtensionPtrTy);

  llvm::Constant *Init =
    llvm::ConstantStruct::get(ObjCTypes.ProtocolExtensionTy, Values);

  // No special section, but goes in llvm.used
  return CreateMetadataVar("\01l_OBJC_PROTOCOLEXT_" + PD->getName(), Init,
                           StringRef(), CGM.getPointerAlign(), true);
}

/*
  struct objc_protocol_list {
    struct objc_protocol_list *next;
    long count;
    Protocol *list[];
  };
*/
llvm::Constant *
CGObjCMac::EmitProtocolList(Twine Name,
                            ObjCProtocolDecl::protocol_iterator begin,
                            ObjCProtocolDecl::protocol_iterator end) {
  SmallVector<llvm::Constant *, 16> ProtocolRefs;

  for (; begin != end; ++begin)
    ProtocolRefs.push_back(GetProtocolRef(*begin));

  // Just return null for empty protocol lists
  if (ProtocolRefs.empty())
    return llvm::Constant::getNullValue(ObjCTypes.ProtocolListPtrTy);

  // This list is null terminated.
  ProtocolRefs.push_back(llvm::Constant::getNullValue(ObjCTypes.ProtocolPtrTy));

  llvm::Constant *Values[3];
  // This field is only used by the runtime.
  Values[0] = llvm::Constant::getNullValue(ObjCTypes.ProtocolListPtrTy);
  Values[1] = llvm::ConstantInt::get(ObjCTypes.LongTy,
                                     ProtocolRefs.size() - 1);
  Values[2] =
    llvm::ConstantArray::get(llvm::ArrayType::get(ObjCTypes.ProtocolPtrTy,
                                                  ProtocolRefs.size()),
                             ProtocolRefs);

  llvm::Constant *Init = llvm::ConstantStruct::getAnon(Values);
  llvm::GlobalVariable *GV =
    CreateMetadataVar(Name, Init, "__OBJC,__cat_cls_meth,regular,no_dead_strip",
                      CGM.getPointerAlign(), false);
  return llvm::ConstantExpr::getBitCast(GV, ObjCTypes.ProtocolListPtrTy);
}

void CGObjCCommonMac::
PushProtocolProperties(llvm::SmallPtrSet<const IdentifierInfo*,16> &PropertySet,
                       SmallVectorImpl<llvm::Constant *> &Properties,
                       const Decl *Container,
                       const ObjCProtocolDecl *Proto,
                       const ObjCCommonTypesHelper &ObjCTypes,
                       bool IsClassProperty) {
  for (const auto *P : Proto->protocols()) 
    PushProtocolProperties(PropertySet, Properties, Container, P, ObjCTypes,
                           IsClassProperty);

  for (const auto *PD : Proto->properties()) {
    if (IsClassProperty != PD->isClassProperty())
      continue;
    if (!PropertySet.insert(PD->getIdentifier()).second)
      continue;
    llvm::Constant *Prop[] = {
      GetPropertyName(PD->getIdentifier()),
      GetPropertyTypeString(PD, Container)
    };
    Properties.push_back(llvm::ConstantStruct::get(ObjCTypes.PropertyTy, Prop));
  }
}

/*
  struct _objc_property {
    const char * const name;
    const char * const attributes;
  };

  struct _objc_property_list {
    uint32_t entsize; // sizeof (struct _objc_property)
    uint32_t prop_count;
    struct _objc_property[prop_count];
  };
*/
llvm::Constant *CGObjCCommonMac::EmitPropertyList(Twine Name,
                                       const Decl *Container,
                                       const ObjCContainerDecl *OCD,
                                       const ObjCCommonTypesHelper &ObjCTypes,
                                       bool IsClassProperty) {
  if (IsClassProperty) {
    // Make this entry NULL for OS X with deployment target < 10.11, for iOS
    // with deployment target < 9.0.
    const llvm::Triple &Triple = CGM.getTarget().getTriple();
    if ((Triple.isMacOSX() && Triple.isMacOSXVersionLT(10, 11)) ||
        (Triple.isiOS() && Triple.isOSVersionLT(9)))
      return llvm::Constant::getNullValue(ObjCTypes.PropertyListPtrTy);
  }

  SmallVector<llvm::Constant *, 16> Properties;
  llvm::SmallPtrSet<const IdentifierInfo*, 16> PropertySet;

  auto AddProperty = [&](const ObjCPropertyDecl *PD) {
    llvm::Constant *Prop[] = {GetPropertyName(PD->getIdentifier()),
                              GetPropertyTypeString(PD, Container)};
    Properties.push_back(llvm::ConstantStruct::get(ObjCTypes.PropertyTy, Prop));
  };
  if (const ObjCInterfaceDecl *OID = dyn_cast<ObjCInterfaceDecl>(OCD))
    for (const ObjCCategoryDecl *ClassExt : OID->known_extensions())
      for (auto *PD : ClassExt->properties()) {
        if (IsClassProperty != PD->isClassProperty())
          continue;
        PropertySet.insert(PD->getIdentifier());
        AddProperty(PD);
      }

  for (const auto *PD : OCD->properties()) {
    if (IsClassProperty != PD->isClassProperty())
      continue;
    // Don't emit duplicate metadata for properties that were already in a
    // class extension.
    if (!PropertySet.insert(PD->getIdentifier()).second)
      continue;
    AddProperty(PD);
  }

  if (const ObjCInterfaceDecl *OID = dyn_cast<ObjCInterfaceDecl>(OCD)) {
    for (const auto *P : OID->all_referenced_protocols())
      PushProtocolProperties(PropertySet, Properties, Container, P, ObjCTypes,
                             IsClassProperty);
  }
  else if (const ObjCCategoryDecl *CD = dyn_cast<ObjCCategoryDecl>(OCD)) {
    for (const auto *P : CD->protocols())
      PushProtocolProperties(PropertySet, Properties, Container, P, ObjCTypes,
                             IsClassProperty);
  }

  // Return null for empty list.
  if (Properties.empty())
    return llvm::Constant::getNullValue(ObjCTypes.PropertyListPtrTy);

  unsigned PropertySize =
    CGM.getDataLayout().getTypeAllocSize(ObjCTypes.PropertyTy);
  llvm::Constant *Values[3];
  Values[0] = llvm::ConstantInt::get(ObjCTypes.IntTy, PropertySize);
  Values[1] = llvm::ConstantInt::get(ObjCTypes.IntTy, Properties.size());
  llvm::ArrayType *AT = llvm::ArrayType::get(ObjCTypes.PropertyTy,
                                             Properties.size());
  Values[2] = llvm::ConstantArray::get(AT, Properties);
  llvm::Constant *Init = llvm::ConstantStruct::getAnon(Values);

  llvm::GlobalVariable *GV =
    CreateMetadataVar(Name, Init,
                      (ObjCABI == 2) ? "__DATA, __objc_const" :
                      "__OBJC,__property,regular,no_dead_strip",
                      CGM.getPointerAlign(),
                      true);
  return llvm::ConstantExpr::getBitCast(GV, ObjCTypes.PropertyListPtrTy);
}

llvm::Constant *
CGObjCCommonMac::EmitProtocolMethodTypes(Twine Name,
                                         ArrayRef<llvm::Constant*> MethodTypes,
                                         const ObjCCommonTypesHelper &ObjCTypes) {
  // Return null for empty list.
  if (MethodTypes.empty())
    return llvm::Constant::getNullValue(ObjCTypes.Int8PtrPtrTy);

  llvm::ArrayType *AT = llvm::ArrayType::get(ObjCTypes.Int8PtrTy,
                                             MethodTypes.size());
  llvm::Constant *Init = llvm::ConstantArray::get(AT, MethodTypes);

  llvm::GlobalVariable *GV = CreateMetadataVar(
      Name, Init, (ObjCABI == 2) ? "__DATA, __objc_const" : StringRef(),
      CGM.getPointerAlign(), true);
  return llvm::ConstantExpr::getBitCast(GV, ObjCTypes.Int8PtrPtrTy);
}

/*
  struct objc_method_description_list {
  int count;
  struct objc_method_description list[];
  };
*/
llvm::Constant *
CGObjCMac::GetMethodDescriptionConstant(const ObjCMethodDecl *MD) {
  llvm::Constant *Desc[] = {
    llvm::ConstantExpr::getBitCast(GetMethodVarName(MD->getSelector()),
                                   ObjCTypes.SelectorPtrTy),
    GetMethodVarType(MD)
  };
  if (!Desc[1])
    return nullptr;

  return llvm::ConstantStruct::get(ObjCTypes.MethodDescriptionTy,
                                   Desc);
}

llvm::Constant *
CGObjCMac::EmitMethodDescList(Twine Name, StringRef Section,
                              ArrayRef<llvm::Constant *> Methods) {
  // Return null for empty list.
  if (Methods.empty())
    return llvm::Constant::getNullValue(ObjCTypes.MethodDescriptionListPtrTy);

  llvm::Constant *Values[2];
  Values[0] = llvm::ConstantInt::get(ObjCTypes.IntTy, Methods.size());
  llvm::ArrayType *AT = llvm::ArrayType::get(ObjCTypes.MethodDescriptionTy,
                                             Methods.size());
  Values[1] = llvm::ConstantArray::get(AT, Methods);
  llvm::Constant *Init = llvm::ConstantStruct::getAnon(Values);

  llvm::GlobalVariable *GV =
    CreateMetadataVar(Name, Init, Section, CGM.getPointerAlign(), true);
  return llvm::ConstantExpr::getBitCast(GV,
                                        ObjCTypes.MethodDescriptionListPtrTy);
}

/*
  struct _objc_category {
  char *category_name;
  char *class_name;
  struct _objc_method_list *instance_methods;
  struct _objc_method_list *class_methods;
  struct _objc_protocol_list *protocols;
  uint32_t size; // <rdar://4585769>
  struct _objc_property_list *instance_properties;
  struct _objc_property_list *class_properties;
  };
*/
void CGObjCMac::GenerateCategory(const ObjCCategoryImplDecl *OCD) {
  unsigned Size = CGM.getDataLayout().getTypeAllocSize(ObjCTypes.CategoryTy);

  // FIXME: This is poor design, the OCD should have a pointer to the category
  // decl. Additionally, note that Category can be null for the @implementation
  // w/o an @interface case. Sema should just create one for us as it does for
  // @implementation so everyone else can live life under a clear blue sky.
  const ObjCInterfaceDecl *Interface = OCD->getClassInterface();
  const ObjCCategoryDecl *Category =
    Interface->FindCategoryDeclaration(OCD->getIdentifier());

  SmallString<256> ExtName;
  llvm::raw_svector_ostream(ExtName) << Interface->getName() << '_'
                                     << OCD->getName();

  SmallVector<llvm::Constant *, 16> InstanceMethods, ClassMethods;
  for (const auto *I : OCD->instance_methods())
    // Instance methods should always be defined.
    InstanceMethods.push_back(GetMethodConstant(I));

  for (const auto *I : OCD->class_methods())
    // Class methods should always be defined.
    ClassMethods.push_back(GetMethodConstant(I));

  llvm::Constant *Values[8];
  Values[0] = GetClassName(OCD->getName());
  Values[1] = GetClassName(Interface->getObjCRuntimeNameAsString());
  LazySymbols.insert(Interface->getIdentifier());
  Values[2] = EmitMethodList("OBJC_CATEGORY_INSTANCE_METHODS_" + ExtName.str(),
                             "__OBJC,__cat_inst_meth,regular,no_dead_strip",
                             InstanceMethods);
  Values[3] = EmitMethodList("OBJC_CATEGORY_CLASS_METHODS_" + ExtName.str(),
                             "__OBJC,__cat_cls_meth,regular,no_dead_strip",
                             ClassMethods);
  if (Category) {
    Values[4] =
        EmitProtocolList("OBJC_CATEGORY_PROTOCOLS_" + ExtName.str(),
                         Category->protocol_begin(), Category->protocol_end());
  } else {
    Values[4] = llvm::Constant::getNullValue(ObjCTypes.ProtocolListPtrTy);
  }
  Values[5] = llvm::ConstantInt::get(ObjCTypes.IntTy, Size);

  // If there is no category @interface then there can be no properties.
  if (Category) {
    Values[6] = EmitPropertyList("\01l_OBJC_$_PROP_LIST_" + ExtName.str(),
                                 OCD, Category, ObjCTypes, false);
    Values[7] = EmitPropertyList("\01l_OBJC_$_CLASS_PROP_LIST_" + ExtName.str(),
                                 OCD, Category, ObjCTypes, true);
  } else {
    Values[6] = llvm::Constant::getNullValue(ObjCTypes.PropertyListPtrTy);
    Values[7] = llvm::Constant::getNullValue(ObjCTypes.PropertyListPtrTy);
  }

  llvm::Constant *Init = llvm::ConstantStruct::get(ObjCTypes.CategoryTy,
                                                   Values);

  llvm::GlobalVariable *GV =
      CreateMetadataVar("OBJC_CATEGORY_" + ExtName.str(), Init,
                        "__OBJC,__category,regular,no_dead_strip",
                        CGM.getPointerAlign(), true);
  DefinedCategories.push_back(GV);
  DefinedCategoryNames.insert(ExtName.str());
  // method definition entries must be clear for next implementation.
  MethodDefinitions.clear();
}

enum FragileClassFlags {
  /// Apparently: is not a meta-class.
  FragileABI_Class_Factory                 = 0x00001,

  /// Is a meta-class.
  FragileABI_Class_Meta                    = 0x00002,

  /// Has a non-trivial constructor or destructor.
  FragileABI_Class_HasCXXStructors         = 0x02000,

  /// Has hidden visibility.
  FragileABI_Class_Hidden                  = 0x20000,

  /// Class implementation was compiled under ARC.
  FragileABI_Class_CompiledByARC           = 0x04000000,

  /// Class implementation was compiled under MRC and has MRC weak ivars.
  /// Exclusive with CompiledByARC.
  FragileABI_Class_HasMRCWeakIvars         = 0x08000000,
};

enum NonFragileClassFlags {
  /// Is a meta-class.
  NonFragileABI_Class_Meta                 = 0x00001,

  /// Is a root class.
  NonFragileABI_Class_Root                 = 0x00002,

  /// Has a non-trivial constructor or destructor.
  NonFragileABI_Class_HasCXXStructors      = 0x00004,

  /// Has hidden visibility.
  NonFragileABI_Class_Hidden               = 0x00010,

  /// Has the exception attribute.
  NonFragileABI_Class_Exception            = 0x00020,

  /// (Obsolete) ARC-specific: this class has a .release_ivars method
  NonFragileABI_Class_HasIvarReleaser      = 0x00040,

  /// Class implementation was compiled under ARC.
  NonFragileABI_Class_CompiledByARC        = 0x00080,

  /// Class has non-trivial destructors, but zero-initialization is okay.
  NonFragileABI_Class_HasCXXDestructorOnly = 0x00100,

  /// Class implementation was compiled under MRC and has MRC weak ivars.
  /// Exclusive with CompiledByARC.
  NonFragileABI_Class_HasMRCWeakIvars      = 0x00200,
};

static bool hasWeakMember(QualType type) {
  if (type.getObjCLifetime() == Qualifiers::OCL_Weak) {
    return true;
  }

  if (auto recType = type->getAs<RecordType>()) {
    for (auto field : recType->getDecl()->fields()) {
      if (hasWeakMember(field->getType()))
        return true;
    }
  }

  return false;
}

/// For compatibility, we only want to set the "HasMRCWeakIvars" flag
/// (and actually fill in a layout string) if we really do have any
/// __weak ivars.
static bool hasMRCWeakIvars(CodeGenModule &CGM,
                            const ObjCImplementationDecl *ID) {
  if (!CGM.getLangOpts().ObjCWeak) return false;
  assert(CGM.getLangOpts().getGC() == LangOptions::NonGC);

  for (const ObjCIvarDecl *ivar =
         ID->getClassInterface()->all_declared_ivar_begin();
       ivar; ivar = ivar->getNextIvar()) {
    if (hasWeakMember(ivar->getType()))
      return true;
  }

  return false;
}

/*
  struct _objc_class {
  Class isa;
  Class super_class;
  const char *name;
  long version;
  long info;
  long instance_size;
  struct _objc_ivar_list *ivars;
  struct _objc_method_list *methods;
  struct _objc_cache *cache;
  struct _objc_protocol_list *protocols;
  // Objective-C 1.0 extensions (<rdr://4585769>)
  const char *ivar_layout;
  struct _objc_class_ext *ext;
  };

  See EmitClassExtension();
*/
void CGObjCMac::GenerateClass(const ObjCImplementationDecl *ID) {
  DefinedSymbols.insert(ID->getIdentifier());

  std::string ClassName = ID->getNameAsString();
  // FIXME: Gross
  ObjCInterfaceDecl *Interface =
    const_cast<ObjCInterfaceDecl*>(ID->getClassInterface());
  llvm::Constant *Protocols =
      EmitProtocolList("OBJC_CLASS_PROTOCOLS_" + ID->getName(),
                       Interface->all_referenced_protocol_begin(),
                       Interface->all_referenced_protocol_end());
  unsigned Flags = FragileABI_Class_Factory;
  if (ID->hasNonZeroConstructors() || ID->hasDestructors())
    Flags |= FragileABI_Class_HasCXXStructors;

  bool hasMRCWeak = false;

  if (CGM.getLangOpts().ObjCAutoRefCount)
    Flags |= FragileABI_Class_CompiledByARC;
  else if ((hasMRCWeak = hasMRCWeakIvars(CGM, ID)))
    Flags |= FragileABI_Class_HasMRCWeakIvars;

  CharUnits Size =
    CGM.getContext().getASTObjCImplementationLayout(ID).getSize();

  // FIXME: Set CXX-structors flag.
  if (ID->getClassInterface()->getVisibility() == HiddenVisibility)
    Flags |= FragileABI_Class_Hidden;

  SmallVector<llvm::Constant *, 16> InstanceMethods, ClassMethods;
  for (const auto *I : ID->instance_methods())
    // Instance methods should always be defined.
    InstanceMethods.push_back(GetMethodConstant(I));

  for (const auto *I : ID->class_methods())
    // Class methods should always be defined.
    ClassMethods.push_back(GetMethodConstant(I));

  for (const auto *PID : ID->property_impls()) {
    if (PID->getPropertyImplementation() == ObjCPropertyImplDecl::Synthesize) {
      ObjCPropertyDecl *PD = PID->getPropertyDecl();

      if (ObjCMethodDecl *MD = PD->getGetterMethodDecl())
        if (llvm::Constant *C = GetMethodConstant(MD))
          InstanceMethods.push_back(C);
      if (ObjCMethodDecl *MD = PD->getSetterMethodDecl())
        if (llvm::Constant *C = GetMethodConstant(MD))
          InstanceMethods.push_back(C);
    }
  }

  llvm::Constant *Values[12];
  Values[ 0] = EmitMetaClass(ID, Protocols, ClassMethods);
  if (ObjCInterfaceDecl *Super = Interface->getSuperClass()) {
    // Record a reference to the super class.
    LazySymbols.insert(Super->getIdentifier());

    Values[ 1] =
      llvm::ConstantExpr::getBitCast(GetClassName(Super->getObjCRuntimeNameAsString()),
                                     ObjCTypes.ClassPtrTy);
  } else {
    Values[ 1] = llvm::Constant::getNullValue(ObjCTypes.ClassPtrTy);
  }
  Values[ 2] = GetClassName(ID->getObjCRuntimeNameAsString());
  // Version is always 0.
  Values[ 3] = llvm::ConstantInt::get(ObjCTypes.LongTy, 0);
  Values[ 4] = llvm::ConstantInt::get(ObjCTypes.LongTy, Flags);
  Values[ 5] = llvm::ConstantInt::get(ObjCTypes.LongTy, Size.getQuantity());
  Values[ 6] = EmitIvarList(ID, false);
  Values[7] = EmitMethodList("OBJC_INSTANCE_METHODS_" + ID->getName(),
                             "__OBJC,__inst_meth,regular,no_dead_strip",
                             InstanceMethods);
  // cache is always NULL.
  Values[ 8] = llvm::Constant::getNullValue(ObjCTypes.CachePtrTy);
  Values[ 9] = Protocols;
  Values[10] = BuildStrongIvarLayout(ID, CharUnits::Zero(), Size);
  Values[11] = EmitClassExtension(ID, Size, hasMRCWeak,
                                  false/*isClassProperty*/);
  llvm::Constant *Init = llvm::ConstantStruct::get(ObjCTypes.ClassTy,
                                                   Values);
  std::string Name("OBJC_CLASS_");
  Name += ClassName;
  const char *Section = "__OBJC,__class,regular,no_dead_strip";
  // Check for a forward reference.
  llvm::GlobalVariable *GV = CGM.getModule().getGlobalVariable(Name, true);
  if (GV) {
    assert(GV->getType()->getElementType() == ObjCTypes.ClassTy &&
           "Forward metaclass reference has incorrect type.");
    GV->setInitializer(Init);
    GV->setSection(Section);
    GV->setAlignment(CGM.getPointerAlign().getQuantity());
    CGM.addCompilerUsedGlobal(GV);
  } else
    GV = CreateMetadataVar(Name, Init, Section, CGM.getPointerAlign(), true);
  DefinedClasses.push_back(GV);
  ImplementedClasses.push_back(Interface);
  // method definition entries must be clear for next implementation.
  MethodDefinitions.clear();
}

llvm::Constant *CGObjCMac::EmitMetaClass(const ObjCImplementationDecl *ID,
                                         llvm::Constant *Protocols,
                                         ArrayRef<llvm::Constant*> Methods) {
  unsigned Flags = FragileABI_Class_Meta;
  unsigned Size = CGM.getDataLayout().getTypeAllocSize(ObjCTypes.ClassTy);

  if (ID->getClassInterface()->getVisibility() == HiddenVisibility)
    Flags |= FragileABI_Class_Hidden;

  llvm::Constant *Values[12];
  // The isa for the metaclass is the root of the hierarchy.
  const ObjCInterfaceDecl *Root = ID->getClassInterface();
  while (const ObjCInterfaceDecl *Super = Root->getSuperClass())
    Root = Super;
  Values[ 0] =
    llvm::ConstantExpr::getBitCast(GetClassName(Root->getObjCRuntimeNameAsString()),
                                   ObjCTypes.ClassPtrTy);
  // The super class for the metaclass is emitted as the name of the
  // super class. The runtime fixes this up to point to the
  // *metaclass* for the super class.
  if (ObjCInterfaceDecl *Super = ID->getClassInterface()->getSuperClass()) {
    Values[ 1] =
      llvm::ConstantExpr::getBitCast(GetClassName(Super->getObjCRuntimeNameAsString()),
                                     ObjCTypes.ClassPtrTy);
  } else {
    Values[ 1] = llvm::Constant::getNullValue(ObjCTypes.ClassPtrTy);
  }
  Values[ 2] = GetClassName(ID->getObjCRuntimeNameAsString());
  // Version is always 0.
  Values[ 3] = llvm::ConstantInt::get(ObjCTypes.LongTy, 0);
  Values[ 4] = llvm::ConstantInt::get(ObjCTypes.LongTy, Flags);
  Values[ 5] = llvm::ConstantInt::get(ObjCTypes.LongTy, Size);
  Values[ 6] = EmitIvarList(ID, true);
  Values[7] =
      EmitMethodList("OBJC_CLASS_METHODS_" + ID->getNameAsString(),
                     "__OBJC,__cls_meth,regular,no_dead_strip", Methods);
  // cache is always NULL.
  Values[ 8] = llvm::Constant::getNullValue(ObjCTypes.CachePtrTy);
  Values[ 9] = Protocols;
  // ivar_layout for metaclass is always NULL.
  Values[10] = llvm::Constant::getNullValue(ObjCTypes.Int8PtrTy);
  // The class extension is used to store class properties for metaclasses.
  Values[11] = EmitClassExtension(ID, CharUnits::Zero(), false/*hasMRCWeak*/,
                                  true/*isClassProperty*/);
  llvm::Constant *Init = llvm::ConstantStruct::get(ObjCTypes.ClassTy,
                                                   Values);

  std::string Name("OBJC_METACLASS_");
  Name += ID->getName();

  // Check for a forward reference.
  llvm::GlobalVariable *GV = CGM.getModule().getGlobalVariable(Name, true);
  if (GV) {
    assert(GV->getType()->getElementType() == ObjCTypes.ClassTy &&
           "Forward metaclass reference has incorrect type.");
    GV->setInitializer(Init);
  } else {
    GV = new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ClassTy, false,
                                  llvm::GlobalValue::PrivateLinkage,
                                  Init, Name);
  }
  GV->setSection("__OBJC,__meta_class,regular,no_dead_strip");
  GV->setAlignment(4);
  CGM.addCompilerUsedGlobal(GV);

  return GV;
}

llvm::Constant *CGObjCMac::EmitMetaClassRef(const ObjCInterfaceDecl *ID) {
  std::string Name = "OBJC_METACLASS_" + ID->getNameAsString();

  // FIXME: Should we look these up somewhere other than the module. Its a bit
  // silly since we only generate these while processing an implementation, so
  // exactly one pointer would work if know when we entered/exitted an
  // implementation block.

  // Check for an existing forward reference.
  // Previously, metaclass with internal linkage may have been defined.
  // pass 'true' as 2nd argument so it is returned.
  llvm::GlobalVariable *GV = CGM.getModule().getGlobalVariable(Name, true);
  if (!GV)
    GV = new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ClassTy, false,
                                  llvm::GlobalValue::PrivateLinkage, nullptr,
                                  Name);

  assert(GV->getType()->getElementType() == ObjCTypes.ClassTy &&
         "Forward metaclass reference has incorrect type.");
  return GV;
}

llvm::Value *CGObjCMac::EmitSuperClassRef(const ObjCInterfaceDecl *ID) {
  std::string Name = "OBJC_CLASS_" + ID->getNameAsString();
  llvm::GlobalVariable *GV = CGM.getModule().getGlobalVariable(Name, true);

  if (!GV)
    GV = new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ClassTy, false,
                                  llvm::GlobalValue::PrivateLinkage, nullptr,
                                  Name);

  assert(GV->getType()->getElementType() == ObjCTypes.ClassTy &&
         "Forward class metadata reference has incorrect type.");
  return GV;
}

/*
  Emit a "class extension", which in this specific context means extra
  data that doesn't fit in the normal fragile-ABI class structure, and
  has nothing to do with the language concept of a class extension.

  struct objc_class_ext {
  uint32_t size;
  const char *weak_ivar_layout;
  struct _objc_property_list *properties;
  };
*/
llvm::Constant *
CGObjCMac::EmitClassExtension(const ObjCImplementationDecl *ID,
                              CharUnits InstanceSize, bool hasMRCWeakIvars,
                              bool isClassProperty) {
  uint64_t Size =
    CGM.getDataLayout().getTypeAllocSize(ObjCTypes.ClassExtensionTy);

  llvm::Constant *Values[3];
  Values[0] = llvm::ConstantInt::get(ObjCTypes.IntTy, Size);
  if (isClassProperty) {
    llvm::Type *PtrTy = CGM.Int8PtrTy;
    Values[1] = llvm::Constant::getNullValue(PtrTy);
  } else
    Values[1] = BuildWeakIvarLayout(ID, CharUnits::Zero(), InstanceSize,
                                    hasMRCWeakIvars);
  if (isClassProperty)
    Values[2] = EmitPropertyList("\01l_OBJC_$_CLASS_PROP_LIST_" + ID->getName(),
                                 ID, ID->getClassInterface(), ObjCTypes, true);
  else
    Values[2] = EmitPropertyList("\01l_OBJC_$_PROP_LIST_" + ID->getName(),
                                 ID, ID->getClassInterface(), ObjCTypes, false);

  // Return null if no extension bits are used.
  if ((!Values[1] || Values[1]->isNullValue()) && Values[2]->isNullValue())
    return llvm::Constant::getNullValue(ObjCTypes.ClassExtensionPtrTy);

  llvm::Constant *Init =
    llvm::ConstantStruct::get(ObjCTypes.ClassExtensionTy, Values);
  return CreateMetadataVar("OBJC_CLASSEXT_" + ID->getName(), Init,
                           "__OBJC,__class_ext,regular,no_dead_strip",
                           CGM.getPointerAlign(), true);
}

/*
  struct objc_ivar {
    char *ivar_name;
    char *ivar_type;
    int ivar_offset;
  };

  struct objc_ivar_list {
    int ivar_count;
    struct objc_ivar list[count];
  };
*/
llvm::Constant *CGObjCMac::EmitIvarList(const ObjCImplementationDecl *ID,
                                        bool ForClass) {
  std::vector<llvm::Constant*> Ivars;

  // When emitting the root class GCC emits ivar entries for the
  // actual class structure. It is not clear if we need to follow this
  // behavior; for now lets try and get away with not doing it. If so,
  // the cleanest solution would be to make up an ObjCInterfaceDecl
  // for the class.
  if (ForClass)
    return llvm::Constant::getNullValue(ObjCTypes.IvarListPtrTy);

  const ObjCInterfaceDecl *OID = ID->getClassInterface();

  for (const ObjCIvarDecl *IVD = OID->all_declared_ivar_begin(); 
       IVD; IVD = IVD->getNextIvar()) {
    // Ignore unnamed bit-fields.
    if (!IVD->getDeclName())
      continue;
    llvm::Constant *Ivar[] = {
      GetMethodVarName(IVD->getIdentifier()),
      GetMethodVarType(IVD),
      llvm::ConstantInt::get(ObjCTypes.IntTy,
                             ComputeIvarBaseOffset(CGM, OID, IVD))
    };
    Ivars.push_back(llvm::ConstantStruct::get(ObjCTypes.IvarTy, Ivar));
  }

  // Return null for empty list.
  if (Ivars.empty())
    return llvm::Constant::getNullValue(ObjCTypes.IvarListPtrTy);

  llvm::Constant *Values[2];
  Values[0] = llvm::ConstantInt::get(ObjCTypes.IntTy, Ivars.size());
  llvm::ArrayType *AT = llvm::ArrayType::get(ObjCTypes.IvarTy,
                                             Ivars.size());
  Values[1] = llvm::ConstantArray::get(AT, Ivars);
  llvm::Constant *Init = llvm::ConstantStruct::getAnon(Values);

  llvm::GlobalVariable *GV;
  if (ForClass)
    GV =
        CreateMetadataVar("OBJC_CLASS_VARIABLES_" + ID->getName(), Init,
                          "__OBJC,__class_vars,regular,no_dead_strip",
                          CGM.getPointerAlign(), true);
  else
    GV = CreateMetadataVar("OBJC_INSTANCE_VARIABLES_" + ID->getName(), Init,
                           "__OBJC,__instance_vars,regular,no_dead_strip",
                           CGM.getPointerAlign(), true);
  return llvm::ConstantExpr::getBitCast(GV, ObjCTypes.IvarListPtrTy);
}

/*
  struct objc_method {
  SEL method_name;
  char *method_types;
  void *method;
  };

  struct objc_method_list {
  struct objc_method_list *obsolete;
  int count;
  struct objc_method methods_list[count];
  };
*/

/// GetMethodConstant - Return a struct objc_method constant for the
/// given method if it has been defined. The result is null if the
/// method has not been defined. The return value has type MethodPtrTy.
llvm::Constant *CGObjCMac::GetMethodConstant(const ObjCMethodDecl *MD) {
  llvm::Function *Fn = GetMethodDefinition(MD);
  if (!Fn)
    return nullptr;

  llvm::Constant *Method[] = {
    llvm::ConstantExpr::getBitCast(GetMethodVarName(MD->getSelector()),
                                   ObjCTypes.SelectorPtrTy),
    GetMethodVarType(MD),
    llvm::ConstantExpr::getBitCast(Fn, ObjCTypes.Int8PtrTy)
  };
  return llvm::ConstantStruct::get(ObjCTypes.MethodTy, Method);
}

llvm::Constant *CGObjCMac::EmitMethodList(Twine Name, StringRef Section,
                                          ArrayRef<llvm::Constant *> Methods) {
  // Return null for empty list.
  if (Methods.empty())
    return llvm::Constant::getNullValue(ObjCTypes.MethodListPtrTy);

  llvm::Constant *Values[3];
  Values[0] = llvm::Constant::getNullValue(ObjCTypes.Int8PtrTy);
  Values[1] = llvm::ConstantInt::get(ObjCTypes.IntTy, Methods.size());
  llvm::ArrayType *AT = llvm::ArrayType::get(ObjCTypes.MethodTy,
                                             Methods.size());
  Values[2] = llvm::ConstantArray::get(AT, Methods);
  llvm::Constant *Init = llvm::ConstantStruct::getAnon(Values);

  llvm::GlobalVariable *GV =
    CreateMetadataVar(Name, Init, Section, CGM.getPointerAlign(), true);
  return llvm::ConstantExpr::getBitCast(GV, ObjCTypes.MethodListPtrTy);
}

llvm::Function *CGObjCCommonMac::GenerateMethod(const ObjCMethodDecl *OMD,
                                                const ObjCContainerDecl *CD) {
  SmallString<256> Name;
  GetNameForMethod(OMD, CD, Name);

  CodeGenTypes &Types = CGM.getTypes();
  llvm::FunctionType *MethodTy =
    Types.GetFunctionType(Types.arrangeObjCMethodDeclaration(OMD));
  llvm::Function *Method =
    llvm::Function::Create(MethodTy,
                           llvm::GlobalValue::InternalLinkage,
                           Name.str(),
                           &CGM.getModule());
  MethodDefinitions.insert(std::make_pair(OMD, Method));

  return Method;
}

llvm::GlobalVariable *CGObjCCommonMac::CreateMetadataVar(Twine Name,
                                                         llvm::Constant *Init,
                                                         StringRef Section,
                                                         CharUnits Align,
                                                         bool AddToUsed) {
  llvm::Type *Ty = Init->getType();
  llvm::GlobalVariable *GV =
    new llvm::GlobalVariable(CGM.getModule(), Ty, false,
                             llvm::GlobalValue::PrivateLinkage, Init, Name);
  if (!Section.empty())
    GV->setSection(Section);
  GV->setAlignment(Align.getQuantity());
  if (AddToUsed)
    CGM.addCompilerUsedGlobal(GV);
  return GV;
}

llvm::Function *CGObjCMac::ModuleInitFunction() {
  // Abuse this interface function as a place to finalize.
  FinishModule();
  return nullptr;
}

llvm::Constant *CGObjCMac::GetPropertyGetFunction() {
  return ObjCTypes.getGetPropertyFn();
}

llvm::Constant *CGObjCMac::GetPropertySetFunction() {
  return ObjCTypes.getSetPropertyFn();
}

llvm::Constant *CGObjCMac::GetOptimizedPropertySetFunction(bool atomic, 
                                                           bool copy) {
  return ObjCTypes.getOptimizedSetPropertyFn(atomic, copy);
}

llvm::Constant *CGObjCMac::GetGetStructFunction() {
  return ObjCTypes.getCopyStructFn();
}

llvm::Constant *CGObjCMac::GetSetStructFunction() {
  return ObjCTypes.getCopyStructFn();
}

llvm::Constant *CGObjCMac::GetCppAtomicObjectGetFunction() {
  return ObjCTypes.getCppAtomicObjectFunction();
}

llvm::Constant *CGObjCMac::GetCppAtomicObjectSetFunction() {
  return ObjCTypes.getCppAtomicObjectFunction();
}

llvm::Constant *CGObjCMac::EnumerationMutationFunction() {
  return ObjCTypes.getEnumerationMutationFn();
}

void CGObjCMac::EmitTryStmt(CodeGenFunction &CGF, const ObjCAtTryStmt &S) {
  return EmitTryOrSynchronizedStmt(CGF, S);
}

void CGObjCMac::EmitSynchronizedStmt(CodeGenFunction &CGF,
                                     const ObjCAtSynchronizedStmt &S) {
  return EmitTryOrSynchronizedStmt(CGF, S);
}

namespace {
  struct PerformFragileFinally final : EHScopeStack::Cleanup {
    const Stmt &S;
    Address SyncArgSlot;
    Address CallTryExitVar;
    Address ExceptionData;
    ObjCTypesHelper &ObjCTypes;
    PerformFragileFinally(const Stmt *S,
                          Address SyncArgSlot,
                          Address CallTryExitVar,
                          Address ExceptionData,
                          ObjCTypesHelper *ObjCTypes)
      : S(*S), SyncArgSlot(SyncArgSlot), CallTryExitVar(CallTryExitVar),
        ExceptionData(ExceptionData), ObjCTypes(*ObjCTypes) {}

    void Emit(CodeGenFunction &CGF, Flags flags) override {
      // Check whether we need to call objc_exception_try_exit.
      // In optimized code, this branch will always be folded.
      llvm::BasicBlock *FinallyCallExit =
        CGF.createBasicBlock("finally.call_exit");
      llvm::BasicBlock *FinallyNoCallExit =
        CGF.createBasicBlock("finally.no_call_exit");
      CGF.Builder.CreateCondBr(CGF.Builder.CreateLoad(CallTryExitVar),
                               FinallyCallExit, FinallyNoCallExit);

      CGF.EmitBlock(FinallyCallExit);
      CGF.EmitNounwindRuntimeCall(ObjCTypes.getExceptionTryExitFn(),
                                  ExceptionData.getPointer());

      CGF.EmitBlock(FinallyNoCallExit);

      if (isa<ObjCAtTryStmt>(S)) {
        if (const ObjCAtFinallyStmt* FinallyStmt =
              cast<ObjCAtTryStmt>(S).getFinallyStmt()) {
          // Don't try to do the @finally if this is an EH cleanup.
          if (flags.isForEHCleanup()) return;

          // Save the current cleanup destination in case there's
          // control flow inside the finally statement.
          llvm::Value *CurCleanupDest =
            CGF.Builder.CreateLoad(CGF.getNormalCleanupDestSlot());

          CGF.EmitStmt(FinallyStmt->getFinallyBody());

          if (CGF.HaveInsertPoint()) {
            CGF.Builder.CreateStore(CurCleanupDest,
                                    CGF.getNormalCleanupDestSlot());
          } else {
            // Currently, the end of the cleanup must always exist.
            CGF.EnsureInsertPoint();
          }
        }
      } else {
        // Emit objc_sync_exit(expr); as finally's sole statement for
        // @synchronized.
        llvm::Value *SyncArg = CGF.Builder.CreateLoad(SyncArgSlot);
        CGF.EmitNounwindRuntimeCall(ObjCTypes.getSyncExitFn(), SyncArg);
      }
    }
  };

  class FragileHazards {
    CodeGenFunction &CGF;
    SmallVector<llvm::Value*, 20> Locals;
    llvm::DenseSet<llvm::BasicBlock*> BlocksBeforeTry;

    llvm::InlineAsm *ReadHazard;
    llvm::InlineAsm *WriteHazard;

    llvm::FunctionType *GetAsmFnType();

    void collectLocals();
    void emitReadHazard(CGBuilderTy &Builder);

  public:
    FragileHazards(CodeGenFunction &CGF);

    void emitWriteHazard();
    void emitHazardsInNewBlocks();
  };
} // end anonymous namespace

/// Create the fragile-ABI read and write hazards based on the current
/// state of the function, which is presumed to be immediately prior
/// to a @try block.  These hazards are used to maintain correct
/// semantics in the face of optimization and the fragile ABI's
/// cavalier use of setjmp/longjmp.
FragileHazards::FragileHazards(CodeGenFunction &CGF) : CGF(CGF) {
  collectLocals();

  if (Locals.empty()) return;

  // Collect all the blocks in the function.
  for (llvm::Function::iterator
         I = CGF.CurFn->begin(), E = CGF.CurFn->end(); I != E; ++I)
    BlocksBeforeTry.insert(&*I);

  llvm::FunctionType *AsmFnTy = GetAsmFnType();

  // Create a read hazard for the allocas.  This inhibits dead-store
  // optimizations and forces the values to memory.  This hazard is
  // inserted before any 'throwing' calls in the protected scope to
  // reflect the possibility that the variables might be read from the
  // catch block if the call throws.
  {
    std::string Constraint;
    for (unsigned I = 0, E = Locals.size(); I != E; ++I) {
      if (I) Constraint += ',';
      Constraint += "*m";
    }

    ReadHazard = llvm::InlineAsm::get(AsmFnTy, "", Constraint, true, false);
  }

  // Create a write hazard for the allocas.  This inhibits folding
  // loads across the hazard.  This hazard is inserted at the
  // beginning of the catch path to reflect the possibility that the
  // variables might have been written within the protected scope.
  {
    std::string Constraint;
    for (unsigned I = 0, E = Locals.size(); I != E; ++I) {
      if (I) Constraint += ',';
      Constraint += "=*m";
    }

    WriteHazard = llvm::InlineAsm::get(AsmFnTy, "", Constraint, true, false);
  }
}

/// Emit a write hazard at the current location.
void FragileHazards::emitWriteHazard() {
  if (Locals.empty()) return;

  CGF.EmitNounwindRuntimeCall(WriteHazard, Locals);
}

void FragileHazards::emitReadHazard(CGBuilderTy &Builder) {
  assert(!Locals.empty());
  llvm::CallInst *call = Builder.CreateCall(ReadHazard, Locals);
  call->setDoesNotThrow();
  call->setCallingConv(CGF.getRuntimeCC());
}

/// Emit read hazards in all the protected blocks, i.e. all the blocks
/// which have been inserted since the beginning of the try.
void FragileHazards::emitHazardsInNewBlocks() {
  if (Locals.empty()) return;

  CGBuilderTy Builder(CGF, CGF.getLLVMContext());

  // Iterate through all blocks, skipping those prior to the try.
  for (llvm::Function::iterator
         FI = CGF.CurFn->begin(), FE = CGF.CurFn->end(); FI != FE; ++FI) {
    llvm::BasicBlock &BB = *FI;
    if (BlocksBeforeTry.count(&BB)) continue;

    // Walk through all the calls in the block.
    for (llvm::BasicBlock::iterator
           BI = BB.begin(), BE = BB.end(); BI != BE; ++BI) {
      llvm::Instruction &I = *BI;

      // Ignore instructions that aren't non-intrinsic calls.
      // These are the only calls that can possibly call longjmp.
      if (!isa<llvm::CallInst>(I) && !isa<llvm::InvokeInst>(I)) continue;
      if (isa<llvm::IntrinsicInst>(I))
        continue;

      // Ignore call sites marked nounwind.  This may be questionable,
      // since 'nounwind' doesn't necessarily mean 'does not call longjmp'.
      llvm::CallSite CS(&I);
      if (CS.doesNotThrow()) continue;

      // Insert a read hazard before the call.  This will ensure that
      // any writes to the locals are performed before making the
      // call.  If the call throws, then this is sufficient to
      // guarantee correctness as long as it doesn't also write to any
      // locals.
      Builder.SetInsertPoint(&BB, BI);
      emitReadHazard(Builder);
    }
  }
}

static void addIfPresent(llvm::DenseSet<llvm::Value*> &S, llvm::Value *V) {
  if (V) S.insert(V);
}

static void addIfPresent(llvm::DenseSet<llvm::Value*> &S, Address V) {
  if (V.isValid()) S.insert(V.getPointer());
}

void FragileHazards::collectLocals() {
  // Compute a set of allocas to ignore.
  llvm::DenseSet<llvm::Value*> AllocasToIgnore;
  addIfPresent(AllocasToIgnore, CGF.ReturnValue);
  addIfPresent(AllocasToIgnore, CGF.NormalCleanupDest);

  // Collect all the allocas currently in the function.  This is
  // probably way too aggressive.
  llvm::BasicBlock &Entry = CGF.CurFn->getEntryBlock();
  for (llvm::BasicBlock::iterator
         I = Entry.begin(), E = Entry.end(); I != E; ++I)
    if (isa<llvm::AllocaInst>(*I) && !AllocasToIgnore.count(&*I))
      Locals.push_back(&*I);
}

llvm::FunctionType *FragileHazards::GetAsmFnType() {
  SmallVector<llvm::Type *, 16> tys(Locals.size());
  for (unsigned i = 0, e = Locals.size(); i != e; ++i)
    tys[i] = Locals[i]->getType();
  return llvm::FunctionType::get(CGF.VoidTy, tys, false);
}

/*

  Objective-C setjmp-longjmp (sjlj) Exception Handling
  --

  A catch buffer is a setjmp buffer plus:
    - a pointer to the exception that was caught
    - a pointer to the previous exception data buffer
    - two pointers of reserved storage
  Therefore catch buffers form a stack, with a pointer to the top
  of the stack kept in thread-local storage.

  objc_exception_try_enter pushes a catch buffer onto the EH stack.
  objc_exception_try_exit pops the given catch buffer, which is
    required to be the top of the EH stack.
  objc_exception_throw pops the top of the EH stack, writes the
    thrown exception into the appropriate field, and longjmps
    to the setjmp buffer.  It crashes the process (with a printf
    and an abort()) if there are no catch buffers on the stack.
  objc_exception_extract just reads the exception pointer out of the
    catch buffer.

  There's no reason an implementation couldn't use a light-weight
  setjmp here --- something like __builtin_setjmp, but API-compatible
  with the heavyweight setjmp.  This will be more important if we ever
  want to implement correct ObjC/C++ exception interactions for the
  fragile ABI.

  Note that for this use of setjmp/longjmp to be correct, we may need
  to mark some local variables volatile: if a non-volatile local
  variable is modified between the setjmp and the longjmp, it has
  indeterminate value.  For the purposes of LLVM IR, it may be
  sufficient to make loads and stores within the @try (to variables
  declared outside the @try) volatile.  This is necessary for
  optimized correctness, but is not currently being done; this is
  being tracked as rdar://problem/8160285

  The basic framework for a @try-catch-finally is as follows:
  {
  objc_exception_data d;
  id _rethrow = null;
  bool _call_try_exit = true;

  objc_exception_try_enter(&d);
  if (!setjmp(d.jmp_buf)) {
  ... try body ...
  } else {
  // exception path
  id _caught = objc_exception_extract(&d);

  // enter new try scope for handlers
  if (!setjmp(d.jmp_buf)) {
  ... match exception and execute catch blocks ...

  // fell off end, rethrow.
  _rethrow = _caught;
  ... jump-through-finally to finally_rethrow ...
  } else {
  // exception in catch block
  _rethrow = objc_exception_extract(&d);
  _call_try_exit = false;
  ... jump-through-finally to finally_rethrow ...
  }
  }
  ... jump-through-finally to finally_end ...

  finally:
  if (_call_try_exit)
  objc_exception_try_exit(&d);

  ... finally block ....
  ... dispatch to finally destination ...

  finally_rethrow:
  objc_exception_throw(_rethrow);

  finally_end:
  }

  This framework differs slightly from the one gcc uses, in that gcc
  uses _rethrow to determine if objc_exception_try_exit should be called
  and if the object should be rethrown. This breaks in the face of
  throwing nil and introduces unnecessary branches.

  We specialize this framework for a few particular circumstances:

  - If there are no catch blocks, then we avoid emitting the second
  exception handling context.

  - If there is a catch-all catch block (i.e. @catch(...) or @catch(id
  e)) we avoid emitting the code to rethrow an uncaught exception.

  - FIXME: If there is no @finally block we can do a few more
  simplifications.

  Rethrows and Jumps-Through-Finally
  --

  '@throw;' is supported by pushing the currently-caught exception
  onto ObjCEHStack while the @catch blocks are emitted.

  Branches through the @finally block are handled with an ordinary
  normal cleanup.  We do not register an EH cleanup; fragile-ABI ObjC
  exceptions are not compatible with C++ exceptions, and this is
  hardly the only place where this will go wrong.

  @synchronized(expr) { stmt; } is emitted as if it were:
    id synch_value = expr;
    objc_sync_enter(synch_value);
    @try { stmt; } @finally { objc_sync_exit(synch_value); }
*/

void CGObjCMac::EmitTryOrSynchronizedStmt(CodeGen::CodeGenFunction &CGF,
                                          const Stmt &S) {
  bool isTry = isa<ObjCAtTryStmt>(S);

  // A destination for the fall-through edges of the catch handlers to
  // jump to.
  CodeGenFunction::JumpDest FinallyEnd =
    CGF.getJumpDestInCurrentScope("finally.end");

  // A destination for the rethrow edge of the catch handlers to jump
  // to.
  CodeGenFunction::JumpDest FinallyRethrow =
    CGF.getJumpDestInCurrentScope("finally.rethrow");

  // For @synchronized, call objc_sync_enter(sync.expr). The
  // evaluation of the expression must occur before we enter the
  // @synchronized.  We can't avoid a temp here because we need the
  // value to be preserved.  If the backend ever does liveness
  // correctly after setjmp, this will be unnecessary.
  Address SyncArgSlot = Address::invalid();
  if (!isTry) {
    llvm::Value *SyncArg =
      CGF.EmitScalarExpr(cast<ObjCAtSynchronizedStmt>(S).getSynchExpr());
    SyncArg = CGF.Builder.CreateBitCast(SyncArg, ObjCTypes.ObjectPtrTy);
    CGF.EmitNounwindRuntimeCall(ObjCTypes.getSyncEnterFn(), SyncArg);

    SyncArgSlot = CGF.CreateTempAlloca(SyncArg->getType(),
                                       CGF.getPointerAlign(), "sync.arg");
    CGF.Builder.CreateStore(SyncArg, SyncArgSlot);
  }

  // Allocate memory for the setjmp buffer.  This needs to be kept
  // live throughout the try and catch blocks.
  Address ExceptionData = CGF.CreateTempAlloca(ObjCTypes.ExceptionDataTy,
                                               CGF.getPointerAlign(),
                                               "exceptiondata.ptr");

  // Create the fragile hazards.  Note that this will not capture any
  // of the allocas required for exception processing, but will
  // capture the current basic block (which extends all the way to the
  // setjmp call) as "before the @try".
  FragileHazards Hazards(CGF);

  // Create a flag indicating whether the cleanup needs to call
  // objc_exception_try_exit.  This is true except when
  //   - no catches match and we're branching through the cleanup
  //     just to rethrow the exception, or
  //   - a catch matched and we're falling out of the catch handler.
  // The setjmp-safety rule here is that we should always store to this
  // variable in a place that dominates the branch through the cleanup
  // without passing through any setjmps.
  Address CallTryExitVar = CGF.CreateTempAlloca(CGF.Builder.getInt1Ty(),
                                                CharUnits::One(),
                                                "_call_try_exit");

  // A slot containing the exception to rethrow.  Only needed when we
  // have both a @catch and a @finally.
  Address PropagatingExnVar = Address::invalid();

  // Push a normal cleanup to leave the try scope.
  CGF.EHStack.pushCleanup<PerformFragileFinally>(NormalAndEHCleanup, &S,
                                                 SyncArgSlot,
                                                 CallTryExitVar,
                                                 ExceptionData,
                                                 &ObjCTypes);

  // Enter a try block:
  //  - Call objc_exception_try_enter to push ExceptionData on top of
  //    the EH stack.
  CGF.EmitNounwindRuntimeCall(ObjCTypes.getExceptionTryEnterFn(),
                              ExceptionData.getPointer());

  //  - Call setjmp on the exception data buffer.
  llvm::Constant *Zero = llvm::ConstantInt::get(CGF.Builder.getInt32Ty(), 0);
  llvm::Value *GEPIndexes[] = { Zero, Zero, Zero };
  llvm::Value *SetJmpBuffer = CGF.Builder.CreateGEP(
      ObjCTypes.ExceptionDataTy, ExceptionData.getPointer(), GEPIndexes,
      "setjmp_buffer");
  llvm::CallInst *SetJmpResult = CGF.EmitNounwindRuntimeCall(
      ObjCTypes.getSetJmpFn(), SetJmpBuffer, "setjmp_result");
  SetJmpResult->setCanReturnTwice();

  // If setjmp returned 0, enter the protected block; otherwise,
  // branch to the handler.
  llvm::BasicBlock *TryBlock = CGF.createBasicBlock("try");
  llvm::BasicBlock *TryHandler = CGF.createBasicBlock("try.handler");
  llvm::Value *DidCatch =
    CGF.Builder.CreateIsNotNull(SetJmpResult, "did_catch_exception");
  CGF.Builder.CreateCondBr(DidCatch, TryHandler, TryBlock);

  // Emit the protected block.
  CGF.EmitBlock(TryBlock);
  CGF.Builder.CreateStore(CGF.Builder.getTrue(), CallTryExitVar);
  CGF.EmitStmt(isTry ? cast<ObjCAtTryStmt>(S).getTryBody()
                     : cast<ObjCAtSynchronizedStmt>(S).getSynchBody());

  CGBuilderTy::InsertPoint TryFallthroughIP = CGF.Builder.saveAndClearIP();

  // Emit the exception handler block.
  CGF.EmitBlock(TryHandler);

  // Don't optimize loads of the in-scope locals across this point.
  Hazards.emitWriteHazard();

  // For a @synchronized (or a @try with no catches), just branch
  // through the cleanup to the rethrow block.
  if (!isTry || !cast<ObjCAtTryStmt>(S).getNumCatchStmts()) {
    // Tell the cleanup not to re-pop the exit.
    CGF.Builder.CreateStore(CGF.Builder.getFalse(), CallTryExitVar);
    CGF.EmitBranchThroughCleanup(FinallyRethrow);

  // Otherwise, we have to match against the caught exceptions.
  } else {
    // Retrieve the exception object.  We may emit multiple blocks but
    // nothing can cross this so the value is already in SSA form.
    llvm::CallInst *Caught =
      CGF.EmitNounwindRuntimeCall(ObjCTypes.getExceptionExtractFn(),
                                  ExceptionData.getPointer(), "caught");

    // Push the exception to rethrow onto the EH value stack for the
    // benefit of any @throws in the handlers.
    CGF.ObjCEHValueStack.push_back(Caught);

    const ObjCAtTryStmt* AtTryStmt = cast<ObjCAtTryStmt>(&S);

    bool HasFinally = (AtTryStmt->getFinallyStmt() != nullptr);

    llvm::BasicBlock *CatchBlock = nullptr;
    llvm::BasicBlock *CatchHandler = nullptr;
    if (HasFinally) {
      // Save the currently-propagating exception before
      // objc_exception_try_enter clears the exception slot.
      PropagatingExnVar = CGF.CreateTempAlloca(Caught->getType(),
                                               CGF.getPointerAlign(),
                                               "propagating_exception");
      CGF.Builder.CreateStore(Caught, PropagatingExnVar);

      // Enter a new exception try block (in case a @catch block
      // throws an exception).
      CGF.EmitNounwindRuntimeCall(ObjCTypes.getExceptionTryEnterFn(),
                                  ExceptionData.getPointer());

      llvm::CallInst *SetJmpResult =
        CGF.EmitNounwindRuntimeCall(ObjCTypes.getSetJmpFn(),
                                    SetJmpBuffer, "setjmp.result");
      SetJmpResult->setCanReturnTwice();

      llvm::Value *Threw =
        CGF.Builder.CreateIsNotNull(SetJmpResult, "did_catch_exception");

      CatchBlock = CGF.createBasicBlock("catch");
      CatchHandler = CGF.createBasicBlock("catch_for_catch");
      CGF.Builder.CreateCondBr(Threw, CatchHandler, CatchBlock);

      CGF.EmitBlock(CatchBlock);
    }

    CGF.Builder.CreateStore(CGF.Builder.getInt1(HasFinally), CallTryExitVar);

    // Handle catch list. As a special case we check if everything is
    // matched and avoid generating code for falling off the end if
    // so.
    bool AllMatched = false;
    for (unsigned I = 0, N = AtTryStmt->getNumCatchStmts(); I != N; ++I) {
      const ObjCAtCatchStmt *CatchStmt = AtTryStmt->getCatchStmt(I);

      const VarDecl *CatchParam = CatchStmt->getCatchParamDecl();
      const ObjCObjectPointerType *OPT = nullptr;

      // catch(...) always matches.
      if (!CatchParam) {
        AllMatched = true;
      } else {
        OPT = CatchParam->getType()->getAs<ObjCObjectPointerType>();

        // catch(id e) always matches under this ABI, since only
        // ObjC exceptions end up here in the first place.
        // FIXME: For the time being we also match id<X>; this should
        // be rejected by Sema instead.
        if (OPT && (OPT->isObjCIdType() || OPT->isObjCQualifiedIdType()))
          AllMatched = true;
      }

      // If this is a catch-all, we don't need to test anything.
      if (AllMatched) {
        CodeGenFunction::RunCleanupsScope CatchVarCleanups(CGF);

        if (CatchParam) {
          CGF.EmitAutoVarDecl(*CatchParam);
          assert(CGF.HaveInsertPoint() && "DeclStmt destroyed insert point?");

          // These types work out because ConvertType(id) == i8*.
          EmitInitOfCatchParam(CGF, Caught, CatchParam);
        }

        CGF.EmitStmt(CatchStmt->getCatchBody());

        // The scope of the catch variable ends right here.
        CatchVarCleanups.ForceCleanup();

        CGF.EmitBranchThroughCleanup(FinallyEnd);
        break;
      }

      assert(OPT && "Unexpected non-object pointer type in @catch");
      const ObjCObjectType *ObjTy = OPT->getObjectType();

      // FIXME: @catch (Class c) ?
      ObjCInterfaceDecl *IDecl = ObjTy->getInterface();
      assert(IDecl && "Catch parameter must have Objective-C type!");

      // Check if the @catch block matches the exception object.
      llvm::Value *Class = EmitClassRef(CGF, IDecl);

      llvm::Value *matchArgs[] = { Class, Caught };
      llvm::CallInst *Match =
        CGF.EmitNounwindRuntimeCall(ObjCTypes.getExceptionMatchFn(),
                                    matchArgs, "match");

      llvm::BasicBlock *MatchedBlock = CGF.createBasicBlock("match");
      llvm::BasicBlock *NextCatchBlock = CGF.createBasicBlock("catch.next");

      CGF.Builder.CreateCondBr(CGF.Builder.CreateIsNotNull(Match, "matched"),
                               MatchedBlock, NextCatchBlock);

      // Emit the @catch block.
      CGF.EmitBlock(MatchedBlock);

      // Collect any cleanups for the catch variable.  The scope lasts until
      // the end of the catch body.
      CodeGenFunction::RunCleanupsScope CatchVarCleanups(CGF);

      CGF.EmitAutoVarDecl(*CatchParam);
      assert(CGF.HaveInsertPoint() && "DeclStmt destroyed insert point?");

      // Initialize the catch variable.
      llvm::Value *Tmp =
        CGF.Builder.CreateBitCast(Caught,
                                  CGF.ConvertType(CatchParam->getType()));
      EmitInitOfCatchParam(CGF, Tmp, CatchParam);

      CGF.EmitStmt(CatchStmt->getCatchBody());

      // We're done with the catch variable.
      CatchVarCleanups.ForceCleanup();

      CGF.EmitBranchThroughCleanup(FinallyEnd);

      CGF.EmitBlock(NextCatchBlock);
    }

    CGF.ObjCEHValueStack.pop_back();

    // If nothing wanted anything to do with the caught exception,
    // kill the extract call.
    if (Caught->use_empty())
      Caught->eraseFromParent();

    if (!AllMatched)
      CGF.EmitBranchThroughCleanup(FinallyRethrow);

    if (HasFinally) {
      // Emit the exception handler for the @catch blocks.
      CGF.EmitBlock(CatchHandler);

      // In theory we might now need a write hazard, but actually it's
      // unnecessary because there's no local-accessing code between
      // the try's write hazard and here.
      //Hazards.emitWriteHazard();

      // Extract the new exception and save it to the
      // propagating-exception slot.
      assert(PropagatingExnVar.isValid());
      llvm::CallInst *NewCaught =
        CGF.EmitNounwindRuntimeCall(ObjCTypes.getExceptionExtractFn(),
                                    ExceptionData.getPointer(), "caught");
      CGF.Builder.CreateStore(NewCaught, PropagatingExnVar);

      // Don't pop the catch handler; the throw already did.
      CGF.Builder.CreateStore(CGF.Builder.getFalse(), CallTryExitVar);
      CGF.EmitBranchThroughCleanup(FinallyRethrow);
    }
  }

  // Insert read hazards as required in the new blocks.
  Hazards.emitHazardsInNewBlocks();

  // Pop the cleanup.
  CGF.Builder.restoreIP(TryFallthroughIP);
  if (CGF.HaveInsertPoint())
    CGF.Builder.CreateStore(CGF.Builder.getTrue(), CallTryExitVar);
  CGF.PopCleanupBlock();
  CGF.EmitBlock(FinallyEnd.getBlock(), true);

  // Emit the rethrow block.
  CGBuilderTy::InsertPoint SavedIP = CGF.Builder.saveAndClearIP();
  CGF.EmitBlock(FinallyRethrow.getBlock(), true);
  if (CGF.HaveInsertPoint()) {
    // If we have a propagating-exception variable, check it.
    llvm::Value *PropagatingExn;
    if (PropagatingExnVar.isValid()) {
      PropagatingExn = CGF.Builder.CreateLoad(PropagatingExnVar);

    // Otherwise, just look in the buffer for the exception to throw.
    } else {
      llvm::CallInst *Caught =
        CGF.EmitNounwindRuntimeCall(ObjCTypes.getExceptionExtractFn(),
                                    ExceptionData.getPointer());
      PropagatingExn = Caught;
    }

    CGF.EmitNounwindRuntimeCall(ObjCTypes.getExceptionThrowFn(),
                                PropagatingExn);
    CGF.Builder.CreateUnreachable();
  }

  CGF.Builder.restoreIP(SavedIP);
}

void CGObjCMac::EmitThrowStmt(CodeGen::CodeGenFunction &CGF,
                              const ObjCAtThrowStmt &S,
                              bool ClearInsertionPoint) {
  llvm::Value *ExceptionAsObject;

  if (const Expr *ThrowExpr = S.getThrowExpr()) {
    llvm::Value *Exception = CGF.EmitObjCThrowOperand(ThrowExpr);
    ExceptionAsObject =
      CGF.Builder.CreateBitCast(Exception, ObjCTypes.ObjectPtrTy);
  } else {
    assert((!CGF.ObjCEHValueStack.empty() && CGF.ObjCEHValueStack.back()) &&
           "Unexpected rethrow outside @catch block.");
    ExceptionAsObject = CGF.ObjCEHValueStack.back();
  }

  CGF.EmitRuntimeCall(ObjCTypes.getExceptionThrowFn(), ExceptionAsObject)
    ->setDoesNotReturn();
  CGF.Builder.CreateUnreachable();

  // Clear the insertion point to indicate we are in unreachable code.
  if (ClearInsertionPoint)
    CGF.Builder.ClearInsertionPoint();
}

/// EmitObjCWeakRead - Code gen for loading value of a __weak
/// object: objc_read_weak (id *src)
///
llvm::Value * CGObjCMac::EmitObjCWeakRead(CodeGen::CodeGenFunction &CGF,
                                          Address AddrWeakObj) {
  llvm::Type* DestTy = AddrWeakObj.getElementType();
  AddrWeakObj = CGF.Builder.CreateBitCast(AddrWeakObj,
                                          ObjCTypes.PtrObjectPtrTy);
  llvm::Value *read_weak =
    CGF.EmitNounwindRuntimeCall(ObjCTypes.getGcReadWeakFn(),
                                AddrWeakObj.getPointer(), "weakread");
  read_weak = CGF.Builder.CreateBitCast(read_weak, DestTy);
  return read_weak;
}

/// EmitObjCWeakAssign - Code gen for assigning to a __weak object.
/// objc_assign_weak (id src, id *dst)
///
void CGObjCMac::EmitObjCWeakAssign(CodeGen::CodeGenFunction &CGF,
                                   llvm::Value *src, Address dst) {
  llvm::Type * SrcTy = src->getType();
  if (!isa<llvm::PointerType>(SrcTy)) {
    unsigned Size = CGM.getDataLayout().getTypeAllocSize(SrcTy);
    assert(Size <= 8 && "does not support size > 8");
    src = (Size == 4) ? CGF.Builder.CreateBitCast(src, ObjCTypes.IntTy)
      : CGF.Builder.CreateBitCast(src, ObjCTypes.LongLongTy);
    src = CGF.Builder.CreateIntToPtr(src, ObjCTypes.Int8PtrTy);
  }
  src = CGF.Builder.CreateBitCast(src, ObjCTypes.ObjectPtrTy);
  dst = CGF.Builder.CreateBitCast(dst, ObjCTypes.PtrObjectPtrTy);
  llvm::Value *args[] = { src, dst.getPointer() };
  CGF.EmitNounwindRuntimeCall(ObjCTypes.getGcAssignWeakFn(),
                              args, "weakassign");
}

/// EmitObjCGlobalAssign - Code gen for assigning to a __strong object.
/// objc_assign_global (id src, id *dst)
///
void CGObjCMac::EmitObjCGlobalAssign(CodeGen::CodeGenFunction &CGF,
                                     llvm::Value *src, Address dst,
                                     bool threadlocal) {
  llvm::Type * SrcTy = src->getType();
  if (!isa<llvm::PointerType>(SrcTy)) {
    unsigned Size = CGM.getDataLayout().getTypeAllocSize(SrcTy);
    assert(Size <= 8 && "does not support size > 8");
    src = (Size == 4) ? CGF.Builder.CreateBitCast(src, ObjCTypes.IntTy)
      : CGF.Builder.CreateBitCast(src, ObjCTypes.LongLongTy);
    src = CGF.Builder.CreateIntToPtr(src, ObjCTypes.Int8PtrTy);
  }
  src = CGF.Builder.CreateBitCast(src, ObjCTypes.ObjectPtrTy);
  dst = CGF.Builder.CreateBitCast(dst, ObjCTypes.PtrObjectPtrTy);
  llvm::Value *args[] = { src, dst.getPointer() };
  if (!threadlocal)
    CGF.EmitNounwindRuntimeCall(ObjCTypes.getGcAssignGlobalFn(),
                                args, "globalassign");
  else
    CGF.EmitNounwindRuntimeCall(ObjCTypes.getGcAssignThreadLocalFn(),
                                args, "threadlocalassign");
}

/// EmitObjCIvarAssign - Code gen for assigning to a __strong object.
/// objc_assign_ivar (id src, id *dst, ptrdiff_t ivaroffset)
///
void CGObjCMac::EmitObjCIvarAssign(CodeGen::CodeGenFunction &CGF,
                                   llvm::Value *src, Address dst,
                                   llvm::Value *ivarOffset) {
  assert(ivarOffset && "EmitObjCIvarAssign - ivarOffset is NULL");
  llvm::Type * SrcTy = src->getType();
  if (!isa<llvm::PointerType>(SrcTy)) {
    unsigned Size = CGM.getDataLayout().getTypeAllocSize(SrcTy);
    assert(Size <= 8 && "does not support size > 8");
    src = (Size == 4) ? CGF.Builder.CreateBitCast(src, ObjCTypes.IntTy)
      : CGF.Builder.CreateBitCast(src, ObjCTypes.LongLongTy);
    src = CGF.Builder.CreateIntToPtr(src, ObjCTypes.Int8PtrTy);
  }
  src = CGF.Builder.CreateBitCast(src, ObjCTypes.ObjectPtrTy);
  dst = CGF.Builder.CreateBitCast(dst, ObjCTypes.PtrObjectPtrTy);
  llvm::Value *args[] = { src, dst.getPointer(), ivarOffset };
  CGF.EmitNounwindRuntimeCall(ObjCTypes.getGcAssignIvarFn(), args);
}

/// EmitObjCStrongCastAssign - Code gen for assigning to a __strong cast object.
/// objc_assign_strongCast (id src, id *dst)
///
void CGObjCMac::EmitObjCStrongCastAssign(CodeGen::CodeGenFunction &CGF,
                                         llvm::Value *src, Address dst) {
  llvm::Type * SrcTy = src->getType();
  if (!isa<llvm::PointerType>(SrcTy)) {
    unsigned Size = CGM.getDataLayout().getTypeAllocSize(SrcTy);
    assert(Size <= 8 && "does not support size > 8");
    src = (Size == 4) ? CGF.Builder.CreateBitCast(src, ObjCTypes.IntTy)
      : CGF.Builder.CreateBitCast(src, ObjCTypes.LongLongTy);
    src = CGF.Builder.CreateIntToPtr(src, ObjCTypes.Int8PtrTy);
  }
  src = CGF.Builder.CreateBitCast(src, ObjCTypes.ObjectPtrTy);
  dst = CGF.Builder.CreateBitCast(dst, ObjCTypes.PtrObjectPtrTy);
  llvm::Value *args[] = { src, dst.getPointer() };
  CGF.EmitNounwindRuntimeCall(ObjCTypes.getGcAssignStrongCastFn(),
                              args, "strongassign");
}

void CGObjCMac::EmitGCMemmoveCollectable(CodeGen::CodeGenFunction &CGF,
                                         Address DestPtr,
                                         Address SrcPtr,
                                         llvm::Value *size) {
  SrcPtr = CGF.Builder.CreateBitCast(SrcPtr, ObjCTypes.Int8PtrTy);
  DestPtr = CGF.Builder.CreateBitCast(DestPtr, ObjCTypes.Int8PtrTy);
  llvm::Value *args[] = { DestPtr.getPointer(), SrcPtr.getPointer(), size };
  CGF.EmitNounwindRuntimeCall(ObjCTypes.GcMemmoveCollectableFn(), args);
}

/// EmitObjCValueForIvar - Code Gen for ivar reference.
///
LValue CGObjCMac::EmitObjCValueForIvar(CodeGen::CodeGenFunction &CGF,
                                       QualType ObjectTy,
                                       llvm::Value *BaseValue,
                                       const ObjCIvarDecl *Ivar,
                                       unsigned CVRQualifiers) {
  const ObjCInterfaceDecl *ID =
    ObjectTy->getAs<ObjCObjectType>()->getInterface();
  return EmitValueForIvarAtOffset(CGF, ID, BaseValue, Ivar, CVRQualifiers,
                                  EmitIvarOffset(CGF, ID, Ivar));
}

llvm::Value *CGObjCMac::EmitIvarOffset(CodeGen::CodeGenFunction &CGF,
                                       const ObjCInterfaceDecl *Interface,
                                       const ObjCIvarDecl *Ivar) {
  uint64_t Offset = ComputeIvarBaseOffset(CGM, Interface, Ivar);
  return llvm::ConstantInt::get(
    CGM.getTypes().ConvertType(CGM.getContext().LongTy),
    Offset);
}

/* *** Private Interface *** */

/// EmitImageInfo - Emit the image info marker used to encode some module
/// level information.
///
/// See: <rdr://4810609&4810587&4810587>
/// struct IMAGE_INFO {
///   unsigned version;
///   unsigned flags;
/// };
enum ImageInfoFlags {
  eImageInfo_FixAndContinue      = (1 << 0), // This flag is no longer set by clang.
  eImageInfo_GarbageCollected    = (1 << 1),
  eImageInfo_GCOnly              = (1 << 2),
  eImageInfo_OptimizedByDyld     = (1 << 3), // This flag is set by the dyld shared cache.

  // A flag indicating that the module has no instances of a @synthesize of a
  // superclass variable. <rdar://problem/6803242>
  eImageInfo_CorrectedSynthesize = (1 << 4), // This flag is no longer set by clang.
  eImageInfo_ImageIsSimulated    = (1 << 5),
  eImageInfo_ClassProperties     = (1 << 6)
};

void CGObjCCommonMac::EmitImageInfo() {
  unsigned version = 0; // Version is unused?
  const char *Section = (ObjCABI == 1) ?
    "__OBJC, __image_info,regular" :
    "__DATA, __objc_imageinfo, regular, no_dead_strip";

  // Generate module-level named metadata to convey this information to the
  // linker and code-gen.
  llvm::Module &Mod = CGM.getModule();

  // Add the ObjC ABI version to the module flags.
  Mod.addModuleFlag(llvm::Module::Error, "Objective-C Version", ObjCABI);
  Mod.addModuleFlag(llvm::Module::Error, "Objective-C Image Info Version",
                    version);
  Mod.addModuleFlag(llvm::Module::Error, "Objective-C Image Info Section",
                    llvm::MDString::get(VMContext,Section));

  if (CGM.getLangOpts().getGC() == LangOptions::NonGC) {
    // Non-GC overrides those files which specify GC.
    Mod.addModuleFlag(llvm::Module::Override,
                      "Objective-C Garbage Collection", (uint32_t)0);
  } else {
    // Add the ObjC garbage collection value.
    Mod.addModuleFlag(llvm::Module::Error,
                      "Objective-C Garbage Collection",
                      eImageInfo_GarbageCollected);

    if (CGM.getLangOpts().getGC() == LangOptions::GCOnly) {
      // Add the ObjC GC Only value.
      Mod.addModuleFlag(llvm::Module::Error, "Objective-C GC Only",
                        eImageInfo_GCOnly);

      // Require that GC be specified and set to eImageInfo_GarbageCollected.
      llvm::Metadata *Ops[2] = {
          llvm::MDString::get(VMContext, "Objective-C Garbage Collection"),
          llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(
              llvm::Type::getInt32Ty(VMContext), eImageInfo_GarbageCollected))};
      Mod.addModuleFlag(llvm::Module::Require, "Objective-C GC Only",
                        llvm::MDNode::get(VMContext, Ops));
    }
  }

  // Indicate whether we're compiling this to run on a simulator.
  const llvm::Triple &Triple = CGM.getTarget().getTriple();
  if ((Triple.isiOS() || Triple.isWatchOS()) &&
      (Triple.getArch() == llvm::Triple::x86 ||
       Triple.getArch() == llvm::Triple::x86_64))
    Mod.addModuleFlag(llvm::Module::Error, "Objective-C Is Simulated",
                      eImageInfo_ImageIsSimulated);

  // Indicate whether we are generating class properties.
  Mod.addModuleFlag(llvm::Module::Error, "Objective-C Class Properties",
                    eImageInfo_ClassProperties);
}

// struct objc_module {
//   unsigned long version;
//   unsigned long size;
//   const char *name;
//   Symtab symtab;
// };

// FIXME: Get from somewhere
static const int ModuleVersion = 7;

void CGObjCMac::EmitModuleInfo() {
  uint64_t Size = CGM.getDataLayout().getTypeAllocSize(ObjCTypes.ModuleTy);

  llvm::Constant *Values[] = {
    llvm::ConstantInt::get(ObjCTypes.LongTy, ModuleVersion),
    llvm::ConstantInt::get(ObjCTypes.LongTy, Size),
    // This used to be the filename, now it is unused. <rdr://4327263>
    GetClassName(StringRef("")),
    EmitModuleSymbols()
  };
  CreateMetadataVar("OBJC_MODULES",
                    llvm::ConstantStruct::get(ObjCTypes.ModuleTy, Values),
                    "__OBJC,__module_info,regular,no_dead_strip",
                    CGM.getPointerAlign(), true);
}

llvm::Constant *CGObjCMac::EmitModuleSymbols() {
  unsigned NumClasses = DefinedClasses.size();
  unsigned NumCategories = DefinedCategories.size();

  // Return null if no symbols were defined.
  if (!NumClasses && !NumCategories)
    return llvm::Constant::getNullValue(ObjCTypes.SymtabPtrTy);

  llvm::Constant *Values[5];
  Values[0] = llvm::ConstantInt::get(ObjCTypes.LongTy, 0);
  Values[1] = llvm::Constant::getNullValue(ObjCTypes.SelectorPtrTy);
  Values[2] = llvm::ConstantInt::get(ObjCTypes.ShortTy, NumClasses);
  Values[3] = llvm::ConstantInt::get(ObjCTypes.ShortTy, NumCategories);

  // The runtime expects exactly the list of defined classes followed
  // by the list of defined categories, in a single array.
  SmallVector<llvm::Constant*, 8> Symbols(NumClasses + NumCategories);
  for (unsigned i=0; i<NumClasses; i++) {
    const ObjCInterfaceDecl *ID = ImplementedClasses[i];
    assert(ID);
    if (ObjCImplementationDecl *IMP = ID->getImplementation())
      // We are implementing a weak imported interface. Give it external linkage
      if (ID->isWeakImported() && !IMP->isWeakImported())
        DefinedClasses[i]->setLinkage(llvm::GlobalVariable::ExternalLinkage);
    
    Symbols[i] = llvm::ConstantExpr::getBitCast(DefinedClasses[i],
                                                ObjCTypes.Int8PtrTy);
  }
  for (unsigned i=0; i<NumCategories; i++)
    Symbols[NumClasses + i] =
      llvm::ConstantExpr::getBitCast(DefinedCategories[i],
                                     ObjCTypes.Int8PtrTy);

  Values[4] =
    llvm::ConstantArray::get(llvm::ArrayType::get(ObjCTypes.Int8PtrTy,
                                                  Symbols.size()),
                             Symbols);

  llvm::Constant *Init = llvm::ConstantStruct::getAnon(Values);

  llvm::GlobalVariable *GV = CreateMetadataVar(
      "OBJC_SYMBOLS", Init, "__OBJC,__symbols,regular,no_dead_strip",
      CGM.getPointerAlign(), true);
  return llvm::ConstantExpr::getBitCast(GV, ObjCTypes.SymtabPtrTy);
}

llvm::Value *CGObjCMac::EmitClassRefFromId(CodeGenFunction &CGF,
                                           IdentifierInfo *II) {
  LazySymbols.insert(II);
  
  llvm::GlobalVariable *&Entry = ClassReferences[II];
  
  if (!Entry) {
    llvm::Constant *Casted =
    llvm::ConstantExpr::getBitCast(GetClassName(II->getName()),
                                   ObjCTypes.ClassPtrTy);
    Entry = CreateMetadataVar(
        "OBJC_CLASS_REFERENCES_", Casted,
        "__OBJC,__cls_refs,literal_pointers,no_dead_strip",
        CGM.getPointerAlign(), true);
  }
  
  return CGF.Builder.CreateAlignedLoad(Entry, CGF.getPointerAlign());
}

llvm::Value *CGObjCMac::EmitClassRef(CodeGenFunction &CGF,
                                     const ObjCInterfaceDecl *ID) {
  // If the class has the objc_runtime_visible attribute, we need to
  // use the Objective-C runtime to get the class.
  if (ID->hasAttr<ObjCRuntimeVisibleAttr>())
    return EmitClassRefViaRuntime(CGF, ID, ObjCTypes);

  return EmitClassRefFromId(CGF, ID->getIdentifier());
}

llvm::Value *CGObjCMac::EmitNSAutoreleasePoolClassRef(CodeGenFunction &CGF) {
  IdentifierInfo *II = &CGM.getContext().Idents.get("NSAutoreleasePool");
  return EmitClassRefFromId(CGF, II);
}

llvm::Value *CGObjCMac::EmitSelector(CodeGenFunction &CGF, Selector Sel) {
  return CGF.Builder.CreateLoad(EmitSelectorAddr(CGF, Sel));
}

Address CGObjCMac::EmitSelectorAddr(CodeGenFunction &CGF, Selector Sel) {
  CharUnits Align = CGF.getPointerAlign();

  llvm::GlobalVariable *&Entry = SelectorReferences[Sel];
  if (!Entry) {
    llvm::Constant *Casted =
      llvm::ConstantExpr::getBitCast(GetMethodVarName(Sel),
                                     ObjCTypes.SelectorPtrTy);
    Entry = CreateMetadataVar(
        "OBJC_SELECTOR_REFERENCES_", Casted,
        "__OBJC,__message_refs,literal_pointers,no_dead_strip", Align, true);
    Entry->setExternallyInitialized(true);
  }

  return Address(Entry, Align);
}

llvm::Constant *CGObjCCommonMac::GetClassName(StringRef RuntimeName) {
    llvm::GlobalVariable *&Entry = ClassNames[RuntimeName];
    if (!Entry)
      Entry = CreateMetadataVar(
          "OBJC_CLASS_NAME_",
          llvm::ConstantDataArray::getString(VMContext, RuntimeName),
          ((ObjCABI == 2) ? "__TEXT,__objc_classname,cstring_literals"
                          : "__TEXT,__cstring,cstring_literals"),
          CharUnits::One(), true);
    return getConstantGEP(VMContext, Entry, 0, 0);
}

llvm::Function *CGObjCCommonMac::GetMethodDefinition(const ObjCMethodDecl *MD) {
  llvm::DenseMap<const ObjCMethodDecl*, llvm::Function*>::iterator
      I = MethodDefinitions.find(MD);
  if (I != MethodDefinitions.end())
    return I->second;

  return nullptr;
}

/// GetIvarLayoutName - Returns a unique constant for the given
/// ivar layout bitmap.
llvm::Constant *CGObjCCommonMac::GetIvarLayoutName(IdentifierInfo *Ident,
                                       const ObjCCommonTypesHelper &ObjCTypes) {
  return llvm::Constant::getNullValue(ObjCTypes.Int8PtrTy);
}

void IvarLayoutBuilder::visitRecord(const RecordType *RT,
                                    CharUnits offset) {
  const RecordDecl *RD = RT->getDecl();

  // If this is a union, remember that we had one, because it might mess
  // up the ordering of layout entries.
  if (RD->isUnion())
    IsDisordered = true;

  const ASTRecordLayout *recLayout = nullptr;
  visitAggregate(RD->field_begin(), RD->field_end(), offset,
                 [&](const FieldDecl *field) -> CharUnits {
    if (!recLayout)
      recLayout = &CGM.getContext().getASTRecordLayout(RD);
    auto offsetInBits = recLayout->getFieldOffset(field->getFieldIndex());
    return CGM.getContext().toCharUnitsFromBits(offsetInBits);
  });
}

template <class Iterator, class GetOffsetFn>
void IvarLayoutBuilder::visitAggregate(Iterator begin, Iterator end, 
                                       CharUnits aggregateOffset,
                                       const GetOffsetFn &getOffset) {
  for (; begin != end; ++begin) {
    auto field = *begin;

    // Skip over bitfields.
    if (field->isBitField()) {
      continue;
    }

    // Compute the offset of the field within the aggregate.
    CharUnits fieldOffset = aggregateOffset + getOffset(field);

    visitField(field, fieldOffset);
  }
}

/// Collect layout information for the given fields into IvarsInfo.
void IvarLayoutBuilder::visitField(const FieldDecl *field,
                                   CharUnits fieldOffset) {
  QualType fieldType = field->getType();

  // Drill down into arrays.
  uint64_t numElts = 1;
  while (auto arrayType = CGM.getContext().getAsConstantArrayType(fieldType)) {
    numElts *= arrayType->getSize().getZExtValue();
    fieldType = arrayType->getElementType();
  }

  assert(!fieldType->isArrayType() && "ivar of non-constant array type?");

  // If we ended up with a zero-sized array, we've done what we can do within
  // the limits of this layout encoding.
  if (numElts == 0) return;

  // Recurse if the base element type is a record type.
  if (auto recType = fieldType->getAs<RecordType>()) {
    size_t oldEnd = IvarsInfo.size();

    visitRecord(recType, fieldOffset);

    // If we have an array, replicate the first entry's layout information.
    auto numEltEntries = IvarsInfo.size() - oldEnd;
    if (numElts != 1 && numEltEntries != 0) {
      CharUnits eltSize = CGM.getContext().getTypeSizeInChars(recType);
      for (uint64_t eltIndex = 1; eltIndex != numElts; ++eltIndex) {
        // Copy the last numEltEntries onto the end of the array, adjusting
        // each for the element size.
        for (size_t i = 0; i != numEltEntries; ++i) {
          auto firstEntry = IvarsInfo[oldEnd + i];
          IvarsInfo.push_back(IvarInfo(firstEntry.Offset + eltIndex * eltSize,
                                       firstEntry.SizeInWords));
        }
      }
    }

    return;
  }

  // Classify the element type.
  Qualifiers::GC GCAttr = GetGCAttrTypeForType(CGM.getContext(), fieldType);

  // If it matches what we're looking for, add an entry.
  if ((ForStrongLayout && GCAttr == Qualifiers::Strong)
      || (!ForStrongLayout && GCAttr == Qualifiers::Weak)) {
    assert(CGM.getContext().getTypeSizeInChars(fieldType)
             == CGM.getPointerSize());
    IvarsInfo.push_back(IvarInfo(fieldOffset, numElts));
  }
}

/// buildBitmap - This routine does the horsework of taking the offsets of
/// strong/weak references and creating a bitmap.  The bitmap is also
/// returned in the given buffer, suitable for being passed to \c dump().
llvm::Constant *IvarLayoutBuilder::buildBitmap(CGObjCCommonMac &CGObjC,
                                llvm::SmallVectorImpl<unsigned char> &buffer) {
  // The bitmap is a series of skip/scan instructions, aligned to word
  // boundaries.  The skip is performed first.
  const unsigned char MaxNibble = 0xF;
  const unsigned char SkipMask = 0xF0, SkipShift = 4;
  const unsigned char ScanMask = 0x0F, ScanShift = 0;

  assert(!IvarsInfo.empty() && "generating bitmap for no data");

  // Sort the ivar info on byte position in case we encounterred a
  // union nested in the ivar list.
  if (IsDisordered) {
    // This isn't a stable sort, but our algorithm should handle it fine.
    llvm::array_pod_sort(IvarsInfo.begin(), IvarsInfo.end());
  } else {
    assert(std::is_sorted(IvarsInfo.begin(), IvarsInfo.end()));
  }
  assert(IvarsInfo.back().Offset < InstanceEnd);

  assert(buffer.empty());

  // Skip the next N words.
  auto skip = [&](unsigned numWords) {
    assert(numWords > 0);

    // Try to merge into the previous byte.  Since scans happen second, we
    // can't do this if it includes a scan.
    if (!buffer.empty() && !(buffer.back() & ScanMask)) {
      unsigned lastSkip = buffer.back() >> SkipShift;
      if (lastSkip < MaxNibble) {
        unsigned claimed = std::min(MaxNibble - lastSkip, numWords);
        numWords -= claimed;
        lastSkip += claimed;
        buffer.back() = (lastSkip << SkipShift);
      }
    }

    while (numWords >= MaxNibble) {
      buffer.push_back(MaxNibble << SkipShift);
      numWords -= MaxNibble;
    }
    if (numWords) {
      buffer.push_back(numWords << SkipShift);
    }
  };

  // Scan the next N words.
  auto scan = [&](unsigned numWords) {
    assert(numWords > 0);

    // Try to merge into the previous byte.  Since scans happen second, we can
    // do this even if it includes a skip.
    if (!buffer.empty()) {
      unsigned lastScan = (buffer.back() & ScanMask) >> ScanShift;
      if (lastScan < MaxNibble) {
        unsigned claimed = std::min(MaxNibble - lastScan, numWords);
        numWords -= claimed;
        lastScan += claimed;
        buffer.back() = (buffer.back() & SkipMask) | (lastScan << ScanShift);
      }
    }

    while (numWords >= MaxNibble) {
      buffer.push_back(MaxNibble << ScanShift);
      numWords -= MaxNibble;
    }
    if (numWords) {
      buffer.push_back(numWords << ScanShift);
    }
  };

  // One past the end of the last scan.
  unsigned endOfLastScanInWords = 0;
  const CharUnits WordSize = CGM.getPointerSize();

  // Consider all the scan requests.
  for (auto &request : IvarsInfo) {
    CharUnits beginOfScan = request.Offset - InstanceBegin;

    // Ignore scan requests that don't start at an even multiple of the
    // word size.  We can't encode them.
    if ((beginOfScan % WordSize) != 0) continue;

    // Ignore scan requests that start before the instance start.
    // This assumes that scans never span that boundary.  The boundary
    // isn't the true start of the ivars, because in the fragile-ARC case
    // it's rounded up to word alignment, but the test above should leave
    // us ignoring that possibility.
    if (beginOfScan.isNegative()) {
      assert(request.Offset + request.SizeInWords * WordSize <= InstanceBegin);
      continue;
    }

    unsigned beginOfScanInWords = beginOfScan / WordSize;
    unsigned endOfScanInWords = beginOfScanInWords + request.SizeInWords;

    // If the scan starts some number of words after the last one ended,
    // skip forward.
    if (beginOfScanInWords > endOfLastScanInWords) {
      skip(beginOfScanInWords - endOfLastScanInWords);

    // Otherwise, start scanning where the last left off.
    } else {
      beginOfScanInWords = endOfLastScanInWords;

      // If that leaves us with nothing to scan, ignore this request.
      if (beginOfScanInWords >= endOfScanInWords) continue;
    }

    // Scan to the end of the request.
    assert(beginOfScanInWords < endOfScanInWords);
    scan(endOfScanInWords - beginOfScanInWords);
    endOfLastScanInWords = endOfScanInWords;
  }

  if (buffer.empty())
    return llvm::ConstantPointerNull::get(CGM.Int8PtrTy);

  // For GC layouts, emit a skip to the end of the allocation so that we
  // have precise information about the entire thing.  This isn't useful
  // or necessary for the ARC-style layout strings.
  if (CGM.getLangOpts().getGC() != LangOptions::NonGC) {
    unsigned lastOffsetInWords =
      (InstanceEnd - InstanceBegin + WordSize - CharUnits::One()) / WordSize;
    if (lastOffsetInWords > endOfLastScanInWords) {
      skip(lastOffsetInWords - endOfLastScanInWords);
    }
  }

  // Null terminate the string.
  buffer.push_back(0);

  bool isNonFragileABI = CGObjC.isNonFragileABI();

  llvm::GlobalVariable *Entry = CGObjC.CreateMetadataVar(
      "OBJC_CLASS_NAME_",
      llvm::ConstantDataArray::get(CGM.getLLVMContext(), buffer),
      (isNonFragileABI ? "__TEXT,__objc_classname,cstring_literals"
                       : "__TEXT,__cstring,cstring_literals"),
      CharUnits::One(), true);
  return getConstantGEP(CGM.getLLVMContext(), Entry, 0, 0);
}

/// BuildIvarLayout - Builds ivar layout bitmap for the class
/// implementation for the __strong or __weak case.
/// The layout map displays which words in ivar list must be skipped
/// and which must be scanned by GC (see below). String is built of bytes.
/// Each byte is divided up in two nibbles (4-bit each). Left nibble is count
/// of words to skip and right nibble is count of words to scan. So, each
/// nibble represents up to 15 workds to skip or scan. Skipping the rest is
/// represented by a 0x00 byte which also ends the string.
/// 1. when ForStrongLayout is true, following ivars are scanned:
/// - id, Class
/// - object *
/// - __strong anything
///
/// 2. When ForStrongLayout is false, following ivars are scanned:
/// - __weak anything
///
llvm::Constant *
CGObjCCommonMac::BuildIvarLayout(const ObjCImplementationDecl *OMD,
                                 CharUnits beginOffset, CharUnits endOffset,
                                 bool ForStrongLayout, bool HasMRCWeakIvars) {
  // If this is MRC, and we're either building a strong layout or there
  // are no weak ivars, bail out early.
  llvm::Type *PtrTy = CGM.Int8PtrTy;
  if (CGM.getLangOpts().getGC() == LangOptions::NonGC &&
      !CGM.getLangOpts().ObjCAutoRefCount &&
      (ForStrongLayout || !HasMRCWeakIvars))
    return llvm::Constant::getNullValue(PtrTy);

  const ObjCInterfaceDecl *OI = OMD->getClassInterface();
  SmallVector<const ObjCIvarDecl*, 32> ivars;

  // GC layout strings include the complete object layout, possibly
  // inaccurately in the non-fragile ABI; the runtime knows how to fix this
  // up.
  //
  // ARC layout strings only include the class's ivars.  In non-fragile
  // runtimes, that means starting at InstanceStart, rounded up to word
  // alignment.  In fragile runtimes, there's no InstanceStart, so it means
  // starting at the offset of the first ivar, rounded up to word alignment.
  //
  // MRC weak layout strings follow the ARC style.
  CharUnits baseOffset;
  if (CGM.getLangOpts().getGC() == LangOptions::NonGC) {
    for (const ObjCIvarDecl *IVD = OI->all_declared_ivar_begin(); 
         IVD; IVD = IVD->getNextIvar())
      ivars.push_back(IVD);

    if (isNonFragileABI()) {
      baseOffset = beginOffset; // InstanceStart
    } else if (!ivars.empty()) {
      baseOffset =
        CharUnits::fromQuantity(ComputeIvarBaseOffset(CGM, OMD, ivars[0]));
    } else {
      baseOffset = CharUnits::Zero();
    }

    baseOffset = baseOffset.alignTo(CGM.getPointerAlign());
  }
  else {
    CGM.getContext().DeepCollectObjCIvars(OI, true, ivars);

    baseOffset = CharUnits::Zero();
  }

  if (ivars.empty())
    return llvm::Constant::getNullValue(PtrTy);

  IvarLayoutBuilder builder(CGM, baseOffset, endOffset, ForStrongLayout);

  builder.visitAggregate(ivars.begin(), ivars.end(), CharUnits::Zero(),
                         [&](const ObjCIvarDecl *ivar) -> CharUnits {
      return CharUnits::fromQuantity(ComputeIvarBaseOffset(CGM, OMD, ivar));
  });

  if (!builder.hasBitmapData())
    return llvm::Constant::getNullValue(PtrTy);

  llvm::SmallVector<unsigned char, 4> buffer;
  llvm::Constant *C = builder.buildBitmap(*this, buffer);
  
   if (CGM.getLangOpts().ObjCGCBitmapPrint && !buffer.empty()) {
    printf("\n%s ivar layout for class '%s': ",
           ForStrongLayout ? "strong" : "weak",
           OMD->getClassInterface()->getName().str().c_str());
    builder.dump(buffer);
  }
  return C;
}

llvm::Constant *CGObjCCommonMac::GetMethodVarName(Selector Sel) {
  llvm::GlobalVariable *&Entry = MethodVarNames[Sel];

  // FIXME: Avoid std::string in "Sel.getAsString()"
  if (!Entry)
    Entry = CreateMetadataVar(
        "OBJC_METH_VAR_NAME_",
        llvm::ConstantDataArray::getString(VMContext, Sel.getAsString()),
        ((ObjCABI == 2) ? "__TEXT,__objc_methname,cstring_literals"
                        : "__TEXT,__cstring,cstring_literals"),
        CharUnits::One(), true);

  return getConstantGEP(VMContext, Entry, 0, 0);
}

// FIXME: Merge into a single cstring creation function.
llvm::Constant *CGObjCCommonMac::GetMethodVarName(IdentifierInfo *ID) {
  return GetMethodVarName(CGM.getContext().Selectors.getNullarySelector(ID));
}

llvm::Constant *CGObjCCommonMac::GetMethodVarType(const FieldDecl *Field) {
  std::string TypeStr;
  CGM.getContext().getObjCEncodingForType(Field->getType(), TypeStr, Field);

  llvm::GlobalVariable *&Entry = MethodVarTypes[TypeStr];

  if (!Entry)
    Entry = CreateMetadataVar(
        "OBJC_METH_VAR_TYPE_",
        llvm::ConstantDataArray::getString(VMContext, TypeStr),
        ((ObjCABI == 2) ? "__TEXT,__objc_methtype,cstring_literals"
                        : "__TEXT,__cstring,cstring_literals"),
        CharUnits::One(), true);

  return getConstantGEP(VMContext, Entry, 0, 0);
}

llvm::Constant *CGObjCCommonMac::GetMethodVarType(const ObjCMethodDecl *D,
                                                  bool Extended) {
  std::string TypeStr;
  if (CGM.getContext().getObjCEncodingForMethodDecl(D, TypeStr, Extended))
    return nullptr;

  llvm::GlobalVariable *&Entry = MethodVarTypes[TypeStr];

  if (!Entry)
    Entry = CreateMetadataVar(
        "OBJC_METH_VAR_TYPE_",
        llvm::ConstantDataArray::getString(VMContext, TypeStr),
        ((ObjCABI == 2) ? "__TEXT,__objc_methtype,cstring_literals"
                        : "__TEXT,__cstring,cstring_literals"),
        CharUnits::One(), true);

  return getConstantGEP(VMContext, Entry, 0, 0);
}

// FIXME: Merge into a single cstring creation function.
llvm::Constant *CGObjCCommonMac::GetPropertyName(IdentifierInfo *Ident) {
  llvm::GlobalVariable *&Entry = PropertyNames[Ident];

  if (!Entry)
    Entry = CreateMetadataVar(
        "OBJC_PROP_NAME_ATTR_",
        llvm::ConstantDataArray::getString(VMContext, Ident->getName()),
        "__TEXT,__cstring,cstring_literals", CharUnits::One(), true);

  return getConstantGEP(VMContext, Entry, 0, 0);
}

// FIXME: Merge into a single cstring creation function.
// FIXME: This Decl should be more precise.
llvm::Constant *
CGObjCCommonMac::GetPropertyTypeString(const ObjCPropertyDecl *PD,
                                       const Decl *Container) {
  std::string TypeStr;
  CGM.getContext().getObjCEncodingForPropertyDecl(PD, Container, TypeStr);
  return GetPropertyName(&CGM.getContext().Idents.get(TypeStr));
}

void CGObjCCommonMac::GetNameForMethod(const ObjCMethodDecl *D,
                                       const ObjCContainerDecl *CD,
                                       SmallVectorImpl<char> &Name) {
  llvm::raw_svector_ostream OS(Name);
  assert (CD && "Missing container decl in GetNameForMethod");
  OS << '\01' << (D->isInstanceMethod() ? '-' : '+')
     << '[' << CD->getName();
  if (const ObjCCategoryImplDecl *CID =
      dyn_cast<ObjCCategoryImplDecl>(D->getDeclContext()))
    OS << '(' << *CID << ')';
  OS << ' ' << D->getSelector().getAsString() << ']';
}

void CGObjCMac::FinishModule() {
  EmitModuleInfo();

  // Emit the dummy bodies for any protocols which were referenced but
  // never defined.
  for (llvm::DenseMap<IdentifierInfo*, llvm::GlobalVariable*>::iterator
         I = Protocols.begin(), e = Protocols.end(); I != e; ++I) {
    if (I->second->hasInitializer())
      continue;

    llvm::Constant *Values[5];
    Values[0] = llvm::Constant::getNullValue(ObjCTypes.ProtocolExtensionPtrTy);
    Values[1] = GetClassName(I->first->getName());
    Values[2] = llvm::Constant::getNullValue(ObjCTypes.ProtocolListPtrTy);
    Values[3] = Values[4] =
      llvm::Constant::getNullValue(ObjCTypes.MethodDescriptionListPtrTy);
    I->second->setInitializer(llvm::ConstantStruct::get(ObjCTypes.ProtocolTy,
                                                        Values));
    CGM.addCompilerUsedGlobal(I->second);
  }

  // Add assembler directives to add lazy undefined symbol references
  // for classes which are referenced but not defined. This is
  // important for correct linker interaction.
  //
  // FIXME: It would be nice if we had an LLVM construct for this.
  if (!LazySymbols.empty() || !DefinedSymbols.empty()) {
    SmallString<256> Asm;
    Asm += CGM.getModule().getModuleInlineAsm();
    if (!Asm.empty() && Asm.back() != '\n')
      Asm += '\n';

    llvm::raw_svector_ostream OS(Asm);
    for (llvm::SetVector<IdentifierInfo*>::iterator I = DefinedSymbols.begin(),
           e = DefinedSymbols.end(); I != e; ++I)
      OS << "\t.objc_class_name_" << (*I)->getName() << "=0\n"
         << "\t.globl .objc_class_name_" << (*I)->getName() << "\n";
    for (llvm::SetVector<IdentifierInfo*>::iterator I = LazySymbols.begin(),
         e = LazySymbols.end(); I != e; ++I) {
      OS << "\t.lazy_reference .objc_class_name_" << (*I)->getName() << "\n";
    }

    for (size_t i = 0, e = DefinedCategoryNames.size(); i < e; ++i) {
      OS << "\t.objc_category_name_" << DefinedCategoryNames[i] << "=0\n"
         << "\t.globl .objc_category_name_" << DefinedCategoryNames[i] << "\n";
    }
    
    CGM.getModule().setModuleInlineAsm(OS.str());
  }
}

CGObjCNonFragileABIMac::CGObjCNonFragileABIMac(CodeGen::CodeGenModule &cgm)
    : CGObjCCommonMac(cgm), ObjCTypes(cgm), ObjCEmptyCacheVar(nullptr),
      ObjCEmptyVtableVar(nullptr) {
  ObjCABI = 2;
}

/* *** */

ObjCCommonTypesHelper::ObjCCommonTypesHelper(CodeGen::CodeGenModule &cgm)
  : VMContext(cgm.getLLVMContext()), CGM(cgm), ExternalProtocolPtrTy(nullptr)
{
  CodeGen::CodeGenTypes &Types = CGM.getTypes();
  ASTContext &Ctx = CGM.getContext();

  ShortTy = Types.ConvertType(Ctx.ShortTy);
  IntTy = Types.ConvertType(Ctx.IntTy);
  LongTy = Types.ConvertType(Ctx.LongTy);
  LongLongTy = Types.ConvertType(Ctx.LongLongTy);
  Int8PtrTy = CGM.Int8PtrTy;
  Int8PtrPtrTy = CGM.Int8PtrPtrTy;

  // arm64 targets use "int" ivar offset variables. All others,
  // including OS X x86_64 and Windows x86_64, use "long" ivar offsets.
  if (CGM.getTarget().getTriple().getArch() == llvm::Triple::aarch64)
    IvarOffsetVarTy = IntTy;
  else
    IvarOffsetVarTy = LongTy;

  ObjectPtrTy = Types.ConvertType(Ctx.getObjCIdType());
  PtrObjectPtrTy = llvm::PointerType::getUnqual(ObjectPtrTy);
  SelectorPtrTy = Types.ConvertType(Ctx.getObjCSelType());

  // I'm not sure I like this. The implicit coordination is a bit
  // gross. We should solve this in a reasonable fashion because this
  // is a pretty common task (match some runtime data structure with
  // an LLVM data structure).

  // FIXME: This is leaked.
  // FIXME: Merge with rewriter code?

  // struct _objc_super {
  //   id self;
  //   Class cls;
  // }
  RecordDecl *RD = RecordDecl::Create(Ctx, TTK_Struct,
                                      Ctx.getTranslationUnitDecl(),
                                      SourceLocation(), SourceLocation(),
                                      &Ctx.Idents.get("_objc_super"));
  RD->addDecl(FieldDecl::Create(Ctx, RD, SourceLocation(), SourceLocation(),
                                nullptr, Ctx.getObjCIdType(), nullptr, nullptr,
                                false, ICIS_NoInit));
  RD->addDecl(FieldDecl::Create(Ctx, RD, SourceLocation(), SourceLocation(),
                                nullptr, Ctx.getObjCClassType(), nullptr,
                                nullptr, false, ICIS_NoInit));
  RD->completeDefinition();

  SuperCTy = Ctx.getTagDeclType(RD);
  SuperPtrCTy = Ctx.getPointerType(SuperCTy);

  SuperTy = cast<llvm::StructType>(Types.ConvertType(SuperCTy));
  SuperPtrTy = llvm::PointerType::getUnqual(SuperTy);

  // struct _prop_t {
  //   char *name;
  //   char *attributes;
  // }
  PropertyTy = llvm::StructType::create("struct._prop_t",
                                        Int8PtrTy, Int8PtrTy, nullptr);

  // struct _prop_list_t {
  //   uint32_t entsize;      // sizeof(struct _prop_t)
  //   uint32_t count_of_properties;
  //   struct _prop_t prop_list[count_of_properties];
  // }
  PropertyListTy =
    llvm::StructType::create("struct._prop_list_t", IntTy, IntTy,
                             llvm::ArrayType::get(PropertyTy, 0), nullptr);
  // struct _prop_list_t *
  PropertyListPtrTy = llvm::PointerType::getUnqual(PropertyListTy);

  // struct _objc_method {
  //   SEL _cmd;
  //   char *method_type;
  //   char *_imp;
  // }
  MethodTy = llvm::StructType::create("struct._objc_method",
                                      SelectorPtrTy, Int8PtrTy, Int8PtrTy,
                                      nullptr);

  // struct _objc_cache *
  CacheTy = llvm::StructType::create(VMContext, "struct._objc_cache");
  CachePtrTy = llvm::PointerType::getUnqual(CacheTy);
}

ObjCTypesHelper::ObjCTypesHelper(CodeGen::CodeGenModule &cgm)
  : ObjCCommonTypesHelper(cgm) {
  // struct _objc_method_description {
  //   SEL name;
  //   char *types;
  // }
  MethodDescriptionTy =
    llvm::StructType::create("struct._objc_method_description",
                             SelectorPtrTy, Int8PtrTy, nullptr);

  // struct _objc_method_description_list {
  //   int count;
  //   struct _objc_method_description[1];
  // }
  MethodDescriptionListTy = llvm::StructType::create(
      "struct._objc_method_description_list", IntTy,
      llvm::ArrayType::get(MethodDescriptionTy, 0), nullptr);

  // struct _objc_method_description_list *
  MethodDescriptionListPtrTy =
    llvm::PointerType::getUnqual(MethodDescriptionListTy);

  // Protocol description structures

  // struct _objc_protocol_extension {
  //   uint32_t size;  // sizeof(struct _objc_protocol_extension)
  //   struct _objc_method_description_list *optional_instance_methods;
  //   struct _objc_method_description_list *optional_class_methods;
  //   struct _objc_property_list *instance_properties;
  //   const char ** extendedMethodTypes;
  //   struct _objc_property_list *class_properties;
  // }
  ProtocolExtensionTy =
    llvm::StructType::create("struct._objc_protocol_extension",
                             IntTy, MethodDescriptionListPtrTy,
                             MethodDescriptionListPtrTy, PropertyListPtrTy,
                             Int8PtrPtrTy, PropertyListPtrTy, nullptr);

  // struct _objc_protocol_extension *
  ProtocolExtensionPtrTy = llvm::PointerType::getUnqual(ProtocolExtensionTy);

  // Handle recursive construction of Protocol and ProtocolList types

  ProtocolTy =
    llvm::StructType::create(VMContext, "struct._objc_protocol");

  ProtocolListTy =
    llvm::StructType::create(VMContext, "struct._objc_protocol_list");
  ProtocolListTy->setBody(llvm::PointerType::getUnqual(ProtocolListTy),
                          LongTy,
                          llvm::ArrayType::get(ProtocolTy, 0),
                          nullptr);

  // struct _objc_protocol {
  //   struct _objc_protocol_extension *isa;
  //   char *protocol_name;
  //   struct _objc_protocol **_objc_protocol_list;
  //   struct _objc_method_description_list *instance_methods;
  //   struct _objc_method_description_list *class_methods;
  // }
  ProtocolTy->setBody(ProtocolExtensionPtrTy, Int8PtrTy,
                      llvm::PointerType::getUnqual(ProtocolListTy),
                      MethodDescriptionListPtrTy,
                      MethodDescriptionListPtrTy,
                      nullptr);

  // struct _objc_protocol_list *
  ProtocolListPtrTy = llvm::PointerType::getUnqual(ProtocolListTy);

  ProtocolPtrTy = llvm::PointerType::getUnqual(ProtocolTy);

  // Class description structures

  // struct _objc_ivar {
  //   char *ivar_name;
  //   char *ivar_type;
  //   int  ivar_offset;
  // }
  IvarTy = llvm::StructType::create("struct._objc_ivar",
                                    Int8PtrTy, Int8PtrTy, IntTy, nullptr);

  // struct _objc_ivar_list *
  IvarListTy =
    llvm::StructType::create(VMContext, "struct._objc_ivar_list");
  IvarListPtrTy = llvm::PointerType::getUnqual(IvarListTy);

  // struct _objc_method_list *
  MethodListTy =
    llvm::StructType::create(VMContext, "struct._objc_method_list");
  MethodListPtrTy = llvm::PointerType::getUnqual(MethodListTy);

  // struct _objc_class_extension *
  ClassExtensionTy =
    llvm::StructType::create("struct._objc_class_extension",
                             IntTy, Int8PtrTy, PropertyListPtrTy, nullptr);
  ClassExtensionPtrTy = llvm::PointerType::getUnqual(ClassExtensionTy);

  ClassTy = llvm::StructType::create(VMContext, "struct._objc_class");

  // struct _objc_class {
  //   Class isa;
  //   Class super_class;
  //   char *name;
  //   long version;
  //   long info;
  //   long instance_size;
  //   struct _objc_ivar_list *ivars;
  //   struct _objc_method_list *methods;
  //   struct _objc_cache *cache;
  //   struct _objc_protocol_list *protocols;
  //   char *ivar_layout;
  //   struct _objc_class_ext *ext;
  // };
  ClassTy->setBody(llvm::PointerType::getUnqual(ClassTy),
                   llvm::PointerType::getUnqual(ClassTy),
                   Int8PtrTy,
                   LongTy,
                   LongTy,
                   LongTy,
                   IvarListPtrTy,
                   MethodListPtrTy,
                   CachePtrTy,
                   ProtocolListPtrTy,
                   Int8PtrTy,
                   ClassExtensionPtrTy,
                   nullptr);

  ClassPtrTy = llvm::PointerType::getUnqual(ClassTy);

  // struct _objc_category {
  //   char *category_name;
  //   char *class_name;
  //   struct _objc_method_list *instance_method;
  //   struct _objc_method_list *class_method;
  //   struct _objc_protocol_list *protocols;
  //   uint32_t size;  // sizeof(struct _objc_category)
  //   struct _objc_property_list *instance_properties;// category's @property
  //   struct _objc_property_list *class_properties;
  // }
  CategoryTy =
    llvm::StructType::create("struct._objc_category",
                             Int8PtrTy, Int8PtrTy, MethodListPtrTy,
                             MethodListPtrTy, ProtocolListPtrTy,
                             IntTy, PropertyListPtrTy, PropertyListPtrTy,
                             nullptr);

  // Global metadata structures

  // struct _objc_symtab {
  //   long sel_ref_cnt;
  //   SEL *refs;
  //   short cls_def_cnt;
  //   short cat_def_cnt;
  //   char *defs[cls_def_cnt + cat_def_cnt];
  // }
  SymtabTy =
    llvm::StructType::create("struct._objc_symtab",
                             LongTy, SelectorPtrTy, ShortTy, ShortTy,
                             llvm::ArrayType::get(Int8PtrTy, 0), nullptr);
  SymtabPtrTy = llvm::PointerType::getUnqual(SymtabTy);

  // struct _objc_module {
  //   long version;
  //   long size;   // sizeof(struct _objc_module)
  //   char *name;
  //   struct _objc_symtab* symtab;
  //  }
  ModuleTy =
    llvm::StructType::create("struct._objc_module",
                             LongTy, LongTy, Int8PtrTy, SymtabPtrTy, nullptr);


  // FIXME: This is the size of the setjmp buffer and should be target
  // specific. 18 is what's used on 32-bit X86.
  uint64_t SetJmpBufferSize = 18;

  // Exceptions
  llvm::Type *StackPtrTy = llvm::ArrayType::get(CGM.Int8PtrTy, 4);

  ExceptionDataTy =
    llvm::StructType::create("struct._objc_exception_data",
                             llvm::ArrayType::get(CGM.Int32Ty,SetJmpBufferSize),
                             StackPtrTy, nullptr);
}

ObjCNonFragileABITypesHelper::ObjCNonFragileABITypesHelper(CodeGen::CodeGenModule &cgm)
  : ObjCCommonTypesHelper(cgm) {
  // struct _method_list_t {
  //   uint32_t entsize;  // sizeof(struct _objc_method)
  //   uint32_t method_count;
  //   struct _objc_method method_list[method_count];
  // }
  MethodListnfABITy =
    llvm::StructType::create("struct.__method_list_t", IntTy, IntTy,
                             llvm::ArrayType::get(MethodTy, 0), nullptr);
  // struct method_list_t *
  MethodListnfABIPtrTy = llvm::PointerType::getUnqual(MethodListnfABITy);

  // struct _protocol_t {
  //   id isa;  // NULL
  //   const char * const protocol_name;
  //   const struct _protocol_list_t * protocol_list; // super protocols
  //   const struct method_list_t * const instance_methods;
  //   const struct method_list_t * const class_methods;
  //   const struct method_list_t *optionalInstanceMethods;
  //   const struct method_list_t *optionalClassMethods;
  //   const struct _prop_list_t * properties;
  //   const uint32_t size;  // sizeof(struct _protocol_t)
  //   const uint32_t flags;  // = 0
  //   const char ** extendedMethodTypes;
  //   const char *demangledName;
  //   const struct _prop_list_t * class_properties;
  // }

  // Holder for struct _protocol_list_t *
  ProtocolListnfABITy =
    llvm::StructType::create(VMContext, "struct._objc_protocol_list");

  ProtocolnfABITy =
    llvm::StructType::create("struct._protocol_t", ObjectPtrTy, Int8PtrTy,
                             llvm::PointerType::getUnqual(ProtocolListnfABITy),
                             MethodListnfABIPtrTy, MethodListnfABIPtrTy,
                             MethodListnfABIPtrTy, MethodListnfABIPtrTy,
                             PropertyListPtrTy, IntTy, IntTy, Int8PtrPtrTy,
                             Int8PtrTy, PropertyListPtrTy,
                             nullptr);

  // struct _protocol_t*
  ProtocolnfABIPtrTy = llvm::PointerType::getUnqual(ProtocolnfABITy);

  // struct _protocol_list_t {
  //   long protocol_count;   // Note, this is 32/64 bit
  //   struct _protocol_t *[protocol_count];
  // }
  ProtocolListnfABITy->setBody(LongTy,
                               llvm::ArrayType::get(ProtocolnfABIPtrTy, 0),
                               nullptr);

  // struct _objc_protocol_list*
  ProtocolListnfABIPtrTy = llvm::PointerType::getUnqual(ProtocolListnfABITy);

  // struct _ivar_t {
  //   unsigned [long] int *offset;  // pointer to ivar offset location
  //   char *name;
  //   char *type;
  //   uint32_t alignment;
  //   uint32_t size;
  // }
  IvarnfABITy = llvm::StructType::create(
      "struct._ivar_t", llvm::PointerType::getUnqual(IvarOffsetVarTy),
      Int8PtrTy, Int8PtrTy, IntTy, IntTy, nullptr);

  // struct _ivar_list_t {
  //   uint32 entsize;  // sizeof(struct _ivar_t)
  //   uint32 count;
  //   struct _iver_t list[count];
  // }
  IvarListnfABITy =
    llvm::StructType::create("struct._ivar_list_t", IntTy, IntTy,
                             llvm::ArrayType::get(IvarnfABITy, 0), nullptr);

  IvarListnfABIPtrTy = llvm::PointerType::getUnqual(IvarListnfABITy);

  // struct _class_ro_t {
  //   uint32_t const flags;
  //   uint32_t const instanceStart;
  //   uint32_t const instanceSize;
  //   uint32_t const reserved;  // only when building for 64bit targets
  //   const uint8_t * const ivarLayout;
  //   const char *const name;
  //   const struct _method_list_t * const baseMethods;
  //   const struct _objc_protocol_list *const baseProtocols;
  //   const struct _ivar_list_t *const ivars;
  //   const uint8_t * const weakIvarLayout;
  //   const struct _prop_list_t * const properties;
  // }

  // FIXME. Add 'reserved' field in 64bit abi mode!
  ClassRonfABITy = llvm::StructType::create("struct._class_ro_t",
                                            IntTy, IntTy, IntTy, Int8PtrTy,
                                            Int8PtrTy, MethodListnfABIPtrTy,
                                            ProtocolListnfABIPtrTy,
                                            IvarListnfABIPtrTy,
                                            Int8PtrTy, PropertyListPtrTy,
                                            nullptr);

  // ImpnfABITy - LLVM for id (*)(id, SEL, ...)
  llvm::Type *params[] = { ObjectPtrTy, SelectorPtrTy };
  ImpnfABITy = llvm::FunctionType::get(ObjectPtrTy, params, false)
                 ->getPointerTo();

  // struct _class_t {
  //   struct _class_t *isa;
  //   struct _class_t * const superclass;
  //   void *cache;
  //   IMP *vtable;
  //   struct class_ro_t *ro;
  // }

  ClassnfABITy = llvm::StructType::create(VMContext, "struct._class_t");
  ClassnfABITy->setBody(llvm::PointerType::getUnqual(ClassnfABITy),
                        llvm::PointerType::getUnqual(ClassnfABITy),
                        CachePtrTy,
                        llvm::PointerType::getUnqual(ImpnfABITy),
                        llvm::PointerType::getUnqual(ClassRonfABITy),
                        nullptr);

  // LLVM for struct _class_t *
  ClassnfABIPtrTy = llvm::PointerType::getUnqual(ClassnfABITy);

  // struct _category_t {
  //   const char * const name;
  //   struct _class_t *const cls;
  //   const struct _method_list_t * const instance_methods;
  //   const struct _method_list_t * const class_methods;
  //   const struct _protocol_list_t * const protocols;
  //   const struct _prop_list_t * const properties;
  //   const struct _prop_list_t * const class_properties;
  //   const uint32_t size;
  // }
  CategorynfABITy = llvm::StructType::create("struct._category_t",
                                             Int8PtrTy, ClassnfABIPtrTy,
                                             MethodListnfABIPtrTy,
                                             MethodListnfABIPtrTy,
                                             ProtocolListnfABIPtrTy,
                                             PropertyListPtrTy,
                                             PropertyListPtrTy,
                                             IntTy,
                                             nullptr);

  // New types for nonfragile abi messaging.
  CodeGen::CodeGenTypes &Types = CGM.getTypes();
  ASTContext &Ctx = CGM.getContext();

  // MessageRefTy - LLVM for:
  // struct _message_ref_t {
  //   IMP messenger;
  //   SEL name;
  // };

  // First the clang type for struct _message_ref_t
  RecordDecl *RD = RecordDecl::Create(Ctx, TTK_Struct,
                                      Ctx.getTranslationUnitDecl(),
                                      SourceLocation(), SourceLocation(),
                                      &Ctx.Idents.get("_message_ref_t"));
  RD->addDecl(FieldDecl::Create(Ctx, RD, SourceLocation(), SourceLocation(),
                                nullptr, Ctx.VoidPtrTy, nullptr, nullptr, false,
                                ICIS_NoInit));
  RD->addDecl(FieldDecl::Create(Ctx, RD, SourceLocation(), SourceLocation(),
                                nullptr, Ctx.getObjCSelType(), nullptr, nullptr,
                                false, ICIS_NoInit));
  RD->completeDefinition();

  MessageRefCTy = Ctx.getTagDeclType(RD);
  MessageRefCPtrTy = Ctx.getPointerType(MessageRefCTy);
  MessageRefTy = cast<llvm::StructType>(Types.ConvertType(MessageRefCTy));

  // MessageRefPtrTy - LLVM for struct _message_ref_t*
  MessageRefPtrTy = llvm::PointerType::getUnqual(MessageRefTy);

  // SuperMessageRefTy - LLVM for:
  // struct _super_message_ref_t {
  //   SUPER_IMP messenger;
  //   SEL name;
  // };
  SuperMessageRefTy =
    llvm::StructType::create("struct._super_message_ref_t",
                             ImpnfABITy, SelectorPtrTy, nullptr);

  // SuperMessageRefPtrTy - LLVM for struct _super_message_ref_t*
  SuperMessageRefPtrTy = llvm::PointerType::getUnqual(SuperMessageRefTy);
    

  // struct objc_typeinfo {
  //   const void** vtable; // objc_ehtype_vtable + 2
  //   const char*  name;    // c++ typeinfo string
  //   Class        cls;
  // };
  EHTypeTy =
    llvm::StructType::create("struct._objc_typeinfo",
                             llvm::PointerType::getUnqual(Int8PtrTy),
                             Int8PtrTy, ClassnfABIPtrTy, nullptr);
  EHTypePtrTy = llvm::PointerType::getUnqual(EHTypeTy);
}

llvm::Function *CGObjCNonFragileABIMac::ModuleInitFunction() {
  FinishNonFragileABIModule();

  return nullptr;
}

void CGObjCNonFragileABIMac::AddModuleClassList(
    ArrayRef<llvm::GlobalValue *> Container, StringRef SymbolName,
    StringRef SectionName) {
  unsigned NumClasses = Container.size();

  if (!NumClasses)
    return;

  SmallVector<llvm::Constant*, 8> Symbols(NumClasses);
  for (unsigned i=0; i<NumClasses; i++)
    Symbols[i] = llvm::ConstantExpr::getBitCast(Container[i],
                                                ObjCTypes.Int8PtrTy);
  llvm::Constant *Init =
    llvm::ConstantArray::get(llvm::ArrayType::get(ObjCTypes.Int8PtrTy,
                                                  Symbols.size()),
                             Symbols);

  llvm::GlobalVariable *GV =
    new llvm::GlobalVariable(CGM.getModule(), Init->getType(), false,
                             llvm::GlobalValue::PrivateLinkage,
                             Init,
                             SymbolName);
  GV->setAlignment(CGM.getDataLayout().getABITypeAlignment(Init->getType()));
  GV->setSection(SectionName);
  CGM.addCompilerUsedGlobal(GV);
}

void CGObjCNonFragileABIMac::FinishNonFragileABIModule() {
  // nonfragile abi has no module definition.

  // Build list of all implemented class addresses in array
  // L_OBJC_LABEL_CLASS_$.

  for (unsigned i=0, NumClasses=ImplementedClasses.size(); i<NumClasses; i++) {
    const ObjCInterfaceDecl *ID = ImplementedClasses[i];
    assert(ID);
    if (ObjCImplementationDecl *IMP = ID->getImplementation())
      // We are implementing a weak imported interface. Give it external linkage
      if (ID->isWeakImported() && !IMP->isWeakImported()) {
        DefinedClasses[i]->setLinkage(llvm::GlobalVariable::ExternalLinkage);
        DefinedMetaClasses[i]->setLinkage(llvm::GlobalVariable::ExternalLinkage);
      }
  }

  AddModuleClassList(DefinedClasses, "OBJC_LABEL_CLASS_$",
                     "__DATA, __objc_classlist, regular, no_dead_strip");

  AddModuleClassList(DefinedNonLazyClasses, "OBJC_LABEL_NONLAZY_CLASS_$",
                     "__DATA, __objc_nlclslist, regular, no_dead_strip");

  // Build list of all implemented category addresses in array
  // L_OBJC_LABEL_CATEGORY_$.
  AddModuleClassList(DefinedCategories, "OBJC_LABEL_CATEGORY_$",
                     "__DATA, __objc_catlist, regular, no_dead_strip");
  AddModuleClassList(DefinedNonLazyCategories, "OBJC_LABEL_NONLAZY_CATEGORY_$",
                     "__DATA, __objc_nlcatlist, regular, no_dead_strip");

  EmitImageInfo();
}

/// isVTableDispatchedSelector - Returns true if SEL is not in the list of
/// VTableDispatchMethods; false otherwise. What this means is that
/// except for the 19 selectors in the list, we generate 32bit-style
/// message dispatch call for all the rest.
bool CGObjCNonFragileABIMac::isVTableDispatchedSelector(Selector Sel) {
  // At various points we've experimented with using vtable-based
  // dispatch for all methods.
  switch (CGM.getCodeGenOpts().getObjCDispatchMethod()) {
  case CodeGenOptions::Legacy:
    return false;
  case CodeGenOptions::NonLegacy:
    return true;
  case CodeGenOptions::Mixed:
    break;
  }

  // If so, see whether this selector is in the white-list of things which must
  // use the new dispatch convention. We lazily build a dense set for this.
  if (VTableDispatchMethods.empty()) {
    VTableDispatchMethods.insert(GetNullarySelector("alloc"));
    VTableDispatchMethods.insert(GetNullarySelector("class"));
    VTableDispatchMethods.insert(GetNullarySelector("self"));
    VTableDispatchMethods.insert(GetNullarySelector("isFlipped"));
    VTableDispatchMethods.insert(GetNullarySelector("length"));
    VTableDispatchMethods.insert(GetNullarySelector("count"));

    // These are vtable-based if GC is disabled.
    // Optimistically use vtable dispatch for hybrid compiles.
    if (CGM.getLangOpts().getGC() != LangOptions::GCOnly) {
      VTableDispatchMethods.insert(GetNullarySelector("retain"));
      VTableDispatchMethods.insert(GetNullarySelector("release"));
      VTableDispatchMethods.insert(GetNullarySelector("autorelease"));
    }

    VTableDispatchMethods.insert(GetUnarySelector("allocWithZone"));
    VTableDispatchMethods.insert(GetUnarySelector("isKindOfClass"));
    VTableDispatchMethods.insert(GetUnarySelector("respondsToSelector"));
    VTableDispatchMethods.insert(GetUnarySelector("objectForKey"));
    VTableDispatchMethods.insert(GetUnarySelector("objectAtIndex"));
    VTableDispatchMethods.insert(GetUnarySelector("isEqualToString"));
    VTableDispatchMethods.insert(GetUnarySelector("isEqual"));

    // These are vtable-based if GC is enabled.
    // Optimistically use vtable dispatch for hybrid compiles.
    if (CGM.getLangOpts().getGC() != LangOptions::NonGC) {
      VTableDispatchMethods.insert(GetNullarySelector("hash"));
      VTableDispatchMethods.insert(GetUnarySelector("addObject"));
    
      // "countByEnumeratingWithState:objects:count"
      IdentifierInfo *KeyIdents[] = {
        &CGM.getContext().Idents.get("countByEnumeratingWithState"),
        &CGM.getContext().Idents.get("objects"),
        &CGM.getContext().Idents.get("count")
      };
      VTableDispatchMethods.insert(
        CGM.getContext().Selectors.getSelector(3, KeyIdents));
    }
  }

  return VTableDispatchMethods.count(Sel);
}

/// BuildClassRoTInitializer - generate meta-data for:
/// struct _class_ro_t {
///   uint32_t const flags;
///   uint32_t const instanceStart;
///   uint32_t const instanceSize;
///   uint32_t const reserved;  // only when building for 64bit targets
///   const uint8_t * const ivarLayout;
///   const char *const name;
///   const struct _method_list_t * const baseMethods;
///   const struct _protocol_list_t *const baseProtocols;
///   const struct _ivar_list_t *const ivars;
///   const uint8_t * const weakIvarLayout;
///   const struct _prop_list_t * const properties;
/// }
///
llvm::GlobalVariable * CGObjCNonFragileABIMac::BuildClassRoTInitializer(
  unsigned flags,
  unsigned InstanceStart,
  unsigned InstanceSize,
  const ObjCImplementationDecl *ID) {
  std::string ClassName = ID->getObjCRuntimeNameAsString();
  llvm::Constant *Values[10]; // 11 for 64bit targets!

  CharUnits beginInstance = CharUnits::fromQuantity(InstanceStart);
  CharUnits endInstance = CharUnits::fromQuantity(InstanceSize);

  bool hasMRCWeak = false;
  if (CGM.getLangOpts().ObjCAutoRefCount)
    flags |= NonFragileABI_Class_CompiledByARC;
  else if ((hasMRCWeak = hasMRCWeakIvars(CGM, ID)))
    flags |= NonFragileABI_Class_HasMRCWeakIvars;

  Values[ 0] = llvm::ConstantInt::get(ObjCTypes.IntTy, flags);
  Values[ 1] = llvm::ConstantInt::get(ObjCTypes.IntTy, InstanceStart);
  Values[ 2] = llvm::ConstantInt::get(ObjCTypes.IntTy, InstanceSize);
  // FIXME. For 64bit targets add 0 here.
  Values[ 3] = (flags & NonFragileABI_Class_Meta)
    ? GetIvarLayoutName(nullptr, ObjCTypes)
    : BuildStrongIvarLayout(ID, beginInstance, endInstance);
  Values[ 4] = GetClassName(ID->getObjCRuntimeNameAsString());
  // const struct _method_list_t * const baseMethods;
  std::vector<llvm::Constant*> Methods;
  std::string MethodListName("\01l_OBJC_$_");
  if (flags & NonFragileABI_Class_Meta) {
    MethodListName += "CLASS_METHODS_";
    MethodListName += ID->getObjCRuntimeNameAsString();
    for (const auto *I : ID->class_methods())
      // Class methods should always be defined.
      Methods.push_back(GetMethodConstant(I));
  } else {
    MethodListName += "INSTANCE_METHODS_";
    MethodListName += ID->getObjCRuntimeNameAsString();
    for (const auto *I : ID->instance_methods())
      // Instance methods should always be defined.
      Methods.push_back(GetMethodConstant(I));

    for (const auto *PID : ID->property_impls()) {
      if (PID->getPropertyImplementation() == ObjCPropertyImplDecl::Synthesize){
        ObjCPropertyDecl *PD = PID->getPropertyDecl();

        if (ObjCMethodDecl *MD = PD->getGetterMethodDecl())
          if (llvm::Constant *C = GetMethodConstant(MD))
            Methods.push_back(C);
        if (ObjCMethodDecl *MD = PD->getSetterMethodDecl())
          if (llvm::Constant *C = GetMethodConstant(MD))
            Methods.push_back(C);
      }
    }
  }
  Values[ 5] = EmitMethodList(MethodListName,
                              "__DATA, __objc_const", Methods);

  const ObjCInterfaceDecl *OID = ID->getClassInterface();
  assert(OID && "CGObjCNonFragileABIMac::BuildClassRoTInitializer");
  Values[ 6] = EmitProtocolList("\01l_OBJC_CLASS_PROTOCOLS_$_"
                                + OID->getObjCRuntimeNameAsString(),
                                OID->all_referenced_protocol_begin(),
                                OID->all_referenced_protocol_end());

  if (flags & NonFragileABI_Class_Meta) {
    Values[ 7] = llvm::Constant::getNullValue(ObjCTypes.IvarListnfABIPtrTy);
    Values[ 8] = GetIvarLayoutName(nullptr, ObjCTypes);
    Values[ 9] = EmitPropertyList(
        "\01l_OBJC_$_CLASS_PROP_LIST_" + ID->getObjCRuntimeNameAsString(),
        ID, ID->getClassInterface(), ObjCTypes, true);
  } else {
    Values[ 7] = EmitIvarList(ID);
    Values[ 8] = BuildWeakIvarLayout(ID, beginInstance, endInstance,
                                     hasMRCWeak);
    Values[ 9] = EmitPropertyList(
        "\01l_OBJC_$_PROP_LIST_" + ID->getObjCRuntimeNameAsString(),
        ID, ID->getClassInterface(), ObjCTypes, false);
  }
  llvm::Constant *Init = llvm::ConstantStruct::get(ObjCTypes.ClassRonfABITy,
                                                   Values);
  llvm::GlobalVariable *CLASS_RO_GV =
    new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ClassRonfABITy, false,
                             llvm::GlobalValue::PrivateLinkage,
                             Init,
                             (flags & NonFragileABI_Class_Meta) ?
                             std::string("\01l_OBJC_METACLASS_RO_$_")+ClassName :
                             std::string("\01l_OBJC_CLASS_RO_$_")+ClassName);
  CLASS_RO_GV->setAlignment(
    CGM.getDataLayout().getABITypeAlignment(ObjCTypes.ClassRonfABITy));
  CLASS_RO_GV->setSection("__DATA, __objc_const");
  return CLASS_RO_GV;

}

/// BuildClassMetaData - This routine defines that to-level meta-data
/// for the given ClassName for:
/// struct _class_t {
///   struct _class_t *isa;
///   struct _class_t * const superclass;
///   void *cache;
///   IMP *vtable;
///   struct class_ro_t *ro;
/// }
///
llvm::GlobalVariable *CGObjCNonFragileABIMac::BuildClassMetaData(
    const std::string &ClassName, llvm::Constant *IsAGV, llvm::Constant *SuperClassGV,
    llvm::Constant *ClassRoGV, bool HiddenVisibility, bool Weak) {
  llvm::Constant *Values[] = {
    IsAGV,
    SuperClassGV,
    ObjCEmptyCacheVar,  // &ObjCEmptyCacheVar
    ObjCEmptyVtableVar, // &ObjCEmptyVtableVar
    ClassRoGV           // &CLASS_RO_GV
  };
  if (!Values[1])
    Values[1] = llvm::Constant::getNullValue(ObjCTypes.ClassnfABIPtrTy);
  if (!Values[3])
    Values[3] = llvm::Constant::getNullValue(
                  llvm::PointerType::getUnqual(ObjCTypes.ImpnfABITy));
  llvm::Constant *Init = llvm::ConstantStruct::get(ObjCTypes.ClassnfABITy,
                                                   Values);
  llvm::GlobalVariable *GV = GetClassGlobal(ClassName, Weak);
  GV->setInitializer(Init);
  GV->setSection("__DATA, __objc_data");
  GV->setAlignment(
    CGM.getDataLayout().getABITypeAlignment(ObjCTypes.ClassnfABITy));
  if (!CGM.getTriple().isOSBinFormatCOFF())
    if (HiddenVisibility)
      GV->setVisibility(llvm::GlobalValue::HiddenVisibility);
  return GV;
}

bool
CGObjCNonFragileABIMac::ImplementationIsNonLazy(const ObjCImplDecl *OD) const {
  return OD->getClassMethod(GetNullarySelector("load")) != nullptr;
}

void CGObjCNonFragileABIMac::GetClassSizeInfo(const ObjCImplementationDecl *OID,
                                              uint32_t &InstanceStart,
                                              uint32_t &InstanceSize) {
  const ASTRecordLayout &RL =
    CGM.getContext().getASTObjCImplementationLayout(OID);

  // InstanceSize is really instance end.
  InstanceSize = RL.getDataSize().getQuantity();

  // If there are no fields, the start is the same as the end.
  if (!RL.getFieldCount())
    InstanceStart = InstanceSize;
  else
    InstanceStart = RL.getFieldOffset(0) / CGM.getContext().getCharWidth();
}

static llvm::GlobalValue::DLLStorageClassTypes getStorage(CodeGenModule &CGM,
                                                          StringRef Name) {
  IdentifierInfo &II = CGM.getContext().Idents.get(Name);
  TranslationUnitDecl *TUDecl = CGM.getContext().getTranslationUnitDecl();
  DeclContext *DC = TranslationUnitDecl::castToDeclContext(TUDecl);

  const VarDecl *VD = nullptr;
  for (const auto &Result : DC->lookup(&II))
    if ((VD = dyn_cast<VarDecl>(Result)))
      break;

  if (!VD)
    return llvm::GlobalValue::DLLImportStorageClass;
  if (VD->hasAttr<DLLExportAttr>())
    return llvm::GlobalValue::DLLExportStorageClass;
  if (VD->hasAttr<DLLImportAttr>())
    return llvm::GlobalValue::DLLImportStorageClass;
  return llvm::GlobalValue::DefaultStorageClass;
}

void CGObjCNonFragileABIMac::GenerateClass(const ObjCImplementationDecl *ID) {
  if (!ObjCEmptyCacheVar) {
    ObjCEmptyCacheVar =
        new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.CacheTy, false,
                                 llvm::GlobalValue::ExternalLinkage, nullptr,
                                 "_objc_empty_cache");
    if (CGM.getTriple().isOSBinFormatCOFF())
      ObjCEmptyCacheVar->setDLLStorageClass(getStorage(CGM, "_objc_empty_cache"));

    // Only OS X with deployment version <10.9 use the empty vtable symbol
    const llvm::Triple &Triple = CGM.getTarget().getTriple();
    if (Triple.isMacOSX() && Triple.isMacOSXVersionLT(10, 9))
      ObjCEmptyVtableVar =
          new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ImpnfABITy, false,
                                   llvm::GlobalValue::ExternalLinkage, nullptr,
                                   "_objc_empty_vtable");
  }

  // FIXME: Is this correct (that meta class size is never computed)?
  uint32_t InstanceStart =
    CGM.getDataLayout().getTypeAllocSize(ObjCTypes.ClassnfABITy);
  uint32_t InstanceSize = InstanceStart;
  uint32_t flags = NonFragileABI_Class_Meta;

  llvm::GlobalVariable *SuperClassGV, *IsAGV;

  StringRef ClassName = ID->getObjCRuntimeNameAsString();
  const auto *CI = ID->getClassInterface();
  assert(CI && "CGObjCNonFragileABIMac::GenerateClass - class is 0");

  // Build the flags for the metaclass.
  bool classIsHidden = (CGM.getTriple().isOSBinFormatCOFF())
                           ? !CI->hasAttr<DLLExportAttr>()
                           : CI->getVisibility() == HiddenVisibility;
  if (classIsHidden)
    flags |= NonFragileABI_Class_Hidden;

  // FIXME: why is this flag set on the metaclass?
  // ObjC metaclasses have no fields and don't really get constructed.
  if (ID->hasNonZeroConstructors() || ID->hasDestructors()) {
    flags |= NonFragileABI_Class_HasCXXStructors;
    if (!ID->hasNonZeroConstructors())
      flags |= NonFragileABI_Class_HasCXXDestructorOnly;
  }

  if (!CI->getSuperClass()) {
    // class is root
    flags |= NonFragileABI_Class_Root;

    SuperClassGV = GetClassGlobal((getClassSymbolPrefix() + ClassName).str(),
                                  CI->isWeakImported());
    if (CGM.getTriple().isOSBinFormatCOFF())
      if (CI->hasAttr<DLLImportAttr>())
        SuperClassGV->setDLLStorageClass(llvm::GlobalValue::DLLImportStorageClass);

    IsAGV = GetClassGlobal((getMetaclassSymbolPrefix() + ClassName).str(),
                           CI->isWeakImported());
    if (CGM.getTriple().isOSBinFormatCOFF())
      if (CI->hasAttr<DLLImportAttr>())
        IsAGV->setDLLStorageClass(llvm::GlobalValue::DLLImportStorageClass);
  } else {
    // Has a root. Current class is not a root.
    const ObjCInterfaceDecl *Root = ID->getClassInterface();
    while (const ObjCInterfaceDecl *Super = Root->getSuperClass())
      Root = Super;

    const auto *Super = CI->getSuperClass();
    StringRef RootClassName = Root->getObjCRuntimeNameAsString();
    StringRef SuperClassName = Super->getObjCRuntimeNameAsString();

    IsAGV = GetClassGlobal((getMetaclassSymbolPrefix() + RootClassName).str(),
                           Root->isWeakImported());
    if (CGM.getTriple().isOSBinFormatCOFF())
      if (Root->hasAttr<DLLImportAttr>())
        IsAGV->setDLLStorageClass(llvm::GlobalValue::DLLImportStorageClass);

    // work on super class metadata symbol.
    SuperClassGV =
        GetClassGlobal((getMetaclassSymbolPrefix() + SuperClassName).str(),
                       Super->isWeakImported());
    if (CGM.getTriple().isOSBinFormatCOFF())
      if (Super->hasAttr<DLLImportAttr>())
        SuperClassGV->setDLLStorageClass(llvm::GlobalValue::DLLImportStorageClass);
  }

  llvm::GlobalVariable *CLASS_RO_GV =
      BuildClassRoTInitializer(flags, InstanceStart, InstanceSize, ID);

  llvm::GlobalVariable *MetaTClass =
      BuildClassMetaData((getMetaclassSymbolPrefix() + ClassName).str(), IsAGV,
                         SuperClassGV, CLASS_RO_GV, classIsHidden,
                         CI->isWeakImported());
  if (CGM.getTriple().isOSBinFormatCOFF())
    if (CI->hasAttr<DLLExportAttr>())
      MetaTClass->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);
  DefinedMetaClasses.push_back(MetaTClass);

  // Metadata for the class
  flags = 0;
  if (classIsHidden)
    flags |= NonFragileABI_Class_Hidden;

  if (ID->hasNonZeroConstructors() || ID->hasDestructors()) {
    flags |= NonFragileABI_Class_HasCXXStructors;

    // Set a flag to enable a runtime optimization when a class has
    // fields that require destruction but which don't require
    // anything except zero-initialization during construction.  This
    // is most notably true of __strong and __weak types, but you can
    // also imagine there being C++ types with non-trivial default
    // constructors that merely set all fields to null.
    if (!ID->hasNonZeroConstructors())
      flags |= NonFragileABI_Class_HasCXXDestructorOnly;
  }

  if (hasObjCExceptionAttribute(CGM.getContext(), CI))
    flags |= NonFragileABI_Class_Exception;

  if (!CI->getSuperClass()) {
    flags |= NonFragileABI_Class_Root;
    SuperClassGV = nullptr;
  } else {
    // Has a root. Current class is not a root.
    const auto *Super = CI->getSuperClass();
    StringRef SuperClassName = Super->getObjCRuntimeNameAsString();

    SuperClassGV =
        GetClassGlobal((getClassSymbolPrefix() + SuperClassName).str(),
                       Super->isWeakImported());
    if (CGM.getTriple().isOSBinFormatCOFF())
      if (Super->hasAttr<DLLImportAttr>())
        SuperClassGV->setDLLStorageClass(llvm::GlobalValue::DLLImportStorageClass);
  }

  GetClassSizeInfo(ID, InstanceStart, InstanceSize);
  CLASS_RO_GV =
      BuildClassRoTInitializer(flags, InstanceStart, InstanceSize, ID);

  llvm::GlobalVariable *ClassMD =
    BuildClassMetaData((getClassSymbolPrefix() + ClassName).str(), MetaTClass,
                       SuperClassGV, CLASS_RO_GV, classIsHidden,
                       CI->isWeakImported());
  if (CGM.getTriple().isOSBinFormatCOFF())
    if (CI->hasAttr<DLLExportAttr>())
      ClassMD->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);
  DefinedClasses.push_back(ClassMD);
  ImplementedClasses.push_back(CI);

  // Determine if this class is also "non-lazy".
  if (ImplementationIsNonLazy(ID))
    DefinedNonLazyClasses.push_back(ClassMD);

  // Force the definition of the EHType if necessary.
  if (flags & NonFragileABI_Class_Exception)
    GetInterfaceEHType(CI, true);
  // Make sure method definition entries are all clear for next implementation.
  MethodDefinitions.clear();
}

/// GenerateProtocolRef - This routine is called to generate code for
/// a protocol reference expression; as in:
/// @code
///   @protocol(Proto1);
/// @endcode
/// It generates a weak reference to l_OBJC_PROTOCOL_REFERENCE_$_Proto1
/// which will hold address of the protocol meta-data.
///
llvm::Value *CGObjCNonFragileABIMac::GenerateProtocolRef(CodeGenFunction &CGF,
                                                         const ObjCProtocolDecl *PD) {

  // This routine is called for @protocol only. So, we must build definition
  // of protocol's meta-data (not a reference to it!)
  //
  llvm::Constant *Init =
    llvm::ConstantExpr::getBitCast(GetOrEmitProtocol(PD),
                                   ObjCTypes.getExternalProtocolPtrTy());

  std::string ProtocolName("\01l_OBJC_PROTOCOL_REFERENCE_$_");
  ProtocolName += PD->getObjCRuntimeNameAsString();

  CharUnits Align = CGF.getPointerAlign();

  llvm::GlobalVariable *PTGV = CGM.getModule().getGlobalVariable(ProtocolName);
  if (PTGV)
    return CGF.Builder.CreateAlignedLoad(PTGV, Align);
  PTGV = new llvm::GlobalVariable(
    CGM.getModule(),
    Init->getType(), false,
    llvm::GlobalValue::WeakAnyLinkage,
    Init,
    ProtocolName);
  PTGV->setSection("__DATA, __objc_protorefs, coalesced, no_dead_strip");
  PTGV->setVisibility(llvm::GlobalValue::HiddenVisibility);
  PTGV->setAlignment(Align.getQuantity());
  CGM.addCompilerUsedGlobal(PTGV);
  return CGF.Builder.CreateAlignedLoad(PTGV, Align);
}

/// GenerateCategory - Build metadata for a category implementation.
/// struct _category_t {
///   const char * const name;
///   struct _class_t *const cls;
///   const struct _method_list_t * const instance_methods;
///   const struct _method_list_t * const class_methods;
///   const struct _protocol_list_t * const protocols;
///   const struct _prop_list_t * const properties;
///   const struct _prop_list_t * const class_properties;
///   const uint32_t size;
/// }
///
void CGObjCNonFragileABIMac::GenerateCategory(const ObjCCategoryImplDecl *OCD) {
  const ObjCInterfaceDecl *Interface = OCD->getClassInterface();
  const char *Prefix = "\01l_OBJC_$_CATEGORY_";
    
  llvm::SmallString<64> ExtCatName(Prefix);
  ExtCatName += Interface->getObjCRuntimeNameAsString();
  ExtCatName += "_$_";
  ExtCatName += OCD->getNameAsString();
    
  llvm::SmallString<64> ExtClassName(getClassSymbolPrefix());
  ExtClassName += Interface->getObjCRuntimeNameAsString();

  llvm::Constant *Values[8];
  Values[0] = GetClassName(OCD->getIdentifier()->getName());
  // meta-class entry symbol
  llvm::GlobalVariable *ClassGV =
      GetClassGlobal(ExtClassName.str(), Interface->isWeakImported());

  Values[1] = ClassGV;
  std::vector<llvm::Constant*> Methods;
  llvm::SmallString<64> MethodListName(Prefix);
    
  MethodListName += "INSTANCE_METHODS_";
  MethodListName += Interface->getObjCRuntimeNameAsString();
  MethodListName += "_$_";
  MethodListName += OCD->getName();

  for (const auto *I : OCD->instance_methods())
    // Instance methods should always be defined.
    Methods.push_back(GetMethodConstant(I));

  Values[2] = EmitMethodList(MethodListName.str(),
                             "__DATA, __objc_const",
                             Methods);

  MethodListName = Prefix;
  MethodListName += "CLASS_METHODS_";
  MethodListName += Interface->getObjCRuntimeNameAsString();
  MethodListName += "_$_";
  MethodListName += OCD->getNameAsString();
    
  Methods.clear();
  for (const auto *I : OCD->class_methods())
    // Class methods should always be defined.
    Methods.push_back(GetMethodConstant(I));

  Values[3] = EmitMethodList(MethodListName.str(),
                             "__DATA, __objc_const",
                             Methods);
  const ObjCCategoryDecl *Category =
    Interface->FindCategoryDeclaration(OCD->getIdentifier());
  if (Category) {
    SmallString<256> ExtName;
    llvm::raw_svector_ostream(ExtName) << Interface->getObjCRuntimeNameAsString() << "_$_"
                                       << OCD->getName();
    Values[4] = EmitProtocolList("\01l_OBJC_CATEGORY_PROTOCOLS_$_"
                                   + Interface->getObjCRuntimeNameAsString() + "_$_"
                                   + Category->getName(),
                                   Category->protocol_begin(),
                                   Category->protocol_end());
    Values[5] = EmitPropertyList("\01l_OBJC_$_PROP_LIST_" + ExtName.str(),
                                 OCD, Category, ObjCTypes, false);
    Values[6] = EmitPropertyList("\01l_OBJC_$_CLASS_PROP_LIST_" + ExtName.str(),
                                 OCD, Category, ObjCTypes, true);
  } else {
    Values[4] = llvm::Constant::getNullValue(ObjCTypes.ProtocolListnfABIPtrTy);
    Values[5] = llvm::Constant::getNullValue(ObjCTypes.PropertyListPtrTy);
    Values[6] = llvm::Constant::getNullValue(ObjCTypes.PropertyListPtrTy);
  }

  unsigned Size = CGM.getDataLayout().getTypeAllocSize(ObjCTypes.CategorynfABITy);
  Values[7] = llvm::ConstantInt::get(ObjCTypes.IntTy, Size);

  llvm::Constant *Init =
    llvm::ConstantStruct::get(ObjCTypes.CategorynfABITy,
                              Values);
  llvm::GlobalVariable *GCATV
    = new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.CategorynfABITy,
                               false,
                               llvm::GlobalValue::PrivateLinkage,
                               Init,
                               ExtCatName.str());
  GCATV->setAlignment(
    CGM.getDataLayout().getABITypeAlignment(ObjCTypes.CategorynfABITy));
  GCATV->setSection("__DATA, __objc_const");
  CGM.addCompilerUsedGlobal(GCATV);
  DefinedCategories.push_back(GCATV);

  // Determine if this category is also "non-lazy".
  if (ImplementationIsNonLazy(OCD))
    DefinedNonLazyCategories.push_back(GCATV);
  // method definition entries must be clear for next implementation.
  MethodDefinitions.clear();
}

/// GetMethodConstant - Return a struct objc_method constant for the
/// given method if it has been defined. The result is null if the
/// method has not been defined. The return value has type MethodPtrTy.
llvm::Constant *CGObjCNonFragileABIMac::GetMethodConstant(
  const ObjCMethodDecl *MD) {
  llvm::Function *Fn = GetMethodDefinition(MD);
  if (!Fn)
    return nullptr;

  llvm::Constant *Method[] = {
    llvm::ConstantExpr::getBitCast(GetMethodVarName(MD->getSelector()),
                                   ObjCTypes.SelectorPtrTy),
    GetMethodVarType(MD),
    llvm::ConstantExpr::getBitCast(Fn, ObjCTypes.Int8PtrTy)
  };
  return llvm::ConstantStruct::get(ObjCTypes.MethodTy, Method);
}

/// EmitMethodList - Build meta-data for method declarations
/// struct _method_list_t {
///   uint32_t entsize;  // sizeof(struct _objc_method)
///   uint32_t method_count;
///   struct _objc_method method_list[method_count];
/// }
///
llvm::Constant *
CGObjCNonFragileABIMac::EmitMethodList(Twine Name, StringRef Section,
                                       ArrayRef<llvm::Constant *> Methods) {
  // Return null for empty list.
  if (Methods.empty())
    return llvm::Constant::getNullValue(ObjCTypes.MethodListnfABIPtrTy);

  llvm::Constant *Values[3];
  // sizeof(struct _objc_method)
  unsigned Size = CGM.getDataLayout().getTypeAllocSize(ObjCTypes.MethodTy);
  Values[0] = llvm::ConstantInt::get(ObjCTypes.IntTy, Size);
  // method_count
  Values[1] = llvm::ConstantInt::get(ObjCTypes.IntTy, Methods.size());
  llvm::ArrayType *AT = llvm::ArrayType::get(ObjCTypes.MethodTy,
                                             Methods.size());
  Values[2] = llvm::ConstantArray::get(AT, Methods);
  llvm::Constant *Init = llvm::ConstantStruct::getAnon(Values);

  llvm::GlobalVariable *GV =
    new llvm::GlobalVariable(CGM.getModule(), Init->getType(), false,
                             llvm::GlobalValue::PrivateLinkage, Init, Name);
  GV->setAlignment(CGM.getDataLayout().getABITypeAlignment(Init->getType()));
  GV->setSection(Section);
  CGM.addCompilerUsedGlobal(GV);
  return llvm::ConstantExpr::getBitCast(GV, ObjCTypes.MethodListnfABIPtrTy);
}

/// ObjCIvarOffsetVariable - Returns the ivar offset variable for
/// the given ivar.
llvm::GlobalVariable *
CGObjCNonFragileABIMac::ObjCIvarOffsetVariable(const ObjCInterfaceDecl *ID,
                                               const ObjCIvarDecl *Ivar) {
  const ObjCInterfaceDecl *Container = Ivar->getContainingInterface();
  llvm::SmallString<64> Name("OBJC_IVAR_$_");
  Name += Container->getObjCRuntimeNameAsString();
  Name += ".";
  Name += Ivar->getName();
  llvm::GlobalVariable *IvarOffsetGV = CGM.getModule().getGlobalVariable(Name);
  if (!IvarOffsetGV) {
    IvarOffsetGV =
        new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.IvarOffsetVarTy,
                                 false, llvm::GlobalValue::ExternalLinkage,
                                 nullptr, Name.str());
    if (CGM.getTriple().isOSBinFormatCOFF()) {
      bool IsPrivateOrPackage =
          Ivar->getAccessControl() == ObjCIvarDecl::Private ||
          Ivar->getAccessControl() == ObjCIvarDecl::Package;

      if (ID->hasAttr<DLLExportAttr>() && !IsPrivateOrPackage)
        IvarOffsetGV->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);
      else if (ID->hasAttr<DLLImportAttr>())
        IvarOffsetGV->setDLLStorageClass(llvm::GlobalValue::DLLImportStorageClass);
    }
  }
  return IvarOffsetGV;
}

llvm::Constant *
CGObjCNonFragileABIMac::EmitIvarOffsetVar(const ObjCInterfaceDecl *ID,
                                          const ObjCIvarDecl *Ivar,
                                          unsigned long int Offset) {
  llvm::GlobalVariable *IvarOffsetGV = ObjCIvarOffsetVariable(ID, Ivar);
  IvarOffsetGV->setInitializer(
      llvm::ConstantInt::get(ObjCTypes.IvarOffsetVarTy, Offset));
  IvarOffsetGV->setAlignment(
      CGM.getDataLayout().getABITypeAlignment(ObjCTypes.IvarOffsetVarTy));

  if (!CGM.getTriple().isOSBinFormatCOFF()) {
    // FIXME: This matches gcc, but shouldn't the visibility be set on the use
    // as well (i.e., in ObjCIvarOffsetVariable).
    if (Ivar->getAccessControl() == ObjCIvarDecl::Private ||
        Ivar->getAccessControl() == ObjCIvarDecl::Package ||
        ID->getVisibility() == HiddenVisibility)
      IvarOffsetGV->setVisibility(llvm::GlobalValue::HiddenVisibility);
    else
      IvarOffsetGV->setVisibility(llvm::GlobalValue::DefaultVisibility);
  }

  IvarOffsetGV->setSection("__DATA, __objc_ivar");
  return IvarOffsetGV;
}

/// EmitIvarList - Emit the ivar list for the given
/// implementation. The return value has type
/// IvarListnfABIPtrTy.
///  struct _ivar_t {
///   unsigned [long] int *offset;  // pointer to ivar offset location
///   char *name;
///   char *type;
///   uint32_t alignment;
///   uint32_t size;
/// }
/// struct _ivar_list_t {
///   uint32 entsize;  // sizeof(struct _ivar_t)
///   uint32 count;
///   struct _iver_t list[count];
/// }
///

llvm::Constant *CGObjCNonFragileABIMac::EmitIvarList(
  const ObjCImplementationDecl *ID) {

  std::vector<llvm::Constant*> Ivars;

  const ObjCInterfaceDecl *OID = ID->getClassInterface();
  assert(OID && "CGObjCNonFragileABIMac::EmitIvarList - null interface");

  // FIXME. Consolidate this with similar code in GenerateClass.

  for (const ObjCIvarDecl *IVD = OID->all_declared_ivar_begin(); 
       IVD; IVD = IVD->getNextIvar()) {
    // Ignore unnamed bit-fields.
    if (!IVD->getDeclName())
      continue;
    llvm::Constant *Ivar[5];
    Ivar[0] = EmitIvarOffsetVar(ID->getClassInterface(), IVD,
                                ComputeIvarBaseOffset(CGM, ID, IVD));
    Ivar[1] = GetMethodVarName(IVD->getIdentifier());
    Ivar[2] = GetMethodVarType(IVD);
    llvm::Type *FieldTy =
      CGM.getTypes().ConvertTypeForMem(IVD->getType());
    unsigned Size = CGM.getDataLayout().getTypeAllocSize(FieldTy);
    unsigned Align = CGM.getContext().getPreferredTypeAlign(
      IVD->getType().getTypePtr()) >> 3;
    Align = llvm::Log2_32(Align);
    Ivar[3] = llvm::ConstantInt::get(ObjCTypes.IntTy, Align);
    // NOTE. Size of a bitfield does not match gcc's, because of the
    // way bitfields are treated special in each. But I am told that
    // 'size' for bitfield ivars is ignored by the runtime so it does
    // not matter.  If it matters, there is enough info to get the
    // bitfield right!
    Ivar[4] = llvm::ConstantInt::get(ObjCTypes.IntTy, Size);
    Ivars.push_back(llvm::ConstantStruct::get(ObjCTypes.IvarnfABITy, Ivar));
  }
  // Return null for empty list.
  if (Ivars.empty())
    return llvm::Constant::getNullValue(ObjCTypes.IvarListnfABIPtrTy);

  llvm::Constant *Values[3];
  unsigned Size = CGM.getDataLayout().getTypeAllocSize(ObjCTypes.IvarnfABITy);
  Values[0] = llvm::ConstantInt::get(ObjCTypes.IntTy, Size);
  Values[1] = llvm::ConstantInt::get(ObjCTypes.IntTy, Ivars.size());
  llvm::ArrayType *AT = llvm::ArrayType::get(ObjCTypes.IvarnfABITy,
                                             Ivars.size());
  Values[2] = llvm::ConstantArray::get(AT, Ivars);
  llvm::Constant *Init = llvm::ConstantStruct::getAnon(Values);
  const char *Prefix = "\01l_OBJC_$_INSTANCE_VARIABLES_";
  llvm::GlobalVariable *GV =
    new llvm::GlobalVariable(CGM.getModule(), Init->getType(), false,
                             llvm::GlobalValue::PrivateLinkage,
                             Init,
                             Prefix + OID->getObjCRuntimeNameAsString());
  GV->setAlignment(
    CGM.getDataLayout().getABITypeAlignment(Init->getType()));
  GV->setSection("__DATA, __objc_const");

  CGM.addCompilerUsedGlobal(GV);
  return llvm::ConstantExpr::getBitCast(GV, ObjCTypes.IvarListnfABIPtrTy);
}

llvm::Constant *CGObjCNonFragileABIMac::GetOrEmitProtocolRef(
  const ObjCProtocolDecl *PD) {
  llvm::GlobalVariable *&Entry = Protocols[PD->getIdentifier()];

  if (!Entry)
    // We use the initializer as a marker of whether this is a forward
    // reference or not. At module finalization we add the empty
    // contents for protocols which were referenced but never defined.
    Entry =
        new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ProtocolnfABITy,
                                 false, llvm::GlobalValue::ExternalLinkage,
                                 nullptr,
                                 "\01l_OBJC_PROTOCOL_$_" + PD->getObjCRuntimeNameAsString());

  return Entry;
}

/// GetOrEmitProtocol - Generate the protocol meta-data:
/// @code
/// struct _protocol_t {
///   id isa;  // NULL
///   const char * const protocol_name;
///   const struct _protocol_list_t * protocol_list; // super protocols
///   const struct method_list_t * const instance_methods;
///   const struct method_list_t * const class_methods;
///   const struct method_list_t *optionalInstanceMethods;
///   const struct method_list_t *optionalClassMethods;
///   const struct _prop_list_t * properties;
///   const uint32_t size;  // sizeof(struct _protocol_t)
///   const uint32_t flags;  // = 0
///   const char ** extendedMethodTypes;
///   const char *demangledName;
///   const struct _prop_list_t * class_properties;
/// }
/// @endcode
///

llvm::Constant *CGObjCNonFragileABIMac::GetOrEmitProtocol(
  const ObjCProtocolDecl *PD) {
  llvm::GlobalVariable *Entry = Protocols[PD->getIdentifier()];

  // Early exit if a defining object has already been generated.
  if (Entry && Entry->hasInitializer())
    return Entry;

  // Use the protocol definition, if there is one.
  if (const ObjCProtocolDecl *Def = PD->getDefinition())
    PD = Def;
  
  // Construct method lists.
  std::vector<llvm::Constant*> InstanceMethods, ClassMethods;
  std::vector<llvm::Constant*> OptInstanceMethods, OptClassMethods;
  std::vector<llvm::Constant*> MethodTypesExt, OptMethodTypesExt;
  for (const auto *MD : PD->instance_methods()) {
    llvm::Constant *C = GetMethodDescriptionConstant(MD);
    if (!C)
      return GetOrEmitProtocolRef(PD);
    
    if (MD->getImplementationControl() == ObjCMethodDecl::Optional) {
      OptInstanceMethods.push_back(C);
      OptMethodTypesExt.push_back(GetMethodVarType(MD, true));
    } else {
      InstanceMethods.push_back(C);
      MethodTypesExt.push_back(GetMethodVarType(MD, true));
    }
  }

  for (const auto *MD : PD->class_methods()) {
    llvm::Constant *C = GetMethodDescriptionConstant(MD);
    if (!C)
      return GetOrEmitProtocolRef(PD);

    if (MD->getImplementationControl() == ObjCMethodDecl::Optional) {
      OptClassMethods.push_back(C);
      OptMethodTypesExt.push_back(GetMethodVarType(MD, true));
    } else {
      ClassMethods.push_back(C);
      MethodTypesExt.push_back(GetMethodVarType(MD, true));
    }
  }

  MethodTypesExt.insert(MethodTypesExt.end(),
                        OptMethodTypesExt.begin(), OptMethodTypesExt.end());

  llvm::Constant *Values[13];
  // isa is NULL
  Values[0] = llvm::Constant::getNullValue(ObjCTypes.ObjectPtrTy);
  Values[1] = GetClassName(PD->getObjCRuntimeNameAsString());
  Values[2] = EmitProtocolList("\01l_OBJC_$_PROTOCOL_REFS_" + PD->getObjCRuntimeNameAsString(),
                               PD->protocol_begin(),
                               PD->protocol_end());

  Values[3] = EmitMethodList("\01l_OBJC_$_PROTOCOL_INSTANCE_METHODS_"
                             + PD->getObjCRuntimeNameAsString(),
                             "__DATA, __objc_const",
                             InstanceMethods);
  Values[4] = EmitMethodList("\01l_OBJC_$_PROTOCOL_CLASS_METHODS_"
                             + PD->getObjCRuntimeNameAsString(),
                             "__DATA, __objc_const",
                             ClassMethods);
  Values[5] = EmitMethodList("\01l_OBJC_$_PROTOCOL_INSTANCE_METHODS_OPT_"
                             + PD->getObjCRuntimeNameAsString(),
                             "__DATA, __objc_const",
                             OptInstanceMethods);
  Values[6] = EmitMethodList("\01l_OBJC_$_PROTOCOL_CLASS_METHODS_OPT_"
                             + PD->getObjCRuntimeNameAsString(),
                             "__DATA, __objc_const",
                             OptClassMethods);
  Values[7] = EmitPropertyList(
      "\01l_OBJC_$_PROP_LIST_" + PD->getObjCRuntimeNameAsString(),
      nullptr, PD, ObjCTypes, false);
  uint32_t Size =
    CGM.getDataLayout().getTypeAllocSize(ObjCTypes.ProtocolnfABITy);
  Values[8] = llvm::ConstantInt::get(ObjCTypes.IntTy, Size);
  Values[9] = llvm::Constant::getNullValue(ObjCTypes.IntTy);
  Values[10] = EmitProtocolMethodTypes("\01l_OBJC_$_PROTOCOL_METHOD_TYPES_"
                                       + PD->getObjCRuntimeNameAsString(),
                                       MethodTypesExt, ObjCTypes);
  // const char *demangledName;
  Values[11] = llvm::Constant::getNullValue(ObjCTypes.Int8PtrTy);

  Values[12] = EmitPropertyList(
      "\01l_OBJC_$_CLASS_PROP_LIST_" + PD->getObjCRuntimeNameAsString(),
      nullptr, PD, ObjCTypes, true);
    
  llvm::Constant *Init = llvm::ConstantStruct::get(ObjCTypes.ProtocolnfABITy,
                                                   Values);

  if (Entry) {
    // Already created, fix the linkage and update the initializer.
    Entry->setLinkage(llvm::GlobalValue::WeakAnyLinkage);
    Entry->setInitializer(Init);
  } else {
    Entry =
      new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ProtocolnfABITy,
                               false, llvm::GlobalValue::WeakAnyLinkage, Init,
                               "\01l_OBJC_PROTOCOL_$_" + PD->getObjCRuntimeNameAsString());
    Entry->setAlignment(
      CGM.getDataLayout().getABITypeAlignment(ObjCTypes.ProtocolnfABITy));

    Protocols[PD->getIdentifier()] = Entry;
  }
  Entry->setVisibility(llvm::GlobalValue::HiddenVisibility);
  CGM.addCompilerUsedGlobal(Entry);

  // Use this protocol meta-data to build protocol list table in section
  // __DATA, __objc_protolist
  llvm::GlobalVariable *PTGV =
    new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ProtocolnfABIPtrTy,
                             false, llvm::GlobalValue::WeakAnyLinkage, Entry,
                             "\01l_OBJC_LABEL_PROTOCOL_$_" + PD->getObjCRuntimeNameAsString());
  PTGV->setAlignment(
    CGM.getDataLayout().getABITypeAlignment(ObjCTypes.ProtocolnfABIPtrTy));
  PTGV->setSection("__DATA, __objc_protolist, coalesced, no_dead_strip");
  PTGV->setVisibility(llvm::GlobalValue::HiddenVisibility);
  CGM.addCompilerUsedGlobal(PTGV);
  return Entry;
}

/// EmitProtocolList - Generate protocol list meta-data:
/// @code
/// struct _protocol_list_t {
///   long protocol_count;   // Note, this is 32/64 bit
///   struct _protocol_t[protocol_count];
/// }
/// @endcode
///
llvm::Constant *
CGObjCNonFragileABIMac::EmitProtocolList(Twine Name,
                                      ObjCProtocolDecl::protocol_iterator begin,
                                      ObjCProtocolDecl::protocol_iterator end) {
  SmallVector<llvm::Constant *, 16> ProtocolRefs;

  // Just return null for empty protocol lists
  if (begin == end)
    return llvm::Constant::getNullValue(ObjCTypes.ProtocolListnfABIPtrTy);

  // FIXME: We shouldn't need to do this lookup here, should we?
  SmallString<256> TmpName;
  Name.toVector(TmpName);
  llvm::GlobalVariable *GV =
    CGM.getModule().getGlobalVariable(TmpName.str(), true);
  if (GV)
    return llvm::ConstantExpr::getBitCast(GV, ObjCTypes.ProtocolListnfABIPtrTy);

  for (; begin != end; ++begin)
    ProtocolRefs.push_back(GetProtocolRef(*begin));  // Implemented???

  // This list is null terminated.
  ProtocolRefs.push_back(llvm::Constant::getNullValue(
                           ObjCTypes.ProtocolnfABIPtrTy));

  llvm::Constant *Values[2];
  Values[0] =
    llvm::ConstantInt::get(ObjCTypes.LongTy, ProtocolRefs.size() - 1);
  Values[1] =
    llvm::ConstantArray::get(llvm::ArrayType::get(ObjCTypes.ProtocolnfABIPtrTy,
                                                  ProtocolRefs.size()),
                             ProtocolRefs);

  llvm::Constant *Init = llvm::ConstantStruct::getAnon(Values);
  GV = new llvm::GlobalVariable(CGM.getModule(), Init->getType(), false,
                                llvm::GlobalValue::PrivateLinkage,
                                Init, Name);
  GV->setSection("__DATA, __objc_const");
  GV->setAlignment(
    CGM.getDataLayout().getABITypeAlignment(Init->getType()));
  CGM.addCompilerUsedGlobal(GV);
  return llvm::ConstantExpr::getBitCast(GV,
                                        ObjCTypes.ProtocolListnfABIPtrTy);
}

/// GetMethodDescriptionConstant - This routine build following meta-data:
/// struct _objc_method {
///   SEL _cmd;
///   char *method_type;
///   char *_imp;
/// }

llvm::Constant *
CGObjCNonFragileABIMac::GetMethodDescriptionConstant(const ObjCMethodDecl *MD) {
  llvm::Constant *Desc[3];
  Desc[0] =
    llvm::ConstantExpr::getBitCast(GetMethodVarName(MD->getSelector()),
                                   ObjCTypes.SelectorPtrTy);
  Desc[1] = GetMethodVarType(MD);
  if (!Desc[1])
    return nullptr;

  // Protocol methods have no implementation. So, this entry is always NULL.
  Desc[2] = llvm::Constant::getNullValue(ObjCTypes.Int8PtrTy);
  return llvm::ConstantStruct::get(ObjCTypes.MethodTy, Desc);
}

/// EmitObjCValueForIvar - Code Gen for nonfragile ivar reference.
/// This code gen. amounts to generating code for:
/// @code
/// (type *)((char *)base + _OBJC_IVAR_$_.ivar;
/// @encode
///
LValue CGObjCNonFragileABIMac::EmitObjCValueForIvar(
                                               CodeGen::CodeGenFunction &CGF,
                                               QualType ObjectTy,
                                               llvm::Value *BaseValue,
                                               const ObjCIvarDecl *Ivar,
                                               unsigned CVRQualifiers) {
  ObjCInterfaceDecl *ID = ObjectTy->getAs<ObjCObjectType>()->getInterface();
  llvm::Value *Offset = EmitIvarOffset(CGF, ID, Ivar);
  return EmitValueForIvarAtOffset(CGF, ID, BaseValue, Ivar, CVRQualifiers,
                                  Offset);
}

llvm::Value *CGObjCNonFragileABIMac::EmitIvarOffset(
  CodeGen::CodeGenFunction &CGF,
  const ObjCInterfaceDecl *Interface,
  const ObjCIvarDecl *Ivar) {
  llvm::Value *IvarOffsetValue = ObjCIvarOffsetVariable(Interface, Ivar);
  IvarOffsetValue = CGF.Builder.CreateAlignedLoad(IvarOffsetValue,
                                                  CGF.getSizeAlign(), "ivar");
  if (IsIvarOffsetKnownIdempotent(CGF, Ivar))
    cast<llvm::LoadInst>(IvarOffsetValue)
        ->setMetadata(CGM.getModule().getMDKindID("invariant.load"),
                      llvm::MDNode::get(VMContext, None));

  // This could be 32bit int or 64bit integer depending on the architecture.
  // Cast it to 64bit integer value, if it is a 32bit integer ivar offset value
  //  as this is what caller always expectes.
  if (ObjCTypes.IvarOffsetVarTy == ObjCTypes.IntTy)
    IvarOffsetValue = CGF.Builder.CreateIntCast(
        IvarOffsetValue, ObjCTypes.LongTy, true, "ivar.conv");
  return IvarOffsetValue;
}

static void appendSelectorForMessageRefTable(std::string &buffer,
                                             Selector selector) {
  if (selector.isUnarySelector()) {
    buffer += selector.getNameForSlot(0);
    return;
  }

  for (unsigned i = 0, e = selector.getNumArgs(); i != e; ++i) {
    buffer += selector.getNameForSlot(i);
    buffer += '_';
  }
}

/// Emit a "vtable" message send.  We emit a weak hidden-visibility
/// struct, initially containing the selector pointer and a pointer to
/// a "fixup" variant of the appropriate objc_msgSend.  To call, we
/// load and call the function pointer, passing the address of the
/// struct as the second parameter.  The runtime determines whether
/// the selector is currently emitted using vtable dispatch; if so, it
/// substitutes a stub function which simply tail-calls through the
/// appropriate vtable slot, and if not, it substitues a stub function
/// which tail-calls objc_msgSend.  Both stubs adjust the selector
/// argument to correctly point to the selector.
RValue
CGObjCNonFragileABIMac::EmitVTableMessageSend(CodeGenFunction &CGF,
                                              ReturnValueSlot returnSlot,
                                              QualType resultType,
                                              Selector selector,
                                              llvm::Value *arg0,
                                              QualType arg0Type,
                                              bool isSuper,
                                              const CallArgList &formalArgs,
                                              const ObjCMethodDecl *method) {
  // Compute the actual arguments.
  CallArgList args;

  // First argument: the receiver / super-call structure.
  if (!isSuper)
    arg0 = CGF.Builder.CreateBitCast(arg0, ObjCTypes.ObjectPtrTy);
  args.add(RValue::get(arg0), arg0Type);

  // Second argument: a pointer to the message ref structure.  Leave
  // the actual argument value blank for now.
  args.add(RValue::get(nullptr), ObjCTypes.MessageRefCPtrTy);

  args.insert(args.end(), formalArgs.begin(), formalArgs.end());

  MessageSendInfo MSI = getMessageSendInfo(method, resultType, args);

  NullReturnState nullReturn;

  // Find the function to call and the mangled name for the message
  // ref structure.  Using a different mangled name wouldn't actually
  // be a problem; it would just be a waste.
  //
  // The runtime currently never uses vtable dispatch for anything
  // except normal, non-super message-sends.
  // FIXME: don't use this for that.
  llvm::Constant *fn = nullptr;
  std::string messageRefName("\01l_");
  if (CGM.ReturnSlotInterferesWithArgs(MSI.CallInfo)) {
    if (isSuper) {
      fn = ObjCTypes.getMessageSendSuper2StretFixupFn();
      messageRefName += "objc_msgSendSuper2_stret_fixup";
    } else {
      nullReturn.init(CGF, arg0);
      fn = ObjCTypes.getMessageSendStretFixupFn();
      messageRefName += "objc_msgSend_stret_fixup";
    }
  } else if (!isSuper && CGM.ReturnTypeUsesFPRet(resultType)) {
    fn = ObjCTypes.getMessageSendFpretFixupFn();
    messageRefName += "objc_msgSend_fpret_fixup";
  } else {
    if (isSuper) {
      fn = ObjCTypes.getMessageSendSuper2FixupFn();
      messageRefName += "objc_msgSendSuper2_fixup";
    } else {
      fn = ObjCTypes.getMessageSendFixupFn();
      messageRefName += "objc_msgSend_fixup";
    }
  }
  assert(fn && "CGObjCNonFragileABIMac::EmitMessageSend");
  messageRefName += '_';

  // Append the selector name, except use underscores anywhere we
  // would have used colons.
  appendSelectorForMessageRefTable(messageRefName, selector);

  llvm::GlobalVariable *messageRef
    = CGM.getModule().getGlobalVariable(messageRefName);
  if (!messageRef) {
    // Build the message ref structure.
    llvm::Constant *values[] = { fn, GetMethodVarName(selector) };
    llvm::Constant *init = llvm::ConstantStruct::getAnon(values);
    messageRef = new llvm::GlobalVariable(CGM.getModule(),
                                          init->getType(),
                                          /*constant*/ false,
                                          llvm::GlobalValue::WeakAnyLinkage,
                                          init,
                                          messageRefName);
    messageRef->setVisibility(llvm::GlobalValue::HiddenVisibility);
    messageRef->setAlignment(16);
    messageRef->setSection("__DATA, __objc_msgrefs, coalesced");
  }
  
  bool requiresnullCheck = false;
  if (CGM.getLangOpts().ObjCAutoRefCount && method)
    for (const auto *ParamDecl : method->parameters()) {
      if (ParamDecl->hasAttr<NSConsumedAttr>()) {
        if (!nullReturn.NullBB)
          nullReturn.init(CGF, arg0);
        requiresnullCheck = true;
        break;
      }
    }
  
  Address mref =
    Address(CGF.Builder.CreateBitCast(messageRef, ObjCTypes.MessageRefPtrTy),
            CGF.getPointerAlign());

  // Update the message ref argument.
  args[1].RV = RValue::get(mref.getPointer());

  // Load the function to call from the message ref table.
  Address calleeAddr =
      CGF.Builder.CreateStructGEP(mref, 0, CharUnits::Zero());
  llvm::Value *callee = CGF.Builder.CreateLoad(calleeAddr, "msgSend_fn");

  callee = CGF.Builder.CreateBitCast(callee, MSI.MessengerType);

  RValue result = CGF.EmitCall(MSI.CallInfo, callee, returnSlot, args);
  return nullReturn.complete(CGF, result, resultType, formalArgs,
                             requiresnullCheck ? method : nullptr);
}

/// Generate code for a message send expression in the nonfragile abi.
CodeGen::RValue
CGObjCNonFragileABIMac::GenerateMessageSend(CodeGen::CodeGenFunction &CGF,
                                            ReturnValueSlot Return,
                                            QualType ResultType,
                                            Selector Sel,
                                            llvm::Value *Receiver,
                                            const CallArgList &CallArgs,
                                            const ObjCInterfaceDecl *Class,
                                            const ObjCMethodDecl *Method) {
  return isVTableDispatchedSelector(Sel)
    ? EmitVTableMessageSend(CGF, Return, ResultType, Sel,
                            Receiver, CGF.getContext().getObjCIdType(),
                            false, CallArgs, Method)
    : EmitMessageSend(CGF, Return, ResultType,
                      EmitSelector(CGF, Sel),
                      Receiver, CGF.getContext().getObjCIdType(),
                      false, CallArgs, Method, Class, ObjCTypes);
}

llvm::GlobalVariable *
CGObjCNonFragileABIMac::GetClassGlobal(StringRef Name, bool Weak) {
  llvm::GlobalValue::LinkageTypes L =
      Weak ? llvm::GlobalValue::ExternalWeakLinkage
           : llvm::GlobalValue::ExternalLinkage;

  llvm::GlobalVariable *GV = CGM.getModule().getGlobalVariable(Name);

  if (!GV)
    GV = new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ClassnfABITy,
                                  false, L, nullptr, Name);

  assert(GV->getLinkage() == L);
  return GV;
}

llvm::Value *CGObjCNonFragileABIMac::EmitClassRefFromId(CodeGenFunction &CGF,
                                                        IdentifierInfo *II,
                                                        bool Weak,
                                                        const ObjCInterfaceDecl *ID) {
  CharUnits Align = CGF.getPointerAlign();
  llvm::GlobalVariable *&Entry = ClassReferences[II];
  
  if (!Entry) {
    StringRef Name = ID ? ID->getObjCRuntimeNameAsString() : II->getName();
    std::string ClassName = (getClassSymbolPrefix() + Name).str();
    llvm::GlobalVariable *ClassGV = GetClassGlobal(ClassName, Weak);
    Entry = new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ClassnfABIPtrTy,
                                     false, llvm::GlobalValue::PrivateLinkage,
                                     ClassGV, "OBJC_CLASSLIST_REFERENCES_$_");
    Entry->setAlignment(Align.getQuantity());
    Entry->setSection("__DATA, __objc_classrefs, regular, no_dead_strip");
    CGM.addCompilerUsedGlobal(Entry);
  }
  return CGF.Builder.CreateAlignedLoad(Entry, Align);
}

llvm::Value *CGObjCNonFragileABIMac::EmitClassRef(CodeGenFunction &CGF,
                                                  const ObjCInterfaceDecl *ID) {
  // If the class has the objc_runtime_visible attribute, we need to
  // use the Objective-C runtime to get the class.
  if (ID->hasAttr<ObjCRuntimeVisibleAttr>())
    return EmitClassRefViaRuntime(CGF, ID, ObjCTypes);

  return EmitClassRefFromId(CGF, ID->getIdentifier(), ID->isWeakImported(), ID);
}

llvm::Value *CGObjCNonFragileABIMac::EmitNSAutoreleasePoolClassRef(
                                                    CodeGenFunction &CGF) {
  IdentifierInfo *II = &CGM.getContext().Idents.get("NSAutoreleasePool");
  return EmitClassRefFromId(CGF, II, false, nullptr);
}

llvm::Value *
CGObjCNonFragileABIMac::EmitSuperClassRef(CodeGenFunction &CGF,
                                          const ObjCInterfaceDecl *ID) {
  CharUnits Align = CGF.getPointerAlign();
  llvm::GlobalVariable *&Entry = SuperClassReferences[ID->getIdentifier()];

  if (!Entry) {
    llvm::SmallString<64> ClassName(getClassSymbolPrefix());
    ClassName += ID->getObjCRuntimeNameAsString();
    llvm::GlobalVariable *ClassGV = GetClassGlobal(ClassName.str(),
                                                   ID->isWeakImported());
    Entry = new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ClassnfABIPtrTy,
                                     false, llvm::GlobalValue::PrivateLinkage,
                                     ClassGV, "OBJC_CLASSLIST_SUP_REFS_$_");
    Entry->setAlignment(Align.getQuantity());
    Entry->setSection("__DATA, __objc_superrefs, regular, no_dead_strip");
    CGM.addCompilerUsedGlobal(Entry);
  }
  return CGF.Builder.CreateAlignedLoad(Entry, Align);
}

/// EmitMetaClassRef - Return a Value * of the address of _class_t
/// meta-data
///
llvm::Value *CGObjCNonFragileABIMac::EmitMetaClassRef(CodeGenFunction &CGF,
                                                      const ObjCInterfaceDecl *ID,
                                                      bool Weak) {
  CharUnits Align = CGF.getPointerAlign();
  llvm::GlobalVariable * &Entry = MetaClassReferences[ID->getIdentifier()];
  if (!Entry) {
    llvm::SmallString<64> MetaClassName(getMetaclassSymbolPrefix());
    MetaClassName += ID->getObjCRuntimeNameAsString();
    llvm::GlobalVariable *MetaClassGV =
      GetClassGlobal(MetaClassName.str(), Weak);

    Entry = new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.ClassnfABIPtrTy,
                                     false, llvm::GlobalValue::PrivateLinkage,
                                     MetaClassGV, "OBJC_CLASSLIST_SUP_REFS_$_");
    Entry->setAlignment(Align.getQuantity());

    Entry->setSection("__DATA, __objc_superrefs, regular, no_dead_strip");
    CGM.addCompilerUsedGlobal(Entry);
  }

  return CGF.Builder.CreateAlignedLoad(Entry, Align);
}

/// GetClass - Return a reference to the class for the given interface
/// decl.
llvm::Value *CGObjCNonFragileABIMac::GetClass(CodeGenFunction &CGF,
                                              const ObjCInterfaceDecl *ID) {
  if (ID->isWeakImported()) {
    llvm::SmallString<64> ClassName(getClassSymbolPrefix());
    ClassName += ID->getObjCRuntimeNameAsString();
    llvm::GlobalVariable *ClassGV = GetClassGlobal(ClassName.str(), true);
    (void)ClassGV;
    assert(ClassGV->hasExternalWeakLinkage());
  }
  
  return EmitClassRef(CGF, ID);
}

/// Generates a message send where the super is the receiver.  This is
/// a message send to self with special delivery semantics indicating
/// which class's method should be called.
CodeGen::RValue
CGObjCNonFragileABIMac::GenerateMessageSendSuper(CodeGen::CodeGenFunction &CGF,
                                                 ReturnValueSlot Return,
                                                 QualType ResultType,
                                                 Selector Sel,
                                                 const ObjCInterfaceDecl *Class,
                                                 bool isCategoryImpl,
                                                 llvm::Value *Receiver,
                                                 bool IsClassMessage,
                                                 const CodeGen::CallArgList &CallArgs,
                                                 const ObjCMethodDecl *Method) {
  // ...
  // Create and init a super structure; this is a (receiver, class)
  // pair we will pass to objc_msgSendSuper.
  Address ObjCSuper =
    CGF.CreateTempAlloca(ObjCTypes.SuperTy, CGF.getPointerAlign(),
                         "objc_super");

  llvm::Value *ReceiverAsObject =
    CGF.Builder.CreateBitCast(Receiver, ObjCTypes.ObjectPtrTy);
  CGF.Builder.CreateStore(
      ReceiverAsObject,
      CGF.Builder.CreateStructGEP(ObjCSuper, 0, CharUnits::Zero()));

  // If this is a class message the metaclass is passed as the target.
  llvm::Value *Target;
  if (IsClassMessage)
      Target = EmitMetaClassRef(CGF, Class, Class->isWeakImported());
  else
    Target = EmitSuperClassRef(CGF, Class);

  // FIXME: We shouldn't need to do this cast, rectify the ASTContext and
  // ObjCTypes types.
  llvm::Type *ClassTy =
    CGM.getTypes().ConvertType(CGF.getContext().getObjCClassType());
  Target = CGF.Builder.CreateBitCast(Target, ClassTy);
  CGF.Builder.CreateStore(
      Target, CGF.Builder.CreateStructGEP(ObjCSuper, 1, CGF.getPointerSize()));

  return (isVTableDispatchedSelector(Sel))
    ? EmitVTableMessageSend(CGF, Return, ResultType, Sel,
                            ObjCSuper.getPointer(), ObjCTypes.SuperPtrCTy,
                            true, CallArgs, Method)
    : EmitMessageSend(CGF, Return, ResultType,
                      EmitSelector(CGF, Sel),
                      ObjCSuper.getPointer(), ObjCTypes.SuperPtrCTy,
                      true, CallArgs, Method, Class, ObjCTypes);
}

llvm::Value *CGObjCNonFragileABIMac::EmitSelector(CodeGenFunction &CGF,
                                                  Selector Sel) {
  Address Addr = EmitSelectorAddr(CGF, Sel);

  llvm::LoadInst* LI = CGF.Builder.CreateLoad(Addr);
  LI->setMetadata(CGM.getModule().getMDKindID("invariant.load"), 
                  llvm::MDNode::get(VMContext, None));
  return LI;
}

Address CGObjCNonFragileABIMac::EmitSelectorAddr(CodeGenFunction &CGF,
                                                 Selector Sel) {
  llvm::GlobalVariable *&Entry = SelectorReferences[Sel];

  CharUnits Align = CGF.getPointerAlign();
  if (!Entry) {
    llvm::Constant *Casted =
      llvm::ConstantExpr::getBitCast(GetMethodVarName(Sel),
                                     ObjCTypes.SelectorPtrTy);
    Entry = new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.SelectorPtrTy,
                                     false, llvm::GlobalValue::PrivateLinkage,
                                     Casted, "OBJC_SELECTOR_REFERENCES_");
    Entry->setExternallyInitialized(true);
    Entry->setSection("__DATA, __objc_selrefs, literal_pointers, no_dead_strip");
    Entry->setAlignment(Align.getQuantity());
    CGM.addCompilerUsedGlobal(Entry);
  }

  return Address(Entry, Align);
}

/// EmitObjCIvarAssign - Code gen for assigning to a __strong object.
/// objc_assign_ivar (id src, id *dst, ptrdiff_t)
///
void CGObjCNonFragileABIMac::EmitObjCIvarAssign(CodeGen::CodeGenFunction &CGF,
                                                llvm::Value *src,
                                                Address dst,
                                                llvm::Value *ivarOffset) {
  llvm::Type * SrcTy = src->getType();
  if (!isa<llvm::PointerType>(SrcTy)) {
    unsigned Size = CGM.getDataLayout().getTypeAllocSize(SrcTy);
    assert(Size <= 8 && "does not support size > 8");
    src = (Size == 4 ? CGF.Builder.CreateBitCast(src, ObjCTypes.IntTy)
           : CGF.Builder.CreateBitCast(src, ObjCTypes.LongTy));
    src = CGF.Builder.CreateIntToPtr(src, ObjCTypes.Int8PtrTy);
  }
  src = CGF.Builder.CreateBitCast(src, ObjCTypes.ObjectPtrTy);
  dst = CGF.Builder.CreateBitCast(dst, ObjCTypes.PtrObjectPtrTy);
  llvm::Value *args[] = { src, dst.getPointer(), ivarOffset };
  CGF.EmitNounwindRuntimeCall(ObjCTypes.getGcAssignIvarFn(), args);
}

/// EmitObjCStrongCastAssign - Code gen for assigning to a __strong cast object.
/// objc_assign_strongCast (id src, id *dst)
///
void CGObjCNonFragileABIMac::EmitObjCStrongCastAssign(
  CodeGen::CodeGenFunction &CGF,
  llvm::Value *src, Address dst) {
  llvm::Type * SrcTy = src->getType();
  if (!isa<llvm::PointerType>(SrcTy)) {
    unsigned Size = CGM.getDataLayout().getTypeAllocSize(SrcTy);
    assert(Size <= 8 && "does not support size > 8");
    src = (Size == 4 ? CGF.Builder.CreateBitCast(src, ObjCTypes.IntTy)
           : CGF.Builder.CreateBitCast(src, ObjCTypes.LongTy));
    src = CGF.Builder.CreateIntToPtr(src, ObjCTypes.Int8PtrTy);
  }
  src = CGF.Builder.CreateBitCast(src, ObjCTypes.ObjectPtrTy);
  dst = CGF.Builder.CreateBitCast(dst, ObjCTypes.PtrObjectPtrTy);
  llvm::Value *args[] = { src, dst.getPointer() };
  CGF.EmitNounwindRuntimeCall(ObjCTypes.getGcAssignStrongCastFn(),
                              args, "weakassign");
}

void CGObjCNonFragileABIMac::EmitGCMemmoveCollectable(
  CodeGen::CodeGenFunction &CGF,
  Address DestPtr,
  Address SrcPtr,
  llvm::Value *Size) {
  SrcPtr = CGF.Builder.CreateBitCast(SrcPtr, ObjCTypes.Int8PtrTy);
  DestPtr = CGF.Builder.CreateBitCast(DestPtr, ObjCTypes.Int8PtrTy);
  llvm::Value *args[] = { DestPtr.getPointer(), SrcPtr.getPointer(), Size };
  CGF.EmitNounwindRuntimeCall(ObjCTypes.GcMemmoveCollectableFn(), args);
}

/// EmitObjCWeakRead - Code gen for loading value of a __weak
/// object: objc_read_weak (id *src)
///
llvm::Value * CGObjCNonFragileABIMac::EmitObjCWeakRead(
  CodeGen::CodeGenFunction &CGF,
  Address AddrWeakObj) {
  llvm::Type *DestTy = AddrWeakObj.getElementType();
  AddrWeakObj = CGF.Builder.CreateBitCast(AddrWeakObj, ObjCTypes.PtrObjectPtrTy);
  llvm::Value *read_weak =
    CGF.EmitNounwindRuntimeCall(ObjCTypes.getGcReadWeakFn(),
                                AddrWeakObj.getPointer(), "weakread");
  read_weak = CGF.Builder.CreateBitCast(read_weak, DestTy);
  return read_weak;
}

/// EmitObjCWeakAssign - Code gen for assigning to a __weak object.
/// objc_assign_weak (id src, id *dst)
///
void CGObjCNonFragileABIMac::EmitObjCWeakAssign(CodeGen::CodeGenFunction &CGF,
                                                llvm::Value *src, Address dst) {
  llvm::Type * SrcTy = src->getType();
  if (!isa<llvm::PointerType>(SrcTy)) {
    unsigned Size = CGM.getDataLayout().getTypeAllocSize(SrcTy);
    assert(Size <= 8 && "does not support size > 8");
    src = (Size == 4 ? CGF.Builder.CreateBitCast(src, ObjCTypes.IntTy)
           : CGF.Builder.CreateBitCast(src, ObjCTypes.LongTy));
    src = CGF.Builder.CreateIntToPtr(src, ObjCTypes.Int8PtrTy);
  }
  src = CGF.Builder.CreateBitCast(src, ObjCTypes.ObjectPtrTy);
  dst = CGF.Builder.CreateBitCast(dst, ObjCTypes.PtrObjectPtrTy);
  llvm::Value *args[] = { src, dst.getPointer() };
  CGF.EmitNounwindRuntimeCall(ObjCTypes.getGcAssignWeakFn(),
                              args, "weakassign");
}

/// EmitObjCGlobalAssign - Code gen for assigning to a __strong object.
/// objc_assign_global (id src, id *dst)
///
void CGObjCNonFragileABIMac::EmitObjCGlobalAssign(CodeGen::CodeGenFunction &CGF,
                                          llvm::Value *src, Address dst,
                                          bool threadlocal) {
  llvm::Type * SrcTy = src->getType();
  if (!isa<llvm::PointerType>(SrcTy)) {
    unsigned Size = CGM.getDataLayout().getTypeAllocSize(SrcTy);
    assert(Size <= 8 && "does not support size > 8");
    src = (Size == 4 ? CGF.Builder.CreateBitCast(src, ObjCTypes.IntTy)
           : CGF.Builder.CreateBitCast(src, ObjCTypes.LongTy));
    src = CGF.Builder.CreateIntToPtr(src, ObjCTypes.Int8PtrTy);
  }
  src = CGF.Builder.CreateBitCast(src, ObjCTypes.ObjectPtrTy);
  dst = CGF.Builder.CreateBitCast(dst, ObjCTypes.PtrObjectPtrTy);
  llvm::Value *args[] = { src, dst.getPointer() };
  if (!threadlocal)
    CGF.EmitNounwindRuntimeCall(ObjCTypes.getGcAssignGlobalFn(),
                                args, "globalassign");
  else
    CGF.EmitNounwindRuntimeCall(ObjCTypes.getGcAssignThreadLocalFn(),
                                args, "threadlocalassign");
}

void
CGObjCNonFragileABIMac::EmitSynchronizedStmt(CodeGen::CodeGenFunction &CGF,
                                             const ObjCAtSynchronizedStmt &S) {
  EmitAtSynchronizedStmt(CGF, S,
      cast<llvm::Function>(ObjCTypes.getSyncEnterFn()),
      cast<llvm::Function>(ObjCTypes.getSyncExitFn()));
}

llvm::Constant *
CGObjCNonFragileABIMac::GetEHType(QualType T) {
  // There's a particular fixed type info for 'id'.
  if (T->isObjCIdType() || T->isObjCQualifiedIdType()) {
    auto *IDEHType = CGM.getModule().getGlobalVariable("OBJC_EHTYPE_id");
    if (!IDEHType) {
      IDEHType =
          new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.EHTypeTy, false,
                                   llvm::GlobalValue::ExternalLinkage, nullptr,
                                   "OBJC_EHTYPE_id");
      if (CGM.getTriple().isOSBinFormatCOFF())
        IDEHType->setDLLStorageClass(getStorage(CGM, "OBJC_EHTYPE_id"));
    }
    return IDEHType;
  }

  // All other types should be Objective-C interface pointer types.
  const ObjCObjectPointerType *PT = T->getAs<ObjCObjectPointerType>();
  assert(PT && "Invalid @catch type.");

  const ObjCInterfaceType *IT = PT->getInterfaceType();
  assert(IT && "Invalid @catch type.");

  return GetInterfaceEHType(IT->getDecl(), false);
}

void CGObjCNonFragileABIMac::EmitTryStmt(CodeGen::CodeGenFunction &CGF,
                                         const ObjCAtTryStmt &S) {
  EmitTryCatchStmt(CGF, S,
      cast<llvm::Function>(ObjCTypes.getObjCBeginCatchFn()),
      cast<llvm::Function>(ObjCTypes.getObjCEndCatchFn()),
      cast<llvm::Function>(ObjCTypes.getExceptionRethrowFn()));
}

/// EmitThrowStmt - Generate code for a throw statement.
void CGObjCNonFragileABIMac::EmitThrowStmt(CodeGen::CodeGenFunction &CGF,
                                           const ObjCAtThrowStmt &S,
                                           bool ClearInsertionPoint) {
  if (const Expr *ThrowExpr = S.getThrowExpr()) {
    llvm::Value *Exception = CGF.EmitObjCThrowOperand(ThrowExpr);
    Exception = CGF.Builder.CreateBitCast(Exception, ObjCTypes.ObjectPtrTy);
    CGF.EmitRuntimeCallOrInvoke(ObjCTypes.getExceptionThrowFn(), Exception)
      .setDoesNotReturn();
  } else {
    CGF.EmitRuntimeCallOrInvoke(ObjCTypes.getExceptionRethrowFn())
      .setDoesNotReturn();
  }

  CGF.Builder.CreateUnreachable();
  if (ClearInsertionPoint)
    CGF.Builder.ClearInsertionPoint();
}

llvm::Constant *
CGObjCNonFragileABIMac::GetInterfaceEHType(const ObjCInterfaceDecl *ID,
                                           bool ForDefinition) {
  llvm::GlobalVariable * &Entry = EHTypeReferences[ID->getIdentifier()];
  StringRef ClassName = ID->getObjCRuntimeNameAsString();

  // If we don't need a definition, return the entry if found or check
  // if we use an external reference.
  if (!ForDefinition) {
    if (Entry)
      return Entry;

    // If this type (or a super class) has the __objc_exception__
    // attribute, emit an external reference.
    if (hasObjCExceptionAttribute(CGM.getContext(), ID)) {
      std::string EHTypeName = ("OBJC_EHTYPE_$_" + ClassName).str();
      Entry = new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.EHTypeTy,
                                       false, llvm::GlobalValue::ExternalLinkage,
                                       nullptr, EHTypeName);
      if (CGM.getTriple().isOSBinFormatCOFF()) {
        if (ID->hasAttr<DLLExportAttr>())
          Entry->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);
        else if (ID->hasAttr<DLLImportAttr>())
          Entry->setDLLStorageClass(llvm::GlobalValue::DLLImportStorageClass);
      }
      return Entry;
    }
  }

  // Otherwise we need to either make a new entry or fill in the initializer.
  assert((!Entry || !Entry->hasInitializer()) && "Duplicate EHType definition");

  std::string VTableName = "objc_ehtype_vtable";
  auto *VTableGV = CGM.getModule().getGlobalVariable(VTableName);
  if (!VTableGV) {
    VTableGV =
        new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.Int8PtrTy, false,
                                 llvm::GlobalValue::ExternalLinkage, nullptr,
                                 VTableName);
    if (CGM.getTriple().isOSBinFormatCOFF())
      VTableGV->setDLLStorageClass(getStorage(CGM, VTableName));
  }

  llvm::Value *VTableIdx = llvm::ConstantInt::get(CGM.Int32Ty, 2);
  llvm::Constant *Values[] = {
      llvm::ConstantExpr::getGetElementPtr(VTableGV->getValueType(), VTableGV,
                                           VTableIdx),
      GetClassName(ID->getObjCRuntimeNameAsString()),
      GetClassGlobal((getClassSymbolPrefix() + ClassName).str()),
  };
  llvm::Constant *Init = llvm::ConstantStruct::get(ObjCTypes.EHTypeTy, Values);

  llvm::GlobalValue::LinkageTypes L = ForDefinition
                                          ? llvm::GlobalValue::ExternalLinkage
                                          : llvm::GlobalValue::WeakAnyLinkage;
  if (Entry) {
    Entry->setInitializer(Init);
  } else {
    Entry =
        new llvm::GlobalVariable(CGM.getModule(), ObjCTypes.EHTypeTy, false, L,
                                 Init, ("OBJC_EHTYPE_$_" + ClassName).str());
    if (CGM.getTriple().isOSBinFormatCOFF())
      if (hasObjCExceptionAttribute(CGM.getContext(), ID))
        if (ID->hasAttr<DLLExportAttr>())
          Entry->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);
  }
  assert(Entry->getLinkage() == L);

  if (!CGM.getTriple().isOSBinFormatCOFF())
    if (ID->getVisibility() == HiddenVisibility)
      Entry->setVisibility(llvm::GlobalValue::HiddenVisibility);

  const auto &DL = CGM.getDataLayout();
  Entry->setAlignment(DL.getABITypeAlignment(ObjCTypes.EHTypeTy));

  if (ForDefinition)
    Entry->setSection("__DATA,__objc_const");

  return Entry;
}

/* *** */

CodeGen::CGObjCRuntime *
CodeGen::CreateMacObjCRuntime(CodeGen::CodeGenModule &CGM) {
  switch (CGM.getLangOpts().ObjCRuntime.getKind()) {
  case ObjCRuntime::FragileMacOSX:
  return new CGObjCMac(CGM);

  case ObjCRuntime::MacOSX:
  case ObjCRuntime::iOS:
  case ObjCRuntime::WatchOS:
    return new CGObjCNonFragileABIMac(CGM);

  case ObjCRuntime::GNUstep:
  case ObjCRuntime::GCC:
  case ObjCRuntime::ObjFW:
    llvm_unreachable("these runtimes are not Mac runtimes");
  }
  llvm_unreachable("bad runtime");
}
