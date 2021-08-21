#include <stdio.h>
#include <string.h>
#include "command.h"

int nplayers = 0;
struct manager *manager = NULL;
struct referee *referee = NULL;
struct player **player = NULL;

int get_single_message(char *message, unsigned long *length, char *messages, unsigned long *data_length);

static char *string_dup(const char *src)
{
	char *dest = NULL;
	int length = 0;

	if (!src) {
		return NULL;
	}
	length = strlen(src) + 1;
	if (!(dest = malloc(length * sizeof(*src)))) {
		return NULL;
	}
	strncpy(dest, src, length);

	return dest;
}

void free_string(char **string)
{
	if (!string || !*string) {
		return;
	}
	free(*string);
	*string = NULL;

	return;
}

char create_manager(const char *executable, const char *information)
{
	if (!(manager = calloc(1, sizeof(*manager)))) {
		return FAILURE;
	}
	if (!executable || !*executable || !(manager->executable = string_dup(executable))) {
		return FAILURE;
	}
	if (information && *information && !(manager->information = string_dup(information))) {
		return FAILURE;
	}
	if (!(manager->process_ctx = create_process_ctx())) {
		return FAILURE;
	}
	return SUCCESS;
}


char create_referee(const char *executable, const char *logfile)
{
	if (!(referee = calloc(1, sizeof(*referee)))) {
		return FAILURE;
	}
	if (!executable || !*executable || !(referee->executable = string_dup(executable))) {
		return FAILURE;
	}
	if (logfile && *logfile && !(referee->logfile = string_dup(logfile))) {
		return FAILURE;
	}
	if (!(referee->process_ctx = create_process_ctx())) {
		return FAILURE;
	}
	return SUCCESS;
}


char create_player_array(unsigned int number_players)
{
	if (!(player = calloc(number_players, sizeof(*player)))) {
		return FAILURE;
	}
	nplayers = number_players;

	return SUCCESS;
}


char create_player(unsigned int player_id, char *executable, int total_time_msec, char *logfile)
{
	struct player *program = NULL;

	if (!(program = calloc(1, sizeof(*program)))) {
		return FAILURE;
	}
	player[player_id] = program;

	program->number = player_id + 1;
	program->total_time_msec = total_time_msec;
	if (!executable || !*executable || !(program->executable = string_dup(executable))) {
		return FAILURE;
	}
	if (logfile && *logfile && !(program->logfile = string_dup(logfile))) {
		return FAILURE;
	}
	if (!(program->process_ctx = create_process_ctx())) {
		return FAILURE;
	}

	return SUCCESS;
}


char start_manager()
{
	if (!manager || !manager->process_ctx || !manager->executable) {
		return FAILURE;
	}
	// manager has no stderr redirection
	if (!spawn_suspended_process(manager->process_ctx, manager->executable, manager->information, 0)) {
		return FAILURE;
	}
	return SUCCESS;
}

void stop_manager()
{
	if (!manager) {
		return;
	}
	terminate_process(manager->process_ctx);
	free_process_ctx(&manager->process_ctx);
	free_string(&manager->executable);
	free_string(&manager->information);
	free(manager);
	manager = NULL;
	return;
}


char start_referee()
{
	if (!referee || !referee->process_ctx || !referee->executable) {
		return FAILURE;
	}
	// referee has no stderr redirection
	if (!spawn_suspended_process(referee->process_ctx, referee->executable, referee->logfile, 0)) {
		return FAILURE;
	}
	return SUCCESS;
}


void stop_referee()
{
	if (!referee) {
		return;
	}
	terminate_process(referee->process_ctx);
	free_process_ctx(&referee->process_ctx);
	free_string(&referee->executable);
	free_string(&referee->logfile);
	free(referee);
	referee = NULL;
	return;
}


