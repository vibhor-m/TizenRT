/****************************************************************************
 *
 * Copyright 2026 Samsung Electronics All Rights Reserved.
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

#include <stdio.h>
#include <debug.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "mqueue.h"
#include <pthread.h>
#include <sys/ioctl.h>
#include "csifw_sample.h"
#include <tinyara/config.h>
#include <tinyara/wifi_csi/wifi_csi_struct.h>
#include <tinyara/wifi_csi/wifi_csi.h>
#include <semaphore.h>

pthread_t csi_data_receiver_th;
mqd_t mq_handle;
unsigned int data_receiver_fd;
unsigned char *get_data_buffptr;
csi_config_type_t configuration_type;
int collection_interval_ms;
bool collection_started = false;
bool csi_collection_flag = false;
static int csi_data_file_fd = -1;

#define RCVR_TH_NAME "csifw_data_receiver_th"
#define CSIFW_MAX_RAW_BUFF_LEN 1024


int csi_packet_receiver_set_csi_config(csi_config_action_t config_action)
{
	int fd, ret = 1;
	OPEN_DRIVER(fd);
	csi_config_args_t config_args;
	config_args.config_action = config_action;
	config_args.config_type = configuration_type;
	config_args.interval = collection_interval_ms;
	int res = ioctl(fd, CSIIOC_SET_CONFIG, (unsigned long)&config_args);
	if (res < OK) {
		printf("Failed to set CSI config (action: %d, type: %d, interval: %d) - errno: %d (%s)", config_action, config_args.config_type, config_args.interval, get_errno(), strerror(get_errno()));
	} else {
		printf("IOCTL : CSIIOC_SET_CONFIG, Success");
	}
	CLOSE_DRIVER(fd)
	return ret;
}

static int readCSIData(int fd, unsigned char *buf, int size)
{
	int err = 0;
	csi_driver_buffer_args_t buf_args;
	buf_args.buflen = size;
	buf_args.buffer = buf;
	err = ioctl(fd, CSIIOC_GET_DATA, (unsigned long)&buf_args);
	if (buf_args.buflen > 0) {
		ssize_t bytes_written = write(csi_data_file_fd, get_data_buffptr, buf_args.buflen);
		if (bytes_written != buf_args.buflen) {
			printf("Failed to write CSI data to file: written=%zd, expected=%d, errno: %d (%s)\n",bytes_written, buf_args.buflen, get_errno(), strerror(get_errno()));
		}
	}	
	if (err <= OK) {
		printf("IOCTL CSIIOC_GET_DATA failed - fd: %d, errno: %d (%s)\n", fd, get_errno(), strerror(get_errno()));
	}
	return err;
}

static void *dataReceiverThread(void *vargp)
{
	int len;
	int prio;
	size_t size;
	int csi_data_len = 0;
	struct wifi_csi_msg_s msg;
	int fd = data_receiver_fd;

	while (csi_collection_flag) {
		size = mq_receive(mq_handle, (char *)&msg, sizeof(msg), &prio);
		if (size != sizeof(msg)) {
			printf("Interrupted while waiting for deque message from kernel %zu, errno: %d (%s)", size, get_errno(), strerror(get_errno()));
		} else {
			switch (msg.msgId) {
			case CSI_MSG_DATA_READY_CB:
                printf("CSI_MSG_DATA_READY_CB\n");
				if (msg.data_len == 0 || msg.data_len > CSIFW_MAX_RAW_BUFF_LEN) {
					printf("Skipping packet: invalid data length: %d\n", msg.data_len);
					continue;
				}
				csi_data_len = msg.data_len;
				len = readCSIData(fd, get_data_buffptr, csi_data_len);
				if (len < 0) {
					printf("Skipping packet: error: %d\n", len);
					continue;
				}
                printf("received data get_data_buffptr[%d],len[%d]\n",csi_data_len,len);
				break;

			case CSI_MSG_ERROR:
				printf("CSI_MSG_ERROR received");
				break;

			default:
				printf("Received unknown message ID: %d", msg.msgId);
				break;
			}
		}
	}
	printf("[THREAD] : EXIT");
	return NULL;
}

int csi_packet_receiver_start_collect(void)
{
	int res = csi_packet_receiver_set_csi_config(CSI_CONFIG_ENABLE);
	if (res != 0) {
		printf("config set success.\n");
	} else {
		printf("config set fail.\n");
	}

    get_data_buffptr = (unsigned char *)malloc(CSIFW_MAX_RAW_BUFF_LEN);
    if (!get_data_buffptr) {
        printf("Buffer allocation Fail.");
        return 0;
    }
    printf("Get data buffer allocation done, size: %d\n", CSIFW_MAX_RAW_BUFF_LEN);

	//create receiver thread
	pthread_attr_t recv_th_attr;
	if (pthread_attr_init(&recv_th_attr) != 0) {
		printf("Failed to initialize receiver thread attributes - errno: %d (%s)", get_errno(), strerror(get_errno()));
		csi_packet_receiver_set_csi_config(CSI_CONFIG_DISABLE);
		return 0;
	}

	if (pthread_attr_setstacksize(&recv_th_attr, (3*1024)) != 0) {
		printf("Failed to set receiver thread stack size to 5KB - errno: %d (%s). Proceeding with default stack size.", get_errno(), strerror(get_errno()));
	}

	if (pthread_create(&csi_data_receiver_th, &recv_th_attr, dataReceiverThread, NULL) != 0) {
		printf("Failed to create CSI data receiver thread - errno: %d (%s)", get_errno(), strerror(get_errno()));
		csi_packet_receiver_set_csi_config(CSI_CONFIG_DISABLE);
		free(get_data_buffptr);
		get_data_buffptr = NULL;
		return 0;
	}

	if (pthread_setname_np(csi_data_receiver_th, RCVR_TH_NAME) != 0) {
		printf("Failed to set receiver thread name - errno: %d (%s)\n", get_errno(), strerror(get_errno()));
	}
	printf("CSI Data_Receive_Thread created (thread ID: %lu)\n", (unsigned long)csi_data_receiver_th);
	return 1;
}


int csi_packet_receiver_initialize(void)
{
	int fd;
	mq_handle = (mqd_t) ERROR; //= p_csifw_ctx->mq_handle;
	struct mq_attr attr_mq;
	attr_mq.mq_maxmsg = CSI_MQ_MSG_COUNT;
	attr_mq.mq_msgsize = sizeof(struct wifi_csi_msg_s);
	attr_mq.mq_flags = 0;
	printf("MQ_NAME: %s, MSG_SIZE: %zu, MQ_TYPE: BLOCKING\n", CSI_MQ_NAME, attr_mq.mq_msgsize);
	printf("Opening message queue: name=%s, maxmsg=%ld, msgsize=%ld \n",CSI_MQ_NAME, attr_mq.mq_maxmsg, attr_mq.mq_msgsize);
	mq_handle = mq_open(CSI_MQ_NAME, O_RDWR | O_CREAT, 0666, &attr_mq);
	if (mq_handle == (mqd_t) ERROR) {
		printf("Failed to open message queue %s. errno: %d (%s) \n", CSI_MQ_NAME, get_errno(), strerror(get_errno()));
		return -1;
	}
	printf("Message queue opened successfully (mq_handle=%p) \n", mq_handle);
	OPEN_DRIVER(fd)
	printf("Starting CSI data collection via IOCTL (fd=%d)\n", fd);
	int ret = ioctl(fd, CSIIOC_START_CSI, NULL);
	if (ret < OK) {
		printf("IOCTL : CSIIOC_START_CSI Failed errno: %d (%s)", get_errno(), strerror(get_errno()));
		mq_close(mq_handle);
		mq_handle = (mqd_t) ERROR;
		CLOSE_DRIVER(fd)
		return -1;
	}
    data_receiver_fd = fd;
	printf("Driver data collection started successfully (fd=%d)\n", fd);
    return 1;
}

void csi_packet_receiver_stop_collect()
{
	csi_packet_receiver_set_csi_config(CSI_CONFIG_DISABLE);
	if (get_data_buffptr) {
		free(get_data_buffptr);
		get_data_buffptr = NULL;
	}
	printf("CSI Data Collection Stopped");
	return;
}

int csi_packet_receiver_cleanup(void)
{
	mq_close(mq_handle);
	mq_handle = (mqd_t) ERROR;
	printf("Data Receiver MQ closed (fd: %d, mq_handle: %p)\n", data_receiver_fd, mq_handle);
	int ret = ioctl(data_receiver_fd, CSIIOC_STOP_CSI, NULL);
	if (ret < OK) {
		printf("IOCTL : CSIIOC_STOP_CSI Failed errno: %d (%s)", get_errno(), strerror(get_errno()));
		CLOSE_DRIVER(data_receiver_fd)
		return 0;
	} else {
		printf("Driver data collect stopped\n");
	}
	CLOSE_DRIVER(data_receiver_fd)
	return 1;
}

static int start_csi_collection()
{
    csi_collection_flag = true;
	csi_data_file_fd = open("/mnt/csi_data.bin", O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (csi_data_file_fd < 0) {
		printf("Failed to open CSI data file: /mnt/csi_data.bin, errno: %d (%s)\n", get_errno(), strerror(get_errno()));
	}
    csi_packet_receiver_initialize();
    csi_packet_receiver_start_collect();
    return 0;
}

static int stop_csi_collection(void)
{
    csi_collection_flag = false;
	csi_packet_receiver_cleanup();
	if (csi_data_file_fd >= 0) {
		close(csi_data_file_fd);
		csi_data_file_fd = -1;
		printf("CSI data file closed\n");
	}
    return 0;
}

static int validate_args(int argc)
{
    if (argc != 4) {
        return -1;
    }
    return 0;
}

int csifw_sample_main(int argc, char **args)
{
    return csifw_sample_app_init(argc, args);
}

int csifw_sample_app_init(int argc, char **args)
{
    // Validate and parse command line arguments
    if (validate_args(argc)) {
        printf("CSIFW_SAMPLE_APP usage:\n"
               "  csifw_sample <Interval> \n"
               "\n"
               "Parameters:\n"
               "  Configuration    : CSI configuration type (0-3)\n"
               "                     0 = HT_CSI_DATA\n"
               "                     1 = NON_HT_CSI_DATA\n"
               "                     2 = NON_HT_CSI_DATA_ACC1\n"
               "                     3 = HT_CSI_DATA_ACC1\n"
               "  Interval         : Data collection interval in milliseconds (minimum 30 ms)\n"
               "                     This determines how frequently CSI data is collected\n"
               "  App_run_duration : Application run time in seconds (must be positive)\n"
               "                     How long the application will collect data before stopping\n"
               "\n"
               "Example: csifw_sample 0 64 100\n"
               "         This collects HT_CSI_DATA every 64ms for 100 seconds only\n"
               "\n");
        return -1;
    }

    configuration_type = (csi_config_type_t)atoi(args[1]);
    if (configuration_type <= MIN_CSI_CONFIG_TYPE || configuration_type >= MAX_CSI_CONFIG_TYPE) {
		printf("Invalid CSI config type: %d. (Valid config range: %d-%d)", configuration_type, HT_CSI_DATA, NON_HT_CSI_DATA_ACC1);
    }
    collection_interval_ms = atoi(args[2]);
    if(collection_interval_ms < 30) {
        printf("invalid interval [minimum is 30]...\n");
    }
    int application_run_duration_sec = atoi(args[3]);
    if(application_run_duration_sec <= 0)
    {
        printf("invalid run duration...\n");
    }

    printf("Starting data colloection...\n");
    if(collection_started) {
        printf("csi data colloection in progress...\n");
        return 0;
    }
    collection_started = true;
    start_csi_collection();

    printf("Collecting data for %d seconds...\n", application_run_duration_sec);
    sleep(application_run_duration_sec);

    printf("Stopping data collection...\n");
    stop_csi_collection();
	collection_started = false;
    return 0;
}
