/* Copyright (C) 2007, 2008 Jaap Taal and Marcel Vlastuin.

This file is part of the Caia project. This project can be
downloaded from the website www.codecup.nl. You can email to
marcel@vlastuin.net or write to Marcel Vlastuin, Perenstraat 40,
2564 SE Den Haag, The Netherlands.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA. */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
//#include <unistd.h>
#include <assert.h>
#include "mainloop.h"
#include "state.h"
#include "manager.h"
//#include "referee.h"
//#include "player.h"
#include "prflush.h"
#include "debug.h"
#include "error.h"
#include "cpuspeed.h"
#include "command.h"

void rewritespace(char * const str)
{
  int i, n = strlen(str);
  for (i = 0; i < n; i++)
  {
    if (str[i] == ' ') str[i] = '_';
  }
}

void mainloop(void)
{
	//class program *lock = NULL;
	struct process_ctx *lock = NULL, *process = NULL;
	//char *line = NULL;
	char line[BUF_SIZE];
	char *linep;
	char word[256];
	char tmp1word[256];
	char tmp2word[256];
	char logline[4096];
	//int linesize = -1;
	unsigned long linesize = 0;
	int tmp1int;
	int tmp2int;
	int i=0;
	int j, k, found;
	int manager_wait_time_ms = 5;
	float time_factor = 1.0;
	io_state state = STATE_NONE;
	char cmd_sender[256];
	assert(manager != NULL);

	debug(DEBUG_MAINLOOP_STARTED);
	while (1)
	{
		if (state == CAIAIO_STOP) break;
		if (state == CAIAIO_ERROR)
		{
			fprflush(stderr, "I: Error detected bailing out!\n");
			break;
		}
		process = manager->process_ctx;
		//if (manager->running()) manager->checkalive();
		if (process->state == PROCESS_ACTIVE) {
			check_alive(process);
		}
		//if (!manager->running() && manager->bufferempty())
		if (process->state != PROCESS_ACTIVE &&	!process->buffer_ctx_in->data_length)
		{
			debug(DEBUG_MANAGER_CLOSED_NO_BUFFER);
			break;
		}
		/* modified */
		//if ((lock==NULL && manager->check_out()) || lock==manager) // Added by Bauke Conijn
		// if ((lock==NULL || lock==manager) && manager->check_out())
		linesize = 0;
		if (lock == NULL || lock==process) {
			receive_timed_process_message(line, &linesize, process, manager_wait_time_ms); // 0.005 secs if no referee
			if (!linesize && process->buffer_ctx_in->read_status == ERROR_BROKEN_PIPE) {
				// manager closed stdout
				// this means that CAIAIO will exit the main loop
				// it's unwise to make a manager that executes fclose(stdout)!
				// normally the CAIAIO will only end up here when the manager crashed (or exits in rare cases)
				fprflush(stderr, "manager suddenly stopped!\n");
				break;
			}
		}
		/* modified */
		if (linesize)
		{
			strcpy(cmd_sender, "manager");
			/* modified
			manager->readln(&line, &linesize);
			if (manager->out_closed())
			{
				// manager closed stdout
				// this means that CAIAIO will exit the main loop
				// it's unwise to make a manager that executes fclose(stdout)!
				// normally the CAIAIO will only end up here when the manager crashed (or exits in rare cases)
				fprflush(stderr, "manager suddenly stopped!\n");
				break;
			}*/
			debug(DEBUG_MESSAGE_FROM_MANAGER, line);
			linep = line;
			readword(&linep, word);

			if (!strcmp(word, IO_NAME_TAG))
			{
				readword(&linep, word);
				state = state_parse_token(word);
				switch(state)
				{
					case CAIAIO_STOP:
						debug(DEBUG_OK);
						break;

					case CAIAIO_LOCK:
						//lock = manager;
						lock = process;
						debug(DEBUG_SEND_OK_BACK_TO_MANAGER);
						//manager->writeln(RESP_LOCK_OK);
						send_message(process, RESP_LOCK_OK);
						break;

					case CAIAIO_UNLOCK:
						lock = NULL;
						debug(DEBUG_OK);
						break;

					case REFEREE_START:
						if (readword(&linep, tmp1word))
						{
							fprflush(stderr, ERROR_INC_PROGRAM, STR_REFEREE_START, line);
							break;
						}
						/* modified */
						//start_referee(tmp1word);
						//if (!readword(&linep, tmp2word))
						//{
						//	referee->add_arg(tmp2word);
						//}
						//referee->start();
						readword(&linep, tmp2word);
						if (!create_referee(tmp1word, tmp2word)) {
							state = CAIAIO_ERROR;
							break;
						}
						if (!start_referee()) {
							state = CAIAIO_ERROR;
							break;
						}
						manager_wait_time_ms = 0;
						/* modified */
						debug(DEBUG_OK);
						break;

					case REFEREE_KILL:
						stop_referee();
						referee = NULL;
						debug(DEBUG_OK);
						break;

					case CAIAIO_CPU:
						if (readint(&linep, word, &tmp1int))
						{
							if (strlen(word)) { fprflush(stderr, ERROR_INV_CCPU, STR_CAIAIO_CPU, word); }
							else { fprflush(stderr, ERROR_INC_CCPU, STR_CAIAIO_CPU, line); }
							state = CAIAIO_ERROR;
							break;
						}
						if (readint(&linep, word, &tmp2int))
						{
							if (strlen(word))
							{
								fprflush(stderr, ERROR_INV_TCPU, STR_CAIAIO_CPU, word);
								state = CAIAIO_ERROR;
								break;
							}
							tmp2int=cpuspeed();
						}
						if (tmp2int <= 0) tmp2int = 1;
						//time_factor = float(tmp1int) / float(tmp2int);
						time_factor = (float)tmp1int / (float)tmp2int;
						debug(DEBUG_CPU_SPEED, time_factor);
						break;

					case PLAYER_NUMBER:
						if (readint(&linep, word, &tmp1int))
						{
							if (strlen(word)) { fprflush(stderr, ERROR_INV_PLAYNUM, STR_PLAYER_NUMBER, word); }
							else { fprflush(stderr, ERROR_INC_PLAYNUM, STR_PLAYER_NUMBER, line); }
							state = CAIAIO_ERROR;
							break;
						}
						if (tmp1int > MAXPLAYERS)
						{
							fprflush(stderr, ERROR_MAX_PLAYNUM, MAXPLAYERS, word);
							state = CAIAIO_ERROR;
							break;
						}
						if (tmp1int < 1) // 16 july 2008 changed 2 into 1 making one-player games possible! (Marcel Vlastuin)
						{
							fprflush(stderr, ERROR_MIN_PLAYNUM, word);
							state = CAIAIO_ERROR;
							break;
						}
						/* modified */
						//init_players(tmp1int);
						if (!create_player_array(tmp1int)) {
							state = CAIAIO_ERROR;
							break;
						}
						/* modified */
						debug(DEBUG_OK);
						break;

					case PLAYER_INIT:
						if (readint(&linep, word, &tmp1int))
						{
							if (strlen(word)) { fprflush(stderr, ERROR_INV_PLAY, STR_PLAYER_INIT, word); }
							else { fprflush(stderr, ERROR_INC_PLAY, STR_PLAYER_INIT, word); }
							state = CAIAIO_ERROR;
							break;
						}
						if (tmp1int < 1 || tmp1int > nplayers)
						{
							fprflush(stderr, ERROR_PLAY_DNE, STR_PLAYER_INIT, tmp1int);
							state = CAIAIO_ERROR;
							break;
						}
						if (player[tmp1int-1] != NULL)
						{
							fprflush(stderr, ERROR_PLAY_AINI, STR_PLAYER_INIT, tmp1int);
							state = CAIAIO_ERROR;
							break;
						}
						if (readword(&linep, tmp1word))
						{
							fprflush(stderr, ERROR_INC_PROG, STR_PLAYER_INIT, line);
							state = CAIAIO_ERROR;
							break;
						}
						if (readint(&linep, word, &tmp2int))
						{
							if (strlen(word)) { fprflush(stderr, ERROR_INV_TIMEOUT, STR_PLAYER_INIT, word); }
							else { fprflush(stderr, ERROR_INC_TIMEOUT, STR_PLAYER_INIT, line); }
							state = CAIAIO_ERROR;
							break;
						}
						if (readword(&linep, tmp2word))
						{
							//init_player(tmp1int-1, tmp1word, multiply(tmp2int, time_factor), NULL);
                            if (!create_player(tmp1int - 1, tmp1word, multiply(tmp2int, time_factor), 0)) {
                                state = CAIAIO_ERROR;
                                break;
                            }
						}
						else
						{
                            // init_player(tmp1int-1, tmp1word, multiply(tmp2int, time_factor), tmp2word);
                            if (!create_player(tmp1int - 1, tmp1word, multiply(tmp2int, time_factor), tmp2word)) {
                                state = CAIAIO_ERROR;
                                break;
                            }
                        }
						debug(DEBUG_OK);
						break;

					case PLAYER_START:
						if (readint(&linep, word, &tmp1int))
						{
							if (strlen(word)) { fprflush(stderr, ERROR_INV_PLAY, STR_PLAYER_START, word);  }
							else { fprflush(stderr, ERROR_INC_PLAY, STR_PLAYER_START, line);  }
							state = CAIAIO_ERROR;
							break;
						}
						if (tmp1int < 1 || tmp1int > nplayers || player[tmp1int-1] == NULL)
						{
							fprflush(stderr, ERROR_PLAY_DNE, STR_PLAYER_START, tmp1int);
							state = CAIAIO_ERROR;
							break;
						}
						/* modified */
						//	start_player(tmp1int-1);
						/* modified */
						if (readint(&linep, word, &tmp2int))
						{
							if (strlen(word))
							{
								fprflush(stderr, ERROR_INV_FIRSTERR, STR_PLAYER_START, word);
								state = CAIAIO_ERROR;
								break;
							}
							/* modified */
                            if (!start_player(tmp1int-1, 0)) {
                                state = CAIAIO_ERROR;
                                break;
                            }
							debug(DEBUG_OK);
						}
						else
						{
							/* modified */
                            if (!start_player(tmp1int-1, tmp2int)) {
                                state = CAIAIO_ERROR;
                                break;
                            }
							//if (player[tmp1int-1]->poll_error(tmp2int))
							//{
								//player[tmp1int-1]->readlnerr(&line, &linesize);
								//strcpy(logline, line);
							if (player[tmp1int-1]->process_ctx->buffer_ctx_in->data_length)
							{
								strcpy(line, player[tmp1int-1]->process_ctx->buffer_ctx_in->buffer);
								strcpy(logline, line);
								player[tmp1int-1]->process_ctx->buffer_ctx_in->data_length = 0;
							/* modified */
								k=0; found=0;
								for (j=0; j<(int)strlen(line); ++j)
								{
									if (!found && line[j]!=' ' && line[j]!='\t' && line[j]!='\n') {found=1; ++k;}
									else if (found && (line[j]==' ' || line[j]=='\t' || line[j]=='\n')) {found=0;}
								}
								if (k==0) sprintf(line, "%s %s", PLAYER_ERROR_ZEROSTRING_TAG, PLAYER_ERROR_ZEROSTRING_TAG);
								else if (k==1) {strcat(line, " "); strcat(line, PLAYER_ERROR_ZEROSTRING_TAG);}
								sprintf(tmp1word, "%s %s", STR_FIRSTERROR, line);
								debug(DEBUG_CAIAIO_SENDS_TO_MANAGER, tmp1word);
								/* modified */
								//manager->writeln(tmp1word);
								//debug("%d: %s\n", tmp1int, logline);
								//player[tmp1int-1]->write_logfile(logline);
								//manager->writeln(tmp1word);
								debug("%d: %s\n", tmp1int, logline);
								send_message(process, tmp1word);
								/* modified */
							}
							else
							{
								debug(DEBUG_CAIAIO_SENDS_TO_MANAGER, STR_NO_FIRSTERROR);
								/* modified */
								//manager->writeln(STR_NO_FIRSTERROR);
								send_message(process, STR_NO_FIRSTERROR);
								/* modified */
							}
						}
						break;

					case PLAYER_KILL:
						if (readint(&linep, word, &tmp1int))
						{
							if (strlen(word)) { fprflush(stderr, ERROR_INV_PLAY, STR_PLAYER_KILL, word);  }
							else { fprflush(stderr, ERROR_INC_PLAY, STR_PLAYER_KILL, line);  }
							state = CAIAIO_ERROR;
							break;
						}
						if (tmp1int < 1 || tmp1int > nplayers || player[tmp1int-1] == NULL)
						{
							fprflush(stderr, ERROR_PLAY_DNE, STR_PLAYER_KILL, tmp1int);
							state = CAIAIO_ERROR;
							break;
						}
						/* modified */
						//while (player[tmp1int-1]->check_err())
						//{
						//	player[tmp1int-1]->readlnerr(&line, &linesize);
						//	debug("%d: %s\n", tmp1int, line);
						//	player[tmp1int-1]->write_logfile(line);
						//}
						/* modified */
						stop_player(tmp1int-1);
						debug(DEBUG_OK);
						break;

					case PLAYER_LISTEN:
						if (readint(&linep, word, &tmp1int))
						{
							if (strlen(word)) { fprflush(stderr, ERROR_INV_PLAY, STR_PLAYER_LISTEN, word);  }
							else { fprflush(stderr, ERROR_INC_PLAY, STR_PLAYER_LISTEN, line);  }
							state = CAIAIO_ERROR;
							break;
						}
						if (tmp1int < 1 || tmp1int > nplayers || player[tmp1int-1] == NULL)
						{
							fprflush(stderr, ERROR_PLAY_DNE, STR_PLAYER_LISTEN, tmp1int);
							state = CAIAIO_ERROR;
							break;
						}
						debug(DEBUG_OK);

						/* modified
						player[tmp1int-1]->checkalive();
						if (player[tmp1int-1]->crashed())
						{
							sprintf(line, PLAYER_ERROR_CRASHED_TAG, strsignal(player[tmp1int-1]->crashedsig())); //see mainloop.h
							rewritespace(line);
						}
						else if (!player[tmp1int-1]->running() && !player[tmp1int-1]->check_out())
						{
							sprintf(line, PLAYER_ERROR_STOPPED_TAG); //see mainloop.h
						}
						else
						{
							player[tmp1int-1]->readln(&line, &linesize);
						}
						if (player[tmp1int-1]->timedout())
						{
							sprintf(line, PLAYER_ERROR_TIMEOUT_TAG); //see mainloop.h
						}*/
						if (!receive_player_message(line, &linesize, tmp1int-1)) {
							if (check_alive(player[tmp1int-1]->process_ctx)) {
								sprintf(line, PLAYER_ERROR_TIMEOUT_TAG); //see mainloop.h
							} else {
								tmp2int = player[tmp1int-1]->process_ctx->exit_code;
								if (tmp2int < 2) { /* Windows exitcodes don't make much sense, best guess */
									sprintf(line, PLAYER_ERROR_STOPPED_TAG); //see mainloop.h
								} else {
									sprintf(line, PLAYER_ERROR_CRASHED_TAG, tmp2int); //see mainloop.h
									rewritespace(line);
								}
							}
						}
						/* modified */
						debug(DEBUG_MESSAGE_FROM_PLAYER, tmp1int, line);
						// if the players sends a "\n" the manager has a problem reading this with scanf():
						if (strlen(line) == 0)
						{
							sprintf(line, PLAYER_ERROR_ZEROSTRING_TAG); //see mainloop.h
						}
						debug(DEBUG_CAIAIO_SENDS_TO_MANAGER, line);
						//manager->writeln(line);
						send_message(process, line);
						break;

					case PLAYER_REQUEST_TIME:
						if (readint(&linep, word, &tmp1int))
						{
							if (strlen(word)) { fprflush(stderr, ERROR_INV_PLAY, STR_PLAYER_REQUEST_TIME, word);  }
							else { fprflush(stderr, ERROR_INC_PLAY, STR_PLAYER_REQUEST_TIME, line);  }
							state = CAIAIO_ERROR;
							break;
						}
						if (tmp1int < 1 || tmp1int > nplayers || player[tmp1int-1] == NULL)
						{
							fprflush(stderr, ERROR_PLAY_DNE, STR_PLAYER_REQUEST_TIME, tmp1int);
							state = CAIAIO_ERROR;
							break;
						}
						//sprintf(line, "time %d %d", time_player(tmp1int - 1), multiply(time_player(tmp1int - 1), 1 / time_factor));
						sprintf(line, "time %d %d", player[tmp1int - 1]->cpu_time_msec,
														multiply(player[tmp1int - 1]->cpu_time_msec, 1 / time_factor));
						debug(DEBUG_CAIAIO_SENDS_TO_MANAGER, line);
						//manager->writeln(line);
						send_message(process, line);
						break;

					default:
						debug(DEBUG_NEWLINE);
						fprflush(stderr, ERROR_UNKNOWN_CMD, "Manager", line);
						state = CAIAIO_ERROR;
						break;
				}
			}
			else for (i = 0; i < nplayers; ++i)
			{
				sprintf(tmp1word, "%d", i+1);
				if (!strcmp(word, tmp1word))
				{
					debug(DEBUG_CAIAIO_SENDS_TO_PLAYER, linep, i+1);
					//player[i]->writeln(linep);
					send_message(player[i]->process_ctx, linep);
					break;
				}
			}
		}

        if (!referee) {
            continue;
        }
		process = referee->process_ctx;
		//if (referee != NULL && referee->running()) referee->checkalive();
		if (process->state == PROCESS_ACTIVE) {
			check_alive(process);
		}
		//if (referee != NULL && !referee->running() && referee->bufferempty())
		if (process->state != PROCESS_ACTIVE &&	!process->buffer_ctx_in->data_length)
		{
			debug("closing referee\n");
			stop_referee();
			//referee->stop();
			//delete referee;
			referee = NULL;
			lock = NULL;
			continue;
		}

		//if (referee !=NULL && ((lock==NULL && referee->check_out()) || lock==referee)) // Added by Bauke Conijn
		//if ((lock==NULL || lock==referee) && referee !=NULL && referee->check_out())
		/*{
			strcpy(cmd_sender, "referee");
			referee->readln(&line, &linesize);
			if (referee->out_closed())
			{
				// referee heeft stdout afgesloten,
				// dit betekent de IO uit de mainloop gaat
				// in de referee NOOIT fclose(stdout) doen dus!!!
				// normaliter komt de IO hier als de referee afgesloten/gecrashed is
				//        debug("referee: out_closed\n");
				//        debug("closing referee\n");
				referee->stop();
				delete referee;
				referee = NULL;
				lock = NULL;
				continue;
			}*/
		linesize = 0;
		if (lock == NULL || lock==process) {
			receive_timed_process_message(line, &linesize, process, 10); // 0.01 secs
			if (!linesize && process->buffer_ctx_in->read_status == ERROR_BROKEN_PIPE) {
				// referee heeft stdout afgesloten,
				// dit betekent de IO uit de mainloop gaat
				// in de referee NOOIT fclose(stdout) doen dus!!!
				// normaliter komt de IO hier als de referee afgesloten/gecrashed is
				//        debug("referee: out_closed\n");
				//        debug("closing referee\n");
				//referee->stop();
				//delete referee;
                debug("closing referee\n");
				stop_referee();
				referee = NULL;
				lock = NULL;
				continue;
			}
		}
		/* modified */
		if (linesize)
		{
			strcpy(cmd_sender, "referee");
			/* modified
			manager->readln(&line, &linesize);
			if (manager->out_closed())
			{
				// manager closed stdout
				// this means that CAIAIO will exit the main loop
				// it's unwise to make a manager that executes fclose(stdout)!
				// normally the CAIAIO will only end up here when the manager crashed (or exits in rare cases)
				fprflush(stderr, "manager suddenly stopped!\n");
				break;
			}*/
			debug(DEBUG_MESSAGE_FROM_REFEREE, line);
			linep = line;
			readword(&linep, word);

			if (!strcmp(word, IO_NAME_TAG))
			{
				readword(&linep, word);
				state = state_parse_token(word);
				switch(state)
				{
					case CAIAIO_LOCK:
						//lock = manager;
						lock = process;
                        debug(DEBUG_SEND_OK_BACK_TO_REFEREE);
						//manager->writeln(RESP_LOCK_OK);
						send_message(process, RESP_LOCK_OK);
						break;

					case CAIAIO_UNLOCK:
						lock = NULL;
						debug(DEBUG_OK);
						break;

					case PLAYER_LISTEN:
						if (readint(&linep, word, &tmp1int))
						{
							if (strlen(word)) { fprflush(stderr, ERROR_INV_PLAY, STR_PLAYER_LISTEN, word);  }
							else { fprflush(stderr, ERROR_INC_PLAY, STR_PLAYER_LISTEN, line);  }
							state = CAIAIO_ERROR;
							break;
						}
						if (tmp1int < 1 || tmp1int > nplayers || player[tmp1int-1] == NULL)
						{
							fprflush(stderr, ERROR_PLAY_DNE, STR_PLAYER_LISTEN, tmp1int);
							state = CAIAIO_ERROR;
							break;
						}
						debug(DEBUG_OK);

						/* modified
						player[tmp1int-1]->checkalive();
						if (player[tmp1int-1]->crashed())
						{
							sprintf(line, PLAYER_ERROR_CRASHED_TAG, strsignal(player[tmp1int-1]->crashedsig())); //see mainloop.h
							rewritespace(line);
						}
						else if (!player[tmp1int-1]->running() && !player[tmp1int-1]->check_out())
						{
							sprintf(line, PLAYER_ERROR_STOPPED_TAG); //see mainloop.h
						}
						else
						{
							player[tmp1int-1]->readln(&line, &linesize);
						}
						if (player[tmp1int-1]->timedout())
						{
							sprintf(line, PLAYER_ERROR_TIMEOUT_TAG); //see mainloop.h
						}*/
						if (!receive_player_message(line, &linesize, tmp1int-1)) {
							if (check_alive(player[tmp1int-1]->process_ctx)) {
								sprintf(line, PLAYER_ERROR_TIMEOUT_TAG); //see mainloop.h
							} else {
								tmp2int = player[tmp1int-1]->process_ctx->exit_code;
								if (tmp2int < 2) { /* Windows exitcodes have no meaning, best guess */
									sprintf(line, PLAYER_ERROR_STOPPED_TAG); //see mainloop.h
								} else {
									sprintf(line, PLAYER_ERROR_CRASHED_TAG, tmp2int); //see mainloop.h
									rewritespace(line);
								}
							}
						}
						/* modified */
						debug(DEBUG_MESSAGE_FROM_PLAYER, tmp1int, line);
						// if the players sends a "\n" the manager has a problem reading this with scanf():
						if (strlen(line) == 0)
						{
							sprintf(line, PLAYER_ERROR_ZEROSTRING_TAG); //see mainloop.h
						}
						debug(DEBUG_CAIAIO_SENDS_TO_REFEREE, line);
						//manager->writeln(line);
						send_message(process, line);
						break;

					case PLAYER_REQUEST_TIME:
						if (readint(&linep, word, &tmp1int))
						{
							if (strlen(word)) { fprflush(stderr, ERROR_INV_PLAY, STR_PLAYER_REQUEST_TIME, word);  }
							else { fprflush(stderr, ERROR_INC_PLAY, STR_PLAYER_REQUEST_TIME, line);  }
							state = CAIAIO_ERROR;
							break;
						}
						if (tmp1int < 1 || tmp1int > nplayers || player[tmp1int-1] == NULL)
						{
							fprflush(stderr, ERROR_PLAY_DNE, STR_PLAYER_REQUEST_TIME, tmp1int);
							state = CAIAIO_ERROR;
							break;
						}
						//sprintf(line, "time %d %d", time_player(tmp1int - 1), multiply(time_player(tmp1int - 1), 1 / time_factor));
						sprintf(line, "time %d %d", player[tmp1int - 1]->cpu_time_msec,
														multiply(player[tmp1int - 1]->cpu_time_msec, 1 / time_factor));
						debug(DEBUG_CAIAIO_SENDS_TO_REFEREE, line);
						//manager->writeln(line);
						send_message(process, line);
						break;

					default:
						debug(DEBUG_NEWLINE);
						fprflush(stderr, ERROR_UNKNOWN_CMD, "Referee", line);
						state = CAIAIO_ERROR;
						break;
				}
			}
			else if (!strcmp(word, MANAGER_NAME_TAG))
			{
				debug(DEBUG_CAIAIO_SENDS_TO_MANAGER, linep);
				//manager->writeln(linep);
				send_message(manager->process_ctx, linep);
                manager_wait_time_ms = 5;
			}
			else for (i = 0; i < nplayers; ++i)
			{
				sprintf(tmp1word, "%d", i+1);
				if (!strcmp(word, tmp1word))
				{
					debug(DEBUG_CAIAIO_SENDS_TO_PLAYER, linep, i+1);
					//player[i]->writeln(linep);
					send_message(player[i]->process_ctx, linep);
					break;
				}
			}
		}
		/*
		for (i = 0; i < nplayers; i++)
		{
			if (player[i] != NULL)
			{
				while (player[i]->check_err())
				{
					if (player[i]->readlnerr(&line, &linesize) != -1) {
						debug("%d: %s\n", i+1, line);
						player[i]->write_logfile(line);
					}
				}
			}
		}*/
	}
	/*
	if (line != NULL) {
		delete[] line;
	}*/
	debug(DEBUG_MAINLOOP_ENDED);
}


