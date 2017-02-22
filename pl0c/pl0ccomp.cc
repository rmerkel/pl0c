/** @file pl0ccomp.cc
 *  
 * PL0C Compiler implementation
 */

#include "pl0ccomp.h"
#include "pl0cinterp.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <limits>
#include <vector>

using namespace std;
using namespace pl0c;

// protected:

/** Write a error message
 *
 * Writes a diagnostic onto standard error output, incrementing the error count. 
 *
 * @param s The error message
 */
void PL0CComp::error(const string& s) {
	cerr << progName << ": " << s << " near line " << ts.lineNum << endl;
	++nErrors;
}

/** Write a error message
 *
 * Writes a diagnostic, in the form "s 't'", onto standard error output, incrementing the error
 * count.
 *
 * @param s The error message
 * @param t Parameter to s.
 */
void PL0CComp::error(const string& s, const string& t) {
	error (s + " \'" + t + "\'");
}

/** Read and return the next token from the token stream
 *  
 * @return The next token from the token stream 
 */ 
Token PL0CComp::next() {
	Token t = ts.get();

	if (verbose)
		cout << progName << ": getting '" << Token::toString(t.kind) << "', " << t.string_value << ", " << t.number_value << "\n";

	return t;
}

/** Accept the next token
 *
 * Returns true, and optionally consumes the current token, if it's type is equal to kind.
 *
 * @param	kind	The token Kind to accept.
 * @param	get		Consume the token, if true. Defaults true.
 *
 * @return	true if the current token is a kind.
 */
bool PL0CComp::accept(Token::Kind kind, bool get) {
	if (current() == kind) {
		if (get) next();			// consume the token
		return true;
	}

	return false;
}

/** Expect the next token
 *
 * Evaluate and return accept(kind, get). Generate an error if accept() returns false.
 *
 * @param	kind	The token Kind to accept.
 * @param	get		Consume the token, if true. Defaults true.
 *
 * @return	true if the current token is a k.
 */
bool PL0CComp::expect(Token::Kind kind, bool get) {
	if (accept(kind, get)) return true;

	error("expected", Token::toString(kind) + "\' got \'" + Token::toString(current()));
	return false;
}

/** Emit an instruction
 *
 *	Appends (op, level, addr) on code, and returns it's address.
 *
 *	@param	op		The pl0 instruction operation code
 *	@param	level	The pl0 instruction block level value. Defaults to zero.
 *	@param	addr	The pl0 instructions address/value. Defaults to zero.
 *
 *	@return The address (code[] index) of the new instruction.
 */
size_t PL0CComp::emit(const OpCode op, int8_t level, Word addr) {
	if (verbose)
		cout << progName << ": emitting " << code->size() << ": "
				<< toString(op) << " " << static_cast<int>(level) << ", " << addr
				<< "\n";

	code->push_back({op, level, addr});
	return code->size() - 1;				// so it's the address of just emitted instruction
}

/** Factor identifier
 *
 * Push a variable or a constant value, or invoke, and push the results, of a
 * function.
 *
 * ident | ident "(" [ ident { "," ident } ] ")"
 *
 * @param	level	The current block level
 */
void PL0CComp::identifier(int level) {
	const string name = ts.current().string_value;
	next();								// Consume the identifier

	auto range = symtbl.equal_range(name);
	if (range.first == range.second)
		error("Undefined identifier", name);

	else {
		auto closest = range.first;
		for (auto it = range.first; it != range.second; ++it)
			if (it->second.level > closest->second.level)
				closest = it;

		if (SymValue::constant == closest->second.kind)
			emit(OpCode::pushConst, 0, closest->second.value);

		else if (SymValue::identifier == closest->second.kind)
			emit(OpCode::pushVar, level - closest->second.level, closest->second.value);

		else if (SymValue::function == closest->second.kind) {
			expect(Token::lparen);
			if (!accept(Token::rparen, false)) {
				do
					expression(level);
				while (accept(Token::comma));
			}
			expect(Token::rparen);
			emit(OpCode::call, level - closest->second.level, closest->second.value);

		} else
			error("Unknown symbol", name);
	}
}

