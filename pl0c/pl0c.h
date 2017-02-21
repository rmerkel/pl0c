/** @file pl0c.h
 *  
 *  A description of the PL/0C Machine operation codes, instruction format and
 *  some utilities used by both the compiler (PL0Comp) and the interpreter
 *  (PL0Interp).
 *
 *	Ported from p0com.p, the compiler/interpreter from Algorithms + Data
 *	Structures = Programs, Wirth.
 *
 * OpCode    | level? | addr?   | Notes
 * --------- | ------ | ------- | ---------------------------------------------
 * odd       |        |         | Unary: is top-of-stack odd?
 * neg       |        |         | Unary: negate the top-of-stack
 * add       |        |         | Binary addition
 * sub       |        |         | Binary subtraction
 * mul       |        |         | Binary multiplication
 * div       |        |         | Binary division
 * equ       |        |         | Binary is equal?
 * neq       |        |         | Binary is not equal?
 * lt        |        |         | Binary is less than?
 * lte       |        |         | Binary is less than or equal?
 * gt        |        |         | Binary is greater than?
 * gte       |        |         | Binary greater than or equal?
 * pushConst |        | value   | Push a constant value on the stack
 * pushVar   | Yes    | offset  | Read and then push a variable on the stack
 * pop       | Yes    | offset  | Pop and write a variable off of the stack
 * call      | Yes    | address | Call procedure with base(level)
 * ret       |        | offset  | Return from procedure, pop offset (args)
 * reti      |        | offset  | Return integer from function, pop args
 * enter     |        | offset  | Allocate locals on the stack (sp+=offset)
 * jump      |        | address | Jump to address
 * jneq      |        | address | Jump to address if top-of-stack == false (0)
 *
 * Notes:
 * - Unary - replace the top-of-stack with the result
 * - Binary - replace the top two items on the stack with the result.
 * - level/address - Effective address is base(level) + offset, where
 * base(level) is the base address n levels down the stack.
 */

#ifndef	PL0_H
#define PL0_H

#include <cstdint>
#include <string>
#include <vector>

/// The PL0 machine operation codes and instruction format.
namespace pl0c {
	/// Runtime block/stack frame layout
	enum Frame {
		base,								///< base(n)
		oldfp,								///< Saved frame pointer register
		rAddr,								///< Return from proc/func address
		rValue,								///< Function return value

		size								///< # of elements in the frame
	};

	/// Operation codes
	enum class OpCode : std::uint8_t {

		// Unary operations

		odd,								///< is top-of-stack odd?
		neg,								///< Negate the top-of-stack
												 
		// Binary operations

		add,								///< Addition
		sub,								///< Subtraction
		mul,								///< Multiplication
		div,								///< Division
		equ,								///< Is equal?
		neq,								///< Is not equal?
		lt,									///< Less than?
		lte,								///< Less then or equal?
		gt,									///< Greater than?
		gte,								///< Greater than or equal?
		
											// Push/pop

		pushConst,							///< Push a constant literal on the stack
		pushVar,							///< Read and push a variable on the stack
		pop,								///< Pop and write a variable off of the stack

		call,								///< Call a procedure, setting up a activation block (frame)
		ret,								///< Return from procedure
		reti,								///< Return from function
		enter,								///< Allocate locals on the stack
		jump,								///< Jump to a location
		jneq								///< Jump if top-of-stack == false (0), pop
	};

	std::string toString(OpCode op);		///< Return the name of the OpCode as a string

	typedef std::int32_t		Word;		///< A data word or address
	typedef std::vector<Word>	WordVector;	///< A vector of Words

	/// A Instruction
	struct Instr {
		struct {
			Word		addr;				///< Address or data value
			OpCode		op;					///< Operation code
			int8_t		level;				///< level: 0..255
		};

		/// Default constructor; results in pushConst 0, 0
		Instr() : addr{0}, op{OpCode::pushConst}, level{0}	{}

		/// Construct an instruction from it's components
		Instr(OpCode o, int8_t l = 0, Word a = 0) : addr{a}, op{o}, level{l} {}
	};

	/// A vector of Instr's
	typedef std::vector<Instr>	InstrVector;

	Word disasm(Word loc, const Instr& instr, const std::string label = "");
}

#endif