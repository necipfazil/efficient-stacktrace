#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

#include "cg.hpp"

// TODO: For better performance, consider using different data structures
// (e.g., raw pointers instead of std::vectors). 

typedef std::vector<uintptr_t> StackTrace;
// Medium stack trace hashes used for pruning
typedef std::unordered_set<uint32_t> MedSTSet;
// TODO: Account for hash collisions in inputted stack traces (e.g., use
// array of stack traces instead).
typedef std::unordered_map<uintptr_t, StackTrace> STSet;

// Used as inout to DFS
struct STInfo {
  /* in  */ StackTrace ST;
  /* in  */ uintptr_t Hash = 0; 
  /* out */ uintptr_t NumHashMatches = 0;
  /* out */ bool FoundCorrectMatch = false;
};

// TODO: Account for hash collisions in inputted stack traces (e.g., use
// array of stack traces instead).
typedef std::unordered_map<uintptr_t, STInfo> STInfoSet;

struct DFSRes {
  uintptr_t PruningCount = 0;
  uintptr_t VisitedNodeCount = 0;
};

uintptr_t 
HashStep(uintptr_t Hash, uintptr_t PC, size_t Idx, size_t kMedHashIdx) {
  uintptr_t CRC32 = __builtin_ia32_crc32di(Hash, PC);
  // TODO: is that the right approach to OR hashes? XOR instead?
  if (Idx == kMedHashIdx)
    return CRC32 | (Hash << 32);
  else
    return CRC32 | ((Hash >> 32) << 32);
}

uintptr_t Hash(const StackTrace &ST, size_t kMedHashIdx) {
  uintptr_t Res = 0;
  for (size_t I = 0; I < ST.size(); I++) 
    Res = HashStep(Res, ST[I], I, kMedHashIdx);
  return Res;
}

std::unordered_map<std::string /* FuncName */, STSet>
ReadStackTraces(std::istream &In, size_t DepthLimit, size_t kMedHashIdx) {
  std::unordered_map<std::string, STSet> Res;
  std::string X;
  int CountStackTracesClipped = 0;
  int CountHashCollisions = 0;
  while (std::getline(In, X)) {
    std::stringstream Line(X);
    std::string FuncName;
    Line >> FuncName;
    StackTrace ST;
    int CurrentDepth = 0;
    while (true) {
      uintptr_t PC;
      Line >> std::hex >> PC;
      if (!Line) break;
      ST.push_back(PC);
      if (++CurrentDepth == DepthLimit) {
        CountStackTracesClipped++;
        break;
      }

    }
    uintptr_t STHash = Hash(ST, kMedHashIdx);
    // TODO: stack traces with hash collisions might or might not be same
    // as we don't compare the full stack traces here. Also keep track of
    // collisions for different stack traces, which is important to design
    // the compression method.
    if (Res[FuncName].count(STHash)) CountHashCollisions++;
    Res[FuncName][STHash] = ST;
  }
  if (CountStackTracesClipped)
    fprintf(stderr, "WARNING: %d stack traces were clipped as they exceeded "
                    "the depth limit.\n", CountStackTracesClipped);
  if (CountHashCollisions)
    fprintf(stderr, "WARNING: %d stack traces had hash collisions.\n", 
                                                      CountHashCollisions);
  return Res;
}

void
ProcessMatch(STInfoSet &STIS, uintptr_t *ST, size_t Depth, size_t kMedHashIdx)
{
  // Create the stack trace with depth (slice from ST).
  StackTrace ST1(ST, ST + Depth);

  // Re-compute the hash and verify
  uintptr_t H = Hash(ST1, kMedHashIdx);
  assert(STIS.count(H) 
        && "Can't verify the match: no stack trace with such hash.");

  // Check if the stack traces match
  bool STMatches = ST1 == STIS[H].ST;

  STIS[H].FoundCorrectMatch |= STMatches;
  STIS[H].NumHashMatches += STMatches;
}