/** Factor
 *
 * factor = ident | number | "{" expression "}" ;
 *
 * @param	level	The current block level.
 */
void PL0CComp::factor(int level) {
	if (accept(Token::identifier, false))
		identifier(level);

	else if (accept(Token::number, false)) {
		emit(OpCode::pushConst, 0, ts.current().number_value);
		expect(Token::number);

	} else if (accept(Token::lparen)) {
		expression(level);
		expect(Token::rparen);

	} else {
		error("factor: syntax error; expected ident | num | { expr }, but got:",
			Token::toString(current()));
		next();
	}
}

/** Terminal
 *
 * term = fact { (*|/) fact } ;
 *
 * @param	level	The current block level.
 */
void PL0CComp::terminal(int level) {
	factor(level);

	for (Token::Kind k = current(); k == Token::mul || k == Token::div; k = current()) {
		next();							// consume the token

		factor(level);
		Token::mul == k ? emit(OpCode::mul) : emit(OpCode::div);
	}
}

/** Expression
 *
 * expr = [ +|-] term { (+|-) term } ;
 *
 * @param	level	The current block level.
 */
void PL0CComp::expression(int level) {
	const Token::Kind unary = current();	// unary [ +|-]?
	if (unary == Token::add || unary == Token::sub)
		next();

	terminal(level);
	if (Token::sub == unary)
		emit(OpCode::neg);				// ignore unary +!

	for (Token::Kind k = current(); k == Token::add || k == Token::sub; k = current()) {
		next();							// consume the token

		terminal(level);
		Token::add == k ? emit(OpCode::add) : emit(OpCode::sub);
	}
}

/** Condition
 *
 * cond =  "odd" expr | expr ("="|"!="|"<"|"<="|">"|">=") expr ;
 *
 * @param	level	The current block level.
 */
void PL0CComp::condition(int level) {
	if (accept(Token::odd)) {
		expression(level);
		emit(OpCode::odd);

	} else {
		expression(level);
		const Token::Kind op = current();

		if (accept(Token::lte)		||
			accept(Token::lt)		||
			accept(Token::equ)		||
			accept(Token::gt)		||
			accept(Token::gte)		||
			accept(Token::neq))
		{
			expression(level);

			switch(op) {
			case Token::lte:		emit(OpCode::lte);	break;
			case Token::lt:			emit(OpCode::lt);	break;
			case Token::equ:		emit(OpCode::equ);	break;
			case Token::gt:			emit(OpCode::gt);	break;
			case Token::gte:		emit(OpCode::gte);	break;
			case Token::neq:		emit(OpCode::neq);	break;
			default:			assert(false);		// can't get here!
			}
		}
	}
}

/** Assignment statement
 *
 * ident "=" expression
 *
 * @param	name	The identifier value
 * @param	val		The identifiers symbol table entry value
 * @param	level	The current block level.
 */
void PL0CComp::assignStmt(const string& name, const SymValue& val, int level) {
	expression(level);

	switch(val.kind) {
	case SymValue::identifier:
		emit(OpCode::pop, level - val.level, val.value);
		break;

	case SymValue::function:
		emit(OpCode::pop, 0, rValue);
		break;

	case SymValue::constant:
		error("Can't assign to a constant", name);
		break;

	case SymValue::proc:
		error("Can't assign to a procedure", name);
		break;

	default:
		assert(false);
	}
}

/** Call statement
 *  
 *  "call" ident "(" [ expr  { "," expr }] ")"...
 *
 * @param	name	The identifier value
 * @param	val		The identifiers symbol table entry value
 * @param	level	The current block level
 */ 
