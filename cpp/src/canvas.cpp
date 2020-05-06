/*=======================================
 canvas.cpp:                    k-vernooy
 last modified:               Tue, Feb 18
 
 A file for all the canvas functions for
 various gui apps, tests, functions and
 visualizations.
========================================*/

#include <iostream>
#include <random>
#include <cmath>

#include "../include/community.hpp"
#include "../include/util.hpp"
#include "../include/canvas.hpp"
#include "../include/geometry.hpp"  
#include <boost/filesystem.hpp>

using std::cout;
using std::endl;
using namespace hte;
using namespace Graphics;

int RECURSION_STATE = 0;
double PADDING = (15.0/16.0);


Style& Style::outline(RGB_Color c) {
    // set the outline color
    outline_ = c;
    return *this;
}


Style& Style::thickness(int t) {
    // set the outline thickness
    thickness_ = t;
    return *this;
}


Style& Style::fill(RGB_Color c) {
    // set the fill color (RGB)
    fill_ = c;
    return *this;
}


Style& Style::fill(HSL_Color c) {
    // set the fill color (HSL)
    fill_ = hsl_to_rgb(c);
    return *this;
}


double Graphics::hue_to_rgb(double p, double q, double t) {	
    /*
        Convert a hue to a single rgb value
        I honestly forget how this works sorry future me
    */

    if(t < 0) t += 1;
    if(t > 1) t -= 1;
    if(t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if(t < 1.0/2.0) return q;
    if(t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}	


RGB_Color Graphics::hsl_to_rgb(HSL_Color hsl) {
    /*
        @desc: Convert a HSL_Color object into RGB_Color
        @params: `HSL_Color` hsl: color to convert
        @return: `RGB_Color` converted color
    */

    double r, g, b;

    if (hsl.s == 0.0) {
        r = hsl.l;
        g = hsl.l;
        b = hsl.l;
    }
    else {
        double q = (hsl.l < 0.5) ? hsl.l * (1.0 + hsl.s)
            : hsl.l + hsl.s - hsl.l * hsl.s;

        double p = 2.0 * hsl.l - q;

        r = hue_to_rgb(p, q, hsl.h + 1.0/3.0);
        g = hue_to_rgb(p, q, hsl.h);
        b = hue_to_rgb(p, q, hsl.h - 1.0/3.0);
    }

    return RGB_Color((r * 255.0), (g * 255.0), (b * 255.0));
}


HSL_Color Graphics::rgb_to_hsl(RGB_Color rgb) {
    /*
        @desc: Converts RGB_Color object into HSL_Color
    */

    double r = (double) rgb.r / 255.0;
    double g = (double) rgb.g / 255.0;
    double b = (double) rgb.b / 255.0;
    double max = r, min = r;

    for (double x : {r,g,b}) {
        if (x > max) max = x;
        else if (x < min) min = x;
    }


    double h, s, l = (max + min) / 2.0;

    if (max == min) {
        h = 0.0;
        s = 0.0; // achromatic
    }
    else {
        double d = max - min;
        s = (l > 0.5) ? (d / (2.0 - max - min)) : (d / (max + min));

        if (max == r) h = (g - b) / d + ((g < b) ? 6.0 : 0.0);
        else if (max == g) h = (b - r) / d + 2;
        else h = (r - g) / d + 4;

        h /= 6.0;
    }

    return HSL_Color(h, s, l);
}


double Graphics::lerp(double a, double b, double t) {
    return(a + (b - a) * t);
}


HSL_Color Graphics::interpolate_hsl(HSL_Color hsl1, HSL_Color hsl2, double interpolator) {	
    return HSL_Color(
        lerp(hsl1.h, hsl2.h, interpolator),
        lerp(hsl1.s, hsl2.s, interpolator),
        lerp(hsl1.l, hsl2.l, interpolator)
    );
}


RGB_Color Graphics::interpolate_rgb(RGB_Color rgb1, RGB_Color rgb2, double interpolator) {
    return RGB_Color(
        round(lerp(rgb1.r, rgb2.r, interpolator)),
        round(lerp(rgb1.g, rgb2.g, interpolator)),
        round(lerp(rgb1.b, rgb2.b, interpolator))
    );
}


std::vector<RGB_Color> Graphics::generate_n_colors(int n) {
    /*
        @desc: Generates a number of colors blindly (and semi-randomly)
        @params: `int` n: number of colors to generate
        @return: `vector<Graphics::RGB_Color>` list of color objects
    */

    std::vector<RGB_Color> colors;
    for (int i = 0; i < 360; i += 360 / n) {
        // create and add colors
        colors.push_back(
            hsl_to_rgb({
                (int)((double)i / 360.0),
                (int)((double)(80 + rand_num(0, 20)) / 100.0),
                (int)((double)(50 + rand_num(0, 10)) / 100.0)
            })
        );

    }

    return colors;
}


int PixelBuffer::index_from_position(int a, int b) {
    return ((x * (b - 1)) + a - 1);
}


void PixelBuffer::set_from_position(int a, int b, Uint32 value) {
    ar[index_from_position(a, b)] = value;
}


Geometry::bounding_box get_bounding_box(Outline outline) {
    /*
        @desc: returns a bounding box of the outline
        
        @params: none;
        @return: `bounding_box` the superior bounding box of the shape
    */

    // set dummy extremes
    return outline.border.get_bounding_box();
}


Geometry::bounding_box Canvas::get_bounding_box() {
    /*
        @desc:
            returns a bounding box of the internal list of hulls
            (because holes cannot be outside shapes)
        
        @params: none;
        @return: `bounding_box` the superior bounding box of the shape
    */

    if (outlines.size() > 0) {
        // set dummy extremes
        int top = outlines[0].border.border[0][1], 
            bottom = outlines[0].border.border[0][1], 
            left = outlines[0].border.border[0][0], 
            right = outlines[0].border.border[0][0];

        // loop through and find actual corner using ternary assignment
        for (Outline ring : outlines) {
            Geometry::bounding_box x = ring.border.get_bounding_box();
            if (x[0] > top) top = x[0];
            if (x[1] < bottom) bottom = x[1];
            if (x[2] < left) left = x[2];
            if (x[3] > right) right = x[3];
        }

        box = {top, bottom, left, right};
    }
    else {
        box = {height, 0, 0, width};
    }

    return box; // return bounding box
}


void Canvas::translate(long int t_x, long int t_y, bool b) {
    /*
        @desc:
            Translates all linear rings contained in the
            canvas object by t_x and t_y
        
        @params: 
            `long int` t_x: x coordinate to translate
            `long int` t_y: y coordinate to translate
        
        @return: void
    */

    for (int i = 0; i < outlines.size(); i++) {
        for (int j = 0; j < outlines[i].border.border.size(); j++) {
            outlines[i].border.border[j][0] += t_x;
            outlines[i].border.border[j][1] += t_y;
        }
    }

    for (int i = 0; i < holes.size(); i++) {
        for (int j = 0; j < holes[i].border.border.size(); j++) {
            holes[i].border.border[j][0] += t_x;
            holes[i].border.border[j][1] += t_y;
        }
    }
    
    to_date = false;
    if (b) box = {box[0] + t_y, box[1] + t_y, box[2] + t_x, box[3] + t_x};
}


void Canvas::scale(double scale_factor) {
    /*
        @desc:
            Scales all linear rings contained in the canvas
            object by scale_factor (including holes)
        
        @params: `double` scale_factor: factor to scale coordinates by
        
        @return: void
    */

    for (int i = 0; i < outlines.size(); i++) {
        for (int j = 0; j < outlines[i].border.border.size(); j++) {
            outlines[i].border.border[j][0] *= scale_factor;
            outlines[i].border.border[j][1] *= scale_factor;
        }
    }

    for (int i = 0; i < holes.size(); i++) {
        for (int j = 0; j < holes[i].border.border.size(); j++) {
            holes[i].border.border[j][0] *= scale_factor;
            holes[i].border.border[j][1] *= scale_factor;
        }
    }

    to_date = false;
}


// void Outline::flood_fill_util(Geometry::coordinate coord, Color c1, Color c2, Canvas& canvas) {
//     RECURSION_STATE++;
//     if (RECURSION_STATE > 10000) return;

//     if (coord[0] < 0 || coord[0] > pixels.size() || coord[1] < 0 || coord[1] > pixels[0].size()) return;
//     if (this->get_pixel({coord[0], coord[1]}).color != c1) return;
    
//     Pixel p(coord[0], coord[1], c2);
//     this->pixels[coord[0]][coord[1]] = p;
//     canvas.pixels[coord[0]][coord[1]] = p;

//     flood_fill_util({coord[0] + 1, coord[1]}, c1, c2, canvas);
//     flood_fill_util({coord[0] - 1, coord[1]}, c1, c2, canvas);
//     flood_fill_util({coord[0], coord[1] + 1}, c1, c2, canvas);
//     flood_fill_util({coord[0], coord[1] - 1}, c1, c2, canvas);

//     return;
// }


// void Outline::flood_fill(Geometry::coordinate coord, Color c, Canvas& canvas) {
//     RECURSION_STATE = 0;
//     this->flood_fill_util(coord, Color(-1,-1,-1), c, canvas);
//     return;
// }


void Graphics::draw_line(PixelBuffer& buffer, Geometry::coordinate start, Geometry::coordinate end, RGB_Color color, double t) {
    /*
        @desc: draws a line from a to b using Bresenham's algorithm

        @params: 
            `PixelBuffer&` buffer: the buffer to draw line to
            `Geometry::coordinate` start, end: endpoints of the line
            `double` t: thickness of the line

        @return: void
    */

    int dx = abs(end[0] - start[0]), sx = start[0] < end[0] ? 1 : -1;
    int dy = abs(end[1] - start[1]), sy = start[1] < end[1] ? 1 : -1;
    int err = dx - dy, e2, x2, y2;
    float ed = dx + dy == 0 ? 1 : sqrt((float)dx * dx + (float)dy * dy);
    
    for (t = (t + 1) / 2; ;) {
        // if cval is 0, we want to draw pure color
        double cval = std::max(0.0, 255 * (abs(err - dx + dy) / ed - t + 1));
        buffer.set_from_position(start[0], start[1], interpolate_rgb(color, RGB_Color(255, 255, 255), (cval / 255.0)).to_uint());
        e2 = err; x2 = start[0];

        if (2 * e2 >= -dx) {
            for (e2 += dy, y2 = start[1]; e2 < ed*t && (end[1] != y2 || dx > dy); e2 += dx) {
                double cval = std::max(0.0, 255 * (abs(e2) / ed - t + 1));
                buffer.set_from_position(start[0], y2 += sy, interpolate_rgb(color, RGB_Color(255, 255, 255), (cval / 255.0)).to_uint());
            }
            if (start[0] == end[0]) break;
            e2 = err; err -= dy; start[0] += sx; 
        } 
        if (2 * e2 <= dy) {
            for (e2 = dx - e2; e2 < ed * t && (end[0] != x2 || dx < dy); e2 += dy) {
                int cval = std::max(0.0, 255 * (abs(e2) / ed - t + 1));
                buffer.set_from_position(x2 += sx, start[1], interpolate_rgb(color, RGB_Color(255, 255, 255), (cval / 255.0)).to_uint());
            }
            if (start[1] == end[1]) break;
            err += dx; start[1] += sy; 
        }
    }

    
    // // old untested method for setting
    // // single width lines
    // int dx = end[0] - start[0],
    //     dy = end[1] - start[1];

    // int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);

    // double xinc = (double) dx / (double) steps;
    // double yinc = (double) dy / (double) steps;

    // double xv = (double) start[0];
    // double yv = (double) start[1];

    // for (int i = 0; i <= steps; i++) {

    //     buffer.set_from_position(xv, yv, color.to_uint());

    //     xv += xinc;
    //     yv += yinc;
    // }

    return;
}


Uint32 RGB_Color::to_uint() {
    Uint32 rgba =
        (0 << 24) +
        (r << 16) +
        (g << 8)  +
        (b);

    return rgba;
}


void Canvas::clear() {
    // @warn: reset background here
    this->outlines = {};
    this->holes = {};
    this->pixel_buffer = PixelBuffer(width, height);
    to_date = true;
}


void Canvas::get_bmp(std::string write_path, SDL_Window* window, SDL_Renderer* renderer) {
    /*
        @desc: Takes a screenshot as a BMP image of an SDL surface
        @params:
            `string` write_path: output image file
    */

    // create empty RGB surface to hold window data
    SDL_Surface* pScreenShot = SDL_CreateRGBSurface(
        0, width, height, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000
    );
    
    // check file path and output
    if (boost::filesystem::exists(write_path + ".bmp")) cout << "File already exists, returning" << endl;
    
    if (pScreenShot) {

        // read pixels from render target, save to surface
        SDL_RenderReadPixels(
            renderer, NULL, SDL_GetWindowPixelFormat(window),
            pScreenShot->pixels, pScreenShot->pitch
        );

        // Create the bmp screenshot file
        SDL_SaveBMP(pScreenShot, std::string(write_path + ".bmp").c_str());
        SDL_FreeSurface(pScreenShot);
    }
    else {
        cout << "Uncaught error here, returning" << endl;
        return;
    }
}


void Canvas::save_image(ImageFmt fmt, std::string path) {
    if (!to_date && fmt != ImageFmt::SVG) rasterize();
    

    // if (fmt == ImageFmt::BMP) {
    //     get_bmp(path);
    // }
    // else if (fmt == ImageFmt::BMP) {
    //     writef(get_svg(), path);
    // }
}


void Canvas::rasterize() {
    /*
        @desc: Updates the canvas's pixel buffer with rasterized outlines
        @params: none
        @return `void`
    */

    // if (!to_date) {
        cout << "ok" << endl;
        this->clear();
        cout << "ok" << endl;
        // this->pixel_buffer = PixelBuffer(width, height);

        // @warn may be doing extra computation here
        get_bounding_box();
        cout << "ok" << endl;
        // translate into first quadrant
        translate(-box[2], -box[1], true);
        cout << "ok" << endl;

        // determine smaller side/side ratio for scaling
        double ratio_top = ceil((double) this->box[0]) / (double) (width);
        double ratio_right = ceil((double) this->box[3]) / (double) (height);
        double scale_factor = 1 / ((ratio_top > ratio_right) ? ratio_top : ratio_right); 
        // scale by factor
        scale(scale_factor * PADDING);
        int px = (int)((double)width * (1.0-PADDING) / 2.0), py = (int)((double)height * (1.0-PADDING) / 2.0);
        translate(px, py, false);

        cout << "ok" << endl;
        draw_line(pixel_buffer, {50,50}, {20, 200}, RGB_Color(255, 0, 0), 1.0);
        // if (ratio_top < ratio_right) {
        //     // center vertically
        //     std::cout << "x" << std::endl;
        //     int t = (int)((((double)y - ((double)py * 2.0)) - (double)this->box[0]) / 2.0);
        //     std::cout << t << std::endl;
        //     translate(0, t, false);
        // }

        // for (int i = 0; i < outlines.size(); i++) {
        //     outlines[i].rasterize(*this);
        // }
    // }

    cout << "rasterized" << endl;

    to_date = true;
}


void Canvas::draw_to_window() {
    /*
        @desc:
            Prints the shapes in the canvas to the screen
            (in the case of no window passed, create a window)

        @params: none
        @return: void
    */

    this->rasterize();
    SDL_Init(SDL_INIT_VIDEO);
    
    cout << "A" << endl;
    SDL_Event event;
    SDL_Window* window = SDL_CreateWindow("Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);

    SDL_SetWindowResizable(window, SDL_TRUE);
    SDL_UpdateTexture(texture, NULL, pixel_buffer.ar, width * sizeof(Uint32));
    SDL_WaitEvent(&event);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    bool quit = false;

    while (!quit) {
        SDL_UpdateTexture(texture, NULL, pixel_buffer.ar, width * sizeof(Uint32));
        SDL_WaitEvent(&event);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        if (event.type == SDL_QUIT) quit = true;
    }

    // destroy arrays and SDL objects
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
