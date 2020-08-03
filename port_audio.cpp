#include "port_audio.h"

#include "port_audio_node.h"

PortAudio *PortAudio::singleton = NULL;

PortAudio *PortAudio::get_singleton() {
	return singleton;
}

int PortAudio::port_audio_callback_converter(const void *p_input_buffer, void *p_output_buffer,
		unsigned long p_frames_per_buffer, const PaStreamCallbackTimeInfo *p_time_info,
		PaStreamCallbackFlags p_status_flags, void *p_user_data) {

	PaCallbackUserData *user_data = (PaCallbackUserData *)p_user_data;
	if (!user_data) {
		print_line("PortAudio::port_audio_callback_converter: !user_data");
		return 0;
	}
	// copy input buffer to godot type (TODO might need to find a faster way)
	PoolVector<float> pool_vector_in;
	if (p_input_buffer) {
		Error err = pool_vector_in.resize(p_frames_per_buffer);
		if (err != OK) {
			print_line(vformat("PortAudio::port_audio_node_callback: can not resize pool_vector_in to: %d", (unsigned int)p_frames_per_buffer));
			return 0;
		}
		float *in = (float *)p_input_buffer;
		PoolVector<float>::Write write_in = pool_vector_in.write();
		for (unsigned int i = 0; i < p_frames_per_buffer; i++) {
			write_in[i] = in[i];
		}
		write_in.release();
	}
	// provide time info
	Dictionary time_info;
	time_info["input_buffer_adc_time"] = p_time_info->inputBufferAdcTime;
	time_info["current_time"] = p_time_info->currentTime;
	time_info["output_buffer_dac_time"] = p_time_info->outputBufferDacTime;
	// perform callback
	PoolVector<float> pool_vector_out;
	AudioCallback *audio_callback = (AudioCallback *)user_data->audio_callback;
	audio_callback(pool_vector_in, pool_vector_out, p_frames_per_buffer,
			time_info, p_status_flags, user_data->audio_callback_user_data);
	// copy result into buffer (TODO might need to find a faster way)
	float *out = (float *)p_output_buffer;
	unsigned int size_out = pool_vector_out.size();
	PoolVector<float>::Read read_out = pool_vector_out.read();
	for (unsigned int i = 0; i < size_out; i++) {
		out[i] = read_out[i];
	}
	read_out.release();
	return 0;
}

/** Retrieve the release number of the currently running PortAudio build.
 For example, for version "19.5.1" this will return 0x00130501.

 @see paMakeVersionNumber
*/
int PortAudio::get_version() {
	return Pa_GetVersion();
}

/** Retrieve a textual description of the current PortAudio build,
 e.g. "PortAudio V19.5.0-devel, revision 1952M".
 The format of the text may change in the future. Do not try to parse the
 returned string.

 @deprecated As of 19.5.0, use Pa_GetVersionInfo()->versionText instead.
*/
String PortAudio::get_version_text() {
	return String(Pa_GetVersionText());
}

/**
 * Translate the supplied PortAudio error code into a human readable message.
*/
String PortAudio::get_error_text(PortAudio::PortAudioError p_error) {
	switch (p_error) {
		case UNDEFINED:
			return "Undefined error code";
		case NOT_PORT_AUDIO_NODE:
			return "Provided node is not of type PortAudioNode";
	}
	return String(Pa_GetErrorText(p_error));
}

/** Retrieve the number of available host APIs. Even if a host API is
 available it may have no devices available.

 @return A non-negative value indicating the number of available host APIs
 or, a PaErrorCode (which are always negative) if PortAudio is not initialized
 or an error is encountered.

 @see PaHostApiIndex
*/
int PortAudio::get_host_api_count() {
	return Pa_GetHostApiCount();
}

/** Retrieve the index of the default host API. The default host API will be
 the lowest common denominator host API on the current platform and is
 unlikely to provide the best performance.

 @return A non-negative value ranging from 0 to (Pa_GetHostApiCount()-1)
 indicating the default host API index or, a PaErrorCode (which are always
 negative) if PortAudio is not initialized or an error is encountered.
*/
int PortAudio::get_default_host_api() {
	return Pa_GetDefaultHostApi();
}