void PL0CComp::callStmt(const string& name, const SymValue& val, int level) {
	if (!accept(Token::rparen, false))
		do {									// [expr {, expr }]
			expression(level);
		} while (accept (Token::comma));

	expect(Token::rparen);

	if (SymValue::proc != val.kind)
		error("Identifier is not a procedure", name);
	else
		emit(OpCode::call, level - val.level, val.value);
}

/** Identifier statement
 *
 * ident "=" expr | ident "(" [ ident { "," ident } ] ")"
 *
 * @param	level	The current block level
 */
void PL0CComp::identStmt(int level) {
	// Save the identifier string before consuming it
	const string name = ts.current().string_value;
	next();

	auto range = symtbl.equal_range(name);		// look it up....
	if (range.first == range.second)
		error("undefined variable", name);

	else {
		auto closest = range.first;				// Find the "closest" entry
		for (auto it = range.first; it != range.second; ++it)
			if (it->second.level > closest->second.level)
				closest = it;

		if (accept(Token::assign))			// ident "=" expression
			assignStmt(closest->first, closest->second, level);

		else if (accept(Token::lparen))		// proc "(" ... ")"
			callStmt(closest->first, closest->second, level);

		else
			error("identifier is not a variable or a procedure", name);
	}
}


/** While statement
 *
 * "while" condition "do" statement...
 *
 * @param	level	The current block level.
 */
 void PL0CComp::whileStmt(int level) {
	const size_t cond_pc = code->size();			// Start of while condition
	condition(level);

	const size_t jmp_pc = emit(OpCode::jneq, 0, 0);	// jump if condition false
	expect(Token::Do);								// consume "do"
	statement(level);

	emit(OpCode::jump, 0, cond_pc);					// Jump back to conditon test

	if (verbose)
		cout << progName << ": patching address at " << jmp_pc << " to " << code->size() << "\n";
	(*code)[jmp_pc].addr = code->size();			// Patch jump on condition false instruction
 }

/** if statement
 *  
 *  "if" condition "then" statement1 [ "else" statement2 ]
 */
 void PL0CComp::ifStmt(int level) {
	condition(level);

	const size_t jmp_pc = emit(OpCode::jneq, 0, 0);	// jump if condition false
	expect(Token::then);							// Consume "then"
	statement(level);

	// Jump over else statement, but only if there is an else
	const bool Else = accept(Token::Else);
	size_t else_pc = 0;
	if (Else) else_pc = emit(OpCode::jump, 0, 0);
	
	if (verbose)
		cout << progName << ": patching address at " << jmp_pc << " to " << code->size() << "\n";
	(*code)[jmp_pc].addr = code->size();				// Patch jump on condition false instruction

	if (Else) {
		statement(level);

		if (verbose)
			cout << progName << ": patching address at " << else_pc << " to " << code->size() << "\n";
		(*code)[else_pc].addr = code->size();				// Patch jump on condition false instruction
	}
 }

 /** Repeat statement
  *  
  * [ "repeat" stmt "until" cond ] 
  *  
  * @param	level 	The current block level 
  */
 void PL0CComp::repeatStmt(int level) {
	 const size_t loop_pc = code->size();			// jump here until condition fails
	 statement(level);
	 expect(Token::until);
	 condition(level);
	 emit(OpCode::jneq, 0, loop_pc);
 }

/** Statement
 *
 * stmt =	[ ident ":=" expr
 * 		  	| "call" ident
 *          | "?" ident
 * 		  	| "!" expr
 *          | "begin" stmt {";" stmt } "end"
 *          | "if" cond "then" stmt { "else" stmt }
 *          | "while" cond "do" stmt ] ;
 *
 * @param	level	The current block level.
 */
void PL0CComp::statement(int level) {
	if (accept(Token::identifier, false)) 			// assignment or proc call
		identStmt(level);

	else if (accept(Token::begin)) {				// begin ... end
		do {
			statement(level);
		} while (accept(Token::scomma));
		expect(Token::end);

	} else if (accept(Token::If)) 					// if condition...
		ifStmt(level);

	else if (accept(Token::While))					// "while" condition...
		whileStmt(level);

	else if (accept(Token::repeat))
		repeatStmt(level);

	// else: nothing
}

