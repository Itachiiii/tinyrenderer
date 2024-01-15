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
Vec3f   eye(0.5,1,2);
Vec3f    center(0,0,1);
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

struct PhongShader : public IShader {
    mat<2,3,float> varying_uv; // written by vertex shader, read by fragment shader
    mat<4,3,float> varying_view_pos;

    Vec4f vertex(int iface, int nthvert) {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = embed<4>(model->vert(iface,nthvert));
        varying_view_pos.set_col(nthvert, (ModelView * gl_Vertex));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        gl_Vertex = gl_Vertex / gl_Vertex[3];
        return gl_Vertex;
    }

    bool fragment(Vec3f bar, TGAColor &color) {
        Vec2f uv = varying_uv * bar;
        Vec3f view_pos = proj<3>(varying_view_pos * bar);
        Vec3f n = proj<3>(MIT * embed<4>(model->normal(uv))).normalize();
        Vec3f l = proj<3>(ModelView * embed<4>(light_dir)).normalize(); // to the light
        Vec3f r = n * 2 * (n * l) - l;
        Vec3f v = (eye - view_pos).normalize();
        float specular = pow( std::max(0.f, r * v), model->specular(uv));
        float diffuse = std::max(0.f, l * n);
        TGAColor c = model->diffuse(uv);
        color = c;
        for (int i=0; i<3; i++) color[i] = std::min<float>(5 + c[i]*(diffuse + .6*specular), 255);
        return false;
    }
};


int main(int argc, char** argv) {
    TGAImage image(width, height, TGAImage::RGB);

    model = new Model("obj/floor.obj");
    float* zbuffer = new float[width * height];
    for(int i = width * height; i--; zbuffer[i] = -std::numeric_limits<float>::max());
    
    lookat(eye,center,Vec3f(0,1,0));
    viewport(width/8, height/8, width*3/4, height*3/4);
    projection(-1.f/(eye - center).norm());
    light_dir.normalize();

    PhongShader shader;
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
        triangle_bary(shader, screen_coord, zbuffer, image, shader.varying_view_pos);
    }

    image.flip_vertically(); 
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}

