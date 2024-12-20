#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#include <android/log.h>
#include <jni.h>
#include <inttypes.h>

#include "jni-protocol.h"
#include "audiodecoder.h"

#define READ_BUFFER_SIZE 4096
#define INITIAL_WRITE_BUFFER_SIZE 4096
#define LOG_TAG "audiodecoder.c"

#define LOGI(...) {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__);}
#define LOGE(...) {__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__);}
#define LOGW(...) {__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__);}


/*
void show_output_formats() {
	AVOutputFormat* fmt = NULL;
	while ((fmt = av_oformat_next(fmt))) {
		LOGI("format %s: ", fmt->name);
	}
}

void show_encoders() {
	AVCodec *p = NULL;
	while ((p = av_codec_next(p))) {
		LOGI("codec: %s", p->name);
	}
}

void s16_interleaved_gen_tone(
		double* pos,
		char* samples,
		int length,
		int sample_rate,
		int nb_channels,
		int hz,
		float loudness) {

	double tincr = 2 * M_PI * hz / (double)sample_rate;
	int j, k;
	int tmp;
	for (j = 0; j < length - nb_channels*2 + 1; j += (nb_channels*2)) {
		tmp = (int)(sin(*pos)*32767*loudness);
		samples[j] = (0x00ff & tmp);
		samples[j+1] = 0x00ff & ((0xff00 & tmp) >> 8);
		for (k = 1; k < nb_channels; k++) {
			samples[j+(2*k)] = samples[j];
			samples[j+(2*k)+1] = samples[j+1];
		}
		*pos += tincr;
		while(*pos > 2 * M_PI * hz) *pos -= 2 * M_PI * hz;
	}
}
*/

/**
 * Convenience function that creates and initializes an SwrContext
 * @return initialized SwrContext
 */