/** Retrieve a pointer to a structure containing information about a specific
 host Api.

 @param hostApi A valid host API index ranging from 0 to (Pa_GetHostApiCount()-1)

 @return A pointer to an immutable PaHostApiInfo structure describing
 a specific host API. If the hostApi parameter is out of range or an error
 is encountered, the function returns NULL.

 The returned structure is owned by the PortAudio implementation and must not
 be manipulated or freed. The pointer is only guaranteed to be valid between
 calls to Pa_Initialize() and Pa_Terminate().
*/
Dictionary PortAudio::get_host_api_info(int p_host_api) {
	const PaHostApiInfo *pa_api_info = Pa_GetHostApiInfo(p_host_api);

	Dictionary api_info;

	/** this is struct version 1 */
	api_info["struct_version"] = pa_api_info->structVersion;

	/** The well known unique identifier of this host API @see PaHostApiTypeId */
	api_info["type"] = (int)pa_api_info->type; // TODO enum

	/** A textual description of the host API for display on user interfaces. */
	api_info["name"] = String(pa_api_info->name);

	/**  The number of devices belonging to this host API. This field may be
     used in conjunction with Pa_HostApiDeviceIndexToDeviceIndex() to enumerate
     all devices for this host API.
     @see Pa_HostApiDeviceIndexToDeviceIndex
    */
	api_info["device_count"] = pa_api_info->deviceCount;

	/** The default input device for this host API. The value will be a
     device index ranging from 0 to (Pa_GetDeviceCount()-1), or paNoDevice
     if no default input device is available.
    */
	api_info["default_input_device"] = pa_api_info->defaultInputDevice;

	/** The default output device for this host API. The value will be a
     device index ranging from 0 to (Pa_GetDeviceCount()-1), or paNoDevice
     if no default output device is available.
    */
	api_info["default_output_device"] = pa_api_info->defaultOutputDevice;

	return api_info;
}

/** Convert a static host API unique identifier, into a runtime
 host API index.

 @param type A unique host API identifier belonging to the PaHostApiTypeId
 enumeration.

 @return A valid PaHostApiIndex ranging from 0 to (Pa_GetHostApiCount()-1) or,
 a PaErrorCode (which are always negative) if PortAudio is not initialized
 or an error is encountered.
 
 The paHostApiNotFound error code indicates that the host API specified by the
 type parameter is not available.

 @see PaHostApiTypeId
*/
int PortAudio::host_api_type_id_to_host_api_index(int p_host_api_type_id) {
	return Pa_HostApiTypeIdToHostApiIndex((PaHostApiTypeId)p_host_api_type_id);
}

/** Convert a host-API-specific device index to standard PortAudio device index.
 This function may be used in conjunction with the deviceCount field of
 PaHostApiInfo to enumerate all devices for the specified host API.

 @param hostApi A valid host API index ranging from 0 to (Pa_GetHostApiCount()-1)

 @param hostApiDeviceIndex A valid per-host device index in the range
 0 to (Pa_GetHostApiInfo(hostApi)->deviceCount-1)

 @return A non-negative PaDeviceIndex ranging from 0 to (Pa_GetDeviceCount()-1)
 or, a PaErrorCode (which are always negative) if PortAudio is not initialized
 or an error is encountered.

 A paInvalidHostApi error code indicates that the host API index specified by
 the hostApi parameter is out of range.

 A paInvalidDevice error code indicates that the hostApiDeviceIndex parameter
 is out of range.
 
 @see PaHostApiInfo
*/
int PortAudio::host_api_device_index_to_device_index(int p_host_api, int p_host_api_device_index) {
	return Pa_HostApiDeviceIndexToDeviceIndex(p_host_api, p_host_api_device_index);
}

/* Device enumeration and capabilities */

/** Retrieve the number of available devices. The number of available devices
 may be zero.

 @return A non-negative value indicating the number of available devices or,
 a PaErrorCode (which are always negative) if PortAudio is not initialized
 or an error is encountered.
*/
int PortAudio::get_device_count() {
	return Pa_GetDeviceCount();
}

/** Retrieve the index of the default input device. The result can be
 used in the inputDevice parameter to Pa_OpenStream().

 @return The default input device index for the default host API, or paNoDevice
 if no default input device is available or an error was encountered.
*/
int PortAudio::get_default_input_device() {
	return Pa_GetDefaultInputDevice();
}

/** Retrieve the index of the default output device. The result can be
 used in the outputDevice parameter to Pa_OpenStream().

 @return The default output device index for the default host API, or paNoDevice
 if no default output device is available or an error was encountered.

 @note
 On the PC, the user can specify a default device by
 setting an environment variable. For example, to use device #1.
<pre>
 set PA_RECOMMENDED_OUTPUT_DEVICE=1
</pre>
 The user should first determine the available device ids by using
 the supplied application "pa_devs".
*/
int PortAudio::get_default_output_device() {
	return Pa_GetDefaultOutputDevice();
}

