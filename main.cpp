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

void line(int x0, int y0, int x1, int y1, TGAImage &image, TGAColor color) {
    bool steep = false;
    int dx = abs(x0 - x1);
    int dy = abs(y0 - y1);
    int a0,a1,b0,b1;
    if(dx < dy){
        a0 = y0;
        b0 = x0;
        a1 = y1;
        b1 = x1;
        steep = true;
    }
    else{
        a0 = x0;
        b0 = y0;
        a1 = x1;
        b1 = y1;
    }
    if(a0 > a1){
        std::swap(a0, a1);
        std::swap(b0, b1);
    }
    int da = a1 - a0;
    int db = b1 - b0;
    int derror2 = 2 * abs(db);
    int error2 = 0;
    int b = b0;
    for(int a = a0; a <= a1; a++)
    {
        if(steep){
            image.set(b, a, color);
        }else{
            image.set(a, b, color);
        }
        error2 += derror2;
        if(error2 > da)
        {
            b += (b1 > b0 ? 1 : -1);
            error2 -= 2 * da;
        }
    }
}

void triangle(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage &image, TGAColor color){
    if(t0.y == t1.y && t1.y == t2.y) return;
    if(t0.y > t1.y)swap(t0, t1);
    if(t1.y > t2.y)swap(t1, t2);
    if(t0.y > t1.y)swap(t0, t1);
    int tot_height = t2.y - t0.y, seg_height, height;
    bool upper;
    float alpha, beta;
    Vec2f a, b, va, vb, ori;
    for(int y = t0.y; y < t2.y; y++)
    {
        upper = (y > t1.y || t1.y == t0.y);
        seg_height = upper ? t2.y - t1.y : t1.y - t0.y;
        height = upper ? t2.y - y : y - t0.y;
        va = upper ? Vec2f{t1.x-t2.x, t1.y-t2.y} : Vec2f{t1.x-t0.x, t1.y-t0.y};
        vb = upper ? Vec2f{t0.x-t2.x, t0.y-t2.y} : Vec2f{t2.x-t0.x, t2.y-t0.y};
        ori = upper ? Vec2f{t2.x, t2.y} : Vec2f{t0.x, t0.y};
        alpha = (float)height / seg_height;
        beta = (float)height / tot_height;
        a = alpha * va + ori; //cause for flaot !
        b = beta * vb + ori;
        if(a.x > b.x)swap(a, b);
        for(int x = a.x; x <= b.x; x++)
            image.set(x, y, color);
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

void triangle_bary(Vec3f* pts, float* zbuffer, TGAImage &image, TGAColor color)
{
    Vec2i bboxmin(image.get_width() - 1, image.get_height() - 1);
    Vec2i bboxmax(0, 0);
    for(int i = 0; i < 3; i++)
    {
        bboxmin.x = max(0, min(bboxmin.x, (int)pts[i].x));
        bboxmin.y = max(0, min(bboxmin.y, (int)pts[i].y));
        bboxmax.x = min(image.get_width()  - 1,  max(bboxmax.x, (int)pts[i].x));
        bboxmax.y = min(image.get_height() - 1,  max(bboxmax.y, (int)pts[i].y));
    }
    for(int x = bboxmin.x; x <= bboxmax.x; x++)
    {
        for(int y = bboxmin.y; y <= bboxmax.y; y++)
        {
            Vec3f bc = barycentric(pts, Vec3f(x+0.5f, y+0.5f, 1));
            if(bc.x < 0 || bc.y < 0 || bc.z < 0)continue;
            float z = 0;
            z = pts[0].z * bc.x + pts[1].z * bc.y + pts[2].z * bc.z;
            if(z > zbuffer[x + y * width]){
                zbuffer[x + y * width] = z;
                image.set(x, y, color);
            }
        }
    }
}

int main(int argc, char** argv) {
    TGAImage image(width, height, TGAImage::RGB);
    Vec3f light_dir(0, 0, -1);
    
    model = new Model("obj/african_head.obj");
    float* zbuffer = new float[width * height];
    for(int i = width * height; i--; zbuffer[i] = -std::numeric_limits<float>::max());

    for(int i = 0; i < model->nfaces(); i++)
    {
        vector<int> f = model->face(i);
        Vec3f world_coord[3];
        Vec3f screen_coord[3];
        for(int j = 0; j < 3; j++)
        {
            world_coord[j] = model->vert(f[j]);
            screen_coord[j] = Vec3f((world_coord[j].x + 1)*width/2., (world_coord[j].y + 1)*height/2., world_coord[j].z);
        }
       
        // method 1
        triangle_bary(screen_coord, zbuffer, image, TGAColor(rand()%255, rand()%255, rand()%255, 255));

        // method 2
        // triangle(Vec2i(screen_coord[0].x, screen_coord[0].y), 
        //         Vec2i(screen_coord[1].x, screen_coord[1].y),
        //         Vec2i(screen_coord[2].x, screen_coord[2].y),
        //         image, TGAColor(rand()%255, rand()%255, rand()%255, 255));
    }

    image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}

