/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

//***************************************************************************
// Included Files
//***************************************************************************

#include <tinyara/config.h>

#include <cstdio>
#include <debug.h>

#include <tinyara/init.h>
#include <queue>
#include <functional>
#include <pthread.h>

pthread_mutex_t mutex;
pthread_cond_t cond;
pthread_mutex_t queueMutex;
pthread_cond_t queueCond;

std::queue<std::function<void()>> mQueueData;

int init()
{
	//Initialize mutex and cond
	if (pthread_mutex_init(&mutex, NULL) != 0) {
		printf("pthread_mutex_init failed for mutex\n");
		return -1;
	}
																				
	if (pthread_cond_init(&cond, NULL) != 0) {
		printf("pthread_cond_init failed for cond\n");
		return -1;
	}

	//Initialize queue mutex and queue cond
	if (pthread_mutex_init(&queueMutex, NULL) != 0) {
		printf("pthread_mutex_init failed for queueMutex\n");
		return -1;
	}                                                                     
																				
	if (pthread_cond_init(&queueCond, NULL) != 0) {
		printf("pthread_cond_init failed for queueCond\n");
		return -1;
	}

	return 0;
}

void createPlayer(int &ret)
{
	struct sched_param sparam;
	sched_getparam(getpid(), &sparam);
	printf("createPlayer priority %d\n", sparam.sched_priority);

	if (pthread_mutex_lock(&mutex) != 0) {
		printf("pthread_mutex_lock failed on mutex in createPlayer()\n");
		ret = -1;
		return;
	}
	if (pthread_cond_signal(&cond) != 0) {
    	printf("pthread_cond_signal failed on cond in createplayer()\n");
		ret = -1;
		return;
	}
	printf("@@@@@@ pthread_cond_signal done in createPlayer()\n");
	if (pthread_mutex_unlock(&mutex) != 0) {
		printf("pthread_mutex_unlock failed on mutex in createPlayer()\n");
		ret = -1;
		return;
	}
	printf("@@@@@@ createPlayer done\n");
}

void enQueue(void) {
	int ret = 0;
	// Add the function into queue
	if (pthread_mutex_lock(&queueMutex) != 0) {
		printf("pthread_mutex_lock failed on queueMutex in enQueue\n");
		return;
	}
	std::function<void()> func = std::bind(createPlayer, std::ref(ret));
	mQueueData.push(func);
	if (pthread_cond_signal(&queueCond) != 0) {
    	printf("pthread_cond_signal failed on queueCond in enQueue\n");
		return;
	}	
	if (pthread_mutex_unlock(&queueMutex) != 0) {
		printf("pthread_mutex_unlock failed on queueMutex in enQueue\n");
		return;
	}
	printf("@@@@@@ createPlayer enqueued done successfully\n");
}

std::function<void()> deQueue()
{
	if (pthread_mutex_lock(&queueMutex) != 0) {
		printf("pthread_mutex_lock failed on queueMutex in deQueue\n");
		return {};
	}
	printf("deQueue before empty\n");
	if (mQueueData.empty()) {
		printf("deQueue going for pthread_cond_wait\n");
		if (pthread_cond_wait(&queueCond, &queueMutex) != 0) {
			printf("pthread_cond_wait failed on queueCond & queueMutex in deQueue()\n");
			return {};
		}
	}

	auto data = std::move(mQueueData.front());
	mQueueData.pop();

	if (pthread_mutex_unlock(&queueMutex) != 0) {
		printf("pthread_mutex_unlock failed on queueMutex in deQueue\n");
		return {};
	}	
	return data;
}

// Worker thread
void* workerThread(void* arg)
{
	while (true) {
		std::function<void()> run = deQueue();
		if (run != nullptr) {
			run();
		}
	}
}

int create()
{
	int ret = 0;
	if (pthread_mutex_lock(&mutex) != 0) {
		printf("pthread_mutex_lock failed on mutex in create()\n");
		return -1;
	}

	// Create worker thread
	pthread_t thread;
	struct sched_param sparam;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 8192);
	sparam.sched_priority = 150;
	pthread_attr_setschedparam(&attr, &sparam);
	ret = pthread_create(&thread, &attr, workerThread, NULL);
	if (ret != OK) {
		medvdbg("Fail to create worker thread, return value : %d\n", ret);
		return NULL;
	}
	pthread_setname_np(thread, "WorkerThread");

	enQueue();

	printf("@@@@@@ Now, pthread_cond_wait in create()\n");
	if (pthread_cond_wait(&cond, &mutex) != 0) {
		printf("pthread_cond_wait failed on cond and mutex in create()\n");
		return -1;
	}
	printf("pthread_cond_wait completed in create()\n");
	if (pthread_mutex_unlock(&mutex) != 0) {
		printf("pthread_mutex_unlock failed on mutex in create()\n");
		return -1;
	}
	printf("@@@@@@ create done\n");

	return ret;
}

//Application thread
void* appThread(void* arg)
{
	// Initialize mutex and condition variable
	int ret = init();
	if (ret != 0) {
		printf("init failed\n");
		return NULL;
	}
	printf("init success\n");

	struct sched_param sparam;
	sched_getparam(getpid(), &sparam);
	printf("appThread priority %d\n", sparam.sched_priority);

	ret = create();
	if (ret != 0) {
		printf("create failed\n");
	}
	printf("create success\n");
	return NULL;
}

extern "C"
{
	int helloxx_main(int argc, char *argv[])
	{
		// Create application thread with priority 100.
		pthread_t mWorkerThread;
		struct sched_param sparam;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 8192);
		sparam.sched_priority = 120;
		pthread_attr_setschedparam(&attr, &sparam);
		int ret = pthread_create(&mWorkerThread, &attr, appThread, NULL);	
		if (ret != OK) {
			printf("Fail to create application thread, return value : %d\n", ret);
			return -1;
		}
		pthread_setname_np(mWorkerThread, "ApplicationThread");

		while (true) {
			sleep(10);
		}
	}
}