Dictionary PortAudio::get_device_info(int p_device_index) {

	const PaDeviceInfo *pa_device_info = Pa_GetDeviceInfo((PaDeviceIndex)p_device_index);

	Dictionary device_info;

	/* this is struct version 2 */
	device_info["struct_version"] = pa_device_info->structVersion;

	device_info["name"] = String(pa_device_info->name);

	/**< note this is a host API index, not a type id*/
	device_info["host_api"] = pa_device_info->hostApi;
	device_info["max_input_channels"] = pa_device_info->maxInputChannels;
	device_info["max_output_channels"] = pa_device_info->maxOutputChannels;

	/** Default latency values for interactive performance. */
	device_info["default_low_input_latency"] = pa_device_info->defaultLowInputLatency;
	device_info["default_low_output_latency"] = pa_device_info->defaultLowOutputLatency;

	/** Default latency values for robust non-interactive applications (eg. playing sound files). */
	device_info["default_high_input_latency"] = pa_device_info->defaultHighInputLatency;
	device_info["default_high_output_latency"] = pa_device_info->defaultHighOutputLatency;

	device_info["default_sample_rate"] = pa_device_info->defaultSampleRate;

	return device_info;
}

/** Library initialization function - call this before using PortAudio.
 This function initializes internal data structures and prepares underlying
 host APIs for use.  With the exception of Pa_GetVersion(), Pa_GetVersionText(),
 and Pa_GetErrorText(), this function MUST be called before using any other
 PortAudio API functions.

 If Pa_Initialize() is called multiple times, each successful 
 call must be matched with a corresponding call to Pa_Terminate(). 
 Pairs of calls to Pa_Initialize()/Pa_Terminate() may overlap, and are not 
 required to be fully nested.

 Note that if Pa_Initialize() returns an error code, Pa_Terminate() should
 NOT be called.

 @return paNoError if successful, otherwise an error code indicating the cause
 of failure.

 @see Pa_Terminate
*/
PortAudio::PortAudioError PortAudio::initialize() {
	PaError err = Pa_Initialize();
	return get_error(err);
}

/** Library termination function - call this when finished using PortAudio.
 This function deallocates all resources allocated by PortAudio since it was
 initialized by a call to Pa_Initialize(). In cases where Pa_Initialise() has
 been called multiple times, each call must be matched with a corresponding call
 to Pa_Terminate(). The final matching call to Pa_Terminate() will automatically
 close any PortAudio streams that are still open.

 Pa_Terminate() MUST be called before exiting a program which uses PortAudio.
 Failure to do so may result in serious resource leaks, such as audio devices
 not being available until the next reboot.

 @return paNoError if successful, otherwise an error code indicating the cause
 of failure.
 
 @see Pa_Initialize
*/
PortAudio::PortAudioError PortAudio::terminate() {
	PaError err = Pa_Terminate();
	return get_error(err);
}

PortAudio::PortAudioError PortAudio::open_default_stream(void **p_stream, int p_input_channel_count,
		int p_output_channel_count, double p_sample_rate, unsigned long p_frames_per_buffer,
		AudioCallback *p_audio_callback, void *p_user_data) {
	// TODO delete(p_audio_callback) - maybe add to list & add finished flag and clean up periodically
	PaCallbackUserData *user_data = new PaCallbackUserData();
	user_data->port_audio = this;
	user_data->audio_callback = p_audio_callback;
	user_data->audio_callback_user_data = p_user_data;
	PaError err = Pa_OpenDefaultStream(p_stream,
			p_input_channel_count,
			p_output_channel_count,
			paFloat32,
			p_sample_rate,
			p_frames_per_buffer,
			&PortAudio::port_audio_callback_converter,
			user_data);
	return get_error(err);
}

/**
Commences audio processing.
*/
PortAudio::PortAudioError PortAudio::start_stream(void *p_stream) {
	PaError err = Pa_StartStream(p_stream);
	return get_error(err);
}

/**
Terminates audio processing. It waits until all pending
 audio buffers have been played before it returns.
*/
PortAudio::PortAudioError PortAudio::stop_stream(void *p_stream) {
	PaError err = Pa_StopStream(p_stream);
	return get_error(err);
}

