#include <iostream>
#include <memory>
#include <random>

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


    /** Create ROS-style Occupancy Grid Map 
     * - int8[] data ... probability [0,100], unknown = -1
    */
    struct MapMetaData {
        int width;
        int height;
        float resolution; // meters per pixel
        float origin_x;  // origin in meters
        float origin_y;  // origin in meters
    };
    MapMetaData map_metadata {
        .width = width,
        .height = height,
        .resolution = 0.1f, // 0.1 m per cell
        .origin_x = 0.0f,   // origin at (0,0)
        .origin_y = 0.0f    // origin at (0,0)
    };
    std::unique_ptr<int8_t[]> occupancy_grid = std::make_unique<int8_t[]>(width * height);

    for (int i = 0; i < width * height; ++i) {
        if (data.get()[i] == 0) {
            occupancy_grid[i] = 100;
        } else {
            occupancy_grid[i] = 0;
        }
    }
    
    int px_per_cell = 10;
    int grid_width = width * px_per_cell;
    int grid_height = height * px_per_cell;
    std::cout << "Grid size: " << grid_width << "x" << grid_height << "\n";

    RenderModule::Init(1200, 1200, 0.0);
    RenderModule::EnableRootWindowDocking();


    /* Create a FB and draw to it */
    int fb_width = width*px_per_cell;
    int fb_height = height*px_per_cell;
    
    // NVGLUframebuffer* fb = nvgluCreateFramebuffer(vg, width, height, NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY | NVG_IMAGE_FLIPY);
    nvg::SetContext(RenderModule::GetNanoVGContext());
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

    /* Draw Grid Map */
    nvg::BeginFrame(fb_width, fb_height, 1.0f);

        nvg::BeginPath();
        nvg::Rect(0, 0, static_cast<float>(grid_width), static_cast<float>(grid_height));
        nvg::FillColor(nvg::RGBAf(1.0f, 1.0f, 1.0f, 1.0f));
        nvg::Fill();

        for (int i = 0; i < width * height; ++i) {
            int x = (i % width) * px_per_cell;
            int y = (i / width) * px_per_cell;
            y = grid_height - y - px_per_cell; // Invert y-axis for correct rendering
            int8_t value = occupancy_grid.get()[i];
            if (value == 0) {
                continue;
            }
            nvg::BeginPath();
            nvg::Rect(static_cast<float>(x), static_cast<float>(y), static_cast<float>(px_per_cell), static_cast<float>(px_per_cell));
            nvg::FillColor(nvg::RGBAf(0.6f, 0.6f, 0.6f, 1.0f));
            nvg::Fill();
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
        nvg::StrokeWidth(0.8f);
        nvg::Stroke();

    nvg::EndFrame();
    nvg::GLUtilsBindFramebuffer(nullptr); 
        
    NVGpaint img_paint = nvg::ImagePattern(0, 0, fb_width, fb_height, 0, fb->image, 1.0f);
    if (img_paint.image == 0) {
        std::cerr << "Failed to create image pattern." << std::endl;
        return -1;
    }
    
    // RenderModule::RegisterNanoVGCallback("Red Circle", [&](NVGcontext* vg) {
    //     nvgBeginPath(vg);
    //     nvgRect(vg, 0, 0, 200, 200);
    //     nvgFillPaint(vg, img_paint);
    //     nvgFill(vg);
    //     nvgClosePath(vg);

    //     nvg::GLUtilsBindFramebuffer(fb);
    //     glad::glViewport(0, 0, fb_width, fb_height);
    //     // glad::glClearColor(0, 0, 0, 0);
    //     // glad::glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    //     // nvgSave(vg);
    //     // nvgluBindFramebuffer(fb);
    //     nvgBeginFrame(vg, fb_width, fb_height, 1.0f);
    //     nvgBeginPath(vg);
    //     static float cx = 10;
    //     static float cy = 10;
    //     static float dx = 0.5f;
    //     nvgCircle(vg, cx, cy, 5);
    //     cx += dx;
    //     cy += dx;
    //     nvgFillColor(vg, nvgRGBA(255, 0, 0, 155));
    //     nvgFill(vg);
    //     nvgClosePath(vg);
    //     nvgEndFrame(vg);
    //     nvgluBindFramebuffer(NULL);
    //     // nvgRestore(vg);
    // });

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


    std::random_device rd;
    std::mt19937 gen(rd());
    // std::uniform_real_distribution<float> dist(0.0f, std::static_cast<float>(fb_width));
    std::uniform_real_distribution<float> dist(0.0f, static_cast<float>(fb_width));

    float test = 0.5f;
    bool toggle = true;
    int count = 1;

    RenderModule::RegisterImGuiCallback([&]() {
        ImGui::Begin("Controls");
        ImGui::Text("Grid Map Metadata:");
        ImGui::SliderFloat("Test", &test, 0.0f, 1.0f);
        ImGui::SliderInt("count", &count, 1, 100);
        //create on off button
        if (ImGui::Button(toggle ? "ON" : "OFF")) {
            toggle = !toggle;
        }
        ImGui::End();

        // ImGui::Begin("Main Text Window", nullptr);
        // ImGui::Begin("Main Text Window");
        // ImGui::TextWrapped(
        //     "CJK text will only appear if the font was loaded with the appropriate CJK character ranges. "
        //     "Call io.Fonts->AddFontFromFileTTF() manually to load extra character ranges. "
        //     "Read docs/FONTS.md for details.");
        // ImGui::Text("Hiragana: \xe3\x81\x8b\xe3\x81\x8d\xe3\x81\x8f\xe3\x81\x91\xe3\x81\x93 (kakikukeko)");
        // ImGui::Text("Kanjis: \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e (nihongo)");
        // static char buf[32] = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";
        // ImGui::InputText("UTF-8 input", buf, IM_ARRAYSIZE(buf));
        // ImGui::End();
    });

    RenderModule::RegisterImGuiCallback([&]() {
        ImGui::Begin("Grid Map Viewer");
        ImGui::Text("Grid Map Metadata:");
        ImGui::Text("Width: %d, Height: %d, Resolution: %.2f m/pixel", map_metadata.width, map_metadata.height, map_metadata.resolution);
        ImGui::Text("Origin: (%.2f, %.2f)", map_metadata.origin_x, map_metadata.origin_y);
        ImGui::Text(" ");
        ImGui::End();
    });

    RenderModule::RegisterNanoVGCallback("Grid Map Viewer", 
        /* Render */
        [&](NVGcontext* vg) {
            RenderModule::ZoomView([&](NVGcontext* vg) {
                nvg::BeginPath();
                nvg::Rect(0, 0, fb_width, fb_height);
                nvg::FillPaint(img_paint);
                nvg::Fill();
            });
        },
        /* Offscreen Render */
        [&](NVGcontext* vg) {
            RenderModule::IsolatedFrameBuffer([&](NVGcontext* vg) {
                if (toggle) {
                    nvg::GLUtilsBindFramebuffer(fb);
                    glad::glViewport(0, 0, fb_width, fb_height);

                    nvg::BeginFrame(fb_width, fb_height, 1.0f);
                    nvg::BeginPath();
                    for (int i = 0; i < count; ++i) {
                        nvg::Rect(std::floor(dist(gen)), std::floor(dist(gen)), px_per_cell, px_per_cell);
                    }
                    nvg::FillColor(nvg::RGBAf(1.0f, 0.0f, 0.0f, 1.0f));
                    nvg::Fill();
                    nvg::EndFrame();
                    nvg::GLUtilsBindFramebuffer(nullptr);
                }
            });
        }
    );

    RenderModule::Run();
    RenderModule::Shutdown(); 
    // nvgluDeleteFramebuffer(fb);


    return 0;
}