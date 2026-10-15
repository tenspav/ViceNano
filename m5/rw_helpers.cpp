#include "../vendor/revc-viewer/fakerw/rwcore.h"
#include "../vendor/revc-viewer/fakerw/rpworld.h"
RpAtomic*
GetFirstAtomicCallback(RpAtomic *atm, void *data)
{
	*(RpAtomic**)data = atm;
	return nil;
}

RpAtomic*
GetFirstAtomic(RpClump *clump)
{
	RpAtomic *atm;

	atm = nil;
	RpClumpForAllAtomics(clump, GetFirstAtomicCallback, &atm);
	return atm;
}

RwTexture*
GetFirstTextureCallback(RwTexture *tex, void *data)
{
	*(RwTexture**)data = tex;
	return nil;
}

RwTexture*
GetFirstTexture(RwTexDictionary *txd)
{
	RwTexture *tex;

	tex = nil;
	RwTexDictionaryForAllTextures(txd, GetFirstTextureCallback, &tex);
	return tex;
}

