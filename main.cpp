#include "tgaimage.h"
#include "model.h"
#include <iostream>
using namespace std;

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green = TGAColor(0, 255,   0,   255);
const int width = 1000;
const int height = 1000;
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
    Vec2i va, vb, a, b, ori;
    for(int y = t0.y; y < t2.y; y++)
    {
        upper = (y > t1.y || t1.y == t0.y);
        seg_height = upper ? t2.y - t1.y + 1 : t1.y - t0.y + 1;
        height = upper ? t2.y - y : y - t0.y;
        va = upper ? t1 - t2 : t1 - t0;
        vb = upper ? t0 - t2 : t2 - t0;
        ori = upper ? t2 : t0;
        alpha = (float)height / seg_height;
        beta = (float)height / tot_height;
        a = alpha * va + ori;
        b = beta * vb + ori;
        if(a.x > b.x)swap(a, b);
        for(int x = a.x; x <= b.x; x++)
            image.set(x, y, color);
    }
}

Vec3f barycentric(Vec2i* pts, Vec2i p)
{
    Vec2i a(p - pts[0]);
    Vec2i b(pts[1] - pts[0]);
    Vec2i c(pts[2] - pts[0]);
    float x1 = a.x, y1 = a.y;
    float x2 = b.x, y2 = b.y;
    float x3 = c.x, y3 = c.y;
    float u = (x3 * y1 - x1 * y3) / (y2 * x3 - x2 * y3);
    float v = (x2 * y1 - x1 * y2) / (x2 * y3 - x3 * y2);
    return Vec3f(1 - u - v, u, v);
}

void triangle_bary(Vec2i* pts, TGAImage &image, TGAColor color)
{
    Vec2i bboxmin(image.get_width() - 1, image.get_height() - 1);
    Vec2i bboxmax(0, 0);
    for(int i = 0; i < 3; i++)
    {
        bboxmin.x = max(0, min(bboxmin.x, pts[i].x));
        bboxmin.y = max(0, min(bboxmin.y, pts[i].y));
        bboxmax.x = min(image.get_width() - 1,  max(bboxmax.x, pts[i].x));
        bboxmax.y = min(image.get_height() - 1, max(bboxmax.y, pts[i].y));
    }
    for(int x = bboxmin.x; x <= bboxmax.x; x++)
    {
        for(int y = bboxmin.y; y <= bboxmax.y; y++)
        {
            Vec3f bc = barycentric(pts, Vec2i(x, y));
            if(bc.x <= 0 || bc.y <= 0 || bc.z <= 0)continue;
            image.set(x, y, color);
        }
    }
}

int main(int argc, char** argv) {
    TGAImage image(width, height, TGAImage::RGB);
    
    model = new Model("obj/african_head.obj");
    for(int i = 0; i < model->nfaces(); i++)
    {
        vector<int> f = model->face(i);
        Vec3f world_coord[3];
        Vec2i screen_coord[3];
        for(int i = 0; i < 3; i++)
        {
            world_coord[i] = model->vert(f[i]);
            screen_coord[i] = Vec2i((world_coord[i].x + 1)*width/2., (world_coord[i].y + 1)*height/2.);
        }
        triangle_bary(screen_coord, image, TGAColor(rand()%255, rand()%255, rand()%255, 255));
        // triangle(screen_coord[0], screen_coord[1], screen_coord[2], 
        //     image, TGAColor(rand()%255, rand()%255, rand()%255, 255));
    }

    image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
    image.write_tga_file("output1.tga");
    delete model;
    return 0;
}