uintptr_t /* Number of nodes visited */
DFS(
  CallGraph &CG,     /* Call graph */
  uintptr_t *ST,     /* Constructed stack trace */
  uintptr_t STSize,  /* Max size for the stack trace being constructed */
  uintptr_t EntryPC, /* Entry to the reverse call graph. Updated with each step. */
  uintptr_t Hash,    /* Current hash for the stack trace being constructed. */
  size_t Depth,      /* Depth taken so far in the DFS. Will be capped by STSize. */
  size_t kMedHashIdx, /* Medium hash index used for pruning. */
  STInfoSet &STIS,   /* Stack trace set to search matches for. Also update with results. */
  MedSTSet &MSTS,    /* Set of pruning hashes, one per each ST in STSet. */
  DFSRes &DFSResult) /* DFS Results. Out. */
{
  uintptr_t Count = 1; // Number of nodes in the DFS visited. Current node is +1.
  ++DFSResult.VisitedNodeCount;

  // Depth is at most STSize, i.e., max depth.
  assert(Depth <= STSize);

  // Check for hash matches (or collisions). Record/log any info.
  if (STIS.count(Hash)) ProcessMatch(STIS, ST, Depth, kMedHashIdx);

  // Pruning
  if (Depth == kMedHashIdx && !MSTS.count(Hash)) {
      ++DFSResult.PruningCount;
      return 1;
  }
  if (Depth < STSize) {
    // Pull all possible callers for the function
    const auto &CallerVec = CG.TargetsToCallers[EntryPC];

    for (auto FuncCall : CallerVec) {
      // Take edge
      ST[Depth] = FuncCall.CallSitePc;
      Count += DFS(
        CG,
        ST,
        STSize,
        FuncCall.CallerPc, // Updated function entry with edge taken
        HashStep(Hash, FuncCall.CallSitePc, Depth, kMedHashIdx), // Updated hash
        Depth + 1, // Updated depth
        kMedHashIdx,
        STIS,
        MSTS,
        DFSResult);
    }
  } // else (i.e., if max depth is reached), don't visit further nodes.
  return Count;

}

uintptr_t /* Number of nodes visited */
DFS(
  CallGraph &CG,      /* Call graph */
  uintptr_t Func0,    /* Entry point to reverse CG: last function called */
  size_t MaxDepth,    /* Maximum depth during DFS */
  size_t kMedHashIdx, /* Medium hash index used for pruning */
  STInfoSet &STIS,    /* Stack trace set. Looks for all traces simultaneously */
  DFSRes &DFSResult)  /* DFS Results. Out. */
{
  // Compute the right shifted hashes used for pruning
  MedSTSet MSTS;
  for (auto ST : STIS) MSTS.insert(ST.first >> 32);
  // Create space for stack trace to be used reconstruction
  StackTrace ST(MaxDepth);

  return DFS(
    CG,         // reverse call graph
    ST.data(),  // an empty stack trace vector
    ST.size(),  // size of the empty stack trace vector
    Func0,      // entry PC
    0,          // Hash
    0,          // Depth
    kMedHashIdx, // Medium hash index used for pruning
    STIS,       // hash: stacktrace mappings
    MSTS,       // hash portions used for pruning
    DFSResult);

}

void
PrintDFSResults(std::ostream &Out, std::ostream &Err,
                const std::string& FuncName,
                const CallGraph &CG,
                const DFSRes &DFSResults, const STInfoSet &STIS,
                bool PrintNonDecompST)
{
  uintptr_t TotalST = STIS.size();
  uintptr_t TotalFoundCorrectly = 0;
  uintptr_t TotalCouldNotFind = 0;
  float PercFoundCorrectly = 0;
  uintptr_t TotalHadIncorrectCollisions = 0;
  uintptr_t TotalIncorrectCollisions = 0;
  uintptr_t TotalDirCallsCorrectlyFound = 0;
  uintptr_t TotalIndirCallsCorrectFound = 0;

  if (PrintNonDecompST)
    Err << "== STACK TRACES CAN'T DECOMP FOR \"" << FuncName << "\" ==\n";
  for (const auto &El : STIS) {
    auto Hash = El.first;
    const auto &STI = El.second;
    
    // TODO: Record these into DFSResults instead of computing here.
    TotalFoundCorrectly += STI.FoundCorrectMatch;
    TotalCouldNotFind += !STI.FoundCorrectMatch;
    TotalHadIncorrectCollisions += (STI.NumHashMatches - STI.FoundCorrectMatch) > 0;
    TotalIncorrectCollisions += STI.NumHashMatches - STI.FoundCorrectMatch;

    if (STI.FoundCorrectMatch) {
      for (const auto& Addr : STI.ST) {
        TotalDirCallsCorrectlyFound += CG.DirCallSiteAddrs.count(Addr); 
        TotalIndirCallsCorrectFound += CG.IndirCallSiteAddrs.count(Addr);
      }
    } else if(!STI.FoundCorrectMatch && PrintNonDecompST) {
      Err << FuncName;
      // TODO: symbolize the addresses as func+offset
      for (const auto& Addr : STI.ST) Err << " " << std::hex << Addr;
      Err << "\n";
    }
  }

  PercFoundCorrectly = TotalST ? (float)TotalFoundCorrectly / TotalST : 1;
  PercFoundCorrectly *= 100;

  // TODO: print how many indirect/direct calls were there, i.e., visited indirect/direct call sites
  Out 
      // Number of unique stack traces after cutting off to max depth.
      << "\nNum unique stack traces         : " << TotalST
      // Number of stack traces that were decompressed correctly.
      << "\nNum decompressed correctly      : " << TotalFoundCorrectly
      // Number of stack traces that could not be decompressed.
      << "\nNum could not be decompressed   : " << TotalCouldNotFind
      // Percentage of correctly decompressed stack trace
      << "\nSuccess rate                    : " << std::fixed << std::setprecision(2)
                                                << PercFoundCorrectly << "%"
      // Number of stack traces that were matched with a wrong stack trace
      // due to hash collision at least once.
      << "\nNum ST had incorrect collisions : " << TotalHadIncorrectCollisions
      // Number of incorrect hash collisions, e.g., if a stack trace had
      // multiple incorrect hash collisions, all instances are counted.
      << "\nNum incorrect collisions        : " << TotalIncorrectCollisions
      << "\nNum dir calls found correctly   : " << TotalDirCallsCorrectlyFound
      << "\nNum indir calls found correctly : " << TotalIndirCallsCorrectFound
      // Performance results
      << "\nNum nodes visited during DFS    : " << DFSResults.VisitedNodeCount
      // TODO: document that more pruning does not necessarily mean better
      // performance. Pruning less but at less deeper nodes can be better.
      << "\nNum pruning done                : " << DFSResults.PruningCount
      << "\n";
}

