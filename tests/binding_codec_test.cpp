#include "BindingCodec.h"

#include <cassert>

int main() {
    HotkeySettings parsed;
    assert(BindingCodec::ParseKeyboard(
        "keyboard:0x48;ctrl=1;alt=1;shift=0", parsed));
    assert(parsed.virtualKey == 0x48 && parsed.control && parsed.alt && !parsed.shift);
    assert(BindingCodec::EncodeKeyboard(parsed) ==
        "keyboard:0x48;ctrl=1;alt=1;shift=0");
    assert(BindingCodec::Display(parsed) == "Ctrl + Alt + H");
    assert(!BindingCodec::ParseKeyboard(
        "keyboard:0x100;ctrl=0;alt=0;shift=0", parsed));
    assert(!BindingCodec::ParseKeyboard(
        "keyboard:0x48;ctrl=2;alt=0;shift=0", parsed));
    assert(BindingCodec::ParseKeyboard({}, parsed));
    assert(parsed.virtualKey == 0 && BindingCodec::Display(parsed) == "Unbound");
    return 0;
}
