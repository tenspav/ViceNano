#include "rwcore.h"
#include "rpworld.h"
using namespace rw;

RwCamera    *RwCameraBeginUpdate(RwCamera * camera) { camera->beginUpdate(); return camera; }

RwCamera    *RwCameraEndUpdate(RwCamera * camera) { camera->endUpdate(); return camera; }

RwCamera    *RwCameraClear(RwCamera * camera, RwRGBA * colour, RwInt32 clearMode) { camera->clear(colour, clearMode); return camera; }

RwBool RwRenderStateSet(RwRenderState state, void *value)
{
	uint32 uival = (uintptr)value;
	uint32 fog;
	switch(state){
	case rwRENDERSTATETEXTURERASTER: SetRenderStatePtr(TEXTURERASTER, value); return true;
	case rwRENDERSTATETEXTUREADDRESS: SetRenderState(TEXTUREADDRESS, uival); return true;
	case rwRENDERSTATETEXTUREADDRESSU: SetRenderState(TEXTUREADDRESSU, uival); return true;
	case rwRENDERSTATETEXTUREADDRESSV: SetRenderState(TEXTUREADDRESSV, uival); return true;
	case rwRENDERSTATETEXTUREPERSPECTIVE: return true;
	case rwRENDERSTATEZTESTENABLE: SetRenderState(ZTESTENABLE, uival); return true;
	case rwRENDERSTATESHADEMODE: return true;
	case rwRENDERSTATEZWRITEENABLE: SetRenderState(ZWRITEENABLE, uival); return true;
	case rwRENDERSTATETEXTUREFILTER: SetRenderState(TEXTUREFILTER, uival); return true;
	case rwRENDERSTATESRCBLEND: SetRenderState(SRCBLEND, uival); return true;
	case rwRENDERSTATEDESTBLEND: SetRenderState(DESTBLEND, uival); return true;
	case rwRENDERSTATEVERTEXALPHAENABLE: SetRenderState(VERTEXALPHA, uival); return true;
	case rwRENDERSTATEBORDERCOLOR: return true;
	case rwRENDERSTATEFOGENABLE: SetRenderState(FOGENABLE, uival); return true;
	case rwRENDERSTATEFOGCOLOR:
		// have to swap R and B here
		fog = (uival>>16)&0xFF;
		fog |= (uival&0xFF)<<16;
		fog |= uival&0xFF00;
		fog |= uival&0xFF000000;
		SetRenderState(FOGCOLOR, fog);
		return true;
	case rwRENDERSTATEFOGTYPE: return true;
	case rwRENDERSTATEFOGDENSITY: return true;
	case rwRENDERSTATEFOGTABLE: return true;
	case rwRENDERSTATEALPHAPRIMITIVEBUFFER: return true;
	case rwRENDERSTATECULLMODE: SetRenderState(CULLMODE, uival); return true;

	// all unsupported
	case rwRENDERSTATESTENCILENABLE:
	case rwRENDERSTATESTENCILFAIL:
	case rwRENDERSTATESTENCILZFAIL:
	case rwRENDERSTATESTENCILPASS:
	case rwRENDERSTATESTENCILFUNCTION:
	case rwRENDERSTATESTENCILFUNCTIONREF:
	case rwRENDERSTATESTENCILFUNCTIONMASK:
	case rwRENDERSTATESTENCILFUNCTIONWRITEMASK:
	default:
		return true;
	}
}

RpAtomic *RpAtomicRender(RpAtomic * atomic) { atomic->render(); return atomic; }
