// ============================================================================
//
// Audio.cpp
// Copyright Sony Online Entertainment
//
// ============================================================================

#include "clientAudio/FirstClientAudio.h"
#include "clientAudio/Audio.h"

#include "clientAudio/AudioSampleInformation.h"
#include "clientAudio/ConfigClientAudio.h"
#include "clientAudio/Sample2d.h"
#include "clientAudio/Sample3d.h"
#include "clientAudio/SampleStream.h"
#include "clientAudio/SampleCacheEntry.h"
#include "clientAudio/SoundTemplateList.h"
#include "clientAudio/Sound3d.h"
#include "clientAudio/Sound3dTemplate.h"
#include "clientAudio/SoundObject3d.h"
#include "sharedDebug/DataLint.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/PerformanceTimer.h"
#include "sharedDebug/Profiler.h"
#include "sharedFile/FileNameUtils.h"
#include "sharedFile/Iff.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/LessPointerComparator.h"
#include "sharedFoundation/Os.h"
#include "sharedFoundation/PerThreadData.h"
#include "sharedFoundation/TemporaryCrcString.h"
#include "sharedMath/VectorArgb.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/CellProperty.h"
#include "sharedUtility/DataTable.h"
#include "sharedUtility/LocalMachineOptionManager.h"

#include <limits>
#include <list>
#include <map>

#include <cstdarg>
#include <cstdio>

// ============================================================================
// CONSULT-63 (2026-07-05): Win32 builds against the retail-era Miles 7.2e SDK;
// x64 stays on the 9.3b header (9.3v DLL at runtime). The 9.3b 32-bit DLL's
// mixer storms when one-shot samples start inside an MP3 stream track-transition
// window (A/B-convicted: same x64 exe, 9.3b DLL storms, 9.3v DLL clean) and no
// 32-bit build of the fixed 9.3v exists. Two API deltas are bridged here so the
// call sites stay identical across both builds.
// ============================================================================

#if MSS_MAJOR_VERSION < 9

// Miles 9.3 added a bus_index arg to the room-type (EAX environment) calls; 7.2e has none.
namespace MilesCompat
{
	inline void setRoomType(HDIGDRIVER dig, S32 /*busIndex*/, S32 roomType) { AIL_set_room_type(dig, roomType); }
	inline S32 roomType(HDIGDRIVER dig, S32 /*busIndex*/) { return AIL_room_type(dig); }
}
#define AIL_set_room_type MilesCompat::setRoomType
#define AIL_room_type     MilesCompat::roomType

#endif // MSS_MAJOR_VERSION < 9

// ============================================================================
// 35-05 Miles audio integration state: login-title-music stream fix + 3D-mute setting.
// ============================================================================

namespace
{
	// Login-title-music stream fix. Miles 9.3x fills a stream's buffers from its own background IO
	// thread, which re-opens the stream file through our AIL_set_file_callbacks open hook -- but passes
	// an EMPTY filename (the async/IO-thread context doesn't carry it), so TreeFile::open of " fails and
	// the stream goes silent. Only the STREAM path hits these callbacks (cached samples are in-memory).
	// We remember the most-recent real stream filename and serve the empty-name re-open from it; track
	// the sequential read offset so each re-open continues instead of restarting at 0; and close the
	// prior re-opened handle so the per-chunk re-opens don't leak.
	//
	// CONSULT-61 ROOT CAUSE: the empty-name re-opens were never a Miles quirk -- Miles' async IO
	// treats FileHandle==0 as "not opened yet, open by (empty) name" (milesasync.cpp:440-443), and
	// our handle counter STARTED AT 0, so the first stream opened after install (the title music)
	// was poisoned into per-chunk empty-name opens. With handles starting at 1 the empty-name path
	// never fires. The shim's ONE-SLOT global state is actively harmful with multiple concurrent
	// in-world streams (cross-stream name/offset swaps -> decoder pops; closing substituted handles
	// mid-read -> permanently starved streams: the dead mus_theme_tatooine in audio-diag.log), so it
	// now defaults OFF. [ClientAudio] titleMusicStreamFix=true re-arms it if title music ever
	// regresses (it should not -- its chunk reads now carry a valid handle).
	bool s_titleMusicStreamFix = false;

	char                     s_lastAudioOpenName[1024] = { 0 };
	int                      s_streamResumeOffset = 0;
	std::map<unsigned, bool> s_substitutedHandleMap;

	// CONSULT-61: the file callbacks run on the MAIN thread (stream opens, one-shot sample
	// loads) AND Miles' async IO thread (chunk reads) concurrently; s_fileMap /
	// s_nextFileHandle / the shim state above were completely unsynchronized (the same
	// unguarded-cross-thread-container class as the CONSULT-56 TreeFile/ShaderCache fixes).
	// The lock guards the SHARED STATE ONLY -- never hold it across TreeFile I/O: a
	// main-thread one-shot sample load holding it through a full file read blocks Miles'
	// IO thread mid-music-stream = audible crackle (convicted 2026-07-04 round 3). Safe
	// because each file handle has a single consumer sequencing its open/seek/read/close
	// (Miles synchronizes its IO thread against AIL_close_stream), so only the map and the
	// shim globals are cross-thread.
	struct FileCallbackCriticalSection
	{
		CRITICAL_SECTION m_criticalSection;
		FileCallbackCriticalSection()  { InitializeCriticalSection(&m_criticalSection); }
		~FileCallbackCriticalSection() { DeleteCriticalSection(&m_criticalSection); }
	};
	FileCallbackCriticalSection s_fileCallbackCriticalSection;

	struct FileCallbackLock
	{
		FileCallbackLock()  { EnterCriticalSection(&s_fileCallbackCriticalSection.m_criticalSection); }
		~FileCallbackLock() { LeaveCriticalSection(&s_fileCallbackCriticalSection.m_criticalSection); }
	};

	// Turn OFF Miles' DIG_3D_MUTE_AT_MAX (default YES in 9.3x) so distant 3D sounds attenuate smoothly
	// instead of hard-muting at their max distance. Applied before AIL_open_digital_driver.
	bool s_disable3dMuteAtMax = true;
}

// ============================================================================

namespace AudioNamespace
{
	class ProviderData
	{
	public:

		ProviderData();

		bool        m_supported;
		std::string m_name;
	};

	typedef std::map<HPROVIDER, ProviderData> ProviderMap;

	ProviderMap get3dProviders();

	struct SoundBucketData
	{
		SoundBucketData(Sound2 * const sound, float const distanceSquared, bool const soundIsAlreadyPlaying)
		 : m_sound(sound)
		 , m_distanceSquared(distanceSquared)
		 , m_soundIsAlreadyPlaying(soundIsAlreadyPlaying)
		{
		}

		Sound2 * m_sound;
		float m_distanceSquared;
		bool m_soundIsAlreadyPlaying;

	private:

		//Disabled
		SoundBucketData();
	};

	typedef std::map<CrcString const *, SampleCacheEntry, LessPointerComparator> SampleCache;
	typedef std::map<CrcString const *, int, LessPointerComparator>              MusicOffsetMap;
	typedef std::map<unsigned int, AbstractFile *>                               FileMap;
	typedef std::map<SampleId, Sample2d>                                         SampleIdToSample2dMap;
	typedef std::map<SampleId, Sample3d>                                         SampleIdToSample3dMap;
	typedef std::map<SampleId, SampleStream>                                     SampleIdToSampleStreamMap;
	typedef std::map<SoundId, Sound2 *>                                          SoundIdToSoundMap;
	typedef std::list<Sound2 *> PrioritizedPlayingSounds;
	typedef std::vector<Sound2 *> UtilitySoundList;
	typedef std::vector<SoundId>                                                 SoundIdList;
	typedef std::list<Sound2 *>                                                  QueuedSamplesToStartList;
	typedef std::map<CrcString const *, SoundBucketData, LessPointerComparator> SoundBucketList;

	PrioritizedPlayingSounds     s_prioritizedPlayingSounds;
	UtilitySoundList             s_utilitySoundList;
	QueuedSamplesToStartList     s_queuedSamplesToStartList;
	ProviderMap                  s_3dProviderMap;
	bool                         s_debugPrintAllocations = false;
	// 24-02 (D-12): true until Audio::install's startup probe finds the Miles codec/provider redist absent.
	// When false, it gates the per-sample DEBUG_WARNING flood site below (REVIEW/Codex HIGH — the one-shot
	// startup warning alone does NOT stop the 141k-line "Error loading and allocating the sample" flood).
	bool                         s_milesCodecRedistAvailable = true;
	HDIGDRIVER                   s_digitalDevice2d = NULL;              // Digital playback device for 2d sounds
	MSS_MC_SPEC                  s_speakers;
	DataTable *                  s_musicDataTable;
	MusicOffsetMap               s_musicOffsetMap;
	SampleCache                  s_sampleCache;                         // The current cached digital sound samples
	SoundIdToSoundMap            s_soundIdToSoundMap;                   // Map of 2d and 3d sounds currently playing
	SampleIdToSample2dMap        s_sampleIdToSample2dMap;
	SampleIdToSample3dMap        s_sampleIdToSample3dMap;
	SampleIdToSampleStreamMap    s_sampleIdToSampleStreamMap;
	FileMap                      s_fileMap;
	SoundObject3d                s_listener;
	SoundIdList                  s_localPurgeList;
	bool                         s_installed = false;
	int                          s_currentCacheSize = 0;
	int                          s_cacheHitCount = 0;
	int                          s_cacheMissCount = 0;
	int                          s_instantRejectionCount = 0;
	int                          s_nextSoundId = 1;
	int                          s_nextSampleId = 1;
	// CONSULT-61: MUST start at 1 -- Miles' async IO treats FileHandle==0 as "not
	// opened, open by name" (milesasync.cpp:440-443) with an EMPTY name for stream
	// chunks, so a stream issued handle 0 degenerates into per-chunk empty-name
	// re-opens (the whole title-music-fix pathology). 0 is also our open-failure
	// return value, so it must never be a valid handle.
	unsigned int                 s_nextFileHandle = 1;
	float                        s_soundCategoryVolumes[Audio::SC_count];
	float                        s_streamVolume = 1.0f;
	bool                         s_audioEnabled = true;
	float                        s_masterVolume = 1.0f;
	int                          s_requestedMaxNumberOfSamples = 32;
	int                          s_maxCached2dSampleSize = 0;
	ConstWatcher<Object>         s_listenerObject;
	float const                  s_rollOffFactor = 0.0f; // The volume of sounds is based on linear distance
	float                        s_occlusion = 0.0f;
	float                        s_obstruction = 0.0f;
	std::string                  s_soundProvider;
	bool                         s_debugWindow = false;
	bool                         s_debugWindowPrioritizedSounds = false;
	bool                         s_debugWindowQueuedSounds = false;
	bool                         s_debugWindowPlayingSamples = false;
	bool                         s_debugSoundStartStop = false;
	bool                         s_debugDumpCachedSamplesTextFile = false;
	bool                         s_debugVisuals = false;
	bool                         s_debugTimerDelay = false;
	bool                         s_toolApplication = false;
	int                          s_priorityCount = 10;
	int                          s_timerHighestDelay = 0;
	int                          s_timerCurrentDelay = 0;
	int                          s_digitalLatency = 0;
	int                          s_digitalCpuPercent = 0;
	int                          s_nonVoiceoverFadeCount = 0;
	int                          s_nonBackgroundFadeCount = 0;
	int                          s_allAudioFadeCount = 0;
	// CONSULT-62 postscript: keep 16. Raising to 32 was audible as sound-start lag
	// (keyboard clicks / footsteps behind the action, Kenny 2026-07-04) -- the
	// mix-ahead delays every start by its own length. 16ms is the responsiveness
	// ceiling; margin problems must be fixed at the source, not buffered.
	int const                    s_bufferFragmentsMin = 16;
	float                        s_averageTimerDelay = 0.0f;
	PerformanceTimer *           s_audioServePerformanceTimer = NULL;
	float                        s_globalAudioFadeVolume = 1.0f;
	float                        s_allAudioFadeFactor = 0.5f;
	float                        s_nonBuffereMusicFadeVolume = 1.0f;

	// CONSULT-62 wave 3b: background music's OWN smoothed fade. It previously
	// multiplied by s_globalAudioFadeVolume only while its count-gate said so --
	// so when unSilenceAllNonBackgroundMusic dropped the count at load-end, the
	// theme instantly inherited the STALE ducked-to-zero global fade (VOLZERO
	// probe: theme muted with tmpl/fade/atten/user all healthy) and then crawled
	// back up: Kenny's deterministic "pops" at every load-in. This variable ramps
	// toward the same targets, so gate flips can never step the volume.
	float                        s_backgroundMusicFadeVolume = 1.0f;
	QueuedSamplesToStartList s_centerBucket;
	SoundBucketList s_leftBucket;
	SoundBucketList s_rightBucket;
	SoundBucketList s_frontBucket;
	SoundBucketList s_backBucket;
	bool s_consolidateQueuedSounds = true;
	int s_soundFallOffPower = 3;
	float const s_centerConsolidateDistanceSquared = sqr(40.0f);
	int s_maxDigitalMixerChannels = 0;
	int s_allocated2dSampleHandles = 0;
	int s_allocated3dSampleHandles = 0;
	bool s_disableMiles = false;

	// CONSULT-60 audio diagnostics: [ClientAudio] streamBufferBytes overrides the
	// Miles default stream buffer (0 = stock: one second of compressed data);
	// [ClientAudio] audioDiagLog=true samples stream fill/starvation state to
	// audio-diag.log (working dir) to convict the load-in music skip + in-game
	// crackle classes. Both default OFF = stock behavior.
	int   s_streamBufferBytes = 0;
	bool  s_audioDiagLog = false;
	FILE* s_audioDiagFile = 0;
	// CONSULT-63 7.2e bring-up: budget-limited Miles file-callback I/O trace (FILEOPEN
	// always; FILESEEK/FILEREAD for the first N calls) -- shows exactly what Miles read
	// before a failed stream open ("Error getting sound format").
	volatile LONG s_diagFileIoLogBudget = 120;
	float s_audioDiagSummaryTimer = 0.0f;

	// CONSULT-67 VOLSET probe: the per-stream VOLSTEP/STARVED probes read
	// stream->samp fields through the miles-9.3b/7.2e COMPILE-TIME struct
	// layouts, which do NOT match either shipped runtime anymore (Win32 runs
	// the retail 7.2a DLL, x64 runs 9.3v) -- those reads are garbage on both
	// (x64 logs chunks=-856759528; Win32 logs constant 0/0). This probe
	// instead edge-logs OUR final computed volume at the AIL_set_sample_
	// volume_levels handoff for music streams, with every term of the
	// product, so a load-fade volume step can be attributed engine-side or
	// pinned inside the Miles DLL. Keyed by HSTREAM; erased at stream close.
	std::map<void *, float> s_lastVolSet;

	HSAMPLE s_bufferedSoundSample = 0;
	HSAMPLE s_bufferedMusicSample = 0;

	bool s_silenceNonBufferedMusic = false;

	int         getMaxStreamSampleCount();
	char const *getAttenuationMethodString(Audio::AttenuationMethod const attenuationMethod);

	CrcString const *  cacheSound(TemporaryCrcString const &path, SampleCache::iterator &iterSampleCache, bool const needToCacheSample);
	void               cacheSample(TemporaryCrcString const &path, AbstractFile *file, SampleCache::iterator &iterSampleCache);
	void               clearMusicOffsets();
	bool               rejectSound(SoundTemplate const &soundTemplate, Vector const &position);
	void               decreaseReferenceCount(CrcString const &path);
	void               addPlayingSound(Sound2 const &sound);
	int                getPlayingSoundCount(CrcString const *soundPath);
	SoundId            playSound(char const * const soundPath, Vector const * const position, Object const * const object, char const *hardPointName, CellProperty const * const parentCell);
	SampleId           createSampleId(Sound2 &sound);
	void               stopSound(SoundId const &soundId, float const fadeOutTime, bool const keepAlive);
	void               removeSoundFromPrioritizedPlayingSounds(Sound2 const &sound);
	void               setSoundCategoryVolume(Audio::SoundCategory const soundCategory, float const volume);
	float              getSoundCategoryVolume(Audio::SoundCategory const soundCategory, bool settingOnly=false);
	bool queueSample(SoundBucketList & soundBucketList, Sound2 & sound, float const distanceSquared, bool const soundIsAlreadyPlaying);
	void insertionSort(QueuedSamplesToStartList & list, Sound2 & sound);
	int getMaxNumberOfSamples();
	int getProviderSpec(std::string const & provider);
	std::string getSpeakerSpec();
	bool isNonBufferedMusic(Audio::SoundCategory const soundCategory);

#ifdef _DEBUG
	char const * const getSoundCategoryString(Audio::SoundCategory const soundCategory);
#endif // _DEBUG

#ifdef _DEBUG
	typedef std::map<uint32, std::string> HandleNameMap;
	HandleNameMap ms_handleNameMap;

	typedef std::set<uint32> HandleSet;
	HandleSet ms_fileCloseHandleSet;

	void determineCallbackError(char const * const callbackName, uint32 const handle)
	{
		HandleNameMap::iterator iter = ms_handleNameMap.find(handle);
		bool const fileOpen = iter != ms_handleNameMap.end();
		bool const fileClosed = ms_fileCloseHandleSet.find(handle) != ms_fileCloseHandleSet.end();

		DEBUG_FATAL(true, ("Trying to file %s on a file that no longer exists. handle=%d, filename=%s, open=%d, still open=%d, closed=%d",
			callbackName,
			handle,
			fileOpen ? iter->second.c_str() : "invalid handle",
			fileOpen ? 1 : 0,
			s_fileMap.find(handle) != s_fileMap.end() ? 1 : 0,
			fileClosed ? 1 : 0));
	}
#endif
}

using namespace AudioNamespace;

// Callbacks for Miles to the TreeFile system

// x64: the Miles AIL_file_*_callback ABI uses UINTa (pointer-width) for the file
// handle (== U32 on x86, __int64 on x64). The handle itself is a synthetic small
// counter (see s_nextFileHandle), so it fits a U32 map key; only the callback
// signature must be UINTa to match the Miles typedef (x64 ABI, BITS-02).
static U32 __stdcall fileOpenCallBack(char const *fileName, UINTa *fileHandle);
static void __stdcall fileCloseCallBack(UINTa fileHandle);
static S32 __stdcall fileSeekCallBack(UINTa fileHandle, S32 offset, U32 type);
static U32 __stdcall fileReadCallBack(UINTa fileHandle, void *buffer, U32 bytes);

static SoundId attachSound(SoundTemplate const *soundTemplate, Object const *object, char const *hardPointName=0);
static bool cacheSound(SoundTemplate const *soundTemplate);
static S32 getBits();
static S32 getFrequency();
static SampleIdToSample3dMap::iterator getIterSampleIdToSample3dMap(SampleId const &sampleId);
static SoundIdToSoundMap::iterator getIterSoundIdToSoundMap(SoundId const &soundId);
static void getSampleTime(char const *path, byte *fileImage, int fileSize, float &timeTotal, float &timeCurrent);
//static int getSoundTemplateSampleSize(Sound2dTemplate const &sound2dTemplate);
static bool isCached(CrcString const &path, SampleCache::iterator &iterSampleCache);
static bool isSample2d(SampleId const &sampleId, SampleIdToSample2dMap::iterator &iterSampleIdToSample2dMap);
static bool isSample3d(SampleId const &sampleId, SampleIdToSample3dMap::iterator &iterSampleIdToSample3dMap);
static bool isSampleStream(SampleId const &sampleId, SampleIdToSampleStreamMap::iterator &iterSampleIdToSampleStreamMap);
static SoundId playSound2d(SoundTemplate const *soundTemplate, Vector const * const position, CellProperty const * const parentCell);
static SoundId playSound3d(SoundTemplate const *soundTemplate, Vector const &position, CellProperty const * const parentCell);
static int getNextSampleId();
static int getNextSoundId();
static Audio::AttenuationMethod getAttenuationMethod(SoundId const &soundId);
static char const * getAttenuationMethodString(Audio::AttenuationMethod const attenuationMethod);
static void stopAllSounds(float const fadeOutTime, bool const suspendActiveSounds);
static void stopSample(Sound2 const &sound);
static void __stdcall endOfSample2dCallBack(HSAMPLE sample);
static void __stdcall endOfSample3dCallBack(HSAMPLE sample);
static void __stdcall endOfSampleStreamCallBack(HSTREAM stream);

// ============================================================================
//
// ProviderData
//
// ============================================================================

//-----------------------------------------------------------------------------
ProviderData::ProviderData()
 : m_supported(false)
 , m_name("")
{
}

// ============================================================================
//
// AudioNamespace
//
// ============================================================================

//-----------------------------------------------------------------------------
char const *AudioNamespace::getAttenuationMethodString(Audio::AttenuationMethod const attenuationMethod)
{
	char const *result = "invalid";

	switch (attenuationMethod)
	{
		case Audio::AM_none:
			{
				result = "attenuation: none";
			}
			break;
		case Audio::AM_2d:
			{
				result = "attenuation: 2D";
			}
			break;
		case Audio::AM_3d:
			{
				result = "attenuation: 3D";
			}
			break;
	}

	return result;
}

//-----------------------------------------------------------------------------
void AudioNamespace::clearMusicOffsets()
{
	MusicOffsetMap::iterator iterMusicOffsetMap = s_musicOffsetMap.begin();

	for (; iterMusicOffsetMap != s_musicOffsetMap.end(); ++iterMusicOffsetMap)
	{
		delete iterMusicOffsetMap->first;
	}

	s_musicOffsetMap.clear();
}