SwrContext* get_swr(SwrContext** swr, AVCodecContext* in) {
	if (!*swr) {
		*swr = swr_alloc();
		av_opt_set_int(*swr, "in_channel_layout",  in->channel_layout > 0 ? in->channel_layout : AV_CH_LAYOUT_STEREO, 0);
		av_opt_set_int(*swr, "in_sample_rate",     in->sample_rate > 0 ? in->sample_rate : 44100, 0);
		av_opt_set_sample_fmt(*swr, "in_sample_fmt",  in->sample_fmt != -1 ? in->sample_fmt : AV_SAMPLE_FMT_S16, 0);
		av_opt_set_int(*swr, "out_channel_layout", AV_CH_LAYOUT_STEREO,  0);
		av_opt_set_int(*swr, "out_sample_rate",    44100, 0);
		av_opt_set_sample_fmt(*swr, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
		swr_init(*swr);
	}
	return *swr;
}


/**
 * Read data from Java Layer by calling com/appunite/ffmpeg/decoding/DataFeed:read and write into @buffer
 * This function is passed to the AVIOContext of the AVFormatContext in (jni_audio_converter_start_decoding)
 * @return 0 if successful -1, if user aborted decoding
 */
int audio_decoder_read(void* opaque, uint8_t* buffer, int length) {
	JNIContext* jnictx = (JNIContext*)opaque;

	jint bytes =  (*(jnictx->env))->CallIntMethod(jnictx->env, jnictx->feed, jnictx->read, jnictx->readBuffer);

	if (bytes > -1) {
		(*(jnictx->env))->GetByteArrayRegion(jnictx->env, jnictx->readBuffer, 0, bytes, buffer);
	}

	return bytes;
}

/**
 * Write data into a Java ByteArray and call com/appunite/ffmpeg/decoding/DataFeed:write
 * @return 0 if successful -1, if user aborted decoding
 */
int audio_decoder_write(JNIContext* jnictx, uint8_t* buffer, int length) {
	(*(jnictx->env))->SetByteArrayRegion(jnictx->env, jnictx->jb_array[jnictx->run], 0, length, buffer);
	return (*(jnictx->env))->CallIntMethod(jnictx->env, jnictx->feed, jnictx->write, jnictx->jb_array[jnictx->run], length);
}

/**
 * Function called from both (decode_audio_packet_flush) and (decode_audio_packet_stream)
 * @return 0 if successful AV_ERROR on error
 */
int resample_and_write_frame(AudioDecoderCtx* dec) {
	int err;

	dec->dst_nb_samples = av_rescale_rnd(
			swr_get_delay(get_swr(dec->swr, dec->codecctx), dec->frame->sample_rate) + dec->frame->nb_samples,
			dec->dst_sample_rate,
			dec->frame->sample_rate,
			AV_ROUND_UP);


	if (dec->dst_nb_samples > dec->max_dst_nb_samples) {
		av_free(dec->dst_samples_array[0][0]); // free buffer
		av_free(dec->dst_samples_array[1][0]); // free buffer

		(*dec->jnictx->env)->DeleteLocalRef(dec->jnictx->env, dec->jnictx->jb_array[0]); // free java byte array
		(*dec->jnictx->env)->DeleteLocalRef(dec->jnictx->env, dec->jnictx->jb_array[1]); // free java byte array

		// realloc buffer
		err = av_samples_alloc(dec->dst_samples_array[0], &dec->dst_line_size, dec->dst_nb_channels,
										   dec->dst_nb_samples, AV_SAMPLE_FMT_S16, 1);
		if (err < 0) return err;
<<<<<<< HEAD
		err = av_samples_alloc(dec->dst_samples_array[1], &dec->dst_line_size, dec->dst_nb_channels,
												   dec->dst_nb_samples, AV_SAMPLE_FMT_S16, 1);
		if (err < 0) return err;
		// realloc java byte array
		dec->jnictx->jb_array[0] = (*dec->jnictx->env)->NewByteArray(
				dec->jnictx->env,
				dec->dst_line_size);
		dec->jnictx->jb_array[1] = (*dec->jnictx->env)->NewByteArray(
						dec->jnictx->env,
						dec->dst_line_size);

		// set new buffer size/nb_samples
=======

		(*dec->jnictx->env)->DeleteLocalRef(dec->jnictx->env, dec->jnictx->writeBuffer);
		dec->jnictx->writeBuffer = (*dec->jnictx->env)->NewByteArray(dec->jnictx->env, dec->dst_line_size);
		if (!dec->jnictx->writeBuffer) {
			return -1;
		}

>>>>>>> 9c1e277 (added reallocation for java byte array)
		dec->max_dst_nb_samples = dec->dst_nb_samples;
	}
	err = swr_convert(
			get_swr(dec->swr, dec->codecctx),
			dec->dst_samples_array[dec->jnictx->run],
			dec->dst_nb_samples,
			(const uint8_t **)&(dec->frame->data),
			dec->frame->nb_samples);
	if (err < 0) {
		LOGE("error converting samples to s16: %s", av_err2str(err));
		dec->dst_samples_array[dec->jnictx->run] = dec->frame->data;
		dec->dst_line_size = av_samples_get_buffer_size(
				NULL,
				dec->codecctx->channels,
				dec->frame->nb_samples,
				dec->frame->format,
				1);
	}
	err = (*(dec->write_func))(dec->jnictx, *dec->dst_samples_array[dec->jnictx->run], dec->dst_nb_samples * (2 * dec->dst_nb_channels));
	dec->jnictx->run = !dec->jnictx->run;
	return err;
}

/**
 * Function called for flushing the buffer after the main read/write loop in
 * (jni_audio_converter_start_decoding) exits
 * @return 0 if successful AV_ERROR on error
 */
int decode_audio_packet_flush(AudioDecoderCtx* dec) {
	av_init_packet(dec->pkt);
	// Decode all the remaining frames in the buffer, until the end is reached
	int got_frame = 0;
	int err;
	while (avcodec_decode_audio4(dec->codecctx, dec->frame, &got_frame, dec->pkt) >= 0
			&& got_frame) {
		err = resample_and_write_frame(dec);
		if (err != 0) {
			return err;
		}
	}
	return 0;
}

/**
 * Function called from main read/write loop in (jni_audio_converter_start_decoding)
 * @return 0 if successful AV_ERROR on error
 */
int decode_audio_packet_stream(AudioDecoderCtx* dec) {
	int err, got_frame = 0;
	while (dec->pkt->size > 0) {
		err = avcodec_decode_audio4(dec->codecctx, dec->frame, &got_frame, dec->pkt);

		if (err >= 0 && got_frame) {
			dec->pkt->size -= err;
			dec->pkt->data += err;

			err = resample_and_write_frame(dec);
			if (err != 0) {
				return err;
			}

		} else {
			dec->pkt->size = 0;
			dec->pkt->data = NULL;
		}
	}
	return 0;
}

/**
 * Main JNI Method - com/appunite/ffmpeg/decoding/DataFeed to read audio data by calling 'read',
 * decode data using ffmpeg and susequently write back to com/appunite/ffmpeg/decoding/DataFeed
 * by calling 'write'
 *
 * The audio decoding tries to be always one step ahead of the costly write back operation
 * (AudioTrack blocks) by using 2 Threads, 1 for writing, one for decoding.
 * This is achieved by using a threaded implementation of 'write' in the instances of
 * com.appunite.ffmpeg.decoding.Decoder.
 * A newly arriving call to 'write' will block, if another one is still in progress.
 *
 * @return void
 */
void jni_audio_converter_start_decoding(JNIEnv *env, jobject thiz, jobject encoderDataFeed) {

	int err;
	int stream_idx;
	AVFormatContext* fctx;
	AVIOContext* ioctx;
	AVCodec* codec;
	AVCodecContext* codecctx;
	SwrContext* swr = NULL;
	AVPacket packet;
	AVFrame* frame = NULL;

	if (!encoderDataFeed) {
		LOGE("DataFeed was null");
		goto end;
	}

	// attach this thread to the jvm, so it can be blocked later
	JavaVM* jvm;
	(*env)->GetJavaVM(env, &jvm);
	(*jvm)->AttachCurrentThread(jvm, &env, NULL);

	jbyteArray jb_array[2];
	jb_array[0] = (*env)->NewByteArray(env, INITIAL_WRITE_BUFFER_SIZE);
	jb_array[1] = (*env)->NewByteArray(env, INITIAL_WRITE_BUFFER_SIZE);
	JNIContext jnictx = {
		env,
<<<<<<< HEAD
		(*env)->NewByteArray(env, READ_BUFFER_SIZE),   		// data is read from JAVA into a buffer of BUFFER_SIZE
		jb_array, 										// data is written back on a per frame basis
			  				// at this point the needed size in bytes is unknown,
												  	  	// so choose a relatively large buffer initially
												  	 	// and realloc later on, if the size is insufficient
=======
		(*env)->NewByteArray(env, READ_BUFFER_SIZE),
		(*env)->NewByteArray(env, INITIAL_WRITE_BUFFER_SIZE),
>>>>>>> 9c1e277 (added reallocation for java byte array)
		encoderDataFeed,
		global_ctx.feed_class,
		global_ctx.read,
		global_ctx.write,
		global_ctx.start,
		global_ctx.stop,
		0
	};

	// invoke Java 'start' callback
	(*env)->CallVoidMethod(env, encoderDataFeed, jnictx.start);


	av_register_all();
	avcodec_register_all();

	char *ioctx_buffer = av_malloc(READ_BUFFER_SIZE);
	if (!ioctx_buffer) {
		LOGE("Could not allocate read buffer: %s", av_err2str(err));
		goto end;
	}


	fctx = avformat_alloc_context();
	ioctx = avio_alloc_context(
			ioctx_buffer,
			READ_BUFFER_SIZE,
			0,
			&jnictx,
			audio_decoder_read,
			NULL,
			NULL);
	ioctx->seekable = 0;
	ioctx->write_flag = 0;
	fctx->pb = ioctx;

	err = avformat_open_input(&fctx, "dummy", NULL, NULL);
	if (err < 0) {
		LOGE("Could not open input format: %s", av_err2str(err));
		goto end;
	}
	LOGI("detected format: %s", fctx->iformat->name);

	err = avformat_find_stream_info(fctx, NULL);
	if (err < 0) {
		LOGE("Failed to detect streams: %s", av_err2str(err));
		goto end;
	}

	err = av_find_best_stream(fctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
	if (err < 0) {
		LOGE("Failed to find audio stream: %s", av_err2str(err));
		goto end;
	}
	LOGI("detected codec: %s", codec->name);
	stream_idx = err;


	codecctx = fctx->streams[stream_idx]->codec;

	err = avcodec_open2(codecctx, codec, NULL);
	if (err != 0) {
		LOGE("Failed to open codec context: %s", av_err2str(err));
		goto end;
	}

	av_init_packet(&packet);
	packet.data = NULL;
	packet.size = 0;

	frame = av_frame_alloc();
	if (!frame) {
		LOGE("Failed to allocate frame: %s", av_err2str(err));
		goto end;
	}

	uint8_t** dst_samples_array[2];
	AudioDecoderCtx audio_dec = {
			&jnictx,
			codecctx,
			frame,
			NULL, // packet is passed from the main loop below
			&swr,
			&audio_decoder_write,
<<<<<<< HEAD
			INITIAL_WRITE_BUFFER_SIZE / (av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO)*2),
			av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO),
			44100,
			0, // will be computed later
			//av_rescale_rnd(1024, 44100, 44100, AV_ROUND_UP),
			INITIAL_WRITE_BUFFER_SIZE / (av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO)*2),
			dst_samples_array,
=======
			INITIAL_WRITE_BUFFER_SIZE / (2 * av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO)),
			av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO),
			44100,
			0,
			INITIAL_WRITE_BUFFER_SIZE / (2 * av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO)),
			//av_rescale_rnd(1024, 44100, 44100, AV_ROUND_UP),
			dst_samples
