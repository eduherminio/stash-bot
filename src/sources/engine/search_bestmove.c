/*
**	Stash, a UCI chess playing engine developed from scratch
**	Copyright (C) 2019-2020 Morgan Houppin
**
**	Stash is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	Stash is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "history.h"
#include "imath.h"
#include "info.h"
#include "lazy_smp.h"
#include "movelist.h"
#include "tt.h"
#include "uci.h"

score_t	search_pv(board_t *board, int depth, score_t alpha, score_t beta,
		searchstack_t *ss)
{
	if (depth <= 0)
		return (qsearch(board, alpha, beta, ss));

	worker_t			*worker = get_worker(board);
	movelist_t			list;
	move_t				pv[256];
	score_t				best_value = -INF_SCORE;

	if (!worker->idx)
		check_time();

	if (worker->seldepth < ss->plies + 1)
		worker->seldepth = ss->plies + 1;

	if (g_engine_send == DO_EXIT || g_engine_send == DO_ABORT
		|| is_draw(board, ss->plies))
		return (0);

	if (ss->plies >= MAX_PLIES)
		return (!board->stack->checkers ? evaluate(board) : 0);

	// Check for interesting tt values

	move_t		tt_move = NO_MOVE;
	bool		found;
	tt_entry_t	*entry = tt_probe(board->stack->board_key, &found);
	score_t		eval;

	if (found)
	{
		eval = entry->eval;
		tt_move = entry->bestmove;
	}
	else
		eval = evaluate(board);

	(ss + 1)->pv = pv;
	pv[0] = NO_MOVE;
	(ss + 1)->plies = ss->plies + 1;
	(ss + 2)->killers[0] = (ss + 2)->killers[1] = NO_MOVE;

	list_pseudo(&list, board);
	generate_move_values(&list, board, tt_move, ss->killers);

	move_t	bestmove = NO_MOVE;
	int		move_count = 0;
	move_t	quiets[64];
	int		qcount = 0;

	for (const extmove_t *extmove = movelist_begin(&list);
		extmove < movelist_end(&list); ++extmove)
	{
		place_top_move((extmove_t *)extmove, (extmove_t *)movelist_end(&list));
		if (!board_legal(board, extmove->move))
			continue ;

		move_count++;

		boardstack_t	stack;
		score_t			next;
		int				reduction;
		int				new_depth = depth - 1;
		bool			is_quiet = !is_capture_or_promotion(board, extmove->move);

		do_move(board, extmove->move, &stack);

		// Can we apply LMR ?
		if (depth >= LMR_MinDepth && move_count > LMR_MinMoves && !board->stack->checkers)
			reduction = (depth + move_count) / 10 + 1;
		else
			reduction = 0;

		if (reduction)
			next = -search(board, new_depth - reduction, -alpha - 1, -alpha, ss + 1);

		// If LMR is not possible, or our LMR failed, do a search with no reductions
		if ((reduction && next > alpha) || (!reduction && move_count != 1))
			next = -search(board, new_depth, -alpha - 1, -alpha, ss + 1);

		// Finally, if null window search and LMR search failed above alpha, do
		// a full window search.
		if (move_count == 1 || next > alpha)
		{
			pv[0] = NO_MOVE;
			next = -search_pv(board, new_depth, -beta, -alpha, ss + 1);
		}

		undo_move(board, extmove->move);

		if (g_engine_send == DO_ABORT || g_engine_send == DO_EXIT)
			return (0);

		if (best_value < next)
		{
			best_value = next;

			if (alpha < best_value)
			{
				ss->pv[0] = bestmove = extmove->move;
				alpha = best_value;

				size_t	j;
				for (j = 0; (ss + 1)->pv[j] != NO_MOVE; ++j)
					ss->pv[j + 1] = (ss + 1)->pv[j];

				ss->pv[j + 1] = NO_MOVE;

				if (alpha >= beta)
				{
					if (is_quiet)
						update_quiet_history(worker->history, board, depth,
							bestmove, quiets, qcount, ss);
					break ;
				}
			}
		}

		if (qcount < 64 && is_quiet)
			quiets[qcount++] = extmove->move;

		if (depth < 4 && qcount > depth * 8)
			break ;
	}

	// Checkmate/Stalemate ?

	if (move_count == 0)
		best_value = (board->stack->checkers) ? mated_in(ss->plies) : 0;

	// Do not erase entries with higher depth for same position.

	if (entry->key != board->stack->board_key || entry->depth <= depth)
	{
		int bound = (bestmove == NO_MOVE) ? UPPER_BOUND
			: (best_value >= beta) ? LOWER_BOUND : EXACT_BOUND;

		tt_save(entry, board->stack->board_key, score_to_tt(best_value, ss->plies), eval, depth, bound, bestmove);
	}

	return (best_value);
}

void	search_bestmove(board_t *board, int depth, score_t alpha, score_t beta,
		root_move_t *begin, root_move_t *end, int pv_line)
{
	extern goparams_t	g_goparams;
	searchstack_t		sstack[512];
	move_t				pv[512];

	memset(sstack, 0, sizeof(sstack));

	sstack[0].plies = 1;
	sstack[0].pv = pv;

	movelist_t			list;
	int					move_count = 0;

	list_pseudo(&list, board);
	generate_move_values(&list, board, begin->move, NULL);

	for (const extmove_t *extmove = movelist_begin(&list);
		extmove < movelist_end(&list); ++extmove)
	{
		root_move_t	*cur;

		place_top_move((extmove_t *)extmove, (extmove_t *)movelist_end(&list));
		if ((cur = find_root_move(begin, end, extmove->move)) == NULL)
			continue ;

		move_count++;

		clock_t		elapsed = chess_clock() - g_goparams.start;

		if (!get_worker(board)->idx && elapsed > 3000)
		{
			printf("info depth %d currmove %s currmovenumber %d\n",
				depth, move_to_str(cur->move, board->chess960),
				move_count + pv_line);
			fflush(stdout);
		}

		boardstack_t	stack;
		score_t			next;
		int				reduction;
		int				new_depth = depth - 1;

		do_move(board, extmove->move, &stack);

		// Can we apply LMR ?
		if (depth >= LMR_MinDepth && move_count > LMR_MinMoves && !board->stack->checkers)
			reduction = (depth + move_count) / 10 + 1;
		else
			reduction = 0;

		if (reduction)
			next = -search(board, new_depth - reduction, -alpha - 1, -alpha, sstack);

		// If LMR is not possible, or our LMR failed, do a search with no reductions
		if ((reduction && next > alpha) || (!reduction && move_count != 1))
			next = -search(board, new_depth, -alpha - 1, -alpha, sstack);

		// Finally, if null window search and LMR search failed above alpha, do
		// a full window search.
		if (move_count == 1 || next > alpha)
		{
			pv[0] = NO_MOVE;
			next = -search_pv(board, new_depth, -beta, -alpha, sstack);
		}

		undo_move(board, cur->move);

		if (g_engine_send == DO_ABORT || g_engine_send == DO_EXIT)
			return ;

		else if (move_count == 1 || next > alpha)
		{
			cur->score = next;
			alpha = max(alpha, next);
			cur->seldepth = get_worker(board)->seldepth;
			cur->pv[0] = cur->move;

			size_t	j;
			for (j = 0; sstack[0].pv[j] != NO_MOVE; ++j)
				cur->pv[j + 1] = sstack[0].pv[j];

			cur->pv[j + 1] = NO_MOVE;

			if (next >= beta)
				return ;
		}
		else
			cur->score = -INF_SCORE;
	}
	return ;
}
