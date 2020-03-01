/*=======================================
 gui.cpp:                       k-vernooy
 last modified:               Tue, Feb 18
 
 A file for all the sdl functions for
 various gui apps, tests, functions and
 visualizations.
========================================*/

#include "../include/gui.hpp"
#include "../include/geometry.hpp"

GeoGerry::bounding_box normalize_coordinates(GeoGerry::Shape* shape) {

    /*
        returns a normalized bounding box, and modifies 
        shape object's coordinates to move it to Quadrant I
    */

    // set dummy extremes
    int top = shape->hull.border[0][1], 
        bottom = shape->hull.border[0][1], 
        left = shape->hull.border[0][0], 
        right = shape->hull.border[0][0];

    // loop through and find actual corner using ternary assignment
    for (GeoGerry::coordinate coord : shape->hull.border) {
        top = (coord[1] > top)? coord[1] : top;
        bottom = (coord[1] < bottom)? coord[1] : bottom;
        left = (coord[0] < left)? coord[0] : left;
        right = (coord[0] > right)? coord[0] : right;
    }

    // add to each coordinate to move it to quadrant 1
    for (int i = 0; i < shape->hull.border.size(); i++) {
        shape->hull.border[i][0] += (0 - left);
        shape->hull.border[i][1] += (0 - bottom);
    }

    // normalize the bounding box too
    top += (0 - bottom);
    right += (0 - left);
    bottom = 0;
    left = 0;

    return {top, bottom, left, right}; // return bounding box
}

GeoGerry::coordinate_set resize_coordinates(GeoGerry::bounding_box box, GeoGerry::coordinate_set shape, int screenX, int screenY) {
    // scales an array of coordinates to fit 
    // on a screen of dimensions {screenX, screenY}
    
    double ratioTop = ceil((float) box[0]) / (float) (screenX);   // the rounded ratio of top:top
    double ratioRight = ceil((float) box[3]) / (float) (screenY); // the rounded ratio of side:side
    
    // find out which is larger and assign its reciporical to the scale factor
    double scaleFactor = floor(1 / ((ratioTop > ratioRight) ? ratioTop : ratioRight)); 

    // dilate each coordinate in the shape
    for ( int i = 0; i < shape.size(); i++ ) {
        shape[i][0] *= scaleFactor;
        shape[i][1] *= scaleFactor;        
    }

    // return scaled coordinates
    return shape;
}

Uint32* pix_array(GeoGerry::coordinate_set shape, int x, int y) {

    /* 
        creates and returns a pixel array 
        from an vector of floating point coordinates
    */

    Uint32 * pixels = new Uint32[x * y];           // new blank pixel array
    memset(pixels, 255, x * y * sizeof(Uint32));   // fill pixel array with white
    int total = (x * y) - 1;                       // last index in pixel array;

    for (GeoGerry::coordinate coords : shape) {
        // locate the coordinate, set it to black
        int start = (int)((int)coords[1] * x - (int)coords[0]);
        pixels[total - start] = 0;
    }

    // return array
    return pixels;
}

GeoGerry::coordinate_set connect_dots(GeoGerry::coordinate_set shape) {
    GeoGerry::coordinate_set newShape;

    int dx, dy, p, x, y, x0, x1, y0, y1;

    for (int i = 0; i < shape.size() - 1; i++) {

        x0 = (int) shape[i][0];
        y0 = (int) shape[i][1];

        if ( i != shape.size() - 1) {
            x1 = (int) shape[i + 1][0];
            y1 = (int) shape[i + 1][1];
        }
        else {
            x1 = (int) shape[0][0];
            y1 = (int) shape[0][1];
        }

        dx = x1 - x0;
        dy = y1 - y0;

        int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);

        double xinc = dx / (double) steps;
        double yinc = dy / (double) steps;

        double x = x0;
        double y = y0;

        for (int i = 0; i <= steps; i++) {
            newShape.push_back({(long long int)x, (long long int)y});
            x += xinc;
            y += yinc;
        }
    }
    
    return newShape;
}

void destroy_window(SDL_Window* window) {
    
    // cleanup for sdl windows

    SDL_DestroyWindow( window );
    SDL_Quit();
}

