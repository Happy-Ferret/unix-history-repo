//=-- ExprEngineObjC.cpp - ExprEngine support for Objective-C ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines ExprEngine's support for Objective-C expressions.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/StmtObjC.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExprEngine.h"

using namespace clang;
using namespace ento;

void ExprEngine::VisitLvalObjCIvarRefExpr(const ObjCIvarRefExpr *Ex, 
                                          ExplodedNode *Pred,
                                          ExplodedNodeSet &Dst) {
  ProgramStateRef state = Pred->getState();
  const LocationContext *LCtx = Pred->getLocationContext();
  SVal baseVal = state->getSVal(Ex->getBase(), LCtx);
  SVal location = state->getLValue(Ex->getDecl(), baseVal);
  
  ExplodedNodeSet dstIvar;
  StmtNodeBuilder Bldr(Pred, dstIvar, *currentBuilderContext);
  Bldr.generateNode(Ex, Pred, state->BindExpr(Ex, LCtx, location));
  
  // Perform the post-condition check of the ObjCIvarRefExpr and store
  // the created nodes in 'Dst'.
  getCheckerManager().runCheckersForPostStmt(Dst, dstIvar, Ex, *this);
}

void ExprEngine::VisitObjCAtSynchronizedStmt(const ObjCAtSynchronizedStmt *S,
                                             ExplodedNode *Pred,
                                             ExplodedNodeSet &Dst) {
  getCheckerManager().runCheckersForPreStmt(Dst, Pred, S, *this);
}

void ExprEngine::VisitObjCForCollectionStmt(const ObjCForCollectionStmt *S,
                                            ExplodedNode *Pred,
                                            ExplodedNodeSet &Dst) {
  
  // ObjCForCollectionStmts are processed in two places.  This method
  // handles the case where an ObjCForCollectionStmt* occurs as one of the
  // statements within a basic block.  This transfer function does two things:
  //
  //  (1) binds the next container value to 'element'.  This creates a new
  //      node in the ExplodedGraph.
  //
  //  (2) binds the value 0/1 to the ObjCForCollectionStmt* itself, indicating
  //      whether or not the container has any more elements.  This value
  //      will be tested in ProcessBranch.  We need to explicitly bind
  //      this value because a container can contain nil elements.
  //
  // FIXME: Eventually this logic should actually do dispatches to
  //   'countByEnumeratingWithState:objects:count:' (NSFastEnumeration).
  //   This will require simulating a temporary NSFastEnumerationState, either
  //   through an SVal or through the use of MemRegions.  This value can
  //   be affixed to the ObjCForCollectionStmt* instead of 0/1; when the loop
  //   terminates we reclaim the temporary (it goes out of scope) and we
  //   we can test if the SVal is 0 or if the MemRegion is null (depending
  //   on what approach we take).
  //
  //  For now: simulate (1) by assigning either a symbol or nil if the
  //    container is empty.  Thus this transfer function will by default
  //    result in state splitting.

  const Stmt *elem = S->getElement();
  ProgramStateRef state = Pred->getState();
  SVal elementV;
  
  if (const DeclStmt *DS = dyn_cast<DeclStmt>(elem)) {
    const VarDecl *elemD = cast<VarDecl>(DS->getSingleDecl());
    assert(elemD->getInit() == 0);
    elementV = state->getLValue(elemD, Pred->getLocationContext());
  }
  else {
    elementV = state->getSVal(elem, Pred->getLocationContext());
  }
  
  ExplodedNodeSet dstLocation;
  evalLocation(dstLocation, S, elem, Pred, state, elementV, NULL, false);

  ExplodedNodeSet Tmp;
  StmtNodeBuilder Bldr(Pred, Tmp, *currentBuilderContext);

  for (ExplodedNodeSet::iterator NI = dstLocation.begin(),
       NE = dstLocation.end(); NI!=NE; ++NI) {
    Pred = *NI;
    ProgramStateRef state = Pred->getState();
    const LocationContext *LCtx = Pred->getLocationContext();
    
    // Handle the case where the container still has elements.
    SVal TrueV = svalBuilder.makeTruthVal(1);
    ProgramStateRef hasElems = state->BindExpr(S, LCtx, TrueV);
    
    // Handle the case where the container has no elements.
    SVal FalseV = svalBuilder.makeTruthVal(0);
    ProgramStateRef noElems = state->BindExpr(S, LCtx, FalseV);
    
    if (loc::MemRegionVal *MV = dyn_cast<loc::MemRegionVal>(&elementV))
      if (const TypedValueRegion *R = 
          dyn_cast<TypedValueRegion>(MV->getRegion())) {
        // FIXME: The proper thing to do is to really iterate over the
        //  container.  We will do this with dispatch logic to the store.
        //  For now, just 'conjure' up a symbolic value.
        QualType T = R->getValueType();
        assert(Loc::isLocType(T));
        unsigned Count = currentBuilderContext->getCurrentBlockCount();
        SymbolRef Sym = SymMgr.getConjuredSymbol(elem, LCtx, T, Count);
        SVal V = svalBuilder.makeLoc(Sym);
        hasElems = hasElems->bindLoc(elementV, V);
        
        // Bind the location to 'nil' on the false branch.
        SVal nilV = svalBuilder.makeIntVal(0, T);
        noElems = noElems->bindLoc(elementV, nilV);
      }
    
    // Create the new nodes.
    Bldr.generateNode(S, Pred, hasElems);
    Bldr.generateNode(S, Pred, noElems);
  }

  // Finally, run any custom checkers.
  // FIXME: Eventually all pre- and post-checks should live in VisitStmt.
  getCheckerManager().runCheckersForPostStmt(Dst, Tmp, S, *this);
}

