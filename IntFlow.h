#ifndef INFOAPP_H_
#define INFOAPP_H_

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Scalar.h"

//#include "Infoflow.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#define WHITE_LIST "/tmp/whitelist.files"
#define MODE_FILE "/tmp/mode"
#define WHITELISTING 1
#define BLACKLISTING 2
#define SENSITIVE 3
#define BLACK_SENSITIVE 4
#define MODE_MAX_NUM 4

using namespace llvm;

namespace {

//
// Structure which records which arguments should be marked tainted.
//
typedef struct CallTaintSummary {
  static const unsigned NumArguments = 10;
  bool TaintsReturnValue;
  bool TaintsArgument[NumArguments];
  bool TaintsVarargArguments;
} CallTaintSummary;

//
// Holds an entry for a single function, summarizing which arguments are
// tainted values and which point to tainted memory.
//
typedef struct CallTaintEntry {
  const char *Name;
  CallTaintSummary ValueSummary;
  CallTaintSummary DirectPointerSummary;
  CallTaintSummary RootPointerSummary;
} CallTaintEntry;

#define TAINTS_NOTHING                                                         \
  { false, {}, false }
#define TAINTS_ALL_ARGS                                                        \
  { false, {true, true, true, true, true, true, true, true, true, true}, true }
#define TAINTS_RETURN_VAL                                                      \
  { true, {}, false }
#define TAINTS_ARG_1                                                           \
  { false, {true}, false }
#define TAINTS_ARG_2                                                           \
  { false, {false, true}, false }

static const struct CallTaintEntry bLstSourceSummaries[] = {
    // function, tainted values, tainted direct memory, tainted root ptrs
    {"fgets", TAINTS_RETURN_VAL, TAINTS_ARG_1, TAINTS_NOTHING},
    {"getchar", TAINTS_RETURN_VAL, TAINTS_NOTHING, TAINTS_NOTHING},
    {"_IO_getc", TAINTS_RETURN_VAL, TAINTS_NOTHING, TAINTS_NOTHING},
    {"__isoc99_scanf", TAINTS_NOTHING, TAINTS_ARG_2, TAINTS_NOTHING},
    {0, TAINTS_NOTHING, TAINTS_NOTHING, TAINTS_NOTHING}};

static const struct CallTaintEntry wLstSourceSummaries[] = {
    // function, tainted values, tainted direct memory, tainted root ptrs
    {"gettimeofday", TAINTS_RETURN_VAL, TAINTS_ARG_1, TAINTS_NOTHING},
    {0, TAINTS_NOTHING, TAINTS_NOTHING, TAINTS_NOTHING}};

static const struct CallTaintEntry sensSinkSummaries[] = {
    // function, tainted values, tainted direct memory, tainted root ptrs
    {"malloc", TAINTS_ARG_1, TAINTS_NOTHING, TAINTS_NOTHING},
    {"calloc", TAINTS_ALL_ARGS, TAINTS_NOTHING, TAINTS_NOTHING},
    {"realloc", TAINTS_ALL_ARGS, TAINTS_NOTHING, TAINTS_NOTHING},
    {"mmap", TAINTS_ALL_ARGS, TAINTS_NOTHING, TAINTS_NOTHING},
    {"memcpy", TAINTS_ALL_ARGS, TAINTS_NOTHING, TAINTS_NOTHING},
    {"memset", TAINTS_ALL_ARGS, TAINTS_NOTHING, TAINTS_NOTHING},
    {0, TAINTS_NOTHING, TAINTS_NOTHING, TAINTS_NOTHING}};

CallTaintEntry nothing = {0, TAINTS_NOTHING, TAINTS_NOTHING, TAINTS_NOTHING};

static void getWhiteList() {
  std::string line, file, function, conv;
  std::string overflow, shift;
  bool conv_bool, overflow_bool, shift_bool;
  unsigned numLines;
  unsigned i;
  unsigned pos = 0;
  std::ifstream whitelistFile;
  whitelistFile.open(WHITE_LIST);
  // get number of lines
  numLines = 0;
  while (whitelistFile.good()) {
    std::getline(whitelistFile, line);
    if (!line.empty())
      numLines++;
  }

  whitelistFile.clear();
  whitelistFile.seekg(0, std::ios::beg);

  rmCheckList = new rmChecks[numLines];
  for (i = 0; i < numLines; i++) {
    getline(whitelistFile, line);
    // handle each line
    pos = 0;
    function = line.substr(pos, line.find(","));
    pos = line.find(",") + 1;
    file = line.substr(pos, line.find(",", pos) - pos);
    pos = line.find(",", pos) + 1;
    conv = line.substr(pos, line.find(",", pos) - pos);
    pos = line.find(",", pos) + 1;
    overflow = line.substr(pos, line.find(",", pos) - pos);
    pos = line.find(",", pos) + 1;
    shift = line.substr(pos, line.size() - pos);

    conv_bool = (conv.compare("true") == 0);
    overflow_bool = (overflow.compare("true") == 0);
    shift_bool = (shift.compare("true") == 0);

    if (function.compare("0") == 0)
      rmCheckList[i].func = (char *)0;
    else {
      rmCheckList[i].func = new char[strlen(function.c_str()) + 1];
      for (unsigned j = 0; j < strlen(function.c_str()); j++)
        rmCheckList[i].func[j] = function[j];
      rmCheckList[i].func[strlen(function.c_str())] = '\0';
    }
    if (file.compare("0") == 0)
      rmCheckList[i].fname = (char *)0;
    else {
      rmCheckList[i].fname = new char[strlen(file.c_str()) + 1];
      for (unsigned j = 0; j < strlen(file.c_str()); j++)
        rmCheckList[i].fname[j] = file[j];
      rmCheckList[i].fname[strlen(file.c_str())] = '\0';
    }
    rmCheckList[i].conversion = conv_bool;
    rmCheckList[i].overflow = overflow_bool;
    rmCheckList[i].shift = shift_bool;
  }
  whitelistFile.close();
}

typedef struct {
  char *func;
  char *fname;
  bool conversion;
  bool overflow;
  bool shift;
} rmChecks;

static rmChecks *rmCheckList;

class IntflowPass : public ModulePass {
public:
  IntflowPass() : ModulePass(ID) {}
  static char ID;
  bool runOnModule(Module &M) {
    doInitializationAndRun(M);
    return false;
  }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<Infoflow>();
    AU.setPreservesAll();
  }

private:
  Infoflow *infoflow;
  uint64_t unique_id;
  std::string *iocIdName;
  DenseMap<const Value *, bool> xformMap;
  std::set<StringRef> whiteSet;
  std::set<StringRef> blackSet;
  unsigned char mode;