//-----------------------------------------------------------------------------
bool AudioNamespace::rejectSound(SoundTemplate const &soundTemplate, Vector const &position)
{
	bool result = false;

	if ((s_listenerObject != NULL) &&
		!soundTemplate.isInfiniteLooping())
	{
		float const maxAudibleDistance = Audio::getFallOffDistance(soundTemplate.getDistanceAtMaxVolume());
		float const distanceSquaredFromSound = Vector(position - s_listenerObject->getPosition_w()).magnitudeSquared();

		if (distanceSquaredFromSound > (maxAudibleDistance * maxAudibleDistance))
		{
			result = true;

			//DEBUG_REPORT_LOG(true, ("Audio: rejected %s\n", soundTemplate.getName()));

			++s_instantRejectionCount;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
void AudioNamespace::decreaseReferenceCount(CrcString const &path)
{
	SampleCache::iterator iterSampleCache = s_sampleCache.find(&path);

	if (iterSampleCache != s_sampleCache.end())
	{
		// Decrement the reference count

		--(iterSampleCache->second.m_referenceCount);

		//DEBUG_REPORT_LOG(Audio::isDebugEnabled(), ("Audio: Ref count decreased: %d %s\n", iterSampleCache->second.m_referenceCount, iterSampleCache->first->getString()));

		if(iterSampleCache->second.m_referenceCount <= 0)
		{
			// Deallocate its memory

			delete [] iterSampleCache->second.m_sampleRawData;
			s_currentCacheSize -= iterSampleCache->second.m_fileSize;

			DEBUG_REPORT_LOG(Audio::isDebugEnabled(), ("Audio: Sample removed from cache: %s - cache size (%5dk)\n", iterSampleCache->first->getString(), Audio::getCurrentCacheSize() / 1024));

			// Remove it from the sound cache

			delete iterSampleCache->first;

			s_sampleCache.erase(iterSampleCache);
		}
	}
	else
	{
		DEBUG_REPORT_LOG(true, ("Audio: Trying to remove a sample that is not in the cache: %s\n", path.getString()));
	}
}

//-----------------------------------------------------------------------------
void AudioNamespace::addPlayingSound(Sound2 const &sound)
{
	NP_PROFILER_AUTO_BLOCK_DEFINE("AudioNamespace::addPlayingSound");

	SoundIdToSoundMap::const_iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.find(sound.getSoundId());

	if (iterSoundIdToSoundMap != s_soundIdToSoundMap.end())
	{
		PrioritizedPlayingSounds::iterator iterPrioritizedPlayingSounds = s_prioritizedPlayingSounds.begin();

		for (; iterPrioritizedPlayingSounds != s_prioritizedPlayingSounds.end(); ++iterPrioritizedPlayingSounds)
		{
			int const newPriority = iterSoundIdToSoundMap->second->getTemplate()->getPriority();
			int const currentPriority = (*iterPrioritizedPlayingSounds)->getTemplate()->getPriority();
			float const newDistanceSquared = iterSoundIdToSoundMap->second->getDistanceSquaredFromListener();
			float const currentDistanceSquared = (*iterPrioritizedPlayingSounds)->getDistanceSquaredFromListener();

			if ((newPriority < currentPriority) ||
				((newPriority == currentPriority) &&
				 (newDistanceSquared < currentDistanceSquared)))
			{
				s_prioritizedPlayingSounds.insert(iterPrioritizedPlayingSounds, NON_NULL(iterSoundIdToSoundMap->second));
				//DEBUG_REPORT_LOG(s_debugSoundStartStop, ("1 Prioritized sound added: %d %s\n", sound.getSoundId().getId(), iterSoundIdToSoundMap->second->getTemplate()->getName()));
				break;
			}
		}

		if (iterPrioritizedPlayingSounds == s_prioritizedPlayingSounds.end())
		{
			s_prioritizedPlayingSounds.push_back(NON_NULL(iterSoundIdToSoundMap->second));
			//DEBUG_REPORT_LOG(s_debugSoundStartStop, ("2 Prioritized sound added: %d %s\n", sound.getSoundId().getId(), iterSoundIdToSoundMap->second->getTemplate()->getName()));
		}
	}
	else
	{
		DEBUG_FATAL(true, ("This sound should exist: %s", sound.getTemplate()->getName()));
	}

	DEBUG_WARNING((static_cast<int>(s_prioritizedPlayingSounds.size()) > getMaxNumberOfSamples()), ("Too many sounds are playing: %d/ %d max", static_cast<int>(s_prioritizedPlayingSounds.size()), getMaxNumberOfSamples()));
}

//-----------------------------------------------------------------------------
int AudioNamespace::getPlayingSoundCount(CrcString const *soundPath)
{
	int result = 0;

	if (soundPath != NULL)
	{
		PrioritizedPlayingSounds::const_iterator iterPrioritizedPlayingSounds = s_prioritizedPlayingSounds.begin();

		for (; iterPrioritizedPlayingSounds != s_prioritizedPlayingSounds.end(); ++iterPrioritizedPlayingSounds)
		{
			Sound2 const *sound = (*iterPrioritizedPlayingSounds);

			if (sound->getTemplate()->getCrcName() == *soundPath)
			{
				++result;
			}
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
SoundId AudioNamespace::playSound(char const * const soundPath, Vector const * const position, Object const * const object, char const *hardPointName, CellProperty const * const parentCell)
{
//	DEBUG_WARNING((object != NULL) && !object->isInWorld(), ("Trying to play a sound on an object that is not in the world: %s object: %s", soundPath, object->getObjectTemplateName()));

	SoundId result (0, soundPath);

	if (s_installed &&
		(soundPath != NULL))
	{
		// Get the extension

		char const *extension = soundPath;
		char const *currentPosition = soundPath;
		bool done = false;

		do
		{
			currentPosition = strchr(extension, '.');

			if (currentPosition != NULL)
			{
				extension = currentPosition + 1;
			}
			else
			{
				done = true;
			}
		}
		while (!done);

		// Make sure this is a supported sound file

		if (_stricmp(extension, "snd") == 0)
		{
			SoundTemplate const *soundTemplate = SoundTemplateList::fetch(soundPath);

			if (soundTemplate != NULL)
			{
				// Don't reject a stereo sound

				if (soundTemplate->getAttenuationMethod() != Audio::AM_none)
				{
					// The sound looks valid, queue it

					if (position != NULL)
					{
						if (rejectSound(*soundTemplate, *position))
						{
							// Sound is out of range

							//-- Release local resources.
							SoundTemplateList::release(soundTemplate);
							return result;
						}
					}

					if (object != NULL)
					{
						if (rejectSound(*soundTemplate, object->getPosition_w()))
						{
							// Sound is out of range
							//-- Release local resources.
							SoundTemplateList::release(soundTemplate);
							return result;
						}
					}
				}

				if (soundTemplate->is3d())
				{
					if (object != NULL)
					{
						result = attachSound(soundTemplate, object, hardPointName);
					}
					else if (position != NULL)
					{
						result = playSound3d(soundTemplate, *position, parentCell);
					}
					else
					{
						DEBUG_WARNING(true, ("Playing a 3d sound template as a 2d sound because neither a position or object was specified: %s", soundPath));

						result = playSound2d(soundTemplate, NULL, parentCell);
					}
				}
				else if (soundTemplate->is2d())
				{
					if (   (object != NULL)
						&& (soundTemplate->getAttenuationMethod() != Audio::AM_none))
					{
						result = attachSound(soundTemplate, object, hardPointName);
					}
					else
					{
						result = playSound2d(soundTemplate, position, parentCell);
					}
				}
				else
				{
					DEBUG_WARNING(true, ("Attempting to play a non 2d or 3d sound template: %s", soundPath));
				}

				SoundTemplateList::release(soundTemplate);
			}
			else
			{
				if (Audio::isEnabled())
				{
					DEBUG_WARNING(true, ("SoundTemplateList returned a NULL sound template: %s", soundPath));
				}
			}
		}
		else
		{
			WARNING(true, ("Attempting to play unsupported file: %s", soundPath));
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
SampleId AudioNamespace::createSampleId(Sound2 &sound)
{
	SampleId sampleId;

	if (sound.getSamplePath() == NULL)
	{
		DEBUG_WARNING(true, ("Trying to play a sound with a NULL sample path: %s", sound.getTemplate()->getName()));
	}
	else
	{
		// Get the size of the sample

		int sampleSize = 0;

		SampleCache::const_iterator iterSampleCache = s_sampleCache.find(sound.getSamplePath());

		if (iterSampleCache != s_sampleCache.end() &&
			(iterSampleCache->second.m_fileSize != 0))
		{
			sampleSize = iterSampleCache->second.m_fileSize;
		}
		else
		{
			sampleSize = Audio::getSampleSize(sound.getSamplePath()->getString());
		}

		if (sound.is2d())
		{
			// See if this sample needs to be streamed or cached

			if (sampleSize > Audio::getMaxCached2dSampleSize())
			{
				// Stream the sample

				SampleStream sampleStream;

				// CONSULT-60: stream_mem 0 = Miles default of exactly ONE SECOND of
				// compressed data (bufsize = datarate / MSS_STREAM_CHUNKS,
				// mssstrm.cpp:739). [ClientAudio] streamBufferBytes overrides for the
				// load-in music-skip A/B (a 1MB first attempt did NOT cure the skips
				// -- awaiting audio-diag conviction before committing to a value).
				// CONSULT-62 wave 2: time the open -- AIL_open_stream holds the global
				// Miles mutex for allocations/codec setup; long opens on main starve
				// the mixer (crackle-at-sound-start suspect).
				DWORD const diagOpenStart = s_audioDiagFile ? GetTickCount() : 0;
				sampleStream.m_stream = AIL_open_stream(s_digitalDevice2d, sound.getSamplePath()->getString(), s_streamBufferBytes);
				if (s_audioDiagFile)
				{
					DWORD const diagOpenMs = GetTickCount() - diagOpenStart;
					SYSTEMTIME st;
					GetLocalTime(&st);
					fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d OPENCALL %s took %lums stream=%p%s%s\n",
						st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
						sound.getSamplePath()->getString(), diagOpenMs,
						static_cast<void *>(sampleStream.m_stream),
						sampleStream.m_stream ? "" : " lastError=",
						sampleStream.m_stream ? "" : AIL_last_error());
					fflush(s_audioDiagFile);
				}

				if (sampleStream.m_stream != NULL)
				{
					sampleStream.setPath(sound.getSamplePath()->getString());
					sampleStream.m_sound = &sound;
					sampleStream.m_status = Audio::PS_notStarted;

					// Save the current id

					sampleId = SampleId(getNextSampleId());

					// Add it to the stream sample map

					s_sampleIdToSampleStreamMap.insert(std::make_pair(sampleId, sampleStream));
				}
			}
			else if (sampleSize > 0)
			{
				// Save the id to path mapping

				Sample2d sample2d;

				// Create the sample handle

				sample2d.m_sample = AIL_allocate_sample_handle(s_digitalDevice2d);

				if (sample2d.m_sample != NULL)
				{
					// Clear out the sample

					++s_allocated2dSampleHandles;

					sample2d.setPath(sound.getSamplePath()->getString());
					sample2d.m_sound = &sound;
					sample2d.m_status = Audio::PS_notStarted;

					// Save the current id

					sampleId = SampleId(getNextSampleId());

					// Add it to the 2d sample map

					s_sampleIdToSample2dMap.insert(std::make_pair(sampleId, sample2d));
				}
				else if (s_audioDiagFile)
				{
					SYSTEMTIME st;
					GetLocalTime(&st);
					fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d ALLOCFAIL2D %s alloc2d=%d alloc3d=%d lastError=%s\n",
						st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
						sound.getSamplePath()->getString(), s_allocated2dSampleHandles, s_allocated3dSampleHandles, AIL_last_error());
					fflush(s_audioDiagFile);
				}
			}
		}
		else if (sound.is3d())
		{
			// Save the id to path mapping

			Sample3d sample3d;

			// Create the sample handle

			sample3d.m_sample = AIL_allocate_sample_handle(s_digitalDevice2d);

			if (sample3d.m_sample != NULL)
			{
				++s_allocated3dSampleHandles;

				sample3d.setPath(sound.getSamplePath()->getString(), sampleSize);
				sample3d.m_sound = &sound;
				sample3d.m_status = Audio::PS_notStarted;

				// Save the current id

				sampleId = SampleId(getNextSampleId());

				// Add it to the map

				s_sampleIdToSample3dMap.insert(std::make_pair(sampleId, sample3d));
			}
			else
			{
#ifdef _DEBUG
				int const count2d = AIL_active_sample_count(s_digitalDevice2d);
				UNREF(count2d);
				int const count3d = AIL_active_sample_count(s_digitalDevice2d);
				UNREF(count3d);
#endif // _DEBUG
				if (s_audioDiagFile)
				{
					SYSTEMTIME st;
					GetLocalTime(&st);
					fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d ALLOCFAIL3D %s alloc2d=%d alloc3d=%d lastError=%s\n",
						st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
						sound.getSamplePath()->getString(), s_allocated2dSampleHandles, s_allocated3dSampleHandles, AIL_last_error());
					fflush(s_audioDiagFile);
				}
			}
		}
		else
		{
			DEBUG_FATAL(true, ("Sound is neither 2d or 3d"));
		}
	}

	return sampleId;
}

//-----------------------------------------------------------------------------
void AudioNamespace::stopSound(SoundId const &soundId, float const fadeOutTime, bool const keepAlive)
{
	SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundIdToSoundMap != s_soundIdToSoundMap.end())
	{
		// Stop the sound

		Sound2 * const sound = NON_NULL(iterSoundIdToSoundMap->second);

		// CONSULT-62 wave 3 attribution probe: name every stop so the recurring
		// music mute ~7s after zone-in can be pinned to its caller class.
		if (s_audioDiagFile)
		{
			SYSTEMTIME st;
			GetLocalTime(&st);
			fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d STOPSOUND %s fade=%.2f keepAlive=%d\n",
				st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
				sound->getTemplate() && sound->getTemplate()->getName() ? sound->getTemplate()->getName() : "(null)",
				fadeOutTime, keepAlive ? 1 : 0);
			fflush(s_audioDiagFile);
		}

		sound->stop(fadeOutTime, keepAlive);

		if (!keepAlive)
		{
			if (fadeOutTime <= 0.0f)
			{
				DEBUG_REPORT_LOG(s_debugSoundStartStop, ("Audio stop sound(%d) %s\n", sound->getSoundId().getId(), sound->getTemplate()->getName()));

				if (sound->getTemplate()->getSoundCategory() == Audio::SC_voiceover)
				{
					Audio::unFadeAllNonVoiceover();
				}

				s_soundIdToSoundMap.erase(iterSoundIdToSoundMap);

				delete sound;
			}
		}
		else
		{
			DEBUG_REPORT_LOG(s_debugSoundStartStop, ("Audio stop (KEEP ALIVE) sound(%d) %s\n", sound->getSoundId().getId(), sound->getTemplate()->getName()));
		}
	}
}

//-----------------------------------------------------------------------------
void AudioNamespace::removeSoundFromPrioritizedPlayingSounds(Sound2 const &sound)
{
	bool found = false;
	PrioritizedPlayingSounds::iterator iterPrioritizedPlayingSounds = s_prioritizedPlayingSounds.begin();

	for (; iterPrioritizedPlayingSounds != s_prioritizedPlayingSounds.end(); ++iterPrioritizedPlayingSounds)
	{
		Sound2 const &currentSound = *(*iterPrioritizedPlayingSounds);

		if (currentSound.getSoundId().getId() == sound.getSoundId().getId())
		{
			found = true;
			s_prioritizedPlayingSounds.erase(iterPrioritizedPlayingSounds);
			DEBUG_REPORT_LOG(s_debugSoundStartStop, ("Prioritized sound removed(%d) %s\n", currentSound.getSoundId().getId(), currentSound.getTemplate()->getName()));
			break;
		}
	}

	DEBUG_REPORT_LOG(!found, ("Prioritized sound not found(%s)\n", sound.getTemplate()->getName()));
}

// The sound category volume changes the volumes level of the specified sound category,
// this is a further 2d and 3d sound reduction on top of the setSoundVolume, [0...1]
//-----------------------------------------------------------------------------
void AudioNamespace::setSoundCategoryVolume(Audio::SoundCategory const soundCategory, float const volume)
{
#ifdef _DEBUG
	DEBUG_WARNING((volume < 0.0f), ("sound category volume(%f) < 0", volume));
	DEBUG_WARNING((volume > 1.0f), ("sound category volume(%f) > 1", volume));
#endif // _DEBUG

	// CONSULT-62 wave 3 attribution probe: category volume changes silently rescale
	// every playing sound -- log CHANGES so a category-driven music mute is visible.
	float const clamped = clamp(0.0f, volume, 1.0f);
	if (s_audioDiagFile && clamped != s_soundCategoryVolumes[static_cast<unsigned int>(soundCategory)])
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d CATVOL cat=%d %.3f -> %.3f\n",
			st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
			static_cast<int>(soundCategory),
			s_soundCategoryVolumes[static_cast<unsigned int>(soundCategory)], clamped);
		fflush(s_audioDiagFile);
	}

	s_soundCategoryVolumes[static_cast<unsigned int>(soundCategory)] = clamped;
}

// Returns the volume of the specified sound category, this is a further 2d and
// 3d sound reduction on top of the getSoundVolume(), [0...1]
//-----------------------------------------------------------------------------
float AudioNamespace::getSoundCategoryVolume(Audio::SoundCategory const soundCategory, bool settingOnly)
{
	float result = 1.0f;

	result = s_soundCategoryVolumes[static_cast<unsigned int>(soundCategory)];

	if (!settingOnly)
	{
		if (isNonBufferedMusic(soundCategory))
		{
			result *= s_nonBuffereMusicFadeVolume;
		}

		if (soundCategory != Audio::SC_backGroundMusic && soundCategory != Audio::SC_voiceover)
		{
			result *= s_globalAudioFadeVolume;
		}
		else if (soundCategory == Audio::SC_backGroundMusic)
		{
			// CONSULT-62 wave 3b: use the dedicated smoothed fade (see its comment).
			// Multiplying by s_globalAudioFadeVolume only when the count-gate opened
			// stepped background music straight to the stale ducked value.
			result *= s_backgroundMusicFadeVolume;
		}
		else if (soundCategory == Audio::SC_voiceover && (s_nonBackgroundFadeCount != 0 || s_allAudioFadeCount != 0) )
		{
			result *= s_globalAudioFadeVolume;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
bool AudioNamespace::queueSample(SoundBucketList & soundBucketList, Sound2 & sound, float const distanceSquared, bool const soundIsAlreadyPlaying)
{
	bool result = true;

	// See if the sound already exists in the bucket

	SoundBucketList::iterator iterSoundBucketList = soundBucketList.find(&sound.getTemplate()->getCrcName());

	if (iterSoundBucketList != soundBucketList.end())
	{
		SoundBucketData const & existingSound = iterSoundBucketList->second;
		bool playNewSound = false;

		if (   distanceSquared < existingSound.m_distanceSquared
			|| (distanceSquared <= existingSound.m_distanceSquared && !sound.isInfiniteLooping()))
		{
			playNewSound = true;
		}
		else if (   !sound.isInfiniteLooping()
				 && (distanceSquared > 0.0f))
		{
			// The sound is further from the listener, but check to see if we want to play it anyways

			int const totalTimeMs = sound.getTotalTime();

			if (totalTimeMs > 0)
			{
				float const distancePercent = 1.0f - sqr(static_cast<float>(existingSound.m_distanceSquared) / static_cast<float>(distanceSquared));
				int const currentTimeMs = sound.getCurrentTime();
				float const playPercent = static_cast<float>(currentTimeMs) / static_cast<float>(totalTimeMs);

				playNewSound = (playPercent > distancePercent);
			}
		}

		// Sound already exists in the bucket, if the new sound is closer, play it and stop the previous sound

		if (playNewSound)
		{
			// Kill previous sound, since the new one is closer

			float const fadeOutTime = 0.0f;
			bool const keepAlive = iterSoundBucketList->second.m_sound->isInfiniteLooping();

			AudioNamespace::stopSound(iterSoundBucketList->second.m_sound->getSoundId(), fadeOutTime, keepAlive);
			//DEBUG_REPORT_LOG(true, ("Audio::queueSample() iterSoundBucketList->second.m_sound->endOfSample(%s)\n", iterSoundBucketList->second.m_sound->getTemplate()->getName()));

			// Insert the new sound

			iterSoundBucketList->second.m_sound = &sound;
			iterSoundBucketList->second.m_distanceSquared = distanceSquared;
			iterSoundBucketList->second.m_soundIsAlreadyPlaying = soundIsAlreadyPlaying;
		}
		else
		{
			// The new sound is NOT a better choice that what already exists

			result = false;
			//DEBUG_REPORT_LOG(true, ("Audio::queueSample() sound.endOfSample(%s)\n", sound.getTemplate()->getName()));
		}
	}
	else
	{
		// This sound is not in this bucket, add it

		soundBucketList.insert(std::make_pair(&sound.getTemplate()->getCrcName(), SoundBucketData(&sound, distanceSquared, soundIsAlreadyPlaying)));
		//DEBUG_REPORT_LOG(true, ("Audio::queueSample() soundBucketList.insert(%s)\n", sound.getTemplate()->getName()));
	}

	return result;
}

//-----------------------------------------------------------------------------
void AudioNamespace::insertionSort(QueuedSamplesToStartList & list, Sound2 & sound)
{
	int const newPriority = sound.getTemplate()->getPriority();
	float const newDistanceSquaredFromListener = sound.getDistanceSquaredFromListener();

	// Insert the new sound into the queue sorted by priority then distance

	QueuedSamplesToStartList::iterator iterQueuedSoundsToStartList = list.begin();

	for (; iterQueuedSoundsToStartList != list.end(); ++iterQueuedSoundsToStartList)
	{
		int const currentPriority = (*iterQueuedSoundsToStartList)->getTemplate()->getPriority();
		float const currentDistanceSquaredFromListener = (*iterQueuedSoundsToStartList)->getDistanceSquaredFromListener();

		if ((newPriority < currentPriority) ||
			((newPriority == currentPriority) &&
			 (newDistanceSquaredFromListener < currentDistanceSquaredFromListener)))
		{
			list.insert(iterQueuedSoundsToStartList, &sound);
			break;
		}
	}

	// If the item was not inserted previously, then it goes at the end of the list

	if (iterQueuedSoundsToStartList == list.end())
	{
		list.push_back(&sound);
	}
}

//-----------------------------------------------------------------------------
int AudioNamespace::getMaxNumberOfSamples()
{
	return std::min(Audio::getRequestedMaxNumberOfSamples(), Audio::getMaxDigitalMixerChannels());
}

//-----------------------------------------------------------------------------
int AudioNamespace::getProviderSpec(std::string const & provider)
{
	if (provider == "Windows Speaker Configuration")
		return MSS_MC_USE_SYSTEM_CONFIG;
	else if (provider == "Headphones")
		return MSS_MC_HEADPHONES;
	else if (provider == "2 Speakers")
		return MSS_MC_STEREO;
	else if (provider == "4 Speakers")
		return MSS_MC_40_DISCRETE;
	else if (provider == "5.1 Speakers")
		return MSS_MC_51_DISCRETE;
	else if (provider == "6.1 Speakers")
		return MSS_MC_61_DISCRETE;
	else if (provider == "7.1 Speakers")
		return MSS_MC_71_DISCRETE;
	else if (provider == "8.1 Speakers")
		return MSS_MC_81_DISCRETE;
	else if (provider == "Dolby Surround")
		return MSS_MC_DOLBY_SURROUND;

	return MSS_MC_USE_SYSTEM_CONFIG;
}

//-----------------------------------------------------------------------------
std::string AudioNamespace::getSpeakerSpec()
{
	if (s_speakers == MSS_MC_USE_SYSTEM_CONFIG)
		return "Windows Speaker Configuration";
	else if (s_speakers == MSS_MC_HEADPHONES)
		return "Headphones";
	else if (s_speakers == MSS_MC_STEREO)
		return "2 Speakers";
	else if (s_speakers == MSS_MC_40_DISCRETE)
		return "4 Speakers";
	else if (s_speakers == MSS_MC_51_DISCRETE)
		return "5.1 Speakers";
	else if (s_speakers == MSS_MC_61_DISCRETE)
		return "6.1 Speakers";
	else if (s_speakers == MSS_MC_71_DISCRETE)
		return "7.1 Speakers";
	else if (s_speakers == MSS_MC_81_DISCRETE)
		return "8.1 Speakers";
	else if (s_speakers == MSS_MC_DOLBY_SURROUND)
		return "Dolby Surround";

	return "Unknown";
}

#ifdef _DEBUG
//-----------------------------------------------------------------------------
char const * const AudioNamespace::getSoundCategoryString(Audio::SoundCategory const soundCategory)
{
	switch (soundCategory)
	{
		case Audio::SC_ambient:         { return "<ambient>"; } break;
		case Audio::SC_explosion:       { return "<explosion>"; } break;
		case Audio::SC_item:            { return "<item>"; } break;
		case Audio::SC_movement:        { return "<movement>"; } break;
		case Audio::SC_userInterface:   { return "<userInterface>"; } break;
		case Audio::SC_vehicle:         { return "<vehicle>"; } break;
		case Audio::SC_vocalization:    { return "<vocalization>"; } break;
		case Audio::SC_weapon:          { return "<weapon>"; } break;
		case Audio::SC_backGroundMusic: { return "<backGroundMusic>"; } break;
		case Audio::SC_playerMusic:     { return "<playerMusic>"; } break;
		case Audio::SC_machine:         { return "<machine>"; } break;
		case Audio::SC_installation:    { return "<installation>"; } break;
		case Audio::SC_combatMusic:     { return "<combatMusic>"; } break;
		case Audio::SC_voiceover:       { return "<voiceover>"; } break;
		case Audio::SC_bufferedSound:   { return "<bufferedSound>"; } break;
		case Audio::SC_bufferedMusic:   { return "<bufferedMusic>"; } break;
		default: {}
	}

	return "";
}
#endif // _DEBUG

// ============================================================================
//
// Audio
//
// ============================================================================

#ifdef _DEBUG
//-----------------------------------------------------------------------------
void Audio::showAudioDebug()
{
	DEBUG_REPORT_PRINT(!s_audioEnabled, ("Audio Enabled              - no\n"));
	DEBUG_REPORT_PRINT(true, ("Miles Version              - %s\n", getMilesVersion().c_str()));
	DEBUG_REPORT_PRINT(true, ("Cache Size                 - %d KB\n", getCurrentCacheSize() / 1024));
	//DEBUG_REPORT_PRINT(true, ("Max Cached 2d Sample Size  - %d KB\n", getMaxCached2dSampleSize() / 1024));
	DEBUG_REPORT_PRINT(true, ("Cached Samples             - %d\n", getCachedSampleCount()));
	DEBUG_REPORT_PRINT(true, ("# of Sounds Created        - %d\n", getSoundCount()));
	DEBUG_REPORT_PRINT(true, ("# of Samples Playing       - %2d/max(%2d) requestedMax(%2d) milesMax(%2d)\n", static_cast<int>(s_prioritizedPlayingSounds.size()), getMaxNumberOfSamples(), Audio::getRequestedMaxNumberOfSamples(), Audio::getMaxDigitalMixerChannels()));
	DEBUG_REPORT_PRINT(true, ("Allocated Sound2d          - %d/%d peak\n", Sound2d::getCount(), Sound2d::getMaxCount()));
	DEBUG_REPORT_PRINT(true, ("Allocated Sound3d          - %d/%d peak\n", Sound3d::getCount(), Sound3d::getMaxCount()));
	DEBUG_REPORT_PRINT(true, ("CPU Usage/Latency          - %d%%/%d ms\n", s_digitalCpuPercent, s_digitalLatency));
	DEBUG_REPORT_PRINT(true, ("Sample Cache Hits/Misses   - %d/%d\n", getCacheHitCount(), getCacheMissCount()));
	DEBUG_REPORT_PRINT(true, ("Sound Instant Rejections   - %d\n", s_instantRejectionCount));
	DEBUG_REPORT_PRINT(true, ("Next sampleId/soundId      - %d/%d\n", s_nextSampleId, s_nextSoundId));
	DEBUG_REPORT_PRINT(true, ("Timer Delay (ms)           - %d/%d peak (%.1f) avg\n", s_timerCurrentDelay, s_timerHighestDelay, s_averageTimerDelay));
	DEBUG_REPORT_PRINT(true, ("User Speaker Setting       - %s\n", getCurrent3dProvider().c_str()));
	DEBUG_REPORT_PRINT(true, ("Miles Speaker Setting      - %s\n", getSpeakerSpec().c_str()));
	DEBUG_REPORT_PRINT(true, ("Environmental Reverb       - %s\n", getRoomTypeString()));
	DEBUG_REPORT_PRINT(true, ("Obstruction (interiors)    - %d%%\n", static_cast<int>(getObstruction() * 100.0f + 0.5f)));
	DEBUG_REPORT_PRINT(true, ("Occlusion (inside vs out)  - %d%%\n", static_cast<int>(getOcclusion() * 100.0f + 0.5f)));
	DEBUG_REPORT_PRINT(true, ("Listener Position          - (%.1f, %.1f, %.1f)\n", s_listener.m_positionCurrent.x, s_listener.m_positionCurrent.y, s_listener.m_positionCurrent.z));
	DEBUG_REPORT_PRINT(true, ("Falloff Power              - (%d)\n", s_soundFallOffPower));

	DEBUG_REPORT_PRINT(true, ("Non-voiceover fade count   - (%d)\n", s_nonVoiceoverFadeCount));
	DEBUG_REPORT_PRINT(true, ("Non-back music fade count  - (%d)\n", s_nonBackgroundFadeCount));
	DEBUG_REPORT_PRINT(true, ("All audio fade count       - (%d)\n", s_allAudioFadeCount));
	DEBUG_REPORT_PRINT(true, ("Non-buffered silenced      - (%s)\n", s_silenceNonBufferedMusic ? "true" : "false"));

	//DEBUG_REPORT_PRINT(true, ("Listener Translation       - (%.1f, %.1f, %.1f)\n", getListenerPosition().x, getListenerPosition().y, getListenerPosition().z));
	//DEBUG_REPORT_PRINT(true, ("Master Volume              - %.2f\n", getMasterVolume()));
	//DEBUG_REPORT_PRINT(true, ("Sound Effect Volume        - %.2f\n", getSoundEffectVolume()));
	//DEBUG_REPORT_PRINT(true, ("Background Music Volume    - %.2f\n", getBackGroundMusicVolume()));
	//DEBUG_REPORT_PRINT(true, ("Player Music Volume        - %.2f\n", getPlayerMusicVolume()));
	//DEBUG_REPORT_PRINT(true, ("User Interface Volume      - %.2f\n", getUserInterfaceVolume()));

	if (s_debugWindowPrioritizedSounds)
	{
		// Prioritized sounds

		DEBUG_REPORT_PRINT(true, ("** Prioritized Sounds Playing (%d) **\n", static_cast<int>(s_prioritizedPlayingSounds.size())));
		PrioritizedPlayingSounds::const_iterator iterPrioritizedPlayingSounds = s_prioritizedPlayingSounds.begin();

		for (; iterPrioritizedPlayingSounds != s_prioritizedPlayingSounds.end(); ++iterPrioritizedPlayingSounds)
		{
			Sound2 const *sound = (*iterPrioritizedPlayingSounds);

			bool const occluded = sound->isOccluded();
			bool obstructed = sound->isObstructed();

			DEBUG_REPORT_PRINT(true, ("%d %6.2f sid: %3d %s%s%s %s\n", sound->getTemplate()->getPriority() + 1, sound->getDistanceFromListener(), sound->getSoundId().getId(), occluded ? "OCC" : "", obstructed ? "OBS" : "", !occluded && !obstructed ? " NA" : "", sound->getTemplate()->getName()));
		}
	}

	if (s_debugWindowQueuedSounds)
	{
		// Show all the sounds queued in the audio system

		typedef std::map<std::string, int> SoundCount;
		static SoundCount soundCount;
		soundCount.clear();

		DEBUG_REPORT_PRINT(true, ("** Sounds Queued ** (%d)\n", static_cast<int>(s_soundIdToSoundMap.size())));
		SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.begin();

		for (; iterSoundIdToSoundMap != s_soundIdToSoundMap.end(); ++iterSoundIdToSoundMap)
		{
			const char *path = iterSoundIdToSoundMap->second->getTemplate()->getName();

			SoundCount::iterator iterSoundCount = soundCount.find(path);

			if (iterSoundCount == soundCount.end())
			{
				soundCount.insert(std::make_pair(path, 1));
			}
			else
			{
				++(iterSoundCount->second);
			}
		}

		SoundCount::const_iterator iterSoundCount = soundCount.begin();

		for (; iterSoundCount != soundCount.end(); ++iterSoundCount)
		{
			DEBUG_REPORT_PRINT(true, ("%2dx %s\n", iterSoundCount->second, iterSoundCount->first.c_str()));
		}
	}

	if (s_debugWindowPlayingSamples)
	{
		// Show all the playing streamed samples

		DEBUG_REPORT_PRINT(true, ("** Stream Samples Playing ** (%d)\n", static_cast<int>(s_sampleIdToSampleStreamMap.size())));
		SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap = s_sampleIdToSampleStreamMap.begin();

		for (; iterSampleIdToSampleStreamMap != s_sampleIdToSampleStreamMap.end(); ++iterSampleIdToSampleStreamMap)
		{
			int const attenuation = static_cast<int>(iterSampleIdToSampleStreamMap->second.m_sound->getAttenuation() * 100.0f);
			int const volume = static_cast<int>(iterSampleIdToSampleStreamMap->second.m_sound->getTemplateVolume() * 100.0f);
			float const distance = iterSampleIdToSampleStreamMap->second.m_sound->getDistanceFromListener();
			std::string fileName = FileNameUtils::get(iterSampleIdToSampleStreamMap->second.getPath()->getString(), FileNameUtils::fileName | FileNameUtils::extension);

			DEBUG_REPORT_PRINT(true, ("v%%: %3d atn%%: %3d dis: %6.2f sid: %3d %s\n", volume, attenuation, distance, iterSampleIdToSampleStreamMap->second.m_sound->getSoundId().getId(), fileName.c_str()));
		}

		// Show all the playing 2d samples

		DEBUG_REPORT_PRINT(true, ("** 2d Samples Playing ** (%d)\n", static_cast<int>(s_sampleIdToSample2dMap.size())));
		SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap = s_sampleIdToSample2dMap.begin();

		for (; iterSampleIdToSample2dMap != s_sampleIdToSample2dMap.end(); ++iterSampleIdToSample2dMap)
		{
			int const attenuation = static_cast<int>(iterSampleIdToSample2dMap->second.m_sound->getAttenuation() * 100.0f);
			int const volume = static_cast<int>(iterSampleIdToSample2dMap->second.m_sound->getTemplateVolume() * 100.0f);
			float const distance = iterSampleIdToSample2dMap->second.m_sound->getDistanceFromListener();
			std::string fileName = FileNameUtils::get(iterSampleIdToSample2dMap->second.getPath()->getString(), FileNameUtils::fileName | FileNameUtils::extension);

			DEBUG_REPORT_PRINT(true, ("v%%: %3d atn%%: %3d dis: %6.2f sid: %3d %s\n", volume, attenuation, distance, iterSampleIdToSample2dMap->second.m_sound->getSoundId().getId(), fileName.c_str()));
		}

		// Show all the playing 3d samples

		DEBUG_REPORT_PRINT(true, ("** 3d Samples Playing ** (%d)\n", static_cast<int>(s_sampleIdToSample3dMap.size())));
		SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap = s_sampleIdToSample3dMap.begin();

		for (; iterSampleIdToSample3dMap != s_sampleIdToSample3dMap.end(); ++iterSampleIdToSample3dMap)
		{
			int const attenuation = static_cast<int>(iterSampleIdToSample3dMap->second.m_sound->getAttenuation() * 100.0f);
			int const volume = static_cast<int>(iterSampleIdToSample3dMap->second.m_sound->getTemplateVolume() * 100.0f);
			float const distance = iterSampleIdToSample3dMap->second.m_sound->getDistanceFromListener();
			std::string fileName = FileNameUtils::get(iterSampleIdToSample3dMap->second.getPath()->getString(), FileNameUtils::fileName | FileNameUtils::extension);

			DEBUG_REPORT_PRINT(true, ("v%%: %3d atn%%: %3d dis: %6.2f sid: %3d %s\n", volume, attenuation, distance, iterSampleIdToSample3dMap->second.m_sound->getSoundId().getId(), fileName.c_str()));
		}
	}
}

//-----------------------------------------------------------------------------
void Audio::debugDumpAudioText()
{
	s_debugDumpCachedSamplesTextFile = false;

	FILE *fp = fopen("audio.txt", "w+");

	if (fp != NULL)
	{
		fprintf(fp, "SWG Audio - Reference Counted Sound Cache (%s %s)\n", __DATE__, __TIME__);
		fprintf(fp, "%d samples @ %d KB\n", static_cast<int>(s_sampleCache.size()), Audio::getCurrentCacheSize() / 1024);
		fprintf(fp, "------------------\n");
		fprintf(fp, "\n");

		// Make an alphabetically sorted list of all the samples

		typedef std::map<std::string, SampleCacheEntry> TempMap;
		TempMap tempMap;

		SampleCache::const_iterator iterSampleCache = s_sampleCache.begin();

		for (; iterSampleCache != s_sampleCache.end(); ++iterSampleCache)
		{
			tempMap.insert(std::make_pair(iterSampleCache->first->getString(), iterSampleCache->second));
		}

		// Dump all the cached non-streamed samples

		int index = 1;
		iterSampleCache = s_sampleCache.begin();

		fprintf(fp, "** Cached Non-Streamed Samples **\n");
		fprintf(fp, "\n");

		TempMap::const_iterator iterTempMap = tempMap.begin();

		for (; iterTempMap != tempMap.end(); ++iterTempMap)
		{
			SampleCacheEntry const &sampleCacheEntry = iterTempMap->second;

			if (sampleCacheEntry.m_fileSize > 0)
			{
				fprintf(fp, "%4d ref cnt (%2d) (%4d KB) %s\n", index, sampleCacheEntry.m_referenceCount, sampleCacheEntry.m_fileSize / 1024, iterTempMap->first.c_str());
				++index;
			}
		}
		fprintf(fp, "\n");

		// Dump all the cached streamed samples

		index = 1;
		iterTempMap = tempMap.begin();

		fprintf(fp, "** Cached Streamed Samples **\n");
		fprintf(fp, "\n");

		for (; iterTempMap != tempMap.end(); ++iterTempMap)
		{
			SampleCacheEntry const &sampleCacheEntry = iterTempMap->second;

			if (sampleCacheEntry.m_fileSize <= 0)
			{
				fprintf(fp, "%4d (%2d) %s\n", index, sampleCacheEntry.m_referenceCount, iterTempMap->first.c_str());
				++index;
			}
		}

		fclose(fp);
		fp = NULL;
	}
}
#endif // _DEBUG

//-----------------------------------------------------------------------------
bool Audio::install()
{
	DEBUG_FATAL(s_installed, ("Already installed"));

	s_disableMiles = ConfigFile::getKeyBool("ClientAudio", "disableMiles", false);

	// CONSULT-60 audio diagnostics (see the namespace comment)
	s_streamBufferBytes = ConfigFile::getKeyInt("ClientAudio", "streamBufferBytes", 0);
	s_audioDiagLog = ConfigFile::getKeyBool("ClientAudio", "audioDiagLog", false);

	// CONSULT-61: the one-slot empty-name shim is obsolete (handles start at 1 so the
	// empty-name path never fires) and harmful with concurrent streams; off unless
	// explicitly re-armed.
	s_titleMusicStreamFix = ConfigFile::getKeyBool("ClientAudio", "titleMusicStreamFix", false);

	// CONSULT-62: config gate for the 35-05 distant-3D-sound change so overall
	// loudness can be A/B'd against a stock client (keeping far 3D sources audible
	// instead of hard-muted makes the whole soundscape denser/louder).
	s_disable3dMuteAtMax = ConfigFile::getKeyBool("ClientAudio", "disable3dMuteAtMax", true);

	if (s_disableMiles)
	{
		REPORT_LOG(true, ("Audio: Miles is disabled. To enable miles, set \"disableMiles=false\" in the [ClientAudio] section of client.cfg.\n"));
		return false;
	}

#ifdef _DEBUG
	DebugFlags::registerFlag(s_debugTimerDelay, "ClientAudio", "debugView_TimerDelay");
	DebugFlags::registerFlag(s_debugVisuals, "ClientAudio", "debugVisuals");
	DebugFlags::registerFlag(s_debugWindow, "ClientAudio", "debugWindow", Audio::showAudioDebug);
	DebugFlags::registerFlag(s_debugSoundStartStop, "ClientAudio", "debugView_SoundStartStop");
	DebugFlags::registerFlag(s_debugDumpCachedSamplesTextFile, "ClientAudio", "debugDump_audio.txt", Audio::debugDumpAudioText);
	DebugFlags::registerFlag(s_debugWindowPrioritizedSounds, "ClientAudio", "debugWindow_PrioritizedSounds");
	DebugFlags::registerFlag(s_debugWindowQueuedSounds, "ClientAudio", "debugWindow_QueuedSounds");
	DebugFlags::registerFlag(s_debugWindowPlayingSamples, "ClientAudio", "debugWindow_PlayingSamples");
	DebugFlags::registerFlag(s_consolidateQueuedSounds, "ClientAudio", "consolidateQueuedSounds");
#endif // _DEBUG

	DEBUG_REPORT_LOG (true, ("+=======================================================================\n"));
	REPORT_LOG(true, ("Audio: Starting intialization\n"));

	// Set the initial sound category volumes

	for (int i = 0; i < Audio::SC_count; ++i)
	{
		if (static_cast<SoundCategory>(i) == SC_backGroundMusic)
		{
			s_soundCategoryVolumes[i] = getDefaultBackGroundMusicVolume();
		}
		else if (static_cast<SoundCategory>(i) == SC_playerMusic)
		{
			s_soundCategoryVolumes[i] = getDefaultPlayerMusicVolume();
		}
		else if (static_cast<SoundCategory>(i) == SC_userInterface)
		{
			s_soundCategoryVolumes[i] = getDefaultUserInterfaceVolume();
		}
		else
		{
			s_soundCategoryVolumes[i] = 1.0f;
		}
	}

	s_allAudioFadeFactor = getDefaultFadeAllFactor();

	// Load the settings from the previous session

	const char * const section = "ClientAudio";
	LocalMachineOptionManager::registerOption(s_masterVolume,                             section, "masterVolume");
	LocalMachineOptionManager::registerOption(s_soundCategoryVolumes[SC_explosion],       section, "soundEffectVolume");
	LocalMachineOptionManager::registerOption(s_soundCategoryVolumes[SC_backGroundMusic], section, "backGroundMusicVolume");
	LocalMachineOptionManager::registerOption(s_soundCategoryVolumes[SC_playerMusic],     section, "playerMusicVolume");
	LocalMachineOptionManager::registerOption(s_soundCategoryVolumes[SC_userInterface],   section, "userInterfaceVolume");
	LocalMachineOptionManager::registerOption(s_soundCategoryVolumes[SC_ambient],         section, "ambientEffectVolume");
	LocalMachineOptionManager::registerOption(s_requestedMaxNumberOfSamples, section, "maxSampleCount");
	LocalMachineOptionManager::registerOption(s_audioEnabled,                             section, "enabled");
	LocalMachineOptionManager::registerOption(s_soundProvider, section, "soundProvider");
	LocalMachineOptionManager::registerOption(s_allAudioFadeFactor, section, "fadeFactor");

	REPORT_LOG(!s_audioEnabled, ("Audio: Audio is disabled. To enable audio, set \"enabled=true\" in the [ClientAudio] section of client.cfg, or enable audio in the game options menu.\n"));

	setRequestedMaxNumberOfSamples(s_requestedMaxNumberOfSamples);
	setMaxCached2dSampleSize(ConfigClientAudio::getMaxCached2dSampleSize());
	setSoundEffectVolume(s_soundCategoryVolumes[SC_explosion]);

	s_localPurgeList.clear();

	// Set the miles directory

	std::string redistDirectory(AIL_set_redist_directory("miles"));

	// Initialize the Miles Sound System

	AIL_startup();

	// 24-02 (D-12): one-shot Miles codec/provider redist presence probe.
	// redistDirectory is the RELATIVE string Miles returns (typically "miles"), resolved against the
	// process working dir (stage/ at launch) -- build probe paths the same way Miles loads them so the
	// check matches actual load resolution. A fresh clone or a stale/partial stage/miles makes music +
	// in-world audio silent while UI 2D wavs still play (the codec-only "half-dead audio" case that the
	// AIL_open_digital_driver failure path at :1292 does NOT catch, since the digital driver opens via
	// system DirectSound). Emit EXACTLY ONE clear startup warning here and set s_milesCodecRedistAvailable
	// so the per-sample flood site below stays quiet (REVIEW/Codex HIGH: loud-once-then-quiet).
	{
		// 35-02 (Pitfall 1 / concern 11): the 7.2e hardcode probed the legacy software-codec provider
		// filenames -- which 9.3b does NOT ship -- so the probe ALWAYS false-reported "redist missing"
		// and gated audio off.
		// Probe the platform-correct 9.3b provider set instead, split into two tiers:
		//   REQUIRED-CORE: runtime DLL + the decoders the music/UI path needs. A miss => suppress audio.
		//   OPTIONAL-EFFECT: the 3D (DS3D) + reverb (EAX) filters. A miss => warn-by-name only; music + UI
		//                    + decode still work, only 3D-positioning / reverb degrade (D-06), NOT a mute.
		// The WARNING names the SPECIFIC missing filename so the boot log says WHICH file suppressed audio.
		//
		// codec-probe platform split -- intentional _M_X64 (NOT Phase-33 scaffolding); keep.
#if defined(_M_X64)
		static char const * const s_requiredCoreMilesCodecs[]   = { "mss64.dll", "mss64mp3.asi", "mss64ogg.asi" };
		static char const * const s_optionalEffectMilesCodecs[] = { "mss64ds3d.flt", "mss64eax.flt" };
#else
		static char const * const s_requiredCoreMilesCodecs[]   = { "mss32.dll", "mssmp3.asi", "mssogg.asi" };
		static char const * const s_optionalEffectMilesCodecs[] = { "mssds3d.flt", "msseax.flt" };
#endif

		// REQUIRED-CORE: a miss flips s_milesCodecRedistAvailable=false (audio suppressed) + names the file.
		bool coreCodecsPresent = true;
		for (size_t i = 0; i < sizeof(s_requiredCoreMilesCodecs) / sizeof(s_requiredCoreMilesCodecs[0]); ++i)
		{
			std::string const codecPath(redistDirectory + "\\" + s_requiredCoreMilesCodecs[i]);
			FILE * const codecFile = fopen(codecPath.c_str(), "rb");
			if (codecFile)
				fclose(codecFile);
			else
			{
				coreCodecsPresent = false;
				s_milesCodecRedistAvailable = false;
				WARNING(true, ("Audio: REQUIRED Miles codec/provider '%s' missing in '%s' -- music and world audio will be silent. Run tools/setup/setup-client.ps1 or rebuild to stage stage/miles.", s_requiredCoreMilesCodecs[i], redistDirectory.c_str()));
				break;
			}
		}

		// OPTIONAL-EFFECT: a miss warns by name only -- 3D / reverb degrade but audio is NOT suppressed.
		if (coreCodecsPresent)
		{
			for (size_t i = 0; i < sizeof(s_optionalEffectMilesCodecs) / sizeof(s_optionalEffectMilesCodecs[0]); ++i)
			{
				std::string const codecPath(redistDirectory + "\\" + s_optionalEffectMilesCodecs[i]);
				FILE * const codecFile = fopen(codecPath.c_str(), "rb");
				if (codecFile)
					fclose(codecFile);
				else
				{
					WARNING(true, ("Audio: OPTIONAL Miles effect filter '%s' missing in '%s' -- 3D-positioning / reverb will degrade, but music + UI audio still play.", s_optionalEffectMilesCodecs[i], redistDirectory.c_str()));
				}
			}
		}
	}

	// Set the file system callbacks

	AIL_set_file_callbacks(fileOpenCallBack, fileCloseCallBack, fileSeekCallBack, fileReadCallBack);

	// Initialize the audio driver

	s_maxDigitalMixerChannels = AIL_get_preference(DIG_MIXER_CHANNELS);

	// 35-05: turn OFF Miles' DIG_3D_MUTE_AT_MAX (default YES in 9.3x) so distant 3D sounds attenuate
	// smoothly instead of hard-muting at their max distance. Must precede AIL_open_digital_driver.
	if (s_disable3dMuteAtMax)
		AIL_set_preference(DIG_3D_MUTE_AT_MAX, 0);   // 0 == NO

	s_digitalDevice2d = AIL_open_digital_driver(getFrequency(), getBits(), getProviderSpec(getCurrent3dProvider()), 0);

	if (!s_digitalDevice2d)
	{
		// If the sound driver set via the options fails, attempt to just use a default driver
		WARNING(true, ("Audio:  Digital Audio - Attempting to use a generic stereo seting, user option failed. Miles Error (%s)", AIL_last_error()));
		s_digitalDevice2d = AIL_open_digital_driver(getFrequency(), getBits(), MSS_MC_STEREO, 0);
		s_soundProvider = "2 Speakers";

		if (!s_digitalDevice2d)
		{
			// default driver also failed, shutting down the audio system
			WARNING(true, ("Audio: Digital Audio - Shutting down the audio system. Miles Error (%s)", AIL_last_error()));

			remove();
			setEnabled(false);
			return false;
		}
	}

	s_listener.m_object = s_digitalDevice2d;
	s_listener.alter();

	AIL_set_3D_rolloff_factor(s_digitalDevice2d, s_rollOffFactor);

	AIL_speaker_configuration(s_digitalDevice2d, NULL, NULL, NULL, &s_speakers);

	REPORT_LOG(true, ("Audio: %s\n", getCurrent3dProvider().c_str()));
	REPORT_LOG(true, ("Audio: Miles speakers are %s\n", getSpeakerSpec().c_str()));
	REPORT_LOG(true, ("Audio: Miles Max DIG_MIXER_CHANNELS(%d)\n", getMaxDigitalMixerChannels()));

	// This disables any reberb from a previous product

	// 35-02 (concern 7 / A1): the 9.3b AIL_set_room_type / AIL_room_type take a S32 bus_index arg
	// (absent in 7.2e). We route reverb on bus_index=0 (the master mixer bus). Log it ONCE at install
	// so the boot log positively proves the reverb edit used bus 0 -- making A1 observable without
	// relying solely on the cell-transition UAT ear.
	REPORT_LOG(true, ("Audio: room_type bus_index=0 (master bus) -- 9.3b reverb API port (35-02).\n"));

	setRoomType(RT_generic);

	s_obstruction = ConfigClientAudio::getObstruction();
	s_occlusion = ConfigClientAudio::getOcclusion();

	// Create the data table

	clearMusicOffsets();

	Iff dataTableIff;

	if (dataTableIff.open("datatables/music/music.iff", true))
	{
		delete s_musicDataTable;
		s_musicDataTable = new DataTable;
		s_musicDataTable->load(dataTableIff);

		// Store the row music name and its row index for fast indexing
		// into the DataTable.

		const int rowCount = s_musicDataTable->getNumRows();

		for (int index = 0; index < rowCount; ++index)
		{
			std::string const &musicName = s_musicDataTable->getStringValue(0, index);

			s_musicOffsetMap.insert(std::make_pair(new PersistentCrcString(musicName.c_str(), true), index));
		}
	}

	setNormalPreMixBuffer();

	AbstractFile::setAudioServe(serve);

	REPORT_LOG(true, ("Audio: Finished initializing\n"));
	DEBUG_REPORT_LOG (true, ("+=======================================================================\n"));

	s_audioServePerformanceTimer = new PerformanceTimer;

	s_installed = true;

	return true;
}

//-----------------------------------------------------------------------------
void Audio::remove()
{
	setRoomType(RT_generic);

#ifdef _DEBUG
	DebugFlags::unregisterFlag(s_debugTimerDelay);
	DebugFlags::unregisterFlag(s_debugVisuals);
	DebugFlags::unregisterFlag(s_debugWindow);
	DebugFlags::unregisterFlag(s_debugWindowPrioritizedSounds);
	DebugFlags::unregisterFlag(s_debugWindowQueuedSounds);
	DebugFlags::unregisterFlag(s_debugWindowPlayingSamples);
	DebugFlags::unregisterFlag(s_debugSoundStartStop);
	DebugFlags::unregisterFlag(s_debugDumpCachedSamplesTextFile);
#endif // _DEBUG

	Audio::stopAllSounds();

#ifdef _DEBUG
	unsigned int const sample2dMapSize = s_sampleIdToSample2dMap.size();
	UNREF(sample2dMapSize);
	DEBUG_WARNING(!s_sampleIdToSample2dMap.empty(), ("Sample 2d map not empty"));
#endif // _DEBUG
	s_sampleIdToSample2dMap.clear();

#ifdef _DEBUG
	unsigned int const sample3dMapSize = s_sampleIdToSample3dMap.size();
	UNREF(sample3dMapSize);
	DEBUG_WARNING(!s_sampleIdToSample3dMap.empty(), ("Sample 3d map not empty"));
#endif // _DEBUG
	s_sampleIdToSample3dMap.clear();

#ifdef _DEBUG
	unsigned int const streamMapSize = s_sampleIdToSampleStreamMap.size();
	UNREF(streamMapSize);
	DEBUG_WARNING(!s_sampleIdToSampleStreamMap.empty(), ("Sample stream map not empty"));
#endif // _DEBUG
	s_sampleIdToSampleStreamMap.clear();

	s_localPurgeList.clear();

	// Shutdown Miles

	if (s_installed)
	{
		s_installed = false;

		// Close the 3d driver


		// Close the 2d driver

		if (s_digitalDevice2d)
		{
			s_digitalDevice2d = 0;
		}

		// Now shutdown Miles completely

		AIL_shutdown();
	}

#ifdef _DEBUG
	int const fileMapCount = s_fileMap.size();
	DEBUG_WARNING((fileMapCount > 0), ("File handles (%d) are still allocated.", fileMapCount));
#endif // _DEBUG

	clearMusicOffsets();

	delete s_musicDataTable;
	s_musicDataTable = NULL;

	// Delete all the reference counted samples

	SampleCache::iterator iterSampleCache= s_sampleCache.begin();

	for (; iterSampleCache != s_sampleCache.end(); ++iterSampleCache)
	{
		// Delete the CrcString

		delete iterSampleCache->first;

		// Delete the sample data

		delete [] iterSampleCache->second.m_sampleRawData;
	}

	s_sampleCache.clear();

	delete s_audioServePerformanceTimer;
	s_audioServePerformanceTimer = NULL;
}

//-----------------------------------------------------------------------------
bool Audio::isEnabled()
{
	return s_audioEnabled;
}

//-----------------------------------------------------------------------------
void Audio::setEnabled(bool const enabled)
{
	s_audioEnabled = enabled;
}

//-----------------------------------------------------------------------------

bool Audio::isMilesEnabled()
{
	return !s_disableMiles;
}

//-----------------------------------------------------------------------------

void *Audio::getMilesDigitalDriver()
{
	return (void *)s_digitalDevice2d;
}

//-----------------------------------------------------------------------------

CrcString const *Audio::increaseReferenceCount(const char *path, bool const needToCacheSample)
{
	SampleCache::iterator iterSampleCache;
	CrcString const *result = AudioNamespace::cacheSound(TemporaryCrcString(path, true), iterSampleCache, needToCacheSample);

	return result;
}

//-----------------------------------------------------------------------------
void AudioNamespace::cacheSample(TemporaryCrcString const &path, AbstractFile *file, SampleCache::iterator &iterSampleCache)
{
	if (file != NULL)
	{
		// Load and allocate the sample from disk

		int fileSize = file->length();
		byte *fileImage = file->readEntireFileAndClose();

		if (fileImage != NULL)
		{
			// Get the lenth of the sample

			float timeTotal;
			float timeCurrent;

			getSampleTime(path.getString(), fileImage, fileSize, timeTotal, timeCurrent);

			s_currentCacheSize += fileSize;

			iterSampleCache->second.m_sampleRawData = fileImage;
			iterSampleCache->second.m_fileSize = fileSize;
			iterSampleCache->second.m_time = timeTotal;

			DEBUG_REPORT_LOG(Audio::isDebugEnabled(), ("Audio: Sample cached (%5dk): size: %4dk %s (%s)\n", Audio::getCurrentCacheSize() / 1024, fileSize / 1024, path.getString(), Audio::getSampleType(fileImage, fileSize).c_str()));
		}
		else
		{
			// 24-02 (D-12, REVIEW/Codex HIGH): gate the per-sample flood on the codec-availability flag.
			// When the Miles codec/provider redist is known-absent (the startup probe already warned once),
			// suppress this per-sample warning so a missing-codec state does not flood the log 141k lines.
			// When codecs ARE present (flag stays true), genuine one-off bad samples still warn as before.
			DEBUG_WARNING(s_milesCodecRedistAvailable, ("Error loading and allocating the sample: %s", path.getString()));
		}
	}
}

//-----------------------------------------------------------------------------
CrcString const *AudioNamespace::cacheSound(TemporaryCrcString const &path, SampleCache::iterator &iterSampleCache, bool const needToCacheSample)
{
	CrcString const *result = NULL;

	if (s_installed && !path.isEmpty())
	{
		bool const cached = isCached(path, iterSampleCache);

		if (!cached ||
			(needToCacheSample &&
			(iterSampleCache->second.m_sampleRawData == NULL)))
		{
			AbstractFile *file = TreeFile::open(path.getString(), AbstractFile::PriorityData, true);

			if (file != NULL)
			{
				++s_cacheMissCount;

				if (cached)
				{
					// Since the sound was already cached but the sample was not, just cache the sample now

					cacheSample(path, file, iterSampleCache);
				}
				else
				{
					SampleCacheEntry sampleCacheEntry;

					// Save information about this sample

					sampleCacheEntry.setExtension(path.getString());

					std::pair<SampleCache::iterator, bool> resultPair = s_sampleCache.insert(std::make_pair(new PersistentCrcString(path.getString(), path.getCrc()), sampleCacheEntry));
					iterSampleCache = resultPair.first;

					DEBUG_FATAL((resultPair.second == false), ("Error putting the sample in the sound cache: %s", path.getString()));

					if (needToCacheSample)
					{
						cacheSample(path, file, iterSampleCache);
					}
				}

				++(iterSampleCache->second.m_referenceCount);
				//DEBUG_REPORT_LOG(Audio::isDebugEnabled(), ("Audio: Ref count increased: %d %s\n", iterSampleCache->second.m_referenceCount, path.getString()));

				result = iterSampleCache->first;

				delete file;
			}
			else
			{
				// The file must not exist on disk or it is not a sound file

				DEBUG_WARNING(true, ("The sound file does not exist: %s", path.getString()));
				result = NULL;
			}
		}
		else
		{
			// Increase the reference count to this sample

			++(iterSampleCache->second.m_referenceCount);
			//DEBUG_REPORT_LOG(Audio::isDebugEnabled(), ("Audio: Ref count increased: %d %s\n", iterSampleCache->second.m_referenceCount, iterSampleCache->first->getString()));

			result = iterSampleCache->first;

			// Increase the number of cache hits

			++s_cacheHitCount;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
bool AudioNamespace::isNonBufferedMusic(Audio::SoundCategory const soundCategory)
{
	switch (soundCategory)
	{
	case Audio::SC_backGroundMusic:
	case Audio::SC_combatMusic:
	case Audio::SC_playerMusic:
		return true;
	default:
		return false;
	}
}

//-----------------------------------------------------------------------------
bool isCached(CrcString const &path, SampleCache::iterator &iterSampleCache)
{
	iterSampleCache = s_sampleCache.find(&path);

	return (iterSampleCache != s_sampleCache.end());
}

//-----------------------------------------------------------------------------
void Audio::decreaseReferenceCount(CrcString const &path)
{
	AudioNamespace::decreaseReferenceCount(path);
}

// Creates a SoundId to use to play a sound over and over.
//-----------------------------------------------------------------------------
SoundId Audio::createSoundId(char const *path)
{
	return SoundId(0, path);
}

// Allows a single instance of a sound to be played. If the sound is currently
// playing, it stops it and starts it again.
//-----------------------------------------------------------------------------
void Audio::playSound(SoundId &soundId, Vector const *position, CellProperty const * const parentCell)
{
	if (s_installed)
	{
		// If this sound is currently playing, stop it

		stopSound(soundId, 0.0f);

		// Start the sound back up and re-assign a the new sound id

		soundId = playSound(soundId.getPath().getString(), position, parentCell);
	}
}

//-----------------------------------------------------------------------------
void Audio::restartSound(SoundId &soundId, Vector const *position, float const fadeOutTime)
{
	if (s_installed)
	{
		soundId = restartSound(soundId.getPath().getString(), position, fadeOutTime);
	}
}

//-----------------------------------------------------------------------------
SoundId Audio::restartSound(char const *path, Vector const *position, float const fadeOutTime)
{
	SoundId result;

	if (s_installed)
	{
		// Build a list of all the sounds playing with the same path

		TemporaryCrcString temporaryCrcString(path, true);

		SoundIdToSoundMap::const_iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.begin();

		for (; iterSoundIdToSoundMap != s_soundIdToSoundMap.end(); ++iterSoundIdToSoundMap)
		{
			if (iterSoundIdToSoundMap->first.getPath() == temporaryCrcString)
			{
				s_localPurgeList.push_back(iterSoundIdToSoundMap->first);
			}
		}

		// Remove all the sounds playing with the same path

		for (uint i = 0; i < s_localPurgeList.size(); ++i)
		{
			UNREF(fadeOutTime);
			stopSound(s_localPurgeList[i], fadeOutTime);
		}

		s_localPurgeList.clear();

		// Start a new instance of the sound

		result = playSound(path, position, NULL);
	}

	return result;
}

//-----------------------------------------------------------------------------
SoundId Audio::playSound(char const * const path)
{
	return playSound(path, NULL, NULL);
}

// This always plays the sound as a 2d sound.
//-----------------------------------------------------------------------------
SoundId Audio::playSound(char const *path, CellProperty const * const parentCell)
{
	return playSound(path, NULL, parentCell);
}

// This could play the sound with either 2d or 3d depending on the file type
// specified. Sound Templates could either be 2d, 2d attenuated, or 3d.
//-----------------------------------------------------------------------------
SoundId Audio::playSound(char const *path, Vector const &position, CellProperty const * const parentCell)
{
	return playSound(path, &position, parentCell);
}

//-----------------------------------------------------------------------------
SoundId Audio::playSound(char const *path, Vector const * const position, CellProperty const * const parentCell)
{
	return AudioNamespace::playSound(path, position, NULL, NULL, parentCell);
}

//-----------------------------------------------------------------------------
SoundId playSound2d(SoundTemplate const *soundTemplate, Vector const * const position, CellProperty const * const parentCell)
{
	SoundId result;

	if (s_installed)
	{
		if (soundTemplate->is2d())
		{
			Sound2dTemplate const *sound2dTemplate = static_cast<Sound2dTemplate const *>(soundTemplate);
			NOT_NULL(sound2dTemplate);

			// Create the 2d sound and assign it an id

			result = SoundId(getNextSoundId(), (sound2dTemplate->getName() == 0) ? "" : sound2dTemplate->getName());

			DEBUG_REPORT_LOG(s_debugSoundStartStop, ("Audio play sound 2d: %4d %s\n", result.getId(), soundTemplate->getName()));

			Sound2d *sound2d = new Sound2d(sound2dTemplate, result);

			if (sound2d == NULL)
			{
				result.invalidate();

				DEBUG_REPORT_LOG(true, ("Audio: Sound is not playing because the memory block manager is full. %s\n", soundTemplate->getName()));
			}
			else
			{
				if (position != NULL)
				{
					sound2d->setPosition_w(*position);
				}

				sound2d->setParentCell(parentCell);

				s_soundIdToSoundMap.insert(std::make_pair(result, sound2d));

				if (soundTemplate->getSoundCategory() == Audio::SC_voiceover)
				{
					Audio::fadeAllNonVoiceover();
				}

			}
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
SoundId playSound3d(SoundTemplate const *soundTemplate, Vector const &position, CellProperty const * const parentCell)
{
	SoundId result;

	if (s_installed)
	{
		if (soundTemplate->is3d())
		{
			Sound3dTemplate const *sound3dTemplate = static_cast<Sound3dTemplate const *>(soundTemplate);
			NOT_NULL(sound3dTemplate);

			// Create the 3d sound and assign it an id

			result = SoundId(getNextSoundId(), (soundTemplate->getName() == 0) ? "" : soundTemplate->getName());

			DEBUG_REPORT_LOG(s_debugSoundStartStop, ("Audio play sound 3d: %4d %s\n", result.getId(), soundTemplate->getName()));

			Sound3d *sound3d = new Sound3d(sound3dTemplate, result);

			if (sound3d == NULL)
			{
				result.invalidate();

				DEBUG_REPORT_LOG(true, ("Audio: Sound is not playing because the memory block manager is full. %s\n", soundTemplate->getName()));
			}
			else
			{
				sound3d->setPosition_w(position);
				sound3d->setParentCell(parentCell);

				s_soundIdToSoundMap.insert(std::make_pair(result, sound3d));

				if (soundTemplate->getSoundCategory() == Audio::SC_voiceover)
				{
					Audio::fadeAllNonVoiceover();
				}

			}
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
SoundId attachSound(SoundTemplate const * soundTemplate, Object const * object, char const * hardPointName)
{
	SoundId result;

	if (s_installed &&
		(soundTemplate != NULL) &&
		(object != NULL))
	{
		result = SoundId(getNextSoundId(), (soundTemplate->getName() == 0) ? "" : soundTemplate->getName());

		DEBUG_REPORT_LOG(s_debugSoundStartStop, ("Audio attach sound(%d) %s\n", result.getId(), soundTemplate->getName()));

		if (soundTemplate->is3d())
		{
			Sound3dTemplate const *sound3dTemplate = static_cast<Sound3dTemplate const *>(soundTemplate);
			NOT_NULL(sound3dTemplate);

			// Create the 3d sound and assign it an id

			Sound3d *sound3d = new Sound3d(sound3dTemplate, result);

			if (sound3d == NULL)
			{
				result.invalidate();

				DEBUG_REPORT_LOG(true, ("Audio: Sound is not playing because the memory block manager is full. %s\n", soundTemplate->getName()));
			}
			else
			{
				sound3d->setObject(object);
				sound3d->setHardPointName(hardPointName);

				s_soundIdToSoundMap.insert(std::make_pair(result, sound3d));

				if (soundTemplate->getSoundCategory() == Audio::SC_voiceover)
				{
					Audio::fadeAllNonVoiceover();
				}
			}
		}
		else
		{
			if (soundTemplate->is2d())
			{
				Sound2dTemplate const *sound2dTemplate = static_cast<Sound2dTemplate const *>(soundTemplate);
				NOT_NULL(sound2dTemplate);

				switch (sound2dTemplate->getAttenuationMethod())
				{
					case Audio::AM_2d:
						{
							// Create the 2d attenuated sound and assign it an id

							Sound2d *sound2d = new Sound2d(sound2dTemplate, result);

							if (sound2d == NULL)
							{
								result.invalidate();

								DEBUG_REPORT_LOG(true, ("Audio: Sound is not playing because the memory block manager is full. %s\n", soundTemplate->getName()));
							}
							else
							{
								sound2d->setObject(object);
								sound2d->setHardPointName(hardPointName);

								s_soundIdToSoundMap.insert(std::make_pair(result, sound2d));

								if (soundTemplate->getSoundCategory() == Audio::SC_voiceover)
								{
									Audio::fadeAllNonVoiceover();
								}
							}
						}
						break;
					case Audio::AM_none:
					default:
						{
							DEBUG_WARNING(true, ("Trying to attach a non-3d or non-attenuated 2d sound to an object: %s", soundTemplate->getName()));
						}
						break;
				}
			}
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
// CONSULT-60 audio diagnostic: convict the load-in music-skip / in-game crackle
// class. Edge-logs every stream starvation transition (SAMPLE::starved -- "buffer
// stream has run out of data", set by the Miles mixer when it wants data none of
// the stream's chunk buffers can supply) plus a 500ms summary of each stream's
// status and buffered-chunk count. Config-gated, no cost when off.
static void audioDiagUpdate(float const deltaTime)
{
	if (!s_audioDiagFile)
	{
		fopen_s(&s_audioDiagFile, "audio-diag.log", "at");
		if (!s_audioDiagFile)
			return;
		fprintf(s_audioDiagFile, "---- audio-diag armed (streamBufferBytes=%d) ----\n", s_streamBufferBytes);
		fflush(s_audioDiagFile);
	}

	SYSTEMTIME st;
	GetLocalTime(&st);

	// CONSULT-62 wave 4: edge-log the global ducking state -- the residual shallow
	// load-end music dips are invisible to the per-stream probes, so watch the fade
	// variables and their driving counts directly.
	{
		static float s_lastGlobalFade = 1.0f;
		static float s_lastBgFade = 1.0f;

		if ((s_globalAudioFadeVolume < 0.995f) != (s_lastGlobalFade < 0.995f)
			|| (s_backgroundMusicFadeVolume < 0.995f) != (s_lastBgFade < 0.995f))
		{
			fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d DUCK global=%.3f bg=%.3f nonBg=%d nonVo=%d all=%d\n",
				st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
				s_globalAudioFadeVolume, s_backgroundMusicFadeVolume,
				s_nonBackgroundFadeCount, s_nonVoiceoverFadeCount, s_allAudioFadeCount);
			fflush(s_audioDiagFile);
		}

		s_lastGlobalFade = s_globalAudioFadeVolume;
		s_lastBgFade = s_backgroundMusicFadeVolume;
	}

	// edge-detect starvation + abrupt volume steps per stream, every call (cheap)
	static std::map<HSTREAM, S32> s_lastStarved;
	static std::map<HSTREAM, F32> s_lastVolume;

	// CONSULT-62 wave 2: prune tracking for streams that closed, so a reused
	// HSTREAM pointer registers as a fresh OPEN instead of inheriting stale state.
	{
		for (std::map<HSTREAM, S32>::iterator it = s_lastStarved.begin(); it != s_lastStarved.end();)
		{
			bool live = false;
			SampleIdToSampleStreamMap::iterator j = s_sampleIdToSampleStreamMap.begin();
			for (; j != s_sampleIdToSampleStreamMap.end(); ++j)
			{
				if (j->second.m_stream == it->first)
				{
					live = true;
					break;
				}
			}

			if (!live)
			{
				s_lastVolume.erase(it->first);
				std::map<HSTREAM, S32>::iterator const dead = it++;
				s_lastStarved.erase(dead);
			}
			else
				++it;
		}
	}

	{
		SampleIdToSampleStreamMap::iterator iter = s_sampleIdToSampleStreamMap.begin();
		for (; iter != s_sampleIdToSampleStreamMap.end(); ++iter)
		{
			HSTREAM const stream = iter->second.m_stream;
			if (!stream || !stream->samp)
				continue;

			S32 const starved = stream->samp->starved;
			std::map<HSTREAM, S32>::iterator last = s_lastStarved.find(stream);
			S32 const lastStarved = (last != s_lastStarved.end()) ? last->second : 0;

			// CONSULT-62 wave 2: explicit OPEN edge -- a STARVED line right after its
			// stream's OPEN is the known init-state artifact; a STARVED with no
			// adjacent OPEN on the same pointer is LIVE refill starvation (the
			// promoted zone-in suspect).
			if (last == s_lastStarved.end())
			{
				fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d OPEN %s stream=%p\n",
					st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
					iter->second.getPath() ? iter->second.getPath()->getString() : "(null)",
					static_cast<void *>(stream));
				fflush(s_audioDiagFile);
			}

			if (starved && !lastStarved)
			{
				fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d STARVED %s stream=%p\n",
					st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
					iter->second.getPath() ? iter->second.getPath()->getString() : "(null)",
					static_cast<void *>(stream));
				fflush(s_audioDiagFile);
			}

			s_lastStarved[stream] = starved;

			// CONSULT-61 (Sonnet test #2): AIL_set_sample_volume_levels in Miles 9.3
			// is a bare scalar assignment -- NO ramp -- so a one-frame volume step is
			// an audible click candidate (the .snd volume-wander path snaps when
			// interpolationRate=0). Log steps > 0.05 in one frame to correlate with
			// heard pops. (CONSULT-62 wave 4: threshold lowered to 0.015 -- the
			// residual load-end music dips ride below 0.05/frame.)
			F32 const volume = stream->samp->left_volume;
			std::map<HSTREAM, F32>::iterator lastVol = s_lastVolume.find(stream);
			if (lastVol != s_lastVolume.end())
			{
				F32 const delta = volume - lastVol->second;
				if (delta > 0.015f || delta < -0.015f)
				{
					fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d VOLSTEP %.3f -> %.3f %s\n",
						st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
						lastVol->second, volume,
						iter->second.getPath() ? iter->second.getPath()->getString() : "(null)");
					fflush(s_audioDiagFile);
				}
			}
			s_lastVolume[stream] = volume;
		}
	}

	// periodic summary
	s_audioDiagSummaryTimer += deltaTime;
	if (s_audioDiagSummaryTimer >= 0.5f)
	{
		s_audioDiagSummaryTimer = 0.0f;

		fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d streams=%d playing=%d",
			st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
			static_cast<int>(s_sampleIdToSampleStreamMap.size()),
			static_cast<int>(s_prioritizedPlayingSounds.size()));

		SampleIdToSampleStreamMap::iterator iter = s_sampleIdToSampleStreamMap.begin();
		for (; iter != s_sampleIdToSampleStreamMap.end(); ++iter)
		{
			HSTREAM const stream = iter->second.m_stream;
			if (!stream)
				continue;

			// buffered-chunk estimate: chunks queued in the sample right now
			S32 bufferedChunks = -1;
			S32 starved = -1;
			if (stream->samp)
			{
				bufferedChunks = stream->samp->head - stream->samp->tail;
				if (bufferedChunks < 0)
					bufferedChunks += stream->samp->n_buffers;
				starved = stream->samp->starved;
			}

			char const * const path = iter->second.getPath() ? iter->second.getPath()->getString() : "(null)";
			char const * const shortName = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;

			fprintf(s_audioDiagFile, "  [%s st=%d chunks=%d/%d starved=%d]",
				shortName, AIL_stream_status(stream), bufferedChunks,
				stream->samp ? stream->samp->n_buffers : -1, starved);
		}

		fprintf(s_audioDiagFile, "\n");
		fflush(s_audioDiagFile);
	}
}

// Update all the sounds
//-----------------------------------------------------------------------------
void Audio::alter(float const deltaTime, Object const *listener)
{
	s_listenerObject = listener;

	if (!s_installed)
	{
		return;
	}

	if (s_audioDiagLog)
		audioDiagUpdate(deltaTime);

	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("update listener");

		s_timerCurrentDelay = static_cast<int>(AIL_get_timer_highest_delay());
		s_timerHighestDelay = (s_timerCurrentDelay > s_timerHighestDelay) ? s_timerCurrentDelay : s_timerHighestDelay;
		s_digitalLatency = getDigitalLatency();
		s_digitalCpuPercent = getDigitalCpuPercent();

		s_averageTimerDelay = (s_averageTimerDelay * 0.95f) + (s_timerCurrentDelay * 0.05f);

		DEBUG_REPORT_LOG(s_debugWindow && (s_timerCurrentDelay > s_digitalLatency) && !s_prioritizedPlayingSounds.empty(), ("AUDIO WARNING: timer delay %d ms > premix buffer %d ms\n", s_timerCurrentDelay, s_digitalLatency));
		DEBUG_REPORT_LOG(s_debugTimerDelay && (s_timerCurrentDelay > 50), ("Audio: timer delay (%d)", s_timerCurrentDelay));

		// Update the position of the listener
		{
			if (listener != NULL)
			{
				s_listener.m_positionCurrent = listener->getPosition_w();
				s_listener.m_vectorForward = listener->getObjectFrameK_w();
				s_listener.m_vectorUp = listener->getObjectFrameJ_w();
			}

			s_listener.alter();
		}
	}

	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("queue existing");

		s_utilitySoundList.clear();
		{
			PrioritizedPlayingSounds::iterator iterPrioritizedPlayingSounds = s_prioritizedPlayingSounds.begin();

			for (; iterPrioritizedPlayingSounds != s_prioritizedPlayingSounds.end(); ++iterPrioritizedPlayingSounds)
			{
				Sound2 * sound = *iterPrioritizedPlayingSounds;
				s_utilitySoundList.push_back(sound);
			}
		}

		// Insert all the current sounds into the consilidation buckets, if the sounds are in range

		{
			UtilitySoundList::iterator iterUtilitySoundList = s_utilitySoundList.begin();

			for (; iterUtilitySoundList != s_utilitySoundList.end(); ++iterUtilitySoundList)
			{
				Sound2 * sound = *iterUtilitySoundList;
				bool const soundIsAlreadyPlaying = true;

				if (   (sound->getDistanceSquaredFromListener() > sqr(sound->getDistanceAtVolumeCutOff()))
					|| !queueSample(*sound, soundIsAlreadyPlaying))
				{
					// The sample is not a good choice to play so stop it
					float const fadeOutTime = 0.0f;
					bool const keepAlive = sound->isInfiniteLooping();

					AudioNamespace::stopSound(sound->getSoundId(), fadeOutTime, keepAlive);
				}
			}
		}
		s_utilitySoundList.clear();
	}

	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("update existing");

		// Update the sounds, samples get queued up in the alter calls if a sound is ready to play

		SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.begin();

		for (; iterSoundIdToSoundMap != s_soundIdToSoundMap.end(); ++iterSoundIdToSoundMap)
		{
			Sound2 * const sound = iterSoundIdToSoundMap->second;

			if (sound != NULL)
			{
				sound->alter(deltaTime);
			}
		}
	}

	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("build sample list");

		// Build the queued sample list from the consolidation buckets

		s_queuedSamplesToStartList.clear();

		// Center
		QueuedSamplesToStartList::iterator iterCenterBucket = s_centerBucket.begin();

		for (; iterCenterBucket != s_centerBucket.end(); ++iterCenterBucket)
		{
			insertionSort(s_queuedSamplesToStartList, **iterCenterBucket);
		}

		// Front
		SoundBucketList::iterator iterFrontBucket = s_frontBucket.begin();

		for (; iterFrontBucket != s_frontBucket.end(); ++iterFrontBucket)
		{
			if (!iterFrontBucket->second.m_soundIsAlreadyPlaying)
			{
				insertionSort(s_queuedSamplesToStartList, *(iterFrontBucket->second.m_sound));
			}
		}

		// Back
		SoundBucketList::iterator iterBackBucket = s_backBucket.begin();

		for (; iterBackBucket != s_backBucket.end(); ++iterBackBucket)
		{
			if (!iterBackBucket->second.m_soundIsAlreadyPlaying)
			{
				insertionSort(s_queuedSamplesToStartList, *(iterBackBucket->second.m_sound));
			}
		}

		// Left
		SoundBucketList::iterator iterLeftBucket = s_leftBucket.begin();

		for (; iterLeftBucket != s_leftBucket.end(); ++iterLeftBucket)
		{
			if (!iterLeftBucket->second.m_soundIsAlreadyPlaying)
			{
				insertionSort(s_queuedSamplesToStartList, *(iterLeftBucket->second.m_sound));
			}
		}

		// Right
		SoundBucketList::iterator iterRightBucket = s_rightBucket.begin();

		for (; iterRightBucket != s_rightBucket.end(); ++iterRightBucket)
		{
			if (!iterRightBucket->second.m_soundIsAlreadyPlaying)
			{
				insertionSort(s_queuedSamplesToStartList, *(iterRightBucket->second.m_sound));
			}
		}

		// Clear the consolidation buckets

		s_centerBucket.clear();
		s_leftBucket.clear();
		s_rightBucket.clear();
		s_frontBucket.clear();
		s_backBucket.clear();
	}

	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("stream EOS poll");

		// CONSULT-62 wave 4: drive stream end-of-sample from POLLED status instead of
		// trusting Miles' stream callback (which demonstrably does not fire for these
		// MP3 music streams -- finished streams sat done-but-unreleased for 30-90s,
		// and new sounds triggered against that zombie window crackle; Kenny's
		// three-run desert/door experiment 2026-07-04). Sound2d::reset() then releases
		// the stream within a frame, exactly as the callback path would have.

		SampleIdToSampleStreamMap::iterator iterStreamEosPoll = s_sampleIdToSampleStreamMap.begin();

		for (; iterStreamEosPoll != s_sampleIdToSampleStreamMap.end(); ++iterStreamEosPoll)
		{
			if (iterStreamEosPoll->second.m_status != Audio::PS_done
				&& iterStreamEosPoll->second.m_stream != NULL
				&& AIL_stream_status(iterStreamEosPoll->second.m_stream) == SMP_DONE)
			{
				// Opened-but-never-started streams can also read SMP_DONE -- only a
				// stream that actually advanced counts as finished.
				S32 pollTotalMs = 0;
				S32 pollCurrentMs = 0;
				AIL_stream_ms_position(iterStreamEosPoll->second.m_stream, &pollTotalMs, &pollCurrentMs);
				if (pollCurrentMs <= 0)
					continue;

				iterStreamEosPoll->second.m_status = Audio::PS_done;

				Sound2 * const sound = iterStreamEosPoll->second.m_sound;
				NOT_NULL(sound);

				if (s_audioDiagFile)
				{
					SYSTEMTIME st;
					GetLocalTime(&st);
					fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d EOSPOLL %s stream=%p\n",
						st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
						iterStreamEosPoll->second.getPath() ? iterStreamEosPoll->second.getPath()->getString() : "(null)",
						static_cast<void *>(iterStreamEosPoll->second.m_stream));
					fflush(s_audioDiagFile);
				}

				sound->endOfSample();
			}
		}
	}

	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("delete old sounds");

		// Delete any old sounds, if an object dies that had a sound attachment, the sound will die here

		SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.begin();

		for (; iterSoundIdToSoundMap != s_soundIdToSoundMap.end(); ++iterSoundIdToSoundMap)
		{
			SoundId const &soundId = iterSoundIdToSoundMap->first;
			Sound2 * const sound = iterSoundIdToSoundMap->second;

			if (((sound != NULL) &&
				  sound->isDeletable()) ||
				(sound == NULL))
			{
				s_localPurgeList.push_back(soundId);
			}
		}

		SoundIdList::const_iterator iterLocalPurgeList = s_localPurgeList.begin();

		for (; iterLocalPurgeList != s_localPurgeList.end(); ++iterLocalPurgeList)
		{
			SoundId const & soundId = (*iterLocalPurgeList);

			stopSound(soundId);
		}

		s_localPurgeList.clear();
	}

	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("replace sounds");

		// Check and see if the playing sounds need to be replaced by any new sounds that are queued to play

		if (static_cast<int>(s_prioritizedPlayingSounds.size() + s_queuedSamplesToStartList.size()) > getMaxNumberOfSamples())
		{
			// Populate the swap vector

			s_utilitySoundList.clear();

			PrioritizedPlayingSounds::iterator iterPrioritizedPlayingSounds = s_prioritizedPlayingSounds.begin();

			//DEBUG_REPORT_LOG(s_debugWindow, ("Pre Sort\n"));
			for (; iterPrioritizedPlayingSounds != s_prioritizedPlayingSounds.end(); ++iterPrioritizedPlayingSounds)
			{
				s_utilitySoundList.push_back(*iterPrioritizedPlayingSounds);

				//DEBUG_REPORT_LOG(s_debugWindow, ("Priority: %d Distance: %.2f\n", (*iterPrioritizedPlayingSounds)->getTemplate()->getPriority(), (*iterPrioritizedPlayingSounds)->getDistanceSquaredFromListener()));
			}

			// See if the prioritized sounds are already sorted

			bool sorted = true;
			{
				for (unsigned int i = 1; i < s_utilitySoundList.size(); ++i)
				{
					int const currentPriority = s_utilitySoundList[i]->getTemplate()->getPriority();
					float const currentDistanceSquaredFromListener = s_utilitySoundList[i]->getDistanceSquaredFromListener();

					int const previousPriority = s_utilitySoundList[i - 1]->getTemplate()->getPriority();
					float const previousDistanceSquaredFromListener = s_utilitySoundList[i - 1]->getDistanceSquaredFromListener();

					if ((currentPriority < previousPriority) ||
						((currentPriority == previousPriority) &&
						 (currentDistanceSquaredFromListener < previousDistanceSquaredFromListener)))
					{
						sorted = false;
						break;
					}
				}
			}

			if (!sorted)
			{
				//DEBUG_REPORT_LOG(s_debugWindow, ("Sounds are not sorted, sorting.\n"));

				// Re-sort the prioritized playing sounds since they are not in the correct order due to the movement
				// of the listener

				for (unsigned int i = 0; i < s_utilitySoundList.size(); ++i)
				{
					for (unsigned int j = i + 1; j < s_utilitySoundList.size(); ++j)
					{
						int const currentPriority = s_utilitySoundList[j]->getTemplate()->getPriority();
						float const currentDistanceSquaredFromListener = s_utilitySoundList[j]->getDistanceSquaredFromListener();

						int const previousPriority = s_utilitySoundList[i]->getTemplate()->getPriority();
						float const previousDistanceSquaredFromListener = s_utilitySoundList[i]->getDistanceSquaredFromListener();

						if ((currentPriority < previousPriority) ||
							((currentPriority == previousPriority) &&
							 (currentDistanceSquaredFromListener < previousDistanceSquaredFromListener)))
						{
							//DEBUG_REPORT_LOG(s_debugWindow, ("Swapping sound due to higher priority/distance: previous: %d %.2f current %d %.2f\n", previousPriority, previousDistanceSquaredFromListener, currentPriority, currentDistanceSquaredFromListener));

							// Swap current and previous

							Sound2 *temp = s_utilitySoundList[j];
							s_utilitySoundList[j] = s_utilitySoundList[i];
							s_utilitySoundList[i] = temp;
						}
					}
				}
			}
			else
			{
				//DEBUG_REPORT_LOG(s_debugWindow, ("Sounds are already sorted.\n"));
			}

			// Shove the re-sorted vector back into the list

			s_prioritizedPlayingSounds.clear();

			UtilitySoundList::iterator iterSwapVector = s_utilitySoundList.begin();

			//DEBUG_REPORT_LOG(s_debugWindow, ("Post Sort\n"));
			for (; iterSwapVector != s_utilitySoundList.end(); ++iterSwapVector)
			{
				NOT_NULL(*iterSwapVector);
				s_prioritizedPlayingSounds.push_back(*iterSwapVector);

				//DEBUG_REPORT_LOG(s_debugWindow, ("Priority: %d Distance: %.2f\n", (*iterSwapVector)->getTemplate()->getPriority(), (*iterSwapVector)->getDistanceSquaredFromListener()));
			}

			// Trim the lists

			for (;;)
			{
				if (static_cast<int>(s_prioritizedPlayingSounds.size() + s_queuedSamplesToStartList.size()) <= getMaxNumberOfSamples())
				{
					// Done trimming

					break;
				}

				// If the user switches the max number of sounds on the fly, we may need to trim the playing sounds

				if (static_cast<int>(s_prioritizedPlayingSounds.size()) > getMaxNumberOfSamples())
				{
					Sound2 * const sound = s_prioritizedPlayingSounds.back();
					bool const keepAlive = sound->isInfiniteLooping();
					AudioNamespace::stopSound(sound->getSoundId(), 0.0f, keepAlive);
				}
				else
				{
					DEBUG_FATAL(s_queuedSamplesToStartList.empty(), ("Empty queued sample list"));

					// Too many sounds to play, remove some more

					if ((s_prioritizedPlayingSounds.size() > 0) &&
						(s_queuedSamplesToStartList.size() > 0))
					{
						// Remove a sound from the list which has the lowest priority/distance

						int const queuedPriority = s_queuedSamplesToStartList.back()->getTemplate()->getPriority();
						float const queuedDistanceSquaredFromListener = s_queuedSamplesToStartList.back()->getDistanceSquaredFromListener();

						if (s_prioritizedPlayingSounds.back()->getTemplate()->getPriority() == queuedPriority)
						{
							if (s_prioritizedPlayingSounds.back()->getDistanceSquaredFromListener() > queuedDistanceSquaredFromListener)
							{
								Sound2 * const sound = s_prioritizedPlayingSounds.back();
								bool const keepAlive = sound->isInfiniteLooping();
								AudioNamespace::stopSound(sound->getSoundId(), 0.0f, keepAlive);
							}
							else
							{
								Sound2 * const sound = s_queuedSamplesToStartList.back();
								float const fadeOutTime = 0.0f;
								bool const keepAlive = sound->isInfiniteLooping();

								AudioNamespace::stopSound(sound->getSoundId(), fadeOutTime, keepAlive);

								s_queuedSamplesToStartList.pop_back();
							}
						}
						else if (s_prioritizedPlayingSounds.back()->getTemplate()->getPriority() > queuedPriority)
						{
							// The playing sound has a worse priority then the queued sound, so stop the playing sound

							Sound2 * const sound = s_prioritizedPlayingSounds.back();
							bool const keepAlive = sound->isInfiniteLooping();
							AudioNamespace::stopSound(s_prioritizedPlayingSounds.back()->getSoundId(), 0.0f, keepAlive);
						}
						else
						{
							// The queued sound has a worse priority then the playing sound, so stop the queued sound

							Sound2 * const sound = s_queuedSamplesToStartList.back();
							float const fadeOutTime = 0.0f;
							bool const keepAlive = sound->isInfiniteLooping();

							AudioNamespace::stopSound(sound->getSoundId(), fadeOutTime, keepAlive);

							s_queuedSamplesToStartList.pop_back();
						}
					}
					else if (s_queuedSamplesToStartList.size() > 0)
					{
						Sound2 * const sound = s_queuedSamplesToStartList.back();
						float const fadeOutTime = 0.0f;
						bool const keepAlive = sound->isInfiniteLooping();

						AudioNamespace::stopSound(sound->getSoundId(), fadeOutTime, keepAlive);

						s_queuedSamplesToStartList.pop_back();
					}
					else if (s_prioritizedPlayingSounds.size() > 0)
					{
						DEBUG_FATAL(true, ("Somehow there is more prioritized sounds playing than the maximum number of sounds."));
					}
					else
					{
						DEBUG_FATAL(true, ("Somehow the maximum number of supported sounds is <= 0."));
					}
				}
			}
		}
	}

	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("start sounds");

		// Start the new queued sounds that did not get thrown out

		// CONSULT-62 postscript: leave this as ONE batch-wide lock. Per-sound lock
		// cycling (tried 2026-07-04) made the main thread wait behind a full mixer
		// pass (incl. MP3 decode) between every start -- audible sound-start lag
		// (footsteps/UI clicks) and worse D3D9 frame hitches. Verified regression.
		Audio::lock();

		QueuedSamplesToStartList::const_iterator iterQueuedSoundsToStartdList = s_queuedSamplesToStartList.begin();

		for (; iterQueuedSoundsToStartdList != s_queuedSamplesToStartList.end(); ++iterQueuedSoundsToStartdList)
		{
			Sound2 * const sound = NON_NULL(*iterQueuedSoundsToStartdList);

			// Make sure the sound is still valid because it could have died between the time it was
			// inserted in the queue and the time it should start playing

			if (isSoundValid(sound->getSoundId()))
			{
				startSample(*sound);
			}
		}

		Audio::unLock();

		s_queuedSamplesToStartList.clear();

		DEBUG_FATAL(static_cast<int>(s_prioritizedPlayingSounds.size()) > getMaxNumberOfSamples(), ("Invalid number of sounds: %d max: %d", static_cast<int>(s_prioritizedPlayingSounds.size()), getMaxNumberOfSamples()));
	}

	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("debug visuals");
#ifdef _DEBUG
		// Add the debugging text

		SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.begin();

		for (; iterSoundIdToSoundMap != s_soundIdToSoundMap.end(); ++iterSoundIdToSoundMap)
		{
			if (isDebugEnabled())
			{
				// Set the debug text

				char text[256];
				char const *soundTemplateName = iterSoundIdToSoundMap->second->getTemplate()->getName();
				char const *removeSoundSlash = strstr(soundTemplateName, "sound/");
				if (removeSoundSlash != NULL)
				{
					soundTemplateName = removeSoundSlash + strlen("sound/");
				}
				int const totalSampleSize = iterSoundIdToSoundMap->second->getTotalSampleSize();
				int const templateVolume = static_cast<int>(iterSoundIdToSoundMap->second->getTemplateVolume() * 100.0f);
				int const templatePitch = static_cast<int>(iterSoundIdToSoundMap->second->getPitchDelta() * 100.0f);
				float sampleTimeTotal = 0.0f;
				float sampleTimeCurrent = 0.0f;
				getCurrentSampleTime(iterSoundIdToSoundMap->first, sampleTimeTotal, sampleTimeCurrent);
				int const volumePercent = static_cast<int>(iterSoundIdToSoundMap->second->getAttenuation() * 100.0f);
				float const distanceFromListener = iterSoundIdToSoundMap->second->getDistanceFromListener();
				bool const occluded = iterSoundIdToSoundMap->second->isOccluded();
				bool const obstructed = iterSoundIdToSoundMap->second->isObstructed();
				float const distanceAtMaxVolume = iterSoundIdToSoundMap->second->getTemplate()->getDistanceAtMaxVolume();
				float const distanceAtVolumeCutOff = iterSoundIdToSoundMap->second->getDistanceAtVolumeCutOff();
				bool const outOfRange = (distanceFromListener >= distanceAtVolumeCutOff);
				TemporaryCrcString currentSample;
				getCurrentSample(iterSoundIdToSoundMap->first, currentSample);
				bool const streamed = iterSoundIdToSoundMap->second->isStreamed();
				AttenuationMethod attenuationMethod = iterSoundIdToSoundMap->second->getTemplate()->getAttenuationMethod();

				snprintf(text, sizeof(text), "%s %d %s\nv %d%% tv %d tp %d dis (%.1f, %.1f, %.1f)\n%s %s %s%s\n%s (%.2f/%.2f)", soundTemplateName, ((totalSampleSize < 1024) ? totalSampleSize : (totalSampleSize / 1024)), ((totalSampleSize < 1024) ? "bytes" : "KB"), volumePercent, templateVolume, templatePitch, distanceAtMaxVolume, distanceFromListener, distanceAtVolumeCutOff, getSoundCategoryString(iterSoundIdToSoundMap->second->getTemplate()->getSoundCategory()), AudioNamespace::getAttenuationMethodString(attenuationMethod), streamed ? "Streamed" : "Cached", outOfRange ? " OUT OF RANGE" : (occluded ? " OCC" : (obstructed ? " OBS" : "")), (strlen(currentSample.getString()) <= 0) ? "delayed" : currentSample.getString(), sampleTimeCurrent, sampleTimeTotal);
				iterSoundIdToSoundMap->second->setText(text, outOfRange ? VectorArgb::solidRed : ((occluded || obstructed) ? VectorArgb::solidCyan : VectorArgb::solidGreen));
			}
		}
#endif // _DEBUG
	}

	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("global volume fade");

		float fadeTarget = 1.0f;

		if (s_nonVoiceoverFadeCount || s_nonBackgroundFadeCount)
		{
			static float const nonVoiceoverFadeTarget = 0.5f;
			static float const nonBackgroundFadeTarget = 0.0f;

			if ( s_nonBackgroundFadeCount )
				fadeTarget = fadeTarget < nonBackgroundFadeTarget ? fadeTarget : nonBackgroundFadeTarget;

			if ( s_nonVoiceoverFadeCount )
				fadeTarget = fadeTarget < nonVoiceoverFadeTarget ? fadeTarget : nonVoiceoverFadeTarget;
		}

		if(s_allAudioFadeCount)
			fadeTarget = fadeTarget < s_allAudioFadeFactor ? fadeTarget : s_allAudioFadeFactor;

		if (s_globalAudioFadeVolume < fadeTarget)
		{
			static float const fadeInRate = 1.5f;   // one over fade time

			s_globalAudioFadeVolume = clamp(0.0f, s_globalAudioFadeVolume + (deltaTime * fadeInRate), fadeTarget);
		}
		else if (s_globalAudioFadeVolume > fadeTarget)
		{
			static float const fadeOutRate = 10.0f;	// one over fade time

			s_globalAudioFadeVolume = clamp(fadeTarget, s_globalAudioFadeVolume - (deltaTime * fadeOutRate), 1.0f);
		}

		// CONSULT-62 wave 3b: background music's own smoothed fade. Same targets as
		// the branch background music is ELIGIBLE for (global fade applies to bg
		// music only when no non-background duck is active, or during an all-audio
		// fade), but ramped continuously so a count-gate flip can't step the volume
		// to a stale ducked value (the deterministic load-end music dip).
		{
			float bgFadeTarget = 1.0f;

			if (s_nonBackgroundFadeCount == 0 || s_allAudioFadeCount != 0)
				bgFadeTarget = fadeTarget;

			if (s_backgroundMusicFadeVolume < bgFadeTarget)
			{
				static float const fadeInRate = 1.5f;   // one over fade time

				s_backgroundMusicFadeVolume = clamp(0.0f, s_backgroundMusicFadeVolume + (deltaTime * fadeInRate), bgFadeTarget);
			}
			else if (s_backgroundMusicFadeVolume > bgFadeTarget)
			{
				static float const fadeOutRate = 10.0f;	// one over fade time

				s_backgroundMusicFadeVolume = clamp(bgFadeTarget, s_backgroundMusicFadeVolume - (deltaTime * fadeOutRate), 1.0f);
			}
		}

	}

	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("non-buffered music volume fade");

		float fadeTarget = 1.0f;

		if (s_silenceNonBufferedMusic)
			fadeTarget = 0.0f;

		if (s_nonBuffereMusicFadeVolume < fadeTarget)
		{
			float const fadeInRate = 1.5f;   // one over fade time

			s_nonBuffereMusicFadeVolume = clamp(0.0f, s_nonBuffereMusicFadeVolume + (deltaTime * fadeInRate), fadeTarget);
		}
		else if (s_nonBuffereMusicFadeVolume > fadeTarget)
		{
			float const fadeOutRate = 10.0f;	// one over fade time

			s_nonBuffereMusicFadeVolume = clamp(fadeTarget, s_nonBuffereMusicFadeVolume - (deltaTime * fadeOutRate), 1.0f);
		}

	}

	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("Miles AIL_serve()");

		serve();
	}

}