char start_player(int player_id, int firsterror_time_msec)
{
	struct process_ctx *process_ctx = NULL;
	struct player *program = NULL;
	unsigned long length;

	if (!player || !player[player_id] || !player[player_id]->process_ctx) {
		return FAILURE;
	}
	program = player[player_id];
	process_ctx = program->process_ctx;
	program->cpu_time_msec = 0;
	program->firsterror_time_msec = firsterror_time_msec;

	if (!spawn_suspended_process(process_ctx, program->executable, NULL, 1)) {
		return FAILURE;
	}
	if (!create_stderr_async_ctx(process_ctx, player_id + 1)) {
        return FAILURE;
	}
	if (program->logfile && !create_write_file(program->logfile, &process_ctx->stderr_async_ctx->logfile)) {
        fprintf(stderr, "Could not create logfile %s for player %d\n", program->logfile, player_id + 1);
		return FAILURE;
	}
	if (firsterror_time_msec) {
		process_ctx->buffer_ctx_in->buffer[0] = '\0';
		process_ctx->buffer_ctx_in->data_length = 0;
		if (!get_firsterror(process_ctx, program->firsterror_time_msec, &length)) {
			return FAILURE;
		}
		if (program->logfile && length) {
			// write message to logfile
			if (!write_to_handle(process_ctx->stderr_async_ctx->logfile,
									process_ctx->stderr_async_ctx->pipe_buffer, length)) {
				return FAILURE;
			}
		}
		// copy first line to buffer (if linefeed found and short enough), Strips original, so do this last
		if (length < BUF_SIZE) {
			get_single_message(process_ctx->buffer_ctx_in->buffer, &process_ctx->buffer_ctx_in->data_length,
								process_ctx->stderr_async_ctx->pipe_buffer, &length);
		}
	}
	// activate players async stderr pipe read end completion routine callback
	if (!activate_async_read_callback(process_ctx->stderr_async_ctx)) {
		return FAILURE;
	}

	return SUCCESS;
}

void stop_player(int player_id)
{
    if (!player || player_id >= nplayers || !player[player_id]) {
        return;
    }
	terminate_process(player[player_id]->process_ctx);
    free_process_ctx(&player[player_id]->process_ctx);
	free_string(&player[player_id]->executable);
	free_string(&player[player_id]->logfile);
	free(player[player_id]);
	player[player_id] = NULL;
	return;
}


void stop_players()
{
	int i;

	if (!nplayers || !player) {
		return;
	}
	for (i = 0; i <nplayers; i++) {
		stop_player(i);
	}
	free(player);
	player = NULL;
}


char send_message(struct process_ctx *process_ctx, char *message)
{
    unsigned long length;
	char pipe_message[BUF_SIZE];

	if (!message || !process_ctx) {
		return FAILURE;
	}
	length = strlen(message);
	if (!length || length >= BUF_SIZE) {
		return FAILURE;
	}
	strncpy(pipe_message, message, length);
	pipe_message[length] = '\n';
	length++;

	if (!write_to_handle(process_ctx->write_stdin, pipe_message, length)) {
		return FAILURE;
	}

	return SUCCESS;
}


char receive_timed_process_message(char *message, unsigned long *length, struct process_ctx *process_ctx, int max_wait_msec)
{
	*length = 0;
	*message = '\0';
	if (!process_ctx->buffer_ctx_in->data_length) {
		if (!read_from_timed_process(process_ctx, max_wait_msec)) {
			return FAILURE;
		}
	}
	if (SUCCESS != get_single_message(message, length,
										process_ctx->buffer_ctx_in->buffer,
										&process_ctx->buffer_ctx_in->data_length)) {
		return FAILURE;
	}

	return SUCCESS;
}


char receive_player_message(char *message, unsigned long *length, int player_number)
{
	int max_wait_msec = player[player_number]->total_time_msec - player[player_number]->cpu_time_msec;

	if (!receive_timed_process_message(message, length, player[player_number]->process_ctx, max_wait_msec)) {
        if (player[player_number]->process_ctx->buffer_ctx_in->read_status == ERROR_BROKEN_PIPE) {
            fprintf(stderr, "Could not read from stdin of program %s\n", player[player_number]->executable);
        }
		return FALSE;
	}
	player[player_number]->cpu_time_msec += player[player_number]->process_ctx->response_time_msec;

	return TRUE;
}

int get_single_message(char *message, unsigned long *length, char *messages, unsigned long *data_length)
{
	int pos, delim_len = 1;
	char *token, *src;

	if (!messages || !*messages || !data_length || !*data_length) {
		return BUF_ERROR_NO_DATA;
	}
	token = strchr(messages, '\n');
	if (!token) {
		token = strchr(messages, '\r');
	}
	if (!token) {
		return BUF_ERROR_NO_EOL;
	}
	pos = token - messages;
	if (pos && messages[pos - 1] == '\r') {
		delim_len = 2; // \r\n
		token--;
		pos--;
	}
	if (!pos) {
		return BUF_ERROR_EMPTY_LINE;
	}
	src = messages;
	while (src != token) {
		*message++ = *src++;
	}
	*message = '\0';
	*length = pos;
	src = token + delim_len;
	while (*src) {
		*messages++ = *src++;
	}
	*messages = '\0';
	*data_length -= pos + delim_len;

	return SUCCESS;
}