/** Constant declaration
 *
 * [ const ident = number {, ident = number} ; ]
 *
 * @note Doesn't emit any code; just stores the named value in the symbol table.
 *
 * @param	level	The current block level.
 */
void PL0CComp::constDecl(int level) {
	// Capture the identifier name before consuming it...
	const string name = ts.current().string_value;

	expect(Token::identifier);						// Consume the identifier
	expect(Token::assign);							// Consume the "="
	if (expect(Token::number, false)) {
		auto number = ts.current().number_value;
		next();										// Consume the number

		auto range = symtbl.equal_range(name);
		for (auto it = range.first; it != range.second; ++it)
			if (it->second.level == level) {
				error("identifier has previously been defined", name);
				return;
			}

		symtbl.insert({ name, { SymValue::constant, level, number } });
		if (verbose) 
			cout << progName << ": constDecl " << name << ": " << level << ", " << number << "\n";
	}
}


/** Variable declaration
 *
 * Allocate space on the stack for the variable and install it's offset from the block in the symbol
 * table.
 *  
 * @param	offset	Stack offset for the next varaible
 * @param	level	The current block level.
 * 
 * @return	Stack offset for the next varaible.
 */
int PL0CComp::varDecl(int offset, int level) {
	const string name = ts.current().string_value;

	if (expect(Token::identifier)) {
		auto range = symtbl.equal_range(name);
		for (auto it = range.first; it != range.second; ++it)
			if (it->second.level == level) {
				error("identifer has previously been defined", name);
				return offset;
			}

		symtbl.insert( { name, { SymValue::identifier, level, offset }} );
		if (verbose) 
			cout << progName << ": varDecl " << name << ": " << level << ", " << offset << "\n";
		return offset + 1;
	}

	return offset;
}

/** Subroutine declaration
 *
 *   "procedure" ident "(" [ident {, "ident" }] ")" block ";"
 * | "function"  ident "(" [ident {, "ident" }] ")" block ";"
 *
 * @param	level	The current block level.
 */
void PL0CComp::subDecl(int level) {
	const	Token::Kind kind = current();	// proc or function decl
	next();									// consume token...

											// Save the identifier...
	const 	string name = ts.current().string_value;
			vector<string> args;			// formal arguments, if any

	if (expect(Token::identifier)) {
		auto range = symtbl.equal_range(name);
		for (auto it = range.first; it != range.second; ++it)
			if (it->second.level == level)
				error("identifier has previously been defined", name);
		
		SymbolTable::iterator it;			// insert the new symbol...
		if (Token::procDecl == kind) {
			it = symtbl.insert( { name, { SymValue::proc, level, 0 }} );
			if (verbose)
				cout << progName << ": procDecl " << name << ": " << level << ", 0\n";

		} else {							// funcDecl
			it = symtbl.insert( { name, { SymValue::function, level, 0 }} );
			if (verbose)
				cout << progName << ": funcDecl " << name << ": " << level << ", 0\n";
		}

		expect(Token::lparen);				// procedure name "()"
		if (accept(Token::identifier, false)) { // identifier {, identifier }
			int offset = 0;					// Record the arguments...
			do {
				args.push_back(ts.current().string_value);
				--offset;							// -1, -2, ..., -n
				accept(Token::identifier);			// Consume the identifier
			} while (accept(Token::comma));

			/**
			 * Add the arguments, with negative offsets from the block/frame, so
			 * that they have offsets of -n, ..., -2, -1. Note that their levels
			 * must be the same as the following block.
			 */
			for (auto& x : args) {
				const string& name = x;
				symtbl.insert( { name, { SymValue::identifier, level+1, offset++ }});
			}
		}

		expect(Token::rparen);
		block(it->second, level+1, args.size());
		expect(Token::scomma);	// procedure declarations end with a ';'!
	}
}

