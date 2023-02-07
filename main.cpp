#include "tgaimage.h"
#include "model.h"
#include "our_gl.h"
#include <iostream>
#include <limits>
#include <cmath>
#include <cstdlib>
using namespace std;

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green = TGAColor(0, 255,   0,   255);
const int width = 800;
const int height = 800;
const int shadowsize = 800;
Model* model = nullptr;
float* shadowbuffer = nullptr;
float* zbuffer = nullptr;

Vec3f light_dir(1,1,0);
Vec3f   eye(1.2,-.8,3);
Vec3f    center(0,0,0);
Vec3f        up(0,1,0);

TGAImage total(1024, 1024, TGAImage::GRAYSCALE);
TGAImage  occl(1024, 1024, TGAImage::GRAYSCALE);

Vec3f rand_point_on_unit_sphere() {
    float u = (float)rand()/(float)RAND_MAX;
    float v = (float)rand()/(float)RAND_MAX;
    float theta = 2.f*M_PI*u;
    float phi   = acos(2.f*v - 1.f);
    return Vec3f(sin(phi)*cos(theta), sin(phi)*sin(theta), cos(phi));
}

struct ZShader : public IShader {
    mat<4,3,float> varying_tri;

    Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = Viewport*ModelView*embed<4>(model->vert(iface,nthvert));
        varying_tri.set_col(nthvert, gl_Vertex);
        return gl_Vertex;
    }

    bool fragment(Vec3f bar, TGAColor &color) {
        Vec4f vppos = varying_tri * bar;
        color = TGAColor(255, 255, 255)*((vppos[2])/250.f);
        return false;
    }
};

struct SSZShader : public IShader {
    mat<4,3,float> varying_tri;

    Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = ModelView*embed<4>(model->vert(iface,nthvert));
        varying_tri.set_col(nthvert, gl_Vertex);
        gl_Vertex =  Viewport * Projection* gl_Vertex;
        gl_Vertex = gl_Vertex / gl_Vertex[3];
        return gl_Vertex;
    }

    bool fragment(Vec3f bar, TGAColor &color) {
        color = TGAColor(0, 0, 0);
        return false;
    }
};

float max_elevation_angle(float* zbuffer, Vec2f start, Vec2f dir)
{
    float max_angle = 0.f;
    Vec2f nowpos = start;
    int sx = nowpos[0], sy = nowpos[1];
    for(float t = .0f; t <= 1000.f; t += 1.f)
    {
        nowpos = start + dir * t;
        int x = nowpos[0], y = nowpos[1];
        if(x >= width || x < 0 || y < 0 || y >= height) break;
        float distance = (nowpos - start).norm();
        if(distance <= 1)continue;
        float zsub = zbuffer[x + y * width] - zbuffer[sx + sy * width];
        max_angle = max(max_angle, atanf(zsub/distance));
    }
    return max_angle;
}

struct Shader : public IShader {
    mat<2,3,float> varying_uv;
    mat<4,3,float> varying_tri;

    virtual Vec4f vertex(int iface, int nthvert) {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = Viewport*ModelView*embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, gl_Vertex);
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color) {
        Vec2f uv = varying_uv*bar;
        Vec4f vppos = varying_tri * bar;
        if (std::abs(shadowbuffer[int(vppos[0])+int(vppos[1])*width]-vppos[2])<1e-2) {
            occl.set(uv.x*1024, uv.y*1024, TGAColor(255));
        }
        color = TGAColor(255, 0, 0);
        return false;
    }
};

struct AOShader : public IShader {
    mat<2,3,float> varying_uv;
    mat<4,3,float> varying_tri;
    TGAImage aoimage;

    virtual Vec4f vertex(int iface, int nthvert) {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = Viewport*Projection*ModelView*embed<4>(model->vert(iface, nthvert));
        gl_Vertex = gl_Vertex / gl_Vertex[3];
        varying_tri.set_col(nthvert, gl_Vertex);
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color) {
        Vec2f uv = varying_uv*bar;
        int t = aoimage.get(uv.x*1024, uv.y*1024)[0];
        color = TGAColor(t, t, t);
        return false;
    }
};

int main(int argc, char** argv) {
    TGAImage image(width, height, TGAImage::RGB);
    TGAImage shadowmap(shadowsize, shadowsize, TGAImage::GRAYSCALE);

    model = new Model("obj/diablo3_pose.obj");
    light_dir.normalize();
    zbuffer = new float[width * height];
    for(int i = width * height; i--; zbuffer[i] = -std::numeric_limits<float>::max());

    TGAImage frame(width, height, TGAImage::RGB);
    lookat(eye,center,up);
    viewport(width/8, height/8, width*3/4, height*3/4);
    projection(-1.f/(eye-center).norm());

    SSZShader zshader;
    for (int i=0; i<model->nfaces(); i++) {
        vector<int> f = model->face(i);
        Vec3f screen_coord[3];
        for(int j = 0; j < 3; j++)
        {
            Vec4f tmp = zshader.vertex(i,j);
            screen_coord[j] = Vec3f(tmp[0],tmp[1],tmp[2]);
        }
        triangle_bary(zshader, screen_coord, zbuffer, frame, zshader.varying_tri);
        //triangle_ans(zshader.varying_tri, zshader, frame, zbuffer);
    }

    for (int x=0; x<width; x++) {
        for (int y=0; y<height; y++) {
            if (zbuffer[x+y*width] < -1e5) continue;
            
            float total = 0;
            for(float angle = 0; angle < 2 * M_PI-1e-4; angle += (M_PI / 4)){
                total += (M_PI / 2 - max_elevation_angle(zbuffer, Vec2f(x,y), Vec2f(cos(angle), sin(angle))));
            }
            
            total /= 8 * (M_PI / 2);
            total = pow(total, 100.0f);
            frame.set(x, y, TGAColor(total*255, total*255, total*255));
        }
    }

    frame.flip_vertically();
    frame.write_tga_file("framebuffer1.tga");
    
    delete [] zbuffer;
    delete model;
    return 0;
}

