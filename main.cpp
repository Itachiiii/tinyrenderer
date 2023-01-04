#include "tgaimage.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);

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
    for(int a = a0; a <= a1; a++)
    {
        float t = (a - a0) / (float)(a1 - a0);
        int b = t * (b1 - b0) + b0;
        if(steep){
            image.set(b, a, color);
        }else{
            image.set(a, b, color);
        }
    }
}

int main(int argc, char** argv) {
    TGAImage image(100, 100, TGAImage::RGB);
    line(13, 20, 80, 40, image, white); 
    line(20, 13, 40, 80, image, red); 
    line(80, 40, 13, 20, image, red);
    image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
    image.write_tga_file("output.tga");
    return 0;
}
