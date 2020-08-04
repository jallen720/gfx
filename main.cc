// Tests
// #include "gfx/tests/default.h"
// #include "gfx/tests/stencil.h"
// #include "gfx/tests/input_attachments.h"
#include "gfx/tests/depth_peeling.h"

#define RUN(TEST) TEST##_test_main();

////////////////////////////////////////////////////////////
/// Main
////////////////////////////////////////////////////////////
s32 main() {
    RUN(depth_peeling)
}