int main(int argc, char **argv) {
  if (argc != 5 && argc != 6) {
    // TODO: print info on CLI.
    std::cerr << "Error: CLI" << std::endl;
    // 1: call graph disassembly output
    // 2: stack trace set
    // 3: funcname
    // 4: depth
    // 5: medium hash index used for pruning
    // 6: (optional) set to non-zero to print stack traces that could not 
    //    be decompressed.
    return 1;
  }

  // TODO: support multiple hashes. Currently, whole stack trace is
  // compressed into a single hash value with a single kMedHashIdx. Instead,
  // support multiple hash values computed at some frequency (e.g., per 8
  // frames).

  // Read the call graph (disassembly output).
  std::ifstream CGIn(argv[1]);
  CallGraph CG(CGIn);
  //CG.Print(std::cerr);

  //std::cout << "\n== Reverse call graph ==" << std::endl;
  //CG.PrintReverseCG(std::cout, false);
  //std::cout << "\n==\n" << std::endl;

  // Read depth
  size_t Depth = atoi(argv[3]);

  // Read medium hash index used for pruning
  size_t kMedHashIdx = atoi(argv[4]);

  // Read stack traces
  std::ifstream TargetStacksIn(argv[2]);
  auto STS = ReadStackTraces(TargetStacksIn, Depth, kMedHashIdx);

  // Whether to print stack traces that could not be recovered
  bool PrintNonDecompST = argc == 6 && atoi(argv[5]);

  // Get input stack trace info for DFS
  std::unordered_map<std::string /*FuncName*/, STInfoSet> STIS;
  for (auto &ST : STS) {
    std::string FuncName = ST.first;
    auto &FSTS = ST.second;
    for (auto &ST : FSTS) {
      STInfo STI {
      .ST = ST.second,
      .Hash = ST.first,
      .NumHashMatches = 0,
      .FoundCorrectMatch = false };
      STIS[FuncName][ST.first] = STI;
    }
  }

  DFSRes DFSResult;

  for (auto &El : STIS) {
    std::string FuncName = El.first;
    std::cout << "=== FUNC: \"" << FuncName << "\" ===" << std::endl;
    auto PC = CG.FuncNameToAddr[FuncName];
    STInfoSet &FSTIS = El.second;
    std::cout << "Starting DFS.. " << std::endl;
    uintptr_t Count = DFS(CG, PC, Depth, kMedHashIdx, FSTIS, DFSResult);
    std::cout << "Finished DFS. Printing the results.." << std::endl;
    PrintDFSResults(std::cout, std::cerr, FuncName, CG, DFSResult, FSTIS, PrintNonDecompST);
    std::cout << std::endl;
  }
  return 0;
}