void GeoGerry::Shape::draw() {
    /*
        open an SDL window, create a pixel array 
        with the shape's geometry, and print it to the window
    */

    int dim[2] = {900, 900}; // the size of the SDL window

    // prepare array of coordinates to be drawn
    bounding_box box = normalize_coordinates(this);
    coordinate_set shape = resize_coordinates(box, this->hull.border, dim[0], dim[1]);
    shape = connect_dots(shape);

    // write coordinates to pixel array
    Uint32 * pixels = pix_array(shape, dim[0], dim[1]);

    // initialize window
    SDL_Event event;
    SDL_Window * window = SDL_CreateWindow("Shape", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, dim[0], dim[1], 0);
    SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture * texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, dim[0], dim[1]);
    SDL_SetWindowResizable(window, SDL_TRUE);
    bool quit = false;

    while (!quit) {

        // write current array to screen
        SDL_UpdateTexture(texture, NULL, pixels, dim[0] * sizeof(Uint32));

        // wait for screen to quit
        SDL_WaitEvent(&event);

        if (event.type == SDL_QUIT)
            quit = true;

        // update the SDL renderer
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    // destroy arrays and SDL objects
    delete[] pixels;
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    destroy_window(window);
}

GeoGerry::bounding_box normalize_coordinates(GeoGerry::Multi_Shape* multi_shape) {
    // set dummy extremes
    int top = multi_shape->border[0].hull.border[0][1], 
        bottom = multi_shape->border[0].hull.border[0][1], 
        left = multi_shape->border[0].hull.border[0][0], 
        right = multi_shape->border[0].hull.border[0][0];

    // loop through and find actual corner using ternary assignment
    for (int i = 0; i < multi_shape->border.size(); i++) {
        for (int x = 0; x < multi_shape->border[i].hull.border.size(); x++) {
            top = (multi_shape->border[i].hull.border[x][1] > top) ? multi_shape->border[i].hull.border[x][1] : top;
            bottom = (multi_shape->border[i].hull.border[x][1] < bottom)? multi_shape->border[i].hull.border[x][1] : bottom;
            left = (multi_shape->border[i].hull.border[x][0] < left)? multi_shape->border[i].hull.border[x][0] : left;
            right = (multi_shape->border[i].hull.border[x][0] > right)? multi_shape->border[i].hull.border[x][0] : right;
        }
    }

    for (int i = 0; i < multi_shape->border.size(); i++) {
        // add to each coordinate to move it to quadrant 1
        for (int x = 0; x < multi_shape->border[i].hull.border.size(); x++) {
            multi_shape->border[i].hull.border[x][0] += (0 - left);
            multi_shape->border[i].hull.border[x][1] += (0 - bottom);
        }

        for (int j = 0; j < multi_shape->border[i].holes.size(); j++) {
            for (int k = 0; k < multi_shape->border[i].holes[j].border.size(); k++) {
                multi_shape->border[i].holes[j].border[k][0] += (0 - left);
                multi_shape->border[i].holes[j].border[k][1] += (0 - bottom);
            }
        }
    }

    // normalize the bounding box too
    top += (0 - bottom);
    right += (0 - left);
    bottom = 0;
    left = 0;

    return {top, bottom, left, right}; // return bounding box
}

std::vector<GeoGerry::Shape> resize_coordinates(GeoGerry::bounding_box box, std::vector<GeoGerry::Shape> shapes, int screenX, int screenY) {
    // scales an array of coordinates to fit 
    // on a screen of dimensions {screenX, screenY}
    
    double ratioTop = ceil((float) box[0]) / (float) (screenX);   // the rounded ratio of top:top
    double ratioRight = ceil((float) box[3]) / (float) (screenY); // the rounded ratio of side:side
    
    // find out which is larger and assign it's reciporical to the scale factor
    double scaleFactor = floor(1 / ((ratioTop > ratioRight) ? ratioTop : ratioRight)); 

    // dilate each coordinate in the shape
    for (int i = 0; i < shapes.size(); i++) {
        for ( int x = 0; x < shapes[i].hull.border.size(); x++ ) {
            shapes[i].hull.border[x][0] *= scaleFactor;
            shapes[i].hull.border[x][1] *= scaleFactor;        
        }
        for (int j = 0; j < shapes[i].holes.size(); j++) {
            for (int k = 0; k < shapes[i].holes[j].border.size(); k++) {
                shapes[i].holes[j].border[k][0] *= scaleFactor;
                shapes[i].holes[j].border[k][1] *= scaleFactor;
            }
        }
    }

    // return scaled coordinates
    return shapes;
}