static bool isSubclass(const ObjCInterfaceDecl *Class, IdentifierInfo *II) {
  if (!Class)
    return false;
  if (Class->getIdentifier() == II)
    return true;
  return isSubclass(Class->getSuperClass(), II);
}

void ExprEngine::VisitObjCMessage(const ObjCMessageExpr *ME,
                                  ExplodedNode *Pred,
                                  ExplodedNodeSet &Dst) {
  CallEventManager &CEMgr = getStateManager().getCallEventManager();
  CallEventRef<ObjCMethodCall> Msg =
    CEMgr.getObjCMethodCall(ME, Pred->getState(), Pred->getLocationContext());

  // Handle the previsits checks.
  ExplodedNodeSet dstPrevisit;
  getCheckerManager().runCheckersForPreObjCMessage(dstPrevisit, Pred,
                                                   *Msg, *this);
  ExplodedNodeSet dstGenericPrevisit;
  getCheckerManager().runCheckersForPreCall(dstGenericPrevisit, dstPrevisit,
                                            *Msg, *this);

  // Proceed with evaluate the message expression.
  ExplodedNodeSet dstEval;
  StmtNodeBuilder Bldr(dstGenericPrevisit, dstEval, *currentBuilderContext);

  for (ExplodedNodeSet::iterator DI = dstGenericPrevisit.begin(),
       DE = dstGenericPrevisit.end(); DI != DE; ++DI) {
    ExplodedNode *Pred = *DI;
    ProgramStateRef State = Pred->getState();
    CallEventRef<ObjCMethodCall> UpdatedMsg = Msg.cloneWithState(State);
    
    if (UpdatedMsg->isInstanceMessage()) {
      SVal recVal = UpdatedMsg->getReceiverSVal();
      if (!recVal.isUndef()) {
        // Bifurcate the state into nil and non-nil ones.
        DefinedOrUnknownSVal receiverVal = cast<DefinedOrUnknownSVal>(recVal);
        
        ProgramStateRef notNilState, nilState;
        llvm::tie(notNilState, nilState) = State->assume(receiverVal);
        
        // There are three cases: can be nil or non-nil, must be nil, must be
        // non-nil. We ignore must be nil, and merge the rest two into non-nil.
        // FIXME: This ignores many potential bugs (<rdar://problem/11733396>).
        // Revisit once we have lazier constraints.
        if (nilState && !notNilState) {
          continue;
        }
        
        // Check if the "raise" message was sent.
        assert(notNilState);
        if (Msg->getSelector() == RaiseSel) {
          // If we raise an exception, for now treat it as a sink.
          // Eventually we will want to handle exceptions properly.
          Bldr.generateNode(currentStmt, Pred, State, true);
          continue;
        }
        
        // Generate a transition to non-Nil state.
        if (notNilState != State)
          Pred = Bldr.generateNode(currentStmt, Pred, notNilState);
      }
    } else {
      // Check for special class methods.
      if (const ObjCInterfaceDecl *Iface = Msg->getReceiverInterface()) {
        if (!NSExceptionII) {
          ASTContext &Ctx = getContext();
          NSExceptionII = &Ctx.Idents.get("NSException");
        }
        
        if (isSubclass(Iface, NSExceptionII)) {
          enum { NUM_RAISE_SELECTORS = 2 };
          
          // Lazily create a cache of the selectors.
          if (!NSExceptionInstanceRaiseSelectors) {
            ASTContext &Ctx = getContext();
            NSExceptionInstanceRaiseSelectors =
              new Selector[NUM_RAISE_SELECTORS];
            SmallVector<IdentifierInfo*, NUM_RAISE_SELECTORS> II;
            unsigned idx = 0;
            
            // raise:format:
            II.push_back(&Ctx.Idents.get("raise"));
            II.push_back(&Ctx.Idents.get("format"));
            NSExceptionInstanceRaiseSelectors[idx++] =
              Ctx.Selectors.getSelector(II.size(), &II[0]);
            
            // raise:format:arguments:
            II.push_back(&Ctx.Idents.get("arguments"));
            NSExceptionInstanceRaiseSelectors[idx++] =
              Ctx.Selectors.getSelector(II.size(), &II[0]);
          }
          
          Selector S = Msg->getSelector();
          bool RaisesException = false;
          for (unsigned i = 0; i < NUM_RAISE_SELECTORS; ++i) {
            if (S == NSExceptionInstanceRaiseSelectors[i]) {
              RaisesException = true;
              break;
            }
          }
          if (RaisesException) {
            // If we raise an exception, for now treat it as a sink.
            // Eventually we will want to handle exceptions properly.
            Bldr.generateNode(currentStmt, Pred, Pred->getState(), true);
            continue;
          }

        }
      }
    }

    // Evaluate the call.
    defaultEvalCall(Bldr, Pred, *UpdatedMsg);
  }
  
  ExplodedNodeSet dstPostvisit;
  getCheckerManager().runCheckersForPostCall(dstPostvisit, dstEval,
                                             *Msg, *this);

  // Finally, perform the post-condition check of the ObjCMessageExpr and store
  // the created nodes in 'Dst'.
  getCheckerManager().runCheckersForPostObjCMessage(Dst, dstPostvisit,
                                                    *Msg, *this);
}
