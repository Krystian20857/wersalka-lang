* stdlib
* (more) arrays, lists, overall collections
* break, continue
* for stmt
* proper `new { ... }` expression
* DiagnosticReporter -> exception adapter
* eval(...), load_module_sources(...), etc
* cinterop (e.g., for raylib)
* inline caches
* JIT compiler
  * at first simple templated interpreter
  * later experiment with optimizing JIT, profiling, deopt, OSR, etc
    * CFG SSA IR, predictible inlining
    * very basic escape analysis
    * codegen backend(s), reg alloc, insn scheduling, etc
    * macro assembler, (maybe) custom arch descriptor files, or just use asmjit idk