//-----------------------------------------------------------------------------
void Audio::setSamplePosition_w(SampleId const &sampleId, Vector const &position_w)
{
	SampleIdToSample3dMap::const_iterator iterSampleIdToSample3dMap = s_sampleIdToSample3dMap.find(sampleId);

	if (iterSampleIdToSample3dMap != s_sampleIdToSample3dMap.end())
	{
		AIL_set_sample_3D_position(iterSampleIdToSample3dMap->second.m_sample, position_w.x, position_w.y, position_w.z);
	}
}

//-----------------------------------------------------------------------------
std::string Audio::getMilesVersion()
{
	char version[256] = { 0 };   // zero-init so a no-fill AIL_MSS_version returns an empty string, not uninitialized memory.
	AIL_MSS_version(version, sizeof(version));

	return version;
}

//-----------------------------------------------------------------------------
S32 getFrequency()
{
	// Should this value be user definable?

	return 22050;
}

//-----------------------------------------------------------------------------
S32 getBits()
{
	// Should this value be user definable?

	return 16;
}

//-----------------------------------------------------------------------------
S32 getChannels()
{
	// Should this value be user definable?

	return 2;
}

//-----------------------------------------------------------------------------
int Audio::getCurrentCacheSize()
{
	return s_currentCacheSize;
}

