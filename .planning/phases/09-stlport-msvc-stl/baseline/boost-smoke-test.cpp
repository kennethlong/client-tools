// Phase 9 Wave 1 - boost-subset MSVC compatibility smoke test (D-07).
// Compiled standalone with cl /c against MSVC v145; success = D-07 satisfied.
//
// Out-of-scope for the Phase 9 build graph; this file is NEVER added to any
// CMakeLists.txt - it is hand-compiled in Wave 1 only.
//
// Build flags (must mirror the real Phase 9 client build):
//   /std:c++17 /MT /EHsc /D_HAS_AUTO_PTR_ETC=1 /I <boost-subset>
//
// _HAS_AUTO_PTR_ETC=1 is REQUIRED: the vendored boost/smart_ptr.hpp at lines
// 150/164/178/183 references std::auto_ptr (deprecated/removed in C++17).
// Phase 9 keeps std::auto_ptr available via this MSVC opt-in macro; see
// .planning/phases/09-stlport-msvc-stl/baseline/wave1-auto-ptr-usage.txt
// for the rationale + the 30 auto_ptr usage sites in the active build graph.

#include "boost/config.hpp"
#include "boost/smart_ptr.hpp"
#include "boost/static_assert.hpp"
#include "boost/utility.hpp"

BOOST_STATIC_ASSERT(sizeof(int) == 4);

struct NonCopyableThing : private boost::noncopyable {
    int value;
    NonCopyableThing() : value(42) {}
};

int main() {
    boost::shared_ptr<int> sp(new int(7));
    boost::scoped_ptr<NonCopyableThing> nc(new NonCopyableThing());
    return *sp + nc->value;  // 49
}
