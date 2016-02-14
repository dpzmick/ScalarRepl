//===- ScalarReplAggregates.cpp - Scalar Replacement of Aggregates --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This transformation implements the well known scalar replacement of
// aggregates transformation.  This xform breaks up alloca instructions of
// structure type into individual alloca instructions for
// each member (if possible).  Then, if possible, it transforms the individual
// alloca instructions into nice clean scalar SSA form.
//
// This combines a simple SRoA algorithm with Mem2Reg because they
// often interact, especially for C++ programs.  As such, this code
// iterates between SRoA and Mem2Reg until we run out of things to promote.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "scalarrepl"

#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>
#include <algorithm>
#include <iterator>
using namespace llvm;

STATISTIC(NumReplaced,  "Number of aggregate allocas broken up");
STATISTIC(NumPromoted,  "Number of scalar allocas promoted to register");
STATISTIC(NumOuterIterations, "Number of iterations of the outer mem2reg/promote loop");

/// this pass handles the following cases
// - structs with scalar elements
// - structs with mixed scalar/array elements
// - structs used in function calls (from a different unit) are not promoted
// - structs used in function calls in the same unit are sometimes promoted
//   (clang seems to expand the function call to take n args, for struct of n
//   fields)
// - when a struct field is unused and the struct can be promoted, the unused
//   fields can easily be cleaned up
namespace {
  struct SROA : public FunctionPass {
    static char ID; // Pass identification
    SROA() : FunctionPass(ID) { }

    // Entry point for the overall scalar-replacement pass
    bool runOnFunction(Function &F);

    // getAnalysisUsage - List passes required by this pass.  We also know it
    // will not alter the CFG, so say so.
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<DominatorTreeWrapperPass>();
      AU.setPreservesCFG();
    }

  private:

    /// checks if the argument argument could be promoted to a register
    bool allocaPromoteable(const AllocaInst &I);

    /// calls SROA::allocaPromotable, but also checks that our result agrees
    /// with the llvm result
    bool checkedPromoteable(const AllocaInst &I);

    /// all allocas which can be promoted
    std::vector<AllocaInst*> findPromotableAllocas(Function &F);

    /// all allocas in function
    std::vector<AllocaInst*> findAllocas(Function &F);

    bool splitAllocas(Function&);
    std::vector<AllocaInst*> removeAlloca(Function&, AllocaInst&);
    void modifyGEP(GetElementPtrInst *, std::vector<AllocaInst*>);
    void modifyCMP(CmpInst *);

    // tests
    bool U1(User *I);
    bool U2(User *I);
  };
}

char SROA::ID = 0;
static RegisterPass<SROA> X("scalarrepl-zmick2", "Scalar Replacement of Aggregates (by zmick2)");

// borrowed from
// http://stackoverflow.com/questions/11186052/segfault-when-using-getelementptrinst-with-an-arrayref-containing-more-than-one
static llvm::LLVMContext& global = llvm::getGlobalContext();
static llvm::Type* int1ty = llvm::Type::getInt1Ty(global);
static llvm::Type* int64ty = llvm::Type::getInt64Ty(global);

llvm::ConstantInt* getInt1(int n)
{
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(global), n);
}

llvm::ConstantInt* getInt64(int n)
{
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(global), n);
}

bool SROA::allocaPromoteable(const AllocaInst &I) {
  DEBUG(errs() << "allocaPromoteable: " << I << "\n");
  auto allocatedType = I.getAllocatedType();

  bool firstClass =
    allocatedType->isFPOrFPVectorTy()
    || allocatedType->isIntOrIntVectorTy()
    || allocatedType->isPtrOrPtrVectorTy();

  DEBUG(errs() << "first class? = " << firstClass
      << " allocated type " << *allocatedType << "\n");

  if (!firstClass) return false;

  for(auto use : I.users()) {
    DEBUG(errs() << " investigating use: " << *use << "\n");
    if (auto load = dyn_cast<LoadInst>(use)) {
      // we got a load
      if (load->isVolatile()) {
        DEBUG(errs() << " not promotable, load is volatile\n");
        return false;
      }

    } else if (auto store = dyn_cast<StoreInst>(use)) {
      // we got a store
      if (store->isVolatile()) {
        DEBUG(errs() << " not promotable, store is volatile\n");
        return false;
      }

    } else {
      // we got neither
      return false;
    }

    // fall through for true return
  }

  return true;
}

bool SROA::checkedPromoteable(const AllocaInst &I) {
  bool ret = allocaPromoteable(I);
  if (ret) {
    assert(llvm::isAllocaPromotable(&I));
    NumPromoted++;
  }

  return ret;
}

std::vector<AllocaInst*> SROA::findAllocas(Function &F) {
  std::vector<AllocaInst*> ret;

  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; I++) {
    if (auto AI = dyn_cast<AllocaInst>(&*I)) {
      ret.push_back(AI);
    }
  }

  return ret;
}

std::vector<AllocaInst*> SROA::findPromotableAllocas(Function &F) {
  std::vector<AllocaInst*> allocas = findAllocas(F);

  std::vector<AllocaInst*> ret;
  for (auto i : allocas) {
    if (checkedPromoteable(*i)) {
      DEBUG(errs() << "marking alloca as promotable: " << *i << "\n");
      ret.push_back(i);
    }
  }

  return ret;
}

