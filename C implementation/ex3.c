/*
 * ex3.c
 */

#define _GNU_SOURCE

#include <curl/curl.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#define REQUEST_TIMEOUT_SECONDS 2L

#define URL_OK 0
#define URL_ERROR 1
#define URL_UNKNOWN 2

#define QUEUE_SIZE 32

#define handle_error_en(en, msg) \
		do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct {
	int ok, error, unknown;
} UrlStatus;


typedef struct {
	void **array;
	int size;
	int capacity;
	int head;
	int tail;
	pthread_mutex_t mutex;
	pthread_cond_t cv_empty; /* get notified when the queue is not full */
	pthread_cond_t cv_full; /* get notified when the queue is not empty */
} Queue;

void queue_init(Queue *queue, int capacity) {
	/*
	 * Initializes the queue with the specified capacity.
	 * This function should allocate the internal array, initialize its properties
	 * and also initialize its mutex and condition variables.
	 */
	queue->array = (void**)malloc(sizeof(void*) * capacity);
	if (queue->array == NULL) {
		perror("unable to allocate memory");
		exit(EXIT_FAILURE);
	}
	queue->capacity = capacity;
	queue->size = 0;
	queue->head = 0;
	queue->tail = 0;
	pthread_mutex_init(&(queue->mutex), NULL);
	pthread_cond_init(&(queue->cv_empty), NULL);
	pthread_cond_init(&(queue->cv_full), NULL);
}

void enqueue(Queue *queue, void *data) {
	/*
	 * Enqueue an object to the queue.
	 *
	 * TODO:
	 * 1. This function should be synchronized on the queue's mutex
	 * 2. If the queue is full, it should wait until it is not full
	 *      (i.e. cv_empty)
	 * 3. Add an element to the tail of the queue, and update the tail & size
	 *      parameters
	 * 4. Signal that the queue is not empty (i.e. cv_full)
	 */
	int ret;


	ret = pthread_mutex_lock(&(queue->mutex));
	/* checks if mutex lock was successful */
	if (ret != 0){
		handle_error_en(ret, "unable to lock mutex");
	}
	if (queue->size == queue->capacity){
		ret = pthread_cond_wait(&(queue->cv_empty), &(queue->mutex));
		/* checks if wait was successful */
		if (ret != 0){
			handle_error_en(ret, "unable to make the thread to wait");
		}
	}

	/* -- CRITICAL SECTION -- */
	queue->array[queue->tail] = data;
	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->size++;
	/* -- END OF CRITICAL SECTION -- */

	ret = pthread_mutex_unlock(&(queue->mutex));
	if (ret != 0){
		handle_error_en(ret, "unable to unlock mutex");
	}
	ret = pthread_cond_signal(&(queue->cv_full));
	if (ret != 0){
		handle_error_en(ret, "unable to to signal that queue is not empty");
	}
}

void *dequeue(Queue *queue) {
	/*
	 * Dequeue an object from the queue.
	 *
	 * TODO:
	 * 1. This function should be synchronized on the queue's mutex
	 * 2. If the queue is empty, it should wait until it is not empty (i.e. cv_full)
	 * 3. Read the head element, and update the head & size parameters
	 * 4. Signal that the queue is not full (i.e. cv_empty)
	 * 5. Return the dequeued item
	 */
	void *data;
	int ret;

	ret = pthread_mutex_lock(&(queue->mutex));
	/* checks if mutex lock was successful */
	if (ret != 0){
		handle_error_en(ret, "unable to lock mutex");
	}
	if (queue->size == 0){
		ret = pthread_cond_wait(&(queue->cv_full), &(queue->mutex));
		/* checks if wait was successful */
		if (ret != 0){
			handle_error_en(ret, "unable to make the thread to wait");
		}
	}

	/* -- CRITICAL SECTION -- */
	data = queue->array[queue->head];
	queue->head = (queue->head + 1) % queue->capacity;
	queue->size--;
	/* -- END OF CRITICAL SECTION -- */

	ret = pthread_mutex_unlock(&(queue->mutex));
	if (ret != 0){
		handle_error_en(ret, "unable to unlock mutex");
	}
	ret = pthread_cond_signal(&(queue->cv_empty));
	if (ret != 0){
		handle_error_en(ret, "unable to to signal that queue is empty");
	}
	return data;
}

