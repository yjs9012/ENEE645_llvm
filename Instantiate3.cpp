#define DEBUG_TYPE "Instantiate"

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h" //Step5
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h" //Step4
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include <set>
#include <vector>
#include <sstream>

using namespace llvm;

namespace
{
    struct Instantiate3 : public ModulePass
    {
        static char ID;
        Instantiate3() : ModulePass(ID) {}

        virtual bool runOnModule(Module &M) override;
        virtual bool runOnFunction(Function &F);
    };
}

char Instantiate3::ID = 0;
static RegisterPass<Instantiate3> X("Instantiate3", "Function Argument Instantiation", false, false);
/*******************************************************************************/
// Check function declaration & Do runOnFunction
bool Instantiate3::runOnModule(Module &M)
{
    bool isChanged = false;
    std::vector<Function *> functions;
    M.print(errs(), NULL);
    for (Module::iterator f = M.begin(); f != M.end(); f++)
    {
        if (!f->isDeclaration())
        {
            isChanged = runOnFunction(*f);
            // errs() << *f << '\n';
        }
    }
    M.print(errs(), NULL);
    return isChanged;
}
/*******************************************************************************/

bool Instantiate3::runOnFunction(Function &F)
{
    bool isChanged = false;
    int functionIndex = 1;
    std::vector<llvm::Instruction *> unnecessaryInst;

    errs() << "Defined Function Name: " << F.getName() << "\n";
    for (Function::iterator b = F.begin(); b != F.end(); b++)
    {
        for (BasicBlock::iterator i = b->begin(); i != b->end(); i++)
        {
            llvm::LLVMContext context;
            llvm::IntegerType *intType = llvm::Type::getInt32Ty(context);

            Instruction *Inst = i;
            if (dyn_cast<llvm::CallInst>(Inst))
            {
                CallInst *callInst = dyn_cast<CallInst>(Inst);
                Function *callee = callInst->getCalledFunction();
                ValueToValueMapTy Vmap; // for clonFunction
                // if callee is from library e.g printf, then pass iteration
                if (callee->isDeclaration())
                    continue;
                // Step3
                unsigned int numArgOperands = callInst->getNumArgOperands();
                std::vector<Value *> newArgs; // for copied function
                std::vector<Value *> conArgs; // const Value
                std::set<int> idx4ConstArgs;      // index of constArg

                Function::arg_iterator argi = callee->arg_begin();
                for (unsigned int j = 0; j < numArgOperands; j++)
                {
                    Value *V = callInst->getArgOperand(j);
                    if (isa<ConstantInt>(V))
                    {
                        idx4ConstArgs.insert(j); // put index in contArgs
                        conArgs.push_back(V);
                        Vmap[&*argi] = cast<ConstantInt>(V);
                    }
                    else
                    {
                        newArgs.push_back(V);
                    } // put pointer to parameter
                    argi ++;
                }               
                //errs() << "newArgs size: " << newArgs.size() << "\n";
                //errs() << "conArgs size: " << conArgs.size() << "\n";
                //errs() << "number of Vmap :" <<Vmap.size()<<"\n";
                // Step 4
                if (!idx4ConstArgs.empty())
                {
                    Function *copiedFunction = CloneFunction(callee, Vmap, false);

                    std::stringstream ss;
                    ss << functionIndex;
                    copiedFunction->setName(callee->getName() + ss.str());
                    functionIndex++;
                    callee->getParent()->getFunctionList().push_back(copiedFunction);

                    // Step 5(for futrue work, delete formal parameter)
                    Instruction *newInstruction = CallInst::Create(copiedFunction, newArgs, "", Inst);
                    Instruction *toErase = Inst;
                    //errs() << "newInstruction :" << *newInstruction << "\n";
                    //errs() << "originalInstru :" << *Inst << "\n";
                    toErase->replaceAllUsesWith(newInstruction);
                    unnecessaryInst.push_back(toErase);

                    //Change copiedFunction's instruction with constant value
                    for (Function::iterator bb = copiedFunction->begin(); bb != copiedFunction->end(); ++bb)
                    {
                        for(BasicBlock::iterator ii = bb->begin(); ii!=bb->end(); ++ii){
                            if(Argument *arg = dyn_cast<Argument>(&*ii)){
                                if (idx4ConstArgs.count(arg->getArgNo()))
                                {
                                    // Replace the deleted parameter with the corresponding value from conArgs
                                    ii->replaceAllUsesWith(conArgs[arg->getArgNo()]);
                                    ii->eraseFromParent();
                                }
                            }
                        }
                    isChanged = true;
                    }
                }
            }
        }
    }
    // Another Part 5
    for (int i = 0; i < unnecessaryInst.size(); i++)
            unnecessaryInst[i]->eraseFromParent();

    return isChanged;
}