GeoGerry::coordinate_set connect_dots(std::vector<GeoGerry::Shape> shapes) {
    /*
        Given an array of shapes, calculates the
        pixels in a sized matrix that connect the 
        vertices of the shape
    */

    GeoGerry::coordinate_set newShape;

    // loop through shapes
    for (int j = 0; j < shapes.size(); j++) {
        int dx, dy, p, x, y, x0, x1, y0, y1;

        for (int i = 0; i < shapes[j].hull.border.size() - 1; i++) {

            x0 = (int) shapes[j].hull.border[i][0];
            y0 = (int) shapes[j].hull.border[i][1];

            if ( i != shapes[j].hull.border.size() - 1) {
                x1 = (int) shapes[j].hull.border[i + 1][0];
                y1 = (int) shapes[j].hull.border[i + 1][1];
            }
            else {
                x1 = (int) shapes[j].hull.border[0][0];
                y1 = (int) shapes[j].hull.border[0][1];
            }

            dx = x1 - x0;
            dy = y1 - y0;

            int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);

            int xinc = dx / (float) steps;
            float yinc = dy / (float) steps;

            float x = x0;
            float y = y0;

            for (int n = 0; n <= steps; n++) {
                if (j == 0) {
                    newShape.push_back({(long long int)x + 1, (long long int)y});
                    newShape.push_back({(long long int)x, (long long int)y + 1});
                    newShape.push_back({(long long int)x + 1, (long long int)y + 1});
                }
                newShape.push_back({(long long int)x, (long long int)y});
                x += xinc;
                y += yinc;
            }
        }
    }

    return newShape;
}

void GeoGerry::Multi_Shape::draw() {
    // combine precincts into single array, draw array

    int dim[2] = {900, 900}; // the size of the SDL window
    // prepare array of coordinates to be drawn
    GeoGerry::bounding_box box = normalize_coordinates(this);
    std::vector<GeoGerry::Shape> shapes = resize_coordinates(box, this->border, dim[0], dim[1]);

    GeoGerry::coordinate_set shape = connect_dots(shapes);
    // write coordinates to pixel array
    Uint32 * pixels = pix_array(shape, dim[0], dim[1]);

    // initialize window
    SDL_Event event;
    SDL_Window * window = SDL_CreateWindow("Shape", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, dim[0], dim[1], 0);
    SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture * texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, dim[0], dim[1]);
    SDL_SetWindowResizable(window, SDL_TRUE);
    bool quit = false;

    while (!quit) {
        // write current array to screen
        SDL_UpdateTexture(texture, NULL, pixels, dim[0] * sizeof(Uint32));
        // wait for screen to quit
        SDL_WaitEvent(&event);
        
        if (event.type == SDL_QUIT)
            quit = true;

        // update the SDL renderer
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    // destroy arrays and SDL objects
    delete[] pixels;
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    destroy_window(window);
}

GeoGerry::bounding_box normalize_coordinates(GeoGerry::State* state) {
    /*
        returns a normalized bounding box, and modifies 
        shape object's coordinates to move it to Quadrant I
    */
   
    // set dummy extremes
    int top = state->precincts[0].hull.border[0][1], 
        bottom = state->precincts[0].hull.border[0][1], 
        left = state->precincts[0].hull.border[0][0], 
        right = state->precincts[0].hull.border[0][0];

    // loop through and find actual corner using ternary assignment
    for (int i = 0; i < state->precincts.size(); i++) {
        for (int x = 0; x < state->precincts[i].hull.border.size(); x++) {
            top = (state->precincts[i].hull.border[x][1] > top) ? state->precincts[i].hull.border[x][1] : top;
            bottom = (state->precincts[i].hull.border[x][1] < bottom)? state->precincts[i].hull.border[x][1] : bottom;
            left = (state->precincts[i].hull.border[x][0] < left)? state->precincts[i].hull.border[x][0] : left;
            right = (state->precincts[i].hull.border[x][0] > right)? state->precincts[i].hull.border[x][0] : right;
        }
    }

    for (int i = 0; i < state->precincts.size(); i++) {
        // add to each coordinate to move it to quadrant 1
        for (int x = 0; x < state->precincts[i].hull.border.size(); x++) {
            state->precincts[i].hull.border[x][0] += (0 - left);
            state->precincts[i].hull.border[x][1] += (0 - bottom);
        }
        for (int j = 0; j < state->precincts[i].holes.size(); j++) {
            for (int k = 0; k < state->precincts[i].holes[j].border.size(); k++) {
                state->precincts[i].holes[j].border[k][0] += (0 - left);
                state->precincts[i].holes[j].border[k][1] += (0 - bottom);
            }
        }
    }

    // normalize the bounding box too
    top += (0 - bottom);
    right += (0 - left);
    bottom = 0;
    left = 0;

    return {top, bottom, left, right}; // return bounding box
}