//-----------------------------------------------------------------------------
int Audio::getCacheHitCount()
{
	return s_cacheHitCount;
}

//-----------------------------------------------------------------------------
int Audio::getCacheMissCount()
{
	return s_cacheMissCount;
}

//-----------------------------------------------------------------------------
std::vector<std::string> Audio::get3dProviders()
{
	std::vector<std::string> providers;
	ProviderMap::const_iterator iter3dProviderMap = s_3dProviderMap.begin();

	for (; iter3dProviderMap != s_3dProviderMap.end(); ++iter3dProviderMap)
	{
		if (iter3dProviderMap->second.m_supported)
		{
			providers.push_back(iter3dProviderMap->second.m_name);
		}
	}

	return providers;
}

//-----------------------------------------------------------------------------
std::string Audio::getSampleType(void *fileImage, int fileSize)
{
	std::string text("");

	if (s_installed)
	{
		// Get the file size

		if (fileImage && fileSize > 0)
		{
			// Get the file type

			S32 fileType = AIL_file_type(fileImage, fileSize);

			// Set the file type string

			switch (fileType)
			{
				case AILFILETYPE_PCM_WAV:
					{
						text = "Standard PCM wav file";
					}
					break;
				case AILFILETYPE_ADPCM_WAV:
					{
						text = "IMA ADPCM compressed wave file";
					}
					break;
				case AILFILETYPE_OTHER_WAV:
					{
						text = "Unsupported compressed wave";
					}
					break;
				case AILFILETYPE_OTHER_ASI_WAV:
					{
						text = "Compressed wave file that is handled by an unspecified ASI decoder";
					}
					break;
				case AILFILETYPE_VOC:
					{
						text = "Creative VOC digital sound file";
					}
					break;
				case AILFILETYPE_MIDI:
					{
						text = "Standard MIDI file";
					}
					break;
				case AILFILETYPE_XMIDI:
					{
						text = "XMIDI file";
					}
					break;
				case AILFILETYPE_XMIDI_DLS:
					{
						text = "XMIDI file containing embedded, umcompressed DLS data";
					}
					break;
				case AILFILETYPE_XMIDI_MLS:
					{
						text = "XMIDI file containing embedded, compressed DLS data";
					}
					break;
				case AILFILETYPE_DLS:
					{
						text = "Uncompressed DLS file";
					}
					break;
				case AILFILETYPE_MLS:
					{
						text = "Compressed DLS file";
					}
					break;
				case AILFILETYPE_MPEG_L1_AUDIO:
					{
						text = "Compressed MPEG Layer 1 file (which MSS cannot play)";
					}
					break;
				case AILFILETYPE_MPEG_L2_AUDIO:
					{
						text = "Compressed MPEG Layer 2 file (which MSS cannot play)";
					}
					break;
				case AILFILETYPE_MPEG_L3_AUDIO:
					{
						text = "Compressed MPEG Layer 3 file";
					}
					break;
				default:
					{
						DEBUG_FATAL(true, ("Audio::getSampleType() - Unsupported file type."));
					}
				case AILFILETYPE_UNKNOWN:
					{
						text = "The file is not one of the supported types";
					}
					break;
			}
		}
		else
		{
			text = "File does not exist on disk";
		}
	}

	return text;
}