>>>>>>> 9c1e277 (added reallocation for java byte array)
	};


	err = av_samples_alloc_array_and_samples(&audio_dec.dst_samples_array[0],
			&audio_dec.dst_line_size, audio_dec.dst_nb_channels,
			audio_dec.dst_nb_samples, AV_SAMPLE_FMT_S16, 0);
	if (err < 0) {
		LOGE("Could not allocate destination samples: %s", av_err2str(err));
		goto end;
	}

	err = av_samples_alloc_array_and_samples(&audio_dec.dst_samples_array[1],
				&audio_dec.dst_line_size, audio_dec.dst_nb_channels,
				audio_dec.dst_nb_samples, AV_SAMPLE_FMT_S16, 0);
	if (err < 0) {
		LOGE("Could not allocate destination samples: %s", av_err2str(err));
		goto end;
	}

	while(av_read_frame(fctx, &packet) >= 0) {
		AVPacket decoding_packet = packet;

		if (decoding_packet.stream_index == stream_idx) {
			audio_dec.pkt = &decoding_packet;
			err = decode_audio_packet_stream(&audio_dec);
			if (err != 0) {
				LOGW("Could not decode an audio frame: %s", av_err2str(err));
				goto end;
			}
		}
		av_free_packet(&packet);
	}

	if (codecctx->codec->capabilities & CODEC_CAP_DELAY)
	{
		audio_dec.pkt = &packet;
		err = decode_audio_packet_flush(&audio_dec);
		if (err != 0) {
			LOGE("Could not flush frame buffer: %s", av_err2str(err));
			goto end;
		}
	}

	goto end;

end:

	if (&packet) {
		av_free_packet(&packet);
	}

	if (dst_samples_array) {
		av_freep(dst_samples_array[0]);
		av_freep(dst_samples_array[1]);
	}
	av_freep(dst_samples_array);

	if (frame) {
		av_frame_free(&frame);
	}

	if (swr) {
		swr_free(&swr);
	}

	if (fctx) {
		avformat_free_context(fctx);
	}

	(*env)->DeleteLocalRef(env, jnictx.readBuffer);
	(*env)->DeleteLocalRef(env, jnictx.jb_array[0]);
	(*env)->DeleteLocalRef(env, jnictx.jb_array[1]);

	// invoke Java 'stop' callback
	(*env)->CallVoidMethod(env, encoderDataFeed, jnictx.stop);
}