std::vector<GeoGerry::Precinct> resize_coordinates(GeoGerry::bounding_box box, std::vector<GeoGerry::Precinct> shapes, int screenX, int screenY) {
    // scales an array of coordinates to fit 
    // on a screen of dimensions {screenX, screenY}
    
    float ratioTop = ceil((float) box[0]) / (float) (screenX);   // the rounded ratio of top:top
    float ratioRight = ceil((float) box[3]) / (float) (screenY); // the rounded ratio of side:side
    
    // find out which is larger and assign it's reciporical to the scale factor
    float scaleFactor = floor(1 / ((ratioTop > ratioRight) ? ratioTop : ratioRight)); 

    // dilate each coordinate in the shape
    for (int i = 0; i < shapes.size(); i++) {
        for ( int x = 0; x < shapes[i].hull.border.size(); x++ ) {
            shapes[i].hull.border[x][0] *= scaleFactor;
            shapes[i].hull.border[x][1] *= scaleFactor;        
        }
        for (int j = 0; j < shapes[i].holes.size(); j++) {
            for (int k = 0; k < shapes[i].holes[j].border.size(); k++) {
                shapes[i].holes[j].border[k][0] *= scaleFactor;
                shapes[i].holes[j].border[k][1] *= scaleFactor;
            }
        }
    }

    // return scaled coordinates
    return shapes;
}

GeoGerry::coordinate_set connect_dots(std::vector<GeoGerry::Precinct> shapes) {
    /*
        Given an array of shapes, calculates the
        pixels in a sized matrix that connect the 
        vertices of the shape
    */

    GeoGerry::coordinate_set newShape;

    // loop through shapes
    for (int j = 0; j < shapes.size(); j++) {
        int dx, dy, p, x, y, x0, x1, y0, y1;

        for (int i = 0; i < shapes[j].hull.border.size() - 1; i++) {

            x0 = (int) shapes[j].hull.border[i][0];
            y0 = (int) shapes[j].hull.border[i][1];

            if ( i != shapes[j].hull.border.size() - 1) {
                x1 = (int) shapes[j].hull.border[i + 1][0];
                y1 = (int) shapes[j].hull.border[i + 1][1];
            }
            else {
                x1 = (int) shapes[j].hull.border[0][0];
                y1 = (int) shapes[j].hull.border[0][1];
            }

            dx = x1 - x0;
            dy = y1 - y0;

            int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);

            float xinc = dx / (float) steps;
            float yinc = dy / (float) steps;

            float x = x0;
            float y = y0;

            for (int i = 0; i <= steps; i++) {
                newShape.push_back({(long long int)x, (long long int)y});
                x += xinc;
                y += yinc;
            }
        }
    }

    return newShape;
}

void GeoGerry::State::draw() {
    // combine precincts into single array, draw array
    int dim[2] = {900, 900}; // the size of the SDL window
    // prepare array of coordinates to be drawn
    GeoGerry::bounding_box box = normalize_coordinates(this);
    std::vector<GeoGerry::Precinct> shapes = resize_coordinates(box, this->precincts, dim[0], dim[1]);
    GeoGerry::coordinate_set shape = connect_dots(shapes);

    // write coordinates to pixel array
    Uint32 * pixels = pix_array(shape, dim[0], dim[1]);
    // initialize window
    SDL_Event event;
    SDL_Window * window = SDL_CreateWindow("Shape", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, dim[0], dim[1], 0);
    SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture * texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, dim[0], dim[1]);
    SDL_SetWindowResizable(window, SDL_TRUE);
    bool quit = false;

    while (!quit) {
        // write current array to screen
        SDL_UpdateTexture(texture, NULL, pixels, dim[0] * sizeof(Uint32));
        // wait for screen to quit
        SDL_WaitEvent(&event);
        
        if (event.type == SDL_QUIT)
            quit = true;

        // update the SDL renderer
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    // destroy arrays and SDL objects
    delete[] pixels;
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    destroy_window(window);
}