#ifndef __CALL_GRAPH_H__
#define __CALL_GRAPH_H__

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <tuple>
#include <string>

struct CallSite {
  uintptr_t CallerPc;
  uintptr_t CallSitePc;

  CallSite(uintptr_t CallerPc, uintptr_t CallSitePc)
    : CallerPc(CallerPc), CallSitePc(CallSitePc) {}
};

// Just the content from the file
struct CallGraph {
  // Indirect targets: { TypeId: [CallPC,] }
  std::unordered_map<uintptr_t, std::vector<uintptr_t>> TypeIdToIndirTargets;

  // Indirect calls: { TypeId: [EntryPC,] }
  std::unordered_map<uintptr_t, std::vector<uintptr_t>> TypeIdToIndirCalls;

  // Indirect calls: { CallerFuncPc: [IndirectCallSiteAddr,] }
  std::unordered_map<uintptr_t, std::vector<uintptr_t>> FuncAddrToIndirCallSites;

  // Direct calls: { CallerAddr: [(CallSiteAddr, TargetAddr),] }
  std::unordered_map<uintptr_t, 
                    std::vector<std::tuple<uintptr_t, uintptr_t>>
                      > FuncAddrToDirCallSites;

  // Set of direct call site addresses.
  std::unordered_set<uintptr_t> DirCallSiteAddrs;

  // Set of indirect call site addresses.
  std::unordered_set<uintptr_t> IndirCallSiteAddrs;

  // Functions: { FuncAddr: FuncName }
  std::unordered_map<uintptr_t, std::string> FuncAddrToName;

  // Computed from raw info
  std::unordered_map<std::string, uintptr_t> FuncNameToAddr;
  std::unordered_map<uintptr_t /* TargetFuncPc */, std::vector<CallSite> /* potential calls to it */ > TargetsToCallers;

  private:
    void UpdateTargetToCallers();    

    std::unordered_map<uintptr_t /* TargetFuncPc */, std::vector<CallSite>> GetIndirectCalls();

  public:
    // Read from llvm-objdump output
    CallGraph(std::istream &In);

    void Print(std::ostream &Out) const;

    void PrintReverseCG(std::ostream &Out, bool demagle) const;

};

#endif