  virtual void doInitializationAndRun(Module &M) {
    unique_id = 0;
    infoflow = &getAnalysis<Infoflow>();
    getWhiteList();
    getMode();

    if (mode == WHITELISTING) {
      runOnModuleWhitelisting(M);
    } else if (mode == BLACKLISTING) {
      runOnModuleBlacklisting(M);
    } else if (mode == SENSITIVE) {
      runOnModuleSensitive(M);
    } else
      exit(mode);

    doFinalization();
  }

  virtual void doFinalization() {
    DenseMap<const Value *, bool>::const_iterator xi = xformMap.begin();
    DenseMap<const Value *, bool>::const_iterator xe = xformMap.end();

    for (; xi != xe; xi++) {
      std::string output;
      raw_string_ostream rs(output);
      if (xi->second) {
        format_ioc_report_func(xi->first, rs);
      }
    }

    for (unsigned i = 0; rmCheckList[i].func; i++) {
      delete rmCheckList[i].func;
      delete rmCheckList[i].fname;
    }

    delete rmCheckList;
  }

  void runOnModuleWhitelisting(Module &M) {
    for (Module::iterator mi = M.begin(); mi != M.end(); mi++) {
      Function &F = *mi;
      removeChecksForFunction(F, M);

      for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock &B = *bi;
        for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {
          if (CallInst *ci = dyn_cast<CallInst>(ii)) {

            Function *func = ci->getCalledFunction();
            if (!func)
              continue;

            if (ioc_report_all_but_conv(func->getName())) {
              std::stringstream ss;
              ss << ci->getNumOperands();
              std::string sinkKind = getKindId("arithm", &unique_id);
              InfoflowSolution *soln = getBackSolArithm(sinkKind, ci);

              std::set<const Value *> vMap;
              soln->getValueMap(vMap);

              if (isConstAssign(vMap)) {
                xformMap[ci] = true;
              } else {
                xformMap[ci] = trackSoln(M, soln, ci, sinkKind);
              }

              if (xformMap[ci]) {
                setWrapper(ci, M, func);
              }
            } else if (func->getName() == "__ioc_report_conversion") {
              std::string sinkKind = getKindId("conv", &unique_id);
              InfoflowSolution *soln = getBackSolConv(sinkKind, ci);

              std::set<const Value *> vMap;
              soln->getValueMap(vMap);

              if (isConstAssign(vMap)) {
                xformMap[ci] = true;
              } else {
                xformMap[ci] = trackSoln(M, soln, ci, sinkKind);
              }

              if (xformMap[ci]) {
                setWrapper(ci, M, func);
              }
            } else if ((func->getName() == "div") ||
                       (func->getName() == "ldiv") ||
                       (func->getName() == "lldiv") ||
                       (func->getName() == "iconv")) {
              setWrapper(ci, M, func);
            }
          }
        }
      }
    }
  }
  void runOnModuleBlacklisting(Module &M) {
    Function *func;
    InfoflowSolution *fsoln;

    for (Module::iterator mi = M.begin(); mi != M.end(); mi++) {
      Function &F = *mi;
      // this is for whitelisting
      removeChecksForFunction(F, M);
      for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock &B = *bi;
        for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {

          if (CallInst *ci = dyn_cast<CallInst>(ii)) {
            func = ci->getCalledFunction();
            if (!func)
              continue;
            const CallTaintEntry *entry =
                findEntryForFunction(bLstSourceSummaries, func->getName());
            if (entry->Name) {
              std::string srcKind = getKindId("src", &unique_id);
              fsoln = getForwardSolFromEntry(srcKind, ci, entry);

              backwardSlicingBlacklisting(M, fsoln, ci);
            } else if ((func->getName() == "div") ||
                       (func->getName() == "ldiv") ||
                       (func->getName() == "lldiv") ||
                       (func->getName() == "iconv")) {
              setWrapper(ci, M, func);
            }
          }
        }
      }
    }
    removeBenignChecks(M);
  }

  void runOnModuleSensitive(Module &M) {
    populateMapsSensitive(M);
    createArraysAndSensChecks(M);
    insertIOCChecks(M);

    Function *func;
    for (Module::iterator mi = M.begin(); mi != M.end(); mi++) {
      Function &F = *mi;
      removeChecksForFunction(F, M);
      for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock &B = *bi;
        for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {
          if (CallInst *ci = dyn_cast<CallInst>(ii)) {
            func = ci->getCalledFunction();
            if (!func)
              continue;

            xformMap[ci] = false;
          }
        }
      }
    }

    removeBenignChecks(M);
  }

  void populateMapsSensitive(Module &M) {
    Function *func;
    BasicBlock *bb;
    for (Module::iterator mi = M.begin(); mi != M.end(); mi++) {
      Function &F = *mi;
      for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock &B = *bi;
        for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {

          if (CallInst *ci = dyn_cast<CallInst>(ii)) {

            func = ci->getCalledFunction();
            if (!func)
              continue;

            std::string iocKind = "";

            if (StringRef(func->getName()).startswith("__ioc")) {
              iocKind = getKindCall(M, F, ci);
              sensPoints[iocKind] = std::vector<std::string>();
            }

            if (ioc_report_arithm(func->getName())) {
              bb = ci->getParent()->getSinglePredecessor();
              if (bb == nullptr) {
                continue;
              }

              BasicBlock &BP = *bb;
              for (BasicBlock::iterator pii = BP.begin(); pii != BP.end();
                   pii++) {
                if (CallInst *cinst = dyn_cast<CallInst>(pii)) {
                  if (cinst->getCalledFunction()) {
                    std::string cfname = cinst->getCalledFunction()->getName();
                    if (llvm_arithm(cfname)) {
                      searchSensFromArithm(F, M, iocKind, cinst);
                      break;
                    }
                  }
                }
              }

            } else if (ioc_report_shl(func->getName())) {
              bi++;
              if (func->getName() == "__ioc_report_shl_strict") {
                // pass first instruction of the next bb
                searchSensFromInst(F, M, iocKind, bi->front());
              }

              if (func->getName() == "__ioc_report_shr_bitwidth") {
                BasicBlock &SB = *bi;
                for (BasicBlock::iterator sii = SB.begin(); sii != SB.end();
                     sii++) {
                  sii++;
                  searchSensFromInst(F, M, iocKind, *sii);
                  break;
                }

                bi--;
                continue;
              }
            } else if (func->getName() == "__ioc_report_conversion") {
              ;
            } else if (func->getName() == "__ioc_report_div_error") {
              bi++;
              searchSensFromInst(F, M, iocKind, bi->front());
              bi--;
              continue;
            } else {
              ;
            }
          }
        }
      }
    }
  }

  void createArraysAndSensChecks(Module &M) {
    Function *func;
    for (Module::iterator mi = M.begin(); mi != M.end(); mi++) {
      Function &F = *mi;
      for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock &B = *bi;
        for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {
          if (CallInst *ci = dyn_cast<CallInst>(ii)) {
            func = ci->getCalledFunction();
            if (!func)
              continue;

            const CallTaintEntry *entry =
                findEntryForFunction(sensSinkSummaries, func->getName());
            if (entry->Name) {
              std::string sinkKind = getKindCall(M, F, ci);
              uint64_t totalIOC = iocPoints[sinkKind].size();
              if (totalIOC > 0) {
                GlobalVariable *glA = createGlobalArray(M, totalIOC, sinkKind);
                Instruction *sti = bi->getFirstNonPHI();
                insertIntFlowFunction(M, "checkIOC", sti, ii, glA, totalIOC);
              }
            }
          }
        }
      }
    }
  }

  void insertIOCChecks(Module &M) {
    GlobalVariable *glA = NULL;
    Function *func;
    BasicBlock *bb;

    uint64_t glA_pos = 0;

    for (Module::iterator mi = M.begin(); mi != M.end(); mi++) {
      Function &F = *mi;
      for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock &B = *bi;
        for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {

          if (CallInst *ci = dyn_cast<CallInst>(ii)) {

            func = ci->getCalledFunction();
            if (!func)
              continue;

            std::string fname = func->getName();
            std::string iocKind = "";

            if (StringRef(fname).startswith("__ioc")) {
              iocKind = getKindCall(M, F, ci);

              if (sensPoints[iocKind].empty())
                continue;

              sensPointVector spv = sensPoints[iocKind];

              for (sensPointVector::const_iterator svi = spv.begin();
                   svi != spv.end(); ++svi) {

                std::string sink = *svi;
                iocPointVector ipv = iocPoints[sink];
                glA_pos = find(ipv.begin(), ipv.end(), iocKind) - ipv.begin();
                glA = getGlobalArray(M, sink);
                Instruction *sti = bi->getFirstNonPHI();
                insertIntFlowFunction(M, "setTrueIOC", sti, ii, glA, glA_pos);

                if (ioc_report_arithm(fname) || ioc_report_shl(fname) ||
                    (fname == "__ioc_report_conversion") ||
                    (fname == "__ioc_report_div_error")) {

                  bb = ci->getParent()->getSinglePredecessor();
                  if (bb == nullptr) {
                    continue;
                  }

                  BasicBlock &BP = *bb;
                  Instruction *pinst = BP.getFirstNonPHI();

                  insertIntFlowFunction(M, "setFalseIOC", pinst, ii, glA,
                                        glA_pos);
                }
              }
            }
          }
        }
      }
    }
  }

  void insertIntFlowFunction(Module &M, std::string name, Instruction *ci,
                             BasicBlock::iterator ii, GlobalVariable *tmpgl,
                             uint64_t idx) {
    Instruction *iocCheck;

    std::vector<Value *> ptr_arrayidx_indices;
    ConstantInt *c_int32 =
        ConstantInt::get(M.getContext(), APInt(64, StringRef("0"), 10));
    Value *array_idx = ConstantInt::get(Type::getInt32Ty(M.getContext()), 0);
    ptr_arrayidx_indices.push_back(c_int32);
    ptr_arrayidx_indices.push_back(array_idx);

    GetElementPtrInst *ptr_arrayidx_m =
        GetElementPtrInst::Create(tmpgl, ptr_arrayidx_indices, "get_ptr", ii);

    PointerType *arrayPtr =
        PointerType::get(IntegerType::get(M.getContext(), 32), 0);

    Constant *fc =
        M.getOrInsertFunction(name, Type::getInt32Ty(M.getContext()), arrayPtr,
                              Type::getInt32Ty(M.getContext()),
                              GlobalValue::ExternalLinkage, (Type *)0);

    std::vector<Value *> fargs;
    fargs.push_back(ptr_arrayidx_m);
    fargs.push_back(ConstantInt::get(M.getContext(), APInt(32, idx)));

    ArrayRef<Value *> functionArguments(fargs);
    iocCheck = CallInst::Create(fc, functionArguments, "");
    if (ci->getParent())
      ci->getParent()->getInstList().insert(ci, iocCheck);
  }

  GlobalVariable *createGlobalArray(Module &M, uint64_t size,
                                    std::string sinkKind) {
    ArrayType *intPtr =
        ArrayType::get(IntegerType::get(M.getContext(), 32), size);

    GlobalVariable *iocArray =
        new GlobalVariable(M, intPtr, false, GlobalValue::ExternalLinkage, 0,
                           "__gl_ioc_malloc_" + sinkKind);
    iocArray->setAlignment(8);

    std::vector<Constant *> Initializer;
    Initializer.reserve(size);
    Constant *zero = ConstantInt::get(IntegerType::get(M.getContext(), 32), 0);

    for (uint64_t i = 0; i < size; i++) {
      Initializer[i] = zero;
    }

    ArrayType *ATy = ArrayType::get(IntegerType::get(M.getContext(), 32), size);
    Constant *initAr = llvm::ConstantArray::get(ATy, Initializer);

    iocArray->setInitializer(initAr);
    return iocArray;
  }

  GlobalVariable *getGlobalArray(Module &M, std::string sinkKind) {
    iplist<GlobalVariable>::iterator gvIt;
    GlobalVariable *tmpgl = NULL;

    for (gvIt = M.global_begin(); gvIt != M.global_end(); gvIt++)
      if (gvIt->getName().str() == "__gl_ioc_malloc_" + sinkKind)
        tmpgl = &*gvIt;

    return tmpgl;
  }

  void addFunctions(Module &M, GlobalVariable *gl);

  bool trackSoln(Module &M, InfoflowSolution *soln, CallInst *sinkCI,
                 std::string &kind) {
    bool ret = false;

    for (Module::iterator mi = M.begin(); mi != M.end(); mi++) {
      Function &F = *mi;
      for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock &B = *bi;
        for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {
          if (checkBackwardTainted(*ii, soln)) {
            DEBUG(ii->dump());
            if (CallInst *ci = dyn_cast<CallInst>(ii)) {
              ret = ret || trackSolnInst(ci, M, sinkCI, soln, kind);
            }
          }
        }
      }
    }
    return ret;
  }

  bool trackSolnInst(CallInst *ci, Module &M, CallInst *sinkCI,
                     InfoflowSolution *soln, std::string &kind) {
    bool ret = false;

    Function *func = ci->getCalledFunction();
    if (!func)
      return false;
    std::string fname = func->getName();

    const CallTaintEntry *entry =
        findEntryForFunction(wLstSourceSummaries, fname);
    if (entry->Name && mode == WHITELISTING) {
      std::string srcKind = "src0" + kind;

      InfoflowSolution *fsoln = getForwardSolFromEntry(srcKind, ci, entry);

      Function *sinkFunc = sinkCI->getCalledFunction();
      if (!sinkFunc)
        return false;
      if (sinkFunc->getName() == "__ioc_report_conversion") {
        if (checkForwardTainted(*(sinkCI->getOperand(7)), fsoln)) {
          ret = true;
        } else {
          ret = false;
        }
      } else if (ioc_report_all_but_conv(sinkFunc->getName())) {
        if (checkForwardTainted(*(sinkCI->getOperand(4)), fsoln) ||
            checkForwardTainted(*(sinkCI->getOperand(5)), fsoln)) {
          ret = true;
        } else {
          ret = false;
        }
      } else {
        ;
      }
    }

    entry = findEntryForFunction(bLstSourceSummaries, fname);

    if (entry->Name && mode == BLACKLISTING) {
      std::string srcKind = "src1" + kind;
      InfoflowSolution *fsoln = getForwardSolFromEntry(srcKind, ci, entry);

      Function *sinkFunc = sinkCI->getCalledFunction();
      if (!sinkFunc)
        return false;
      if (sinkFunc->getName() == "__ioc_report_conversion") {
        if (checkForwardTainted(*(sinkCI->getOperand(7)), fsoln)) {
          return false;
        } else {
          ;
        }

      } else if (ioc_report_all_but_conv(sinkFunc->getName())) {
        if (checkForwardTainted(*(sinkCI->getOperand(4)), fsoln) ||
            checkForwardTainted(*(sinkCI->getOperand(5)), fsoln)) {
          return false;
        } else {
          ;
        }
      } else {
        ;
      }
    }

    if (mode == SENSITIVE) {
      ;
    }

    return ret;
  }

  void backSensitiveArithm(Module &M, CallInst *srcCI, std::string iocKind,
                           InfoflowSolution *fsoln) {
    Function *func;

    for (Module::iterator mi = M.begin(); mi != M.end(); mi++) {
      Function &F = *mi;
      for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock &B = *bi;
        for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {
          if (CallInst *ci = dyn_cast<CallInst>(ii)) {
            func = ci->getCalledFunction();
            if (!func)
              continue;

            const CallTaintEntry *entry =
                findEntryForFunction(sensSinkSummaries, func->getName());
            if (entry->Name) {
              std::string sinkKind = getKindCall(M, F, ci);
              if (checkForwardTaintedAny(ci, fsoln)) {
                unsigned int i;
                for (i = 0; i < ci->getNumOperands() - 1; i++) {
                  infoflow->setUntainted(sinkKind, *(ci->getOperand(i)));
                }

                std::set<std::string> kinds;
                kinds.insert(sinkKind);

                InfoflowSolution *soln =
                    infoflow->greatestSolution(kinds, false);

                if (checkBackwardTainted(*(srcCI->getOperand(0)), soln) ||
                    checkBackwardTainted(*(srcCI->getOperand(1)), soln)) {

                  // add sens sink to this ioc
                  sensPoints[iocKind].push_back(sinkKind);
                  iocPoints[sinkKind].push_back(iocKind);
                }
              }
            }
          }
        }
      }
    }
  }

  void backSensitiveInst(Function &F, Module &M, Instruction &srcCI,
                         std::string iocKind, InfoflowSolution *fsoln) {
    Function *func;

    for (Module::iterator mi = M.begin(); mi != M.end(); mi++) {
      Function &F = *mi;
      for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock &B = *bi;
        for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {
          if (CallInst *ci = dyn_cast<CallInst>(ii)) {
            func = ci->getCalledFunction();
            if (!func)
              continue;

            const CallTaintEntry *entry =
                findEntryForFunction(sensSinkSummaries, func->getName());
            if (entry->Name) {
              std::string sinkKind = getKindCall(M, F, ci);

              if (checkForwardTaintedAny(ci, fsoln)) {
                unsigned int i;
                for (i = 0; i < ci->getNumOperands() - 1; i++) {
                  infoflow->setUntainted(sinkKind, *(ci->getOperand(i)));
                }

                std::set<std::string> kinds;
                kinds.insert(sinkKind);

                InfoflowSolution *soln =
                    infoflow->greatestSolution(kinds, false);

                if (checkBackwardTainted(srcCI, soln)) {
                  handleStrictShift(iocKind, sinkKind, F, M);
                  sensPoints[iocKind].push_back(sinkKind);
                  iocPoints[sinkKind].push_back(iocKind);
                }
              }
            }
          }
        }
      }
    }
  }

  void searchSensFromArithm(Function &F, Module &M, std::string iocKind,
                            CallInst *ci) {
    std::string srcKind = getKindCall(M, F, ci);
    InfoflowSolution *fsoln = getForwardSol(srcKind, ci);
    backSensitiveArithm(M, ci, iocKind, fsoln);
  }

  void searchSensFromInst(Function &F, Module &M, std::string iocKind,
                          Instruction &i) {
    std::string srcKind = getKindInst(M, F, i);
    infoflow->setTainted(srcKind, i);
    std::set<std::string> kinds;
    kinds.insert(srcKind);
    InfoflowSolution *fsoln = infoflow->leastSolution(kinds, false, true);

    backSensitiveInst(F, M, i, iocKind, fsoln);
  }

  void handleStrictShift(std::string iocKind, std::string sinkKind, Function &F,
                         Module &M) {
    std::string shlKind;
    Function *func;
    Function *pfunc;
    bool found_shl = false;

    for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
      BasicBlock &B = *bi;
      for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {
        if (CallInst *ci = dyn_cast<CallInst>(ii)) {
          func = ci->getCalledFunction();
          if (!func)
            continue;
          if (func->getName() == "__ioc_report_shl_strict") {
            bi--;
            bi--;

            BasicBlock &PB = *bi;
            for (BasicBlock::iterator pii = PB.begin(); pii != PB.end();
                 pii++) {

              if (CallInst *pci = dyn_cast<CallInst>(pii)) {
                pfunc = pci->getCalledFunction();
                if (!pfunc)
                  continue;

                std::string pfname = pfunc->getName();
                if (pfname == "__ioc_report_shl_bitwidth") {
                  shlKind = getKindCall(M, F, pci);
                  found_shl = true;
                  break;
                }
              }
            }
          }
        }
        if (found_shl)
          break;
      }
      if (found_shl)
        break;
    }

    if (found_shl) {
      sensPoints[shlKind].push_back(sinkKind);
      iocPoints[sinkKind].push_back(shlKind);
    }
  }

  void backwardSlicingBlacklisting(Module &M, InfoflowSolution *fsoln,
                                   CallInst *srcCI) {
    Function *func;
    for (Module::iterator mi = M.begin(); mi != M.end(); mi++) {
      Function &F = *mi;
      for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock &B = *bi;
        for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {
          if (CallInst *ci = dyn_cast<CallInst>(ii)) {
            if (xformMap[ci])
              continue;

            func = ci->getCalledFunction();
            if (!func)
              continue;
            if (func->getName() == "__ioc_report_conversion") {
              xformMap[ci] = false;

              if (checkForwardTainted(*(ci->getOperand(7)), fsoln)) {
                std::string sinkKind = getKindId("conv", &unique_id);

                InfoflowSolution *soln = getBackSolConv(sinkKind, ci);

                if (checkBackwardTainted(*srcCI, soln))
                  xformMap[ci] = true;
              }

            } else if (ioc_report_all_but_conv(func->getName())) {
              xformMap[ci] = false;
              if (checkForwardTainted(*(ci->getOperand(4)), fsoln) ||
                  checkForwardTainted(*(ci->getOperand(5)), fsoln)) {
                std::string sinkKind = getKindId("arithm", &unique_id);
                InfoflowSolution *soln = getBackSolArithm(sinkKind, ci);

                if (checkBackwardTainted(*srcCI, soln))
                  xformMap[ci] = true;
              }
            }
          }
        }
      }
    }
  }

  void taintForward(std::string srcKind, CallInst *ci,
                    const CallTaintEntry *entry) {
    const CallTaintSummary *vSum = &(entry->ValueSummary);
    const CallTaintSummary *dSum = &(entry->DirectPointerSummary);
    const CallTaintSummary *rSum = &(entry->RootPointerSummary);

    if (vSum->TaintsReturnValue)
      infoflow->setTainted(srcKind, *ci);

    for (unsigned ArgIndex = 0; ArgIndex < vSum->NumArguments; ++ArgIndex) {
      if (vSum->TaintsArgument[ArgIndex])
        infoflow->setTainted(srcKind, *(ci->getOperand(ArgIndex)));
    }

    if (dSum->TaintsReturnValue)
      infoflow->setDirectPtrTainted(srcKind, *ci);

    for (unsigned ArgIndex = 0; ArgIndex < dSum->NumArguments; ++ArgIndex) {
      if (dSum->TaintsArgument[ArgIndex])
        infoflow->setDirectPtrTainted(srcKind, *(ci->getOperand(ArgIndex)));
    }

    if (rSum->TaintsReturnValue)
      infoflow->setReachPtrTainted(srcKind, *ci);

    for (unsigned ArgIndex = 0; ArgIndex < rSum->NumArguments; ++ArgIndex) {
      if (rSum->TaintsArgument[ArgIndex])
        infoflow->setReachPtrTainted(srcKind, *(ci->getOperand(ArgIndex)));
    }
  }

  void taintBackwards(std::string sinkKind, CallInst *ci,
                      const CallTaintEntry *entry) {
    const CallTaintSummary *vSum = &(entry->ValueSummary);
    const CallTaintSummary *dSum = &(entry->DirectPointerSummary);
    const CallTaintSummary *rSum = &(entry->RootPointerSummary);

    if (vSum->TaintsReturnValue)
      infoflow->setUntainted(sinkKind, *ci);
    for (unsigned ArgIndex = 0; ArgIndex < vSum->NumArguments; ++ArgIndex) {
      if (vSum->TaintsArgument[ArgIndex])
        infoflow->setUntainted(sinkKind, *(ci->getOperand(ArgIndex)));
    }

    if (dSum->TaintsReturnValue)
      infoflow->setDirectPtrUntainted(sinkKind, *ci);
    for (unsigned ArgIndex = 0; ArgIndex < dSum->NumArguments; ++ArgIndex) {
      if (dSum->TaintsArgument[ArgIndex])
        infoflow->setDirectPtrUntainted(sinkKind, *(ci->getOperand(ArgIndex)));
    }

    if (rSum->TaintsReturnValue)
      infoflow->setReachPtrUntainted(sinkKind, *ci);
    for (unsigned ArgIndex = 0; ArgIndex < rSum->NumArguments; ++ArgIndex) {
      if (rSum->TaintsArgument[ArgIndex])
        infoflow->setReachPtrUntainted(sinkKind, *(ci->getOperand(ArgIndex)));
    }
  }

  InfoflowSolution *forwardSlicingBlacklisting(CallInst *ci,
                                               const CallTaintEntry *entry,
                                               uint64_t *id);

  InfoflowSolution *getForwardSolFromEntry(std::string srcKind, CallInst *ci,
                                           const CallTaintEntry *entry) {
    taintForward(srcKind, ci, entry);
    std::set<std::string> kinds;
    kinds.insert(srcKind);
    InfoflowSolution *fsoln = infoflow->leastSolution(kinds, false, true);

    return fsoln;
  }

  InfoflowSolution *getBackwardsSolFromEntry(std::string sinkKind, CallInst *ci,
                                             const CallTaintEntry *entry) {
    taintBackwards(sinkKind, ci, entry);
    std::set<std::string> kinds;
    kinds.insert(sinkKind);
    InfoflowSolution *fsoln = infoflow->greatestSolution(kinds, false);

    return fsoln;
  }

  InfoflowSolution *getForwardSol(std::string srcKind, CallInst *ci) {
    infoflow->setTainted(srcKind, *ci);
    std::set<std::string> kinds;
    kinds.insert(srcKind);
    InfoflowSolution *fsoln = infoflow->leastSolution(kinds, false, true);

    return fsoln;
  }

  InfoflowSolution *getBackwardsSol(std::string sinkKind, CallInst *ci) {
    infoflow->setUntainted(sinkKind, *ci);
    std::set<std::string> kinds;
    kinds.insert(sinkKind);
    InfoflowSolution *fsoln = infoflow->greatestSolution(kinds, false);

    return fsoln;
  }

  InfoflowSolution *getBackSolArithm(std::string sinkKind, CallInst *ci) {
    Value *lval = ci->getOperand(4);
    Value *rval = ci->getOperand(5);
    infoflow->setUntainted(sinkKind, *lval);
    infoflow->setUntainted(sinkKind, *rval);

    std::set<std::string> kinds;
    kinds.insert(sinkKind);

    return infoflow->greatestSolution(kinds, false);
  }

  InfoflowSolution *getForwSolArithm(std::string srcKind, CallInst *ci) {
    Value *lval = ci->getOperand(4);
    Value *rval = ci->getOperand(5);
    infoflow->setTainted(srcKind, *lval);
    infoflow->setTainted(srcKind, *rval);

    std::set<std::string> kinds;
    kinds.insert(srcKind);

    return infoflow->leastSolution(kinds, false, true);
  }

  InfoflowSolution *getForwSolConv(std::string srcKind, CallInst *ci) {
    Value *val = ci->getOperand(7);
    infoflow->setTainted(srcKind, *val);

    std::set<std::string> kinds;
    kinds.insert(srcKind);

    return infoflow->leastSolution(kinds, false, true);
  }

  InfoflowSolution *getBackSolConv(std::string sinkKind, CallInst *ci) {
    Value *val = ci->getOperand(7);
    infoflow->setUntainted(sinkKind, *val);

    std::set<std::string> kinds;
    kinds.insert(sinkKind);

    return infoflow->greatestSolution(kinds, false);
  }

  void removeBenignChecks(Module &M) {
    for (Module::iterator mi = M.begin(); mi != M.end(); mi++) {
      Function &F = *mi;
      for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
        BasicBlock &B = *bi;
        for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {
          if (CallInst *ci = dyn_cast<CallInst>(ii)) {
            Function *func = ci->getCalledFunction();
            if (!func)
              continue;
            if (ioc_report_all(func->getName()) && !xformMap[ci]) {
              setWrapper(ci, M, func);
            }
          }
        }
      }
    }
  }

  void checkfTainted(Module &M, InfoflowSolution *f);

  void setWrapper(CallInst *ci, Module &M, Function *func) {
    FunctionType *ftype = func->getFunctionType();
    std::string fname = "__ioc_" + std::string(func->getName());

    Constant *ioc_wrapper =
        M.getOrInsertFunction(fname, ftype, func->getAttributes());
    ci->setCalledFunction(ioc_wrapper);
  }

  bool ioc_report_all_but_conv(std::string name) {
    return (name == "__ioc_report_add_overflow" ||
            name == "__ioc_report_sub_overflow" ||
            name == "__ioc_report_mul_overflow" ||
            name == "__ioc_report_shr_bitwidth" ||
            name == "__ioc_report_shl_bitwidth" ||
            name == "__ioc_report_shl_strict");
  }

  bool ioc_report_all(std::string name) {
    return (name == "__ioc_report_add_overflow" ||
            name == "__ioc_report_sub_overflow" ||
            name == "__ioc_report_mul_overflow" ||
            name == "__ioc_report_shr_bitwidth" ||
            name == "__ioc_report_shl_bitwidth" ||
            name == "__ioc_report_shl_strict" ||
            name == "__ioc_report_conversion");
  }

  bool ioc_report_arithm(std::string name) {
    return (name == "__ioc_report_add_overflow" ||
            name == "__ioc_report_sub_overflow" ||
            name == "__ioc_report_mul_overflow");
  }

  bool ioc_report_shl(std::string name) {
    return (name == "__ioc_report_shr_bitwidth" ||
            name == "__ioc_report_shl_bitwidth" ||
            name == "__ioc_report_shl_strict");
  }
  bool llvm_arithm(std::string name) {
    return (StringRef(name).startswith("llvm.sadd.with.overflow") ||
            StringRef(name).startswith("llvm.uadd.with.overflow") ||
            StringRef(name).startswith("llvm.ssub.with.overflow") ||
            StringRef(name).startswith("llvm.usub.with.overflow") ||
            StringRef(name).startswith("llvm.smul.with.overflow") ||
            StringRef(name).startswith("llvm.umul.with.overflow"));
  }

  bool checkBackwardTainted(Value &V, InfoflowSolution *soln,
                            bool direct = true) {
    bool ret = (!soln->isTainted(V));

    if (direct) {
      ret = ret || (!soln->isDirectPtrTainted(V));
    }

    return ret;
  }

  bool checkForwardTainted(Value &V, InfoflowSolution *soln,
                           bool direct = true) {
    bool ret = (soln->isTainted(V));

    if (direct) {
      ret = ret || (soln->isDirectPtrTainted(V));
    }

    return ret;
  }
  bool checkForwardTaintedAny(CallInst *ci, InfoflowSolution *soln) {
    unsigned int i;

    for (i = 0; i < ci->getNumOperands() - 1; i++) {
      if (checkForwardTainted(*(ci->getOperand(i)), soln)) {
        return true;
      }
    }

    return false;
  }

  bool isConstAssign(const std::set<const Value *> vMap) {
    std::set<const Value *>::const_iterator vi = vMap.begin();
    std::set<const Value *>::const_iterator ve = vMap.end();

    for (; vi != ve; vi++) {
      const Value *val = (const Value *)*vi;
      if (const CallInst *ci = dyn_cast<const CallInst>(val)) {
        Function *func = ci->getCalledFunction();
        if (func && func->getName().startswith("llvm.ssub.with.overflow")) {
          continue;
        } else {
          return false;
        }
      } else if (dyn_cast<const LoadInst>(val)) {
        return false;
      } else {
        ;
      }
    }
    return true;
  }

  void removeChecksForFunction(Function &F, Module &M) {
    for (unsigned i = 0; rmCheckList[i].func; i++) {
      if (F.getName() == rmCheckList[i].func) {
        for (Function::iterator bi = F.begin(); bi != F.end(); bi++) {
          BasicBlock &B = *bi;
          for (BasicBlock::iterator ii = B.begin(); ii != B.end(); ii++) {
            if (CallInst *ci = dyn_cast<CallInst>(ii))
              removeChecksInst(ci, i, M);
          }
        }
      }
    }
  }

  void removeChecksInst(CallInst *ci, unsigned int i, Module &M) {
    Function *f = ci->getCalledFunction();
    if (!f)
      return;

    std::string fname = f->getName();

    if ((rmCheckList[i].overflow && ioc_report_arithm(fname)) ||
        (rmCheckList[i].conversion && (fname == "__ioc_report_conversion")) ||
        (rmCheckList[i].shift && ioc_report_shl(fname))) {
      xformMap[ci] = true;
      setWrapper(ci, M, f);
    }
  }

  void format_ioc_report_func(const Value *val, raw_string_ostream &rs) {
    const CallInst *ci = dyn_cast<CallInst>(val);
    if (!xformMap[ci])
      return;

    const Function *func = ci->getCalledFunction();
    if (!func)
      return;

    uint64_t line = getIntFromVal(ci->getOperand(0));
    uint64_t col = getIntFromVal(ci->getOperand(1));

    std::string fname = "";
    getStringFromVal(ci->getOperand(2), fname);

    rs << func->getName().str() << ":";
    rs << fname << ":";
    rs << " (line ";
    rs << line;
    rs << ", col ";
    rs << col << ")";

    if (ioc_report_all_but_conv(func->getName())) {
      ;
    } else if (func->getName() == "__ioc_report_conversion") {
      ;
    } else {
      ;
    }
  }

  void getStringFromVal(Value *val, std::string &output) {
    Constant *gep = dyn_cast<Constant>(val);
    assert(gep && "assertion");
    GlobalVariable *global = dyn_cast<GlobalVariable>(gep->getOperand(0));
    assert(global && "assertion");
    ConstantDataArray *array =
        dyn_cast<ConstantDataArray>(global->getInitializer());
    if (array->isCString())
      output = array->getAsCString();
  }

  void getMode() {
    std::ifstream ifmode;
    std::string tmp;
    ifmode.open(MODE_FILE);
    if (!ifmode.is_open()) {
      exit(1);
    }
    ifmode >> tmp;
    mode = (unsigned char)atoi(tmp.c_str());
    ifmode >> tmp;
    if (ifmode.good()) {
      exit(1);
    }
    if (mode < 1 || mode > MODE_MAX_NUM) {
      exit(1);
    }
  }

  uint64_t getIntFromVal(Value *val) {
    ConstantInt *num = dyn_cast<ConstantInt>(val);
    assert(num && "constant int casting check");
    return num->getZExtValue();
  }

  uint64_t getColFromVal(Value *val);

  std::string getKindId(std::string name, uint64_t *unique_id) {
    std::stringstream SS;
    SS << (*unique_id)++;
    return name + SS.str();
  }

  std::string getKindCall(Module &M, Function &F, CallInst *ci) {
    std::stringstream SS;
    std::string tmp = M.getModuleIdentifier();
    SS << tmp;
    SS << ":";

    tmp = F.getName();
    SS << tmp;
    SS << ":";

    Function *func = ci->getCalledFunction();
    if (func)
      tmp = func->getName();
    else
      tmp = "main";
    SS << tmp;
    SS << ":";

    if (ci->getParent())
      tmp = ci->getParent()->getName();
    SS << tmp;

    std::string stringKind = SS.str();
    return stringKind;
  }

  std::string getKindInst(Module &M, Function &F, Instruction &ci) {
    std::stringstream SS;
    std::string tmp = M.getModuleIdentifier();
    SS << tmp;
    SS << ":";

    tmp = F.getName();
    SS << tmp;
    SS << ":";

    tmp = ci.getOpcodeName();
    SS << tmp;
    SS << ":";

    if (ci.getParent())
      tmp = ci.getParent()->getName();
    SS << tmp;

    std::string stringKind = SS.str();
    return stringKind;
  }

}; // class

typedef std::vector<std::string> iocPointVector;
typedef std::map<std::string, iocPointVector> iocPointsForSens;

typedef std::vector<std::string> sensPointVector;
typedef std::map<std::string, sensPointVector> sensPointsForIOC;

iocPointsForSens iocPoints;
sensPointsForIOC sensPoints;
} // namespace

#endif