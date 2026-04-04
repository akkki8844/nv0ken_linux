#include <stdio.h>
#include <string.h>

#define W 16
#define H 16
#define T 255
#define _ 255

typedef struct { int r, g, b; } Px;
static Px bg  = {255,255,255};
static Px blk = {0,0,0};
static Px wht = {220,220,220};

static void write_ppm(const char *path, Px pixels[H][W]) {
    FILE *f = fopen(path, "w");
    fprintf(f, "P3\n%d %d\n255\n", W, H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++)
            fprintf(f, "%d %d %d  ", pixels[y][x].r,
                                     pixels[y][x].g,
                                     pixels[y][x].b);
        fprintf(f, "\n");
    }
    fclose(f);
}

static void gen_arrow(void) {
    Px p[H][W];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            p[y][x] = bg;

    /* outline — black */
    int arrow[H] = {1,2,3,4,5,6,7,8,9,6,5,0,0,0,0,0};
    for (int y = 0; y < 11; y++) {
        p[y][0] = blk;
        if (arrow[y] > 0) p[y][arrow[y]] = blk;
    }
    for (int x = 0; x <= 7; x++) p[0][x] = blk;
    p[7][5] = blk; p[8][5] = blk;
    p[9][3] = blk; p[10][1] = blk;

    /* fill — white */
    for (int y = 1; y <= 10; y++)
        for (int x = 1; x < arrow[y]; x++)
            p[y][x] = wht;

    write_ppm("arrow.ppm", p);
}

static void gen_ibeam(void) {
    Px p[H][W];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            p[y][x] = bg;

    int cx = W / 2;
    /* top and bottom serifs */
    for (int x = cx - 2; x <= cx + 2; x++) {
        p[0][x]      = blk;
        p[H-1][x]    = blk;
    }
    /* vertical stem */
    for (int y = 1; y < H - 1; y++) p[y][cx] = blk;

    write_ppm("ibeam.ppm", p);
}

static void gen_resize(void) {
    Px p[H][W];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            p[y][x] = bg;

    int mid = H / 2;
    /* horizontal arrow shaft */
    for (int x = 1; x < W - 1; x++) p[mid][x] = blk;
    /* left arrowhead */
    for (int i = 0; i <= 3; i++) {
        p[mid - i][3 - i + 1] = blk;
        p[mid + i][3 - i + 1] = blk;
    }
    /* right arrowhead */
    for (int i = 0; i <= 3; i++) {
        p[mid - i][W - 3 + i - 1] = blk;
        p[mid + i][W - 3 + i - 1] = blk;
    }

    write_ppm("resize.ppm", p);
}

static void gen_wait(void) {
    Px p[H][W];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            p[y][x] = bg;

    int cx = W / 2, cy = H / 2;
    /* outer circle */
    int r = 6;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int dx = x - cx, dy = y - cy;
            int dist2 = dx*dx + dy*dy;
            if (dist2 >= (r-1)*(r-1) && dist2 <= r*r + 2)
                p[y][x] = blk;
            else if (dist2 < (r-1)*(r-1))
                p[y][x] = wht;
        }
    }
    /* inner dot */
    p[cy][cx]       = blk;
    p[cy-1][cx]     = blk;
    p[cy+1][cx]     = blk;
    p[cy][cx-1]     = blk;
    p[cy][cx+1]     = blk;

    write_ppm("wait.ppm", p);
}

int main(void) {
    gen_arrow();
    gen_ibeam();
    gen_resize();
    gen_wait();
    printf("generated arrow.ppm, ibeam.ppm, resize.ppm, wait.ppm\n");
    return 0;
}