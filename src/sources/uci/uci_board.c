/*
**    Stash, a UCI chess playing engine developed from scratch
**    Copyright (C) 2019-2021 Morgan Houppin
**
**    Stash is free software: you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation, either version 3 of the License, or
**    (at your option) any later version.
**
**    Stash is distributed in the hope that it will be useful,
**    but WITHOUT ANY WARRANTY; without even the implied warranty of
**    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**    GNU General Public License for more details.
**
**    You should have received a copy of the GNU General Public License
**    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "board.h"
#include "info.h"
#include "lazy_smp.h"
#include "uci.h"
#include "engine.h"

// Pretty prints the board, along with the hash key and the eval.

void    uci_d(const char *args __attribute__((unused)))
{
    const char      *grid = "+---+---+---+---+---+---+---+---+";
    const char      *piece_to_char = " PNBRQK  pnbrqk";

    puts(grid);

    for (file_t rank = RANK_8; rank >= RANK_1; --rank)
    {
        for (file_t file = FILE_A; file <= FILE_H; ++file)
            printf("| %c ", piece_to_char[piece_on(&Board, create_sq(file, rank))]);

        puts("|");
        puts(grid);
    }

    printf("\nKey: 0x%" KEY_INFO "\n", (info_t)Board.stack->board_key);

    double  eval = (double)evaluate(&Board) / 100.0;

    printf("Eval (from %s's POV): %+.2lf\n\n", Board.side_to_move == WHITE ? "White" : "Black", eval);
    fflush(stdout);
}

void    uci_position(const char *args)
{
    static boardstack_t **hidden_list = NULL;
    static size_t       hidden_size = 0;

    wait_search_end();

    if (hidden_size > 0)
    {
        for (size_t i = 0; i < hidden_size; ++i)
            free(hidden_list[i]);
        free(hidden_list);
    }

    hidden_list = malloc(sizeof(boardstack_t *));
    *hidden_list = malloc(sizeof(boardstack_t));
    hidden_size = 1;

    char    *fen;
    char    *copy = strdup(args);
    char    *ptr = copy;
    char    *token = get_next_token(&ptr);

    if (!strcmp(token, "startpos"))
    {
        fen = strdup("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        token = get_next_token(&ptr);
    }
    else if (!strcmp(token, "fen"))
    {
        fen = strdup("");
        token = get_next_token(&ptr);

        while (token && strcmp(token, "moves"))
        {
            char    *tmp = malloc(strlen(fen) + strlen(token) + 2);

            strcpy(tmp, fen);
            strcat(tmp, " ");
            strcat(tmp, token);
            free(fen);
            fen = tmp;
            token = get_next_token(&ptr);
        }
    }
    else
        return ;

    set_board(&Board, fen, Options.chess960, *hidden_list);
    Board.worker = WPool.list;
    free(fen);
    token = get_next_token(&ptr);

    move_t  move;

    while (token && (move = str_to_move(&Board, token)) != NO_MOVE)
    {
        hidden_list = realloc(hidden_list, sizeof(boardstack_t *) * ++hidden_size);
        hidden_list[hidden_size - 1] = malloc(sizeof(boardstack_t));

        do_move(&Board, move, hidden_list[hidden_size - 1]);
        token = get_next_token(&ptr);
    }

    free(copy);
}