void queue_destroy(Queue *queue) {
	/*
	 * Free the queue memory and destroy the mutex and the condition variables.
	 */
	int ret;

	free(queue->array);

	ret = pthread_mutex_destroy(&(queue->mutex));
	if (ret != 0) {
		handle_error_en(ret, "unable to destroy mutex");
	}
	ret = pthread_cond_destroy(&(queue->cv_empty));
	if (ret != 0) {
		handle_error_en(ret, "unable to destroy cv_empty condition variable");
	}
	ret = pthread_cond_destroy(&(queue->cv_full));
	if (ret != 0) {
		handle_error_en(ret, "unable to destroy cv_full condition variable");
	}
}

void usage() {
	fprintf(stderr, "usage:\n\t./ex3 FILENAME NUMBER_OF_THREADS\n");
	exit(EXIT_FAILURE);
}

int check_url(const char *url) {
	CURL *curl;
	CURLcode res;
	long response_code = 0L;
	int http_status = URL_UNKNOWN;

	curl = curl_easy_init();

	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, REQUEST_TIMEOUT_SECONDS);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); /* do a HEAD request */

		res = curl_easy_perform(curl);
		if(res == CURLE_OK) {
			res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

			if (res == CURLE_OK &&
					response_code >= 200 &&
					response_code < 400) {
				http_status = URL_OK;
			} else {
				http_status = URL_ERROR;
			}
		}
		curl_easy_cleanup(curl);
	}

	return http_status;
}

typedef struct {
	Queue *url_queue;
	Queue *result_queue;
} WorkerArguments;

void *worker(void *args) {
	/*
	 * TODO:
	 * 1. Initialize a UrlStatus (allocate memory using malloc, and zero it
	 *      using memset)
	 * 2. Dequeue URL from url_queue, run check_url on it, and update results
	 *    (don't forget to free() the url)
	 * 3. After dequeuing a NULL value:
	 *      Enqueue results to result_queue and return
	 */
	WorkerArguments *worker_args = (WorkerArguments *)args;
	UrlStatus *results = NULL;
	char *url;
	results = (UrlStatus*)malloc(sizeof(UrlStatus));
	if (results == NULL) {
		perror("unable to allocate memory");
		exit(EXIT_FAILURE);
	}
	/* setting all values in UrlStatus to 0 */
	memset(results, 0, sizeof(UrlStatus));
	url = dequeue(worker_args->url_queue);
	/* while url queue is not empty - keep running */
	while(url != NULL){
		switch (check_url(url)) {
		case URL_OK:
			results->ok += 1;
			break;
		case URL_ERROR:
			results->error += 1;
			break;
		default:
			results->unknown += 1;
		}
		free(url);
		url = dequeue(worker_args->url_queue);
	}

	/* Enqueue results (of type UrlStatus) to results queue */
	enqueue(worker_args->result_queue, results);

	return NULL;
}

typedef struct {
	const char *filename;
	Queue *url_queue;
} FileReaderArguments;

void *file_reader(void *args) {
	/*
	 * TODO:
	 * 1. Open filename (use fopen, check for errors)
	 * 2. Use getline() to read lines (i.e. URLs) from the file (use errno to check for errors)
	 * 3. Copy each url to the heap (use malloc and strncpy)
	 * 4. Enqueue the URL to url_queue
	 * 5. Don't forget to free the line variable, and close the file (and check for errors!)
	 */
	FileReaderArguments *file_reader_args = (FileReaderArguments *)args;
	FILE *toplist_file;
	char *line = NULL;
	char *url = NULL;
	size_t len = 0;
	ssize_t read = 0;

	toplist_file = fopen(file_reader_args->filename, "r");
	/* check if fopen was successful */
	if (toplist_file == NULL) {
		exit(EXIT_FAILURE);
	}

	while ((read = getline(&line, &len, toplist_file)) != -1) {
		if (read == -1){
			handle_error_en(read, "unable to read from file");
		}
		line[read-1] = '\0'; /* null-terminate the URL */
		/* allocating memory on the heap of the url */
		url = (char*)malloc(sizeof(char)*read);
		if (url == NULL) {
			perror("unable to allocate memory");
			exit(EXIT_FAILURE);
		}
		/* using strncpy as requested to copy line to allocated url */
		strncpy(url, line, strlen(line));
		enqueue(file_reader_args->url_queue, url);
	}

	free(line);
	fclose(toplist_file);

	return NULL;
}

