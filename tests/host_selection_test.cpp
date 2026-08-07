#include "WorkbenchHost.h"

#include <cassert>

int main() {
    using WorkbenchHost::Selection;
    // The policy must never permit a second renderer after any successful,
    // suppressed, or incompatible central-host handshake.
    assert(WorkbenchHost::ShouldInstallStandaloneRendererForSelection(
        Selection::Unavailable, true, false));
    assert(!WorkbenchHost::ShouldInstallStandaloneRendererForSelection(
        Selection::Unavailable, false, false));
    assert(!WorkbenchHost::ShouldInstallStandaloneRendererForSelection(
        Selection::Active, true, false));
    assert(!WorkbenchHost::ShouldInstallStandaloneRendererForSelection(
        Selection::Suppressed, true, false));
    assert(!WorkbenchHost::ShouldInstallStandaloneRendererForSelection(
        Selection::Incompatible, true, false));
    assert(!WorkbenchHost::ShouldInstallStandaloneRendererForSelection(
        Selection::Unavailable, true, true));
    return 0;
}