//-----------------------------------------------------------------------------
int Audio::getSampleSize(char const *path)
{
	int const result = TreeFile::getFileSize(path);

	// If you hit this error, you should probably fix the code that is calling this

	DEBUG_WARNING(!DataLint::isEnabled() && (result == -1), ("Could not get sample size for %s", path));

	return result;
}

// Stops a sound in the requested time. The sound will then be removed from the
// sound system which invalidates the sound id.
//-----------------------------------------------------------------------------
void Audio::stopSound(SoundId const &soundId, float const fadeOutTime)
{
	AudioNamespace::stopSound(soundId, fadeOutTime, false);
}

// Get the status of the sound. If a sound id is no longer valid, PS_done is returned.
//-----------------------------------------------------------------------------
Audio::PlayBackStatus Audio::getSoundPlayBackStatus(SoundId const &soundId)
{
	PlayBackStatus result = PS_doesNotExist;

	if (s_installed)
	{
		SoundIdToSoundMap::iterator soundIdToSoundMap = s_soundIdToSoundMap.find(soundId);

		if (soundIdToSoundMap != s_soundIdToSoundMap.end())
		{
			NOT_NULL(soundIdToSoundMap->second);

			if (soundIdToSoundMap->second->isPlaying())
			{
				result = PS_playing;
			}
			else
			{
				result = PS_done;
			}
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
Audio::PlayBackStatus Audio::getSamplePlayBackStatus(SampleId const &sampleId)
{
	PlayBackStatus result = PS_done;

	if (s_installed)
	{
		SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
		SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
		SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

		U32 status = PS_done;
		PlayBackStatus samplePlayBackStatus = PS_done;

		if (isSample2d(sampleId, iterSampleIdToSample2dMap))
		{
			status = AIL_sample_status(iterSampleIdToSample2dMap->second.m_sample);
			samplePlayBackStatus = iterSampleIdToSample2dMap->second.m_status;
		}
		else if (isSample3d(sampleId, iterSampleIdToSample3dMap))
		{
			status = AIL_sample_status(iterSampleIdToSample3dMap->second.m_sample);
			samplePlayBackStatus = iterSampleIdToSample3dMap->second.m_status;
		}
		else if (isSampleStream(sampleId, iterSampleIdToSampleStreamMap))
		{
			status = AIL_stream_status(iterSampleIdToSampleStreamMap->second.m_stream);
			samplePlayBackStatus = iterSampleIdToSampleStreamMap->second.m_status;
		}

		switch (status)
		{
			case SMP_DONE:
				{
					result = samplePlayBackStatus;
				}
				break;
			case SMP_PLAYING:
				{
					result = PS_playing;
				}
				break;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
bool Audio::isSampleValid(SampleId const &sampleId)
{
	bool result = false;

	if (s_installed)
	{
		SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
		SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
		SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

		if (isSample2d(sampleId, iterSampleIdToSample2dMap))
		{
			result = true;
		}
		else if (isSample3d(sampleId, iterSampleIdToSample3dMap))
		{
			result = true;
		}
		else if (isSampleStream(sampleId, iterSampleIdToSampleStreamMap))
		{
			result = true;
		}
	}

	return result;
}

//----------------------------------------------------------------------

bool Audio::isSampleForSoundIdPlaying (const SoundId & soundId)
{
	const Sound2 * const sound = getSoundById (soundId);
	if (sound)
		return isSampleValid (sound->getSampleId ());

	return false;
}

//-----------------------------------------------------------------------------
bool Audio::isSoundValid(SoundId const &soundId)
{
	bool result = false;

	if (s_installed)
	{
		SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.find(soundId);

		if (iterSoundIdToSoundMap != s_soundIdToSoundMap.end())
		{
			result = true;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
void Audio::startSample(Sound2 &sound)
{
	NP_PROFILER_AUTO_BLOCK_DEFINE("Audio::startSample");
	bool result = true;

	SampleId const sampleId(createSampleId(sound));

	if (sampleId.getId() == 0)
	{
		DEBUG_WARNING(true, ("Unable to create a valid sample id. All the samples must be occupied."));

		float const fadeOutTime = 0.0f;
		bool const keepAlive = sound.isInfiniteLooping();

		AudioNamespace::stopSound(sound.getSoundId(), fadeOutTime, keepAlive);
		return;
	}

	SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
	SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
	SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

	if (isSample2d(sampleId, iterSampleIdToSample2dMap))
	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("Audio::startSample::isSample2d");
		NOT_NULL(iterSampleIdToSample2dMap->second.m_sample);

		// Get the sample from the cache

		SampleCache::iterator iterSample = s_sampleCache.find(iterSampleIdToSample2dMap->second.getPath());

		if (iterSample != s_sampleCache.end())
		{
			char const *extension = iterSample->second.getExtension();
			void *sampleRawData = iterSample->second.m_sampleRawData;
			NOT_NULL(sampleRawData);
			U32 fileSize = iterSample->second.m_fileSize;
			DEBUG_FATAL((fileSize == 0), ("File size is 0 %s", iterSampleIdToSample2dMap->second.getPath()));
			HSAMPLE sample2d = iterSampleIdToSample2dMap->second.m_sample;

			S32 const setNamedResult = AIL_set_named_sample_file(sample2d, extension, sampleRawData, fileSize, 0);
			if (!setNamedResult && s_audioDiagFile)
			{
				SYSTEMTIME st;
				GetLocalTime(&st);
				fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d SETNAMEDFAIL %s ext=%s size=%u lastError=%s\n",
					st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
					iterSampleIdToSample2dMap->second.getPath()->getString(), extension, fileSize, AIL_last_error());
				fflush(s_audioDiagFile);
			}
			if (setNamedResult)
			{
				setSampleVolume(iterSampleIdToSample2dMap->first, sound.getVolume());

				sound.setPlayBackRate(getSamplePlayBackRate(iterSampleIdToSample2dMap->first));

				setSamplePlayBackRate(iterSampleIdToSample2dMap->first, sound.getPlayBackRate(), sound.getPlayBackRateDelta());

				// Set the loop points

				int loopStartOffset;
				int loopEndOffset;

				if (Audio::getLoopOffsets(*iterSampleIdToSample2dMap->second.getPath(), loopStartOffset, loopEndOffset))
				{
					AIL_set_sample_loop_block(sample2d, static_cast<S32>(loopStartOffset), static_cast<S32>(loopEndOffset));
				}

				// Does this sample loop forever? Telling Miles lets us not have a hitch at the loop point.

				int const loopCount = iterSampleIdToSample2dMap->second.m_sound->getLoopCount();
				bool const infiniteLooping = iterSampleIdToSample2dMap->second.m_sound->getTemplate()->isInfiniteLooping();
				bool const noDelay = (iterSampleIdToSample2dMap->second.m_sound->getTemplate()->getLoopDelayMax() <= 0.0f);
				int const sampleCount = iterSampleIdToSample2dMap->second.m_sound->getTemplate()->getSampleCount();

				if (infiniteLooping && noDelay && (sampleCount <= 1))
				{
					// Specify infinite looping

					AIL_set_sample_loop_count(sample2d, 0);
				}
				else
				{
					if ((loopCount >= 1) && noDelay && (sampleCount <= 1))
					{
						// Specify the loop count

						AIL_set_sample_loop_count(sample2d, loopCount);
					}

					// Register the end of sample callback

					AIL_register_EOS_callback(sample2d, &endOfSample2dCallBack);
				}

				// Play the sample

				AIL_start_sample(sample2d);

				sound.setSampleId(sampleId);

				addPlayingSound(sound);

//#ifdef _DEBUG
//				float leftLevel;
//				float rightLevel;
//				AIL_sample_volume_levels(iterSampleIdToSample2dMap->second.m_sample, &leftLevel, &rightLevel);
//
//				DEBUG_REPORT_LOG(true, ("AIL_start_sample() <id> %s <left vol> %.2f <right vol> %.2f\n", iterSample->first->getString(), leftLevel, rightLevel));
//#endif // _DEBUG
			}
			else
			{
				DEBUG_WARNING(true, ("Error preparing 2D sample for playback: %s (%s)\n", iterSampleIdToSample2dMap->second.getPath()->getString(), AIL_last_error()));
				DEBUG_WARNING(true, ("  Miles 2d count: %d\n", (s_digitalDevice2d != NULL) ? AIL_active_sample_count(s_digitalDevice2d) : 0));

				result = false;
			}
		}
		else
		{
			DEBUG_WARNING(true, ("Unable to find the sample in the sound cache: %s\n", iterSampleIdToSample2dMap->second.getPath()->getString()));
			result = false;
		}
	}
	else if (isSample3d(sampleId, iterSampleIdToSample3dMap))
	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("Audio::startSample::isSample3d");
		NOT_NULL(iterSampleIdToSample3dMap->second.m_sample);

		// Get the sample from the cache

		SampleCache::iterator iterSample = s_sampleCache.find(iterSampleIdToSample3dMap->second.getPath());

		if (iterSample != s_sampleCache.end())
		{
			void *sampleRawData = iterSample->second.m_sampleRawData;
			HSAMPLE hSample3d = iterSampleIdToSample3dMap->second.m_sample;

			// 35-05: load the 3D sample with the NAMED call (format suffix + length), matching the 2D
			// path (~3071) and RAD's own examples (examms.cpp:1320, demo.c:708 — both use the named
			// call even for the 3D sample). The old extensionless AIL_set_sample_file relied on Miles
			// auto-detecting the format from the header; 9.3b's stricter detection failed for some 3D
			// assets -> resultSet3dSampleFile==0 -> the sound was silently dropped (spotty door /
			// footsteps / glitchy ship). Giving Miles the suffix + size removes the guesswork.
			char const *extension = iterSample->second.getExtension();
			U32 const fileSize = iterSample->second.m_fileSize;

			S32 resultSet3dSampleFile = AIL_set_named_sample_file(hSample3d, extension, sampleRawData, fileSize, 0);

			if (resultSet3dSampleFile != 0)
			{
				setSampleVolume(iterSampleIdToSample3dMap->first, sound.getVolume());
				setSampleOcclusion(iterSampleIdToSample3dMap->first, sound.getOcclusion());
				setSampleObstruction(iterSampleIdToSample3dMap->first, sound.getObstruction());

				sound.setPlayBackRate(getSamplePlayBackRate(iterSampleIdToSample3dMap->first));
				setSamplePlayBackRate(iterSampleIdToSample3dMap->first, sound.getPlayBackRate(), sound.getPlayBackRateDelta());

				// Does this sample loop forever? Telling Miles lets us not have a hitch at the loop point.

				int const loopCount = iterSampleIdToSample3dMap->second.m_sound->getLoopCount();
				bool const infiniteLooping = iterSampleIdToSample3dMap->second.m_sound->isInfiniteLooping();
				bool const noDelay = (iterSampleIdToSample3dMap->second.m_sound->getTemplate()->getLoopDelayMax() <= 0.0f);
				int const sampleCount = iterSampleIdToSample3dMap->second.m_sound->getTemplate()->getSampleCount();

				if (infiniteLooping && noDelay && (sampleCount <= 1))
				{
					// Specify infinite looping

					AIL_set_sample_loop_count(hSample3d, 0);
				}
				else
				{
					if ((loopCount >= 1) && noDelay && (sampleCount <= 1))
					{
						// Specify the loop count

						AIL_set_sample_loop_count(hSample3d, loopCount);
					}

					// Register the end of sample callback

					AIL_register_EOS_callback(hSample3d, &endOfSample3dCallBack);
				}

				Vector const position(iterSampleIdToSample3dMap->second.m_sound->getPosition_w());

				AIL_set_sample_3D_position(hSample3d, position.x, position.y, position.z);
				AIL_set_sample_3D_velocity_vector(hSample3d, 0.0f, 0.0f, 0.0f);

				// Set the audible distances

				float distanceAtMaxVolume = iterSampleIdToSample3dMap->second.m_sound->getTemplate()->getDistanceAtMaxVolume();

				bool fixDistanceMin = false;

				if (distanceAtMaxVolume <= 0.0f)
				{
					fixDistanceMin = true;
				}

				if (fixDistanceMin)
				{
					DEBUG_WARNING(true, ("sound distance at max volume (%f) should be greater greater than zero for \"%s\".", distanceAtMaxVolume, iterSampleIdToSample3dMap->second.getPath()));
					distanceAtMaxVolume = 2.0f;
				}

				float const distanceMin = distanceAtMaxVolume;
				float const distanceMax = Audio::getFallOffDistance(distanceMin);

				AIL_set_sample_3D_distances(hSample3d, distanceMax, distanceMin, 1);

				// Play the sample

				AIL_start_sample(hSample3d);

//#ifdef _DEBUG
//				DEBUG_REPORT_LOG(true, ("AIL_start_sample() <id> %s <vol> %.2f\n", iterSample->first->getString(), AIL_sample_volume_levels(hSample3d)));
//#endif // _DEBUG

				sound.setSampleId(sampleId);

				addPlayingSound(sound);
			}
			else
			{
				DEBUG_WARNING(true, ("Error preparing 3D sample for playback: \"%s\" Miles Error: (%s) Prioritized Sound Count(%d)", iterSampleIdToSample3dMap->second.getPath()->getString(), AIL_last_error(), static_cast<int>(s_prioritizedPlayingSounds.size())));
				DEBUG_REPORT_LOG(true, ("  Miles count(%d)\n", AIL_active_sample_count(s_digitalDevice2d)));
				DEBUG_REPORT_LOG(true, ("  Sample:    %s\n", iterSample->first->getString()));
				DEBUG_REPORT_LOG(true, ("  FileSize:  %d\n", iterSample->second.m_fileSize));
				DEBUG_REPORT_LOG(true, ("  Time:      %f\n", iterSample->second.m_time));
				DEBUG_REPORT_LOG(true, ("  Ref Count: %d\n", iterSample->second.m_referenceCount));

				result = false;
			}
		}
		else
		{
			DEBUG_WARNING(true, ("Unable to find the sample in the sound cache: \"%s\"", iterSampleIdToSample3dMap->second.getPath()->getString()));
			result = false;
		}
	}
	else if (isSampleStream(sampleId, iterSampleIdToSampleStreamMap))
	{
		NP_PROFILER_AUTO_BLOCK_DEFINE("Audio::startSample::isSampleStream");
		// The number of streamed samples is further limited due to disk access

		if (iterSampleIdToSampleStreamMap->second.m_stream != NULL)
		{
			setSampleVolume(iterSampleIdToSampleStreamMap->first, sound.getVolume());

			sound.setPlayBackRate(getSamplePlayBackRate(iterSampleIdToSampleStreamMap->first));
			setSamplePlayBackRate(iterSampleIdToSampleStreamMap->first, sound.getPlayBackRate(), sound.getPlayBackRateDelta());

			HSTREAM sampleStream = iterSampleIdToSampleStreamMap->second.m_stream;

			// Set the loop points

			int loopStartOffset;
			int loopEndOffset;

			if (Audio::getLoopOffsets(*iterSampleIdToSampleStreamMap->second.getPath(), loopStartOffset, loopEndOffset))
			{
				AIL_set_stream_loop_block(sampleStream, static_cast<S32>(loopStartOffset), static_cast<S32>(loopEndOffset));
			}

			DEBUG_REPORT_LOG(s_debugSoundStartStop, ("Audio start stream: %s\n", iterSampleIdToSampleStreamMap->second.getPath()->getString()));

			// Does this sample loop forever? Telling Miles lets us not have a hitch at the loop point.

			int const loopCount = iterSampleIdToSampleStreamMap->second.m_sound->getLoopCount();
			bool const infiniteLooping = (loopCount <= -1);
			bool const noDelay = (iterSampleIdToSampleStreamMap->second.m_sound->getTemplate()->getLoopDelayMax() <= 0.0f);
			int const sampleCount = iterSampleIdToSampleStreamMap->second.m_sound->getTemplate()->getSampleCount();

			// This is being changed to see if I can handle the looping using callbacks and it still not have a hitch

			if (infiniteLooping && noDelay && (sampleCount <= 1))
			{
				// Specify infinite looping

				AIL_set_stream_loop_count(sampleStream, 0);
			}
			else
			{
				if ((loopCount >= 1) && noDelay && (sampleCount <= 1))
				{
					// Specify the loop count

					AIL_set_stream_loop_count(sampleStream, loopCount);
				}

				// Register the end of sample callback

				AIL_register_stream_callback(sampleStream, &endOfSampleStreamCallBack);
			}

//#ifdef _DEBUG
//			float leftLevel;
//			float rightLevel;
//			AIL_stream_volume_levels(iterSampleIdToSampleStreamMap->second.m_stream, &leftLevel, &rightLevel);
//
//			DEBUG_REPORT_LOG(true, ("AIL_start_stream() <id> %d <left vol> %.2f <right vol> %.2f\n", iterSampleIdToSampleStreamMap->first.getId(), leftLevel, rightLevel));
//#endif // _DEBUG

			AIL_start_stream(sampleStream);

			sound.setSampleId(sampleId);

			addPlayingSound(sound);
		}
		else
		{
			DEBUG_WARNING(true, ("Sound stream is NULL\n"));

			// This should not happen but lets be graceful and not crash

			s_sampleIdToSampleStreamMap.erase(iterSampleIdToSampleStreamMap);
			result = false;
		}
	}
	else
	{
		result = false;
		DEBUG_WARNING(true, ("Invalid sampleId specified: %d", sound.getSampleId()));
	}

	// If there was an error, release this sampleId

	if (!result)
	{
		DEBUG_REPORT_LOG(true, ("Unable to start the sample...releasing: %s\n", sound.getTemplate()->getName()));

		float const fadeOutTime = 0.0f;
		bool const keepAlive = sound.isInfiniteLooping();

		AudioNamespace::stopSound(sound.getSoundId(), fadeOutTime, keepAlive);
	}
}

//-----------------------------------------------------------------------------
void Audio::setSampleOcclusion(SampleId const &sampleId, float const occlusion)
{
	DEBUG_WARNING((occlusion < 0.0f) || (occlusion > 1.0f), ("occlusion(%f) must be [0...1]", occlusion));

	SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;

	if (isSample3d(sampleId, iterSampleIdToSample3dMap))
	{
		Sample3d const &sample3d = iterSampleIdToSample3dMap->second;

		AIL_set_sample_occlusion(sample3d.m_sample, clamp(0.0f, occlusion, 1.0f));
	}
}

//-----------------------------------------------------------------------------
void Audio::setSampleObstruction(SampleId const &sampleId, float const obstruction)
{
	DEBUG_WARNING((obstruction < 0.0f) || (obstruction > 1.0f), ("obstruction(%f) must be [0...1]", obstruction));

	SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;

	if (isSample3d(sampleId, iterSampleIdToSample3dMap))
	{
		Sample3d const &sample3d = iterSampleIdToSample3dMap->second;

		AIL_set_sample_obstruction(sample3d.m_sample, clamp(0.0f, obstruction, 1.0f));
	}
}

//-----------------------------------------------------------------------------
void Audio::setSampleVolume(SampleId const &sampleId, float const volume)
{
#ifdef _DEBUG
	//static int count = 0;
	DEBUG_WARNING((volume < 0.0f), ("volume(%f) must be [0...1]", volume));
	DEBUG_WARNING((volume > 1.0f), ("volume(%f) must be [0...1]", volume));
#endif // _DEBUG

	SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
	SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
	SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

	if (isSample2d(sampleId, iterSampleIdToSample2dMap))
	{
		NOT_NULL(iterSampleIdToSample2dMap->second.m_sample);

		float const finalVolume = !s_audioEnabled ? 0.0f : clamp(0.0f, volume * s_masterVolume * getSoundCategoryVolume(iterSampleIdToSample2dMap->second.m_sound->getTemplate()->getSoundCategory()), 1.0f);
		float const leftLevel = finalVolume;
		float const rightLevel = finalVolume;

		//DEBUG_REPORT_LOG((getSampleVolume(sampleId) != finalVolume), ("Audio::setSampleVolume() %d <id> %s <volume> %.2f <old volume> %.2f\n", count++, iterSampleIdToSample2dMap->second.getPath()->getString(), finalVolume, getSampleVolume(sampleId)));

		Sample2d const &sample2d = iterSampleIdToSample2dMap->second;

		AIL_set_sample_volume_levels(sample2d.m_sample, leftLevel, rightLevel);
		AIL_set_sample_reverb_levels(sample2d.m_sample, 1.0f, 0.0f);
	}
	else if (isSample3d(sampleId, iterSampleIdToSample3dMap))
	{
		NOT_NULL(iterSampleIdToSample3dMap->second.m_sample);

		static float finalVolume;

		finalVolume = !s_audioEnabled ? 0.0f : clamp(0.0f, volume * s_masterVolume * getSoundCategoryVolume(iterSampleIdToSample3dMap->second.m_sound->getTemplate()->getSoundCategory()), 1.0f);

		// This is a hack because the EAX providers sometimes play sounds at full volume
		// even when passing in a small number for the volume

		if (finalVolume < 0.01)
		{
			finalVolume = 0.0f;
		}

		//DEBUG_REPORT_LOG((getSampleVolume(sampleId) != finalVolume), ("Audio::setSampleVolume() %d <id> %s <volume> %.2f <old volume> %.2f\n", count++, iterSampleIdToSample3dMap->second.getPath()->getString(), finalVolume, getSampleVolume(sampleId)));

		Sample3d const &sample3d = iterSampleIdToSample3dMap->second;

		AIL_set_sample_volume_levels(sample3d.m_sample, finalVolume, finalVolume);
		AIL_set_sample_reverb_levels(sample3d.m_sample, 1.0f, 0.0f);
	}
	else if (isSampleStream(sampleId, iterSampleIdToSampleStreamMap))
	{
		NOT_NULL(iterSampleIdToSampleStreamMap->second.m_stream);

		Audio::SoundCategory const soundCategory = iterSampleIdToSampleStreamMap->second.m_sound->getTemplate()->getSoundCategory();
		float const categoryVolume = getSoundCategoryVolume(soundCategory);
		float const finalVolume = !s_audioEnabled ? 0.0f : clamp(0.0f, volume * s_masterVolume * categoryVolume, 1.0f);
		float const leftLevel = finalVolume;
		float const rightLevel = finalVolume;

		SampleStream const &sampleStream = iterSampleIdToSampleStreamMap->second;

		// CONSULT-67 VOLSET probe (see s_lastVolSet): attribute music-stream
		// volume steps from OUR side of the AIL handoff -- runtime-agnostic.
		if (s_audioDiagFile)
		{
			char const * const path = sampleStream.getPath() ? sampleStream.getPath()->getString() : "";
			if (strstr(path, "music") != NULL)
			{
				void * const key = static_cast<void *>(sampleStream.m_stream);
				std::map<void *, float>::iterator const iterLast = s_lastVolSet.find(key);
				if (iterLast != s_lastVolSet.end())
				{
					float const delta = finalVolume - iterLast->second;
					if (delta > 0.02f || delta < -0.02f)
					{
						SYSTEMTIME st;
						GetLocalTime(&st);
						fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d VOLSET %.3f -> %.3f in=%.3f master=%.3f cat=%.3f global=%.3f bg=%.3f %s\n",
							st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
							iterLast->second, finalVolume,
							volume, s_masterVolume, categoryVolume,
							s_globalAudioFadeVolume, s_backgroundMusicFadeVolume,
							path);
						fflush(s_audioDiagFile);
					}
					iterLast->second = finalVolume;
				}
				else
				{
					// Log the SEED too: a stream whose volume is set once and never
					// changed produced ZERO lines, which is exactly the case that
					// mattered for the too-soft cantina band (2026-08-15).
					SYSTEMTIME st;
					GetLocalTime(&st);
					fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d VOLSET seed %.3f in=%.3f master=%.3f cat=%.3f global=%.3f bg=%.3f %s\n",
						st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
						finalVolume, volume, s_masterVolume, categoryVolume,
						s_globalAudioFadeVolume, s_backgroundMusicFadeVolume,
						path);
					fflush(s_audioDiagFile);
					s_lastVolSet[key] = finalVolume;
				}
			}
		}

		AIL_set_sample_volume_levels((AIL_stream_sample_handle(sampleStream.m_stream)), leftLevel, rightLevel);
		AIL_set_sample_reverb_levels((AIL_stream_sample_handle(sampleStream.m_stream)), 1.0f, 0.0f);
	}
}

//-----------------------------------------------------------------------------
void Audio::setSamplePlayBackRate(SampleId const &sampleId, int const playBackRate, float const playBackRateDelta)
{
	DEBUG_FATAL((playBackRate <= 0), ("Invalid initial playback rate: %d", playBackRate));

	if (s_installed &&
		(playBackRateDelta > 0.0f))
	{
		int const finalPlayBackRate = static_cast<int>(static_cast<float>(playBackRate) * playBackRateDelta);
		static int const minPlayBackRate = 5000;
		static int const maxPlayBackRate = 60000;

		SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
		SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
		SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

		int const clampedPlayBackRate = clamp(minPlayBackRate, finalPlayBackRate, maxPlayBackRate);

#ifdef _DEBUG
		CrcString const * fileName = 0;
#endif

		if (isSample2d(sampleId, iterSampleIdToSample2dMap))
		{
#ifdef _DEBUG
			fileName = iterSampleIdToSample2dMap->second.getPath();
#endif

			AIL_set_sample_playback_rate(iterSampleIdToSample2dMap->second.m_sample, clampedPlayBackRate);
		}
		else if (isSample3d(sampleId, iterSampleIdToSample3dMap))
		{
#ifdef _DEBUG
			fileName = iterSampleIdToSample3dMap->second.getPath();
#endif

			AIL_set_sample_playback_rate(iterSampleIdToSample3dMap->second.m_sample, clampedPlayBackRate);
		}
		else if (isSampleStream(sampleId, iterSampleIdToSampleStreamMap))
		{
#ifdef _DEBUG
			fileName = iterSampleIdToSampleStreamMap->second.getPath();
#endif

			AIL_set_sample_playback_rate((AIL_stream_sample_handle(iterSampleIdToSampleStreamMap->second.m_stream)), clampedPlayBackRate);
		}
		else
		{
			DEBUG_WARNING(true, ("Trying to set the playback rate of a sample [%i] that no longer exists.", sampleId.getId()));
		}

#ifdef _DEBUG
		DEBUG_WARNING(fileName && (finalPlayBackRate < minPlayBackRate || finalPlayBackRate > maxPlayBackRate), ("Audio::setSamplePlayBackRate(%s): Invalid playbackrate specified: %d (rate=%d, delta=%1.2f) clamping to (min=%d, max=%d)", fileName->getString(), finalPlayBackRate, playBackRate, playBackRateDelta, minPlayBackRate, maxPlayBackRate));
#endif
	}
}

//-----------------------------------------------------------------------------
float Audio::getSampleVolume(SampleId const &sampleId)
{
	float result = 0.0f;

	SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
	SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
	SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

	if (isSample2d(sampleId, iterSampleIdToSample2dMap))
	{
		float leftLevel;
		float rightLevel;

		AIL_sample_volume_levels(iterSampleIdToSample2dMap->second.m_sample, &leftLevel, &rightLevel);

		result = (leftLevel + rightLevel) / 2.0f;
	}
	else if (isSample3d(sampleId, iterSampleIdToSample3dMap))
	{
		float leftLevel = 0.0f;
		float rightLevel = 0.0f;

		AIL_sample_volume_levels(iterSampleIdToSample3dMap->second.m_sample, &leftLevel, &rightLevel);

		result = (leftLevel + rightLevel) / 2.0f;
	}
	else if (isSampleStream(sampleId, iterSampleIdToSampleStreamMap))
	{
		float leftLevel = 0.0f;
		float rightLevel = 0.0f;

		AIL_sample_volume_levels((AIL_stream_sample_handle(iterSampleIdToSampleStreamMap->second.m_stream)), &leftLevel, &rightLevel);

		result = (leftLevel + rightLevel) / 2.0f;
	}

	return result;
}

//-----------------------------------------------------------------------------
int Audio::getSamplePlayBackRate(SampleId const &sampleId)
{
	int result = 0;

	if (s_installed)
	{
		SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
		SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
		SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

		if (isSample2d(sampleId, iterSampleIdToSample2dMap))
		{
			result = AIL_sample_playback_rate(iterSampleIdToSample2dMap->second.m_sample);
		}
		else if (isSample3d(sampleId, iterSampleIdToSample3dMap))
		{
			result = AIL_sample_playback_rate(iterSampleIdToSample3dMap->second.m_sample);
		}
		else if (isSampleStream(sampleId, iterSampleIdToSampleStreamMap))
		{
			result = AIL_sample_playback_rate((AIL_stream_sample_handle(iterSampleIdToSampleStreamMap->second.m_stream)));
		}
		else
		{
			DEBUG_WARNING(true, ("Trying to get the playback rate of a sample [%i] that no longer exists.", sampleId.getId()));
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
void stopSample(Sound2 const &sound)
{
	if (s_installed)
	{
		SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
		SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
		SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

		if (isSample2d(sound.getSampleId(), iterSampleIdToSample2dMap))
		{
			AIL_stop_sample(iterSampleIdToSample2dMap->second.m_sample);
			AIL_end_sample(iterSampleIdToSample2dMap->second.m_sample);
		}
		else if (isSample3d(sound.getSampleId(), iterSampleIdToSample3dMap))
		{
			AIL_stop_sample(iterSampleIdToSample3dMap->second.m_sample);
			AIL_end_sample(iterSampleIdToSample3dMap->second.m_sample);
		}
		else if (isSampleStream(sound.getSampleId(), iterSampleIdToSampleStreamMap))
		{
			//AIL_pause_stream(iterSampleIdToSampleStreamMap->second.m_stream, 1);

			// CONSULT-62 wave 2: time the close -- AIL_close_stream takes the global
			// Miles mutex and cancels in-flight async IO (a disk wait); a long close
			// on main at end-of-track is the "+34s crackle storm" suspect (the theme
			// ends ~34s after zone-in in every session).
			DWORD const diagCloseStart = s_audioDiagFile ? GetTickCount() : 0;
			HSTREAM const diagClosedStream = iterSampleIdToSampleStreamMap->second.m_stream;
			s_lastVolSet.erase(static_cast<void *>(diagClosedStream));   // CONSULT-67 VOLSET probe: HSTREAM may be reused
			AIL_close_stream(iterSampleIdToSampleStreamMap->second.m_stream);
			if (s_audioDiagFile)
			{
				DWORD const diagCloseMs = GetTickCount() - diagCloseStart;
				SYSTEMTIME st;
				GetLocalTime(&st);
				fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d CLOSE %s took %lums stream=%p\n",
					st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
					iterSampleIdToSampleStreamMap->second.getPath() ? iterSampleIdToSampleStreamMap->second.getPath()->getString() : "(null)",
					diagCloseMs, static_cast<void *>(diagClosedStream));
				fflush(s_audioDiagFile);
			}

			iterSampleIdToSampleStreamMap->second.m_stream = NULL;
		}
		else
		{
			DEBUG_WARNING(true, ("Audio::stopSample - Trying to stop a sample [%i] that no longer exists.", sound.getSampleId().getId()));
		}
	}
}

//-----------------------------------------------------------------------------
void Audio::releaseSampleId(Sound2 const &sound)
{
	if (s_installed)
	{
		// Make sure the sample is stopped

		stopSample(sound);

		// Now release the id

		SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
		SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
		SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

		if (isSample2d(sound.getSampleId(), iterSampleIdToSample2dMap))
		{
			AIL_release_sample_handle(iterSampleIdToSample2dMap->second.m_sample);
			--s_allocated2dSampleHandles;
			iterSampleIdToSample2dMap->second.m_sample = NULL;

			s_sampleIdToSample2dMap.erase(iterSampleIdToSample2dMap);
			removeSoundFromPrioritizedPlayingSounds(sound);
		}
		else if (isSample3d(sound.getSampleId(), iterSampleIdToSample3dMap))
		{
			AIL_release_sample_handle(iterSampleIdToSample3dMap->second.m_sample);
			--s_allocated3dSampleHandles;
			iterSampleIdToSample3dMap->second.m_sample = NULL;

			s_sampleIdToSample3dMap.erase(iterSampleIdToSample3dMap);
			removeSoundFromPrioritizedPlayingSounds(sound);
		}
		else if (isSampleStream(sound.getSampleId(), iterSampleIdToSampleStreamMap))
		{
			s_sampleIdToSampleStreamMap.erase(iterSampleIdToSampleStreamMap);
			removeSoundFromPrioritizedPlayingSounds(sound);
		}
		else
		{
			DEBUG_WARNING(true, ("Trying to release a sample [%i] that no longer exists.", sound.getSampleId().getId()));
		}
	}
}

//-----------------------------------------------------------------------------
float Audio::getSoundTemplateVolume(SoundId const &soundId)
{
	float result = 0.0f;

	SoundIdToSoundMap::iterator iterSoundMap = getIterSoundIdToSoundMap(soundId);

	if (iterSoundMap != s_soundIdToSoundMap.end())
	{
		result = iterSoundMap->second->getTemplateVolume();
	}

	return result;
}

//-----------------------------------------------------------------------------
float Audio::getSoundAttenuation(SoundId const &soundId)
{
	float result = 0.0f;

	SoundIdToSoundMap::iterator iterSoundMap = getIterSoundIdToSoundMap(soundId);

	if (iterSoundMap != s_soundIdToSoundMap.end())
	{
		result = iterSoundMap->second->getAttenuation();
	}

	return result;
}

//-----------------------------------------------------------------------------
float Audio::getSoundVolume(SoundId const &soundId)
{
	float result = 0.0f;

	SoundIdToSoundMap::iterator iterSoundMap = getIterSoundIdToSoundMap(soundId);

	if (iterSoundMap != s_soundIdToSoundMap.end())
	{
		result = iterSoundMap->second->getVolume();
	}

	return result;
}

// Adjustment in half steps
//-----------------------------------------------------------------------------
float Audio::getSoundPitchDelta(SoundId const &soundId)
{
	float result = 0.0f;

	SoundIdToSoundMap::iterator iterSoundMap = getIterSoundIdToSoundMap(soundId);

	if (iterSoundMap != s_soundIdToSoundMap.end())
	{
		result = iterSoundMap->second->getPitchDelta();
	}

	return result;
}

//-----------------------------------------------------------------------------
int Audio::getCachedSampleCount()
{
	return static_cast<int>(s_sampleCache.size());
}

//-----------------------------------------------------------------------------
int Audio::getSample2dCount()
{
	return static_cast<int>(s_sampleIdToSample2dMap.size());
}

//-----------------------------------------------------------------------------
int Audio::getSample3dCount()
{
	return static_cast<int>(s_sampleIdToSample3dMap.size());
}

//-----------------------------------------------------------------------------
int Audio::getSampleStreamCount()
{
	return static_cast<int>(s_sampleIdToSampleStreamMap.size());
}

//-----------------------------------------------------------------------------
int Audio::getSampleCount()
{
	return getSample2dCount() + getSample3dCount() + getSampleStreamCount();
}

//-----------------------------------------------------------------------------
int Audio::getSoundCount()
{
	return static_cast<int>(s_soundIdToSoundMap.size());
}

//-----------------------------------------------------------------------------
int Audio::getDigitalCpuPercent()
{
	int result = 0;

	if (s_installed)
	{
		result = AIL_digital_CPU_percent(s_digitalDevice2d);
	}

	return result;
}

//-----------------------------------------------------------------------------
int Audio::getDigitalLatency()
{
	int result = 0;

	if (s_installed)
	{
		result = AIL_digital_latency(s_digitalDevice2d);
	}

	return result;
}

//-----------------------------------------------------------------------------
SoundId Audio::playSound(Iff &iff)
{
	return playSound(iff, NULL, NULL);
}

//-----------------------------------------------------------------------------
SoundId Audio::playSound(Iff &iff, Vector const &position)
{
	return playSound(iff, &position, NULL);
}

//-----------------------------------------------------------------------------
SoundId Audio::playSound(Iff &iff, Vector const * const position, CellProperty const * const parentCell)
{
	SoundId result;

	if (s_installed)
	{
		SoundTemplate const *soundTemplate = SoundTemplateList::fetch(&iff);

		if (soundTemplate != NULL)
		{
			if ((position != NULL) &&
				soundTemplate->is3d())
			{
				result = playSound3d(soundTemplate, *position, parentCell);
			}
			else if (soundTemplate->is2d())
			{
				result = playSound2d(soundTemplate, position, parentCell);
			}
			else
			{
				DEBUG_WARNING(true, ("Audio::playSound - Attempting to play a sound with an invalid iff file: %s", iff.getFileName () ? iff.getFileName () : "null"));
			}

			SoundTemplateList::release(soundTemplate);
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
SoundId Audio::attachSound(char const *path, Object const *object, char const *hardPointName)
{
	return AudioNamespace::playSound(path, NULL, object, hardPointName, NULL);
}

//-----------------------------------------------------------------------------
SoundId Audio::attachSound(Iff &iff, Object const *object, char const *hardPointName)
{
	SoundId result;

	if (s_installed &&
		(object != NULL))
	{
		SoundTemplate const *soundTemplate = SoundTemplateList::fetch(&iff);

		if (soundTemplate != NULL)
		{
			result = ::attachSound(soundTemplate, object, hardPointName);

			SoundTemplateList::release(soundTemplate);
		}
	}

	return result;
}

// Removes the single sound from the object it is stuck to
//-----------------------------------------------------------------------------
void Audio::detachSound(SoundId const &soundId, float const fadeOutTime)
{
	stopSound(soundId, fadeOutTime);
}

// Removes all the sounds attached to the object
//-----------------------------------------------------------------------------
void Audio::detachSound(Object const &object, float const fadeOutTime)
{
	SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.begin();
	s_localPurgeList.clear();

	for (; iterSoundIdToSoundMap != s_soundIdToSoundMap.end(); ++iterSoundIdToSoundMap)
	{
		if (iterSoundIdToSoundMap->second->getObject() == &object)
		{
			s_localPurgeList.push_back(iterSoundIdToSoundMap->first);
		}
	}

	// Stop all the sounds

	for (unsigned int i = 0; i < s_localPurgeList.size(); ++i)
	{
		stopSound(s_localPurgeList[i], fadeOutTime);
	}
}

//-----------------------------------------------------------------------------
bool isSample2d(SampleId const &sampleId, SampleIdToSample2dMap::iterator &iterSampleIdToSample2dMap)
{
	bool result = false;

	if (s_installed)
	{
		iterSampleIdToSample2dMap = s_sampleIdToSample2dMap.find(sampleId);

		result = (iterSampleIdToSample2dMap != s_sampleIdToSample2dMap.end());
	}

	return result;
}

//-----------------------------------------------------------------------------
bool isSample3d(SampleId const &sampleId, SampleIdToSample3dMap::iterator &iterSampleIdToSample3dMap)
{
	bool result = false;

	if (s_installed)
	{
		iterSampleIdToSample3dMap = s_sampleIdToSample3dMap.find(sampleId);

		result = (iterSampleIdToSample3dMap != s_sampleIdToSample3dMap.end());
	}

	return result;
}

//-----------------------------------------------------------------------------
bool isSampleStream(SampleId const &sampleId, SampleIdToSampleStreamMap::iterator &iterSampleIdToSampleStreamMap)
{
	bool result = false;

	if (s_installed)
	{
		iterSampleIdToSampleStreamMap = s_sampleIdToSampleStreamMap.find(sampleId);

		result = (iterSampleIdToSampleStreamMap != s_sampleIdToSampleStreamMap.end());
	}

	return result;
}


//-----------------------------------------------------------------------------
AudioSampleInformation Audio::getSampleInformation(std::string const &path)
{
	AudioSampleInformation audioSampleInformation;

	if (s_installed)
	{
		if (!path.empty())
		{
			AILSOUNDINFO soundInfo;

			AbstractFile *file = TreeFile::open(path.c_str(), AbstractFile::PriorityData, true);
			if (file)
			{
				int fileSize = file->length();
				byte *fileImage = file->readEntireFileAndClose();
				delete file;

				S32 result = AIL_WAV_info(fileImage, &soundInfo);

				if (result)
				{
					audioSampleInformation.m_bits = static_cast<int>(soundInfo.bits);
					audioSampleInformation.m_blockSize = static_cast<int>(soundInfo.block_size);
					audioSampleInformation.m_channels = static_cast<int>(soundInfo.channels);
					audioSampleInformation.m_dataLength = static_cast<int>(soundInfo.data_len);
					audioSampleInformation.m_format = static_cast<int>(soundInfo.format);

					float timeCurrent = 0;
					::getSampleTime(path.c_str(), fileImage, fileSize, audioSampleInformation.m_time, timeCurrent);

					audioSampleInformation.m_rate = static_cast<int>(soundInfo.rate);
					audioSampleInformation.m_samples = static_cast<int>(soundInfo.samples);
					audioSampleInformation.m_type = getSampleType(fileImage, fileSize);
				}

				delete [] fileImage;
			}
		}
	}

	return audioSampleInformation;
}

// End all sounds and invalidates their sound ids
//-----------------------------------------------------------------------------
void Audio::stopAllSounds(float const fadeOutTime)
{
	bool const keepAlive = false;
	::stopAllSounds(fadeOutTime, keepAlive);

	DEBUG_FATAL(!s_soundIdToSoundMap.empty() && (fadeOutTime <= 0.0f), ("Audio::stopAllSounds - s_soundIdToSoundMap not empty"));
}

//-----------------------------------------------------------------------------
void stopAllSounds(float const fadeOutTime, bool const keepAlive)
{
	s_localPurgeList.clear();
	SoundIdToSoundMap::const_iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.begin();

	for (; iterSoundIdToSoundMap != s_soundIdToSoundMap.end(); ++iterSoundIdToSoundMap)
	{
		s_localPurgeList.push_back(iterSoundIdToSoundMap->first);
	}

	SoundIdList::const_iterator iterLocalPurgeList = s_localPurgeList.begin();

	for (; iterLocalPurgeList != s_localPurgeList.end(); ++iterLocalPurgeList)
	{
		SoundId const & soundId = (*iterLocalPurgeList);
		stopSound(soundId, fadeOutTime, keepAlive);
	}

	s_localPurgeList.clear();

	Audio::stopBufferedSound();
	Audio::stopBufferedMusic();

	Audio::alter(0.0f, s_listenerObject);
}

//-----------------------------------------------------------------------------
std::string const &Audio::getCurrent3dProvider()
{
	return s_soundProvider;
}


// Set a filter that affects all sounds, but not music
//-----------------------------------------------------------------------------
void Audio::setRoomType(RoomType const roomType)
{
	if (s_installed && isRoomTypeSupported())
	{
		switch (roomType)
		{
			case RT_alley:           { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_ALLEY); } break;
			case RT_arena:           { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_ARENA); } break;
			case RT_auditorium:      { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_AUDITORIUM); } break;
			case RT_bathRoom:        { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_BATHROOM); } break;
			case RT_carpetedHallway: { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_HALLWAY); } break;
			case RT_cave:            { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_CAVE); } break;
			case RT_city:            { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_CITY); } break;
			case RT_concertHall:     { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_CONCERTHALL); } break;
			case RT_dizzy:           { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_DIZZY); } break;
			case RT_drugged:         { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_DRUGGED); } break;
			case RT_forest:          { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_FOREST); } break;
			case RT_generic:         { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_GENERIC); } break;
			case RT_hallway:         { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_HALLWAY); } break;
			case RT_hangar:          { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_HANGAR); } break;
			case RT_livingRoom:      { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_LIVINGROOM); } break;
			case RT_mountains:       { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_MOUNTAINS); } break;
			case RT_paddedCell:      { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_PADDEDCELL); } break;
			case RT_parkingLot:      { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_PARKINGLOT); } break;
			case RT_plain:           { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_PLAIN); } break;
			case RT_psychotic:       { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_PSYCHOTIC); } break;
			case RT_quarry:          { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_QUARRY); } break;
			case RT_room:            { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_ROOM); } break;
			case RT_sewerPipe:       { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_SEWERPIPE); } break;
			case RT_stoneCorridor:   { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_STONECORRIDOR); } break;
			case RT_stoneRoom:       { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_STONEROOM); } break;
			case RT_underWater:      { AIL_set_room_type(s_digitalDevice2d, 0, ENVIRONMENT_UNDERWATER); } break;
			default:                 { DEBUG_FATAL(true, ("Trying to set an unknown room type.")); } break;
		}
	}
}