typedef struct {
	int number_of_threads;
	Queue *url_queue;
	Queue *result_queue;
} CollectorArguments;

void *collector(void *args) {
	/*
	 * TODO:
	 * 1. Enqueue number_of_threads NULLs to the url_queue
	 * 2. Dequeue and aggregate number_of_threads thread_results
	 *      from result_queue into results (don't forget to free() thread_results)
	 * 3. Print aggregated results to the screen
	 */
	CollectorArguments *collector_args = (CollectorArguments *)args;
	UrlStatus results = {0};
	UrlStatus *thread_results;
	int i;

	/* Enqueue number_of_threads NULLs to the url_queue */
	for(i = 0; i < collector_args->number_of_threads; i++){
		enqueue(collector_args->url_queue, NULL);
	}

	/* Dequeue and place in thread_results */
	for(i = 0; i < collector_args->number_of_threads; i++){
		thread_results = dequeue(collector_args->result_queue);
		results.ok += thread_results->ok;
		results.error += thread_results->error;
		results.unknown += thread_results->unknown;
	}

	printf("%d OK, %d Error, %d Unknown\n",
			results.ok,
			results.error,
			results.unknown);
	return NULL;
}

void parallel_checker(const char *filename, int number_of_threads) {
	/*
	 * TODO:
	 * 1. Initialize a Queue for URLs, a Queue for results (use QUEUE_SIZE)
	 * 2. Start number_of_threads threads running worker()
	 * 3. Start a thread running file_reader(), and join it
	 * 4. Start a thread running collector(), and join it
	 * 5. Join all worker threads
	 * 6. Destroy both queues
	 */
	Queue url_queue, result_queue;
	WorkerArguments worker_arguments = {0};
	FileReaderArguments file_reader_arguments = {0};
	CollectorArguments collector_arguments = {0};
	pthread_t *worker_threads;
	pthread_t file_reader_thread, collector_thread;
	int i, ret;

	worker_threads = (pthread_t *) malloc(sizeof(pthread_t) * number_of_threads);
	if (worker_threads == NULL) {
		perror("unable to allocate memory");
		return;
	}

	curl_global_init(CURL_GLOBAL_ALL); /* init libcurl before starting threads */

	/* Initialize a Queue for URLs and for results */
	queue_init(&url_queue ,QUEUE_SIZE);
	queue_init(&result_queue ,QUEUE_SIZE);

	/* Initialize worker_arguments to send to workers */
	worker_arguments.result_queue = &result_queue;
	worker_arguments.url_queue = &url_queue;

	/* Start number_of_threads threads running worker() */
	for(i = 0; i < number_of_threads; i++) {
		ret = pthread_create(&(worker_threads[i]), NULL, &worker, &worker_arguments);
		if (ret != 0) {
			handle_error_en(ret, "unable to create thread");
		}
	}

	/* Initialize file_reader_arguments to send to file_reader */
	file_reader_arguments.filename = filename;
	file_reader_arguments.url_queue = &url_queue;

	/* Start a thread running file_reader(), and join it */
	ret = pthread_create(&(file_reader_thread), NULL, &file_reader, &file_reader_arguments);
	if (ret != 0) {
		handle_error_en(ret, "unable to create thread");
	}
	pthread_join(file_reader_thread, NULL);

	/* Initialize collector_arguments to send to */
	collector_arguments.number_of_threads = number_of_threads;
	collector_arguments.url_queue = &url_queue;
	collector_arguments.result_queue = &result_queue;

	/* Start a thread running collector(), and join it */
	ret = pthread_create(&(collector_thread), NULL, &collector, &collector_arguments);
	if (ret != 0) {
		handle_error_en(ret, "unable to create thread");
	}
	pthread_join(collector_thread, NULL);

	/* Join all worker threads */
	for (i = 0; i < number_of_threads; i++) {
		pthread_join(worker_threads[i],NULL);
	}

	/* Destroying Queues */
	queue_destroy(&url_queue);
	queue_destroy(&result_queue);

	free(worker_threads);

}

int main(int argc, char **argv) {
	if (argc != 3) {
		usage();
	} else {
		parallel_checker(argv[1], atoi(argv[2]));
	}

	return EXIT_SUCCESS;
}
