#include "cg.hpp"  

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <tuple>
#include <string>

template<typename T>
static std::string utohexstr(const T &i) {
  std::stringstream stream;
  stream << "0x" << std::hex << i;
  return stream.str();
}

CallGraph::CallGraph(std::istream &In) {
  std::string X;

  // TODO: reuse code: readToList() etc.
  while(std::getline(In, X)) {
    if (!X.find("INDIRECT TARGETS TYPES")) {
      assert (TypeIdToIndirTargets.empty() 
              && "Multiple \"INDIRECT TARGETS TYPES\" sections.");
      while (std::getline(In, X)) {
        if (X == "") break;
        std::stringstream Line(X);
        // Read type id.
        uintptr_t TypeId;
        Line >> std::hex >> TypeId;
        // Read function entry addresses.
        do {
          uintptr_t TargetAddr;
          Line >> std::hex >> TargetAddr;
          TypeIdToIndirTargets[TypeId].push_back(TargetAddr);
        } while (Line.good());
      }
    }
    if (!X.find("INDIRECT CALLS TYPES")) {
      assert (TypeIdToIndirCalls.empty() 
              && "Multiple \"INDIRECT CALLS TYPES\" sections.");
      while (std::getline(In, X)) {
        if (X == "") break;
        std::stringstream Line(X);
        // Read type id.
        uintptr_t TypeId;
        Line >> std::hex >> TypeId;
        // Read indirect call site addresses.
        do {
          uintptr_t CallSiteAddr;
          Line >> std::hex >> CallSiteAddr;
          TypeIdToIndirCalls[TypeId].push_back(CallSiteAddr);
        } while (Line.good());
      }
    }
    if (!X.find("INDIRECT CALL SITES")) {
      assert (FuncAddrToIndirCallSites.empty() 
              && "Multiple \"INDIRECT CALL SITES\" sections.");
      while (std::getline(In, X)) {  
        if (X == "") break;
        std::stringstream Line(X);
        // Read caller address.
        uintptr_t CallerAddr;
        Line >> std::hex >> CallerAddr;
        // Read indirect call site addresses.
        do {
          uintptr_t CallSiteAddr;
          Line >> std::hex >> CallSiteAddr;
          FuncAddrToIndirCallSites[CallerAddr].push_back(CallSiteAddr);
          IndirCallSiteAddrs.insert(CallSiteAddr);
        } while (Line.good());
      }
    }
    if (!X.find("DIRECT CALL SITES")) {
      assert (FuncAddrToDirCallSites.empty() 
              && "Multiple \"DIRECT CALL SITES\" sections.");
      while (std::getline(In, X)) {
        if (X == "") break;
        std::stringstream Line(X);
        // Read caller address.
        uintptr_t CallerAddr;
        Line >> std::hex >> CallerAddr;
        // Read direct call site and target addresses.
        do {
          uintptr_t CallSiteAddr, TargetAddr;
          Line >> std::hex >> CallSiteAddr >> TargetAddr;
          FuncAddrToDirCallSites[CallerAddr].emplace_back(CallSiteAddr, TargetAddr);
          DirCallSiteAddrs.insert(CallSiteAddr);
        } while (Line.good());
      }
    }
    if (!X.find("FUNCTION SYMBOLS")) {
      assert (FuncAddrToName.empty() 
              && "Multiple \"FUNCTION SYMBOLS\" sections.");
      while (std::getline(In, X)) {
        if (X == "") break;
        std::stringstream Line(X);
        uintptr_t FuncAddr;
        std::string FuncName;
        Line >> std::hex >> FuncAddr >> FuncName;
        FuncAddrToName[FuncAddr] = FuncName;
      }
    }
  }
 
  // Set FuncNameToAddr
  for (auto &El : FuncAddrToName)
    FuncNameToAddr[El.second] = El.first;

  // Compute and set TargetToCallers (reverse call graph) from raw call graph.
  UpdateTargetToCallers();
}


// Use type ids to recover { Target (FuncPc) : {WhoMightCall (FuncPc) : Where(CallSitePc)} }
std::unordered_map<uintptr_t, std::vector<CallSite>>
CallGraph::GetIndirectCalls() {
  auto Res = std::unordered_map<uintptr_t /* TargetFuncPc */, std::vector<CallSite> /* potential calls to it */>();

  // Reverse TypeIdToIndirectCalls: mapping from indirect call site pc to type id
  std::unordered_map<uintptr_t, uintptr_t> ICallSitePcToTypeId;
  for (const auto &El : TypeIdToIndirCalls) {
    const auto &TypeId = El.first;
    const auto &ICallSiteAddrs = El.second;

    for (const auto &ICallSiteAddr : ICallSiteAddrs)
      ICallSitePcToTypeId[ICallSiteAddr] = TypeId;
  }

  for (const auto &El : FuncAddrToIndirCallSites) {
    const auto &FuncAddr = El.first;
    const auto &ICallSiteAddrs = El.second;

    for (const auto &ICallSiteAddr : ICallSiteAddrs) {
      // Only relevant indirect calls that appears in the stack trace.
      if (ICallSitePcToTypeId.count(ICallSiteAddr)) {
        const auto &TypeId = ICallSitePcToTypeId[ICallSiteAddr];

        // Get the potential indirect targets.
        const auto &PotentialTargets = TypeIdToIndirTargets[TypeId];

        // FuncPc might call any one of PotentialTargets at ICallSitePc
        for (const auto &PotentialTarget : PotentialTargets) {   
          // { Target(FuncPc) : {WhereMightBeCalled(CallSitePc) : Who(FuncPc)} }       
          Res[PotentialTarget].emplace_back(FuncAddr, ICallSiteAddr);
        }
      }
    }
  }

  return Res;
}

