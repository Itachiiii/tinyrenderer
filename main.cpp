#include "tgaimage.h"
#include "model.h"
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

Vec3f light_dir = Vec3f(1,-1,1).normalize();
Vec3f eye(1,1,3);
Vec3f center(0,0,0);

const int depth = 255;
Matrix Projection,ModelView,Viewport;

void viewport(int x, int y, int w, int h) {
    Viewport = Matrix::identity();
    Viewport[0][3] = x+w/2.f;
    Viewport[1][3] = y+h/2.f;
    Viewport[2][3] = depth/2.f;
    Viewport[0][0] = w/2.f;
    Viewport[1][1] = h/2.f;
    Viewport[2][2] = depth/2.f;
}

void projection(float coeff) {
    Projection = Matrix::identity();
    Projection[3][2] = coeff;
}

void lookat(Vec3f eye, Vec3f center, Vec3f up) {
    Vec3f z = (eye-center).normalize();
    Vec3f x = cross(up,z).normalize();
    Vec3f y = cross(z,x).normalize();
    ModelView = Matrix::identity();
    for (int i=0; i<3; i++) {
        ModelView[0][i] = x[i];
        ModelView[1][i] = y[i];
        ModelView[2][i] = z[i];
        ModelView[i][3] = -center[i];
    }
}

Vec3f barycentric(Vec3f* pts, Vec3f p)
{
    Vec2f a((p - pts[0]).x, (p - pts[0]).y);
    Vec2f b((pts[1] - pts[0]).x,(pts[1] - pts[0]).y);
    Vec2f c((pts[2] - pts[0]).x,(pts[2] - pts[0]).y);
    float x1 = a.x, y1 = a.y;
    float x2 = b.x, y2 = b.y;
    float x3 = c.x, y3 = c.y;
    float u = (x3 * y1 - x1 * y3) / (y2 * x3 - x2 * y3);
    float v = (x2 * y1 - x1 * y2) / (x2 * y3 - x3 * y2);
    return Vec3f(1.f - u - v, u, v);
}

void triangle_bary(Vec3f* pts, float* zbuffer, TGAImage &image, float* intensity)
{
    Vec2i bboxmin(image.get_width() - 1, image.get_height() - 1);
    Vec2i bboxmax(0, 0);
    for(int i = 0; i < 3; i++)
    {
        bboxmin.x = std::max(0, std::min(bboxmin.x, (int)pts[i].x));
        bboxmin.y = std::max(0, std::min(bboxmin.y, (int)pts[i].y));
        bboxmax.x = std::min(image.get_width()  - 1,  std::max(bboxmax.x, (int)pts[i].x));
        bboxmax.y = std::min(image.get_height() - 1,  std::max(bboxmax.y, (int)pts[i].y));
    }
    for(int x = bboxmin.x; x <= bboxmax.x; x++)
    {
        for(int y = bboxmin.y; y <= bboxmax.y; y++)
        {
            Vec3f bc = barycentric(pts, Vec3f(x+0.5f, y+0.5f, 1));
            if(bc.x < 0 || bc.y < 0 || bc.z < 0)continue;
            float z = 0;
            z = pts[0].z * bc.x + pts[1].z * bc.y + pts[2].z * bc.z;
            float _intensity = intensity[0] * bc.x + intensity[1] * bc.y + intensity[2] * bc.z;
            TGAColor color = TGAColor(255,255,255) * _intensity;
            if(z > zbuffer[x + y * width]){
                zbuffer[x + y * width] = z;
                image.set(x, y, color);
            }
        }
    }
}

int main(int argc, char** argv) {
    TGAImage image(width, height, TGAImage::RGB);
    
    model = new Model("obj/african_head.obj");
    float* zbuffer = new float[width * height];
    for(int i = width * height; i--; zbuffer[i] = -std::numeric_limits<float>::max());

    lookat(eye,center,Vec3f(0,1,0));
    viewport(width/8, height/8, width*3/4, height*3/4);
    projection(-1.f/(eye - center).norm());

    for(int i = 0; i < model->nfaces(); i++)
    {
        vector<int> f = model->face(i);
        Vec3f world_coord[3];
        Vec3f screen_coord[3];
        float intensity[3];
        for(int j = 0; j < 3; j++)
        {
            world_coord[j] = model->vert(f[j]);
            Vec4f tmp = embed<4>(world_coord[j]);
            Vec4f tmp2 = Viewport*Projection*ModelView*tmp;
            tmp2 = tmp2 / tmp2[3];
            screen_coord[j] = Vec3f(tmp2[0], tmp2[1], tmp2[2]);
            intensity[j] = light_dir * model->normal(i,j);
        }
        //method1
        triangle_bary(screen_coord, zbuffer, image, intensity);

        // method 2
        // triangle(Vec2i(screen_coord[0].x, screen_coord[0].y), 
        //         Vec2i(screen_coord[1].x, screen_coord[1].y),
        //         Vec2i(screen_coord[2].x, screen_coord[2].y),
        //         image, TGAColor(intensity*255, intensity*255, intensity*255, 255));
    }

    image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}

