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
/****************************************************************************
 * examples/hello/hello_main.c
 *
 *   Copyright (C) 2008, 2011-2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>
#include <stdio.h>

#include <errno.h>
#include <sys/socket.h>
#include <wifi_manager/wifi_manager.h>
#include <stress_tool/st_perf.h>

#define TAG "[HELLO_SERVER]"

#define CT_LOG(tag, fmt, args...) \
printf(tag "[T%d] " fmt "\t%s:%d\n", getpid(), ##args, __FUNCTION__, __LINE__)

#define CT_LOGE(tag, fmt, args...) \
printf(tag "[ERR][T%d] " fmt "\t%s:%d\n", getpid(), ##args, __FUNCTION__, __LINE__)

static sem_t g_wm_sem;

#define HELLO_TEST_SIGNAL              \
	do {                            \
		sem_post(&g_wm_sem);        \
		CT_LOG(TAG, "send signal"); \
	} while (0)

#define HELLO_TEST_WAIT                \
	do {                            \
		CT_LOG(TAG, "wait signal"); \
		sem_wait(&g_wm_sem);        \
	} while (0)

/*
 * callbacks
 */
static void wm_cb_sta_connected(wifi_manager_cb_msg_s msg, void *arg);
static void wm_cb_sta_disconnected(wifi_manager_cb_msg_s msg, void *arg);
static void wm_cb_softap_sta_join(wifi_manager_cb_msg_s msg, void *arg);
static void wm_cb_softap_sta_leave(wifi_manager_cb_msg_s msg, void *arg);
static void wm_cb_scan_done(wifi_manager_cb_msg_s msg, void *arg);

static wifi_manager_cb_s g_wifi_callbacks = {
	wm_cb_sta_connected,
	wm_cb_sta_disconnected,
	wm_cb_softap_sta_join,
	wm_cb_softap_sta_leave,
	wm_cb_scan_done,
};

void wm_cb_sta_connected(wifi_manager_cb_msg_s msg, void *arg)
{
	CT_LOG(TAG, "--> res(%d)", msg.res);
	CT_LOG(TAG, "bssid %02x:%02x:%02x:%02x:%02x:%02x",
		   msg.bssid[0], msg.bssid[1],
		   msg.bssid[2], msg.bssid[3],
		   msg.bssid[4], msg.bssid[5]);
	int conn = 0;
	if (WIFI_MANAGER_SUCCESS == msg.res) {
		conn = 0;
	} else {
		conn = 2;
	}
	HELLO_TEST_SIGNAL;
}

void wm_cb_sta_disconnected(wifi_manager_cb_msg_s msg, void *arg)
{
	CT_LOG(TAG, "--> res(%d) reason %d", msg.res, msg.reason);
	HELLO_TEST_SIGNAL;
}

void wm_cb_softap_sta_join(wifi_manager_cb_msg_s msg, void *arg)
{
	CT_LOG(TAG, "--> res(%d)", msg.res);
	CT_LOG(TAG, "bssid %02x:%02x:%02x:%02x:%02x:%02x",
		   msg.bssid[0], msg.bssid[1],
		   msg.bssid[2], msg.bssid[3],
		   msg.bssid[4], msg.bssid[5]);
	HELLO_TEST_SIGNAL;
}

void wm_cb_softap_sta_leave(wifi_manager_cb_msg_s msg, void *arg)
{
	CT_LOG(TAG, "--> res(%d) reason %d", msg.res, msg.reason);
	HELLO_TEST_SIGNAL;
}

void wm_cb_scan_done(wifi_manager_cb_msg_s msg, void *arg)
{
	CT_LOG(TAG, "--> res(%d)", msg.res);
	if (msg.res != WIFI_MANAGER_SUCCESS || msg.scanlist == NULL) {
		HELLO_TEST_SIGNAL;
		return;
	}
	//ct_print_scanlist(msg.scanlist);
	HELLO_TEST_SIGNAL;
}

static int hello_run_procedure(void)
{
	int res = sem_init(&g_wm_sem, 0, 0);
	if (res != 0) {
		return -1;
	}
	
	wifi_manager_result_e wres = WIFI_MANAGER_SUCCESS;

	/* Initialise Wifi */
	CT_LOG(TAG, "init wi-fi");
	wres = wifi_manager_init(&g_wifi_callbacks);
	if (wres != WIFI_MANAGER_SUCCESS) {
		CT_LOGE(TAG, "fail to init %d", wres);
		return -1;
	}
	
	/*  Start softAP */
	CT_LOG(TAG, "start softAP");
	wifi_manager_softap_config_s softap_config;
	wm_get_softapinfo(&softap_config, "tp1x", "12341234", 1);
	wres = wifi_manager_set_mode(SOFTAP_MODE, &softap_config);
	if (wres != WIFI_MANAGER_SUCCESS) {
		CT_LOGE(TAG, "fail to start softap %d", wres);
		return -1;
	}

	/*  wait join event */
	CT_LOG(TAG, "wait join event");
	HELLO_TEST_WAIT;

	/*  scan in softAP mode */
	CT_LOG(TAG, "scan in softAP mode");
	wres = wifi_manager_scan_ap(NULL);
	if (wres != WIFI_MANAGER_SUCCESS) {
		CT_LOGE(TAG, "fail to scan %d", wres);
		return -1;
	}
	/*  wait scan event */
	CT_LOG(TAG, "wait scan done event");
	HELLO_TEST_WAIT;
	
	/*  set STA */
	CT_LOG(TAG, "start STA mode");
	wres = wifi_manager_set_mode(STA_MODE, NULL);
	if (wres != WIFI_MANAGER_SUCCESS) {
		CT_LOGE(TAG, "start STA fail %d", wres);
		return -1;
	}
	
	/*  scan in STA mode */
	CT_LOG(TAG, "scan in STA mode");
	wres = wifi_manager_scan_ap(NULL);
	if (wres != WIFI_MANAGER_SUCCESS) {
		CT_LOGE(TAG, "fail to scan %d", wres);
		return -1;
	}
	/*  wait scan event */
	CT_LOG(TAG, "wait scan done event in STA mode");
	HELLO_TEST_WAIT; 
	
	/* Deinitialise Wifi */
	CT_LOG(TAG, "deinit wi-fi");
	wres = wifi_manager_deinit();
	if (wres != WIFI_MANAGER_SUCCESS) {
		CT_LOGE(TAG, "fail to deinit %d", wres);
		return -1;
	}

	sem_destroy(&g_wm_sem);
	return 0;
}


#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int hello_main(int argc, char *argv[])
#endif
{
	hello_run_procedure();
	printf("Hello, World!!\n");
	return 0;
}