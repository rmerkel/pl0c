/** @file pl0int.cc
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

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include "pl0int.h"

using namespace std;
using namespace pl0;

// private:

/// Dump the current machine state
void PL0Interp::dump() {
	// Dump the last write
	if (lastWrite.valid())
		cout << "    " << setw(5) << lastWrite << ": " << stack[lastWrite] << std::endl;
	lastWrite.invalidate();

	if (!verbose) return;

	// Dump the current mark block (frame)...
	if (sp >= bp) {						// Block/frame established?
		cout <<		"bp: " << setw(5) << bp << ": " << stack[bp] << endl;
		for (int bl = bp+1; bl < sp; ++bl)
			cout <<	"    " << setw(5) << bl << ": " << stack[bl] << endl;

		if (sp != -1)
			cout <<	"sp: " << setw(5) << sp << ": " << stack[sp] << endl;
		else
			cout <<	"sp: " << setw(5) << sp << endl;

	} else {							// Procedure hasn't called 'int' yet
		if (sp != -1)
			cout <<	"sp: " << setw(5) << sp << ": " << stack[sp] << endl;

		else
			cout <<	"sp: " << setw(5) << sp << endl;
		cout <<		"bp: " << setw(5) << bp << ": " << stack[bp] << endl;
	}

	disasm("pc", pc, code[pc]);

	cout << endl;
}

// protected:

/// Find the base 'lvl' levels (frames) up the stack
uint16_t PL0Interp::base(uint16_t lvl) {
	uint16_t b = bp;
	for (; lvl > 0; --lvl)
		b = stack[b];

	return b;
 }

/** Run the machine from it's current state
 *  @return	the number of machine cycles run
 */
size_t PL0Interp::run() {
	size_t cycles = 0;							// # of instrucitons run to date

	if (verbose)
		cout << "Reg  Addr Value/Instr\n"
			 << "---------------------\n";

  	do {
		assert(pc < code.size());
		// we assume that the stack size is less than numberic_limits<int>::max()!
		if (-1 != sp) assert(sp < static_cast<int>(stack.size()));


		dump();									// Dump state and disasm the next instruction
		ir = code[pc++];						// Fetch next instruction...
		++cycles;

    	switch(ir.op) {
        case OpCode::pushConst:	stack[++sp] = ir.addr;							break;
    	case OpCode::Return: 							// return
    		sp = bp - 1; pc = stack[sp + 3]; bp = stack[sp + 2];
    		break;

		case OpCode::Neg:		stack[sp] = -stack[sp];							break;
    	case OpCode::Add:		--sp; stack[sp] = stack[sp] + stack[sp+1];		break;
    	case OpCode::Sub:		--sp; stack[sp] = stack[sp] - stack[sp+1];		break;
    	case OpCode::Mul:		--sp; stack[sp] = stack[sp] * stack[sp+1];		break;
    	case OpCode::Div:		--sp; stack[sp] = stack[sp] / stack[sp+1];		break;
		case OpCode::Odd:		stack[sp] = stack[sp] & 1;						break;
    	case OpCode::Equ:		--sp; stack[sp] = stack[sp] == stack[sp+1];		break;
    	case OpCode::Neq:		--sp; stack[sp] = stack[sp] != stack[sp+1];		break;
    	case OpCode::LT:		--sp; stack[sp] = stack[sp]  < stack[sp+1];		break;
    	case OpCode::GTE:		--sp; stack[sp] = stack[sp] >= stack[sp+1];		break;
    	case OpCode::GT:		--sp; stack[sp] = stack[sp]  > stack[sp+1];		break;
    	case OpCode::LTE:		--sp; stack[sp] = stack[sp] <= stack[sp+1];		break;
        case OpCode::pushVar:	stack[++sp] = stack[base(ir.level) + ir.addr];	break;
        case OpCode::Pop:
			lastWrite = base(ir.level) + ir.addr;// Save the effective address for dump()
			stack[lastWrite] = stack[sp--];
			break;

        case OpCode::Call: 								// Call a procedure
			// Create a enw frame/mark block that the bp register points to
        	stack[sp+1] = base(ir.level);
			stack[sp+2] = bp;
			stack[sp+3] = pc;
        	bp = sp + 1;
			pc = ir.addr;
			break;

        case OpCode::Enter:		sp += ir.addr;									break;
        case OpCode::Jump:		pc = ir.addr;									break;
        case OpCode::Jne:		if (stack[sp--] == 0) pc = ir.addr;				break;
		default:
			assert(false);
		};

	} while (pc != 0);

	return cycles;
}

//public

/** Default constructor
 *
 *  @param	stacksz	Maximum depth of the data segment/stack, in machine Words
 */
PL0Interp::PL0Interp(size_t stacksz) : stack(stacksz), verbose{false} {
	reset();
}

/** Load a applicaton and start the pl/0 machine running...
 *
 *	@param	program	The program to load and run
 *	@param 	ver		True for verbose debugging messages
 *  @return	The number of machine cycles run
 */
size_t PL0Interp::operator()(const InstrVector& program, bool ver) {
	verbose = ver;

	// Fill the stack with -1s for debugging...
	fill(stack.begin(), stack.end(), -1);

	code = program;
	reset();
	return run();
}

/// Reset the machine back to it's initial state.
void PL0Interp::reset() {
	pc = 0, bp = 0, sp = -1;	// Set up the initial mark block/frame...
	stack[0] = stack[1] = stack[2] = 0;
}