//-----------------------------------------------------------------------------
Audio::RoomType Audio::getRoomType()
{
	RoomType result = RT_notSupported;

	if (s_digitalDevice2d != NULL)
	{
		switch (AIL_room_type(s_digitalDevice2d, 0))
		{
			case ENVIRONMENT_ALLEY:           { result = RT_alley; } break;
			case ENVIRONMENT_ARENA:           { result = RT_arena; } break;
			case ENVIRONMENT_AUDITORIUM:      { result = RT_auditorium; } break;
			case ENVIRONMENT_BATHROOM:        { result = RT_bathRoom; } break;
			case ENVIRONMENT_CARPETEDHALLWAY: { result = RT_carpetedHallway; } break;
			case ENVIRONMENT_CAVE:            { result = RT_cave; } break;
			case ENVIRONMENT_CITY:            { result = RT_city; } break;
			case ENVIRONMENT_CONCERTHALL:     { result = RT_concertHall; } break;
			case ENVIRONMENT_DIZZY:           { result = RT_dizzy;}  break;
			case ENVIRONMENT_DRUGGED:         { result = RT_drugged; } break;
			case ENVIRONMENT_FOREST:          { result = RT_forest; } break;
			case ENVIRONMENT_GENERIC:         { result = RT_generic; } break;
			case ENVIRONMENT_HALLWAY:         { result = RT_hallway; } break;
			case ENVIRONMENT_HANGAR:          { result = RT_hangar; } break;
			case ENVIRONMENT_LIVINGROOM:      { result = RT_livingRoom; } break;
			case ENVIRONMENT_MOUNTAINS:       { result = RT_mountains; } break;
			case ENVIRONMENT_PADDEDCELL:      { result = RT_paddedCell; } break;
			case ENVIRONMENT_PARKINGLOT:      { result = RT_parkingLot; } break;
			case ENVIRONMENT_PLAIN:           { result = RT_plain; } break;
			case ENVIRONMENT_PSYCHOTIC:       { result = RT_psychotic; } break;
			case ENVIRONMENT_QUARRY:          { result = RT_quarry; } break;
			case ENVIRONMENT_ROOM:            { result = RT_room; } break;
			case ENVIRONMENT_SEWERPIPE:       { result = RT_sewerPipe; } break;
			case ENVIRONMENT_STONECORRIDOR:   { result = RT_stoneCorridor; } break;
			case ENVIRONMENT_STONEROOM:       { result = RT_stoneRoom; } break;
			case ENVIRONMENT_UNDERWATER:      { result = RT_underWater; } break;
			default:                          {} break;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
char const * const Audio::getRoomTypeString()
{
	static char const * result = "";

	switch (getRoomType())
	{
		case RT_alley:           { result = "Alley"; } break;
		case RT_arena:           { result = "Arena"; } break;
		case RT_auditorium:      { result = "Auditorium"; } break;
		case RT_bathRoom:        { result = "Bathroom"; } break;
		case RT_carpetedHallway: { result = "Carpeted hallway"; } break;
		case RT_cave:            { result = "Cave"; } break;
		case RT_city:            { result = "City"; } break;
		case RT_concertHall:     { result = "Concert hall"; } break;
		case RT_dizzy:           { result = "Dizzy"; } break;
		case RT_drugged:         { result = "Drugged"; } break;
		case RT_forest:          { result = "Forest"; } break;
		case RT_generic:         { result = "Generic"; } break;
		case RT_hallway:         { result = "Hallway"; } break;
		case RT_hangar:          { result = "Hangar"; } break;
		case RT_livingRoom:      { result = "Living room"; } break;
		case RT_mountains:       { result = "Mountains"; } break;
		case RT_paddedCell:      { result = "Padded cell"; } break;
		case RT_parkingLot:      { result = "Parking lot"; } break;
		case RT_plain:           { result = "Plain"; } break;
		case RT_psychotic:       { result = "Psychotic"; } break;
		case RT_quarry:          { result = "Quarry"; } break;
		case RT_room:            { result = "Room"; } break;
		case RT_sewerPipe:       { result = "Sewer pipe"; } break;
		case RT_stoneCorridor:   { result = "Stone corridor"; } break;
		case RT_stoneRoom:       { result = "Stone room"; } break;
		case RT_underWater:      { result = "Underwater"; } break;
		default:                 { result = "Not Supported"; } break;
	}

	return result;
}

//-----------------------------------------------------------------------------
void Audio::setSampleEffectsLevel(SampleId const &sampleId, float const effectLevel)
{
	DEBUG_FATAL(((effectLevel == -0.0f) || (effectLevel < 0.0f) || (effectLevel > 1.0f)), ("effectLevel out of range: %f", effectLevel));

	if (s_installed)
	{
		SampleIdToSample3dMap::iterator iterSampleIdToData3dMap = getIterSampleIdToSample3dMap(sampleId);

		if (iterSampleIdToData3dMap != s_sampleIdToSample3dMap.end())
		{
			AIL_set_sample_reverb_levels(iterSampleIdToData3dMap->second.m_sample, static_cast<F32>(clamp(0.0f, effectLevel, 1.0f)), 0.0f);
		}
	}
}

//-----------------------------------------------------------------------------
float Audio::getSampleEffectsLevel(SampleId const &sampleId)
{
	float resultDry = 0.0f;
	float resultWet = 0.0f;

	if (s_installed)
	{
		SampleIdToSample3dMap::iterator iterSampleIdToData3dMap = getIterSampleIdToSample3dMap(sampleId);

		if (iterSampleIdToData3dMap != s_sampleIdToSample3dMap.end())
		{
			AIL_sample_reverb_levels(iterSampleIdToData3dMap->second.m_sample, &resultDry, &resultWet);
		}
	}

	return resultDry;
}


static int once = true;

//-----------------------------------------------------------------------------
U32 __stdcall fileOpenCallBack(char const *fileName, UINTa *fileHandle)
{
	if (once && !Os::isMainThread())
	{
		once = false;
		PerThreadData::threadInstall(false);
	}

	// 35-05 FIX (gated): substitute the remembered stream filename when Miles' background IO thread
	// re-opens with an empty name; otherwise remember this (real) name for that re-open.
	// Shared state is read/updated under the lock; the name is copied to a local so the
	// TreeFile::open below runs UNLOCKED (see the FileCallbackCriticalSection comment).
	char effectiveName[1024];
	effectiveName[0] = '\0';
	bool substituted = false;
	{
		FileCallbackLock const lock;

		char const *name = fileName;
		if (s_titleMusicStreamFix)
		{
			if ((name == NULL) || (name[0] == '\0'))
			{
				if (s_lastAudioOpenName[0] != '\0')
				{
					name = s_lastAudioOpenName;
					substituted = true;
				}
			}
			else
			{
				// A real (non-empty) name == a NEW stream session opening (AIL_open_stream). Reset the
				// sequential-read tracking so this stream starts streaming from offset 0, and release any
				// substituted handles left over from a previous session (leak guard).
				strncpy_s(s_lastAudioOpenName, name, _TRUNCATE);
				s_streamResumeOffset = 0;
				for (std::map<unsigned, bool>::iterator it = s_substitutedHandleMap.begin(); it != s_substitutedHandleMap.end(); ++it)
				{
					FileMap::iterator fit = s_fileMap.find(it->first);
					if (fit != s_fileMap.end()) { fit->second->close(); delete fit->second; s_fileMap.erase(fit); }
				}
				s_substitutedHandleMap.clear();
			}
		}

		if (name != NULL)
			strncpy_s(effectiveName, name, _TRUNCATE);
	}

	AbstractFile *abstractFile = (effectiveName[0] != '\0')
		? TreeFile::open(effectiveName, AbstractFile::PriorityAudioVideo, true)
		: NULL;

	if (abstractFile != NULL)
	{
		FileCallbackLock const lock;

		*fileHandle = s_nextFileHandle;
		s_fileMap.insert(std::make_pair(s_nextFileHandle, abstractFile));
		++s_nextFileHandle;

#ifdef _DEBUG
		ms_handleNameMap.insert(std::make_pair(*fileHandle, effectiveName));
#endif

		// 35-05 FIX (part 2): a substituted re-open must CONTINUE where streaming left off, not
		// restart at 0. Seek to the tracked offset and remember this handle so reads advance it.
		if (s_titleMusicStreamFix && substituted)
		{
			abstractFile->seek(AbstractFile::SeekBegin, s_streamResumeOffset);

			// LEAK GUARD: Miles re-opens the stream file once per ~36KB chunk and never closes the
			// prior handle (probe: 80 opens / 16 closes -> ~64 AbstractFiles leaked at login). The
			// access pattern is strictly open -> single read -> open-next, so the previous substituted
			// handle is finished. Release it now (callbacks are handle-keyed, so any late access to a
			// closed handle just misses gracefully -- no use-after-free). Keeps exactly one live.
			for (std::map<unsigned, bool>::iterator it = s_substitutedHandleMap.begin(); it != s_substitutedHandleMap.end(); ++it)
			{
				FileMap::iterator fit = s_fileMap.find(it->first);
				if (fit != s_fileMap.end()) { fit->second->close(); delete fit->second; s_fileMap.erase(fit); }
			}
			s_substitutedHandleMap.clear();

			s_substitutedHandleMap[static_cast<unsigned>(*fileHandle)] = true;
		}
	}
	else
	{
		*fileHandle = 0;
	}

	if (s_audioDiagFile)
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d FILEOPEN %s -> %s handle=%u\n",
			st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
			effectiveName[0] ? effectiveName : "(empty)",
			abstractFile ? "ok" : "FAIL",
			static_cast<unsigned>(*fileHandle));
		fflush(s_audioDiagFile);
	}

	return (abstractFile != NULL);
}

//-----------------------------------------------------------------------------
void __stdcall fileCloseCallBack(UINTa const fileHandle)
{
	if (once && !Os::isMainThread())
	{
		once = false;
		PerThreadData::threadInstall(false);
	}

	// Detach from the shared map under the lock; do the close I/O and delete unlocked
	// (once erased, no other thread can reach this AbstractFile).
	AbstractFile *abstractFile = NULL;
	{
		FileCallbackLock const lock;

		FileMap::iterator fileMapIter = s_fileMap.find(fileHandle);

		if (fileMapIter != s_fileMap.end())
		{
			abstractFile = fileMapIter->second;

			// Remove the file from the list

			s_fileMap.erase(fileMapIter);

			// 35-05: stop tracking a closed substituted handle so the per-chunk re-opens don't leak.
			if (s_titleMusicStreamFix)
				s_substitutedHandleMap.erase(static_cast<unsigned>(fileHandle));

#ifdef _DEBUG
			ms_fileCloseHandleSet.insert(fileHandle);
#endif
		}
		else
		{
#ifdef _DEBUG
			determineCallbackError("close", fileHandle);
#endif
		}
	}

	if (abstractFile != NULL)
	{
		// Close the file

		abstractFile->close();

		// Delete the file pointer

		delete abstractFile;
	}
}

//-----------------------------------------------------------------------------
S32 __stdcall fileSeekCallBack(UINTa const fileHandle, S32 const offset, U32 const type)
{
	if (once && !Os::isMainThread())
	{
		once = false;
		PerThreadData::threadInstall(false);
	}

	// Look up the handle under the lock; seek unlocked (single consumer per handle).
	AbstractFile *abstractFile = NULL;
	{
		FileCallbackLock const lock;
		FileMap::iterator fileMapIter = s_fileMap.find(fileHandle);
		if (fileMapIter != s_fileMap.end())
			abstractFile = fileMapIter->second;
	}

	int result = 0;

	if (abstractFile != NULL)
	{
		switch (type)
		{
			case AIL_FILE_SEEK_BEGIN:   // Seek relative to the beginning of the file
				{
					abstractFile->seek(AbstractFile::SeekBegin, static_cast<long>(offset));
				}
				break;
			case AIL_FILE_SEEK_CURRENT: // Seek relative to the current position of the file
				{
					abstractFile->seek(AbstractFile::SeekCurrent, static_cast<long>(offset));
				}
				break;
			case AIL_FILE_SEEK_END:     // Seek relative to the end of the file
				{
					abstractFile->seek(AbstractFile::SeekEnd, static_cast<long>(offset));
				}
				break;
			default:
				{
					FATAL(1, ("Unkown file seek callback type: %d", type));
				}
				break;
		}

		// Set the current file position

		result = abstractFile->tell();
	}
	else
	{
#ifdef _DEBUG
		determineCallbackError("seek", fileHandle);
#endif
	}

	// Return the new absolute position of the file pointer (relative to the beginning of the file)

	if (s_audioDiagFile && (InterlockedDecrement(const_cast<LONG *>(&s_diagFileIoLogBudget)) > 0))
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d FILESEEK handle=%u type=%u offset=%ld -> pos=%d\n",
			st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
			static_cast<unsigned>(fileHandle), static_cast<unsigned>(type), static_cast<long>(offset), result);
		fflush(s_audioDiagFile);
	}

	return result;
}

