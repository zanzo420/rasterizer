#include <cstdio>

#if defined(USE_OPENGL)
    #include <GL/glut.h>
#endif

#include <iostream>

#include <Eigen/Dense>

#include <argparser.h>

#include <Color.h>
#include <Material.h>
#include <Light.h>
#include <Sphere.h>
#include <Util.h>

constexpr int NX = 512;
constexpr int NY = 512;

Color* buffer;
float zbuf[NX][NY];

char shade_mode[20];

void cleanup() {
    delete[] buffer;
}

#if defined(USE_OPENGL)
    void gl_display() {
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT);

        float* float_buffer = new float[3*NX*NY];

        int k = 0;
        for (int i = 0; i < NX; i++) {
            for (int j = 0; j < NY; j++) {
                float_buffer[k++] = buffer[j*NY + i].r;
                float_buffer[k++] = buffer[j*NY + i].g;
                float_buffer[k++] = buffer[j*NY + i].b;
            }
        }

        glDrawPixels(NX, NY, GL_RGB, GL_FLOAT, float_buffer);

        glutSwapBuffers();

        delete[] float_buffer;
    }

    void gl_keyboard(unsigned char key, int x, int y) {
        switch (key) {
            // ESC
            case 27:
                cleanup();
                exit(EXIT_SUCCESS);
        }
    }
#endif

void draw(int x, int y, const Color& color) {
    buffer[x*NY + y] = color.correct(2.2f);
}

template <typename T>
T lerp(T a, T b, T c, float beta, float gamma) {
    return a + (c-a)*beta + (b-a)*gamma;
}

