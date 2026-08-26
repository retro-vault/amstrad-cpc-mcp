//
// CDT reference-clock conversion and cassette transport behavior.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <vector>

#include "cpc/tape.h"
#include "test_support.h"

namespace {

void word(std::vector<cpc::u8> &out, cpc::u16 value)
{
    out.push_back(static_cast<cpc::u8>(value));
    out.push_back(static_cast<cpc::u8>(value >> 8));
}

std::vector<cpc::u8> cdt()
{
    return {'Z', 'X', 'T', 'a', 'p', 'e', '!', 0x1a, 1, 20};
}

void test_clock_conversion()
{
    test::section("CDT 3.5 MHz to CPC 4 MHz conversion");
    std::vector<cpc::u8> image = cdt();
    image.push_back(0x12);
    word(image, 7);
    word(image, 3);
    cpc::tape_deck deck;
    std::string error;
    test::check(deck.insert_cdt(image, true, error),
                "pure-tone CDT block loads");
    test::check_eq(deck.status().duration_tstates, 24,
                   "three 7-unit CDT pulses become three 8-T CPC pulses");
    test::check(!deck.ear_level(), "first pulse begins low");
    deck.tick(8);
    test::check(deck.ear_level(), "edge arrives after eight CPC T-states");
    deck.tick(16);
    test::check(deck.status().finished, "transport ends on the exact edge");
}

void test_pause_and_validation()
{
    test::section("CPC cassette pause and validation");
    std::vector<cpc::u8> image = cdt();
    image.push_back(0x20);
    word(image, 1);
    cpc::tape_deck deck;
    std::string error;
    test::check(deck.insert_cdt(image, true, error), "one-ms pause loads");
    test::check_eq(deck.status().duration_tstates, 4000,
                   "one millisecond is 4000 CPC T-states");

    const std::vector<cpc::u8> invalid = {'N', 'O'};
    test::check(!deck.insert_cdt(invalid, true, error),
                "invalid CDT is rejected");
    test::check_contains(error, "signature", "validation error is useful");
    test::check_eq(deck.status().duration_tstates, 4000,
                   "failed insert preserves the old tape");
}

} // namespace

int main()
{
    test_clock_conversion();
    test_pause_and_validation();
    return test::summary("tape");
}
