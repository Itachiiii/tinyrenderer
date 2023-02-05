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
    shadowbuffer = new float[shadowsize * shadowsize];
    for(int i = width * height; i--; zbuffer[i] = -std::numeric_limits<float>::max());
    for(int i = shadowsize * shadowsize; i--; shadowbuffer[i] = -std::numeric_limits<float>::max());

    lookat(eye,center,up);
    viewport(shadowsize/8, shadowsize/8, shadowsize*3/4, shadowsize*3/4);
    projection(-1.f/(eye-center).norm());

    // use pre-computed ao texture
    TGAImage frame(width, height, TGAImage::RGB);
    AOShader aoshader;
    aoshader.aoimage.read_tga_file("occlusion1.tga");
    aoshader.aoimage.flip_vertically();
    for (int i=0; i<model->nfaces(); i++) {
        vector<int> f = model->face(i);
        Vec3f screen_coord[3];
        for(int j = 0; j < 3; j++)
        {
            Vec4f tmp = aoshader.vertex(i,j);
            screen_coord[j] = Vec3f(tmp[0],tmp[1],tmp[2]);
        }
        triangle_bary(aoshader, screen_coord, shadowbuffer, frame);
    }
    frame.flip_vertically();
    frame.write_tga_file("result1.tga");
    return 0;
    
    const int niters = 1000;
    for(int iter = 1; iter <= niters; iter++)
    {
        for (int i=0; i<3; i++) up[i] = (float)rand()/(float)RAND_MAX;
        eye = rand_point_on_unit_sphere();
        eye.y = std::abs(eye.y);
        for(int i = width * height; i--; zbuffer[i] = -std::numeric_limits<float>::max());
        for(int i = shadowsize * shadowsize; i--; shadowbuffer[i] = -std::numeric_limits<float>::max());

        TGAImage frame(width, height, TGAImage::RGB);
        lookat(eye, center, up);
        viewport(width/8, height/8, width*3/4, height*3/4);
        projection(0);

        ZShader zshader;
        for (int i=0; i<model->nfaces(); i++) {
            vector<int> f = model->face(i);
            Vec3f screen_coord[3];
            for(int j = 0; j < 3; j++)
            {
                Vec4f tmp = zshader.vertex(i,j);
                screen_coord[j] = Vec3f(tmp[0],tmp[1],tmp[2]);
            }
            triangle_bary(zshader, screen_coord, shadowbuffer, frame);
        }
        frame.flip_vertically();
        frame.write_tga_file("framebuffer1.tga");

        Shader shader;
        occl.clear();
        for (int i=0; i<model->nfaces(); i++) {
            vector<int> f = model->face(i);
            Vec3f screen_coord[3];
            for(int j = 0; j < 3; j++)
            {
                Vec4f tmp = shader.vertex(i,j);
                screen_coord[j] = Vec3f(tmp[0],tmp[1],tmp[2]);
            }
            triangle_bary(shader, screen_coord, zbuffer, frame);
        }

        for (int i=0; i<1024; i++) {
            for (int j=0; j<1024; j++) {
                float tmp = total.get(i,j)[0];
                total.set(i, j, TGAColor((tmp*(iter-1)+occl.get(i,j)[0])/(float)iter+.5f));
            }
        }
    }
    total.flip_vertically();
    total.write_tga_file("occlusion1.tga");
    occl.flip_vertically();
    occl.write_tga_file("occl1.tga");

    delete [] zbuffer;
    delete model;
    delete [] shadowbuffer;
}

