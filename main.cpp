#include "tgaimage.h"
#include "model.h"
#include <iostream>
using namespace std;

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
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

int main(int argc, char** argv) {
    TGAImage image(width, height, TGAImage::RGB);
    if(2 == argc)
    {
        model = new Model(argv[1]);
    }
    else
    {
        model = new Model("obj/african_head.obj");
    }
    int fs = model->nfaces();
    for(int f = 0; f < fs; f++)
    {
        auto vs = model->face(f);
        for(int l = 0; l < 3; l++)
        {
            Vec3f v0 = model->vert(vs[l]);
            Vec3f v1 = model->vert(vs[(l+1)%3]);
            v0.x = (v0.x + 1) * width / 2;
            v0.y = (v0.y + 1) * height / 2;
            v1.x = (v1.x + 1) * width / 2;
            v1.y = (v1.y + 1) * height / 2;
            line(v0.x, v0.y, v1.x, v1.y, image, white);
        }
    }

    image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}
