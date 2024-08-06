/****************************************************************************
 *
 * Copyright 2023 Samsung Electronics All Rights Reserved.
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

#include <iostream>
#include <memory>
#include <pthread.h>

using namespace std;

pthread_t gWorker;
pthread_t gBGWork;
extern "C"
{
	void* worker(void* arg)
	{
		struct sched_param sparam;
		sched_getparam(getpid(), &sparam);
		printf("vibhor public worker %d\n", sparam.sched_priority);
		return NULL;
	}

	void* bgwork(void* arg)
	{
		struct sched_param sparam;
		sched_getparam(getpid(), &sparam);
		printf("vibhor public bgwork %d\n", sparam.sched_priority);

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 8192);
		sparam.sched_priority = 150;
		pthread_attr_setschedparam(&attr, &sparam);
		int ret = pthread_create(&gWorker, &attr, worker, NULL);
		if (ret != 0) {
			printf("failed to create worker\n");
			return 0;
		} else {
			pthread_setname_np(gWorker, "bgaudio");
			printf("create worker OK\n");
		}

		return NULL;
	}

	int helloxx_main(int argc, char *argv[])
	{
		struct sched_param sparam;
		sched_getparam(getpid(), &sparam);
		printf("vibhor public helloxx_main %d\n", sparam.sched_priority);

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 8192);
		sparam.sched_priority = 120;
		pthread_attr_setschedparam(&attr, &sparam);
		int ret = pthread_create(&gWorker, &attr, bgwork, NULL);
		if (ret != 0) {
			printf("failed to create bgwork\n");
			return 0;
		} else {
			pthread_setname_np(gWorker, "bgaudio");
			printf("create bgwork OK\n");
		}

		while (true) {
			sleep(10);
		}
	}
}