//-----------------------------------------------------------------------------
U32 __stdcall fileReadCallBack(UINTa const fileHandle, void *buffer, U32 const bytes)
{
// miles crasher hack
#if 0
	static bool trashMiles = false;

	if(trashMiles)
	{
		buffer = (void *)0xdeadbeef;
	}
#endif
// end miles crasher hack
	if (once && !Os::isMainThread())
	{
		once = false;
		PerThreadData::threadInstall(false);
	}

	// Look up the handle under the lock; the read I/O itself runs UNLOCKED so a
	// main-thread one-shot sample load can't stall Miles' IO thread mid-music-stream.
	AbstractFile *abstractFile = NULL;
	{
		FileCallbackLock const lock;
		FileMap::iterator fileMapIter = s_fileMap.find(fileHandle);
		if (fileMapIter != s_fileMap.end())
			abstractFile = fileMapIter->second;
	}

	int bytesRead = 0;

	if (abstractFile != NULL)
	{
		bytesRead = abstractFile->read(buffer, bytes);

		// 35-05 FIX (part 2): advance the shared streaming position so the NEXT re-open continues
		// from here instead of restarting at 0. tell() also captures any seek Miles did on this
		// handle (e.g. when it loops back), so this naturally follows Miles' own positioning.
		if (s_titleMusicStreamFix)
		{
			int const filePos = abstractFile->tell();
			FileCallbackLock const lock;
			if (s_substitutedHandleMap.find(static_cast<unsigned>(fileHandle)) != s_substitutedHandleMap.end())
				s_streamResumeOffset = filePos;
		}
	}
	else
	{
#ifdef _DEBUG
		determineCallbackError("read", fileHandle);
#endif
	}

	if (s_audioDiagFile && (InterlockedDecrement(const_cast<LONG *>(&s_diagFileIoLogBudget)) > 0))
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d FILEREAD handle=%u want=%u -> got=%d\n",
			st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
			static_cast<unsigned>(fileHandle), static_cast<unsigned>(bytes), bytesRead);
		fflush(s_audioDiagFile);
	}

	return static_cast<U32>(bytesRead);
}

//-----------------------------------------------------------------------------
bool Audio::isRoomTypeSupported()
{
	return (getRoomType() != RT_notSupported);
}

//-----------------------------------------------------------------------------
// Gets the total and current milliseconds of the current sample in the sound
// that is playing.
//-----------------------------------------------------------------------------
void Audio::getCurrentSampleTime(SoundId const &soundId, float &timeTotal, float &timeCurrent)
{
	timeTotal = 0.0f;
	timeCurrent = 0.0f;

	SoundIdToSoundMap::iterator iterSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundMap != s_soundIdToSoundMap.end())
	{
		Sound2 *sound = iterSoundMap->second;

		if (sound->getSampleId().getId() != 0)
		{
			getSampleTime(sound->getSampleId(), timeTotal, timeCurrent);
		}
	}
}

//-----------------------------------------------------------------------------
void Audio::setCurrentSoundTime(SoundId const &soundId, int const milliSecond)
{
	SoundIdToSoundMap::iterator iterSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundMap != s_soundIdToSoundMap.end())
	{
		Sound2 *sound = iterSoundMap->second;

		if (sound != NULL)
		{
			sound->setCurrentTime(milliSecond);
		}
	}
}

