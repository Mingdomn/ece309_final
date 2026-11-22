// src/tools/calculator_eval.cpp — YOU IMPLEMENT (spec §4.6).
//
// calc_evaluate: evaluate a well-formed token list with two stacks
// (one for numbers, one for operators and '('). See spec §4.6 and the
// worked trace in Appendix C. std::stack or std::vector are both fine.

#include "tools/calculator.h"

#include <cstdint>
#include <iostream>
bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
	CalcToken *out_stack, *op_stack;
	double *ans_stack;
	std::size_t out_len = 0, op_len = 0, ans_len = 0;

	out_stack = new CalcToken[tokens.size()];
	op_stack = new CalcToken[tokens.size()];

	for (CalcToken t : tokens) {
		std::int8_t pri, pri_save;

		switch (t.kind) {
		case CalcToken::Kind::Number:
			out_stack[out_len++] = t;
			continue;
		case CalcToken::Kind::Plus:
		case CalcToken::Kind::Minus:
			pri = 2;
			break;
		case CalcToken::Kind::Divide:
			pri = 1;
			break;
		case CalcToken::Kind::Times:
			pri = 0;
			break;
		case CalcToken::Kind::LParen:
			op_stack[op_len++] = t;
			continue;
		case CalcToken::Kind::RParen:
			pri = 3;
			break;
		}

		for (pri_save = pri; op_len > 0; pri = pri_save) {
			/*
			 * NB:
			 * We don't really care about the current operator's
			 * priority. We only care about its priority relative
			 * to the top of the operator stack.
			 */
			switch (op_stack[op_len - 1].kind) {
			case CalcToken::Kind::Divide:
				pri -= 1;
				break;
			case CalcToken::Kind::Plus:
			case CalcToken::Kind::Minus:
				pri -= 2;
				break;
			case CalcToken::Kind::LParen:
				/* NB: An RParen should never make it here. */
				pri -= 3;
				break;
			case CalcToken::Kind::Times:
				break;
			default:
				*error = "Invalid token in operator stack";
				delete[] op_stack;
				delete[] out_stack;
				return false;
			}

			if (pri < 0)
				break;

			if (op_stack[--op_len].kind != CalcToken::Kind::LParen)
				out_stack[out_len++] = op_stack[op_len];
			else
				break;
		}

		/* Parentheses don't go onto the output stack. */
		if (pri_save != 3)
			op_stack[op_len++] = t;
	}

	/* Empty what's left in the operator stack. */
	while (op_len > 0)
		out_stack[out_len++] = op_stack[--op_len];

	delete[] op_stack;
	ans_stack = new double[out_len];

	for (std::size_t i = 0; i < out_len; i++) {
		switch(out_stack[i].kind) {
		case CalcToken::Kind::Number:
			ans_stack[ans_len++] = out_stack[i].value;
			break;
		case CalcToken::Kind::Plus:
			ans_len--;
			ans_stack[ans_len - 1] = ans_stack[ans_len - 1] +
			    ans_stack[ans_len];
			break;
		case CalcToken::Kind::Minus:
			ans_len--;
			ans_stack[ans_len - 1] = ans_stack[ans_len - 1] -
			    ans_stack[ans_len];
			break;
		case CalcToken::Kind::Times:
			ans_len--;
			ans_stack[ans_len - 1] = ans_stack[ans_len - 1] *
			    ans_stack[ans_len];
			break;
		case CalcToken::Kind::Divide:
			ans_len--;

			if (ans_stack[ans_len] == 0) {
				*error = "division by zero";
				delete[] ans_stack;
				delete[] out_stack;
				return false;
			}

			ans_stack[ans_len - 1] = ans_stack[ans_len - 1] /
			     ans_stack[ans_len];
			break;
		default:
			*error = "Invalid token in output stack";
			delete[] ans_stack;
			delete[] out_stack;
			return false;
		}
	}

	*out = ans_stack[0];
	*error = "";

	delete[] ans_stack;
	delete[] out_stack;
	return true;
}
