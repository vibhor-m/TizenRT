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

#include <tinyara/init.h>

#include <media/MediaPlayer.h>
#include <media/MediaPlayerObserverInterface.h>
#include <media/FileInputDataSource.h>
#include <media/BufferOutputDataSource.h>

#include "BufferInputDataSource.h"

#include <iostream>
#include <memory>

using namespace std;
using namespace media;
using namespace media::stream;

sem_t tts_sem;
sem_t finished_sem;

media::MediaPlayer mp;

static const char *filePath = "";

class _Observer : public media::MediaPlayerObserverInterface, public std::enable_shared_from_this<_Observer> {
	void onPlaybackStarted(media::MediaPlayer &mediaPlayer) override {
		printf("##################################\n");
		printf("####    onPlaybackStarted     ####\n");
		printf("##################################\n");
	}
	void onPlaybackFinished(media::MediaPlayer &mediaPlayer) override {
		printf("##################################\n");
		printf("####    onPlaybackFinished    ####\n");
		printf("##################################\n");
		sem_post(&tts_sem);
	}
	void onPlaybackError(media::MediaPlayer &mediaPlayer, media::player_error_t error) override {
		printf("##################################\n");
		printf("####      onPlaybackError     ####\n");
		printf("##################################\n");
	}
	void onStartError(media::MediaPlayer &mediaPlayer, media::player_error_t error) override {

	}
	void onStopError(media::MediaPlayer &mediaPlayer, media::player_error_t error) override {

	}
	void onPauseError(media::MediaPlayer &mediaPlayer, media::player_error_t error) override {

	}
	void onPlaybackPaused(media::MediaPlayer &mediaPlayer) override {

	}
};

static void take_sem(sem_t *sem)
{
	int ret;

	do {
		ret = sem_wait(sem);
		DEBUGASSERT(ret == 0 || errno == EINTR);
	} while (ret < 0);
}

int volume = 8;

extern "C"
{
	int play_audio(int argc, char *argv[])
	{
		sem_init(&tts_sem, 0, 0);
		sem_init(&finished_sem, 0, 0);

		filePath = "/mnt/48x16.pcm";

		mp.create();
		//auto source = std::move(unique_ptr<media::stream::FileInputDataSource>(new media::stream::FileInputDataSource(filePath)));
		auto source = std::move(unique_ptr<BufferInputDataSource>(new BufferInputDataSource()));
		source->setSampleRate(24000);
		source->setChannels(1);
		source->setPcmFormat(media::AUDIO_FORMAT_TYPE_S16_LE);
		mp.setObserver(std::make_shared<_Observer>());
		mp.setDataSource(std::move(source));
		mp.prepare();
		mp.setVolume(volume);
		
		mp.start();

		volatile int test = 0;

		while (true) {
			printf("value of test : %d\n", test);
			for (int i = 0; i < 2147483640; i++) {
				test++;
			}
			printf("value of test : %d\n", test);
			sleep(2);
			test = 0;
		}

		take_sem(&tts_sem);

		printf("##################################\n");
		printf("####   Playback done!!        ####\n");
		printf("##################################\n");

		mp.unprepare();
		mp.destroy();
	
		return 0;
	}

	int helloxx_main(int argc, char *argv[])
	{
		struct sched_param sparam;
		sched_getparam(getpid(), &sparam);
		printf("vibhor public helloxx_main %d\n", sparam.sched_priority);
		int pid = task_create("bgaudio", 160, 8192, play_audio, NULL);
		if (pid < 0) {
			printf("failed to create background audio task\n");
			return 0;
		}
		while (true) {
			sleep(10);//printf("very very long long logssss very very long long logssss very very long long logssss very very long long logssss\n");
		}
	}
}

