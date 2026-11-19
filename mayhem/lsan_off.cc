// Disable LeakSanitizer at build time (SPEC §6.2 item 15): leaks are not the bug class fuzzed here.
extern "C" int __lsan_is_turned_off() { return 1; }