//-----------------------------------------------------------------------------
bool Audio::getCurrentSoundTime(SoundId const &soundId, int &milliSecond)
{
	bool result = false;
	const SoundIdToSoundMap::iterator iterSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundMap != s_soundIdToSoundMap.end())
	{
		Sound2 * const sound = iterSoundMap->second;

		if (sound != NULL)
		{
			result = true;
			milliSecond = sound->getCurrentTime();
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
bool Audio::getCurrentSoundTime(SoundId const &soundId, int &totalMilliSecond, int &milliSecond)
{
	bool result = false;
	const SoundIdToSoundMap::iterator iterSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundMap != s_soundIdToSoundMap.end())
	{
		Sound2 * const sound = iterSoundMap->second;

		if (sound != NULL)
		{
			result = true;
			milliSecond = sound->getCurrentTime();
			totalMilliSecond = sound->getTotalTime ();
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
bool Audio::getTotalSoundTime(SoundId const &soundId, int &milliSecond)
{
	bool result = false;
	const SoundIdToSoundMap::iterator iterSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundMap != s_soundIdToSoundMap.end())
	{
		Sound2 * const sound = iterSoundMap->second;

		if (sound != NULL)
		{
			result = true;
			milliSecond = sound->getTotalTime();
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
bool Audio::isSampleAtEnd(SoundId const &soundId)
{
	bool result = false;
	SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

	SoundIdToSoundMap::iterator iterSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundMap != s_soundIdToSoundMap.end())
	{
		Sound2 *sound = iterSoundMap->second;

		if (isSampleStream(sound->getSampleId(), iterSampleIdToSampleStreamMap))
		{
			HSTREAM stream = iterSampleIdToSampleStreamMap->second.m_stream;
			S32 millisecondsTotal = 0;
			S32 millisecondsCurrent = 0;

			AIL_stream_ms_position(stream, &millisecondsTotal, &millisecondsCurrent);

			result = (millisecondsTotal == millisecondsCurrent);
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
void Audio::getSampleTime(SampleId const &sampleId, float &timeTotal, float &timeCurrent)
{
	timeTotal = 0.0f;
	timeCurrent = 0.0f;

	if (s_installed)
	{
		SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
		SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
		SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

		if (isSample2d(sampleId, iterSampleIdToSample2dMap))
		{
			HSAMPLE sample = iterSampleIdToSample2dMap->second.m_sample;
			S32 millisecondsTotal = 0;
			S32 millisecondsCurrent = 0;

			AIL_sample_ms_position(sample, &millisecondsTotal, &millisecondsCurrent);

			timeTotal = static_cast<float>(millisecondsTotal) / 1000.0f;
			timeCurrent = static_cast<float>(millisecondsCurrent) / 1000.0f;
		}
		else if (isSample3d(sampleId, iterSampleIdToSample3dMap))
		{
			timeCurrent = 0.0f;

			SampleCache::const_iterator iterSampleCache = s_sampleCache.find(iterSampleIdToSample3dMap->second.getPath());
			DEBUG_WARNING((iterSampleCache == s_sampleCache.end()), ("Error finding the sample in the cache: %s", iterSampleIdToSample3dMap->second.getPath()->getString()));

			if (iterSampleCache != s_sampleCache.end())
			{
				timeTotal = iterSampleCache->second.m_time;

				int const sampleOffset = static_cast<int>(AIL_sample_position(iterSampleIdToSample3dMap->second.m_sample));
				int const sampleSize = iterSampleIdToSample3dMap->second.getFileSize();

				if (sampleSize > 0)
				{
					timeCurrent = timeTotal * (static_cast<float>(sampleOffset) / static_cast<float>(sampleSize));
				}
			}
		}
		else if (isSampleStream(sampleId, iterSampleIdToSampleStreamMap))
		{
			HSTREAM stream = iterSampleIdToSampleStreamMap->second.m_stream;
			S32 millisecondsTotal = 0;
			S32 millisecondsCurrent = 0;

			AIL_stream_ms_position(stream, &millisecondsTotal, &millisecondsCurrent);

			timeTotal = static_cast<float>(millisecondsTotal) / 1000.0f;
			timeCurrent = static_cast<float>(millisecondsCurrent) / 1000.0f;
		}
	}
}

//-----------------------------------------------------------------------------
int Audio::getSampleCurrentTime(SampleId const &sampleId)
{
	int result = 0;
	SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
	SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
	SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

	if (isSample2d(sampleId, iterSampleIdToSample2dMap))
	{
		S32 millisecondsTotal = 0;
		S32 millisecondsCurrent = 0;

		AIL_sample_ms_position(iterSampleIdToSample2dMap->second.m_sample, &millisecondsTotal, &millisecondsCurrent);

		result = millisecondsCurrent;
	}
	else if (isSample3d(sampleId, iterSampleIdToSample3dMap))
	{
		SampleCache::const_iterator iterSampleCache = s_sampleCache.find(iterSampleIdToSample3dMap->second.getPath());
		DEBUG_WARNING((iterSampleCache == s_sampleCache.end()), ("Error finding the sample in the cache: %s", iterSampleIdToSample3dMap->second.getPath()->getString()));

		if (iterSampleCache != s_sampleCache.end())
		{
			int const sampleOffset = static_cast<int>(AIL_sample_position(iterSampleIdToSample3dMap->second.m_sample));
			int const sampleSize = iterSampleIdToSample3dMap->second.getFileSize();

			if (sampleSize > 0)
			{
				result = static_cast<int>(iterSampleCache->second.m_time * (static_cast<float>(sampleOffset) / static_cast<float>(sampleSize)) * 1000.0f);
			}
		}
	}
	else if (isSampleStream(sampleId, iterSampleIdToSampleStreamMap))
	{
		S32 millisecondsTotal = 0;
		S32 millisecondsCurrent = 0;

		AIL_stream_ms_position(iterSampleIdToSampleStreamMap->second.m_stream, &millisecondsTotal, &millisecondsCurrent);

		result = millisecondsCurrent;
	}

	return result;
}

//-----------------------------------------------------------------------------
int Audio::getSampleTotalTime(SampleId const &sampleId)
{
	int result = 0;
	SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
	SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
	SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

	if (isSample2d(sampleId, iterSampleIdToSample2dMap))
	{
		S32 millisecondsTotal = 0;
		S32 millisecondsCurrent = 0;

		AIL_sample_ms_position(iterSampleIdToSample2dMap->second.m_sample, &millisecondsTotal, &millisecondsCurrent);

		result = millisecondsTotal;
	}
	else if (isSample3d(sampleId, iterSampleIdToSample3dMap))
	{
		SampleCache::const_iterator iterSampleCache = s_sampleCache.find(iterSampleIdToSample3dMap->second.getPath());
		DEBUG_WARNING((iterSampleCache == s_sampleCache.end()), ("Error finding the sample in the cache: %s", iterSampleIdToSample3dMap->second.getPath()->getString()));

		if (iterSampleCache != s_sampleCache.end())
		{
			result = static_cast<int>(iterSampleCache->second.m_time * 1000.0f);
		}
	}
	else if (isSampleStream(sampleId, iterSampleIdToSampleStreamMap))
	{
		S32 millisecondsTotal = 0;
		S32 millisecondsCurrent = 0;

		AIL_stream_ms_position(iterSampleIdToSampleStreamMap->second.m_stream, &millisecondsTotal, &millisecondsCurrent);

		result = millisecondsTotal;
	}

	return result;
}

//-----------------------------------------------------------------------------
void Audio::setSampleCurrentTime(SampleId const &sampleId, int const milliSeconds)
{
	SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
	SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
	SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

	if (isSample2d(sampleId, iterSampleIdToSample2dMap))
	{
		if (milliSeconds == 0)
		{
			AIL_start_sample(iterSampleIdToSample2dMap->second.m_sample);
		}
		else
		{
			AIL_set_sample_ms_position(iterSampleIdToSample2dMap->second.m_sample, milliSeconds);
		}
	}
	else if (isSample3d(sampleId, iterSampleIdToSample3dMap))
	{
		int const totalTime = getSampleTotalTime(sampleId);

		if (totalTime > 0)
		{
			float const percent = static_cast<float>(milliSeconds) / static_cast<float>(totalTime);
			float const sampleSize = static_cast<float>(getSampleSize(iterSampleIdToSample3dMap->second.getPath()->getString()));
			unsigned int offset = static_cast<unsigned int>(sampleSize * percent);

			offset -= offset % 4;

			AIL_set_sample_position(iterSampleIdToSample3dMap->second.m_sample, offset);
		}
		else
		{
			DEBUG_REPORT_LOG(true, ("Audio::setSampleCurrentTime() - Unable to set the requested 3d sample time.\n"));
		}
	}
	else if (isSampleStream(sampleId, iterSampleIdToSampleStreamMap))
	{
		if (milliSeconds == 0)
		{
			AIL_start_stream(iterSampleIdToSampleStreamMap->second.m_stream);
		}
		else
		{
			AIL_set_stream_ms_position(iterSampleIdToSampleStreamMap->second.m_stream, milliSeconds);
		}
	}
}

//-----------------------------------------------------------------------------
void getSampleTime(char const *path, byte *fileImage, int fileSize, float &timeTotal, float &timeCurrent)
{
	timeTotal = 0.0f;
	timeCurrent = 0.0f;

	if (s_installed && (fileSize > 0))
	{
		S32 total = 0;
		S32 current = 0;

		HSAMPLE sample = AIL_allocate_sample_handle(s_digitalDevice2d);

		if (sample != NULL)
		{

			SampleCacheEntry sampleCacheEntry;
			sampleCacheEntry.m_sampleRawData = fileImage;
			sampleCacheEntry.m_fileSize = fileSize;
			sampleCacheEntry.setExtension(path);

			if (sampleCacheEntry.m_sampleRawData != NULL)
			{
				S32 result = AIL_set_named_sample_file(sample, sampleCacheEntry.getExtension(), sampleCacheEntry.m_sampleRawData, sampleCacheEntry.m_fileSize, 0);

				if (result)
				{
					AIL_sample_ms_position(sample, &total, &current);
					AIL_end_sample(sample);
				}
				else if (s_audioDiagFile)
				{
					SYSTEMTIME st;
					GetLocalTime(&st);
					fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d TIMEPROBE-SETNAMEDFAIL %s ext=%s size=%d lastError=%s\n",
						st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
						path, sampleCacheEntry.getExtension(), fileSize, AIL_last_error());
					fflush(s_audioDiagFile);
				}
			}
			// CONSULT-63: release UNCONDITIONALLY. The original code released only on
			// set_named_sample_file success, leaking one driver sample handle (of
			// DIG_MIXER_CHANNELS=64) per failed duration probe -- invisible on 9.3b
			// where the bind never fails; on 7.2e the boot preload burst drained the
			// whole pool ("Unable to obtain a sample handle" on every open).
			AIL_release_sample_handle(sample);
		}
		else if (s_audioDiagFile)
		{
			SYSTEMTIME st;
			GetLocalTime(&st);
			fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d TIMEPROBE-ALLOCFAIL %s lastError=%s\n",
				st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, path, AIL_last_error());
			fflush(s_audioDiagFile);
		}

		timeTotal = static_cast<float>(total) / 1000.0f;
		timeCurrent = static_cast<float>(current) / 1000.0f;
	}
}

//-----------------------------------------------------------------------------
void Audio::getCurrentSample(SoundId const &soundId, TemporaryCrcString &path)
{
	path.clear();

	if (s_installed)
	{
		SoundIdToSoundMap::iterator current = s_soundIdToSoundMap.find(soundId);

		if (current != s_soundIdToSoundMap.end())
		{
			Sound2 *sound = current->second;

			if (sound->getSampleId().getId() != 0)
			{
				SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
				SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
				SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;

				if (isSample2d(sound->getSampleId(), iterSampleIdToSample2dMap))
				{
					CrcString const &crcString = *iterSampleIdToSample2dMap->second.getPath();

					path.set(crcString.getString(), crcString.getCrc());
				}
				else if (isSample3d(sound->getSampleId(), iterSampleIdToSample3dMap))
				{
					CrcString const &crcString = *iterSampleIdToSample3dMap->second.getPath();

					path.set(crcString.getString(), crcString.getCrc());
				}
				else if (isSampleStream(sound->getSampleId(), iterSampleIdToSampleStreamMap))
				{
					CrcString const &crcString = *iterSampleIdToSampleStreamMap->second.getPath();

					path.set(crcString.getString(), crcString.getCrc());
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
SoundIdToSoundMap::iterator getIterSoundIdToSoundMap(SoundId const &soundId)
{
#ifdef _DEBUG
	int const count = static_cast<int>(s_soundIdToSoundMap.size());
	UNREF(count);
#endif // _DEBUG

	SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.find(soundId);

	DEBUG_FATAL(s_installed && (iterSoundIdToSoundMap == s_soundIdToSoundMap.end()), ("Audio::getIterSoundIdToSoundMap - Trying to get a sound id [%i] that no longer exists.", soundId.getId()));

	return iterSoundIdToSoundMap;
}

////-----------------------------------------------------------------------------
//SoundIdToObjectAttachmentMap::iterator getIterSoundIdToObjectAttachmentMap(SoundId const &soundId)
//{
//	SoundIdToObjectAttachmentMap::iterator iterSoundIdToObjectAttachmentMap = s_soundIdToObjectAttachmentMap.find(soundId);
//
//	DEBUG_FATAL((iterSoundIdToObjectAttachmentMap == s_soundIdToObjectAttachmentMap.end()), ("Audio::getIterSoundIdToObjectAttachmentMap - Trying to get a sound id [%i] that no longer exists.", soundId.getId()));
//
//	return iterSoundIdToObjectAttachmentMap;
//}

//-----------------------------------------------------------------------------
SampleIdToSample3dMap::iterator getIterSampleIdToSample3dMap(SampleId const &sampleId)
{
	SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap = s_sampleIdToSample3dMap.find(sampleId);

	DEBUG_FATAL(s_installed && (iterSampleIdToSample3dMap == s_sampleIdToSample3dMap.end()), ("Audio::getIterSampleIdToSample3dMap - Trying to get a sample id [%i] of a 3d sample that no longer exists.", sampleId.getId()));

	return iterSampleIdToSample3dMap;
}

//-----------------------------------------------------------------------------
char const *getFileError()
{
	char const *result = "";

	switch (AIL_file_error())
	{
		case AIL_NO_ERROR:        { result = "No file errors have occurred"; } break;
		case AIL_IO_ERROR:        { result = "An unspecified I/O error has occurred"; } break;
		case AIL_OUT_OF_MEMORY:   { result = "Couldn't allocate enough memory."; } break;
		case AIL_FILE_NOT_FOUND:  { result = "The file you tried to read could not be found."; } break;
		case AIL_CANT_WRITE_FILE: { result = "Error writing to file."; } break;
		case AIL_CANT_READ_FILE:  { result = "Error reading from file."; } break;
		case AIL_DISK_FULL:       { result = "The drive you are writing to is full."; } break;
		default:                  { result = "Unknown error."; } break;
	}

	return result;
}

//-----------------------------------------------------------------------------
Vector const &Audio::getListenerPosition()
{
	return s_listener.m_positionCurrent;
}

//-----------------------------------------------------------------------------
int getNextSampleId()
{
	// Find the next not used sample id

	do
	{
		++s_nextSampleId;

		// Handle the wrap

		if (s_nextSampleId >= std::numeric_limits<int>::max())
		{
			s_nextSampleId = 1;
		}
	}
	while (Audio::isSampleValid(SampleId(s_nextSampleId)));

	return s_nextSampleId;
}

//-----------------------------------------------------------------------------
int getNextSoundId()
{
	// Find the next not used sound id

	do
	{
		++s_nextSoundId;

		// Handle the wrap

		if (s_nextSoundId >= std::numeric_limits<int>::max())
		{
			s_nextSoundId = 1;
		}
	}
	while (Audio::isSoundValid(SoundId(s_nextSoundId, "")));

	return s_nextSoundId;
}

//-----------------------------------------------------------------------------
bool Audio::isSoundPlaying(SoundId const &soundId)
{
	return isSoundValid(soundId);
}

//-----------------------------------------------------------------------------
bool Audio::getSoundWorldPosition(SoundId const &soundId, Vector &position)
{
	bool result = false;

	SoundIdToSoundMap::const_iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundIdToSoundMap != s_soundIdToSoundMap.end())
	{
		position = iterSoundIdToSoundMap->second->getPosition_w();

		result = true;
	}

	return result;
}

//-----------------------------------------------------------------------------
float Audio::getFallOffDistance(float const minDistance)
{
	float result = minDistance;

	for (int j = 1; j < s_soundFallOffPower; ++j)
	{
		result *= 2;
	}

	return result;
}

//-----------------------------------------------------------------------------
void Audio::setSoundFallOffPower(int const power)
{
	s_soundFallOffPower = clamp(3, power, 6);
}

////-----------------------------------------------------------------------------
//Audio::AttenuationMethod Audio::getAttenuationMethod(SoundId const &soundId)
//{
//	AttenuationMethod result = Audio::AM_none;
//
//	if (s_installed)
//	{
//		SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap;
//		SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap;
//		SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap;
//
//		if (isSample2d(sampleId, iterSampleIdToSample2dMap))
//		{
//			result = iterSampleIdToSample2dMap->second.m_sound->getTemplate()->m_attenuationMethod;
//		}
//		else if (isSample3d(sampleId, iterSampleIdToSample3dMap))
//		{
//			result = iterSampleIdToSample3dMap->second.m_sound->getTemplate()->m_attenuationMethod;
//		}
//		else if (isSampleStream(sampleId, iterSampleIdToSampleStreamMap))
//		{
//			result = iterSampleIdToSampleStreamMap->second.m_sound->getTemplate()->m_attenuationMethod;
//		}
//		else
//		{
//			DEBUG_WARNING(true, ("Trying to get the attenuation method of a sample that no longer exists."));
//		}
//	}
//
//	return result;
//}
//

//-----------------------------------------------------------------------------
bool Audio::getLoopOffsets(CrcString const &path, int &loopStartOffset, int &loopEndOffset)
{
	bool result = false;
	char const *fileName = path.getString();
	char const *currentPosition = fileName;
	bool done = false;

	do
	{
		currentPosition = strstr(fileName, "/");

		if (currentPosition != NULL)
		{
			fileName = currentPosition + 1;
		}
		else
		{
			done = true;
		}
	}
	while (!done);

	TemporaryCrcString crcString(fileName, true);

	MusicOffsetMap::const_iterator iterMusicDataTableIndexMap = s_musicOffsetMap.find(&crcString);

	if (iterMusicDataTableIndexMap != s_musicOffsetMap.end())
	{
		result = true;

		int const row = iterMusicDataTableIndexMap->second;
		loopStartOffset = s_musicDataTable->getIntValue(1, row);
		loopEndOffset = s_musicDataTable->getIntValue(2, row);
	}
	else
	{
		loopStartOffset = 0;
		loopEndOffset = -1;
	}

	return result;
}

//-----------------------------------------------------------------------------
void __stdcall endOfSample2dCallBack(HSAMPLE sample)
{
	SampleIdToSample2dMap::iterator iterSampleIdToSample2dMap = s_sampleIdToSample2dMap.begin();

	for (; iterSampleIdToSample2dMap != s_sampleIdToSample2dMap.end(); ++iterSampleIdToSample2dMap)
	{
		if (iterSampleIdToSample2dMap->second.m_sample == sample)
		{
			iterSampleIdToSample2dMap->second.m_status = Audio::PS_done;

			Sound2 *sound = iterSampleIdToSample2dMap->second.m_sound;
			NOT_NULL(sound);

			sound->endOfSample();

			break;
		}
	}
}

//-----------------------------------------------------------------------------
void __stdcall endOfSample3dCallBack(HSAMPLE sample)
{
	SampleIdToSample3dMap::iterator iterSampleIdToSample3dMap = s_sampleIdToSample3dMap.begin();

	for (; iterSampleIdToSample3dMap != s_sampleIdToSample3dMap.end(); ++iterSampleIdToSample3dMap)
	{
		if (iterSampleIdToSample3dMap->second.m_sample == sample)
		{
			iterSampleIdToSample3dMap->second.m_status = Audio::PS_done;

			Sound2 *sound = iterSampleIdToSample3dMap->second.m_sound;
			NOT_NULL(sound);

			sound->endOfSample();

			break;
		}
	}
}

//-----------------------------------------------------------------------------
void __stdcall endOfSampleStreamCallBack(HSTREAM stream)
{
	SampleIdToSampleStreamMap::iterator iterSampleIdToSampleStreamMap = s_sampleIdToSampleStreamMap.begin();

	for (; iterSampleIdToSampleStreamMap != s_sampleIdToSampleStreamMap.end(); ++iterSampleIdToSampleStreamMap)
	{
		if (iterSampleIdToSampleStreamMap->second.m_stream == stream)
		{
			iterSampleIdToSampleStreamMap->second.m_status = Audio::PS_done;

			Sound2 *sound = iterSampleIdToSampleStreamMap->second.m_sound;
			NOT_NULL(sound);

			// CONSULT-62 wave 4: does this Miles callback ever actually fire? The
			// finished theme stream sat done-but-unreleased for 30-90s every session,
			// which requires this whole path to be dead (Sound2d::reset releases
			// promptly once endOfSample is seen).
			if (s_audioDiagFile)
			{
				SYSTEMTIME st;
				GetLocalTime(&st);
				fprintf(s_audioDiagFile, "%02d:%02d:%02d.%03d EOS %s stream=%p\n",
					st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
					iterSampleIdToSampleStreamMap->second.getPath() ? iterSampleIdToSampleStreamMap->second.getPath()->getString() : "(null)",
					static_cast<void *>(stream));
				fflush(s_audioDiagFile);
			}

			sound->endOfSample();

			break;
		}
	}
}

//-----------------------------------------------------------------------------
void Audio::setMasterVolume(float const volume)
{
	s_masterVolume = clamp(0.0f, volume, 1.0f);

	//if (s_digitalDevice2d != NULL)
	//{
	//	AIL_set_digital_master_volume_level(s_digitalDevice2d, s_masterVolume);
	//
	//	float const dry = s_masterVolume;
	//	float const wet = 0.0f;
	//	AIL_set_digital_master_reverb_levels(s_digitalDevice2d, dry, wet);
	//}
}

//-----------------------------------------------------------------------------
float Audio::getMasterVolume()
{
	return s_masterVolume;
}

//-----------------------------------------------------------------------------
void Audio::setSoundEffectVolume(float const volume)
{
	setSoundCategoryVolume(SC_explosion, volume);
	setSoundCategoryVolume(SC_item, volume);
	setSoundCategoryVolume(SC_vehicle, volume);
	setSoundCategoryVolume(SC_vocalization, volume);
	setSoundCategoryVolume(SC_weapon, volume);
	setSoundCategoryVolume(SC_movement, volume);
	setSoundCategoryVolume(SC_machine, volume);
	setSoundCategoryVolume(SC_installation, volume);
}

//-----------------------------------------------------------------------------
float Audio::getSoundEffectVolume()
{
	return getSoundCategoryVolume(SC_explosion, true);
}

//-----------------------------------------------------------------------------
float Audio::getAmbientEffectVolume()
{
	return getSoundCategoryVolume(SC_ambient, true);
}

//-----------------------------------------------------------------------------
void Audio::setAmbientEffectVolume(float volume)
{
	setSoundCategoryVolume(SC_ambient, volume);
}

//-----------------------------------------------------------------------------
void Audio::setBackGroundMusicVolume(float const volume)
{
	setSoundCategoryVolume(SC_backGroundMusic, volume);
}

//-----------------------------------------------------------------------------
float Audio::getBackGroundMusicVolume()
{
	return getSoundCategoryVolume(SC_backGroundMusic, true);
}

//-----------------------------------------------------------------------------
void Audio::setPlayerMusicVolume(float const volume)
{
	setSoundCategoryVolume(SC_playerMusic, volume);
}

//-----------------------------------------------------------------------------
float Audio::getPlayerMusicVolume()
{
	return getSoundCategoryVolume(SC_playerMusic, true);
}

//-----------------------------------------------------------------------------
void Audio::setUserInterfaceVolume(float const volume)
{
	setSoundCategoryVolume(SC_userInterface, volume);
}

//-----------------------------------------------------------------------------
float Audio::getUserInterfaceVolume()
{
	return getSoundCategoryVolume(SC_userInterface, true);
}

//-----------------------------------------------------------------------------
void Audio::setRequestedMaxNumberOfSamples(int const max)
{
	s_requestedMaxNumberOfSamples = clamp(16, max, 64);
}

//-----------------------------------------------------------------------------
int Audio::getRequestedMaxNumberOfSamples()
{
	return s_requestedMaxNumberOfSamples;
}

//-----------------------------------------------------------------------------
int Audio::getMaxDigitalMixerChannels()
{
	return s_maxDigitalMixerChannels;
}

//-----------------------------------------------------------------------------
void Audio::setMaxCached2dSampleSize(int const bytes)
{
	int const min = 1024 * 64;
	int const max = 1024 * 512;

	s_maxCached2dSampleSize = clamp(min, bytes, max);
}

//-----------------------------------------------------------------------------
int Audio::getMaxCached2dSampleSize()
{
	return s_maxCached2dSampleSize;
}

//-----------------------------------------------------------------------------
Object const * const Audio::getListener()
{
	return s_listenerObject.getPointer();
}

//-----------------------------------------------------------------------------
void Audio::lock()
{
	AIL_lock();
}

//-----------------------------------------------------------------------------
void Audio::unLock()
{
	AIL_unlock();
}

//-----------------------------------------------------------------------------
void Audio::setAutoDelete(SoundId const &soundId, bool const autoDelete)
{
	SoundIdToSoundMap::iterator iterSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundMap != s_soundIdToSoundMap.end())
	{
		iterSoundMap->second->setAutoDelete(autoDelete);
	}
}

//-----------------------------------------------------------------------------
void Audio::serve()
{
	if (Os::isMainThread() &&
		(s_digitalDevice2d != NULL) &&
		(s_prioritizedPlayingSounds.size() > 0) &&
		(s_audioServePerformanceTimer != NULL))
	{
		static float deltaTime = 0.0f;
		s_audioServePerformanceTimer->stop();
		deltaTime += s_audioServePerformanceTimer->getElapsedTime();
		s_audioServePerformanceTimer->start();

		if (deltaTime > 0.05f)
		{
			deltaTime = 0.0f;
			AIL_serve();
		}
	}
}

//-----------------------------------------------------------------------------
void Audio::setEndOfSampleCallBack(SoundId const &soundId, EndOfSampleCallBack callBack)
{
	SoundIdToSoundMap::iterator iterSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundMap != s_soundIdToSoundMap.end())
	{
		iterSoundMap->second->setEndOfSampleCallBack(callBack);
	}
}

//----------------------------------------------------------------------

void Audio::getSoundsAttachedToObject (const Object & obj, SoundVector & sv)
{
	for (SoundIdToSoundMap::const_iterator it = s_soundIdToSoundMap.begin (); it != s_soundIdToSoundMap.end (); ++it)
	{
		Sound2 * const sound = (*it).second;

		if (sound && sound->getObject () == &obj)
			sv.push_back (sound);
	}
}

//----------------------------------------------------------------------

void Audio::transferOwnershipOfSounds(Object const & previousOwner, Object const & newOwner, Plane const * const /*partition*/)
{
	for (SoundIdToSoundMap::const_iterator it = s_soundIdToSoundMap.begin (); it != s_soundIdToSoundMap.end (); ++it)
	{
		Sound2 * const sound = (*it).second;
		if (NULL == sound || sound->getObject() != &previousOwner)
			continue;

		// @todo - handle partition
		sound->setObject(&newOwner);
	}
}


//----------------------------------------------------------------------

Sound2 * Audio::getSoundById (const SoundId & soundId)
{
	const SoundIdToSoundMap::iterator it = s_soundIdToSoundMap.find(soundId);

	if (it != s_soundIdToSoundMap.end())
		return (*it).second;

	return 0;
}

//-----------------------------------------------------------------------------
void Audio::setDebugEnabled(bool const enabled)
{
	s_debugVisuals = enabled;
}

//-----------------------------------------------------------------------------
bool Audio::isDebugEnabled()
{
	return s_debugVisuals;
}

//-----------------------------------------------------------------------------
float Audio::getObstruction()
{
	return s_obstruction;
}

//-----------------------------------------------------------------------------
float Audio::getOcclusion()
{
	return s_occlusion;
}

//-----------------------------------------------------------------------------
void Audio::setSoundPosition_w(SoundId const &soundId, Vector const &position_w)
{
	SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundIdToSoundMap != s_soundIdToSoundMap.end())
	{
		iterSoundIdToSoundMap->second->setPosition_w(position_w);
	}
}

//-----------------------------------------------------------------------------
void Audio::setSoundVolume(SoundId const &soundId, float volume)
{
	SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundIdToSoundMap != s_soundIdToSoundMap.end())
	{
		iterSoundIdToSoundMap->second->setUserVolume(volume);
	}
}

//-----------------------------------------------------------------------------
void Audio::setSoundPitchDelta(SoundId const &soundId, float pitchDelta)
{
	SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundIdToSoundMap != s_soundIdToSoundMap.end())
	{
		iterSoundIdToSoundMap->second->setUserPitchDelta(pitchDelta);
	}
}

//-----------------------------------------------------------------------------
Vector Audio::getSoundPosition_w(SoundId const &soundId)
{
	Vector result;

	SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundIdToSoundMap != s_soundIdToSoundMap.end())
	{
		result = iterSoundIdToSoundMap->second->getPosition_w();
	}

	return result;
}

//-----------------------------------------------------------------------------
float Audio::getDistanceAtVolumeCutOff(SoundId const &soundId)
{
	float result = 0.0f;

	SoundIdToSoundMap::iterator iterSoundIdToSoundMap = s_soundIdToSoundMap.find(soundId);

	if (iterSoundIdToSoundMap != s_soundIdToSoundMap.end())
	{
		result = iterSoundIdToSoundMap->second->getDistanceAtVolumeCutOff();
	}

	return result;
}

//-----------------------------------------------------------------------------
bool Audio::isToolApplication()
{
	return s_toolApplication;
}


//-----------------------------------------------------------------------------
void Audio::setToolApplication(bool const toolApplication)
{
	s_toolApplication = toolApplication;
}

//-----------------------------------------------------------------------------
void Audio::setLargePreMixBuffer()
{
	// Stock 64ms. CONSULT-62 postscript: 192ms was tried and did NOT reduce the
	// zone-in crackle (which therefore is NOT a <=192ms service-gap/mutex-hold class
	// during load -- stream compressed-refill starvation is the promoted suspect).
	// A mix-ahead >= the DIG_DS_FRAGMENT_CNT ring TotalMs (default 256ms, read at
	// device open) overruns unplayed audio = crackles; any large value also delays
	// every audible start/stop/fade by its own length during loading.
	// CONSULT-63: DIG_DS_MIX_FRAGMENT_CNT is in FRAGMENTS -- 1ms each on 9.3b but
	// 8ms each on 7.2e (DIG_DS_FRAGMENT_SIZE default). On 7.2e keep RAD stock
	// (8 frags = the same 64ms retail shipped with) instead of 64 frags = 512ms.
#if MSS_MAJOR_VERSION >= 9
	AIL_set_preference(DIG_DS_MIX_FRAGMENT_CNT, 64);
	AIL_serve();
#endif
}

//-----------------------------------------------------------------------------
void Audio::setNormalPreMixBuffer()
{
	// CONSULT-63: on 7.2e, s_bufferFragmentsMin (16, meaning 16ms at 9.3b's 1ms
	// fragments) would be 128ms of start-lag at 8ms fragments -- and forcing 2
	// fragments would starve the coarser 5ms-period 7.2e mixer. Retail ran stock
	// (8 frags / 64ms) in-game; leave it stock there.
#if MSS_MAJOR_VERSION >= 9
	AIL_set_preference(DIG_DS_MIX_FRAGMENT_CNT, s_bufferFragmentsMin);
	AIL_serve();
#endif
}

//-----------------------------------------------------------------------------
void Audio::silenceAllNonBackgroundMusic()
{
	//DEBUG_REPORT_LOG(true, ("Audio: Silence\n"));

	++s_nonBackgroundFadeCount;
}

//-----------------------------------------------------------------------------
void Audio::unSilenceAllNonBackgroundMusic()
{
	//DEBUG_REPORT_LOG(true, ("Audio: UnSilence\n"));

	--s_nonBackgroundFadeCount;
}

//-----------------------------------------------------------------------------
void Audio::fadeAllNonVoiceover()
{
	++s_nonVoiceoverFadeCount;
}

//-----------------------------------------------------------------------------
void Audio::unFadeAllNonVoiceover()
{
	--s_nonVoiceoverFadeCount;
}

//-----------------------------------------------------------------------------
void Audio::fadeAll()
{
	++s_allAudioFadeCount;
}

//-----------------------------------------------------------------------------
void Audio::unfadeAll()
{
	--s_allAudioFadeCount;
	if(s_allAudioFadeCount < 0)
	{
		DEBUG_WARNING(true, ("Audio fade all count below 0"));
		s_allAudioFadeCount = 0;
	}
}

//-----------------------------------------------------------------------------
float Audio::getDefaultMasterVolume()
{
	return 1.0f;
}

//-----------------------------------------------------------------------------
float Audio::getDefaultSoundEffectVolume()
{
	return 1.0f;
}

//-----------------------------------------------------------------------------
float Audio::getDefaultAmbientEffectVolume()
{
	return 1.0f;
}

//-----------------------------------------------------------------------------
float Audio::getDefaultBackGroundMusicVolume()
{
	return 0.75f;
}

//-----------------------------------------------------------------------------
float Audio::getDefaultPlayerMusicVolume()
{
	return 0.5f;
}

//-----------------------------------------------------------------------------
float Audio::getDefaultUserInterfaceVolume()
{
	return 0.75f;
}

void Audio::setFadeAllFactor(float factor)
{
	s_allAudioFadeFactor = factor;
}

float Audio::getFadeAllFactor()
{
	return s_allAudioFadeFactor;
}

float Audio::getDefaultFadeAllFactor()
{
	return 0.5f;
}

//-----------------------------------------------------------------------------
bool Audio::queueSample(Sound2 & sound, bool const soundIsAlreadyPlaying)
{
	bool result = true;
	DEBUG_FATAL(sound.getDistanceSquaredFromListener() > sqr(sound.getDistanceAtVolumeCutOff()), ("The sound is out of audible range, why is the sample trying to be queued?"));

	if (sound.getTemplate()->getAttenuationMethod() == AM_none)
	{
		if (!soundIsAlreadyPlaying)
		{
			s_centerBucket.push_back(&sound);
			//DEBUG_REPORT_LOG(true, ("Audio::queueSample() s_centerBucket.push_back1(%s)\n", sound.getTemplate()->getName()));
		}
		return result;
	}

	// Find the bucket to put the sound in

	Vector const listenerPosition_w((s_listenerObject != NULL) ? s_listenerObject->getPosition_w() : Vector::zero);
	Vector const position_w(sound.getPosition_w());
	float const distanceFromListenerSquared = listenerPosition_w.magnitudeBetweenSquared(position_w);

	if (distanceFromListenerSquared <= s_centerConsolidateDistanceSquared)
	{
		if (!soundIsAlreadyPlaying)
		{
			s_centerBucket.push_back(&sound);
			//DEBUG_REPORT_LOG(true, ("Audio::queueSample() s_centerBucket.push_back2(%s)\n", sound.getTemplate()->getName()));
		}
	}
	else if (s_listenerObject != NULL)
	{
		// Rotate the sound to the object space of the listener

		Vector const position_l(s_listenerObject->rotateTranslate_w2o(position_w));

		float const absolutePositionX_l = fabsf(position_l.x);
		float const absolutePositionZ_l = fabsf(position_l.z);

		if (position_l.z > 0.0f)
		{
			if (position_l.x > 0.0f)
			{
				if (position_l.x <= position_l.z)
				{
					// Front
					result = AudioNamespace::queueSample(s_frontBucket, sound, distanceFromListenerSquared, soundIsAlreadyPlaying);
				}
				else
				{
					// Right
					result = AudioNamespace::queueSample(s_rightBucket, sound, distanceFromListenerSquared, soundIsAlreadyPlaying);
				}
			}
			else
			{
				if (absolutePositionX_l <= position_l.z)
				{
					// Front
					result = AudioNamespace::queueSample(s_frontBucket, sound, distanceFromListenerSquared, soundIsAlreadyPlaying);
				}
				else
				{
					// Left
					result = AudioNamespace::queueSample(s_leftBucket, sound, distanceFromListenerSquared, soundIsAlreadyPlaying);
				}
			}
		}
		else
		{
			if (position_l.x > 0.0f)
			{
				if (position_l.x <= absolutePositionZ_l)
				{
					// Back
					result = AudioNamespace::queueSample(s_backBucket, sound, distanceFromListenerSquared, soundIsAlreadyPlaying);
				}
				else
				{
					// Right
					result = AudioNamespace::queueSample(s_rightBucket, sound, distanceFromListenerSquared, soundIsAlreadyPlaying);
				}
			}
			else
			{
				if (absolutePositionX_l <= absolutePositionZ_l)
				{
					// Back
					result = AudioNamespace::queueSample(s_backBucket, sound, distanceFromListenerSquared, soundIsAlreadyPlaying);
				}
				else
				{
					// Left
					result = AudioNamespace::queueSample(s_leftBucket, sound, distanceFromListenerSquared, soundIsAlreadyPlaying);
				}
			}
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
void Audio::playBufferedSound(char const * buffer, uint32 bufferLength, char const * const extension)
{
	if (!s_audioEnabled)
		return;

	if (!s_bufferedSoundSample)
		s_bufferedSoundSample = AIL_allocate_sample_handle(s_digitalDevice2d);
	else
		stopBufferedSound();

	AIL_set_named_sample_file(s_bufferedSoundSample, extension, buffer, bufferLength, 0);

	float const volume = s_masterVolume * getSoundCategoryVolume(SC_bufferedSound);

	AIL_set_sample_volume_levels(s_bufferedSoundSample, volume, volume);

	AIL_start_sample(s_bufferedSoundSample);
}

//-----------------------------------------------------------------------------
void Audio::playBufferedMusic(char const * buffer, uint32 bufferLength, char const * const extension)
{
	if (!s_audioEnabled)
		return;

	if (!s_bufferedMusicSample)
		s_bufferedMusicSample = AIL_allocate_sample_handle(s_digitalDevice2d);
	else
		stopBufferedMusic();

	AIL_set_named_sample_file(s_bufferedMusicSample, extension, buffer, bufferLength, 0);

	float const volume = s_masterVolume * getSoundCategoryVolume(SC_bufferedMusic);

	AIL_set_sample_volume_levels(s_bufferedMusicSample, volume, volume);

	AIL_start_sample(s_bufferedMusicSample);
}

//-----------------------------------------------------------------------------
void Audio::stopBufferedSound()
{
	if (s_bufferedSoundSample)
	{
		AIL_stop_sample(s_bufferedSoundSample);
		AIL_end_sample(s_bufferedSoundSample);
	}
}

//-----------------------------------------------------------------------------
void Audio::stopBufferedMusic()
{
	if (s_bufferedMusicSample)
	{
		AIL_stop_sample(s_bufferedMusicSample);
		AIL_end_sample(s_bufferedMusicSample);
	}
}

//-----------------------------------------------------------------------------
void Audio::setBufferedSoundVolume(float volume)
{
	if (s_bufferedSoundSample)
		AIL_set_sample_volume_levels(s_bufferedSoundSample, volume, volume);

	setSoundCategoryVolume(SC_bufferedSound, volume);
}

//-----------------------------------------------------------------------------
void Audio::setBufferedMusicVolume(float volume)
{
	if (s_bufferedMusicSample)
		AIL_set_sample_volume_levels(s_bufferedMusicSample, volume, volume);

	setSoundCategoryVolume(SC_bufferedMusic, volume);
}

//-----------------------------------------------------------------------------
void Audio::silenceNonBufferedMusic(bool silence)
{
	s_silenceNonBufferedMusic = silence;
}

// ============================================================================
