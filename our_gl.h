#ifndef __OUR_GL_H__
#define __OUR_GL_H__

#include "tgaimage.h"
#include "geometry.h"

extern Matrix ModelView;
extern Matrix Viewport;
extern Matrix Projection;
extern Matrix MIT;
extern Matrix MI;

void viewport(int x, int y, int w, int h);
void projection(float coeff=0.f); // coeff = -1/c
void lookat(Vec3f eye, Vec3f center, Vec3f up);

struct IShader {
    virtual ~IShader();
    virtual Vec4f vertex(int iface, int nthvert) = 0;
    virtual bool fragment(Vec3f bar, TGAColor &color) = 0;
};

void triangle_ans(mat<4,3,float> &clipc, IShader &shader, TGAImage &image, float *zbuffer);
void triangle2(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage &image, TGAColor color);
void triangle_bary(IShader& shader, Vec3f* pts, float* zbuffer, TGAImage &image, mat<4,3,float>& clipc);


#endif //__OUR_GL_H__