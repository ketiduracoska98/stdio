/* Keti Duracoska 332 CB */

#include "util/so_stdio.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Function that allocate a structure S0_FILE */
SO_FILE *allocate_struct(int fileno)
{
	SO_FILE *stream = malloc(sizeof(SO_FILE));

	if (stream == NULL)
		return NULL;
	stream->fileno = fileno;
	stream->loc_file = 0;
	stream->endf = 0;
	stream->err = 0;
	stream->fgetc = 0;
	stream->loc_buf = 0;
	stream->read = 0;
	stream->prev_loc = 0;
	stream->fgetc_entered = 0;
	return stream;
}
/* Function that delete a structure */
void delete_strcut(SO_FILE *stream)
{
	free(stream);
}

/* Function which gets a pathname and a mode and open a file */
SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	int res;

	if (!strcmp(mode, "r")) {
		res = open(pathname, O_RDONLY);
		if (res == -1)
			return NULL;
	} else if (!strcmp(mode, "r+")) {
		res = open(pathname, O_RDWR);
		if (res == -1)
			return NULL;
	} else if (!strcmp(mode, "w")) {
		res = open(pathname, O_CREAT | O_TRUNC | O_WRONLY);
	} else if (!strcmp(mode, "w+")) {
		res = open(pathname, O_CREAT | O_TRUNC | O_RDWR);
	} else if (!strcmp(mode, "a")) {
		res = open(pathname, O_CREAT | O_APPEND  | O_WRONLY);
	} else if (!strcmp(mode, "a+")) {
		res = open(pathname, O_CREAT | O_RDWR | O_APPEND);
	} else {
		return NULL;
	}
	SO_FILE *stream = allocate_struct(res);

	if (stream == NULL)
		return NULL;
	return stream;
}
/* Close a file descriptor */
int so_fclose(SO_FILE *stream)
{
	int res, res_fflush;

	/* we read characters into a buffer, we need to flush them */
	if (stream->loc_buf != 0 && stream->fgetc == 0) {
		res_fflush = so_fflush(stream);
		if (res_fflush == SO_EOF) {
			close(stream->fileno);
			delete_strcut(stream);
			return SO_EOF;
		}
	}
	/* The buffer is empty, just close the file and delete the structure */
	res = close(stream->fileno);
	if (res == -1) {
		stream->endf = 1;
		stream->err = 1;
		delete_strcut(stream);
		return SO_EOF;
	}
	delete_strcut(stream);
	return 0;
}

/* Function which reads 4096(the maximum length of a buffer) characters */
/* Return the current one using stream->loc_buf to count the current position */
int so_fgetc(SO_FILE *stream)
{
	int res;

	stream->fgetc_entered++;
	if (stream->loc_buf == MAX_LEN)
		stream->loc_buf = 0;
	if (stream->loc_buf == 0) {
		res = read(stream->fileno, stream->buffer, MAX_LEN);
		stream->read = res;
		stream->fgetc_entered = 1;
		if (res == 0) {
			stream->endf = 1;
			stream->err = 1;
			return SO_EOF;
		}
		stream->fgetc = 1;
	}
	/* We reached end of file */
	if (stream->loc_buf < MAX_LEN && stream->read > 0 && stream->fgetc_entered > stream->read) {
		stream->endf = 1;
		stream->err = 1;
		return SO_EOF;
	}
	return (int) stream->buffer[stream->loc_buf++];
}

/* "Put" a character into a buffer */
/* If a buffer is full it fllush them in the file */
int so_fputc(int c, SO_FILE *stream)
{
	if (stream->loc_buf >= MAX_LEN)
		so_fflush(stream);
	stream->buffer[stream->loc_buf++] = c;
	return c;
}