void rasterize(const Triangle& tri, const Light& light, const Material& mat, const Eigen::Matrix4f& M) {
    // Backface culling
    Eigen::Vector3f v = tri.centroid().normalized();
    if (v.dot(tri.n) > 0)
        return;

    // Convert triangles coordinates from world to viewport
    auto a_vp = vec4to3(M * Eigen::Vector4f(tri.a[0], tri.a[1], tri.a[2], 1.0f));
    auto b_vp = vec4to3(M * Eigen::Vector4f(tri.b[0], tri.b[1], tri.b[2], 1.0f));
    auto c_vp = vec4to3(M * Eigen::Vector4f(tri.c[0], tri.c[1], tri.c[2], 1.0f));

    Triangle tri_vp(a_vp, b_vp, c_vp);

    BoundingBox bb = tri_vp.bounds();

    float ax = a_vp[0];
    float ay = a_vp[1];
    float bx = b_vp[0];
    float by = b_vp[1];
    float cx = c_vp[0];
    float cy = c_vp[1];

    // Denominators
    float beta_denom  = (ay-cy)*bx + (cx-ax)*by + ax*cy - cx*ay;
    float gamma_denom = (ay-by)*cx + (bx-ax)*cy + ax*by - bx*ay;

    float beta, gamma;
    for (int y = bb.ymin; y <= bb.ymax; y++) {
        for (int x = bb.xmin; x <= bb.xmax; x++) {
            beta  = ((ay-cy)*x + (cx-ax)*y + ax*cy - cx*ay) / beta_denom;
            gamma = ((ay-by)*x + (bx-ax)*y + ax*by - bx*ay) / gamma_denom;

            float z = lerp(tri_vp.a[2], tri_vp.b[2], tri_vp.c[2], beta, gamma);

            if (beta >= 0 && gamma >= 0 && beta + gamma <= 1 && z > zbuf[x][y]) {
                zbuf[x][y] = z;
                if (strcmp(shade_mode, "NONE") == 0) {
                    draw(x, y, Color::white());
                } else if (strcmp(shade_mode, "FLAT") == 0) {
                    draw(x, y, tri.shade(tri.centroid(), tri.n, light, mat));
                } else if (strcmp(shade_mode, "GOURAUD") == 0) {
                    /*
                    Color ac = tri.shade(tri.a, tri.an, light, mat);
                    Color bc = tri.shade(tri.b, tri.bn, light, mat);
                    Color cc = tri.shade(tri.c, tri.cn, light, mat);

                    draw(x, y, lerp_color(ac, bc, cc, beta, gamma));
                    */
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    // Parse system arguments
    argparser ap = argparser_create(argc, argv, PARSEMODE_LENIENT);
    argparser_add(&ap, "-s", "--shading", ARGTYPE_STRING, &shade_mode, nullptr);
    argparser_parse(&ap);

    constexpr float l = -0.1f;
    constexpr float r =  0.1f;
    constexpr float b = -0.1f;
    constexpr float t =  0.1f;
    constexpr float n = -0.1f;
    constexpr float f = -1000.0f;

    Eigen::Matrix4f M, M_world, M_m, M_cam, P, M_orth, M_vp;

    // Modeling transform
    M_m <<     2.0f,            0.0f,           0.0f,           0.0f,
               0.0f,            2.0f,           0.0f,           0.0f,
               0.0f,            0.0f,           2.0f,          -7.0f,
               0.0f,            0.0f,           0.0f,           1.0f;

    // Camera transform
    M_cam <<   1.0f,            0.0f,           0.0f,           0.0f,
               0.0f,            1.0f,           0.0f,           0.0f,
               0.0f,            0.0f,           1.0f,           0.0f,
               0.0f,            0.0f,           0.0f,           1.0f;

    // Perspective transform
    P <<       n,               0.0f,           0.0f,           0.0f,
               0.0f,            n,              0.0f,           0.0f,
               0.0f,            0.0f,           n+f,            -f*n,
               0.0f,            0.0f,           1.0f,           0.0f;

    // Orthographic transform
    M_orth <<  2.0f/(r-l),      0.0f,           0.0f,           -(r+l)/(r-l),
               0.0f,            2.0f/(t-b),     0.0f,           -(t+b)/(t-b),
               0.0f,            0.0f,           2.0f/(n-f),     -(n+f)/(n-f),
               0.0f,            0.0f,           0.0f,           1.0f;

    // Viewport transform
    M_vp <<    NX/2.0f,         0.0f,           0.0f,           (NX-1)/2.0f,
               0.0f,            NY/2.0f,        0.0f,           (NY-1)/2.0f,
               0.0f,            0.0f,           1.0f,           0.0f,
               0.0f,            0.0f,           0.0f,           1.0f;

    // World transform
    M_world = M_cam * M_m;

    // "Viewport" transform
    M = M_vp * M_orth * P;

    // Sphere
    Color ka(0.0f, 1.0f, 0.0f);
    Color kd(0.0f, 0.5f, 0.0f);
    Color ks(0.5f, 0.5f, 0.5f);
    Material mat(ka, kd, ks, 32);
    Sphere sphere(mat, M_world);

    // Light
    Light light(Eigen::Vector3f(-4, 4, -3), 1);

    // Black buffer
    buffer = new Color[NX*NY];
    for (int x = 0; x < NX; x++)
        for (int y = 0; y < NY; y++)
            draw(x, y, Color::black());

    // Z buffer starts at max depth
    for (int x = 0; x < NX; x++)
        for (int y = 0; y < NY; y++)
            zbuf[x][y] = f;

    // Rasterize sphere
    for (const auto& tri : sphere.triangles)
        rasterize(tri, light, mat, M);

    #if defined(USE_OPENGL)
        // Write buffer to OpenGL window
        char window_name[50];
        if      (strcmp(shade_mode, "NONE") == 0)    strcpy(window_name, "Part 1 (Unshaded)");
        else if (strcmp(shade_mode, "FLAT") == 0)    strcpy(window_name, "Part 2 (Flat Shading)");
        else if (strcmp(shade_mode, "GOURAUD") == 0) strcpy(window_name, "Part 3 (Gouraud Shading)");
        else                                         strcpy(window_name, "Part 4 (Phong Shading)");
        glutInit(&argc, argv);
        glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
        glutInitWindowSize(NX, NY);
        glutCreateWindow(window_name);
        glutDisplayFunc(gl_display);
        glutKeyboardFunc(gl_keyboard);
        glutMainLoop();
    #else
        // Write buffer to image file
        char ppmpath[50];
        if      (strcmp(shade_mode, "NONE") == 0)    strcpy(ppmpath, "images/part1-unshaded.ppm");
        else if (strcmp(shade_mode, "FLAT") == 0)    strcpy(ppmpath, "images/part2-flat.ppm");
        else if (strcmp(shade_mode, "GOURAUD") == 0) strcpy(ppmpath, "images/part3-gouruad.ppm");
        else                                         strcpy(ppmpath, "images/part4-phong.ppm");
        FILE* fp = fopen(ppmpath, "w");
        fprintf(fp, "P3\n");
        fprintf(fp, "%d %d %d\n", NX, NY, 255);
        for (int i = NX-1; i >= 0; i--) {
            for (int j = 0; j < NY; j++) {
                // Convert float RGB to int RGB
                int ir = (int)(buffer[j*NY + i].r * 255);
                int ig = (int)(buffer[j*NY + i].g * 255);
                int ib = (int)(buffer[j*NY + i].b * 255);

                fprintf(fp, "%d %d %d\n", ir, ig, ib);
            }
        }
        fclose(fp);
    #endif

    cleanup();

    return EXIT_SUCCESS;
}
