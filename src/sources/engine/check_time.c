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

#include "engine.h"
#include "lazy_smp.h"
#include "timeman.h"

void    check_time(void)
{
    if (--WPool.checks > 0)
        return ;

    // Reset check counter

    WPool.checks = 1000;

    // If we are in infinite mode, or the stop has already been set,
    // we can safely return.

    if (SearchParams.infinite || search_should_abort())
        return ;

    if (get_node_count() >= SearchParams.nodes)
        goto __set_stop;

    if (timeman_must_stop_search(&Timeman, chess_clock()))
        goto __set_stop;

    return ;

__set_stop:
    pthread_mutex_lock(&EngineMutex);
    EngineSend = DO_EXIT;
    pthread_mutex_unlock(&EngineMutex);
}