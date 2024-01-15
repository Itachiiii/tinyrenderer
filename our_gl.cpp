#include <cmath>
#include <limits>
#include <cstdlib>
#include "our_gl.h"
using namespace std;

Matrix ModelView;
Matrix Viewport;
Matrix Projection;
Matrix MIT; // inverse & transpose of modelview
Matrix MI;

IShader::~IShader() {}

void viewport(int x, int y, int w, int h) {
    Viewport = Matrix::identity();
    Viewport[0][3] = x+w/2.f;
    Viewport[1][3] = y+h/2.f;
    Viewport[2][3] = 255.f/2.f;
    Viewport[0][0] = w/2.f;
    Viewport[1][1] = h/2.f;
    Viewport[2][2] = 255.f/2.f;
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

Vec3f barycentric_ans(Vec2f A, Vec2f B, Vec2f C, Vec2f P) {
    Vec3f s[2];
    for (int i=2; i--; ) {
        s[i][0] = C[i]-A[i];
        s[i][1] = B[i]-A[i];
        s[i][2] = A[i]-P[i];
    }
    Vec3f u = cross(s[0], s[1]);
    if (std::abs(u[2])>1e-2) // dont forget that u[2] is integer. If it is zero then triangle ABC is degenerate
        return Vec3f(1.f-(u.x+u.y)/u.z, u.y/u.z, u.x/u.z);
    return Vec3f(-1,1,1); // in this case generate negative coordinates, it will be thrown away by the rasterizator
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

void triangle_ans(mat<4,3,float> &clipc, IShader &shader, TGAImage &image, float *zbuffer) {
    mat<3,4,float> pts  = (Viewport*clipc).transpose(); // transposed to ease access to each of the points
    mat<3,2,float> pts2;
    for (int i=0; i<3; i++) pts2[i] = proj<2>(pts[i]/pts[i][3]);

    Vec2f bboxmin( std::numeric_limits<float>::max(),  std::numeric_limits<float>::max());
    Vec2f bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    Vec2f clamp(image.get_width()-1, image.get_height()-1);
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            bboxmin[j] = std::max(0.f,      std::min(bboxmin[j], pts2[i][j]));
            bboxmax[j] = std::min(clamp[j], std::max(bboxmax[j], pts2[i][j]));
        }
    }
    Vec2i P;
    TGAColor color;
    for (P.x=bboxmin.x; P.x<=bboxmax.x; P.x++) {
        for (P.y=bboxmin.y; P.y<=bboxmax.y; P.y++) {
            Vec3f bc_screen  = barycentric_ans(pts2[0], pts2[1], pts2[2], P);
            Vec3f bc_clip    = Vec3f(bc_screen.x/pts[0][3], bc_screen.y/pts[1][3], bc_screen.z/pts[2][3]);
            bc_clip = bc_clip/(bc_clip.x+bc_clip.y+bc_clip.z);
            float frag_depth = clipc[2]*bc_clip;
            if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0 || zbuffer[P.x+P.y*image.get_width()]>frag_depth) continue;
            bool discard = shader.fragment(bc_clip, color);
            if (!discard) {
                zbuffer[P.x+P.y*image.get_width()] = frag_depth;
                image.set(P.x, P.y, color);
            }
        }
    }
}

void triangle_bary(IShader& shader, Vec3f* pts, float* zbuffer, TGAImage &image, mat<4,3,float>& viewc)
{
    mat<3,4,float> viewc2 = viewc.transpose();
    Vec3f   eye(1.2, -.8, 3);
    Vec3f worldz(viewc2[0][2]-eye[2],viewc2[1][2]-eye[2],viewc2[2][2]-eye[2]);

    Vec2i bboxmin(image.get_width() - 1, image.get_height() - 1);
    Vec2i bboxmax(0, 0);
    for(int i = 0; i < 3; i++)
    {
        bboxmin.x = std::max(0, std::min(bboxmin.x, (int)pts[i].x));
        bboxmin.y = std::max(0, std::min(bboxmin.y, (int)pts[i].y));
        bboxmax.x = std::min(image.get_width()  - 1,  std::max(bboxmax.x, (int)pts[i].x));
        bboxmax.y = std::min(image.get_height() - 1,  std::max(bboxmax.y, (int)pts[i].y));
    }
    TGAColor color;
    for(int x = bboxmin.x; x <= bboxmax.x; x++)
    {
        for(int y = bboxmin.y; y <= bboxmax.y; y++)
        {
            Vec3f bc = barycentric(pts, Vec3f(x, y, 1));
            Vec3f bc_revised = bc;
            for(int i = 0; i < 3; i++)
                bc_revised[i] /= worldz[i];
            float z = 1.f / (bc_revised[0] + bc_revised[1] + bc_revised[2]);
            for(int i = 0; i < 3; i++)
                bc_revised[i] *= z;
            if(bc_revised.x < 0 || bc_revised.y < 0 || bc_revised.z < 0 || z < zbuffer[x + y * image.get_width()])continue;
            bool discard = shader.fragment(bc_revised,color);
            if(!discard){
                zbuffer[x + y * image.get_width()] = z;
                image.set(x, y, color);
            }
        }
    }
}

void triangle2(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage &image, TGAColor color){
    if(t0.y == t1.y && t1.y == t2.y) return;
    if(t0.y > t1.y)std::swap(t0, t1);
    if(t1.y > t2.y)std::swap(t1, t2);
    if(t0.y > t1.y)std::swap(t0, t1);
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
        a = va * alpha + ori; //cause for flaot !
        b = vb * beta + ori;
        if(a.x > b.x)std::swap(a, b);
        for(int x = a.x; x <= b.x; x++)
            image.set(x, y, color);
    }
}

