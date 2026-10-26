// SPDX-License-Identifier: GPL-2.0-or-later
// Map numeric selectors to their encoded normalization values. These constants come from
// the original switch instructions; no format names or raw jump-table data are introduced.
#include <cstdint>

namespace {
constexpr uint32_t selector_not_mapped = 12;
}  // namespace

extern "C" uint32_t bb_render_record_translate_selector(uint32_t *mapping, uint32_t selector)
{
    uint32_t value;
    switch (selector) {
    case 0x0:
        value = 0x00000000; break;
    case 0x1: case 0x3:
        value = 0x00fac40e; break;
    case 0x2:
        value = 0x00fac70e; break;
    case 0x4:
        value = 0x00fac50e; break;
    case 0x5: case 0x7:
        value = 0x001ac40d; break;
    case 0x6:
        value = 0x003ac70d; break;
    case 0x8:
        value = 0x001ac50d; break;
    case 0x9: case 0xc:
        value = 0x00fac40c; break;
    case 0xa:
        value = 0x00fac70c; break;
    case 0xb:
        value = 0x00fac00c; break;
    case 0xd:
        value = 0x00fac10c; break;
    case 0xe:
        value = 0x00fac50c; break;
    case 0xf: case 0x11:
        value = 0x0002c40b; break;
    case 0x10:
        value = 0x0022c70b; break;
    case 0x12:
        value = 0x0002c50b; break;
    case 0x13: case 0x27: case 0x2a:
        value = 0x00004404; break;
    case 0x14: case 0x15: case 0x28: case 0x29:
        value = 0x00204704; break;
    case 0x16:
        value = 0x00020401; break;
    case 0x17: case 0x19:
        value = 0x00fac409; break;
    case 0x18:
        value = 0x00fac009; break;
    case 0x1a:
        value = 0x003ac706; break;
    case 0x1b: case 0x1e:
        value = 0x00fac40a; break;
    case 0x1c:
        value = 0x00fac00a; break;
    case 0x1d:
        value = 0x00fac90a; break;
    case 0x1f:
        value = 0x00fac10a; break;
    case 0x20:
        value = 0x00fac50a; break;
    case 0x21: case 0x24:
        value = 0x0002c405; break;
    case 0x22:
        value = 0x0022c705; break;
    case 0x23:
        value = 0x0022c005; break;
    case 0x25:
        value = 0x0022c105; break;
    case 0x26:
        value = 0x0002c505; break;
    case 0x2b:
        value = 0x00004504; break;
    case 0x30: case 0x32:
        value = 0x0002c403; break;
    case 0x31:
        value = 0x0022c003; break;
    case 0x33:
        value = 0x0022c103; break;
    case 0x34:
        value = 0x0002c503; break;
    case 0x35: case 0x39:
        value = 0x00004402; break;
    case 0x36:
        value = 0x00204702; break;
    case 0x37: case 0x38:
        value = 0x00204002; break;
    case 0x3a:
        value = 0x00204102; break;
    case 0x3b:
        value = 0x00004502; break;
    case 0x3c: case 0x3e:
        value = 0x00004401; break;
    case 0x3d:
        value = 0x00204001; break;
    case 0x3f:
        value = 0x00204101; break;
    case 0x40:
        value = 0x00004501; break;
    case 0x41:
        value = 0x00800001; break;
    case 0x46: case 0x47:
        value = 0x00fac023; break;
    case 0x48:
        value = 0x00fac923; break;
    case 0x49: case 0x4a:
        value = 0x00fac024; break;
    case 0x4b:
        value = 0x00fac924; break;
    case 0x4c: case 0x4d:
        value = 0x00fac025; break;
    case 0x4e:
        value = 0x00fac925; break;
    case 0x4f: case 0x50:
        value = 0x00204026; break;
    case 0x51:
        value = 0x00204126; break;
    case 0x52: case 0x53:
        value = 0x0022c027; break;
    case 0x54:
        value = 0x0022c127; break;
    case 0x55:
        value = 0x0032e010; break;
    case 0x56:
        value = 0x00f2e011; break;
    case 0x57:
        value = 0x00f2e00a; break;
    case 0x58:
        value = 0x0032e00a; break;
    case 0x5a: case 0x5b:
        value = 0x00f2e90a; break;
    case 0x5c: case 0x5d:
        value = 0x0032e90a; break;
    case 0x5e: case 0x5f:
        value = 0x003ac028; break;
    case 0x60:
        value = 0x003ac128; break;
    case 0x61: case 0x62:
        value = 0x00fac029; break;
    case 0x63:
        value = 0x00fac929; break;
    default:
        *mapping = 0;
        return selector_not_mapped;
    }
    *mapping = value;
    return 0;
}