/** Terminates audio processing immediately without waiting for pending
 buffers to complete.
*/
PortAudio::PortAudioError PortAudio::abort_stream(void *p_stream) {
	PaError err = Pa_AbortStream(p_stream);
	return get_error(err);
}

/** Closes an audio stream. If the audio stream is active it
 discards any pending buffers as if Pa_AbortStream() had been called.
*/
PortAudio::PortAudioError PortAudio::close_stream(void *p_stream) {
	PaError err = Pa_CloseStream(p_stream);
	return get_error(err);
}

/** Determine whether the stream is stopped.
 A stream is considered to be stopped prior to a successful call to
 Pa_StartStream and after a successful call to Pa_StopStream or Pa_AbortStream.
 If a stream callback returns a value other than paContinue the stream is NOT
 considered to be stopped.

 @return Returns one (1) when the stream is stopped, zero (0) when
 the stream is running or, a PaErrorCode (which are always negative) if
 PortAudio is not initialized or an error is encountered.

 @see Pa_StopStream, Pa_AbortStream, Pa_IsStreamActive
*/
PortAudio::PortAudioError PortAudio::is_stream_stopped(void *p_stream) {
	PaError err = Pa_IsStreamStopped(p_stream);
	return get_error(err);
}

/** Determine whether the stream is active.
 A stream is active after a successful call to Pa_StartStream(), until it
 becomes inactive either as a result of a call to Pa_StopStream() or
 Pa_AbortStream(), or as a result of a return value other than paContinue from
 the stream callback. In the latter case, the stream is considered inactive
 after the last buffer has finished playing.

 @return Returns one (1) when the stream is active (ie playing or recording
 audio), zero (0) when not playing or, a PaErrorCode (which are always negative)
 if PortAudio is not initialized or an error is encountered.

 @see Pa_StopStream, Pa_AbortStream, Pa_IsStreamStopped
*/
PortAudio::PortAudioError PortAudio::is_stream_active(void *p_stream) {
	PaError err = Pa_IsStreamActive(p_stream);
	return get_error(err);
}

/** Returns the current time in seconds for a stream according to the same clock used
 to generate callback PaStreamCallbackTimeInfo timestamps. The time values are
 monotonically increasing and have unspecified origin. 
 
 Pa_GetStreamTime returns valid time values for the entire life of the stream,
 from when the stream is opened until it is closed. Starting and stopping the stream
 does not affect the passage of time returned by Pa_GetStreamTime.

 This time may be used for synchronizing other events to the audio stream, for 
 example synchronizing audio to MIDI.
                                        
 @return The stream's current time in seconds, or 0 if an error occurred.

 @see PaTime, PaStreamCallback, PaStreamCallbackTimeInfo
*/
double PortAudio::get_stream_time(void *p_stream) {
	PaTime time = Pa_GetStreamTime(p_stream);
	return time;
}

/** Retrieve a pointer to a PaStreamInfo structure containing information
 about the specified stream.
 @return A pointer to an immutable PaStreamInfo structure. If the stream
 parameter is invalid, or an error is encountered, the function returns NULL.

 @param stream A pointer to an open stream previously created with Pa_OpenStream.

 @note PortAudio manages the memory referenced by the returned pointer,
 the client must not manipulate or free the memory. The pointer is only
 guaranteed to be valid until the specified stream is closed.

 @see PaStreamInfo
*/
Dictionary PortAudio::get_stream_info(void *p_stream) {
	const PaStreamInfo *pa_stream_info = Pa_GetStreamInfo(p_stream);

	Dictionary stream_info;
	/** this is struct version 1 */
	stream_info["struct_version"] = pa_stream_info->structVersion;

	/** The input latency of the stream in seconds. This value provides the most
     accurate estimate of input latency available to the implementation. It may
     differ significantly from the suggestedLatency value passed to Pa_OpenStream().
     The value of this field will be zero (0.) for output-only streams.
     @see PaTime
    */
	stream_info["input_latency"] = pa_stream_info->inputLatency;

	/** The output latency of the stream in seconds. This value provides the most
     accurate estimate of output latency available to the implementation. It may
     differ significantly from the suggestedLatency value passed to Pa_OpenStream().
     The value of this field will be zero (0.) for input-only streams.
     @see PaTime
    */
	stream_info["output_latency"] = pa_stream_info->outputLatency;

	/** The sample rate of the stream in Hertz (samples per second). In cases
     where the hardware sample rate is inaccurate and PortAudio is aware of it,
     the value of this field may be different from the sampleRate parameter
     passed to Pa_OpenStream(). If information about the actual hardware sample
     rate is not available, this field will have the same value as the sampleRate
     parameter passed to Pa_OpenStream().
    */
	stream_info["sample_rate"] = pa_stream_info->sampleRate;

	return stream_info;
}

