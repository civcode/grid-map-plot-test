#include <iostream>
#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"




#include "render_module/render_module.hpp" 


int main() {


    {
        // RenderModule::Init(1000, 1000, 1.0);
        // auto vg = RenderModule::GetNanoVGContext();
        
        // stbi_set_flip_vertically_on_load(false);
        // int w, h, channels;
        // unsigned char* data = stbi_load("../data/maze-1-10x10.png", &w, &h, &channels, 4); // force RGBA (4)
        // if (!data) {
        //     std::cerr << "xFailed to load image\n";
        //     return 1;
        // }

        // int size = w * h * 4; // must match forced 4-channel format
        // int img_id = nvgCreateImageMem(vg, NVG_IMAGE_FLIPY | NVG_IMAGE_NEAREST, data, size);

        // // stbi_image_free(data);
 
        // if (img_id == 0) {
        //     std::cerr << "xFailed to create NanoVG image\n";
        //     return 1;
        // }
    }
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


    // Resize image          
    int new_width = 100;
    int new_height = 100;
    int new_channels = channels;
    std::unique_ptr<unsigned char[]> data_resized(new unsigned char[new_width * new_height * new_channels]);
    // stbir_pixel_layout pixel_layout = (channels == 1) ? STBIR_1CHANNEL :
    //                                   (channels == 2) ? STBIR_2CHANNEL :
    //                                   (channels == 3) ? STBIR_RGB :
    //                                   (channels == 4) ? STBIR_RGBA : ;
    if (!stbir_resize_uint8_linear(
            data.get(), width, height, 0, 
            data_resized.get(), new_width, new_height, 0, 
            // STBIR_RGBA)) {
            STBIR_1CHANNEL)) {
        std::cerr << "Failed to resize image\n";
        return 1;
    }

    auto raw_ptr =  data.get();
    // auto raw_ptr =  data_resized.get();


    for (int i = 0; i < width * height * channels; ++i) {
        // std::cout << static_cast<int>(raw_ptr[i]) << " ";
        // if ((i+1) % 10 == 0) {
        //     std::cout << "\n";
        // }
    }

    RenderModule::Init(1200, 1200, 1.0);
    auto vg = RenderModule::GetNanoVGContext();
    int img_id = nvgCreateImage(vg, img_path, NVG_IMAGE_NEAREST | NVG_IMAGE_FLIPY); 
    // int img_id = nvgCreateImageRGBA(vg, width, height, NVG_IMAGE_NEAREST | NVG_IMAGE_FLIPY, raw_ptr);
    // int img_id = nvgCreateImageRGBA(vg, new_width, new_height, NVG_IMAGE_NEAREST | NVG_IMAGE_FLIPY, raw_ptr);
    // int img_id = nvgCreateImageMem(vg,  NVG_IMAGE_NEAREST | NVG_IMAGE_FLIPY, raw_ptr, width * height * channels); 
    if (img_id == 0) {
        std::cerr << "Failed to create image from memory\n";
        return 1;
    }
    NVGpaint img_paint = nvgImagePattern(vg, 0, 0, width, height, 0.0f, img_id, 1.0f);
    // NVGpaint img_paint = nvgImagePattern(vg, 0, 0, new_width, new_height, 0.0f, img_id, 1.0f);

    RenderModule::RegisterNanoVGCallback("Image Viewer", [&](NVGcontext* vg) {
        // RenderModule::NanoVGZoomView([&](NVGvertex* vg));
        // ZoomView::Draw("Zoom View", vg, [&](NVGcontext* vg) {
        RenderModule::ZoomView([&](NVGcontext* vg) {
            nvg::SetContext(vg);

            nvg::BeginPath();
            nvg::Rect(0, 0, width, height);
            // nvg::Rect(0, 0, new_width, new_height);
            nvg::FillPaint(img_paint);
            nvg::Fill();
            nvg::ClosePath();

            nvg::BeginPath();
            // draw grid lines
            for (int i = 0; i <= width; i++) {
                nvg::MoveTo(i, 0);
                nvg::LineTo(i, height);
            }
            for (int i = 0; i <= height; i++) {
                nvg::MoveTo(0, i);
                nvg::LineTo(width, i);
            }
            nvg::StrokeColor(nvg::RGBA(255, 0, 0, 155));
            nvg::StrokeWidth(0.1f);
            nvg::Stroke();
            // nvg::FillColor(nvg::RGBA(255, 0, 0, 255));
            // nvg::Fill();
            nvg::ClosePath();
        });
    });
    RenderModule::Run();
    RenderModule::Shutdown(); 

    return 0;
}