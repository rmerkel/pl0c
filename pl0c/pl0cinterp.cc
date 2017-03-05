/** @file pl0cinterp.cc
 *
 * PL/0 interpreter in C++
 *   
 * @author Randy Merkel, Slowly but Surly Software. 
 * @copyright  (c) 2017 Slowly but Surly Software. All rights reserved.
 */

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include "pl0cinterp.h"

using namespace std;

namespace pl0c {
	// private:

	/// Dump the current machine state
	void Interp::dump() {
		// Dump the last write
		if (lastWrite.valid())
			cout << "    " << setw(5) << lastWrite << ": " << setw(10) << stack[lastWrite] << std::endl;
		lastWrite.invalidate();

		if (!verbose) return;

		// Dump the current  activation frame...
		if (sp == -1) {								// Happens after return from the main procedure
			cout 	 <<	"sp:    "         << sp << ": " << right << setw(10) << stack[sp] << endl;
			cout 	 <<	"bp: " << setw(5) << bp << ": " << right << setw(10) << stack[bp] << endl;

		} else {
			assert(sp >= bp);
			cout     << "bp: " << setw(5) << bp << ": " << right << setw(10) << stack[bp] << endl;
			for (int bl = bp+1; bl < sp; ++bl)
				cout <<	"    " << setw(5) << bl << ": " << right << setw(10) << stack[bl] << endl;
			cout     << "sp: " << setw(5) << sp << ": " << right << setw(10) << stack[sp] << endl;
		}

		disasm(cout, pc, code[pc], "pc");

		cout << endl;
	}

	// protected:

	/**
	 * @param lvl Number of levels down
	 * @return The base, lvl's down the stack
	 */ 
	uint16_t Interp::base(uint16_t lvl) {
		uint16_t b = bp;
		for (; lvl > 0; --lvl)
			b = stack[b];

		return b;
	}

	/**
	 * Unlinks the stack frame, setting the return address as the next instruciton.
	 */
	void Interp::ret() {
		sp = bp - 1; 					// "pop" the activaction frame
		pc = stack[bp + FrameRetAddr];
		bp = stack[bp + FrameOldBp];
		sp -= ir.addr;					// Pop parameters, if any...
	}

	/**
	 *  @return	the number of machine cycles run
	 */
	size_t Interp::run() {
		size_t cycles = 0;						// # of instructions run to date

		if (verbose)
			cout << "Reg  Addr Value/Instr\n"
				 << "---------------------\n";

		do {
			assert(pc < code.size());
			// we assume that the stack size is less than numberic_limits<int>::max()!
			if (-1 != sp) assert(sp < static_cast<int>(stack.size()));


			dump();								// Dump state and disasm the next instruction
			ir = code[pc++];					// Fetch next instruction...
			++cycles;

			switch(ir.op) {

			// unary operations

			case OpCode::Not:		stack[sp] = !stack[sp];						break;
			case OpCode::neg:		stack[sp] = -stack[sp];						break;
			case OpCode::comp:		stack[sp] = ~stack[sp];						break;

			// binrary operations

			case OpCode::add:		--sp; stack[sp] = stack[sp] + stack[sp+1];	break;
			case OpCode::sub:		--sp; stack[sp] = stack[sp] - stack[sp+1];	break;
			case OpCode::mul:		--sp; stack[sp] = stack[sp] * stack[sp+1];	break;
			case OpCode::div:		--sp; stack[sp] = stack[sp] / stack[sp+1];	break;
			case OpCode::rem: 		--sp; stack[sp] = stack[sp] % stack[sp+1];	break;

			case OpCode::bor:		--sp; stack[sp] = stack[sp] | stack[sp+1];	break;
			case OpCode::band:		--sp; stack[sp] = stack[sp] && stack[sp+1];	break;
			case OpCode::bxor:		--sp; stack[sp] = stack[sp] ^ stack[sp+1];	break;

			case OpCode::lshift:	--sp; stack[sp] = stack[sp] << stack[sp+1];	break;
			case OpCode::rshift:	--sp; stack[sp] = stack[sp] >> stack[sp+1];	break;

			case OpCode::equ:		--sp; stack[sp] = stack[sp] == stack[sp+1];	break;
			case OpCode::neq:		--sp; stack[sp] = stack[sp] != stack[sp+1];	break;
			case OpCode::lt:		--sp; stack[sp] = stack[sp]  < stack[sp+1];	break;
			case OpCode::gte:		--sp; stack[sp] = stack[sp] >= stack[sp+1];	break;
			case OpCode::gt:		--sp; stack[sp] = stack[sp]  > stack[sp+1];	break;
			case OpCode::lte:		--sp; stack[sp] = stack[sp] <= stack[sp+1];	break;
			case OpCode::lor:		--sp; stack[sp] = stack[sp] || stack[sp+1];	break;
			case OpCode::land:		--sp; stack[sp] = stack[sp] && stack[sp+1];	break;

			// push/pop..

			case OpCode::pushConst:
				stack[++sp] = ir.addr;
				break;

			case OpCode::pushVar:
				stack[++sp] = base(ir.level) + ir.addr;
				break;

			case OpCode::eval: {
					auto ea = stack[sp];
					stack[sp] = stack[ea];
				}
				break;

			case OpCode::assign:
				lastWrite = stack[sp--];		// Save the effective address for dump()
				stack[lastWrite] = stack[sp--];
				break;

			// control flow...

			case OpCode::call: 					// Call a subroutine
				// Push a new activation frame block on the stack
				stack[sp + 1 + FrameBase] 		= base(ir.level);
				stack[sp + 1 + FrameOldBp] 		= bp;
				stack[sp + 1 + FrameRetAddr]	= pc;
				stack[sp + 1 + FrameRetVal] 	= 0;
				bp = sp + 1;					// Points to start of the frame
				sp += FrameSize;				// Points to the return value...
				pc = ir.addr;					// Set the subroutine'saddress
				break;

			case OpCode::ret: 					// Return from a procedure
				ret();
				break;

			case OpCode::reti: {				// Return integer from function
												// Save the function result...
					auto temp = stack[bp + FrameRetVal];
					ret();						// Unlink the stack frame...
					stack[++sp] = temp;			// Push the result
				}
				break;


			case OpCode::enter:	sp += ir.addr;								break;
			case OpCode::jump:	pc = ir.addr;								break;
			case OpCode::jneq:	if (stack[sp--] == 0) pc = ir.addr;			break;

			default:
				cerr << "Unknown op code: " << toString(ir.op) << endl;
				assert(false);
			};

		} while (pc != 0);
		dump();						// Dump the exit state

		return cycles;
	}

	//public

	/** 
	 *  @param	stacksz	Maximum depth of the data segment/stack, in machine Words
	 */
	Interp::Interp(size_t stacksz) : stack(stacksz), verbose{false} {
		reset();
	}

	/**
	 *	@param	program	The program to run
	 *	@param 	ver		True for verbose/debugging messages
	 *  @return	The number of machine cycles run
	 */
	size_t Interp::operator()(const InstrVector& program, bool ver) {
		verbose = ver;

		// Fill the stack with -1s for debugging...
		fill(stack.begin(), stack.end(), -1);

		code = program;
		reset();
		return run();
	}

	void Interp::reset() {
		pc = 0;

		// Set up the initial mark block/frame...
		stack[0] = stack[1] = stack[2] = stack[3] = 0;
		bp = 0; sp = 3;
	}
}

