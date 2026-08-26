//
// Compile real C11 with xcc, load it, and execute it on the emulated CPC.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "cpc/machine.h"
#include "test_support.h"

int main()
{
    test::section("xcc C11 end-to-end");
    std::string error;
    const auto binary = cpc::read_file(XCC_PROBE_PATH, error);
    test::check(binary.has_value(), "xcc produced a readable Z80 binary");
    if (!binary)
        return test::summary("xcc");

    cpc::machine target;
    target.write_port(0x7f00, 0x8c); // both ROM overlays off
    target.write_memory(0x8000, *binary);
    target.jump(0x8000);

    cpc::run_limits limits;
    limits.max_tstates = 2'000'000;
    limits.stop_on_halt = true;
    const cpc::run_result run = target.run(limits);
    test::check(run.reason == cpc::stop_reason::halted,
                "the xcc runtime returns to its HALT exit stub");
    test::check_eq(target.read_memory(0x9000), 55,
                   "compiled loop wrote 1+...+10 to CPC RAM");
    test::check(run.instructions > 10,
                "the result came from executing compiled instructions");
    return test::summary("xcc");
}