/* Function which reads characters from a file and saves it to ptr */
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int res, new_res = -1, nr_of_bytes = nmemb, j = 0, i, count = 0;
	void *temp = calloc(nmemb + stream->loc_file + 1, size);
	void *temp_buf = calloc(1, size);

	if (temp == NULL)
		return SO_EOF;
	if (temp_buf == NULL)
		return SO_EOF;
	if (stream->prev_loc > stream->loc_file) {
		for (i = stream->loc_file - stream->read; i < stream->loc_file + nmemb - stream->read; i++, j++)
			((char *)ptr)[j] = stream->buffer[i];
		free(temp);
		free(temp_buf);
		return nr_of_bytes;
	}
	/* Length required for reading is less than max length(4096) */
	if (size * nmemb + stream->loc_file < MAX_LEN)
		res = read(stream->fileno, ptr, size * nmemb + stream->loc_file);
	/* Read only MAX_LEN characters*/
	else {
		res = read(stream->fileno, ptr, MAX_LEN);
		((char *)ptr)[res] = '\0';
		for (i = res; i < MAX_LEN; i++) {
			res += read(stream->fileno, temp_buf, 1);
			((char *)ptr)[i] = ((char *)temp_buf)[0];
		}
	}
	if (res == -1) {
		stream->endf = 1;
		stream->err = 1;
		free(temp_buf);
		free(temp);
		return 0;
	}
	count = res;
	/* Save the first part in temp variable */
	((char *)ptr)[res] = '\0';
	strncpy(temp, ptr, res);
	/* Continue reading while required length */
	while (size * nmemb + stream->loc_file > MAX_LEN) {
		nmemb -= MAX_LEN;
		memset(ptr, 0, MAX_LEN);
		if (size * nmemb + stream->loc_file < MAX_LEN) {
			new_res = read(stream->fileno, ptr, size * nmemb + stream->loc_file - stream->read);
			count += new_res;
			for (i = new_res; i < size * nmemb + stream->loc_file - stream->read; i++) {
				new_res += read(stream->fileno, temp_buf, 1);
				((char *)ptr)[i] = ((char *)temp_buf)[0];
				count++;
			}
		}

		else {
			new_res = read(stream->fileno, ptr, MAX_LEN);
			((char *)ptr)[new_res] = '\0';
			count += new_res;
			for (i = new_res; i < MAX_LEN; i++) {
				new_res += read(stream->fileno, temp_buf, 1);
				((char *)ptr)[i] = ((char *)temp_buf)[0];
				count++;
			}
		}
		res += new_res;
		strcat(temp, ptr);
	}

	if (new_res == 0)
		stream->endf = 1;
	res = res - stream->loc_file + stream->read;
	/* Copy the read buffer to ptr*/
	for (i = stream->loc_file - stream->read; i < stream->loc_file + nr_of_bytes; i++, j++)
		((char *)ptr)[j] = ((char *)temp)[i];


	stream->loc_file = res;
	stream->read = res;
	stream->prev_loc = stream->loc_file;
	strncpy(stream->buffer, temp, MAX_LEN);
	free(temp);
	free(temp_buf);
	return res;
}

/* Write a characters in a buffer, if length is 4096 flush it in the file */
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int res = 0, i;

	for (i = 1; i <= nmemb; i++) {
		stream->buffer[stream->loc_buf] = ((char *)ptr)[i - 1];
		stream->loc_buf++;
		stream->loc_file++;
		res++;
		if (i % MAX_LEN == 0)
			so_fflush(stream);
	}
	return res;
}
/* Change the current location in a file using stream->loc_file variable */
int so_fseek(SO_FILE *stream, long offset, int whence)
{
	stream->prev_loc = stream->loc_file;
	if (whence == 0) {
		stream->loc_file = offset;
	} else if (whence == 1) {
		stream->loc_file += offset;
	} else if (whence == 2) {
		char *temp_buf = calloc(4097, 1);
		int res, len = 0;

		while ((res = read(stream->fileno, temp_buf, 1)))
			len += res;
		stream->loc_file = len;
		free(temp_buf);
	}
	return 0;
}
/* Return the current location in a file which is saved in stream->loc_file */
long so_ftell(SO_FILE *stream)
{
	return stream->loc_file;
}

/* Prints the buffer in a file */
int so_fflush(SO_FILE *stream)
{
	int res;

	res = write(stream->fileno, stream->buffer, stream->loc_buf);
	stream->prev_loc = stream->loc_buf; // ova e novo
	stream->loc_buf = 0;
	if (res == -1) {
		stream->endf = 1;
		stream->err = 1;
		return SO_EOF;
	}
	return 0;
}
/* Return the file descriptor */
int so_fileno(SO_FILE *stream)
{
	return stream->fileno;
}

int so_feof(SO_FILE *stream)
{
	if (stream->endf == 1)
		return 1;
	return 0;
}

int so_ferror(SO_FILE *stream)
{
	if (stream->err == 1)
		return 1;
	return 0;
}
SO_FILE *so_popen(const char *command, const char *type)
{
	return 0;
}
int so_pclose(SO_FILE *stream)
{
	return 0;
}
