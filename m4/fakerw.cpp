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

RwTexture *RwTextureCreate(RwRaster * raster) { return Texture::create(raster); }

RwBool RwTextureDestroy(RwTexture * texture) { texture->destroy(); return true; }

RwTexture *RwTextureSetName(RwTexture * texture, const RwChar * name) { strncpy(texture->name, name, 32); return texture; }

RwTexDictionary *RwTexDictionaryCreate(void) { return TexDictionary::create(); }

RwBool RwTexDictionaryDestroy(RwTexDictionary * dict) { dict->destroy(); return true; }

RwTexture *RwTexDictionaryAddTexture(RwTexDictionary * dict, RwTexture * texture) { dict->addFront(texture); return texture; }

RwTexture *RwTexDictionaryFindNamedTexture(RwTexDictionary * dict, const RwChar * name) { return dict->find(name); }

RwTexDictionary *RwTexDictionaryGetCurrent(void) { return TexDictionary::getCurrent(); }

RwTexDictionary *RwTexDictionarySetCurrent(RwTexDictionary * dict) { TexDictionary::setCurrent(dict); return dict; }

const RwTexDictionary *RwTexDictionaryForAllTextures(const RwTexDictionary * dict, RwTextureCallBack fpCallBack, void *pData) {
	FORLIST(lnk, ((RwTexDictionary*)dict)->textures)
		if(fpCallBack(Texture::fromDict(lnk), pData) == nil)
			break;
	return dict;
}

RwFrame *RpClumpGetFrame(const RpClump * clump) { return clump->getFrame(); }

RpClump *RpClumpSetFrame(RpClump * clump, RwFrame * frame) { clump->setFrame(frame); return clump; }

RpClump *RpClumpForAllAtomics(RpClump * clump, RpAtomicCallBack callback, void *pData) {
	FORLIST(lnk, clump->atomics)
		if(callback(Atomic::fromClump(lnk), pData) == nil)
			break;
	return clump;
}

RpClump *RpClumpRender(RpClump * clump) { clump->render(); return clump; }

RpClump *RpClumpAddAtomic(RpClump * clump, RpAtomic * atomic) { clump->addAtomic(atomic); return clump; }

RwBool RpClumpDestroy(RpClump * clump) { clump->destroy(); return true; }

RpClump *RpClumpCreate(void) { return rw::Clump::create(); }

RwInt32 RpClumpGetNumAtomics(RpClump * clump) { return clump->countAtomics(); }
