#pragma once
namespace rw {
#ifdef RW_GLES1
struct EngineOpenParams { int width, height; };
#endif
namespace gles1 {
extern Device renderdevice;
void registerPlatformPlugins();
ObjPipeline *makeDefaultPipeline();
void freeInstanceData(Geometry*);
struct NativeRaster { uint32 texture; uint8 *data; int32 bytes; };
extern int32 nativeRasterOffset;
NativeRaster *nativeRaster(Raster*);
void applyStates();
void loadWorld(Matrix*);
void restore3D();
void im2DRenderLine(void*,int32,int32,int32);
void im2DRenderTriangle(void*,int32,int32,int32,int32);
void im2DRenderPrimitive(PrimitiveType,void*,int32);
void im2DRenderIndexedPrimitive(PrimitiveType,void*,int32,void*,int32);
void im3DTransform(void*,int32,Matrix*,uint32);
void im3DRenderPrimitive(PrimitiveType);
void im3DRenderIndexedPrimitive(PrimitiveType,void*,int32);
void im3DEnd();
extern uint32 unsupportedStates;
struct Im3DVertex
{
	V3d     position;
	V3d     normal;		// librw extension
	uint8   r, g, b, a;
	float32 u, v;

	void setX(float32 x) { this->position.x = x; }
	void setY(float32 y) { this->position.y = y; }
	void setZ(float32 z) { this->position.z = z; }
	void setNormalX(float32 x) { this->normal.x = x; }
	void setNormalY(float32 y) { this->normal.y = y; }
	void setNormalZ(float32 z) { this->normal.z = z; }
	void setColor(uint8 r, uint8 g, uint8 b, uint8 a) {
		this->r = r; this->g = g; this->b = b; this->a = a; }
	void setU(float32 u) { this->u = u; }
	void setV(float32 v) { this->v = v; }

	float getX(void) { return this->position.x; }
	float getY(void) { return this->position.y; }
	float getZ(void) { return this->position.z; }
	float getNormalX(void) { return this->normal.x; }
	float getNormalY(void) { return this->normal.y; }
	float getNormalZ(void) { return this->normal.z; }
	RGBA getColor(void) { return makeRGBA(this->r, this->g, this->b, this->a); }
	float getU(void) { return this->u; }
	float getV(void) { return this->v; }
};


struct Im2DVertex
{
	float32 x, y, z, w;
	uint8   r, g, b, a;
	float32 u, v;

	void setScreenX(float32 x) { this->x = x; }
	void setScreenY(float32 y) { this->y = y; }
	void setScreenZ(float32 z) { this->z = z; }
	// This is a bit unefficient but we have to counteract GL's divide, so multiply
	void setCameraZ(float32 z) { this->w = z; }
	void setRecipCameraZ(float32 recipz) { this->w = 1.0f/recipz; }
	void setColor(uint8 r, uint8 g, uint8 b, uint8 a) {
		this->r = r; this->g = g; this->b = b; this->a = a; }
	void setU(float32 u, float recipz) { this->u = u; }
	void setV(float32 v, float recipz) { this->v = v; }

	float getScreenX(void) { return this->x; }
	float getScreenY(void) { return this->y; }
	float getScreenZ(void) { return this->z; }
	float getCameraZ(void) { return this->w; }
	float getRecipCameraZ(void) { return 1.0f/this->w; }
	RGBA getColor(void) { return makeRGBA(this->r, this->g, this->b, this->a); }
	float getU(void) { return this->u; }
	float getV(void) { return this->v; }
};


}
}