void SROA::modifyGEP(GetElementPtrInst *gep, std::vector<AllocaInst*> new_fields) {
  DEBUG(errs() << "modifying gep: " << *gep << "\n");
  std::vector<Value*> ids(gep->op_begin() + 1, gep->op_end());

  // get the alloca that allocated the field we are accessing
  auto i = dyn_cast<ConstantInt>(ids[1])->getSExtValue();
  auto alloca_to_use = new_fields[i];

  if (ids.size() == 2) {
    // the GEP is just a "dereference," we need to remove it totally
    BasicBlock::iterator ii(gep);
    ReplaceInstWithValue(gep->getParent()->getInstList(), ii, alloca_to_use);
  } else {
    // we need a new GEP with two less derefs
    // the first in the original gets the struct, the second gets the field
    // the remaining derefs are what we need to preserve
    std::vector<Value*> newOffsets(ids.begin() + 2, ids.end());
    // push a zero on the front to deref the pointer, then do whatever the
    // original instruction did
    newOffsets.insert(newOffsets.begin(), getInt64(0));
    auto newGEP = GetElementPtrInst::CreateInBounds(
        alloca_to_use,
        newOffsets,
        "newGEP"
        );

    DEBUG(errs() << "newGEP: " << *newGEP << "\n");

    ReplaceInstWithInst(gep, newGEP);
  }
}

void SROA::modifyCMP(CmpInst *cmp) {
  BasicBlock::iterator ii(cmp);
  if (cmp->isTrueWhenEqual()) {
    // is an eq instruction
    // always false
    ReplaceInstWithValue(cmp->getParent()->getInstList(), ii, getInt1(0));
  } else {
    // is an new
    // always true
    ReplaceInstWithValue(cmp->getParent()->getInstList(), ii, getInt1(1));
  }
}

// assumes that we are getting an alloca for a struct type
std::vector<AllocaInst*> SROA::removeAlloca(Function &F, AllocaInst &AI) {
  // add a new alloca for every field
  // replace all the uses
  auto ty = dyn_cast<StructType>(AI.getAllocatedType());

  // introduce all the new allocas
  std::vector<AllocaInst*> each_field_inst;
  for (auto e : ty->elements()) {
    auto newAI = new AllocaInst(e, 0, AI.getName() + "_field", &AI);
    each_field_inst.push_back(newAI);
  }

  // modify all the uses
  // consider doing some template magic to clean this up
  for (auto u = AI.user_begin(), e = AI.user_end(); u != e;) {
    auto tmp = u;
    u++;

    // since we got here, we know the instructions using the thing defined by
    // the alloca must be either a GEP or a cmp
    if (auto gep = dyn_cast<GetElementPtrInst>(*tmp)) {
      modifyGEP(gep, each_field_inst);
    } else if (auto cmp = dyn_cast<CmpInst>(*tmp)) {
      modifyCMP(cmp);
    } else {
      llvm_unreachable("if we get here, something is wrong with U1 and U2");
    }

  }

  AI.eraseFromParent();

  return each_field_inst;
}

bool SROA::splitAllocas(Function &F) {
  bool changed = false;

  // for every alloca removed, add the allocas it introduces to the worklist.
  // this reduces the number of outer iterations when there are deep nests
  std::vector<AllocaInst*> allocas = findAllocas(F);

  while (!allocas.empty()) {
    // get the top element
    auto ai = allocas.back();
    allocas.pop_back();

    bool canEliminate = isa<StructType>(ai->getAllocatedType());
    for (auto u : ai->users()) {
      canEliminate &= U1(u) || U2(u);
    }

    DEBUG(errs() << "canEliminate " << *ai << " = " << canEliminate << "\n");
    if (canEliminate) {
      NumReplaced++;
      changed = true;

      auto newAllocas = removeAlloca(F, *ai);
      allocas.reserve(allocas.size() + newAllocas.size());
      std::move(newAllocas.begin(), newAllocas.end(), std::inserter(allocas, allocas.end()));
      newAllocas.clear();
    }
  }

  return changed;
}

bool SROA::runOnFunction(Function &F) {
  // as long as stuff changes, keep applying steps 1 and 2
  bool Changed = true;
  while (Changed) {
    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();

    // run every loop, F is changing
    PromoteMemToReg(findPromotableAllocas(F), DT);
    Changed = splitAllocas(F);
    NumOuterIterations++;
  }
  return Changed;

}

bool SROA::U1(User *I) {
  DEBUG(errs() << "U1 for instruction: " << *I << "\n");
  if (GetElementPtrInst *gp = dyn_cast<GetElementPtrInst>(I)) {
    for (auto u : gp->users()) {
      DEBUG(errs() << "looping on" << *u << "\n");
      bool either = U1(u) || U2(u);

      // for load and store, check that we are the pointer argument
      // (not the thing being stored too)
      bool or_ = false;
      if (LoadInst * li = dyn_cast<LoadInst>(u)) {
        DEBUG(errs() << "load from " << *(li->getPointerOperand()) << "\n");
        or_ = true;
      }

      if (StoreInst * si = dyn_cast<StoreInst>(u)) {
        or_ = I != si->getValueOperand();
      }

      if (!(either || or_))
        return false;
    }
    return gp->hasAllConstantIndices();
  }

  return false;
}

bool SROA::U2(User *I) {
  DEBUG(errs() << "U2 for instruction: " << *I << "\n");
  if (CmpInst * cmp = dyn_cast<CmpInst>(I)) {

    // is one null?
    bool oneNull = false;
    for (auto u = cmp->op_begin(), e = cmp->op_end(); u != e; u++) {
      oneNull |= isa<ConstantPointerNull>(*u);
    }

    return cmp->isEquality() && oneNull;
  }
  return false;
}

#undef DEBUG_TYPE
