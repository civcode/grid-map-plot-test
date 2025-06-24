#include <iostream>
#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

// #define NANOVG_GL3_IMPLEMENTATION
// #include <GLFW/glfw3.h>
// #include "nanovg.h"
// #include "nanovg_gl.h"
// #include "nanovg_gl_utils.h"

#include "render_module/render_module.hpp" 

int main() {

    // constexpr auto img_path = "../data/maze-1-10x10.png";
    constexpr auto img_path = "../data/maze-1-100x100.png";
    // constexpr auto img_path = "../data/maze-4-500x500.png";
    int width, height, channels;

    std::cout << "Loading image: " << img_path << "\n";

    // Custom deleter using stbi_image_free
    auto stbi_deleter = [](unsigned char* p) { stbi_image_free(p); };
    int required_channels = 1;

    std::unique_ptr<unsigned char, decltype(stbi_deleter)>
        data(stbi_load(img_path, &width, &height, &channels, required_channels), stbi_deleter);
    channels = required_channels;
    
    if (!data) {
        std::cerr << "Failed to load image\n";
        return 1;
    }

    std::cout << "Loaded image: " << width << "x" << height
              << " with " << channels << " channels.\n";


    for (int i = 0; i < width * height * channels; ++i) {
        // std::cout << static_cast<int>(data.get()[i]) << " ";
        // if ((i+1) % width == 0) {
        //     std::cout << "\n";
        // }
    }
    
    int px_per_cell = 10;
    int grid_width = width * px_per_cell;
    int grid_height = height * px_per_cell;
    std::cout << "Grid size: " << grid_width << "x" << grid_height << "\n";

    RenderModule::Init(1200, 1200, 5.0);
    RenderModule::EnableParentWindowDocking();




    NVGcontext *vg = RenderModule::GetNanoVGContext();

    /* Create a FB and draw to it */
    int fb_width = width*px_per_cell;
    int fb_height = height*px_per_cell;
    
    // NVGLUframebuffer* fb = nvgluCreateFramebuffer(vg, width, height, NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY | NVG_IMAGE_FLIPY);
    nvg::SetContext(vg);
    NVGLUframebuffer* fb = nvg::GLUtilsCreateFramebuffer(fb_width, fb_height, 
        // NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY | 
        // NVG_IMAGE_FLIPY
        // NVG_IMAGE_NEAREST
        0
    );
    if (!fb) {
        std::cerr << "Failed to create framebuffer." << std::endl;
        return -1;
    }
    // nvgImageSize(vg, fb->image, &width, &height);
    // nvgluBindFramebuffer(fb);
    nvg::GLUtilsBindFramebuffer(fb);
    glad::glViewport(0, 0, fb_width, fb_height);
    glad::glClearColor(0, 0, 0, 0);
    glad::glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    // glViewport(0, 0, width, height);
    // glClearColor(0, 0, 0, 0);
    // glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    nvgBeginFrame(vg, fb_width, fb_height, 1.0f);

            nvg::BeginPath();
            nvg::Rect(0, 0, static_cast<float>(grid_width), static_cast<float>(grid_height));
            nvg::FillColor(nvg::RGBAf(1.0f, 1.0f, 1.0f, 1.0f));
            nvg::Fill();
            nvg::ClosePath();

            for (int i = 0; i < width * height; ++i) {
                int x = (i % width) * px_per_cell;
                int y = (i / width) * px_per_cell;
                y = grid_height - y - px_per_cell; // Invert y-axis for correct rendering
                unsigned char value = data.get()[i];
                if (value != 0) {
                    continue;
                }
                nvg::BeginPath();
                nvg::Rect(static_cast<float>(x), static_cast<float>(y), static_cast<float>(px_per_cell), static_cast<float>(px_per_cell));
                nvg::FillColor(nvg::RGBAf(0.6f, 0.6f, 0.6f, 1.0f));
                nvg::Fill();
                nvg::ClosePath();
            }

                        nvg::BeginPath();
            // draw grid lines
            for (int i = 0; i <= grid_width; i+=px_per_cell) {
                nvg::MoveTo(i, 0);
                nvg::LineTo(i, grid_height);
            }
            for (int i = 0; i <= grid_height; i+=px_per_cell) {
                nvg::MoveTo(0, i);
                nvg::LineTo(grid_width, i);
            }
            nvg::StrokeColor(nvg::RGBAf(0.2f, 0.2f, 0.2f, 0.6f));
            nvg::StrokeWidth(0.5f);
            nvg::Stroke();
            // nvg::FillColor(nvg::RGBA(255, 0, 0, 255));
            // nvg::Fill();
            nvg::ClosePath();
    // nvgBeginPath(vg);
    // nvgCircle(vg, fb_width/2+50, fb_height/2+50, 50);
    // nvgFillColor(vg, nvgRGBA(255, 0, 125, 255));
    // nvgFill(vg);

    nvgEndFrame(vg);
    
    nvgluBindFramebuffer(NULL);
        
    // NVGpaint img_paint = nvgImagePattern(vg, 0, 0, 200, 200, 0, fb->image, 1.0f);
    NVGpaint img_paint = nvg::ImagePattern(0, 0, fb_width, fb_height, 0, fb->image, 1.0f);
    if (img_paint.image == 0) {
        std::cerr << "Failed to create image pattern." << std::endl;
        return -1;
    }
    
    RenderModule::RegisterNanoVGCallback("Red Circle", [&](NVGcontext* vg) {
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, 200, 200);
        nvgFillPaint(vg, img_paint);
        nvgFill(vg);
        nvgClosePath(vg);

        // nvgluBindFramebuffer(fb);
        // nvgBeginFrame(vg, fb->image, 200, 200, 1.0f);
        nvgBeginPath(vg);
        nvgCircle(vg, 100, 100, 50);
        nvgFillColor(vg, nvgRGBA(255, 0, 0, 155));
        nvgFill(vg);
        // nvgEndFrame(vg);
        // nvgluBindFramebuffer(NULL);
    });

    // NVGLUframebuffer *fb = nvgluCreateFramebuffer(vg, grid_width, grid_height, NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY);
    // if (!fb) {
    //     std::cerr << "Failed to create framebuffer\n";
    //     return 1;
    // }
    // nvgluBindFramebuffer(fb);

    // nvgBeginPath(vg);
    // nvgCircle(vg, width/2, height/2, 50);
    // nvgFillColor(vg, nvgRGBA(255, 0, 0, 255));
    // nvgFill(vg);

    // nvgluBindFramebuffer(nullptr);




    RenderModule::RegisterNanoVGCallback("Grid Map Viewer", [&](NVGcontext* vg) {
        RenderModule::ZoomView([&](NVGcontext* vg) {
            nvg::SetContext(vg);

            // nvg::BeginPath();
            // nvg::Rect(0, 0, static_cast<float>(grid_width), static_cast<float>(grid_height));
            // nvg::FillColor(nvg::RGBAf(1.0f, 1.0f, 1.0f, 1.0f));
            // nvg::Fill();
            // nvg::ClosePath();

            // for (int i = 0; i < width * height; ++i) {
            //     int x = (i % width) * px_per_cell;
            //     int y = (i / width) * px_per_cell;
            //     y = grid_height - y - px_per_cell; // Invert y-axis for correct rendering
            //     unsigned char value = data.get()[i];
            //     if (value != 0) {
            //         continue;
            //     }
            //     nvg::BeginPath();
            //     nvg::Rect(static_cast<float>(x), static_cast<float>(y), static_cast<float>(px_per_cell), static_cast<float>(px_per_cell));
            //     nvg::FillColor(nvg::RGBAf(0.6f, 0.6f, 0.6f, 1.0f));
            //     nvg::Fill();
            //     nvg::ClosePath();
            // }

            // nvg::BeginPath();
            // nvg::Rect(0, 0, width, height);
            // // nvg::Rect(0, 0, static_cast<float>(grid_width), static_cast<float>(grid_height));
            // nvg::FillColor(nvg::RGBA(0, 0, 0, 255));
            // nvg::Fill();
            // nvg::ClosePath();

            // nvg::BeginPath();
            // nvg::Rect(width+10, 0, width, height);
            // nvg::FillColor(nvg::RGBA(0, 0, 0, 255));
            // nvg::Fill();
            // nvg::ClosePath();

            // nvg::BeginPath();
            // // draw grid lines
            // for (int i = 0; i <= grid_width; i+=px_per_cell) {
            //     nvg::MoveTo(i, 0);
            //     nvg::LineTo(i, grid_height);
            // }
            // for (int i = 0; i <= grid_height; i+=px_per_cell) {
            //     nvg::MoveTo(0, i);
            //     nvg::LineTo(grid_width, i);
            // }
            // nvg::StrokeColor(nvg::RGBAf(0.2f, 0.2f, 0.2f, 0.6f));
            // nvg::StrokeWidth(0.5f);
            // nvg::Stroke();
            // // nvg::FillColor(nvg::RGBA(255, 0, 0, 255));
            // // nvg::Fill();
            // nvg::ClosePath();

            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, fb_width, fb_height);
            nvgFillPaint(vg, img_paint);
            nvgFill(vg);
            nvgClosePath(vg);

        });
    });
    RenderModule::Run();
    RenderModule::Shutdown(); 
    // nvgluDeleteFramebuffer(fb);


    return 0;
}