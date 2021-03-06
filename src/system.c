/*
 * Copyright (C) 2013 Nikos Mavrogiannopoulos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <config.h>
#include <system.h>
#include <unistd.h>
#ifdef __linux__
# include <sys/prctl.h>
#endif

void kill_on_parent_kill(int sig)
{
#ifdef __linux__
	prctl(PR_SET_PDEATHSIG, sig);
#endif
}

sighandler_t ocsignal(int signum, sighandler_t handler)
{
	struct sigaction new_action, old_action;
	
	new_action.sa_handler = handler;
	sigemptyset (&new_action.sa_mask);
	new_action.sa_flags = 0;
	
	sigaction (signum, &new_action, &old_action);
	return old_action.sa_handler;
}
