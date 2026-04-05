// scenes/protopirate_scene_config.h

#include "../defines.h"

ADD_SCENE(protopirate, start, Start)
#ifdef ENABLE_SUB_DECODE_SCENE
ADD_SCENE(protopirate, sub_decode, SubDecode)
#endif
ADD_SCENE(protopirate, about, About)
ADD_SCENE(protopirate, receiver, Receiver)
ADD_SCENE(protopirate, receiver_config, ReceiverConfig)
ADD_SCENE(protopirate, receiver_info, ReceiverInfo)
ADD_SCENE(protopirate, saved, Saved)
ADD_SCENE(protopirate, saved_info, SavedInfo)
#ifdef ENABLE_EMULATE_FEATURE
ADD_SCENE(protopirate, emulate, Emulate)
#endif
#ifdef ENABLE_TIMING_TUNER_SCENE
ADD_SCENE(protopirate, timing_tuner, TimingTuner)
#endif