/** program block
 *
 * block = 	[ const ident = number {, ident = number} ";"]
 *         	[ var ident {, ident} ";" ]
 *         	{ procedure ident "(" [ ident { "," ident } ] ")" block ";"
 *         	 | function ident "(" [ ident { "," ident } ] ")" block ";" }
 *          stmt ;
 *  
 * @param	val		The blocks (procedures) symbol table entry value
 * @param	level	The current block level.
 * @param   nargs	The number of arguments, passed to the block, that must be
 * 					popped on return.
 */
void PL0CComp::block(SymValue& val, int level, unsigned nargs) {
	auto 	jmp_pc	= emit(OpCode::jump, 0, 0);	// Addr to be patched below..
												// Variable offset from block frame
	int		dx		= static_cast<int>(Frame::size);

	if (accept(Token::constDecl)) {				// const ident = number, ...
		do {
			constDecl(level);

		} while (accept(Token::comma));
		expect(Token::scomma);
	}

	if (accept(Token::varDecl)) {				// var ident, ...
		do {
			dx = varDecl(dx, level);

		} while (accept(Token::comma));
		expect(Token::scomma);
	}

	while (accept(Token::procDecl, false) || accept(Token::funcDecl, false))
		subDecl(level);

	/*
	 * Block body
	 *
	 * Emit the block prefix, and then set the blocks staring address, and
	 * patch the jump to it, followed by the postfix.
	 */

	auto addr = emit(OpCode::enter, 0, dx);		// prefix
	if (verbose)
		cout << progName << ": patching address at " << jmp_pc << " to " << addr  << "\n";
	(*code)[jmp_pc].addr = val.value = addr;

	statement(level);

	assert(nargs < numeric_limits<int>::max());	// postfix...
	if (SymValue::function == val.kind)
		emit(OpCode::reti, 0, nargs);			//	function...
	else
		emit(OpCode::ret, 0, nargs);			//	procedure...

	// Finally, remove symbols only visible in this level
	for (auto i = symtbl.begin(); i != symtbl.end(); ) {
		if (i->second.level == level) {
			if (verbose)
				cout << progName << ": purging "
				<< i->first << ": "
				<< SymValue::toString(i->second.kind) << ", " << static_cast<int>(i->second.level) << ", " << i->second.value
				<< " from the symbol table\n";
			i = symtbl.erase(i);

		} else
			++i;
	}
}

// public:

/// Constructor; construct a compiler; use pName for errors
PL0CComp::PL0CComp(const string& pName) : progName {pName}, nErrors{0}, verbose {false}, ts{cin} {
	symtbl.insert({"main", { SymValue::proc, 0, 0 }});	// Install the "main" rountine declaraction
}

/// Compile...
void PL0CComp::run() {
	next();
	auto range = symtbl.equal_range("main");
	assert(range.first != range.second);

	block(range.first->second, 0, 0);
	expect(Token::period);
}

/** Run the compiler
 *
 * @param	inFile	The source file name
 * @param	prog	The generated machine code is appended here
 * @param	v		Output verbose messages if true
 *
 * @return	The number of errors encountered
 */
unsigned PL0CComp::operator()(const string& inFile, InstrVector& prog, bool v) {
	code = &prog;
	verbose = v;

	if ("-" == inFile)  {							// "-" means standard input
		ts.set_input(cin);
		run();

	} else {
		ifstream ifile(inFile);
		if (!ifile.is_open())
			error("error opening source file", inFile);

		else {
			ts.set_input(ifile);
			run();
		}
	}

	if (verbose) {									// Disassemble results
		cout << "\n";
		unsigned loc = 0;
		for (auto it : *code)
			disasm(loc++, it);
		cout << endl;
	}

	code = 0;
		
	return nErrors;
}