double PortAudio::get_stream_cpu_load(void *p_stream) {
	return Pa_GetStreamCpuLoad(p_stream);
}

/** Put the caller to sleep for at least 'msec' milliseconds. This function is
 provided only as a convenience for authors of portable code (such as the tests
 and examples in the PortAudio distribution.)

 The function may sleep longer than requested so don't rely on this for accurate
 musical timing.
*/
void PortAudio::sleep(unsigned int p_ms) {
	Pa_Sleep(p_ms);
}

PortAudio::PortAudioError PortAudio::get_error(PaError p_error) {
	switch (p_error) {
		case paNoError:
			return PortAudio::PortAudioError::NO_ERROR;
		case paNotInitialized:
			return PortAudio::PortAudioError::NOT_INITIALIZED;
		case paUnanticipatedHostError:
			return PortAudio::PortAudioError::UNANTICIPATED_HOST_ERROR;
		case paInvalidChannelCount:
			return PortAudio::PortAudioError::INVALID_CHANNEL_COUNT;
		case paInvalidSampleRate:
			return PortAudio::PortAudioError::INVALID_SAMPLE_RATE;
		case paInvalidDevice:
			return PortAudio::PortAudioError::INVALID_DEVICE;
		case paInvalidFlag:
			return PortAudio::PortAudioError::INVALID_FLAG;
		case paSampleFormatNotSupported:
			return PortAudio::PortAudioError::SAMPLE_FORMAT_NOT_SUPPORTED;
		case paBadIODeviceCombination:
			return PortAudio::PortAudioError::BAD_IO_DEVICE_COMBINATION;
		case paInsufficientMemory:
			return PortAudio::PortAudioError::INSUFFICIENT_MEMORY;
		case paBufferTooBig:
			return PortAudio::PortAudioError::BUFFER_TO_BIG;
		case paBufferTooSmall:
			return PortAudio::PortAudioError::BUFFER_TOO_SMALL;
		case paNullCallback:
			return PortAudio::PortAudioError::NULL_CALLBACK;
		case paBadStreamPtr:
			return PortAudio::PortAudioError::BAD_STREAM_PTR;
		case paTimedOut:
			return PortAudio::PortAudioError::TIMED_OUT;
		case paInternalError:
			return PortAudio::PortAudioError::INTERNAL_ERROR;
		case paDeviceUnavailable:
			return PortAudio::PortAudioError::DEVICE_UNAVAILABLE;
		case paIncompatibleHostApiSpecificStreamInfo:
			return PortAudio::PortAudioError::INCOMPATIBLE_HOST_API_SPECIFIC_STREAM_INFO;
		case paStreamIsStopped:
			return PortAudio::PortAudioError::STREAM_IS_STOPPED;
		case paStreamIsNotStopped:
			return PortAudio::PortAudioError::STREAM_IS_NOT_STOPPED;
		case paInputOverflowed:
			return PortAudio::PortAudioError::INPUT_OVERFLOWED;
		case paOutputUnderflowed:
			return PortAudio::PortAudioError::OUTPUT_UNDERFLOWED;
		case paHostApiNotFound:
			return PortAudio::PortAudioError::HOST_API_NOT_FOUND;
		case paInvalidHostApi:
			return PortAudio::PortAudioError::INVALID_HOST_API;
		case paCanNotReadFromACallbackStream:
			return PortAudio::PortAudioError::CAN_NOT_READ_FROM_A_CALLBACK_STREAM;
		case paCanNotWriteToACallbackStream:
			return PortAudio::PortAudioError::CAN_NOT_WRITE_TO_A_CALLBACK_STREAM;
		case paCanNotReadFromAnOutputOnlyStream:
			return PortAudio::PortAudioError::CAN_NOT_READ_FROM_AN_OUT_PUTONLY_STREAM;
		case paCanNotWriteToAnInputOnlyStream:
			return PortAudio::PortAudioError::CAN_NOT_WRITE_TO_AN_INPUT_ONLY_STREAM;
		case paIncompatibleStreamHostApi:
			return PortAudio::PortAudioError::INCOMPATIBLE_STREAM_HOST_API;
		case paBadBufferPtr:
			return PortAudio::PortAudioError::BAD_BUFFER_PTR;
	}
	print_error(vformat("PortAudio::get_error: UNDEFINED error code: %d", p_error));
	return PortAudio::PortAudioError::UNDEFINED;
}

