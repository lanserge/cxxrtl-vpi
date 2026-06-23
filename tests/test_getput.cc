// SPDX-License-Identifier: ISC
//
// MVP functional test for the VPI object-access surface, exercised directly
// (no cocotb yet): resolve handles by name, read with vpi_get/vpi_get_value,
// drive inputs with vpi_put_value, and check a counter actually counts.
//
// Clock stepping is done via the Model (cxxrtl time advance is the next VPI
// stage — callbacks); everything signal-facing goes through the VPI API.

#include <cstdio>
#include <cstring>

#include <vpi_user.h>

#include "cxxrtl_vpi/model.h"

namespace cxxrtl_vpi {
void vpi_provider_bind(Model *model);
}

static void put_int(vpiHandle h, int v) {
    s_vpi_value val;
    val.format = vpiIntVal;
    val.value.integer = v;
    vpi_put_value(h, &val, nullptr, vpiNoDelay);
}

static int get_int(vpiHandle h) {
    s_vpi_value val;
    val.format = vpiIntVal;
    vpi_get_value(h, &val);
    return val.value.integer;
}

int main() {
    cxxrtl_vpi::Model model;
    model.create();
    cxxrtl_vpi::vpi_provider_bind(&model);

    printf("== discovered signals ==\n");
    for (const auto &s : model.signals())
        printf("  %-20s width=%zu\n", s.name.c_str(), s.width);

    vpiHandle clk = vpi_handle_by_name((PLI_BYTE8 *)"clk", nullptr);
    vpiHandle rst = vpi_handle_by_name((PLI_BYTE8 *)"rst", nullptr);
    vpiHandle count = vpi_handle_by_name((PLI_BYTE8 *)"count", nullptr);
    vpiHandle bogus = vpi_handle_by_name((PLI_BYTE8 *)"does_not_exist", nullptr);

    if (!clk || !rst || !count) {
        printf("FAIL: could not resolve clk/rst/count\n");
        return 1;
    }
    if (bogus) {
        printf("FAIL: unknown name resolved to a non-null handle\n");
        return 1;
    }

    printf("== metadata ==\n");
    printf("  vpi_get(vpiSize, count) = %d (expect 8)\n", vpi_get(vpiSize, count));
    printf("  vpi_get_str(vpiName, count) = %s\n", vpi_get_str(vpiName, count));
    if (vpi_get(vpiSize, count) != 8) {
        printf("FAIL: wrong width\n");
        return 1;
    }
    if (std::strcmp(vpi_get_str(vpiName, count), "count") != 0) {
        printf("FAIL: wrong name\n");
        return 1;
    }

    // One clock cycle = falling then rising edge of clk.
    auto tick = [&]() {
        put_int(clk, 0);
        model.step();
        put_int(clk, 1);
        model.step();
    };

    // Reset.
    put_int(rst, 1);
    tick();
    tick();
    put_int(rst, 0);

    printf("== counting ==\n");
    int prev = -1, fails = 0;
    for (int i = 0; i < 6; i++) {
        tick();
        int c = get_int(count);
        printf("  cycle %d: count=%d\n", i, c);
        if (prev >= 0 && c != ((prev + 1) & 0xff))
            fails++;
        prev = c;
    }

    // Exercise the string + vector read formats too.
    s_vpi_value bin;
    bin.format = vpiBinStrVal;
    vpi_get_value(count, &bin);
    printf("  count (binstr) = %s\n", bin.value.str);

    s_vpi_value vec;
    vec.format = vpiVectorVal;
    vpi_get_value(count, &vec);
    printf("  count (vector aval) = %u\n", vec.value.vector[0].aval);

    vpi_free_object(clk);
    vpi_free_object(rst);
    vpi_free_object(count);

    if (fails) {
        printf("FAIL: %d non-incrementing cycles\n", fails);
        return 1;
    }
    printf("SUCCESS\n");
    return 0;
}
