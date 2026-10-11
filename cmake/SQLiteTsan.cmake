# Keep the verified download unchanged. Patch only the TSan build's generated copy.
# SQLite 3.54.0 walIndexRecover resets nBackfill with a plain write, while
# walTryBeginRead reads it via AtomicLoad before acquiring the read lock.
function(stockguard_prepare_sqlite_tsan input output)
  file(READ "${input}" source)
  set(before "    pInfo = walCkptInfo(pWal);\n    pInfo->nBackfill = 0;\n    pInfo->nBackfillAttempted = pWal->hdr.mxFrame;")
  set(after "    pInfo = walCkptInfo(pWal);\n    AtomicStore(&pInfo->nBackfill, 0);\n    pInfo->nBackfillAttempted = pWal->hdr.mxFrame;")
  string(FIND "${source}" "${before}" first)
  if(first EQUAL -1)
    message(FATAL_ERROR "SQLite TSan patch context missing; review the dependency version")
  endif()
  string(REPLACE "${before}" "" remainder "${source}")
  # Compare lengths to reject repeated contexts rather than silently patching multiple sites.
  string(LENGTH "${source}" source_length)
  string(LENGTH "${remainder}" remainder_length)
  string(LENGTH "${before}" context_length)
  math(EXPR expected_length "${source_length} - ${context_length}")
  if(NOT remainder_length EQUAL expected_length)
    message(FATAL_ERROR "SQLite TSan patch context is ambiguous")
  endif()
  string(REPLACE "${before}" "${after}" patched "${source}")
  file(CONFIGURE OUTPUT "${output}" CONTENT "${patched}" @ONLY)
endfunction()