void PortAudio::_bind_methods() {
	ClassDB::bind_method(D_METHOD("version"), &PortAudio::get_version);
	ClassDB::bind_method(D_METHOD("version_text"), &PortAudio::get_version_text);
	ClassDB::bind_method(D_METHOD("initialize"), &PortAudio::initialize);
	ClassDB::bind_method(D_METHOD("terminate"), &PortAudio::terminate);
	ClassDB::bind_method(D_METHOD("sleep", "ms"), &PortAudio::sleep);

	BIND_ENUM_CONSTANT(UNDEFINED);
	BIND_ENUM_CONSTANT(NOT_PORT_AUDIO_NODE);

	BIND_ENUM_CONSTANT(NO_ERROR);
	BIND_ENUM_CONSTANT(NOT_INITIALIZED);
	BIND_ENUM_CONSTANT(UNANTICIPATED_HOST_ERROR);
	BIND_ENUM_CONSTANT(INVALID_CHANNEL_COUNT);
	BIND_ENUM_CONSTANT(INVALID_SAMPLE_RATE);
	BIND_ENUM_CONSTANT(INVALID_DEVICE);
	BIND_ENUM_CONSTANT(INVALID_FLAG);
	BIND_ENUM_CONSTANT(SAMPLE_FORMAT_NOT_SUPPORTED);
	BIND_ENUM_CONSTANT(BAD_IO_DEVICE_COMBINATION);
	BIND_ENUM_CONSTANT(INSUFFICIENT_MEMORY);
	BIND_ENUM_CONSTANT(BUFFER_TO_BIG);
	BIND_ENUM_CONSTANT(BUFFER_TOO_SMALL);
	BIND_ENUM_CONSTANT(NULL_CALLBACK);
	BIND_ENUM_CONSTANT(BAD_STREAM_PTR);
	BIND_ENUM_CONSTANT(TIMED_OUT);
	BIND_ENUM_CONSTANT(INTERNAL_ERROR);
	BIND_ENUM_CONSTANT(DEVICE_UNAVAILABLE);
	BIND_ENUM_CONSTANT(INCOMPATIBLE_HOST_API_SPECIFIC_STREAM_INFO);
	BIND_ENUM_CONSTANT(STREAM_IS_STOPPED);
	BIND_ENUM_CONSTANT(STREAM_IS_NOT_STOPPED);
	BIND_ENUM_CONSTANT(INPUT_OVERFLOWED);
	BIND_ENUM_CONSTANT(OUTPUT_UNDERFLOWED);
	BIND_ENUM_CONSTANT(HOST_API_NOT_FOUND);
	BIND_ENUM_CONSTANT(INVALID_HOST_API);
	BIND_ENUM_CONSTANT(CAN_NOT_READ_FROM_A_CALLBACK_STREAM);
	BIND_ENUM_CONSTANT(CAN_NOT_WRITE_TO_A_CALLBACK_STREAM);
	BIND_ENUM_CONSTANT(CAN_NOT_READ_FROM_AN_OUT_PUTONLY_STREAM);
	BIND_ENUM_CONSTANT(CAN_NOT_WRITE_TO_AN_INPUT_ONLY_STREAM);
	BIND_ENUM_CONSTANT(INCOMPATIBLE_STREAM_HOST_API);
	BIND_ENUM_CONSTANT(BAD_BUFFER_PTR);
}

PortAudio::PortAudio() {
	singleton = this;
	PortAudio::PortAudioError err = initialize();
	if (err != PortAudio::PortAudioError::NO_ERROR) {
		print_error(vformat("PortAudio::PortAudio: failed to initialize (%d)", err));
	}
}

PortAudio::~PortAudio() {
	PortAudio::PortAudioError err = terminate();
	if (err != PortAudio::PortAudioError::NO_ERROR) {
		print_error(vformat("PortAudio::PortAudio: failed to terminate (%d)", err));
	}
}

PortAudio::PaCallbackUserData::PaCallbackUserData() {
	port_audio = NULL;
	audio_callback = NULL;
	audio_callback_user_data = NULL;
}
