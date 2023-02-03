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
Model* model = nullptr;

Vec3f light_dir(1,1,1);
Vec3f       eye(1,1,3);
Vec3f    center(0,0,0);
Vec3f        up(0,1,0);

struct GouraudShader : public IShader {
    Vec3f varying_intensity; // written by vertex shader, read by fragment shader

    Vec4f vertex(int iface, int nthvert) {
        varying_intensity[nthvert] = max(0.f, light_dir * model->normal(iface, nthvert));
        Vec4f gl_Vertex = embed<4>(model->vert(iface,nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        gl_Vertex = gl_Vertex / gl_Vertex[3];
        return gl_Vertex;
    }

    bool fragment(Vec3f bar, TGAColor &color) {
        float intensity = varying_intensity * bar;  // no, we do not discard this pixel
        color = TGAColor(255,255,255) * intensity;
        return false;
    }
};

struct DebugShader : public IShader {
    Vec3f varying_intensity; // written by vertex shader, read by fragment shader

    Vec4f vertex(int iface, int nthvert) {
        varying_intensity[nthvert] = max(0.f, light_dir * model->normal(iface, nthvert));
        Vec4f gl_Vertex = embed<4>(model->vert(iface,nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        gl_Vertex = gl_Vertex / gl_Vertex[3];
        return gl_Vertex;
    }

    bool fragment(Vec3f bar, TGAColor &color) {
        float intensity = varying_intensity*bar;
        if (intensity>.85) intensity = 1;
        else if (intensity>.60) intensity = .80;
        else if (intensity>.45) intensity = .60;
        else if (intensity>.30) intensity = .45;
        else if (intensity>.15) intensity = .30;
        else intensity = 0;
        color = TGAColor(255, 155, 0)*intensity;
        return false;
    }
};

struct Shader : public IShader {
    Vec3f varying_intensity; // written by vertex shader, read by fragment shader
    mat<2,3,float> varying_uv; // written by vertex shader, read by fragment shader

    Vec4f vertex(int iface, int nthvert) {
        varying_intensity[nthvert] = max(0.f, light_dir * model->normal(iface, nthvert));
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = embed<4>(model->vert(iface,nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        gl_Vertex = gl_Vertex / gl_Vertex[3];
        return gl_Vertex;
    }

    bool fragment(Vec3f bar, TGAColor &color) {
        float intensity = varying_intensity*bar;
        Vec2f uv = varying_uv * bar;
        color = model->diffuse(uv) * intensity;
        return false;
    }
};

struct DetailedShader : public IShader {
    mat<2,3,float> varying_uv; // written by vertex shader, read by fragment shader

    Vec4f vertex(int iface, int nthvert) {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = embed<4>(model->vert(iface,nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        gl_Vertex = gl_Vertex / gl_Vertex[3];
        return gl_Vertex;
    }

    bool fragment(Vec3f bar, TGAColor &color) {
        Vec2f uv = varying_uv * bar;
        Vec3f n = proj<3>(MIT * embed<4>(model->normal(uv))).normalize();
        Vec3f l = proj<3>(ModelView * embed<4>(light_dir)).normalize();
        float intensity = max(0.f, l * n);
        color = model->diffuse(uv) * intensity;
        return false;
    }
};

int main(int argc, char** argv) {
    TGAImage image(width, height, TGAImage::RGB);

    model = new Model("obj/african_head.obj");
    float* zbuffer = new float[width * height];
    for(int i = width * height; i--; zbuffer[i] = -std::numeric_limits<float>::max());
    
    lookat(eye,center,Vec3f(0,1,0));
    viewport(width/8, height/8, width*3/4, height*3/4);
    projection(-1.f/(eye - center).norm());
    light_dir.normalize();

    DetailedShader shader;
    MIT = ModelView.invert_transpose();
    for(int i = 0; i < model->nfaces(); i++)
    {
        vector<int> f = model->face(i);
        Vec3f world_coord[3];
        Vec3f screen_coord[3];
        //-----------------------------vertex processing-----------------------------
        for(int j = 0; j < 3; j++)
        {
            world_coord[j] = model->vert(f[j]);
            Vec4f tmp = shader.vertex(i,j);
            screen_coord[j] = Vec3f(tmp[0],tmp[1],tmp[2]);
        }

        //-----------------------------primitive processing-----------------------------
        triangle_bary(shader, screen_coord, zbuffer, image, width);
    }

    image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}

