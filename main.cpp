#include "tgaimage.h"
#include "model.h"
#include <iostream>
using namespace std;

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green = TGAColor(0, 255,   0,   255);
const int width = 200;
const int height = 200;
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
    if(t0.y > t1.y)swap(t0, t1);
    if(t1.y > t2.y)swap(t1, t2);
    if(t0.y > t1.y)swap(t0, t1);
    int tot_height = t2.y - t0.y;
    int seg_height = t1.y - t0.y + 1;
    Vec2i a(t1 - t0);
    Vec2i b(t2 - t0);
    for(int y = t0.y; y <= t1.y; y++)
    {
        float alpha = (float)(y - t0.y) / tot_height;
        float beta = (float)(y - t0.y) / seg_height;
        Vec2i _t1 = a * beta + t0;
        Vec2i _t2 = alpha * b + t0;
        if(_t1.x > _t2.x)swap(_t2, _t1);
        for(int x = _t1.x; x <= _t2.x; x++)
        {
            image.set(x, y, color);
        }
    }

    seg_height = t2.y - t1.y + 1;
    for(int y = t1.y + 1; y <= t2.y; y++)
    {
        int l = t2.y - y;
        float alpha = (float)l / seg_height;
        float beta = (float)l / tot_height;
        Vec2i _t1 = t2 + alpha * (t1 - t2);
        Vec2i _t0 = t2 + beta * (t0 - t2);
        if(_t0.x > _t1.x)swap(_t0, _t1);
        for(int x = _t0.x; x <= _t1.x; x++)
        {
            image.set(x, y, color);
        }
    }
}

int main(int argc, char** argv) {
    TGAImage image(width, height, TGAImage::RGB);
    
    Vec2i t0[3] = {Vec2i(10, 70),   Vec2i(50, 160),  Vec2i(70, 80)};
    Vec2i t1[3] = {Vec2i(180, 50),  Vec2i(150, 1),   Vec2i(70, 180)};
    Vec2i t2[3] = {Vec2i(180, 150), Vec2i(120, 160), Vec2i(130, 180)};

    triangle(t0[0], t0[1], t0[2], image, red);
    triangle(t1[0], t1[1], t1[2], image, white);
    triangle(t2[0], t2[1], t2[2], image, green);

    image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}

