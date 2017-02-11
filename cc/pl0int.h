/** @file pl0int.h
 *
 *	PL/0 interpreter in C++
 *
 *	Ported from p0com.p, from Algorithms + Data Structions = Programes. Changes include
 *	- index the stack (stack[0..maxstack-1]); thus the initial values for the t and b registers are
 *    -1 and 0.
 *  - replaced single letter variables, e.g., p is now pc.
 *
 *	Currently loads and runs a sample/test program, dumping machine state before each instruction
 *	fetch.
 */

#ifndef	PL0INT_H
#define PL0INT_H

#include <cstdint>
#include <vector>

#include "pl0.h"

/// A pl/0 machine...
class PL0Interp {
public:
	PL0Interp(std::size_t stacksz = 512);
	virtual ~PL0Interp() {}

	std::size_t operator()(const pl0::InstrVector& program, bool v = false);
	void reset();

private:
	/// A Effective Address that maybe invalidated
	class EAddr {
		std::size_t	eaddr;					///< The effective address
		bool		val;					///< Is eaddr valid?

	public:
		EAddr() : eaddr{0}, val{false} {}
		operator std::size_t() const		{	return eaddr;	}
		EAddr& operator=(std::size_t n) {
			eaddr = n;
			val = true;
			return *this;
		}
		bool valid() const					{	return val;		}
		void invalidate() 					{	val = false;	}
	};

	pl0::InstrVector	code;				///< Code segment, indexed by pc
	pl0::WordVector		stack;				///< Data segment (the stack), index by bp and sp
	std::size_t			pc;					///< Program counter register; index of *next* instruction in code[]
	int					bp;					///< Base pointer register; index of the current mark block/frame in stack[]
	int					sp;					///< Top of stack register (stack[sp])
	pl0::Instr			ir;					///< *Current* instruction register (code[pc-1])

	EAddr				lastWrite;			///< Last write effective address (to stack[]), if valid
	bool				verbose;			///< Verbose output if true

	void dump();

protected:
	uint16_t base(uint16_t lvl);
	std::size_t run();
};

#endif