void CallGraph::UpdateTargetToCallers() {
  TargetsToCallers.clear();
  
  // Get mappings for indirect calls.
  auto TargetsToCallers = GetIndirectCalls();

  // Add mappings for direct calls.
  for (auto const &El : FuncAddrToDirCallSites) {
    uintptr_t CallerAddr = El.first;
    const auto &Calls = El.second;

    for (const auto &Call : Calls) {
      auto CallSiteAddr = std::get<0>(Call);
      auto TargetAddr = std::get<1>(Call);
      TargetsToCallers[TargetAddr].emplace_back(CallerAddr, CallSiteAddr);
    }
  }

  // Set TargetsToCallers.
  this->TargetsToCallers = TargetsToCallers;
}

// Format: Target, CallSitePc, PotentialCaller
void CallGraph::PrintReverseCG(std::ostream &Out, bool demangle) const {
  for (auto const &TargetToCaller : TargetsToCallers) {
    auto const &TargetFuncPc = TargetToCaller.first;
    auto const &PotentialCallers = TargetToCaller.second;

    for (auto const &Caller : PotentialCallers) {
      std::string TargetFuncStr = demangle && FuncAddrToName.count(TargetFuncPc)
                                  ? FuncAddrToName.find(TargetFuncPc)->second
                                  : utohexstr(TargetFuncPc);
      std::string CallerFuncStr = demangle && FuncAddrToName.count(Caller.CallerPc)
                                  ? FuncAddrToName.find(Caller.CallerPc)->second
                                  : utohexstr(Caller.CallerPc);
      
      Out << CallerFuncStr << " calls " << TargetFuncStr << " at " << utohexstr(Caller.CallSitePc) << "\n";
    }
  }
}

void CallGraph::Print(std::ostream &Out) const {
  // Indirect targets.
  Out << "Indirect target types:\n";
  for (auto &IndirectTarget : TypeIdToIndirTargets) {
    auto &TypeId = IndirectTarget.first;
    auto &TargetPcs = IndirectTarget.second;

    Out << "0x" << std::hex << TypeId;
    for (auto &TargetPc : TargetPcs)
      Out << " 0x" << std::hex <<  TargetPc;
    Out << "\n";
  }
  Out << "\n";

  // Indirect call types.
  Out << "Indirect call types:\n";
  for (auto &IndirectCall : TypeIdToIndirCalls) {
    auto &TypeId = IndirectCall.first;
    auto &CallSitePcs = IndirectCall.second;

    Out << "0x" << std::hex << TypeId;
    for (auto &CallSitePc : CallSitePcs)
      Out << " 0x" << std::hex << CallSitePc;
    Out << "\n";
  }
  Out << "\n";

  // Indirect calls.
  Out << "Indirect calls:\n";
  for (auto &IndirectCall : FuncAddrToIndirCallSites) {
    auto &CallerPc = IndirectCall.first;
    auto &CallSitePcs = IndirectCall.second;

    Out << "0x" << std::hex << CallerPc;
    for (auto &CallSitePc : CallSitePcs)
      Out << " 0x" << std::hex << CallSitePc;
    Out << "\n";
  }
  Out << "\n";

  // Direct calls
  Out << "Direct calls:\n";
  for (auto &El : FuncAddrToDirCallSites) {
    uintptr_t CallerAddr = El.first;
    const auto &DirectCalls = El.second;
    
    Out << std::hex << CallerAddr;

    for (const auto &DirectCall : DirectCalls) {
      uintptr_t CallSiteAddr = std::get<0>(DirectCall);
      uintptr_t TargetAddr   = std::get<1>(DirectCall);  
      Out << " 0x" << std::hex << CallSiteAddr << " 0x" << TargetAddr;
    }
    Out << "\n";
  }
  Out << "\n";

  // Function entries.
  Out << "Function entries\n";
  for (auto &El : FuncAddrToName) {
    uintptr_t FundAddr = El.first;
    std::string FuncName = El.second;
    
    Out << "0x" << std::hex << FundAddr << " " << FuncName << "\n";
  }
}
