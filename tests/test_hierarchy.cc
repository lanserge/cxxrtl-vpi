// SPDX-License-Identifier: ISC
//
// Hierarchy-discovery test: exercises the calls cocotb makes at startup to find
// the toplevel and walk its signals — vpi_iterate(vpiModule, NULL) for the root,
// vpi_iterate(vpiNet, root)/vpi_scan for its children, and name resolution both
// qualified ("counter.count") and unqualified ("count").

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include <vpi_user.h>

#include "cxxrtl_vpi/model.h"
#include "cxxrtl_vpi/sim.h"

int main() {
    cxxrtl_vpi::Model model;
    model.create();
    model.set_top_name("counter");
    cxxrtl_vpi::vpi_provider_bind(&model);

    // 1. Root module discovery.
    vpiHandle mod_it = vpi_iterate(vpiModule, nullptr);
    if (!mod_it) {
        printf("FAIL: vpi_iterate(vpiModule, NULL) returned null\n");
        return 1;
    }
    vpiHandle root = vpi_scan(mod_it);
    if (!root) {
        printf("FAIL: no root module from scan\n");
        return 1;
    }
    printf("root module: name=%s type=%d\n", vpi_get_str(vpiName, root),
           vpi_get(vpiType, root));
    if (std::strcmp(vpi_get_str(vpiName, root), "counter") != 0 ||
        vpi_get(vpiType, root) != vpiModule) {
        printf("FAIL: root module name/type wrong\n");
        return 1;
    }

    // 2. Iterate the toplevel's signals across nets and regs (cocotb unions
    //    both). `count` is a flop output, so it must classify as a reg.
    std::set<std::string> nets, regs, found;
    for (PLI_INT32 ty : {vpiNet, vpiReg}) {
        vpiHandle it = vpi_iterate(ty, root);
        if (!it) {
            printf("FAIL: vpi_iterate returned null for type %d\n", ty);
            return 1;
        }
        while (vpiHandle s = vpi_scan(it)) {
            std::string n = vpi_get_str(vpiName, s);
            found.insert(n);
            (ty == vpiReg ? regs : nets).insert(n);
            vpi_free_object(s);
        }
    }
    printf("nets:");
    for (const auto &n : nets) printf(" %s", n.c_str());
    printf("  regs:");
    for (const auto &n : regs) printf(" %s", n.c_str());
    printf("\n");

    std::set<std::string> expect{"clk", "count", "rst"};
    if (found != expect) {
        printf("FAIL: signal set mismatch\n");
        return 1;
    }
    if (regs.count("count") == 0 || nets.count("count") != 0) {
        printf("FAIL: 'count' should classify as a reg (flop output)\n");
        return 1;
    }

    // 3. Name resolution, qualified and unqualified, with full-name reporting.
    vpiHandle a = vpi_handle_by_name((PLI_BYTE8 *)"counter.count", nullptr);
    vpiHandle b = vpi_handle_by_name((PLI_BYTE8 *)"count", nullptr);
    if (!a || !b) {
        printf("FAIL: could not resolve qualified/unqualified name\n");
        return 1;
    }
    printf("fullname(count) = %s\n", vpi_get_str(vpiFullName, a));
    if (std::strcmp(vpi_get_str(vpiFullName, a), "counter.count") != 0) {
        printf("FAIL: vpiFullName wrong\n");
        return 1;
    }

    // 4. Parent navigation: a signal's parent is its module; the top has none.
    vpiHandle parent = vpi_handle(vpiParent, a);
    if (!parent || std::strcmp(vpi_get_str(vpiName, parent), "counter") != 0) {
        printf("FAIL: vpi_handle(vpiParent, count) wrong\n");
        return 1;
    }
    if (vpi_handle(vpiParent, root) != nullptr) {
        printf("FAIL: toplevel should have no parent\n");
        return 1;
    }
    printf("parent(count) = %s\n", vpi_get_str(vpiName, parent));
    vpi_free_object(parent);
    vpi_free_object(a);
    vpi_free_object(b);
    vpi_free_object(root);

    printf("SUCCESS\n");
    return 0;
}
