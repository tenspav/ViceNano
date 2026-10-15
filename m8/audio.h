#pragma once
#include <stdint.h>
enum eAudioType
{
	AUDIOTYPE_PHYSICAL = 0,
	AUDIOTYPE_EXPLOSION,
	AUDIOTYPE_FIRE,
	AUDIOTYPE_WEATHER,
	AUDIOTYPE_SCRIPTOBJECT,
#ifdef GTA_BRIDGE
	AUDIOTYPE_BRIDGE,
#endif
	AUDIOTYPE_COLLISION,
	AUDIOTYPE_FRONTEND,
	AUDIOTYPE_PROJECTILE,
	AUDIOTYPE_GARAGE,
	AUDIOTYPE_FIREHYDRANT,
	AUDIOTYPE_WATERCANNON,
	AUDIOTYPE_ESCALATOR,
	AUDIOTYPE_EXTRA_SOUNDS,
	AUDIOTYPE_POLICERADIO,
	TOTAL_AUDIO_TYPES,
};
// Reduced DMAudio contract: retained call names/argument widths. No full AudioManager.
class cDMAudio {
public:
 void Initialise(); void Terminate(); void Service();
 int32_t CreateEntity(eAudioType type,void *uid); void DestroyEntity(int32_t entity);
 void PlayOneShot(int32_t entity,uint16_t sound,float volume);
 void PlayFrontEndSound(uint16_t sound,uint32_t volume);
 void PlayFrontEndTrack(uint32_t track,uint8_t frontend); void StopFrontEndTrack();
 void SetRadioChannel(uint32_t station,int32_t positionMs);
 void SetEffectsMasterVolume(uint8_t volume); void SetMusicMasterVolume(uint8_t volume);
 uint8_t IsAudioInitialised();
};
extern cDMAudio DMAudio;
struct AudioStats {uint32_t blocks,frames,mixUs,maxMixUs,voicesPeak,starts,stops,radioReads,radioEnds,failures,live,peak,cycles,heapLow,heapHigh,late;};
extern AudioStats audioStats;
void AudioMix(int16_t *output,unsigned frames);
void AudioTestTick();
