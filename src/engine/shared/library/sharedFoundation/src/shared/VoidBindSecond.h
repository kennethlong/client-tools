// ======================================================================
//
// VoidBindSecond.h
// copyright 2001 Sony Online Entertainment
// 
// ======================================================================

#include <functional>
#include <cstdint>
#include <type_traits>

// ======================================================================
// Phase 31 (BITS-02, B-GAP-2): width-correct bind-argument conversion.
//
// VoidBindSecond is a SHARED TEMPLATE HEADER. One instantiation binds an
// integer literal (e.g. `VoidBindSecond(..., 0)` for a `T*` second argument --
// EditableAnimationState.cpp). The old `SecondArgumentType(bindArgument)`
// functional cast is an int->pointer reinterpret that widens a 32-bit int to a
// 64-bit pointer on x64 (C4312). The helper below routes an integral->pointer
// bind through uintptr_t (width-correct, preserves the null/value), and is a
// plain forwarding cast for every other (non-pointer / matching) case.
//
// SHARED-HEADER / ABI-CASCADE NOTE (AGENTS.md): this header is consumed by many
// libs and all 4 renderer plugins. The change is to a template's internal
// conversion only (no struct layout change), but plan 31-06 Task 2's full
// 5-target build still recompiles ALL consumers -- no stale-plugin ABI risk.

namespace VoidBindSecondNamespace
{
	template <class Target, class Source>
	inline Target convertBindArgument(const Source &source)
	{
		if constexpr (std::is_pointer_v<Target> && std::is_integral_v<Source>)
		{
			// integral -> pointer: go through uintptr_t so a 32-bit literal is
			// not truncated/sign-extended differently than the pointer width.
			return reinterpret_cast<Target>(static_cast<uintptr_t>(source));
		}
		else
		{
			return Target(source);
		}
	}
}

template <class Operation>
class VoidBinderSecond
{
private:
	using FirstArgType = typename Operation::first_argument_type;
	using SecondArgType = typename Operation::second_argument_type;

	SecondArgType  m_boundValue;
	Operation      m_operation;

public:
	VoidBinderSecond(const Operation& operation, const SecondArgType& boundValue)
		: m_operation(operation), m_boundValue(boundValue)
	{
	}

	void operator()(const FirstArgType& argument) const
	{
		m_operation(argument, m_boundValue);
	}
};

// ======================================================================
/**
 * works just like std::bind2nd() when return value of function is void.
 *
 * this is necessary since our STL breaks under MSVC 6 when using void functions
 * with bind2nd.
 */


template <class Operation, typename BindArgumentType>
inline VoidBinderSecond<Operation> VoidBindSecond(const Operation& operation, const BindArgumentType &bindArgument) 
{
  typedef typename Operation::second_argument_type  SecondArgumentType;

  return VoidBinderSecond<Operation>(operation, VoidBindSecondNamespace::convertBindArgument<SecondArgumentType>(bindArgument));
}

// ======================================================================
